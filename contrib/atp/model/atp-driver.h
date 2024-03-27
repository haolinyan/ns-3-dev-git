#ifndef ATP_DRIVER_H
#define ATP_DRIVER_H
#include <ns3/node.h>
#include "sim-net-device.h"
#include <vector>
#include <unordered_map>
#include "ns3/ipv4.h"
#include "atp-server.h"
#include "atp-client.h"
namespace ns3 {

class AtpDriver : public Object {
public:
    static TypeId GetTypeId (void);
    AtpDriver();
    void Init(void);
    void SetNode(Ptr<Node> node);
    void AddTableEntry(Ipv4Address ip, int devIndex);
    void SendPacket(Ptr<Packet> packet, Ipv4Address dst, uint32_t priority);
    void ReceivePacket(Ptr<Packet> packet);
	void SetServer(Ptr<AtpServer>);
    void SetClient(Ptr<AtpClient>);
    // Add a task
    void AddTask(uint64_t size, Ipv4Address _sip, Ipv4Address _dip, Callback<void> notifyAppFinish);
    void PrintTable();
private:
    std::unordered_map<uint32_t, int> m_rTable;
    std::vector<Ptr<SimNetDevice>> m_nic;
    Ptr<Node> m_node;
    Ptr<AtpServer> m_server{nullptr};
    Ptr<AtpClient> m_client{nullptr};
    
};
}



#endif /* RDMA_DRIVER_H */