#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/programmable-switch.h"
#include <iostream>
#include <string>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATPSimulation");

int main(int argc, char*argv[])
{
    double ErrorRate = 0.00;
    // uint32_t totalSize = 5*1024*1024; // 1MB
    uint32_t totalSize = 1024; // 1MB
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    LogComponentEnable ("AtpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("AtpApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable ("ATPSimulation", LOG_LEVEL_INFO);
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("500ns"));

    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    rem->SetRandomVariable(uv);
    uv->SetStream(50);
    rem->SetAttribute("ErrorRate", DoubleValue(ErrorRate));
    rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    pointToPoint.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));

    NetDeviceContainer link_w0_sw = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
    NetDeviceContainer link_ps_sw = pointToPoint.Install(nodes.Get(1), nodes.Get(3));
    NetDeviceContainer link_w1_sw = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.103.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_w0_sw = address.Assign(link_w0_sw);
    address.SetBase("10.103.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_ps_sw = address.Assign(link_ps_sw);
    address.SetBase("10.103.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_w1_sw = address.Assign(link_w1_sw);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<Ipv4L3Protocol> ipv4 = nodes.Get(3)->GetObject<Ipv4L3Protocol>();

    ProgrammableSwitch* programmableSwitch = new ProgrammableSwitch();
    programmableSwitch->AddWorkerIp(if_w0_sw.GetAddress(0));
    programmableSwitch->AddWorkerIp(if_w1_sw.GetAddress(0));
    ipv4->programmableSwitch = programmableSwitch;
   

    uint16_t port = 9;
    AtpServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(1));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(60.0));

    // W0 -> W1
    Address serverAddress = Address(if_ps_sw.GetAddress(0));
    AtpApplicationHelper atp_w0(serverAddress, port, totalSize, 0, 0);
    apps = atp_w0.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    
    // W2 -> W1
    AtpApplicationHelper atp_w2(serverAddress, port, totalSize, 0, 1);
    apps = atp_w2.Install(nodes.Get(2));
    apps.Start(Seconds(1.0));
    
    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;


}

