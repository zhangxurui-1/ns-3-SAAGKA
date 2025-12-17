#include "crypto/agka.h"
#include "metric.h"
#include "utils.h"

#include "ns3/application.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/singleton.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"

#include <cstdint>
#include <memory>

// NS_LOG_COMPONENT_DEFINE("CI-SGC App");

class RsuApplication : public ns3::Application
{
  public:
    static ns3::TypeId GetTypeId();
    RsuApplication();
    ~RsuApplication() override;
    static ns3::Ptr<RsuApplication> Install(ns3::Ptr<ns3::Node> node, uint32_t port);
    void SetLocalAddress(ns3::Address addr);
    void SetBroadcastAddress(ns3::Address addr);
    void SetPort(uint32_t port);

  private:
    void StartApplication() override;
    void StopApplication() override;
    void SendHeartbeat();
    void HandleRecv(ns3::Ptr<ns3::Socket> socket);

  protected:
    ns3::Ptr<ns3::Socket> socket_;
    ns3::Address broadcast_addr_; // ip+port
    ns3::Address local_addr_;     // ip+port
    uint32_t port_;
    ns3::Time heartbeat_interval_{ns3::Seconds(1)};
};

class VehicleApplication : public ns3::Application
{
  public:
    static ns3::TypeId GetTypeId();
    VehicleApplication();
    ~VehicleApplication() override;
    static ns3::Ptr<VehicleApplication> Install(ns3::Ptr<ns3::Node> node, uint32_t port);

  private:
    void StartApplication() override;
    void StopApplication() override;

    void HandleRecv(ns3::Ptr<ns3::Socket> socket);

    ns3::Address local_addr_;     // ip+port
    ns3::Address broadcast_addr_; // ip+port
    uint32_t port_;
    ns3::Ptr<ns3::Socket> socket_;
    std::shared_ptr<SAAGKA> protocol_;
};
