#include "sgc-vehicle.h"

#include "../application.h"
#include "../crypto/agka.h"
#include "../crypto/utils.h"
#include "../message/bytereader.h"
#include "../message/header.h"
#include "../message/message.h"
#include "../utils.h"
#include "sgc.h"

#include "ns3/singleton.h"

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
#include <sys/resource.h>
#include <sys/types.h>
#include <vector>

uint32_t SGCVehicle::user_seq_ = 0;

SGCVehicle::SGCVehicle()
    : state_(State::kPrepare),
      pid_(user_seq_++),
      ka_proto_(std::make_shared<SAAGKA>())
{
}

SGCVehicle::~SGCVehicle()
{
}

void
SGCVehicle::Enroll()
{
    ka_proto_->KeyGen();
}

std::vector<std::shared_ptr<SGCMessage>>
SGCVehicle::HandleMsg(const uint8_t* bytes, size_t len)
{
    ByteReader br(bytes, len);
    auto header = br.read<Header>();

    auto metric = ns3::Singleton<Metric>::Get();
    metric->Emit(header.type_, br.remaining());

    std::vector<std::shared_ptr<SGCMessage>> resp;

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

    case MsgType::kKeyUpdate: {
        auto key_upd_msg = std::make_shared<KeyUpd>();
        key_upd_msg->Deserialize(br);
        HandleKeyUpd(key_upd_msg);
    }
    break;

    case MsgType::kKeyUpdateAck: {
        auto upd_ack_msg = std::make_shared<KeyUpdAck>();
        upd_ack_msg->Deserialize(br);
        HandleKeyUpdAck(upd_ack_msg);
    }
    break;

    default:
        // INFO("Vehicle-" << pid_ << " receives unknown message type: " << header.type_);
        break;
    }

    return resp;
}

std::shared_ptr<SGCMessage>
SGCVehicle::HandleHeartbeat(std::shared_ptr<SGCMessage> hb_msg)
{
    // INFO("Vehicle-" << pid_ << " starts handling heartbeat");
    auto hb = std::dynamic_pointer_cast<Heartbeat>(hb_msg);
    if (!hb)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    // emit metric
    auto metric = ns3::Singleton<Metric>::Get();
    auto mk = metric->GenerateStatKey(EmitType::kTotalHearbeat, hb->hb_seq_);
    metric->Emit(EmitType::kTotalHearbeat, mk);

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
                    gap_positions_.insert(slot);
                }
                WARN("Vehicle-" << pid_ << " fall behind, diff slots=" << ToString(diff));
            }
            else
            {
                it->second = std::make_shared<GroupSessionInfo>(gsi);
            }
        }
    }

    TryUpdateSessionKey(hb->kv_);
    auto resp = std::make_shared<HeartbeatAck>(state_, pid_, hb->hb_seq_);

    // INFO("Vehicle-" << pid_ << " reponds to heartbeat: " << resp->fmtString());
    return resp;
}

std::shared_ptr<SGCMessage>
SGCVehicle::LaunchJoin(std::shared_ptr<SGCMessage> pos_notify_msg)
{
    auto metric = ns3::Singleton<Metric>::Get();

    auto pos_notify = std::dynamic_pointer_cast<NotifyPosition>(pos_notify_msg);
    if (!pos_notify)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    if (pid_ != pos_notify->pid_ || state_ != State::kPrepare)
    {
        return nullptr;
    }

    // metric computation cost
    std::string mk = metric->GenerateStatKey(EmitType::kComputeJoinStep1);
    metric->Emit(EmitType::kComputeJoinStep1, mk);

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

    metric->Emit(EmitType::kComputeJoinStep1, mk);

    // metric total cost (start)
    auto mk_total = metric->GenerateStatKey(EmitType::kTotalJoin, pid_);
    metric->Emit(EmitType::kTotalJoin, mk_total);

    return resp;
}

