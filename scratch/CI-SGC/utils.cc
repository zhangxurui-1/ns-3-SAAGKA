#include "utils.h"

#include "application.h"

#include <cstddef>
#include <cstdint>
#include <vector>

std::string
AddressToString(ns3::Address addr)
{
    std::stringstream ss;
    if (ns3::InetSocketAddress::IsMatchingType(addr))
    {
        ns3::InetSocketAddress inetAddr = ns3::InetSocketAddress::ConvertFrom(addr);
        ss << inetAddr.GetIpv4() << ":" << inetAddr.GetPort();
    }
    else
    {
        ss << "Unknown Address Type";
    }
    return ss.str();
}

std::chrono::time_point<std::chrono::steady_clock>
BytesToTimePoint(const uint8_t* data)
{
    int64_t us;
    std::memcpy(&us, data, sizeof(us));

    return std::chrono::time_point<std::chrono::steady_clock>(std::chrono::microseconds(us));
}

int
ParseSizeParamFromSid(const std::vector<uint8_t>& sid)
{
    if (sid.size() != SGC::SidLength)
    {
        FATAL_ERROR("Invalid sid");
    }

    int size_param = sid[6];
    size_param = (size_param << 8) | sid[7];
    return size_param;
}

uint16_t
ParseGroupSeqFromSid(const std::vector<uint8_t>& sid)
{
    if (sid.size() != SGC::SidLength)
    {
        FATAL_ERROR("Invalid sid");
    }

    uint16_t seq = sid[4];
    seq = (seq << 8) | sid[5];
    return seq;
}

std::vector<uint32_t>
SlotDiff(const std::vector<uint8_t>& bm_more, const std::vector<uint8_t>& bm_less, uint32_t n_slot)
{
    if (bm_more.size() != bm_less.size())
    {
        FATAL_ERROR("SlotDiff: bm_more and bm_less must have the same size");
    }
    std::vector<uint32_t> diff;

    uint32_t index = 0;
    for (size_t i = 0; i < bm_more.size(); i++)
    {
        if (bm_more[i] == bm_less[i])
        {
            index += 8;
            continue;
        }
        uint8_t diff_byte = bm_more[i] ^ bm_less[i];
        for (int i = 0; i < 8; i++)
        {
            if (diff_byte & (0x80 >> i))
            {
                diff.push_back(index);
            }
            index++;
        }
    }

    return diff;
}

bool
IsEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (int i = 0; i < a.size(); i++)
    {
        if (a[i] != b[i])
        {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t>
BytesXOR(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    if (a.size() != b.size())
    {
        FATAL_ERROR("BytesXOR: a and b must have the same size");
    }
    std::vector<uint8_t> res(a.size());
    for (int i = 0; i < a.size(); i++)
    {
        res[i] = a[i] ^ b[i];
    }
    return res;
}

std::vector<uint8_t>
GenerateRandomBytes(uint32_t len)
{
    std::vector<uint8_t> res(len);
    for (int i = 0; i < len; i++)
    {
        res[i] = rand() % 256;
    }
    return res;
}

std::string
ToString(const std::vector<uint32_t>& vec)
{
    std::string res = "[";
    for (int i = 0; i < vec.size(); i++)
    {
        res += std::to_string(vec[i]);
        if (i != vec.size() - 1)
        {
            res += ", ";
        }
    }
    res += "]";
    return res;
}
