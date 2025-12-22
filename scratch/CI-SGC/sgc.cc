#include "sgc.h"

#include "crypto/agka.h"
#include "crypto/pki.h"
#include "crypto/utils.h"
#include "message/bytereader.h"
#include "message/header.h"
#include "message/message.h"
#include "utils.h"

#include "ns3/singleton.h"

#include <algorithm>
#include <big.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <pairing_1.h>
#include <string>
#include <vector>

const int SGC::MaxGroupNum = 1;
const int SGC::SidLength = 16;
const int SGC::KeyVerifierLength = sizeof(uint32_t) + sizeof(uint64_t) + 32;

uint32_t SGC::group_seq_ = 0;
uint32_t SGC::user_seq_ = 0;

SGC::SGC(Role r)
    : role_(r),
      ka_proto_(nullptr)
{
    if (r == Role::kRoleVehicle)
    {
        pid_ = user_seq_;
        user_seq_++;
        state_ = State::kPosUnset;
        ka_proto_ = std::make_shared<SAAGKA>();
        return;
    }
    else if (r == Role::kRoleRSU)
    {
        state_ = State::kIdle;
        for (int i = 0; i < MaxGroupNum; i++)
        {
            group_seq_++;
            auto gsi = std::make_shared<GroupSessionInfo>(group_seq_, 0);
            gsis_[group_seq_] = gsi;
        }
    }
}

SGC::~SGC()
{
}

void
SGC::Enroll()
{
    ka_proto_->KeyGen();
}

std::shared_ptr<SGCMessage>
SGC::HeartbeatMsg()
{
    pending_join_pid_ = -1; // ready to accept new join request

    auto hb = std::make_shared<Heartbeat>();
    hb->kv_ = cur_kv_;
    hb->hb_seq_ = hb_counter_++;
    for (const auto& [sid_key, ptr_gsi] : gsis_)
    {
        hb->gsis_.push_back(*ptr_gsi);
    }

    return hb;
}

std::vector<std::shared_ptr<SGCMessage>>
SGC::HandleMsg(const uint8_t* bytes, size_t len)
{
    ByteReader br(bytes, len);
    auto header = br.read<Header>();

    std::vector<std::shared_ptr<SGCMessage>> resp;

    if (role_ == Role::kRoleRSU)
    {
        switch (header.type_)
        {
        case MsgType::kHeartbeatAck: {
            auto hb_ack_msg = std::make_shared<HeartbeatAck>();
            hb_ack_msg->Deserialize(br);

            auto hb_ack_resp = HandleHeartbeatAck(hb_ack_msg);
            if (hb_ack_resp)
            {
                resp.push_back(hb_ack_resp);
            }
            auto key_encap_ntf = NotifyKeyEncap();
            if (key_encap_ntf)
            {
                resp.push_back(key_encap_ntf);
            }
        }
        break;

        case MsgType::kJoin: {
            auto join_msg = std::make_shared<Join>();
            join_msg->Deserialize(br);

            auto join_resp = AckJoin(join_msg);
            if (join_resp)
            {
                resp.push_back(join_resp);
            }
        }
        break;
        case MsgType::kKeyEncap: {
            auto key_encap_msg = std::make_shared<KeyEncap>();
            key_encap_msg->Deserialize(br);
            resp.push_back(key_encap_msg);
            HandleKeyEncap(key_encap_msg);
        }
        break;

        default:
            break;
        }
    }
    else
    {
        switch (header.type_)
        {
        case MsgType::kHeartbeat: {
            auto hb_msg = std::make_shared<Heartbeat>();
            hb_msg->Deserialize(br);
            // INFO("Vehicle-" << pid_ << "ready to handle heartbeat");
            auto hb_resp = HandleHeartbeat(hb_msg);
            if (hb_resp)
            {
                resp.push_back(hb_resp);
            }
        }
        break;

        case MsgType::kNotifyPosition: {
            auto notify_pos_msg = std::make_shared<NotifyPosition>();
            notify_pos_msg->Deserialize(br);

            auto join = LaunchJoin(notify_pos_msg);
            if (join)
            {
                resp.push_back(join);
            }
        }
        break;

        case MsgType::kJoin: {
            auto join_msg = std::make_shared<Join>();
            join_msg->Deserialize(br);
            HandleJoin(join_msg);
        }
        break;

        case MsgType::kJoinAck: {
            auto join_ack_msg = std::make_shared<JoinAck>();
            join_ack_msg->Deserialize(br);
            HandleJoinAck(join_ack_msg);
        }
        break;

        case MsgType::kKeyEncapNotify: {
            auto key_encap_notify_msg = std::make_shared<KeyEncapNotify>();
            key_encap_notify_msg->Deserialize(br);
            auto key_encap = EncapsulateKey(key_encap_notify_msg);
            if (key_encap)
            {
                resp.push_back(key_encap);
            }
        }
        break;

        case MsgType::kKeyEncap: {
            auto key_encap_msg = std::make_shared<KeyEncap>();
            key_encap_msg->Deserialize(br);
            DecapsulateKey(key_encap_msg);
        }
        break;

        default:
            break;
        }
    }

    return resp;
}

