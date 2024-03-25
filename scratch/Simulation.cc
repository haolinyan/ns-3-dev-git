#include <ns3/core-module.h>
#include "ns3/ipv4-static-routing-helper.h"
#include <vector>
#include <ns3/switch-node.h>
#include "ns3/internet-module.h"
#include "ns3/error-model.h"
#include "ns3/qbb-helper.h"
#include <unordered_map>
#include "ns3/rdma-driver.h"
#include "ns3/rdma-client-helper.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Simulation");
NodeContainer n;
double error_rate_per_link = 0.0;
uint32_t packet_payload_size = 1000, l2_chunk_size = 4000, l2_ack_interval = 1;
std::ifstream topof;
uint64_t nic_rate;
uint64_t maxRtt, maxBdp;
uint8_t buffer_size = 32;
bool ack_high_prio = false; // {0: ACK has same priority with data packet, 1: prioritize ACK}
// ECN settings
std::unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;
std::unordered_map<uint64_t, double> rate2pmax;

struct Interface{
	uint32_t idx;
	bool up;
	uint64_t delay;
	uint64_t bw;

	Interface() : idx(0), up(false){}
};
std::map<Ptr<Node>, std::map<Ptr<Node>, Interface> > nbr2if;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
std::map<Ptr<Node>, std::map<Ptr<Node>, std::vector<Ptr<Node> > > > nextHop;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairDelay;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairTxDelay;
std::map<uint32_t, std::map<uint32_t, uint64_t> > pairBw;
std::map<Ptr<Node>, std::map<Ptr<Node>, uint64_t> > pairBdp;
std::map<uint32_t, std::map<uint32_t, uint64_t> > pairRtt;

// maintain port number for each host pair
std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint16_t> > portNumder;

Ipv4Address node_id_to_ip(uint32_t id){
	return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t ip_to_node_id(Ipv4Address ip){
	return (ip.Get() >> 8) & 0xffff;
}

uint64_t get_nic_rate(NodeContainer &n){
	for (uint32_t i = 0; i < n.GetN(); i++)
		if (n.Get(i)->GetNodeType() == 0)
			return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
}

void qp_finish(Ptr<RdmaQueuePair> q){
	uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);
	uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did];
	// uint32_t total_bytes = q->m_size + ((q->m_size-1) / packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize() - IntHeader::GetStaticSize()); // translate to the minimum bytes required (with header but no INT)
	uint32_t total_bytes = q->m_size + ((q->m_size-1) / packet_payload_size + 1) * (CustomHeader::GetStaticWholeHeaderSize());
	uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;
	// sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
	NS_LOG_INFO("QpComplete: " << q->sip << " " << q->dip << " " << q->sport << " " << q->dport << " " << q->m_size << " " << q->startTime << " " << Simulator::Now() - q->startTime << " " << standalone_fct);

	// remove rxQp from the receiver
	Ptr<Node> dstNode = n.Get(did);
	Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver> ();
	rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);
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
					DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
				}
				else{
					NS_LOG_INFO("Node " << node->GetId() << " to " << dst->GetId() << " via " << next->GetId() << " on interface " << interface);
					node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface);
				}
			}
		}
	}
}


std::vector<Ipv4Address> serverAddress;

