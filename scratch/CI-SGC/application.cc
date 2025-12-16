#include "application.h"

#include "MIRACL-wrapper.h"

#include <cstdint>
#include <iostream>

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
    socket_->Bind(local_addr_);
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
    const char* msg = "Hello from RSU";
    ns3::Ptr<ns3::Packet> packet =
        ns3::Create<ns3::Packet>(reinterpret_cast<const uint8_t*>(msg), std::strlen(msg));

    NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t RSU send to "
                    << AddressToString(broadcast_addr_) << " Content=" << msg);

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
        std::string msg(buffer.begin(), buffer.end());
        NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t RSU ("
                        << AddressToString(local_addr_) << ") received from "
                        << AddressToString(from) << " Content=" << msg);
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
    TestMiracl();

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
        std::string msg(buffer.begin(), buffer.end());
        NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t Vehicle ("
                        << AddressToString(local_addr_) << ") received from "
                        << AddressToString(from) << " Content=" << msg);

        const char* resp_str = "ACK";
        ns3::Ptr<ns3::Packet> resp =
            ns3::Create<ns3::Packet>(reinterpret_cast<const uint8_t*>(resp_str),
                                     std::strlen(resp_str));
        socket_->SendTo(resp, 0, from);
        NS_LOG_INFO("[" << ns3::Simulator::Now().As(ns3::Time::MS) << "]\t Vehicle ("
                        << AddressToString(local_addr_) << ") send to " << AddressToString(from)
                        << " Content=" << resp_str);
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

    NS_LOG_INFO("VehicleApplication install done. (local address: "
                << local_addr.GetIpv4() << ":" << local_addr.GetPort() << ", broadcast address: "
                << broadcast_addr.GetIpv4() << ":" << broadcast_addr.GetPort() << ")");

    app->SetStartTime(ns3::Seconds(0));
    app->SetStopTime(ns3::Seconds(10));
    node->AddApplication(app);
    return app;
}

// MIRACL test
void
TestMiracl()
{
    miracl* mip = mirsys(100, 16);
    mip->IOBASE = 16;

    big a = mirvar(0);
    big b = mirvar(0);
    big c = mirvar(0);

    cinstr(a, (char*)"123456789ABCDEF");
    cinstr(b, (char*)"FEDCBA987654321");

    multiply(a, b, c);

    char buf[2048];
    cotstr(c, buf);

    std::cout << "[MIRACL TEST] a * b = " << buf << std::endl;

    mirkill(a);
    mirkill(b);
    mirkill(c);
    mirexit();
}
