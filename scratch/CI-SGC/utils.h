#include "ns3/address.h"
#include "ns3/inet-socket-address.h"

#include <string>

// only support InetSocketAddress for now
std::string AddressToString(ns3::Address addr);
