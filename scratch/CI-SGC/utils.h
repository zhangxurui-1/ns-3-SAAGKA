#pragma once

#include "crypto/utils.h"

#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#define FATAL_ERROR(msg)                                                                           \
    std::cerr << ns3::Simulator::Now().As(ns3::Time::MS) << "\t[ERROR] " << __FILE__ << ": line "  \
              << __LINE__ << ": " << msg << std::endl;                                             \
    abort();

#define INFO(msg)                                                                                  \
    std::cout << ns3::Simulator::Now().As(ns3::Time::MS) << "\t[INFO] " << msg << std::endl;
#define WARN(msg)                                                                                  \
    std::cout << ns3::Simulator::Now().As(ns3::Time::MS) << "\t[WARN] " << msg << std::endl;

// only support InetSocketAddress for now
std::string AddressToString(ns3::Address addr);

int ParseSizeParamFromSid(const std::vector<uint8_t>& sid);
uint16_t ParseGroupSeqFromSid(const std::vector<uint8_t>& sid);
std::vector<uint32_t> SlotDiff(const std::vector<uint8_t>& bm_more,
                               const std::vector<uint8_t>& bm_less,
                               uint32_t n_slot);

bool IsEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
std::vector<uint8_t> BytesXOR(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
std::vector<uint8_t> GenerateRandomBytes(uint32_t len);
std::string ToString(const std::vector<uint32_t>& vec);

template <typename Rep, typename Period>
inline ns3::Time
ConvertRealTimeToSimTime(std::chrono::duration<Rep, Period> rt)
{
    return ns3::NanoSeconds(std::chrono::duration_cast<std::chrono::nanoseconds>(rt).count());
}
