#include <ns3/core-module.h>
#include "ns3/net-device-queue-interface.h"
#include "ns3/packet.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/sim-net-device.h"
#include "ns3/sim-channel.h"
#include "ns3/atp-client.h"
#include "ns3/atp-server.h"
#include "ns3/atp-driver.h"
#include "ns3/atp-driver-helper.h"
#include "ns3/error-model.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("ATPFirstTest");
NodeContainer n;
NetDeviceContainer d;
std::string data_rate = "100Gbps";
std::string link_delay = "500ns";
double error_rate = 0.005;
Ipv4Address node_id_to_ip(uint32_t id){
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

void LogThroughput(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
    *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "," << p->GetSize() << std::endl;
}

void Atp_finish() {
    NS_LOG_UNCOND("Simulation Finished at " << Simulator::Now().GetNanoSeconds() << "ns.");
}
void SendPackets(Ptr<AtpDriver> driver){
    driver->AddTask(3*1024000, node_id_to_ip(0), node_id_to_ip(1), &Atp_finish);
}

int main(int argc, char *argv[]) {
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> cli_stream = ascii.CreateFileStream("firstClientTx.csv");
    *cli_stream->GetStream() << "Time(ns),Bytes" << std::endl;

    LogComponentEnable("SimNetDevice", LOG_LEVEL_INFO);
    n.Create(2);
    InternetStackHelper internet;
	internet.Install(n);

    NS_LOG_INFO("Create Network Topology.");
    Ipv4Address server_addr = node_id_to_ip(1);
    Ipv4Address client_addr = node_id_to_ip(0);

    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(50);
	rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    
    AtpDriverHelper m_driverHelper;
    m_driverHelper.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
    m_driverHelper.SetDeviceAttribute("DataRate", StringValue(data_rate));
	m_driverHelper.SetChannelAttribute("Delay", StringValue(link_delay));
    d = m_driverHelper.Install(n);
    d.Get(0)->GetObject<SimNetDevice>()->SetDeviceType(CLI);
    d.Get(1)->GetObject<SimNetDevice>()->SetDeviceType(PS);

    Config::ConnectWithoutContext("/NodeList/0/DeviceList/1/$ns3::SimNetDevice/PhyTxBegin", MakeBoundCallback(&LogThroughput, cli_stream));
    
    Ptr<Ipv4> ipv4 = n.Get(0)->GetObject<Ipv4>();
    ipv4->AddInterface(d.Get(0));
    ipv4->AddAddress(1, Ipv4InterfaceAddress(client_addr, Ipv4Mask(0xff000000)));
    
    ipv4 = n.Get(1)->GetObject<Ipv4>();
    ipv4->AddInterface(d.Get(1));
    ipv4->AddAddress(1, Ipv4InterfaceAddress(server_addr, Ipv4Mask(0xff000000)));

    Ipv4AddressHelper ipv4Helper;
    // This is just to set up the connectivity between nodes. The IP addresses are useless
    ipv4Helper.SetBase("10.103.11.0", "255.255.255.0");
	ipv4Helper.Assign(d);

    NS_LOG_INFO("Create ATP Driver.");
    // install RDMA driver
    // Client
    Ptr<AtpClient> client = CreateObject<AtpClient>();
    client->SetAttribute("NumWorker", UintegerValue(1));
    client->SetAttribute("AppID", UintegerValue(0));
    client->SetAttribute("Host", UintegerValue(0));
    client->SetAttribute("NumPS", UintegerValue(1));
    client->SetAttribute("Key", UintegerValue(0));
    Ptr<AtpDriver> driver_cli = CreateObject<AtpDriver>();
    driver_cli->SetNode(n.Get(0));
    driver_cli->SetClient(client);
    n.Get(0)->AggregateObject(driver_cli);
    driver_cli->Init();


    // Server
    Ptr<AtpServer> server = CreateObject<AtpServer>();
    server->Reset();
    Ptr<AtpDriver> driver_ps = CreateObject<AtpDriver>();
    driver_ps->SetNode(n.Get(1));
    driver_ps->SetServer(server);
    n.Get(1)->AggregateObject(driver_ps);
    driver_ps->Init();

    NS_LOG_INFO("Create Routing Table.");
    driver_cli->AddTableEntry(server_addr, 1);
    driver_ps->AddTableEntry(client_addr, 1);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    NS_LOG_INFO("Create Applications.");
    Simulator::Schedule(Seconds(0), &SendPackets, driver_cli);
    NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(10));
	Simulator::Run();
	Simulator::Destroy();
    return 0;
}