std::shared_ptr<SGCMessage>
SGC::HandleHeartbeat(std::shared_ptr<SGCMessage> hb_msg)
{
    // INFO("Vehicle-" << pid_ << " starts handling heartbeat");
    auto hb = std::dynamic_pointer_cast<Heartbeat>(hb_msg);
    if (!hb)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    for (auto& gsi : hb->gsis_)
    {
        uint32_t seq = ParseGroupSeqFromSid(gsi.sid_);
        auto it = gsis_.find(seq);
        if (it == gsis_.end())
        {
            gsis_[seq] = std::make_shared<GroupSessionInfo>(gsi);
            INFO("Vehicle-" << pid_ << " stores info about Group-" << ToString(gsi.sid_))
            continue;
        }

        if (it->second->n_member_ < gsi.n_member_)
        {
            if (sid_.size() > 0 && IsEqual(it->second->sid_, sid_) && state_ == State::kJoined)
            {
                int n_slot = SAAGKA::GetPublicParameter()->matrices[gsi.size_param_].size();
                auto diff = SlotDiff(gsi.mem_bitmap_, it->second->mem_bitmap_, n_slot);

                for (auto slot : diff)
                {
                    gap_slots_.insert(slot);
                }
                WARN("Vehicle-" << pid_ << " fall behind");
            }
            else
            {
                it->second = std::make_shared<GroupSessionInfo>(gsi);
            }
        }
    }

    if (hb->kv_.version_ > cur_kv_.version_)
    {
        // try to update session key
        auto it = pending_kvs_.find(hb->kv_.version_);
        if (it == pending_kvs_.end())
        {
            pending_kvs_[hb->kv_.version_] = std::vector<KeyTuple>();
            pending_kvs_[hb->kv_.version_].emplace_back(hb->kv_, std::vector<uint8_t>());
        }
        else
        {
            size_t index = -1;
            for (size_t i = 0; i < it->second.size(); i++)
            {
                auto& tuple = it->second[i];
                if (IsEqual(tuple.first.hash_, hb->kv_.hash_) &&
                    tuple.first.version_ == hb->kv_.version_)
                {
                    index = i;
                    break;
                }
            }
            if (index == -1)
            {
                it->second.emplace_back(hb->kv_, std::vector<uint8_t>());
            }
            else if (it->second[index].second.size() > 0)
            {
                cur_kv_ = hb->kv_;
                cur_session_key_ = it->second[index].second;
                INFO("Vehicle-" << pid_ << " updates session key, key="
                                << ToString(cur_session_key_) << ", version=" << hb->kv_.version_);
            }
        }
        // discard stale pending kvs
        CleanPendingKvs(hb->kv_.version_);
    }
    auto resp = std::make_shared<HeartbeatAck>(state_, pid_, hb->hb_seq_);

    INFO("Vehicle-" << pid_ << " reponds to heartbeat: " << resp->fmtString());
    return resp;
}

