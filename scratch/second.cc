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
#include <unordered_map>
#include <vector>
#include "ns3/programmable-switch-node.h"
#include "ns3/Common.h"


using namespace ns3;
NS_LOG_COMPONENT_DEFINE("ATPSecondTest");
NodeContainer n;
uint32_t packet_payload_size = P4ML_PACKET_SIZE;
double error_rate_per_link = 0.00;
std::ifstream topof;
uint64_t nic_rate;
uint64_t maxRtt, maxBdp;
struct Interface{
	uint32_t idx;
	bool up;
	uint64_t delay;
	uint64_t bw;

	Interface() : idx(0), up(false){}
};
std::map<Ptr<Node>, std::map<Ptr<Node>, Interface> > nbr2if;
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<Ptr<Node> > > > nextHop;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairDelay;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairTxDelay;
std::map<uint32_t, std::map<uint32_t, uint64_t> > pairBw;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairBdp;
std::map<uint32_t, std::map<uint32_t, uint64_t> > pairRtt;

Ipv4Address node_id_to_ip(uint32_t id){
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t ip_to_node_id(Ipv4Address ip){
	return (ip.Get() >> 8) & 0xffff;
}

uint64_t get_nic_rate(NodeContainer &n){
	for (uint32_t i = 0; i < n.GetN(); i++)
		if (n.Get(i)->GetNodeType() == 0)
			return DynamicCast<SimNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
}

void Atp_finish() {
    NS_LOG_UNCOND("Simulation Finished at " << Simulator::Now().GetNanoSeconds() << "ns.");
}

void SendPackets(Ptr<AtpDriver> driver, int src, int dst){
    driver->AddTask(1024000, node_id_to_ip(src), node_id_to_ip(dst), &Atp_finish);
}

void CalculateRoute(Ptr<Node> host){
	// queue for the BFS.
	std::vector<Ptr<Node> > q;
	// Distance from the host to each node.
	std::map<Ptr<Node>, int> dis;
	std::map<Ptr<Node>, uint64_t> delay;
	std::map<Ptr<Node>, uint64_t> txDelay;
	std::map<Ptr<Node>, uint64_t> bw;
	// init BFS.
	q.push_back(host);
	dis[host] = 0;
	delay[host] = 0;
	txDelay[host] = 0;
	bw[host] = 0xfffffffffffffffflu;
	// BFS.
	for (int i = 0; i < (int)q.size(); i++){
		Ptr<Node> now = q[i];
		int d = dis[now];
		for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++){
			// skip down link
			if (!it->second.up)
				continue;
			Ptr<Node> next = it->first;
			// If 'next' have not been visited.
			if (dis.find(next) == dis.end()){
				dis[next] = d + 1;
				delay[next] = delay[now] + it->second.delay;
				txDelay[next] = txDelay[now] + packet_payload_size * 1000000000lu * 8 / it->second.bw;
				bw[next] = std::min(bw[now], it->second.bw);
				// we only enqueue switch, because we do not want packets to go through host as middle point
				if (next->GetNodeType() == 1)
					q.push_back(next);
			}
			// if 'now' is on the shortest path from 'next' to 'host'.
			if (d + 1 == dis[next]){
				nextHop[next][host].push_back(now);
			}
		}
	}
	for (auto it : delay)
		pairDelay[it.first][host] = it.second;
	for (auto it : txDelay)
		pairTxDelay[it.first][host] = it.second;
	for (auto it : bw)
		pairBw[it.first->GetId()][host->GetId()] = it.second;
}

void CalculateRoutes(NodeContainer &n){
	for (int i = 0; i < (int)n.GetN(); i++){
		Ptr<Node> node = n.Get(i);
		if (node->GetNodeType() == 0)
			CalculateRoute(node);
	}
}

