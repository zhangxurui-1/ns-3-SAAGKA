#pragma once

#include "../crypto/agka.h"
#include "../crypto/utils.h"

#include "ns3/nstime.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <vector>

class SGCMessage;

class SGC
{
  public:
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
        ns3::Time expiry_time_;
        std::vector<G1> d_; // only maintained by RSU

        GroupSessionInfo(uint16_t seq, uint16_t size_param);
        GroupSessionInfo() = default;
        GroupSessionInfo(const GroupSessionInfo& gsi);

        friend ostream& operator<<(ostream& os, GroupSessionInfo& gsi)
        {
            std::string sid_str(gsi.sid_.begin(), gsi.sid_.end());
            os << "{ sid=" << sid_str << " (" << gsi.sid_.size()
               << "B), size_param=" << gsi.size_param_ << ", n_member=" << gsi.n_member_
               << ", expiry_time=" << gsi.expiry_time_.As(ns3::Time::MS) << ", ek=" << gsi.ek_
               << " }";
            return os;
        }
    };

    class KeyVerifier
    {
      public:
        std::vector<uint8_t> hash_;
        ns3::Time timestamp_;
        uint32_t version_;

        KeyVerifier() = default;

        friend ostream& operator<<(ostream& os, const KeyVerifier& kv)
        {
            os << "{hash=" << ToString(kv.hash_)
               << ", timestamp=" << kv.timestamp_.As(ns3::Time::MS) << ", version=" << kv.version_
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
    const static int SidLength;

    SGC() = default;
    virtual ~SGC() = default;

    virtual std::vector<std::shared_ptr<SGCMessage>> HandleMsg(const uint8_t* bytes, size_t len);
    bool IsPositionOccupied(std::vector<uint8_t> sid, uint32_t pos) const;
    void OccupyOnePosition(std::vector<uint8_t> sid, uint32_t pos);

  private:
  protected:
    std::unordered_map<uint32_t, std::shared_ptr<GroupSessionInfo>> gsis_;
    KeyVerifier cur_kv_;
};