void
SGCVehicle::HandleJoin(std::shared_ptr<SGCMessage> join_msg)
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
        auto metric = ns3::Singleton<Metric>::Get();
        auto mk = metric->GenerateStatKey(EmitType::kComputeJoinStep4);
        metric->Emit(EmitType::kComputeJoinStep4, mk);

        ka_proto_->UpdateKey(join->kam_);
        INFO("Vehicle-" << pid_
                        << " updates asymmetric keys, new ek=" << ka_proto_->GetEncryptionKey());

        auto gsi_p = gsis_.find(ParseGroupSeqFromSid(sid_))->second;
        gsi_p->n_member_++;
        OccupyOnePosition(sid_, join->kam_.pos);
        gsi_p->ek_ = ka_proto_->GetEncryptionKey();

        for (auto it = gap_positions_.begin(); it != gap_positions_.end(); it++)
        {
            if (*it == join->kam_.pos)
            {
                gap_positions_.erase(it);
                break;
            }
        }

        metric->Emit(EmitType::kComputeJoinStep4, mk);
    }
}

void
SGCVehicle::HandleJoinAck(std::shared_ptr<SGCMessage> join_ack_msg)
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

    auto metric = ns3::Singleton<Metric>::Get();

    std::string key = metric->GenerateStatKey(EmitType::kComputeJoinStep3);
    metric->Emit(EmitType::kComputeJoinStep3, key);

    auto group_seq = ParseGroupSeqFromSid(sid_);
    auto gsi_p = gsis_[group_seq];
    if (ka_proto_->AsymKeyDerive(sid_, pos_, join_ack->d_, join_ack->ek_))
    {
        gsi_p->ek_ = join_ack->ek_;
        gsi_p->n_member_++;
        OccupyOnePosition(sid_, pos_);
        state_ = State::kJoined;

        metric->Emit(EmitType::kComputeJoinStep3, key);

        // metric total cost (end)
        ns3::Simulator::Schedule(
            ConvertRealTimeToSimTime(
                metric->GetRealTimeStatMicro(EmitType::kComputeJoinStep3, key)),
            [this, metric]() {
                auto mk_total = metric->GenerateStatKey(EmitType::kTotalJoin, pid_);
                metric->Emit(EmitType::kTotalJoin, mk_total);
            });

        INFO("Vehicle-" << pid_ << " finishes joining, sid=" << ToString(join_ack->sid_)
                        << ", new ek=" << ka_proto_->GetEncryptionKey());
    }
    else
    {
        metric->Cancel(EmitType::kComputeJoinStep3, key);
        WARN("Vehicle-" << pid_ << " aborts joining, sid=" << ToString(join_ack->sid_));
    }
}

std::shared_ptr<SGCMessage>
SGCVehicle::EncapsulateKey(std::shared_ptr<SGCMessage> encap_ntf_msg)
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

    // metric emit
    auto metric = ns3::Singleton<Metric>::Get();
    std::string mk_comp = metric->GenerateStatKey(EmitType::kComputeEncap);
    metric->Emit(EmitType::kComputeEncap, mk_comp);

    // session key
    std::vector<uint8_t> key = GenerateRandomBytes(encap_ntf->key_length_);

    // construct encapsulation
    KeyVerifier kv;
    kv.version_ = encap_ntf->cur_key_version_ + 1;
    kv.timestamp_ = ns3::Simulator::Now();
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

    // metric emit
    metric->Emit(EmitType::kComputeEncap, mk_comp);

    INFO("Vehicle-" << pid_ << " encapsulates session key, key=" << ToString(key)
                    << ", version=" << encap->kv_.version_);

    // metric total cost
    uint hash_int = *reinterpret_cast<uint*>(kv.hash_.data());
    std::string mk_total = metric->GenerateStatKey(EmitType::kTotalKeyDistribution, hash_int);
    metric->Emit(EmitType::kTotalKeyDistribution, mk_total);
    return encap;
}

