#pragma once

#include "crypto/agka.h"
#include "crypto/utils.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

class SGCMessage;

class SGC
{
  public:
    enum class Role
    {
        kRoleUnknown,
        kRoleVehicle,
        kRoleRSU,
    };

    enum class State : uint32_t
    {
        kDefault,
        kPosUnset,
        kPrepare,
        kJoining,
        kJoined,

        kIdle,
        kHandleJoin,
    };

    class GroupSessionInfo
    {
      public:
        uint32_t size_param_;
        uint32_t n_member_;

        // sid format:
        // ---------------------------------------------------------------------------
        // | prefix (4B) | sequence number (2B) | size_param (2B) | expiry time (8B) |
        // ---------------------------------------------------------------------------
        std::vector<uint8_t> sid_;

        std::vector<uint8_t> mem_bitmap_;

        SAAGKA::EncryptionKey ek_;
        std::chrono::time_point<std::chrono::steady_clock> expiry_time_;
        std::vector<G1> d_; // only maintained by RSU

        GroupSessionInfo(uint16_t seq, uint16_t size_param);
        GroupSessionInfo() = default;
        GroupSessionInfo(const GroupSessionInfo& gsi);

        friend ostream& operator<<(ostream& os, GroupSessionInfo& gsi)
        {
            std::string sid_str(gsi.sid_.begin(), gsi.sid_.end());
            os << "{ sid=" << sid_str << " (" << gsi.sid_.size()
               << "B), size_param=" << gsi.size_param_ << ", n_member=" << gsi.n_member_
               << ", expiry_time=" << gsi.expiry_time_.time_since_epoch() << ", ek=" << gsi.ek_
               << " }";
            return os;
        }
    };

    // ignore PID and signature for now
    class KeyVerifier
    {
      public:
        std::vector<uint8_t> hash_;
        std::chrono::time_point<std::chrono::steady_clock> timestamp_;
        uint32_t version_;

        KeyVerifier() = default;

        friend ostream& operator<<(ostream& os, const KeyVerifier& kv)
        {
            os << "{hash=" << ToString(kv.hash_)
               << ", timestamp=" << kv.timestamp_.time_since_epoch() << ", version=" << kv.version_
               << "}";
            return os;
        }
    };

    struct VehicleInfo
    {
        using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

        uint32_t pid_;
        uint32_t group_seq_;
        uint32_t pos_;
        bool has_joined_;
        TimePoint last_hback_time_;
        std::multimap<TimePoint, uint32_t>::iterator time_it_;
    };

    const static int KeyVerifierLength;
    const static int MaxGroupNum;
    const static int SidLength;

    SGC(Role r = Role::kRoleVehicle);
    virtual ~SGC();
    void Enroll();

    std::vector<std::shared_ptr<SGCMessage>> HandleMsg(const uint8_t* bytes, size_t len);

    std::shared_ptr<SGCMessage> HeartbeatMsg();
    std::shared_ptr<SGCMessage> HandleHeartbeat(std::shared_ptr<SGCMessage> hb_msg);
    std::shared_ptr<SGCMessage> HandleHeartbeatAck(std::shared_ptr<SGCMessage> hb_ack_msg);
    std::shared_ptr<SGCMessage> LaunchJoin(std::shared_ptr<SGCMessage> pos_notify_msg);
    void HandleJoin(std::shared_ptr<SGCMessage> join_msg);                     // vehicle handle
    std::shared_ptr<SGCMessage> AckJoin(std::shared_ptr<SGCMessage> join_msg); // RSU handle
    void HandleJoinAck(std::shared_ptr<SGCMessage> join_ack_msg);
    std::shared_ptr<SGCMessage> NotifyKeyEncap();
    std::shared_ptr<SGCMessage> EncapsulateKey(std::shared_ptr<SGCMessage> encap_ntf_msg);
    void HandleKeyEncap(std::shared_ptr<SGCMessage> encap_msg); // RSU handle
    void DecapsulateKey(std::shared_ptr<SGCMessage> encap_msg); // vehicle handle

    uint32_t GetPid() const;
    bool IsPositionOccupied(std::vector<uint8_t> sid, uint32_t pos);
    void OccupyOnePosition(std::vector<uint8_t> sid, uint32_t pos);

    // int ParseSid(std::vector<uint8_t> sid);

  private:
    void CleanPendingKvs(uint32_t version);
    void LazyDropVehicleInfo(uint32_t timeout_seconds);

    static uint32_t group_seq_; // unique sequence number of groups (start from 1)
    static uint32_t user_seq_;  // unique sequence number of users (start from 0)

    Role role_;

    State state_;
    uint32_t pid_;
    uint32_t pos_;
    std::vector<uint8_t> sid_; // the group session that node belongs to
    std::unordered_map<uint32_t, std::shared_ptr<GroupSessionInfo>> gsis_;
    std::shared_ptr<SAAGKA> ka_proto_;
    KeyVerifier cur_kv_;
    std::vector<uint8_t> cur_session_key_;

    using KeyTuple = std::pair<KeyVerifier, std::vector<uint8_t>>;
    std::unordered_map<uint32_t, std::vector<KeyTuple>> pending_kvs_;
    std::set<uint32_t> gap_slots_;

    // only maintained by RSU, pid -> (group_seq, pos)
    uint32_t pending_join_pid_;
    uint32_t hb_counter_{0};
    int sk_dispatcher_{-1};
    std::unordered_map<uint32_t, std::shared_ptr<VehicleInfo>> vis_;
    std::multimap<VehicleInfo::TimePoint, uint32_t> hb_ack_time_;
};

inline std::ostream&
operator<<(std::ostream& os, SGC::State s)
{
    switch (s)
    {
    case SGC::State::kDefault:
        return os << "kDefault";
    case SGC::State::kPosUnset:
        return os << "kPosUnset";
    case SGC::State::kPrepare:
        return os << "kPrepare";
    case SGC::State::kJoining:
        return os << "kJoining";
    case SGC::State::kJoined:
        return os << "kJoined";
    case SGC::State::kIdle:
        return os << "kIdle";
    case SGC::State::kHandleJoin:
        return os << "kHandleJoin";
    default:
        return os << "State(" << static_cast<uint32_t>(s) << ")";
    }
}
