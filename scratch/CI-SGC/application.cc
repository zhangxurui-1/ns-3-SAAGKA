#include "application.h"

#include "message/message.h"
#include "sgc/sgc-rsu.h"
#include "sgc/sgc-vehicle.h"
#include "utils.h"

// #include "MIRACL-wrapper.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

NS_LOG_COMPONENT_DEFINE("CI-SGC-Application");

ns3::TypeId
RsuApplication::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("RsuApplication")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<RsuApplication>();
    return tid;
}

RsuApplication::RsuApplication()
    : ns3::Application()
{
}

RsuApplication::~RsuApplication()
{
}

void
RsuApplication::StartApplication()
{
    NS_ABORT_MSG_IF(broadcast_addr_.IsInvalid(), "Broadcast address not properly set");
    // NS_ABORT_MSG_IF(local_addr_.IsInvalid(), "Local address not properly set");

    socket_ = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
    socket_->SetAllowBroadcast(true);
    socket_->Bind(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port_));
    socket_->SetRecvCallback(ns3::MakeCallback(&RsuApplication::HandleRecv, this));

    SendHeartbeat();
    LaunchSessionKeyEncap();
}

void
RsuApplication::StopApplication()
{
    socket_->Close();
}

void
RsuApplication::SendHeartbeat()
{
    auto sgc_proto_rsu = std::dynamic_pointer_cast<SGCRSU>(sgc_proto_);
    auto heartbeat = sgc_proto_rsu->HeartbeatMsg();

    std::vector<uint8_t> bytes;
    ByteWriter bw(bytes);
    heartbeat->Serialize(bw);

    ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(bytes.data(), bytes.size());

    // NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\tRSU ("
    //                 << AddressToString(local_addr_)
    //                 << ") broadcast heartbeat, Packet size=" << packet->GetSize() << " (bytes)");
    socket_->SendTo(packet, 0, broadcast_addr_);
    ns3::Simulator::Schedule(heartbeat_interval_, &RsuApplication::SendHeartbeat, this);
}

void
RsuApplication::LaunchSessionKeyEncap()
{
    auto sgc_proto_rsu = std::dynamic_pointer_cast<SGCRSU>(sgc_proto_);
    auto ntf = sgc_proto_rsu->NotifyKeyEncap();
    if (ntf)
    {
        std::vector<uint8_t> bytes;
        ByteWriter bw(bytes);
        ntf->Serialize(bw);

        ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(bytes.data(), bytes.size());

        socket_->SendTo(packet, 0, broadcast_addr_);
    }
    ns3::Simulator::Schedule(session_key_encap_interval_,
                             &RsuApplication::LaunchSessionKeyEncap,
                             this);
}

void
RsuApplication::SetLocalAddress(ns3::Address addr)
{
    local_addr_ = addr;
}

void
RsuApplication::SetBroadcastAddress(ns3::Address addr)
{
    broadcast_addr_ = addr;
}

ns3::Ptr<RsuApplication>
RsuApplication::Install(ns3::Ptr<ns3::Node> node,
                        uint32_t port,
                        ns3::Time stop_time,
                        ns3::Time hb_interval,
                        ns3::Time key_encap_interval,
                        ns3::Time key_upd_threshold,
                        uint32_t group_size,
                        uint32_t max_group_num)
{
    NS_ABORT_MSG_IF(!node, "Node does not exist");
    ns3::Ptr<ns3::Ipv4> ipv4 = node->GetObject<ns3::Ipv4>();
    NS_ABORT_MSG_IF(!ipv4, "Node has no Ipv4 object");

    ns3::Ipv4Address local_ip;
    for (int i = 1; i < ipv4->GetNInterfaces(); i++)
    {
        if (ipv4->GetNAddresses(i) > 0)
        {
            local_ip = ipv4->GetAddress(i, 0).GetLocal();
            break;
        }
    }
    NS_ABORT_MSG_IF(local_ip == ns3::Ipv4Address::GetZero(), "Node has no Ipv4 address");

    ns3::InetSocketAddress local_addr = ns3::InetSocketAddress(local_ip, port);
    ns3::InetSocketAddress broadcast_addr =
        ns3::InetSocketAddress(local_ip.GetSubnetDirectedBroadcast("255.255.255.0"), port);

    ns3::Ptr<RsuApplication> app = ns3::CreateObject<RsuApplication>();
    app->local_addr_ = local_addr;
    app->broadcast_addr_ = broadcast_addr;
    app->port_ = port;
    app->sgc_proto_ = std::make_shared<SGCRSU>(group_size, max_group_num, key_upd_threshold);
    app->heartbeat_interval_ = hb_interval;
    app->session_key_encap_interval_ = key_encap_interval;

    // NS_LOG_INFO("RsuApplication install done. (local address: "
    //             << local_addr.GetIpv4() << ":" << local_addr.GetPort() << ", broadcast address: "
    //             << broadcast_addr.GetIpv4() << ":" << broadcast_addr.GetPort() << ")");

    app->SetStartTime(ns3::Seconds(0));
    app->SetStopTime(stop_time);
    node->AddApplication(app);
    return app;
}

