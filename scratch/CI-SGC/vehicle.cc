#include "vehicle.h"

#include "utils.h"

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
CreateVehicleNodes(int nVehicle)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> dist_pos(0.0, 100.0);
    static std::uniform_int_distribution<int> dist_dir(0, 4);
    static std::uniform_real_distribution<double> dist_velocity(5, 30.0);

    ns3::Ptr<ns3::ListPositionAllocator> pa = ns3::CreateObject<ns3::ListPositionAllocator>();

    ns3::NodeContainer vehicles;
    for (int i = 0; i < nVehicle; i++)
    {
        ns3::Ptr<Vehicle> v = ns3::CreateObject<Vehicle>();
        auto dir = static_cast<Direction>(dist_dir(gen));
        v->SetDirection(dir);
        if (dir == NORTH)
        {
            pa->Add(ns3::Vector(0.0, -dist_pos(gen), 0.0));
        }
        else if (dir == SOUTH)
        {
            pa->Add(ns3::Vector(0.0, dist_pos(gen), 0.0));
        }
        else if (dir == EAST)
        {
            pa->Add(ns3::Vector(-dist_pos(gen), 0.0, 0.0));
        }
        else if (dir == WEST)
        {
            pa->Add(ns3::Vector(dist_pos(gen), 0.0, 0.0));
        }

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
