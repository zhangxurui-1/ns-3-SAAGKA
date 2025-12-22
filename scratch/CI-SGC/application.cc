#include "application.h"

#include "message/message.h"
#include "utils.h"

// #include "MIRACL-wrapper.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
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

    SendHeartbeat();
}

void
RsuApplication::StopApplication()
{
    socket_->Close();
}

void
RsuApplication::SendHeartbeat()
{
    auto heartbeat = sgc_proto_->HeartbeatMsg();
    NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS)
                    << "]\t RSU generated raw heartbeat message");

    std::vector<uint8_t> bytes;
    ByteWriter bw(bytes);
    heartbeat->Serialize(bw);

    ns3::Ptr<ns3::Packet> packet = ns3::Create<ns3::Packet>(bytes.data(), bytes.size());

    NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t RSU send to "
                    << AddressToString(broadcast_addr_) << " Packet size=" << packet->GetSize());

    socket_->SendTo(packet, 0, broadcast_addr_);
    socket_->SetRecvCallback(ns3::MakeCallback(&RsuApplication::HandleRecv, this));

    ns3::Simulator::Schedule(heartbeat_interval_, &RsuApplication::SendHeartbeat, this);
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
RsuApplication::Install(ns3::Ptr<ns3::Node> node, uint32_t port)
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
    app->sgc_proto_ = std::make_shared<SGC>(SGC::Role::kRoleRSU);

    NS_LOG_INFO("RsuApplication install done. (local address: "
                << local_addr.GetIpv4() << ":" << local_addr.GetPort() << ", broadcast address: "
                << broadcast_addr.GetIpv4() << ":" << broadcast_addr.GetPort() << ")");

    app->SetStartTime(ns3::Seconds(0));
    app->SetStopTime(ns3::Seconds(10));
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

        NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t RSU ("
                        << AddressToString(local_addr_) << ") received from "
                        << AddressToString(from) << "\t Packet size=" << size << " (bytes)");

        auto resps = sgc_proto_->HandleMsg(buffer.data(), buffer.size());
        // SAAGKA::KAMaterial kam;
        // SAAGKA::Deserialize(buffer.data(), buffer.size(), kam);
        // std::string msg(buffer.begin(), buffer.end());

        for (const auto& resp : resps)
        {
            if (resp)
            {
                std::vector<uint8_t> resp_bytes;
                ByteWriter bw(resp_bytes);
                resp->Serialize(bw);

                NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t RSU send to "
                                << AddressToString(broadcast_addr_)
                                << " Packet size=" << packet->GetSize());
                ns3::Ptr<ns3::Packet> resp_packet =
                    ns3::Create<ns3::Packet>(resp_bytes.data(), resp_bytes.size());
                socket_->SendTo(resp_packet, 0, broadcast_addr_);
            }
        }
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
        NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t Vehicle ("
                        << AddressToString(local_addr_) << ") received from "
                        << AddressToString(from) << " Packet size=" << packet->GetSize());

        auto resps = sgc_proto_->HandleMsg(buffer.data(), buffer.size());

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

                NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t Vehicle-"
                                << sgc_proto_->GetPid() << " send to "
                                << AddressToString(broadcast_addr_)
                                << "\t Packet size=" << resp_bytes.size() << " (bytes)");
            }
        }
    }
}

ns3::Ptr<VehicleApplication>
VehicleApplication::Install(ns3::Ptr<ns3::Node> node, uint32_t port)
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
    app->sgc_proto_ = std::make_shared<SGC>(SGC::Role::kRoleVehicle);

    // Generate key when installation. That is not standard but suffice for our evaluation.
    auto metric = ns3::Singleton<Metric>::Get();
    std::string metric_key = "KeyGen-" + AddressToString(app->local_addr_);
    metric->Emit(EmitType::kAlgKeyGen, metric_key);
    app->sgc_proto_->Enroll();
    metric->Emit(EmitType::kAlgKeyGen, metric_key);

    NS_LOG_INFO("VehicleApplication install done, KeyGen time-cost:"
                << metric->GetStat(EmitType::kAlgKeyGen, metric_key)
                << "(local address: " << AddressToString(app->local_addr_)
                << ", broadcast address: " << AddressToString(app->broadcast_addr_) << ")");

    app->SetStartTime(ns3::Seconds(0));
    app->SetStopTime(ns3::Seconds(10));
    node->AddApplication(app);
    return app;
}