void
RsuApplication::HandleRecv(ns3::Ptr<ns3::Socket> socket)
{
    ns3::Address from;
    while (auto packet = socket->RecvFrom(from))
    {
        uint32_t size = packet->GetSize();
        std::vector<uint8_t> buffer(size);
        packet->CopyData(buffer.data(), size);

        // NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\tRSU ("
        //                 << AddressToString(local_addr_) << ") received from "
        //                 << AddressToString(from) << "\t Packet size=" << size << " (bytes)");

        auto start_time = std::chrono::steady_clock::now();
        auto resps = sgc_proto_->HandleMsg(buffer.data(), buffer.size());
        auto real_exec_time = std::chrono::steady_clock::now() - start_time;
        auto exec_time = ConvertRealTimeToSimTime(real_exec_time);

        ns3::Simulator::Schedule(exec_time, [resps, socket, this]() {
            for (const auto& resp : resps)
            {
                if (resp)
                {
                    std::vector<uint8_t> resp_bytes;
                    ByteWriter bw(resp_bytes);
                    resp->Serialize(bw);

                    // NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\tRSU send to
                    // "
                    //                 << AddressToString(broadcast_addr_)
                    //                 << " Packet size=" << packet->GetSize());
                    ns3::Ptr<ns3::Packet> resp_packet =
                        ns3::Create<ns3::Packet>(resp_bytes.data(), resp_bytes.size());
                    socket_->SendTo(resp_packet, 0, broadcast_addr_);
                }
            }
        });
    }
}

// VehicleApplication

ns3::TypeId
VehicleApplication::GetTypeId()
{
    static ns3::TypeId tid = ns3::TypeId("VehicleApplication")
                                 .SetParent<ns3::Application>()
                                 .SetGroupName("Applications")
                                 .AddConstructor<VehicleApplication>();
    return tid;
}

VehicleApplication::VehicleApplication()
    : ns3::Application()
{
}

VehicleApplication::~VehicleApplication()
{
}

void
VehicleApplication::StartApplication()
{
    socket_ = ns3::Socket::CreateSocket(GetNode(), ns3::UdpSocketFactory::GetTypeId());
    socket_->SetAllowBroadcast(true);
    // cannot bind to local_addr_, that will cause singlecast-only
    socket_->Bind(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port_));
    socket_->SetRecvCallback(ns3::MakeCallback(&VehicleApplication::HandleRecv, this));
    LaunchSessionKeyUpd();
}

void
VehicleApplication::StopApplication()
{
    socket_->Close();
}

void
VehicleApplication::HandleRecv(ns3::Ptr<ns3::Socket> socket)
{
    ns3::Address from;
    while (auto packet = socket->RecvFrom(from))
    {
        uint32_t size = packet->GetSize();
        std::vector<uint8_t> buffer(size);
        packet->CopyData(buffer.data(), size);
        // NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\tVehicle ("
        //                 << AddressToString(local_addr_) << ") received from "
        //                 << AddressToString(from) << " Packet size=" << packet->GetSize());

        auto start_time = std::chrono::steady_clock::now();
        auto resps = sgc_proto_->HandleMsg(buffer.data(), buffer.size());
        ns3::Time exec_time =
            ConvertRealTimeToSimTime(std::chrono::steady_clock::now() - start_time);

        ns3::Simulator::Schedule(exec_time, [resps, socket, this]() {
            for (const auto& resp : resps)
            {
                if (resp)
                {
                    std::vector<uint8_t> resp_bytes;
                    ByteWriter bw(resp_bytes);
                    resp->Serialize(bw);

                    ns3::Ptr<ns3::Packet> resp =
                        ns3::Create<ns3::Packet>(resp_bytes.data(), resp_bytes.size());
                    // socket_->SendTo(resp, 0, from);

                    socket->SendTo(resp, 0, broadcast_addr_);

                    // NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\tVehicle ("
                    //                 << AddressToString(local_addr_) << ") send to "
                    //                 << AddressToString(broadcast_addr_)
                    //                 << " Packet size=" << resp_bytes.size() << " (bytes)");
                }
            }
        });
    }
}

