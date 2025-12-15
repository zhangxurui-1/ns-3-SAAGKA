#include "utils.h"

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