std::shared_ptr<SGCMessage>
SGCVehicle::LaunchKeyUpdate(uint32_t key_length)
{
    if (state_ != State::kJoined)
    {
        return nullptr;
    }
    // metric timecost
    auto metric = ns3::Singleton<Metric>::Get();
    std::string mk_comp = metric->GenerateStatKey(EmitType::kComputeEncap);
    metric->Emit(EmitType::kComputeEncap, mk_comp);

    // session key
    std::vector<uint8_t> key = GenerateRandomBytes(key_length);

    // construct update packet
    KeyVerifier kv;
    uint32_t ver = cur_kv_.version_;
    for (auto& v : pending_kvs_)
    {
        if (v.first > ver)
        {
            ver = v.first;
        }
    }
    kv.version_ = ver + 1;
    kv.timestamp_ = ns3::Simulator::Now();
    kv.hash_.resize(32);
    Sha256(reinterpret_cast<const char*>(key.data()),
           key.size(),
           reinterpret_cast<char*>(kv.hash_.data()));

    auto upd = std::make_shared<KeyUpd>();
    upd->kv_ = kv;
    upd->group_num_ = gsis_.size();
    upd->pid_ = pid_;

    std::vector<SAAGKA::EncryptionKey> eks;
    eks.reserve(gsis_.size());
    for (const auto& [group_seq, gsi_p] : gsis_)
    {
        upd->sids_.push_back(gsi_p->sid_);
        eks.push_back(gsi_p->ek_);
    }
    upd->ct_ = SAAGKA::Encrypt(key, eks);

    // set to pending list
    if (pending_kvs_.find(kv.version_) == pending_kvs_.end())
    {
        pending_kvs_[kv.version_] = std::vector<KeyTuple>();
    }
    pending_kvs_[kv.version_].emplace_back(kv, key);

    metric->Emit(EmitType::kComputeEncap, mk_comp);

    INFO("Vehicle-" << pid_ << " launches session key update, key=" << ToString(key)
                    << ", version=" << upd->kv_.version_);

    // metric total cost
    std::string mk_total_1 =
        metric->GenerateStatKey(EmitType::kTotalKeyUpdate1,
                                *reinterpret_cast<uint*>(upd->kv_.hash_.data()));
    std::string mk_total_2 =
        metric->GenerateStatKey(EmitType::kTotalKeyUpdate2,
                                *reinterpret_cast<uint*>(upd->kv_.hash_.data()));
    metric->Emit(EmitType::kTotalKeyUpdate1, mk_total_1);
    metric->Emit(EmitType::kTotalKeyUpdate2, mk_total_2);

    return upd;
}

void
SGCVehicle::DecapsulateKey(std::shared_ptr<SGCMessage> encap_msg)
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

    // avoid duplicate decapsulation
    uint32_t ver = encap->kv_.version_;
    auto it = pending_kvs_.find(ver);
    size_t target = -1;
    if (it != pending_kvs_.end())
    {
        for (size_t i = 0; i < it->second.size(); i++)
        {
            auto tuple = it->second[i];
            if (tuple.first.hash_ == encap->kv_.hash_)
            {
                target = i;

                if (tuple.second.size() > 0)
                {
                    return;
                }
            }
        }
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

    // metric emit
    auto metric = ns3::Singleton<Metric>::Get();
    std::string mk_comp = metric->GenerateStatKey(EmitType::kComputeDecap);
    metric->Emit(EmitType::kComputeDecap, mk_comp);

    // decap
    auto key = ka_proto_->Decrypt(encap->ct_, index);

    metric->Emit(EmitType::kComputeDecap, mk_comp);

    char hash_res[32];
    Sha256(reinterpret_cast<const char*>(key.data()), key.size(), hash_res);
    if (!IsEqual(encap->kv_.hash_, std::vector<uint8_t>(hash_res, hash_res + 32)))
    {
        WARN("Vehicle-" << pid_ << " decapsulates session key failed, reason=hash not match");
        return;
    }

    INFO("Vehicle-" << pid_ << " decapsulates session key success, key=" << ToString(key)
                    << ", version=" << encap->kv_.version_);

    if (it == pending_kvs_.end())
    {
        pending_kvs_[encap->kv_.version_] = std::vector<KeyTuple>();
        pending_kvs_[encap->kv_.version_].emplace_back(encap->kv_, key);
        return;
    }
    else if (target != -1)
    {
        // set session key
        auto tuple = it->second[target];
        cur_kv_ = encap->kv_;
        cur_session_key_ = key;
        CleanPendingKvs(encap->kv_.version_ + 1);
        INFO("Vehicle-" << pid_ << " updates session key, key=" << ToString(key)
                        << ", version=" << ver);
        return;
    }
    else
    {
        it->second.emplace_back(encap->kv_, key);
    }

    // metric total cost
    ns3::Simulator::Schedule(
        ConvertRealTimeToSimTime(metric->GetRealTimeStatMicro(EmitType::kComputeDecap, mk_comp)),
        [metric, encap]() {
            auto mk_total =
                metric->GenerateStatKey(EmitType::kTotalKeyDistribution,
                                        *reinterpret_cast<uint*>(encap->kv_.hash_.data()));

            metric->Emit(EmitType::kTotalKeyDistribution, mk_total);
        });
}