void SetRoutingEntries(){
	// For each node.
	for (auto i = nextHop.begin(); i != nextHop.end(); i++){
		Ptr<Node> node = i->first;
		auto &table = i->second;
		for (auto j = table.begin(); j != table.end(); j++){
			// The destination node.
			Ptr<Node> dst = j->first;
			// The IP address of the dst.
			Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
			// The next hops towards the dst.
			std::vector<Ptr<Node> > nexts = j->second;
			for (int k = 0; k < (int)nexts.size(); k++){
				Ptr<Node> next = nexts[k];
				uint32_t interface = nbr2if[node][next].idx;
				if (node->GetNodeType() == 1) {
					NS_LOG_INFO("Node[SW] " << node->GetId() << " to " << dst->GetId() << " via " << next->GetId() << " on interface " << interface);
					DynamicCast<ProgrammableSwitchNode>(node)->AddTableEntry(dstAddr, interface);
				}
				else{
					NS_LOG_INFO("Node " << node->GetId() << " to " << dst->GetId() << " via " << next->GetId() << " on interface " << interface);
					node->GetObject<AtpDriver>()->AddTableEntry(dstAddr, interface);
				}
			}
		}
	}
}
std::vector<Ipv4Address> serverAddress;

void LogThroughput(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p) {
    *stream->GetStream() << Simulator::Now().GetNanoSeconds() << "," << p->GetSize() << std::endl;
}