std::shared_ptr<SGCMessage>
SGC::HandleHeartbeatAck(std::shared_ptr<SGCMessage> hb_ack_msg)
{
    auto hb_ack = std::dynamic_pointer_cast<HeartbeatAck>(hb_ack_msg);
    if (!hb_ack)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    // update vehicle info
    auto now = std::chrono::steady_clock::now();

    auto it = vis_.find(hb_ack->pid_);
    if (it == vis_.end())
    {
        auto it = hb_ack_time_.emplace(now, hb_ack->pid_);

        auto vi_p = std::make_shared<VehicleInfo>();
        vi_p->pid_ = hb_ack->pid_;
        vi_p->has_joined_ = false;
        vi_p->last_hback_time_ = now;
        vi_p->time_it_ = it;
        vis_.emplace(hb_ack->pid_, vi_p);
    }
    else
    {
        hb_ack_time_.erase(it->second->time_it_);
        auto new_it = hb_ack_time_.emplace(now, hb_ack->pid_);
        it->second->last_hback_time_ = now;
        it->second->time_it_ = new_it;
    }

    // respond
    std::shared_ptr<NotifyPosition> resp;
    if (state_ == State::kIdle && hb_ack->state_ == State::kPosUnset &&
        hb_ack->hb_seq_ == hb_counter_ - 1 && pending_join_pid_ == -1)
    {
        resp = std::make_shared<NotifyPosition>();
        resp->pos_ = gsis_[1]->n_member_;
        resp->sid_ = gsis_[1]->sid_;
        resp->pid_ = hb_ack->pid_;

        vis_[hb_ack->pid_]->group_seq_ = 1;
        vis_[hb_ack->pid_]->pos_ = resp->pos_;

        state_ = State::kHandleJoin;
        pending_join_pid_ = hb_ack->pid_;

        INFO("RSU instruct Vehicle-" << hb_ack->pid_ << " to join Group-" << ToString(resp->sid_)
                                     << ", pos=" << resp->pos_);
    }
    return resp;
}

std::shared_ptr<SGCMessage>
SGC::LaunchJoin(std::shared_ptr<SGCMessage> pos_notify_msg)
{
    auto pos_notify = std::dynamic_pointer_cast<NotifyPosition>(pos_notify_msg);
    if (!pos_notify)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    if (pid_ != pos_notify->pid_ || state_ != State::kPosUnset)
    {
        return nullptr;
    }

    sid_ = pos_notify->sid_;
    pos_ = pos_notify->pos_;
    state_ = State::kJoining;

    auto group_seq = ParseGroupSeqFromSid(pos_notify->sid_);
    auto gsi_p = gsis_.find(group_seq)->second;

    INFO("Vehicle-" << pid_ << " launched join, sid=" << ToString(sid_) << ", pos=" << pos_
                    << ", original ek=" << gsi_p->ek_);

    auto kam = ka_proto_->MessageGen(sid_, gsi_p->ek_, gsi_p->size_param_, pos_);

    auto resp = std::make_shared<Join>();
    resp->kam_ = kam;
    resp->pid_ = pid_;

    return resp;
}

void
SGC::HandleJoin(std::shared_ptr<SGCMessage> join_msg)
{
    auto join = std::dynamic_pointer_cast<Join>(join_msg);
    if (!join)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    if (state_ != State::kJoined || !IsEqual(sid_, join->kam_.sid) ||
        IsPositionOccupied(sid_, join->kam_.pos))
    {
        return;
    }
    if (SAAGKA::CheckValid(join->kam_))
    {
        ka_proto_->UpdateKey(join->kam_);
        INFO("Vehicle-" << pid_
                        << " updates asymmetric keys, new ek=" << ka_proto_->GetEncryptionKey());

        auto gsi_p = gsis_.find(ParseGroupSeqFromSid(sid_))->second;
        gsi_p->n_member_++;
        OccupyOnePosition(sid_, join->kam_.pos);
        gsi_p->ek_ = ka_proto_->GetEncryptionKey();

        for (auto it = gap_slots_.begin(); it != gap_slots_.end(); it++)
        {
            if (*it == join->kam_.pos)
            {
                gap_slots_.erase(it);
                break;
            }
        }
    }
}

