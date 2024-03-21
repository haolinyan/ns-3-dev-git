#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>
#include <string>
#include "ns3/traffic-control-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/atp-helper.h"
#include "ns3/programmable-switch.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ATPSimulation");

void LogWindowSize(Ptr<OutputStreamWrapper> stream, uint32_t newValue, bool isEcn) {
    *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "," << newValue << "," << isEcn << std::endl;
}

void LogThroughput(Ptr<OutputStreamWrapper> stream, double TX, double RX) {
    *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "," << TX << "," << RX << std::endl;
}

void LogTxRxSW(Ptr<OutputStreamWrapper> stream, 
                Ptr<const Packet> packet,
                Ptr<Ipv4> ipv4,
                uint32_t interface) 
{
    *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "," << packet->GetSize() << "," << interface << std::endl;
}

int main(int argc, char*argv[])
{
    double ErrorRate = 0.00;
    std::string QueueBufferSize = "8000p";
    uint32_t tensor_size = 1024000; // 1024000 * 4B ~ 4MB

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> ws_stream = ascii.CreateFileStream("WindowSizeTraced.csv");
    *ws_stream->GetStream() << "Time,WindowSize,Ecn" << std::endl;

    Ptr<OutputStreamWrapper> w0_stream = ascii.CreateFileStream("W0Throughput.csv");
    *w0_stream->GetStream() << "Time,Tx(Gbps),Rx(Gbps)" << std::endl;

    Ptr<OutputStreamWrapper> w1_stream = ascii.CreateFileStream("W1Throughput.csv");
    *w1_stream->GetStream() << "Time,Tx(Gbps),Rx(Gbps)" << std::endl;

    Ptr<OutputStreamWrapper> sw_rx_stream = ascii.CreateFileStream("SW_RX.csv");
    *sw_rx_stream->GetStream() << "Time,RX(B),ifn" << std::endl;

    Ptr<OutputStreamWrapper> sw_tx_stream = ascii.CreateFileStream("SW_TX.csv");
    *sw_tx_stream->GetStream() << "Time,TX(B),ifn" << std::endl;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    LogComponentEnable ("ATPSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable ("AtpServer", LOG_LEVEL_INFO);
    LogComponentEnable ("AtpClient", LOG_LEVEL_INFO);
    NodeContainer nodes;
    nodes.Create(5);

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

    NetDeviceContainer link_w0_sw = pointToPoint.Install(nodes.Get(0), nodes.Get(3)); // 2->3
    NetDeviceContainer link_ps_sw = pointToPoint.Install(nodes.Get(1), nodes.Get(3)); // 0->3 2->3
    NetDeviceContainer link_w1_sw = pointToPoint.Install(nodes.Get(2), nodes.Get(3)); // 2->3
    NetDeviceContainer link_w2_sw = pointToPoint.Install(nodes.Get(4), nodes.Get(3)); // 4->3

    InternetStackHelper stack;
    stack.Install(nodes);

    ObjectFactory queueFactory;
    queueFactory.SetTypeId("ns3::DropTailQueue<Packet>");
    queueFactory.Set("MaxSize", StringValue(QueueBufferSize));
    Ptr<Queue<Packet>> queueA = queueFactory.Create<Queue<Packet>>();
    Ptr<PointToPointNetDevice> dev = link_ps_sw.Get(1)->GetObject<PointToPointNetDevice>();
    dev->SetQueue(queueA);
    
    Ipv4AddressHelper address;
    address.SetBase("10.103.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_w0_sw = address.Assign(link_w0_sw);
    address.SetBase("10.103.2.0", "255.255.255.0");
    Ipv4InterfaceContainer if_ps_sw = address.Assign(link_ps_sw);
    address.SetBase("10.103.3.0", "255.255.255.0");
    Ipv4InterfaceContainer if_w1_sw = address.Assign(link_w1_sw);
    address.SetBase("10.103.4.0", "255.255.255.0");
    Ipv4InterfaceContainer if_w2_sw = address.Assign(link_w2_sw);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<Ipv4L3Protocol> ipv4 = nodes.Get(3)->GetObject<Ipv4L3Protocol>();
    ProgrammableSwitch* programmableSwitch = new ProgrammableSwitch();
    programmableSwitch->SetUp(MAX_AGTR_COUNT);
    programmableSwitch->AddWorkerIp(if_w0_sw.GetAddress(0));
    programmableSwitch->AddWorkerIp(if_w1_sw.GetAddress(0));
    programmableSwitch->AddWorkerIp(if_w2_sw.GetAddress(0));
    ipv4->programmableSwitch = programmableSwitch;

   
    uint16_t port = 9;
    AtpServerHelper server(port, 0);
    ApplicationContainer apps = server.Install(nodes.Get(1));
    apps.Get(0)->TraceConnectWithoutContext("AggThroughputTrace", MakeBoundCallback(&LogThroughput, w1_stream));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(5.0));

    // W0 -> W1
    Address serverAddress = Address(if_ps_sw.GetAddress(0));
    AtpClientHelper atp_w0(
        port,
        serverAddress,
        0, // host
        3, // num_worker
        0, // appID
        1, // num_PS
        0,  // key
        tensor_size
    );
    apps = atp_w0.Install(nodes.Get(0));
    apps.Get(0)->TraceConnectWithoutContext("WindowSizeTrace", MakeBoundCallback(&LogWindowSize, ws_stream));
    apps.Get(0)->TraceConnectWithoutContext("AggThroughputTrace", MakeBoundCallback(&LogThroughput, w0_stream));
    apps.Start(Seconds(0.0));


    AtpClientHelper atp_w1(
        port,
        serverAddress,
        1, // host
        3, // num_worker
        0, // appID
        1, // num_PS
        0,  // key
        tensor_size
    );
    apps = atp_w1.Install(nodes.Get(2));
    apps.Start(Seconds(0.0));

    // W3 -> W1
    AtpClientHelper atp_w2(
        port,
        serverAddress,
        2, // host
        3, // num_worker
        0, // appID
        1, // num_PS
        0,  // key
        tensor_size
    );
    apps = atp_w2.Install(nodes.Get(4));
    apps.Start(Seconds(0.0));

    Config::ConnectWithoutContext("/NodeList/3/$ns3::Ipv4L3Protocol/Tx", MakeBoundCallback(&LogTxRxSW, sw_tx_stream));
    Config::ConnectWithoutContext("/NodeList/3/$ns3::Ipv4L3Protocol/Rx", MakeBoundCallback(&LogTxRxSW, sw_rx_stream));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;


}

