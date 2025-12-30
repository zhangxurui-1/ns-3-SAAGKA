#pragma once

#include "sgc.h"

#include <cstdint>

class SGCRSU : public SGC
{
  public:
    enum class State : uint32_t
    {
        kIdle,
        kHandleJoin,
    };

    struct VehicleInfo
    {
        uint32_t pid_;
        uint32_t group_seq_;
        uint32_t pos_;
        bool has_joined_;
        ns3::Time last_hback_time_;
        std::multimap<ns3::Time, uint32_t>::iterator time_it_;
    };

    SGCRSU(int size, int max_group_num, ns3::Time key_upd_threshold);
    virtual ~SGCRSU();

    std::vector<std::shared_ptr<SGCMessage>> HandleMsg(const uint8_t* bytes, size_t len) override;

    std::shared_ptr<SGCMessage> HeartbeatMsg();
    std::shared_ptr<SGCMessage> HandleHeartbeatAck(std::shared_ptr<SGCMessage> hb_ack_msg);
    std::shared_ptr<SGCMessage> AckJoin(std::shared_ptr<SGCMessage> join_msg);
    std::shared_ptr<SGCMessage> NotifyKeyEncap();
    void HandleKeyEncap(std::shared_ptr<SGCMessage> encap_msg); // RSU handle
    std::shared_ptr<SGCMessage> HandleKeyUpd(std::shared_ptr<SGCMessage> upd_msg);

  private:
    void LazyDropVehicleInfo(ns3::Time timeout);

    static uint32_t group_seq_; // unique sequence number of groups (start from 1)

    State state_;
    KeyVerifier cur_kv_;
    uint32_t size_param_{std::numeric_limits<uint32_t>::max()};
    uint32_t group_size_;
    uint32_t max_group_num_;
    uint32_t hb_counter_{0};
    int pending_join_pid_{-1};
    int cur_group_seq_{-1};
    int sk_dispatcher_{-1};
    std::unordered_map<uint32_t, std::shared_ptr<VehicleInfo>> vis_; // pid -> vehicle info
    std::multimap<ns3::Time, uint32_t> hb_ack_time_;
    ns3::Time key_upd_threshold_;
    ns3::Time last_key_upd_time_{ns3::Seconds(0)};

    // for metric
    std::string metric_join_commu_key_;
};

inline std::ostream&
operator<<(std::ostream& os, SGCRSU::State st)
{
    switch (st)
    {
    case SGCRSU::State::kIdle:
        os << "kIdle";
        break;
    case SGCRSU::State::kHandleJoin:
        os << "kHandleJoin";
        break;
    default:
        os << "Unknown";
        break;
    }
    return os;
}
