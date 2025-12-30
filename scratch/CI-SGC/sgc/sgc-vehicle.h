#pragma once

#include "sgc.h"

#include <cstdint>
#include <memory>

class SGCVehicle : public SGC
{
  public:
    enum class State : uint32_t
    {
        kDefault,
        kPrepare,
        kJoining,
        kJoined,
    };
    SGCVehicle();
    virtual ~SGCVehicle();
    void Enroll();

    std::vector<std::shared_ptr<SGCMessage>> HandleMsg(const uint8_t* bytes, size_t len) override;

    std::shared_ptr<SGCMessage> HandleHeartbeat(std::shared_ptr<SGCMessage> hb_msg);
    std::shared_ptr<SGCMessage> HandleHeartbeatAck(std::shared_ptr<SGCMessage> hb_ack_msg);
    std::shared_ptr<SGCMessage> LaunchJoin(std::shared_ptr<SGCMessage> pos_notify_msg);
    void HandleJoin(std::shared_ptr<SGCMessage> join_msg); // handle the joining of other vehicles
    void HandleJoinAck(std::shared_ptr<SGCMessage> join_ack_msg);
    std::shared_ptr<SGCMessage> EncapsulateKey(std::shared_ptr<SGCMessage> encap_ntf_msg);
    std::shared_ptr<SGCMessage> LaunchKeyUpdate(uint32_t key_length);
    void DecapsulateKey(std::shared_ptr<SGCMessage> encap_msg); // vehicle handle
    void HandleKeyUpd(std::shared_ptr<SGCMessage> upd_msg);
    void HandleKeyUpdAck(std::shared_ptr<SGCMessage> ack_msg);

    uint32_t GetPid() const;

    // int ParseSid(std::vector<uint8_t> sid);

  private:
    void CleanPendingKvs(uint32_t version);
    // Try to update session key. If failed, the key verifier will be stored in pending_kvs_.
    void TryUpdateSessionKey(const KeyVerifier& kv);

    static uint32_t user_seq_; // unique sequence number of users (start from 0)

    State state_;
    uint32_t pid_;
    uint32_t pos_;
    std::vector<uint8_t> sid_; // the group session that node belongs to
    std::shared_ptr<SAAGKA> ka_proto_;
    KeyVerifier cur_kv_;
    std::vector<uint8_t> cur_session_key_;

    using KeyTuple = std::pair<KeyVerifier, std::vector<uint8_t>>;
    std::unordered_map<uint32_t, std::vector<KeyTuple>> pending_kvs_;
    std::set<uint32_t> gap_positions_;

    // for metric
    std::string metric_join_commu_key_;
};

inline std::ostream&
operator<<(std::ostream& os, SGCVehicle::State st)
{
    switch (st)
    {
    case SGCVehicle::State::kDefault:
        os << "kDefault";
        break;
    case SGCVehicle::State::kPrepare:
        os << "kPrepare";
        break;
    case SGCVehicle::State::kJoining:
        os << "kJoining";
        break;
    case SGCVehicle::State::kJoined:
        os << "kJoined";
        break;
    default:
        os << "Unknown";
        break;
    }
    return os;
}