int main(int argc, char *argv[]){
	AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> cli_stream = ascii.CreateFileStream("SecondClientTx.csv");
    *cli_stream->GetStream() << "Time(ns),Bytes" << std::endl;
    LogComponentEnable("SimNetDevice", LOG_LEVEL_INFO);
	LogComponentEnable("ATPSecondTest", LOG_LEVEL_INFO);
    topof.open("scratch/physical_topology.txt");
	if (!topof.is_open()){
		NS_LOG_ERROR("Cannot open physical_topology.txt");
		return 1;
	}

	uint32_t node_num, switch_num, link_num;
	topof >> node_num >> switch_num >> link_num;
    std::vector<uint32_t> node_type(node_num, 0);
	NS_LOG_INFO("Node num: " << node_num << ", switch num: " << switch_num << ", link num: " << link_num);
    for (uint32_t i = 0; i < switch_num; i++)
	{
		uint32_t sid;
		topof >> sid;
		node_type[sid] = 1;
	}
    for (uint32_t i = 0; i < node_num; i++){
		if (node_type[i] == 0)
			n.Add(CreateObject<Node>());
		else{
			Ptr<ProgrammableSwitchNode> sw = CreateObject<ProgrammableSwitchNode>();
			sw->SetUp(MAX_AGTR_COUNT);
			n.Add(sw);
		}
	}
	NS_LOG_INFO("Create nodes.");
	InternetStackHelper internet;
	internet.Install(n);
    for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0){ // is server
			serverAddress.resize(i + 1);
			serverAddress[i] = node_id_to_ip(i);
            NS_LOG_INFO("Node " << i << " IP: " << serverAddress[i]);
		}
	}

	NS_LOG_INFO("Create channels.");
    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(50);
	rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

	Ipv4AddressHelper ipv4;
	AtpDriverHelper qbb;
	for (uint32_t i = 0; i < link_num; i++)
	{
		uint32_t src, dst;
		std::string data_rate, link_delay;
		double error_rate;
		topof >> src >> dst >> data_rate >> link_delay >> error_rate;

		Ptr<Node> snode = n.Get(src), dnode = n.Get(dst);

		qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
		qbb.SetChannelAttribute("Delay", StringValue(link_delay));
		qbb.SetQueueAttribute("MaxSize", StringValue("78465p"));
		if (error_rate > 0)
		{
			Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
			Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
			rem->SetRandomVariable(uv);
			uv->SetStream(50);
			rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}
		else
		{
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		// Assigne server IP
		// Note: this should be before the automatic assignment below (ipv4.Assign(d)),
		// because we want our IP to be the primary IP (first in the IP address list),
		// so that the global routing is based on our IP
		NetDeviceContainer d = qbb.Install(snode, dnode);
		if (snode->GetNodeType() == 0){
			Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
			ipv4->AddInterface(d.Get(0));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
		}
		if (dnode->GetNodeType() == 0){
			Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
			ipv4->AddInterface(d.Get(1));
			ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
		}

		if (src < node_num - 2) {
			d.Get(0)->GetObject<SimNetDevice>()->SetDeviceType(CLI);
		} else if (src == node_num - 2) {
			d.Get(0)->GetObject<SimNetDevice>()->SetDeviceType(PS);
		}

		d.Get(1)->GetObject<SimNetDevice>()->SetDeviceType(SW);

		// used to create a graph of the topology
		nbr2if[snode][dnode].idx = DynamicCast<SimNetDevice>(d.Get(0))->GetIfIndex();
		nbr2if[snode][dnode].up = true;
		nbr2if[snode][dnode].delay = DynamicCast<SimChannel>(DynamicCast<SimNetDevice>(d.Get(0))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[snode][dnode].bw = DynamicCast<SimNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
		nbr2if[dnode][snode].idx = DynamicCast<SimNetDevice>(d.Get(1))->GetIfIndex();
		nbr2if[dnode][snode].up = true;
		nbr2if[dnode][snode].delay = DynamicCast<SimChannel>(DynamicCast<SimNetDevice>(d.Get(1))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[dnode][snode].bw = DynamicCast<SimNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

		// This is just to set up the connectivity between nodes. The IP addresses are useless
		char ipstring[16];
		snprintf(ipstring, sizeof(ipstring), "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring, "255.255.255.0");
		ipv4.Assign(d);
        NS_LOG_INFO("Link " << src << " -> " << dst << " created.");
	}

	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0 && i != node_num - 2){
			Ptr<AtpClient> client = CreateObject<AtpClient>();
    		client->SetAttribute("NumWorker", UintegerValue(node_num - 2));
    		client->SetAttribute("AppID", UintegerValue(0));
    		client->SetAttribute("Host", UintegerValue(i));
    		client->SetAttribute("NumPS", UintegerValue(1));
    		client->SetAttribute("Key", UintegerValue(0));
			Ptr<AtpDriver> driver_cli = CreateObject<AtpDriver>();
			driver_cli->SetNode(n.Get(i));
			driver_cli->SetClient(client);
			n.Get(i)->AggregateObject(driver_cli);
			driver_cli->Init();
		} else if (n.Get(i)->GetNodeType() == 0 && i == node_num - 2) {
			Ptr<AtpServer> server = CreateObject<AtpServer>();
			server->Reset();
			Ptr<AtpDriver> driver_ps = CreateObject<AtpDriver>();
			driver_ps->SetNode(n.Get(i));
			driver_ps->SetServer(server);
			n.Get(i)->AggregateObject(driver_ps);
			driver_ps->Init();
		}
	}

	// Set up routing
	CalculateRoutes(n);
	SetRoutingEntries();
	// Set Muticast Address
	Ptr<ProgrammableSwitchNode> sw = n.Get(node_num - 1)->GetObject<ProgrammableSwitchNode>();
	for (uint32_t i = 0; i < node_num - 2; i++){
		sw->AddMulticastAddr(serverAddress[i]);
	}
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	NS_LOG_INFO("Create Applications.");
	for (size_t i = 0; i < node_num - 2; i++){
		Simulator::Schedule(Seconds(0), &SendPackets, n.Get(i)->GetObject<AtpDriver>(), i, node_num - 2);
	}
	Config::ConnectWithoutContext("/NodeList/0/DeviceList/1/$ns3::SimNetDevice/PhyTxBegin", MakeBoundCallback(&LogThroughput, cli_stream));
	NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(10));
	Simulator::Run();
	Simulator::Destroy();
	return 0;

}