int main(int argc, char *argv[]){
    LogComponentEnable("Simulation", LOG_LEVEL_INFO);
    bool enable_qcn = false;
    topof.open("scratch/physical_topology.txt");
	if (!topof.is_open()){
		NS_LOG_ERROR("Cannot open physical_topology.txt");
		return 1;
	}

	// KMAX_MAP 3 25000000000 400 50000000000 800 100000000000 1600
	// KMIN_MAP 3 25000000000 100 50000000000 200 100000000000 400
	// PMAX_MAP 3 25000000000 0.2 50000000000 0.2 100000000000 0.2
	rate2kmax[25000000000] = 400;
	rate2kmax[50000000000] = 800;
	rate2kmax[100000000000] = 1600;
	rate2kmin[25000000000] = 100;
	rate2kmin[50000000000] = 200;
	rate2kmin[100000000000] = 400;
	rate2pmax[25000000000] = 0.2;
	rate2pmax[50000000000] = 0.2;
	rate2pmax[100000000000] = 0.2;

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
			Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
			n.Add(sw);
			sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
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
    QbbHelper qbb;
    for (uint32_t i = 0; i < link_num; i++)
	{
		uint32_t src, dst;
		std::string data_rate, link_delay;
		double error_rate;
		topof >> src >> dst >> data_rate >> link_delay >> error_rate;

		Ptr<Node> snode = n.Get(src), dnode = n.Get(dst);

		qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
		qbb.SetChannelAttribute("Delay", StringValue(link_delay));

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

		// used to create a graph of the topology
		nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
		nbr2if[snode][dnode].up = true;
		nbr2if[snode][dnode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
		nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
		nbr2if[dnode][snode].up = true;
		nbr2if[dnode][snode].delay = DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())->GetDelay().GetTimeStep();
		nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

		// This is just to set up the connectivity between nodes. The IP addresses are useless
		char ipstring[16];
		snprintf(ipstring, sizeof(ipstring), "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring, "255.255.255.0");
		ipv4.Assign(d);
        NS_LOG_INFO("Link " << src << " -> " << dst << " created.");
	}

	nic_rate = get_nic_rate(n);
    NS_LOG_INFO("NIC rate: " << nic_rate);
	// config switch
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 1){ // is switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			uint32_t shift = 3; // by default 1/8
			for (uint32_t j = 1; j < sw->GetNDevices(); j++){
				Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
				// set ecn
				uint64_t rate = dev->GetDataRate().GetBitRate();
				NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(), "must set kmin for each link speed");
				NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(), "must set kmax for each link speed");
				NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(), "must set pmax for each link speed");
				sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);
				// set pfc
				uint64_t delay = (uint64_t) DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
				uint32_t headroom = rate * delay / 8 / 1000000000 * 3;
				sw->m_mmu->ConfigHdrm(j, headroom);

				// set pfc alpha, proportional to link bw
				sw->m_mmu->pfc_a_shift[j] = shift;
				while (rate > nic_rate && sw->m_mmu->pfc_a_shift[j] > 0){
					sw->m_mmu->pfc_a_shift[j]--;
					rate /= 2;
				}
			}
			sw->m_mmu->ConfigNPort(sw->GetNDevices()-1);
			sw->m_mmu->ConfigBufferSize(buffer_size * 1024 * 1024);
			sw->m_mmu->node_id = sw->GetId();
		}
	}

	//
	// install RDMA driver
	bool clamp_target_rate = false, l2_back_to_zero = false;
	uint32_t has_win = 1;
	uint32_t global_t = 1;
	uint32_t mi_thresh = 5;
	bool sample_feedback = false;
	double alpha_resume_interval = 55, rp_timer, ewma_gain = 1 / 16;
	uint32_t fast_recovery_times = 5;
	std::string rate_ai, rate_hai, min_rate = "100Mb/s";
	double rate_decrease_interval = 4;
	bool var_win = false, fast_react = true;
	bool multi_rate = true;
	double u_target = 0.95;
	bool rate_bound = true;
	std::string dctcp_rate_ai = "1000Mb/s";
	double pint_prob = 1.0;
	//
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0){ // is server
			// create RdmaHw
			Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
			rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(clamp_target_rate));
			rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(alpha_resume_interval));
			rdmaHw->SetAttribute("RPTimer", DoubleValue(rp_timer));
			rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(fast_recovery_times));
			rdmaHw->SetAttribute("EwmaGain", DoubleValue(ewma_gain));
			rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(rate_ai)));
			rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(rate_hai)));
			rdmaHw->SetAttribute("L2BackToZero", BooleanValue(l2_back_to_zero));
			rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(l2_chunk_size));
			rdmaHw->SetAttribute("L2AckInterval", UintegerValue(l2_ack_interval));
			rdmaHw->SetAttribute("CcMode", UintegerValue(0));
			rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(rate_decrease_interval));
			rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(min_rate)));
			rdmaHw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
			rdmaHw->SetAttribute("MiThresh", UintegerValue(mi_thresh));
			rdmaHw->SetAttribute("VarWin", BooleanValue(var_win));
			rdmaHw->SetAttribute("FastReact", BooleanValue(fast_react));
			rdmaHw->SetAttribute("MultiRate", BooleanValue(multi_rate));
			rdmaHw->SetAttribute("SampleFeedback", BooleanValue(sample_feedback));
			rdmaHw->SetAttribute("TargetUtil", DoubleValue(u_target));
			rdmaHw->SetAttribute("RateBound", BooleanValue(rate_bound));
			rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(dctcp_rate_ai)));
			// rdmaHw->SetPintSmplThresh(pint_prob);
			// create and install RdmaDriver
			Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
			Ptr<Node> node = n.Get(i);
			rdma->SetNode(node);
			rdma->SetRdmaHw(rdmaHw);

			node->AggregateObject (rdma);
			rdma->Init();
			rdma->TraceConnectWithoutContext("QpComplete", MakeCallback (qp_finish));
		}
	}
	
	if (ack_high_prio)
		RdmaEgressQueue::ack_q_idx = 0;
	else
		RdmaEgressQueue::ack_q_idx = 3;

	// Set up routing
	CalculateRoutes(n);
	SetRoutingEntries();

	for (int i = 0; i < node_num; i++) {
		for (int j = 0; j < node_num; j++) {
			std::cout << "Node " << i << " to " << j << ": ";
			std::cout << pairDelay[n.Get(i)][n.Get(j)] << " " << pairTxDelay[n.Get(i)][n.Get(j)] << " " << pairBw[n.Get(i)->GetId()][n.Get(j)->GetId()] << std::endl;
		}
	}
	

	maxRtt = maxBdp = 0;
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() != 0)
			continue;
		for (uint32_t j = 0; j < node_num; j++){
			if (n.Get(j)->GetNodeType() != 0)
				continue;
			uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
			uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
			uint64_t rtt = delay * 2 + txDelay;
			uint64_t bw = pairBw[i][j];
			uint64_t bdp = rtt * bw / 1000000000/8; 
			pairBdp[n.Get(i)][n.Get(j)] = bdp;
			pairRtt[i][j] = rtt;
			if (bdp > maxBdp)
				maxBdp = bdp;
			if (rtt > maxRtt)
				maxRtt = rtt;
		}
	}
	printf("maxRtt=%llu ns maxBdp=%llu B\n", maxRtt, maxBdp);

	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 1){ // switch
			Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
			sw->SetAttribute("CcMode", UintegerValue(0));
			sw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
		}
	}
	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	NS_LOG_INFO("Create Applications.");

	Time interPacketInterval = Seconds(0.0000005 / 2);

	// maintain port number for each host
	for (uint32_t i = 0; i < node_num; i++){
		if (n.Get(i)->GetNodeType() == 0)
			for (uint32_t j = 0; j < node_num; j++){
				if (n.Get(j)->GetNodeType() == 0)
					portNumder[i][j] = 10000; // each host pair use port number from 10000
			}
	}

	RdmaClientHelper clientHelper(
		0, // priority group
		node_id_to_ip(0), // source IP
		node_id_to_ip(1), // dest IP
		portNumder[0][1], // source port
		 ++ portNumder[1][0], // dest port
		packet_payload_size * 5, // write size
		maxBdp, // window
		maxRtt // base RTT
	);

	ApplicationContainer appCon = clientHelper.Install(n.Get(0));
	appCon.Start(Time(0));

	NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(10));
	Simulator::Run();
	Simulator::Destroy();
	NS_LOG_INFO("Done.");

    return 0;
}