std::shared_ptr<SGCMessage>
SGC::AckJoin(std::shared_ptr<SGCMessage> join_msg)
{
    auto join = std::dynamic_pointer_cast<Join>(join_msg);
    if (!join)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    auto pp = SAAGKA::GetPublicParameter();
    int scale = pp->matrices[join->kam_.size_param].size();
    auto& kam = join->kam_;

    if (state_ != State::kHandleJoin || join->pid_ != pending_join_pid_ ||
        kam.pos != vis_[join->pid_]->pos_)
    {
        INFO("RSU reject member joining, reason=malicious message");
        return nullptr;
    }
    if (!SAAGKA::CheckValid(join->kam_))
    {
        WARN("RSU check validity failed");
        return nullptr;
    }

    auto it = gsis_.find(ParseGroupSeqFromSid(kam.sid));

    if (it == gsis_.end())
    {
        FATAL_ERROR("Group session info not exist");
    }
    auto gsi_p = it->second;

    // update member info
    gsi_p->n_member_++;
    OccupyOnePosition(kam.sid, kam.pos);
    vis_[join->pid_]->has_joined_ = true;

    // update ek
    auto& matrix = pp->matrices[kam.size_param];
    auto pfc = pp->pfc;

    auto pk = ns3::Singleton<PKI>::Get()->Get(kam.pk_id);
    auto pseudo_pk = SAAGKA::PublicKey{matrix[kam.pos][scale + 1], matrix[kam.pos][scale + 2]};
    Big v = SAAGKA::HashAnyToBig(kam.sid, pk, kam.u);
    Big pseudo_v = SAAGKA::HashAnyToBig(std::vector<uint8_t>(), pseudo_pk, matrix[kam.pos][scale]);
    G1 tmp = pk.y1 + pfc->mult(pk.y2, v) + (-pseudo_pk.y1) + (-pfc->mult(pseudo_pk.y2, pseudo_v));

    GT delta_mu = pfc->pairing(tmp, pp->g0);
    G1 delta_lamda = kam.u + (-matrix[kam.pos][scale]);

    gsi_p->ek_.lambda = gsi_p->ek_.lambda + delta_lamda;
    gsi_p->ek_.mu = gsi_p->ek_.mu * delta_mu;

    // update d
    for (size_t i = 0; i < scale; i++)
    {
        if (i == kam.pos)
        {
            continue;
        }

        gsi_p->d_[i] = gsi_p->d_[i] + (-matrix[kam.pos][i]) + kam.z[i];
    }
    INFO("RSU accept Vehicle-" << join->pid_ << " joining, sid=" << ToString(kam.sid)
                               << ", pos=" << kam.pos << ", new ek=" << gsi_p->ek_);

    // construct respond message
    auto resp = std::make_shared<JoinAck>();
    resp->sid_ = kam.sid;
    resp->pk_id_ = kam.pk_id;
    resp->pos_ = kam.pos;
    resp->d_ = gsi_p->d_[kam.pos];
    resp->ek_ = gsi_p->ek_;

    // update state
    state_ = State::kIdle;

    return resp;
}

void
SGC::HandleJoinAck(std::shared_ptr<SGCMessage> join_ack_msg)
{
    auto join_ack = std::dynamic_pointer_cast<JoinAck>(join_ack_msg);
    if (!join_ack)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    if (!IsEqual(join_ack->sid_, sid_) || join_ack->pos_ != pos_ || state_ != State::kJoining)
    {
        return;
    }

    auto group_seq = ParseGroupSeqFromSid(sid_);
    auto gsi_p = gsis_[group_seq];
    if (ka_proto_->AsymKeyDerive(sid_, pos_, join_ack->d_, join_ack->ek_))
    {
        INFO("Vehicle-" << pid_ << " finishes joining, sid=" << ToString(join_ack->sid_)
                        << ", new ek=" << ka_proto_->GetEncryptionKey());
        gsi_p->ek_ = join_ack->ek_;
        gsi_p->n_member_++;
        OccupyOnePosition(sid_, pos_);
        state_ = State::kJoined;
    }
    else
    {
        WARN("Vehicle-" << pid_ << " aborts joining, sid=" << ToString(join_ack->sid_));
    }
}

std::shared_ptr<SGCMessage>
SGC::NotifyKeyEncap()
{
    LazyDropVehicleInfo(2);

    auto now = std::chrono::steady_clock::now();
    auto used_time = now - cur_kv_.timestamp_;
    INFO("RSU check key used time, t="
         << std::chrono::duration_cast<std::chrono::milliseconds>(used_time).count() << "ms");
    if (used_time < std::chrono::seconds(2))
    {
        sk_dispatcher_ = -1;
        return nullptr;
    }

    auto it = hb_ack_time_.end();
    while (it != hb_ack_time_.begin())
    {
        it--;
        auto pid = it->second;
        if (vis_[pid]->has_joined_)
        {
            auto msg = std::make_shared<KeyEncapNotify>();
            msg->pid_ = it->second;
            msg->key_length_ = 32;
            msg->cur_key_version_ = cur_kv_.version_;
            sk_dispatcher_ = msg->pid_;

            INFO("RSU instructs Vehicle-" << msg->pid_ << " to encapsulate a session key");
            return msg;
        }
    }

    return nullptr;
}

