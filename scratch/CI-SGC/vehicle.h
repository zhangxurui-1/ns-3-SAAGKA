// #include "ns3/mobility-module.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/node-container.h"
#include "ns3/node.h"
#include "ns3/position-allocator.h"

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

ns3::NodeContainer CreateVehicleNodes(int nVehicle,
                                      int initPosMin,
                                      int initPosMax,
                                      int maxVelocity);
ns3::NodeContainer CreateRSUNode();

inline std::ostream&
operator<<(std::ostream& os, Direction dir)
{
    switch (dir)
    {
    case NORTH:
        os << "NORTH";
        break;
    case SOUTH:
        os << "SOUTH";
        break;
    case EAST:
        os << "EAST";
        break;
    case WEST:
        os << "WEST";
        break;
    default:
        os << "UNKNOWN";
        break;
    }
    return os;
}