void
SGCVehicle::HandleKeyUpd(std::shared_ptr<SGCMessage> upd_msg)
{
    auto upd = std::dynamic_pointer_cast<KeyUpd>(upd_msg);
    if (!upd)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    if (upd->kv_.version_ <= cur_kv_.version_ || state_ != State::kJoined || upd->pid_ == pid_)
    {
        return;
    }

    // avoid duplicate decapsulation
    uint32_t ver = upd->kv_.version_;
    auto it = pending_kvs_.find(ver);
    size_t target = -1;
    if (it != pending_kvs_.end())
    {
        for (size_t i = 0; i < it->second.size(); i++)
        {
            auto tuple = it->second[i];
            if (tuple.first.hash_ == upd->kv_.hash_)
            {
                target = i;

                if (tuple.second.size() > 0)
                {
                    return;
                }
            }
        }
    }

    uint32_t index = -1;
    for (size_t i = 0; i < upd->sids_.size(); i++)
    {
        if (IsEqual(upd->sids_[i], sid_))
        {
            index = i;
            break;
        }
    }
    if (index == -1)
    {
        WARN("Vehicle-" << pid_ << " update session key failed, reason=sid not found");
        return;
    }

    // metric emit
    auto metric = ns3::Singleton<Metric>::Get();
    std::string mk_comp = metric->GenerateStatKey(EmitType::kComputeDecap);
    metric->Emit(EmitType::kComputeDecap, mk_comp);

    // decap
    auto key = ka_proto_->Decrypt(upd->ct_, index);

    metric->Emit(EmitType::kComputeDecap, mk_comp);

    char hash_res[32];
    Sha256(reinterpret_cast<const char*>(key.data()), key.size(), hash_res);
    if (!IsEqual(upd->kv_.hash_, std::vector<uint8_t>(hash_res, hash_res + 32)))
    {
        WARN("Vehicle-" << pid_ << " update session key failed, reason=hash not match");
        return;
    }

    INFO("Vehicle-" << pid_ << " recover updated session key, key=" << ToString(key)
                    << ", version=" << ver);

    if (it == pending_kvs_.end())
    {
        pending_kvs_[upd->kv_.version_] = std::vector<KeyTuple>();
        pending_kvs_[upd->kv_.version_].emplace_back(upd->kv_, key);
        return;
    }
    else if (target != -1)
    {
        // set session key
        auto tuple = it->second[target];
        cur_kv_ = upd->kv_;
        cur_session_key_ = key;
        CleanPendingKvs(upd->kv_.version_ + 1);
        INFO("Vehicle-" << pid_ << " updates session key (key update), key=" << ToString(key)
                        << ", version=" << ver << ", exec_time="
                        << metric->GetRealTimeStatMicro(EmitType::kComputeDecap, mk_comp) << " us");
    }
    else
    {
        it->second.emplace_back(upd->kv_, key);
    }
    // metric total cost
    ns3::Simulator::Schedule(
        ConvertRealTimeToSimTime(metric->GetRealTimeStatMicro(EmitType::kComputeDecap, mk_comp)),
        [metric, upd]() {
            uint hash_int = *reinterpret_cast<uint*>(upd->kv_.hash_.data());
            metric->Emit(EmitType::kTotalKeyUpdate1,
                         metric->GenerateStatKey(EmitType::kTotalKeyUpdate1, hash_int));
        });
}

