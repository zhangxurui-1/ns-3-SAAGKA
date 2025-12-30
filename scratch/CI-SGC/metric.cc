#include "metric.h"

#include "crypto/agka.h"
#include "utils.h"

#include "ns3/simulator.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

Metric::Metric()
    : event_counter_((size_t)EmitType::kTypeNum, 0),
      once_flags_((size_t)MsgType::kMsgTypeNum)
{
    auto real_time_evts_num = static_cast<size_t>(EmitType::kRealTimeEvtsNum);
    auto sim_time_evts_num =
        static_cast<size_t>(EmitType::kTypeNum) - static_cast<size_t>(EmitType::kRealTimeEvtsNum);
    pending_evs_real_.resize(real_time_evts_num);
    pending_evs_sim_.resize(sim_time_evts_num);
    stats_real_.resize(real_time_evts_num);
    stats_sim_.resize(sim_time_evts_num);
}

Metric::~Metric()
{
}

void
Metric::Emit(EmitType type, std::string key)
{
    if (cancelled_keys_.find(key) != cancelled_keys_.end())
    {
        return;
    }

    if (type < EmitType::kRealTimeEvtsNum)
    {
        DoEmitRealTime(type, key);
    }
    else
    {
        DoEmitSimulatorTime(type, key);
    }
}

void
Metric::Emit(MsgType type, int size)
{
    std::call_once(once_flags_[(size_t)type], [&]() { this->msg_size_[type] = size; });
}

bool
Metric::TryEmit(EmitType type, std::string key)
{
    if (cancelled_keys_.find(key) != cancelled_keys_.end())
    {
        return false;
    }

    if (type < EmitType::kRealTimeEvtsNum)
    {
        if (pending_evs_real_[(size_t)type].find(key) != pending_evs_real_[(size_t)type].end())
        {
            DoEmitRealTime(type, key);
        }
    }
    else
    {
        auto index = (size_t)type - static_cast<size_t>(EmitType::kRealTimeEvtsNum);
        if (pending_evs_sim_[index].find(key) != pending_evs_sim_[index].end())
        {
            DoEmitSimulatorTime(type, key);
        }
    }
}

void
Metric::Cancel(EmitType type, std::string key)
{
    cancelled_keys_.insert(key);
    if (type < EmitType::kRealTimeEvtsNum)
    {
        pending_evs_real_[(size_t)type].erase(key);
    }
    else
    {
        pending_evs_sim_[(size_t)type - static_cast<size_t>(EmitType::kRealTimeEvtsNum)].erase(key);
    }
}

// Metric::Microseconds
// Metric::GetStat(EmitType type, std::string key)
// {
//     auto st = stats_[(size_t)type].find(key);
//     if (st == stats_[(size_t)type].end())
//     {
//         return Microseconds(0);
//     }

//     return st->second;
// }

Metric::Microseconds
Metric::GetRealTimeStatMicro(EmitType type, std::string key)
{
    if (type < EmitType::kRealTimeEvtsNum)
    {
        auto st = stats_real_[(size_t)type].find(key);
        if (st == stats_real_[(size_t)type].end())
        {
            return Microseconds(0);
        }

        return st->second;
    }
    else
    {
        WARN("GetRealTimeStatMicro: type " << type << "is not real time event");
        return Microseconds(0);
    }
}

void
Metric::Summarize()
{
    std::cout << "Metric Summary:" << std::endl;
    std::cout << "\tReal Time:" << std::endl;
    for (size_t i = 0; i < stats_real_.size(); i++)
    {
        auto& st = stats_real_[i];
        std::cout << "\t\t" << EmitType(i) << ":" << st.size() << " events" << std::endl;
        Microseconds total(0);
        for (auto& p : st)
        {
            std::cout << "\t\t\t" << p.first << ":" << p.second.count() << " us" << std::endl;
            total += p.second;
        }
        std::cout << "\t\t\tAvg:" << total.count() / st.size() << " us\n\n";
    }

    std::cout << "\t Simulator Time:" << std::endl;
    for (size_t i = 0; i < stats_sim_.size(); i++)
    {
        auto& st = stats_sim_[i];
        std::cout << "\t\t" << EmitType(i + static_cast<size_t>(EmitType::kRealTimeEvtsNum)) << ":"
                  << st.size() << " events" << std::endl;
        ns3::Time total(0);
        for (auto& p : st)
        {
            std::cout << "\t\t\t" << p.first << ":" << p.second.GetMicroSeconds() << " us"
                      << std::endl;
            total += p.second;
        }
        std::cout << "\t\t\tAvg:" << total.GetMicroSeconds() / st.size() << " us\n\n";
    }

    std::cout << "\tMsg Size:" << std::endl;
    for (auto& p : msg_size_)
    {
        std::cout << "\t\t" << p.first << ":" << p.second << " bytes" << std::endl;
    }
}

std::string
Metric::GenerateStatKey(EmitType type)
{
    std::stringstream ss;
    ss << type << "-" << event_counter_[(size_t)type];
    event_counter_[(size_t)type]++;
    return ss.str();
}

std::string
Metric::GenerateStatKey(EmitType type, uint cnt)
{
    std::stringstream ss;
    ss << type << "-" << cnt;
    return ss.str();
}

void
Metric::DoEmitSimulatorTime(EmitType type, std::string key)
{
    if (type < EmitType::kRealTimeEvtsNum)
    {
        WARN("Emit Error, reason=wrong emit type");
        return;
    }
    size_t index = static_cast<size_t>(type) - static_cast<size_t>(EmitType::kRealTimeEvtsNum);

    // skip if key already in stats
    if (stats_sim_[index].find(key) != stats_sim_[index].end())
    {
        return;
    }

    auto& ev = pending_evs_sim_[index];
    auto v = ev.find(key);
    if (v == ev.end())
    {
        ev[key] = MetricValueSim{ns3::Simulator::Now(), 1};
    }
    else
    {
        v->second.phase_++;
        int ph = v->second.phase_;
        if (ph == TotalPhaseNum.find(type)->second)
        {
            auto now = ns3::Simulator::Now();
            auto& s = stats_sim_[index];
            s[key] = now - v->second.start_time_;
            pending_evs_sim_[index].erase(key);
        }
    }
}

void
Metric::DoEmitRealTime(EmitType type, std::string key)
{
    if (type >= EmitType::kRealTimeEvtsNum)
    {
        WARN("Emit Error, reason=wrong emit type");
        return;
    }

    // skip if key already emitted
    if (stats_real_[(size_t)type].find(key) != stats_real_[(size_t)type].end())
    {
        return;
    }

    auto& ev = pending_evs_real_[(size_t)type];
    auto v = ev.find(key);
    if (v == ev.end())
    {
        ev[key] = MetricValueReal{std::chrono::steady_clock::now(), 1};
    }
    else
    {
        v->second.phase_++;
        int ph = v->second.phase_;
        if (ph == TotalPhaseNum.find(type)->second)
        {
            auto now = std::chrono::steady_clock::now();
            auto& s = stats_real_[(size_t)type];
            s[key] = std::chrono::duration_cast<Microseconds>(now - v->second.start_time_);
            pending_evs_real_[(size_t)type].erase(key);
        }
    }
}
