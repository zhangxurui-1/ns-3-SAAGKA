#include "sgc-rsu.h"

#include "../application.h"
#include "../crypto/agka.h"
#include "../crypto/pki.h"
#include "../crypto/utils.h"
#include "../message/bytereader.h"
#include "../message/header.h"
#include "../message/message.h"
#include "../utils.h"
#include "sgc-vehicle.h"
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
#include <vector>

uint32_t SGCRSU::group_seq_ = 0;

SGCRSU::SGCRSU(int size, int max_group_num, ns3::Time key_upd_threshold)
    : state_(State::kIdle),
      max_group_num_(max_group_num),
      key_upd_threshold_(key_upd_threshold)
{
    auto pp = SAAGKA::GetPublicParameter();
    for (size_t i = 0; i < pp->matrices.size(); i++)
    {
        if (pp->matrices[i].size() >= size)
        {
            size_param_ = i;
            break;
        }
    }
    if (size_param_ == std::numeric_limits<uint32_t>::max())
    {
        FATAL_ERROR("Group size " << size << " too large, not supported");
    }
    group_size_ = pp->matrices[size_param_].size();
}

SGCRSU::~SGCRSU()
{
}

std::shared_ptr<SGCMessage>
SGCRSU::HeartbeatMsg()
{
    auto metric = ns3::Singleton<Metric>::Get();
    // handle the case where last heartbeat is not received by any vehicle
    if (hb_counter_ > 0)
    {
        metric->Cancel(EmitType::kTotalHearbeat,
                       metric->GenerateStatKey(EmitType::kTotalHearbeat, hb_counter_ - 1));
    }
    // check state, to avoid hanging on
    if (state_ != State::kIdle)
    {
        auto it = vis_[pending_join_pid_]->time_it_;
        vis_.erase(pending_join_pid_);
        hb_ack_time_.erase(it);

        state_ = State::kIdle;
    }
    pending_join_pid_ = -1;

    // check if current group is full
    auto it = gsis_.find(cur_group_seq_);
    if (it == gsis_.end() || it->second->n_member_ == group_size_)
    {
        // emit metric
        auto mk = metric->GenerateStatKey(EmitType::kComputeInitOneGroup);
        metric->Emit(EmitType::kComputeInitOneGroup, mk);

        auto new_group_seq = group_seq_++;
        cur_group_seq_ = new_group_seq;
        auto gsi_p = std::make_shared<SGC::GroupSessionInfo>(new_group_seq, size_param_);
        INFO("RSU created Group-" << new_group_seq << ", sid=" << ToString(gsi_p->sid_));
        gsis_.emplace(new_group_seq, gsi_p);

        metric->Emit(EmitType::kComputeInitOneGroup, mk);
    }
    // construct heartbeat
    auto hb = std::make_shared<Heartbeat>();
    hb->kv_ = cur_kv_;
    hb->hb_seq_ = hb_counter_++;
    for (const auto& [sid_key, ptr_gsi] : gsis_)
    {
        hb->gsis_.push_back(*ptr_gsi);
    }
    // emit metric
    metric->Emit(EmitType::kTotalHearbeat,
                 metric->GenerateStatKey(EmitType::kTotalHearbeat, hb->hb_seq_));

    return hb;
}

std::vector<std::shared_ptr<SGCMessage>>
SGCRSU::HandleMsg(const uint8_t* bytes, size_t len)
{
    ByteReader br(bytes, len);
    auto header = br.read<Header>();

    auto metric = ns3::Singleton<Metric>::Get();
    metric->Emit(header.type_, br.remaining());

    std::vector<std::shared_ptr<SGCMessage>> resp;

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
    }
    break;

    case MsgType::kJoin: {
        auto join_msg = std::make_shared<Join>();
        auto st = std::chrono::steady_clock::now();
        join_msg->Deserialize(br);
        auto deserialized_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - st);
        INFO("Deserialize Join message cost " << deserialized_time.count() << " (ms)");

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
    case MsgType::kKeyUpdate: {
        auto key_upd_msg = std::make_shared<KeyUpd>();
        key_upd_msg->Deserialize(br);
        resp.push_back(key_upd_msg);
        auto upd_resp = HandleKeyUpd(key_upd_msg);
        if (upd_resp)
        {
            resp.push_back(upd_resp);
        }
    }
    break;

    default:
        // INFO("RSU receives unknown message type: " << header.type_);
        break;
    }

    return resp;
}

