#include "ns3/nstime.h"

#include <chrono>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

enum class EmitType
{
    kAlgSetup,
    kAlgKeyGen,
    kAlgMsg,
    kAlgAgree,
    kAlgEnc,
    kAlgDec,
    kUnknown
};

const std::unordered_map<EmitType, int> TotalPhaseNum = {
    {EmitType::kAlgSetup, 2},
    {EmitType::kAlgKeyGen, 2},
    {EmitType::kAlgMsg, 2},
    {EmitType::kAlgAgree, 2},
    {EmitType::kAlgEnc, 2},
    {EmitType::kAlgDec, 2},
};

class Metric
{
  public:
    using Microseconds = std::chrono::microseconds;

    struct MetricValue
    {
        std::chrono::time_point<std::chrono::steady_clock> start_time_;
        int phase_;
    };

    Metric();
    virtual ~Metric();
    void Emit(EmitType type, std::string key);
    Microseconds GetStat(EmitType type, std::string key);

    static ns3::Time ChronoToSimulatorTimeMS(Microseconds time);

  private:
    std::vector<std::map<std::string, MetricValue>> pending_evs_;
    std::vector<std::map<std::string, Microseconds>> stats_;
};