void
VehicleApplication::LaunchSessionKeyUpd()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> dist(0, 100);

    bool do_upd = dist(gen) < 30;
    if (!do_upd)
    {
        ns3::Simulator::Schedule(session_key_upd_interval_,
                                 &VehicleApplication::LaunchSessionKeyUpd,
                                 this);
        return;
    }

    auto sgc_proto_vehicle = std::dynamic_pointer_cast<SGCVehicle>(sgc_proto_);
    auto start_time = std::chrono::steady_clock::now();
    auto upd = sgc_proto_vehicle->LaunchKeyUpdate(32);
    ns3::Time exec_time = ConvertRealTimeToSimTime(std::chrono::steady_clock::now() - start_time);
    if (upd)
    {
        std::vector<uint8_t> upd_bytes;
        ByteWriter bw(upd_bytes);
        upd->Serialize(bw);

        ns3::Ptr<ns3::Packet> upd_packet =
            ns3::Create<ns3::Packet>(upd_bytes.data(), upd_bytes.size());

        ns3::Simulator::Schedule(exec_time, [this, upd_packet, sgc_proto_vehicle]() {
            // INFO("Vehicle-" << sgc_proto_vehicle->GetPid() << " send session key update");
            socket_->SendTo(upd_packet, 0, broadcast_addr_);
        });
    }

    ns3::Simulator::Schedule(session_key_upd_interval_,
                             &VehicleApplication::LaunchSessionKeyUpd,
                             this);
}

ns3::Ptr<VehicleApplication>
VehicleApplication::Install(ns3::Ptr<ns3::Node> node,
                            uint32_t port,
                            ns3::Time stop_time,
                            ns3::Time key_upd_interval)
{
    NS_ABORT_MSG_IF(!node, "Node does not exist");
    ns3::Ptr<ns3::Ipv4> ipv4 = node->GetObject<ns3::Ipv4>();
    NS_ABORT_MSG_IF(!ipv4, "Node has no Ipv4 object");

    ns3::Ipv4Address local_ip;
    for (int i = 1; i < ipv4->GetNInterfaces(); i++)
    {
        if (ipv4->GetNAddresses(i) > 0)
        {
            local_ip = ipv4->GetAddress(i, 0).GetLocal();
            break;
        }
    }
    NS_ABORT_MSG_IF(local_ip == ns3::Ipv4Address::GetZero(), "Node has no Ipv4 address");
    ns3::InetSocketAddress local_addr = ns3::InetSocketAddress(local_ip, port);
    ns3::InetSocketAddress broadcast_addr =
        ns3::InetSocketAddress(local_ip.GetSubnetDirectedBroadcast("255.255.255.0"), port);

    ns3::Ptr<VehicleApplication> app = ns3::CreateObject<VehicleApplication>();
    app->local_addr_ = local_addr;
    app->broadcast_addr_ = broadcast_addr;
    app->port_ = port;
    app->sgc_proto_ = std::make_shared<SGCVehicle>();
    app->session_key_upd_interval_ = key_upd_interval;

    auto sgc_proto_vehicle = std::dynamic_pointer_cast<SGCVehicle>(app->sgc_proto_);

    // Generate key when installation. That is not standard but suffice for our evaluation.
    auto metric = ns3::Singleton<Metric>::Get();
    std::string metric_key = metric->GenerateStatKey(EmitType::kComputeKeyGen);
    metric->Emit(EmitType::kComputeKeyGen, metric_key);
    sgc_proto_vehicle->Enroll();
    metric->Emit(EmitType::kComputeKeyGen, metric_key);

    // NS_LOG_INFO("VehicleApplication install done, (local address: "
    //             << AddressToString(app->local_addr_)
    //             << ", broadcast address: " << AddressToString(app->broadcast_addr_) << ")");

    app->SetStartTime(ns3::Seconds(0));
    app->SetStopTime(stop_time);
    node->AddApplication(app);
    return app;
}