std::shared_ptr<SGCMessage>
SGCRSU::HandleHeartbeatAck(std::shared_ptr<SGCMessage> hb_ack_msg)
{
    auto hb_ack = std::dynamic_pointer_cast<HeartbeatAck>(hb_ack_msg);
    if (!hb_ack)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }
    // update vehicle info
    auto now = ns3::Simulator::Now();

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
    if (state_ == State::kIdle && hb_ack->state_ == SGCVehicle::State::kPrepare &&
        hb_ack->hb_seq_ == hb_counter_ - 1 && pending_join_pid_ == -1)
    {
        resp = std::make_shared<NotifyPosition>();
        auto it = gsis_.find(cur_group_seq_);

        if (it == gsis_.end() || it->second->n_member_ == group_size_)
        {
            FATAL_ERROR("Unexpected error: Group-" << cur_group_seq_ << " not exist");
        }

        auto gsi_p = it->second;
        resp->pos_ = gsi_p->n_member_; // n_member_ is not incremented for now
        resp->sid_ = gsi_p->sid_;
        resp->pid_ = hb_ack->pid_;

        vis_[hb_ack->pid_]->group_seq_ = cur_group_seq_;
        vis_[hb_ack->pid_]->pos_ = resp->pos_;

        state_ = State::kHandleJoin;
        pending_join_pid_ = hb_ack->pid_;

        INFO("RSU instruct Vehicle-" << hb_ack->pid_ << " to join Group-" << ToString(resp->sid_)
                                     << ", pos=" << resp->pos_);
    }
    return resp;
}

std::shared_ptr<SGCMessage>
SGCRSU::AckJoin(std::shared_ptr<SGCMessage> join_msg)
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
        kam.pos != vis_[join->pid_]->pos_ || cur_group_seq_ != ParseGroupSeqFromSid(kam.sid))
    {
        INFO("RSU reject member joining, reason=malicious message");
        return nullptr;
    }
    // metric emit
    auto metric = ns3::Singleton<Metric>::Get();
    std::string mk = metric->GenerateStatKey(EmitType::kComputeJoinStep2);
    metric->Emit(EmitType::kComputeJoinStep2, mk);

    // if (!SAAGKA::CheckValid(join->kam_))
    // {
    //     WARN("RSU check validity failed");
    //     return nullptr;
    // }

    auto it = gsis_.find(cur_group_seq_);

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

    INFO("RSU ack joining of Vehicle-" << join->pid_);
    // metric emit
    metric->Emit(EmitType::kComputeJoinStep2, mk);
    return resp;
}

std::shared_ptr<SGCMessage>
SGCRSU::NotifyKeyEncap()
{
    LazyDropVehicleInfo(ns3::Seconds(2));

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

void
SGCRSU::HandleKeyEncap(std::shared_ptr<SGCMessage> encap_msg)
{
    auto encap = std::dynamic_pointer_cast<KeyEncap>(encap_msg);
    if (!encap)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    // If the key is rejected, cancel the metric
    auto metric = ns3::Singleton<Metric>::Get();
    auto mk_cancel_key = metric->GenerateStatKey(EmitType::kTotalKeyDistribution,
                                                 *reinterpret_cast<uint*>(encap->kv_.hash_.data()));

    if (encap->pid_ != sk_dispatcher_)
    {
        metric->Cancel(EmitType::kTotalKeyDistribution, mk_cancel_key);

        WARN("RSU rejects encap from Vehicle-" << encap->pid_ << ", reason=not dispatcher");
        return;
    }

    sk_dispatcher_ = -1;
    auto now = ns3::Simulator::Now();
    if (cur_kv_.version_ >= encap->kv_.version_ || encap->kv_.timestamp_ + ns3::Seconds(1) < now)
    {
        metric->Cancel(EmitType::kTotalKeyDistribution, mk_cancel_key);

        WARN("RSU rejects encap from Vehicle-" << encap->pid_ << ", reason=stale key");
        return;
    }
    INFO("RSU accept key verifier (DISTRIBUTION), kv=" << encap->kv_ << ", from=Vehicle-"
                                                       << encap->pid_);
    cur_kv_ = encap->kv_;
    last_key_upd_time_ = now;
}

std::shared_ptr<SGCMessage>
SGCRSU::HandleKeyUpd(std::shared_ptr<SGCMessage> upd_msg)
{
    auto upd = std::dynamic_pointer_cast<KeyUpd>(upd_msg);
    if (!upd)
    {
        FATAL_ERROR("Unexpcted downcast error");
    }

    auto now = ns3::Simulator::Now();
    if (cur_kv_.version_ >= upd->kv_.version_ || upd->kv_.timestamp_ + ns3::Seconds(1) < now)
    {
        WARN("RSU rejects update from Vehicle-" << upd->pid_ << ", reason=stale key");
        return nullptr;
    }
    if (now - last_key_upd_time_ < key_upd_threshold_)
    {
        WARN("RSU rejects update from Vehicle-" << upd->pid_ << ", reason=key update too frequent");
        return nullptr;
    }

    INFO("RSU accept key verifier (UPDATE), kv=" << upd->kv_ << ", from=Vehicle-" << upd->pid_);
    cur_kv_ = upd->kv_;
    last_key_upd_time_ = now;

    auto upd_ack = std::make_shared<KeyUpdAck>();
    upd_ack->kv_ = upd->kv_;
    upd_ack->pid_ = upd->pid_;
    return upd_ack;
}

void
SGCRSU::LazyDropVehicleInfo(ns3::Time timeout)
{
    const auto now = ns3::Simulator::Now();
    auto it = hb_ack_time_.begin();
    while (it != hb_ack_time_.end())
    {
        if (it->first + timeout < now)
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
