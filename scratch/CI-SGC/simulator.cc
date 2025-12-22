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
#include "ns3/yans-wifi-helper.h"

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
    uint32_t nVehicle = 2;
    CommandLine cmd(__FILE__);
    cmd.AddValue("nVehicle", "Number of vehicle nodes", nVehicle);
    cmd.Parse(argc, argv);

    auto metric = ns3::Singleton<Metric>::Get();
    metric->Emit(EmitType::kAlgSetup, "Setup");
    SAAGKA::Setup(128, 30);
    metric->Emit(EmitType::kAlgSetup, "Setup");
    NS_LOG_INFO("Global Setup Done. Time cost=" << metric->GetStat(EmitType::kAlgSetup, "Setup"));

    NodeContainer rsu = CreateRSUNode();
    std::cout << "RSU node created." << std::endl;
    NodeContainer vehicles = CreateVehicleNodes(nVehicle);
    std::cout << nVehicle << " vehicle nodes created." << std::endl;

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    Ssid ssid = Ssid("ssid-CI");
    WifiHelper wifi;

    WifiMacHelper mac;

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer ObuDevices = wifi.Install(phy, mac, vehicles);
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer RsuDevices = wifi.Install(phy, mac, rsu);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    address.Assign(RsuDevices);
    address.Assign(ObuDevices);

    auto app = RsuApplication::Install(rsu.Get(0), 9999);
    app->SetBroadcastAddress(ns3::InetSocketAddress(Ipv4Address("10.1.1.255"), 9999));
    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        VehicleApplication::Install(vehicles.Get(i), 9999);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
