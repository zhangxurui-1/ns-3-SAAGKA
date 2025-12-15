// #include "ns3/mobility-module.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/position-allocator.h"
#include "ns3/internet-stack-helper.h"

enum Direction
{
    NORTH,
    SOUTH,
    EAST,
    WEST,
    UNKNOWN
};

class Vehicle : public ns3::Node
{
    Direction direction_;

  public:
    Vehicle();
    ~Vehicle();

    void SetDirection(Direction dir);

    Direction GetDirection() const;
};

ns3::NodeContainer CreateVehicleNodes(int nVehicle);
ns3::NodeContainer CreateRSUNode();