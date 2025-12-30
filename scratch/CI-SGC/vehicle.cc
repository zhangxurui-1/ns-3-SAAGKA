#include "vehicle.h"

#include "utils.h"

#include <cstdint>
#include <random>

Vehicle::Vehicle()
    : ns3::Node()
{
}

Vehicle::~Vehicle()
{
}

void
Vehicle::SetDirection(Direction dir)
{
    direction_ = dir;
}

Direction
Vehicle::GetDirection() const
{
    return direction_;
}

ns3::NodeContainer
CreateVehicleNodes(int nVehicle, int initPosMin, int initPosMax, int maxVelocity)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> dist_pos(static_cast<double>(initPosMin),
                                                           static_cast<double>(initPosMax));
    static std::uniform_int_distribution<int> dist_dir(0, 3);
    static std::uniform_real_distribution<double> dist_velocity(1,
                                                                static_cast<double>(maxVelocity));

    ns3::Ptr<ns3::ListPositionAllocator> pa = ns3::CreateObject<ns3::ListPositionAllocator>();

    ns3::NodeContainer vehicles;
    for (int i = 0; i < nVehicle; i++)
    {
        ns3::Ptr<Vehicle> v = ns3::CreateObject<Vehicle>();
        auto dir = static_cast<Direction>(dist_dir(gen));
        v->SetDirection(dir);
        ns3::Vector pos(0.0, 0.0, 0.0);
        if (dir == NORTH)
        {
            pos.y = -dist_pos(gen);
            pa->Add(pos);
        }
        else if (dir == SOUTH)
        {
            pos.y = dist_pos(gen);
            pa->Add(pos);
        }
        else if (dir == EAST)
        {
            pos.x = -dist_pos(gen);
            pa->Add(pos);
        }
        else if (dir == WEST)
        {
            pos.x = dist_pos(gen);
            pa->Add(pos);
        }

        INFO("Vehicle-" << i << " direction: " << dir << " position: " << pos);

        vehicles.Add(v);
    }

    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator(pa);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (int i = 0; i < nVehicle; i++)
    {
        ns3::Ptr<Vehicle> v = vehicles.Get(i)->GetObject<Vehicle>();
        if (!v)
        {
            NS_ABORT_MSG("Failed to cast Node to Vehicle");
        }

        Direction dir = v->GetDirection();
        if (dir == NORTH)
        {
            v->GetObject<ns3::ConstantVelocityMobilityModel>()->SetVelocity(
                ns3::Vector(0.0, dist_velocity(gen), 0.0));
        }
        else if (dir == SOUTH)
        {
            v->GetObject<ns3::ConstantVelocityMobilityModel>()->SetVelocity(
                ns3::Vector(0.0, -dist_velocity(gen), 0.0));
        }
        else if (dir == EAST)
        {
            v->GetObject<ns3::ConstantVelocityMobilityModel>()->SetVelocity(
                ns3::Vector(dist_velocity(gen), 0.0, 0.0));
        }
        else if (dir == WEST)
        {
            v->GetObject<ns3::ConstantVelocityMobilityModel>()->SetVelocity(
                ns3::Vector(-dist_velocity(gen), 0.0, 0.0));
        }
        INFO("Vehicle-" << i << " position: "
                        << v->GetObject<ns3::ConstantVelocityMobilityModel>()->GetPosition());
        INFO("Vehicle-" << i << " velocity: "
                        << v->GetObject<ns3::ConstantVelocityMobilityModel>()->GetVelocity());
    }
    ns3::InternetStackHelper stack;
    stack.Install(vehicles);

    return vehicles;
}

ns3::NodeContainer
CreateRSUNode()
{
    ns3::NodeContainer rsu;
    rsu.Create(1);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(rsu);

    ns3::InternetStackHelper stack;
    stack.Install(rsu);

    return rsu;
}
