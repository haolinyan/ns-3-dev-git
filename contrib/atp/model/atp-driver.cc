#include "atp-driver.h"

namespace ns3
{
TypeId AtpDriver::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::AtpDriver")
		.SetParent<Object> ();
	return tid;
}

AtpDriver::AtpDriver(){
}

void AtpDriver::Init(void){
	Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();
	for (uint32_t i = 0; i < m_node->GetNDevices(); i++){
		Ptr<SimNetDevice> dev = m_node->GetDevice(i)->GetObject<SimNetDevice>();
		m_nic.push_back(dev);
        if (!dev) continue;
        if (m_server) {
            dev->m_rxCallback = MakeCallback(&AtpDriver::ReceivePacket, this);
        } else if (m_client) {
            dev->m_rxCallback = MakeCallback(&AtpDriver::ReceivePacket, this);
        } else {
            NS_ASSERT_MSG(false, "Server or Client not set");
        }
    }
    if (m_server) {
        m_server->SetNode(m_node);
        m_server->m_SendPacketCallback = MakeCallback(&AtpDriver::SendPacket, this);
    } else if (m_client) {
        m_client->SetNode(m_node);
        m_client->m_SendPacketCallback = MakeCallback(&AtpDriver::SendPacket, this);
        m_client->m_CancelNICQueueCallback = MakeCallback(&AtpDriver::CancelTask, this);
    } else {
        NS_ASSERT_MSG(false, "Server or Client not set");
    }
}

void AtpDriver::CancelTask() {
    m_nic[1]->ClearQueue();
}

void AtpDriver::AddTableEntry(Ipv4Address ip, int devIndex) {
    m_rTable[ip.Get()] = devIndex;
}

void AtpDriver::PrintTable() {
    for (auto it = m_rTable.begin(); it != m_rTable.end(); it++) {
        std::cout << it->first << " " << it->second << std::endl;
    }
}

void AtpDriver::ReceivePacket(Ptr<Packet> packet) {
    if (m_server) {
        m_server->ReceivePacket(packet);
    } else if (m_client) {
        m_client->ReceivePacket(packet);
    } else {
        NS_ASSERT_MSG(false, "Server or Client not set");
    }
}
void AtpDriver::SetNode(Ptr<Node> node){
	m_node = node;
}

void AtpDriver::SetServer(Ptr<AtpServer> server){
    m_server = server;
}

void AtpDriver::SetClient(Ptr<AtpClient> client){
    m_client = client;
}

void AtpDriver::AddTask(uint64_t size, 
            Ipv4Address _sip, 
            Ipv4Address _dip, 
            Callback<void> notifyAppFinish)
{
    NS_ASSERT_MSG(m_client, "Client is not set");
    m_client->AddTask(size, _sip, _dip, notifyAppFinish);
}

void AtpDriver::SendPacket(Ptr<Packet> packet, Ipv4Address dst, uint32_t priority){
    uint32_t key = dst.Get();
    NS_ASSERT_MSG(m_rTable.find(key) != m_rTable.end(), "Destination not found");
    Ptr<SimNetDevice> dev = m_nic[m_rTable[key]];
    NS_ASSERT_MSG(dev, "Device not found");
    dev->SendWithPriority(packet, dst, 0x0800, priority);
}
} // namespace ns3
