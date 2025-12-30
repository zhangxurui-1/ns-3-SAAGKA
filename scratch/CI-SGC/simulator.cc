/*
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "application.h"
#include "vehicle.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ssid.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"

#include <cstdint>
#include <iostream>
#include <sys/types.h>

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CI-SGC-Simulator");

int
main(int argc, char* argv[])
{
    LogComponentEnable("CI-SGC-Simulator", LOG_LEVEL_INFO);
    LogComponentEnable("CI-SGC-Application", LOG_LEVEL_INFO);

    NS_LOG_INFO("Start CI-SGC Simulation");
    uint32_t nVehicle = 10;
    uint32_t maxVelocity = 20;
    uint32_t initPosMin = 50;
    uint32_t initPosMax = 100;
    uint32_t securityLevel = 80;
    uint32_t maxGroupSize = 10;
    uint32_t groupSizeStep = 10;
    uint32_t maxGroupNum = 1;
    uint32_t groupSize = 10;
    uint32_t stopTime = 10;
    uint32_t hbInterval = 1000;
    uint32_t keyEncapInterval = 3000;
    uint32_t KeyUpdInterval = 2000;
    uint32_t KeyUpdThreshold = 2000;
    CommandLine cmd(__FILE__);
    cmd.AddValue("nVehicle", "Number of vehicle nodes", nVehicle);
    cmd.AddValue("maxVelocity", "Maximum velocity of vehicle nodes", maxVelocity);
    cmd.AddValue("initPosMin", "Minimum initial position of vehicle nodes", initPosMin);
    cmd.AddValue("initPosMax", "Maximum initial position of vehicle nodes", initPosMax);
    cmd.AddValue("securityLevel", "Security level", securityLevel);
    cmd.AddValue("maxGroupSize", "Maximum group size", maxGroupSize);
    cmd.AddValue("groupSizeStep", "Step size of group size", groupSizeStep);
    cmd.AddValue("maxGroupNum", "Maximum group number", maxGroupNum);
    cmd.AddValue("groupSize", "Group size", groupSize);
    cmd.AddValue("stopTime", "End time of simulation (s)", stopTime);
    cmd.AddValue("hbInterval", "Heartbeat interval (ms)", hbInterval);
    cmd.AddValue("keyEncapInterval", "Key encapsulation interval (ms)", keyEncapInterval);
    cmd.AddValue("KeyUpdInterval",
                 "Vehicles lauch key update in this frequency (ms)",
                 KeyUpdInterval);
    cmd.AddValue("KeyUpdThreshold",
                 "RSU decides to accept or reject a key update according to this frequency (ms)",
                 KeyUpdThreshold);

    if (initPosMin >= initPosMax)
    {
        NS_FATAL_ERROR("initPosMin must be less than initPosMax");
    }

    cmd.Parse(argc, argv);

    auto metric = ns3::Singleton<Metric>::Get();
    auto mk = metric->GenerateStatKey(EmitType::kComputeSetup);
    metric->Emit(EmitType::kComputeSetup, mk);
    SAAGKA::Setup(securityLevel, maxGroupSize, groupSizeStep);
    metric->Emit(EmitType::kComputeSetup, mk);

    NodeContainer rsu = CreateRSUNode();
    std::cout << "RSU node created." << std::endl;
    NodeContainer vehicles = CreateVehicleNodes(nVehicle, initPosMin, initPosMax, maxVelocity);
    std::cout << nVehicle << " vehicle nodes created." << std::endl;

    // set wifi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    // set wifi phy for vehicle
    ns3::YansWifiPhyHelper phyVehicle;
    phyVehicle.SetChannel(wifiChannel);
    phyVehicle.Set("TxPowerStart", ns3::DoubleValue(15));
    phyVehicle.Set("TxPowerEnd", ns3::DoubleValue(15));

    // set wifi phy for RSU
    ns3::YansWifiPhyHelper phyRSU;
    phyRSU.SetChannel(wifiChannel);
    phyRSU.Set("TxPowerStart", ns3::DoubleValue(20));
    phyRSU.Set("TxPowerEnd", ns3::DoubleValue(20));

    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 ns3::StringValue("OfdmRate6MbpsBW10MHz"),
                                 "ControlMode",
                                 ns3::StringValue("OfdmRate6MbpsBW10MHz"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac", "QosSupported", BooleanValue(false));

    NetDeviceContainer ObuDevices = wifi.Install(phyVehicle, mac, vehicles);
    NetDeviceContainer RsuDevices = wifi.Install(phyRSU, mac, rsu);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(RsuDevices);
    address.Assign(ObuDevices);

    auto app = RsuApplication::Install(rsu.Get(0),
                                       9999,
                                       Seconds(stopTime),
                                       MilliSeconds(hbInterval),
                                       MilliSeconds(keyEncapInterval),
                                       MilliSeconds(KeyUpdThreshold),
                                       groupSize,
                                       maxGroupNum);
    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        VehicleApplication::Install(vehicles.Get(i),
                                    9999,
                                    Seconds(stopTime),
                                    MilliSeconds(KeyUpdInterval));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(stopTime));

    Simulator::Run();
    Simulator::Destroy();

    metric->Summarize();
    return 0;
}
