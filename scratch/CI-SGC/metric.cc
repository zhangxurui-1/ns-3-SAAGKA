#include "metric.h"

#include <chrono>
#include <cstddef>
#include <utility>
#include <vector>

Metric::Metric()
{
    pending_evs_.resize((size_t)EmitType::kUnknown);
    stats_.resize((size_t)EmitType::kUnknown);
}

Metric::~Metric()
{
}

void
Metric::Emit(EmitType type, std::string key)
{
    auto& ev = pending_evs_[(size_t)type];
    auto v = ev.find(key);
    if (v == ev.end())
    {
        ev[key] = MetricValue{std::chrono::steady_clock::now(), 1};
    }
    else
    {
        v->second.phase_++;
        int ph = v->second.phase_;
        if (ph == TotalPhaseNum.find(type)->second)
        {
            auto now = std::chrono::steady_clock::now();
            auto& s = stats_[(size_t)type];
            s[key] = std::chrono::duration_cast<Microseconds>(now - v->second.start_time_);
        }
    }
}

Metric::Microseconds
Metric::GetStat(EmitType type, std::string key)
{
    auto st = stats_[(size_t)type].find(key);
    if (st == stats_[(size_t)type].end())
    {
        return Microseconds(0);
    }

    return st->second;
}

ns3::Time
Metric::ChronoToSimulatorTimeMS(Microseconds time)
{
    return ns3::MilliSeconds(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
}