void
SGCVehicle::HandleKeyUpdAck(std::shared_ptr<SGCMessage> ack_msg)
{
    auto upd_ack = std::dynamic_pointer_cast<KeyUpdAck>(ack_msg);
    if (!upd_ack)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    if (upd_ack->kv_.version_ <= cur_kv_.version_)
    {
        return;
    }

    // INFO("Vehicle-" << pid_ << " receives key update ack, kv=" << ToString(upd_ack->kv_.hash_)
    //                 << ", version=" << upd_ack->kv_.version_);
    TryUpdateSessionKey(upd_ack->kv_);

    // update successfully
    if (upd_ack->kv_.hash_ == cur_kv_.hash_ && upd_ack->pid_ != pid_)
    {
        // INFO("update_ack->pid_=" << upd_ack->pid_ << ", pid_=" << pid_);
        auto metric = ns3::Singleton<Metric>::Get();
        auto mk_total =
            metric->GenerateStatKey(EmitType::kTotalKeyUpdate2,
                                    *reinterpret_cast<uint*>(upd_ack->kv_.hash_.data()));
        metric->Emit(EmitType::kTotalKeyUpdate2, mk_total);
        return;
    }
}

void
SGCVehicle::CleanPendingKvs(uint32_t version)
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

uint32_t
SGCVehicle::GetPid() const
{
    return pid_;
}

void
SGCVehicle::TryUpdateSessionKey(const KeyVerifier& kv)
{
    if (kv.version_ <= cur_kv_.version_)
    {
        return;
    }

    auto it = pending_kvs_.find(kv.version_);
    if (it == pending_kvs_.end())
    {
        pending_kvs_[kv.version_] = std::vector<KeyTuple>();
        pending_kvs_[kv.version_].emplace_back(kv, std::vector<uint8_t>());
    }
    else
    {
        size_t index = -1;
        for (size_t i = 0; i < it->second.size(); i++)
        {
            auto& tuple = it->second[i];
            if (IsEqual(tuple.first.hash_, kv.hash_) && tuple.first.version_ == kv.version_)
            {
                index = i;
                break;
            }
        }
        if (index == -1)
        {
            it->second.emplace_back(kv, std::vector<uint8_t>());
        }
        else if (it->second[index].second.size() > 0)
        {
            // update session key
            cur_kv_ = kv;
            cur_session_key_ = it->second[index].second;
            INFO("Vehicle-" << pid_ << " updates session key, key=" << ToString(cur_session_key_)
                            << ", version=" << kv.version_);

            // ns3::Simulator::Schedule(ns3::Seconds(0), [kv]() {
            //     auto metric = ns3::Singleton<Metric>::Get();
            //     auto hash_int = *reinterpret_cast<const uint*>(kv.hash_.data());
            //     auto metric_key_upd = metric->GenerateStatKey(EmitType::kTotalKeyUpdate,
            //     hash_int); auto metric_key_dist =
            //         metric->GenerateStatKey(EmitType::kTotalKeyDistribution, hash_int);

            //     if (!metric->TryEmit(EmitType::kTotalKeyDistribution, metric_key_dist))
            //     {
            //         metric->TryEmit(EmitType::kTotalKeyUpdate, metric_key_upd);
            //     }
            // });
        }
    }
    // discard stale pending kvs
    CleanPendingKvs(kv.version_);
}