std::shared_ptr<SGCMessage>
SGC::EncapsulateKey(std::shared_ptr<SGCMessage> encap_ntf_msg)
{
    auto encap_ntf = std::dynamic_pointer_cast<KeyEncapNotify>(encap_ntf_msg);
    if (!encap_ntf)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    if (encap_ntf->pid_ != pid_)
    {
        return nullptr;
    }

    std::vector<uint8_t> key = GenerateRandomBytes(encap_ntf->key_length_);

    // construct encapsulation
    KeyVerifier kv;
    kv.version_ = encap_ntf->cur_key_version_ + 1;
    kv.timestamp_ = std::chrono::steady_clock::now();
    kv.hash_.resize(32);
    Sha256(reinterpret_cast<const char*>(key.data()),
           key.size(),
           reinterpret_cast<char*>(kv.hash_.data()));

    auto encap = std::make_shared<KeyEncap>();
    encap->kv_ = kv;
    encap->group_num_ = gsis_.size();
    encap->pid_ = pid_;

    std::vector<SAAGKA::EncryptionKey> eks;
    eks.reserve(gsis_.size());
    for (const auto& [group_seq, gsi_p] : gsis_)
    {
        encap->sids_.push_back(gsi_p->sid_);
        eks.push_back(gsi_p->ek_);
    }
    encap->ct_ = SAAGKA::Encrypt(key, eks);

    // set to pending list
    if (pending_kvs_.find(kv.version_) == pending_kvs_.end())
    {
        pending_kvs_[kv.version_] = std::vector<KeyTuple>();
    }
    pending_kvs_[kv.version_].emplace_back(kv, key);

    INFO("Vehicle-" << pid_ << " encapsulates session key, key=" << ToString(key)
                    << ", version=" << encap->kv_.version_);

    return encap;
}

void
SGC::DecapsulateKey(std::shared_ptr<SGCMessage> encap_msg)
{
    auto encap = std::dynamic_pointer_cast<KeyEncap>(encap_msg);
    if (!encap)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    if (encap->kv_.version_ <= cur_kv_.version_ || state_ != State::kJoined || encap->pid_ == pid_)
    {
        return;
    }

    uint32_t index = -1;
    for (size_t i = 0; i < encap->sids_.size(); i++)
    {
        if (IsEqual(encap->sids_[i], sid_))
        {
            index = i;
            break;
        }
    }
    if (index == -1)
    {
        WARN("Vehicle-" << pid_ << " decapsulates session key failed, reason=sid not found");
        return;
    }
    auto key = ka_proto_->Decrypt(encap->ct_, index);

    char hash_res[32];
    Sha256(reinterpret_cast<const char*>(key.data()), key.size(), hash_res);
    if (!IsEqual(encap->kv_.hash_, std::vector<uint8_t>(hash_res, hash_res + 32)))
    {
        WARN("Vehicle-" << pid_ << " decapsulates session key failed, reason=hash not match");
        return;
    }

    INFO("Vehicle-" << pid_ << " decapsulates session key success, key=" << ToString(key)
                    << ", version=" << encap->kv_.version_);

    uint32_t ver = encap->kv_.version_;
    auto it = pending_kvs_.find(ver);
    if (it == pending_kvs_.end())
    {
        pending_kvs_[encap->kv_.version_] = std::vector<KeyTuple>();
        pending_kvs_[encap->kv_.version_].emplace_back(encap->kv_, key);
        return;
    }
    else
    {
        for (size_t i = 0; i < it->second.size(); i++)
        {
            auto tuple = it->second[i];
            if (tuple.first.hash_ == encap->kv_.hash_)
            {
                cur_kv_ = encap->kv_;
                cur_session_key_ = key;
                CleanPendingKvs(encap->kv_.version_ + 1);
                INFO("Vehicle-" << pid_ << " updates session key, key=" << ToString(key)
                                << ", version=" << ver);
                return;
            }
        }
        it->second.emplace_back(encap->kv_, key);
    }
}

