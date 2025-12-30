#include "message/header.h"

#include "ns3/nstime.h"

#include <chrono>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

enum class EmitType
{
    kUnknown,
    kComputeSetup,
    kComputeKeyGen,
    kComputeInitOneGroup,
    kComputeJoinStep1,
    kComputeJoinStep2,
    kComputeJoinStep3,
    kComputeJoinStep4,
    kComputeEncap,
    kComputeDecap,

    kRealTimeEvtsNum,

    kTotalHearbeat = kRealTimeEvtsNum,
    kTotalJoin,
    kTotalKeyDistribution,
    kTotalKeyUpdate1,
    kTotalKeyUpdate2,

    kTypeNum,
};

inline std::ostream&
operator<<(std::ostream& os, EmitType type)
{
    switch (type)
    {
    case EmitType::kUnknown:
        os << "kUnknown";
        break;
    case EmitType::kComputeSetup:
        os << "kComputeSetup";
        break;
    case EmitType::kComputeKeyGen:
        os << "kComputeKeyGen";
        break;
    case EmitType::kComputeInitOneGroup:
        os << "kComputeInitOneGroup";
        break;
    case EmitType::kComputeJoinStep1:
        os << "kComputeJoinStep1";
        break;
    case EmitType::kComputeJoinStep2:
        os << "kComputeJoinStep2";
        break;
    case EmitType::kComputeJoinStep3:
        os << "kComputeJoinStep3";
        break;
    case EmitType::kComputeJoinStep4:
        os << "kComputeJoinStep4";
        break;
    case EmitType::kComputeEncap:
        os << "kComputeEncap";
        break;
    case EmitType::kComputeDecap:
        os << "kComputeDecap";
        break;
    case EmitType::kTotalHearbeat:
        os << "kTotalHearbeat";
        break;
    case EmitType::kTotalJoin:
        os << "kTotalJoin";
        break;
    case EmitType::kTotalKeyDistribution:
        os << "kTotalKeyDistribution";
        break;
    case EmitType::kTotalKeyUpdate1:
        os << "kTotalKeyUpdate1";
        break;
    case EmitType::kTotalKeyUpdate2:
        os << "kTotalKeyUpdate2";
        break;
    default:
        os << "Unknown EmitType";
        break;
    }
    return os;
}

const std::unordered_map<EmitType, int> TotalPhaseNum = {
    {EmitType::kComputeSetup, 2},
    {EmitType::kComputeKeyGen, 2},
    {EmitType::kComputeInitOneGroup, 2},
    {EmitType::kComputeJoinStep1, 2},
    {EmitType::kComputeJoinStep2, 2},
    {EmitType::kComputeJoinStep3, 2},
    {EmitType::kComputeJoinStep4, 2},
    {EmitType::kComputeEncap, 2},
    {EmitType::kComputeDecap, 2},
    {EmitType::kTotalHearbeat, 2},
    {EmitType::kTotalJoin, 2},
    {EmitType::kTotalKeyDistribution, 2},
    {EmitType::kTotalKeyUpdate1, 2},
    {EmitType::kTotalKeyUpdate2, 2},
};

class Metric
{
  public:
    using Microseconds = std::chrono::microseconds;

    struct MetricValueReal
    {
        std::chrono::time_point<std::chrono::steady_clock> start_time_;
        int phase_;
    };

    struct MetricValueSim
    {
        ns3::Time start_time_;
        int phase_;
    };

    Metric();
    virtual ~Metric();
    void Emit(EmitType type, std::string key);
    void Emit(MsgType type, int size);
    bool TryEmit(EmitType type, std::string key);
    void Cancel(EmitType type, std::string key);
    void Summarize();

    // Generate a new stat key
    std::string GenerateStatKey(EmitType type);
    // Splice type and cnt to form a stat key, which may already exist
    std::string GenerateStatKey(EmitType type, uint cnt);

    Microseconds GetRealTimeStatMicro(EmitType type, std::string key);

  private:
    void DoEmitSimulatorTime(EmitType type, std::string key);
    void DoEmitRealTime(EmitType type, std::string key);

    std::vector<std::map<std::string, MetricValueReal>> pending_evs_real_;
    std::vector<std::map<std::string, MetricValueSim>> pending_evs_sim_;
    std::vector<std::map<std::string, Microseconds>> stats_real_;
    std::vector<std::map<std::string, ns3::Time>> stats_sim_;
    std::vector<uint> event_counter_;
    std::set<std::string> cancelled_keys_;

    std::map<MsgType, int> msg_size_;
    std::vector<std::once_flag> once_flags_;
};
