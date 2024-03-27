#ifndef ATP_SERVER_H
#define ATP_SERVER_H
#include <ns3/node.h>
#include <queue>
#include <ns3/ipv4-header.h>
#include "Common.h"
#include "ns3/atp-tag.h"
#include "atp-header.h"
#include "ns3/simulator.h"
namespace ns3 {

class AtpServer : public Object {
public:
    static TypeId GetTypeId (void) {
        static TypeId tid = TypeId ("ns3::AtpServer")
            .SetParent<Object> ();
        return tid;
    }
    AtpServer() {}
    
    void SetNode(Ptr<Node> node) {
        m_node = node;
    }

    void ReceivePacket(Ptr<Packet> packet) {
        AtpHeader atpHeader;
        Ipv4Header ipHeader;
        AtpTag tag;
        packet->RemoveHeader(ipHeader);
        packet->RemoveHeader(atpHeader);
        packet->RemovePacketTag(tag);
        Ipv4Address src = ipHeader.GetSource();
        Ipv4Address dst = ipHeader.GetDestination();
        NS_LOG_UNCOND(PS << "[Recv] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
        ipHeader.SetSource(dst);
        ipHeader.SetDestination(src);
        ipHeader.SetTtl(64);
        atpHeader.SetAck(true);
        packet->AddHeader(atpHeader);
        packet->AddHeader(ipHeader);
        m_TxQueue.push(std::make_pair(packet, src));
        Dequeue(1); 
    }

    bool Dequeue(uint32_t num) {
        if (m_TxQueue.empty()) {
            return false;
        } else {
            for (size_t i = 0; i < num; i++) {
                auto p = m_TxQueue.front();
                m_TxQueue.pop();
                m_SendPacketCallback(p.first, p.second, 0); 
            }
        }
        return true;
    }

    SendPacketCallback m_SendPacketCallback;

private:
    std::queue<std::pair<Ptr<Packet>, Ipv4Address>> m_TxQueue;
    Ptr<Node> m_node;
};

}
#endif