void
SGC::HandleKeyEncap(std::shared_ptr<SGCMessage> encap_msg)
{
    auto encap = std::dynamic_pointer_cast<KeyEncap>(encap_msg);
    if (!encap)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    if (encap->pid_ != sk_dispatcher_)
    {
        WARN("RSU rejects encap from Vehicle-" << encap->pid_ << ", reason=not dispatcher");
        return;
    }

    sk_dispatcher_ = -1;
    auto now = std::chrono::steady_clock::now();
    if (cur_kv_.version_ >= encap->kv_.version_ ||
        encap->kv_.timestamp_ + std::chrono::seconds(1) < now)
    {
        WARN("RSU rejects encap from Vehicle-" << encap->pid_ << ", reason=key not fresh enough");
        return;
    }
    INFO("RSU accept key verifier, kv=" << encap->kv_ << ", from=" << encap->pid_);
    cur_kv_ = encap->kv_;
}

bool
SGC::IsPositionOccupied(std::vector<uint8_t> sid, uint32_t pos)
{
    auto gsi_p = gsis_.find(ParseGroupSeqFromSid(sid))->second;
    size_t index = pos / 8;
    uint8_t mask = 0x80 >> (pos % 8);
    return (gsi_p->mem_bitmap_[index] & mask) != 0;
}

void
SGC::OccupyOnePosition(std::vector<uint8_t> sid, uint32_t pos)
{
    auto gsi_p = gsis_.find(ParseGroupSeqFromSid(sid))->second;
    size_t index = pos / 8;
    uint8_t mask = 0x80 >> (pos % 8);
    gsi_p->mem_bitmap_[index] |= mask;
}

void
SGC::CleanPendingKvs(uint32_t version)
{
    for (auto it = pending_kvs_.begin(); it != pending_kvs_.end();)
    {
        if (it->first < version)
        {
            it = pending_kvs_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
SGC::LazyDropVehicleInfo(uint32_t timeout_seconds)
{
    const auto now = std::chrono::steady_clock::now();
    auto it = hb_ack_time_.begin();
    while (it != hb_ack_time_.end())
    {
        if (it->first + std::chrono::seconds(timeout_seconds) < now)
        {
            auto pid = it->second;
            it = hb_ack_time_.erase(it);
            vis_.erase(pid);
            INFO("RSU drop info about Vehicle-" << pid);
        }
        else
        {
            it++;
        }
    }
}

// GroupSessionInfo

SGC::GroupSessionInfo::GroupSessionInfo(uint16_t seq, uint16_t size_param)
    : size_param_(size_param),
      n_member_(0)
{
    auto pp = SAAGKA::GetPublicParameter();
    int scale = pp->matrices[size_param_].size();

    size_t bm_size = ((scale + 63) / 64) * 8;
    mem_bitmap_ = std::vector<uint8_t>(bm_size, 0);

    expiry_time_ = std::chrono::steady_clock::now() + std::chrono::seconds(60);

    // construct sid
    sid_ = std::vector<uint8_t>();
    ByteWriter bw(sid_);

    bw.write("SID-");
    bw.write(seq);
    bw.write(size_param);
    bw.write(expiry_time_);

    auto& matrix = pp->matrices[size_param_];
    d_.resize(scale);
    for (int i = 0; i < scale; i++)
    {
        d_[i].g.clear();
    }
    ek_.lambda.g.clear();

    G1 tmp;
    tmp.g.clear();
    for (int i = 0; i < scale; i++)
    {
        auto v = SAAGKA::HashAnyToBig(std::vector<uint8_t>(),
                                      SAAGKA::PublicKey{matrix[i][scale + 1], matrix[i][scale + 2]},
                                      matrix[i][scale]);
        int j = 0;
        for (; j < scale; ++j)
        {
            if (j == i)
            {
                continue;
            }
            d_[j] = d_[j] + matrix[i][j];
        }
        ek_.lambda = ek_.lambda + matrix[i][j];
        j++;

        tmp = tmp + matrix[i][j] + (pp->pfc->mult(matrix[i][j + 1], v));
    }
    ek_.mu = pp->pfc->pairing(tmp, pp->g0);
}

SGC::GroupSessionInfo::GroupSessionInfo(const GroupSessionInfo& gsi)
    : size_param_(gsi.size_param_),
      n_member_(gsi.n_member_),
      ek_(gsi.ek_),
      expiry_time_(gsi.expiry_time_)
{
    sid_.insert(sid_.end(), gsi.sid_.begin(), gsi.sid_.end());
    mem_bitmap_.insert(mem_bitmap_.end(), gsi.mem_bitmap_.begin(), gsi.mem_bitmap_.end());
}

uint32_t
SGC::GetPid() const
{
    return pid_;
}
