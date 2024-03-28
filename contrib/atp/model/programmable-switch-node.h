#ifndef PSWITCH_NODE_H
#define PSWITCH_NODE_H
#include <unordered_map>
#include <ns3/node.h>
#include <ns3/ipv4-header.h>
#include "atp-header.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "sim-net-device.h"
#include "Common.h"
#include "ns3/atp-tag.h"
namespace ns3 {
struct SWAggregator {
    uint32_t bitmap;
    uint8_t counter;
    bool ecn;
    uint32_t appID;
    uint64_t key;
    uint32_t seqNum;
    double timestamp;
    bool isEmpty;
    bool isAggregated;
};
class Packet;
class ProgrammableSwitchNode : public Node {
private:
    std::unordered_map<uint32_t, int> m_rtTable;
    std::vector<uint32_t> MulticastAddr;
    void DeallocateAgtr(uint16_t index) {
            m_agtr[index].bitmap = 0;
            m_agtr[index].counter = 0;
            m_agtr[index].ecn = false;
            m_agtr[index].appID = 0;
            m_agtr[index].key = 0;
            m_agtr[index].seqNum = 0;
            m_agtr[index].timestamp = 0.;
            m_agtr[index].isEmpty = true;
            m_agtr[index].isAggregated = false;
        }
    SWAggregator* m_agtr{nullptr};

    int SendToDev(Ptr<Packet>p, uint32_t dip, bool Multicast, uint32_t priority) {
        // multicast or send to the ps
        if (Multicast) {
            for (uint32_t i = 0; i < MulticastAddr.size(); i++) {
                auto entry = m_rtTable.find(MulticastAddr[i]);
                NS_ASSERT(entry != m_rtTable.end());
                Ptr<Packet> p_cp = p->Copy();
                GetDevice(entry->second)->GetObject<SimNetDevice>()->SendWithPriority(p_cp, Ipv4Address(dip), 0x0800, priority);
            }
            return 0;
        } else {
            auto entry = m_rtTable.find(dip);
            NS_ASSERT(entry != m_rtTable.end());
            GetDevice(entry->second)->GetObject<SimNetDevice>()->SendWithPriority(p, Ipv4Address(dip), 0x0800, priority);
            return entry->second;
        }
    }

public:
    static TypeId GetTypeId (void) {
        static TypeId tid = TypeId ("ns3::ProgrammableSwitchNode")
                                    .SetParent<Node> ()
                                    .AddConstructor<ProgrammableSwitchNode> ();

        return tid;
    }

    ProgrammableSwitchNode(){
        m_node_type = 1;
    }

    ~ProgrammableSwitchNode() {
        m_rtTable.clear();
        if (m_agtr)
            delete[] m_agtr;
    }

    void SetUp(int max_agtr_count) {
        m_agtr = new SWAggregator[max_agtr_count];
        for (size_t i = 0; i < max_agtr_count; i++)
        {
            DeallocateAgtr(i);
        }
    }

    void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx) {
        uint32_t dip = dstAddr.Get();
	    m_rtTable[dip] = intf_idx;
    }
    void AddMulticastAddr(Ipv4Address &dstAddr) {
        MulticastAddr.push_back(dstAddr.Get());
    }
    void ClearTable() {
        m_rtTable.clear();
    }
    bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet) {
        // Parser
        Ipv4Header ipHeader;
        packet->RemoveHeader(ipHeader);
        Ipv4Address dst = ipHeader.GetDestination();
        if (ipHeader.GetProtocol() != 18) {
            packet->AddHeader(ipHeader);
            SendToDev(packet, dst.Get(), false, 0);
        }

        // Ingress
        AtpHeader atpHeader;
        packet->RemoveHeader(atpHeader);
        AtpTag tag;
        packet->RemovePacketTag(tag);
        tag.SetSeq(atpHeader.GetSeqNum());
        
    #if ATP_ENABLE
        uint16_t index = atpHeader.GetAggregatorIndex();
        if (!atpHeader.GetIsAck()) {
            // Received from workers
            if (tag.GetQueueSize() >= 1000) {
                atpHeader.SetEcn(true);
            }
            if (atpHeader.GetResend()) {
                if (m_agtr[index].appID == atpHeader.GetAppID() && m_agtr[index].seqNum == atpHeader.GetSeqNum() && m_agtr[index].key == atpHeader.GetKey()) {
                    if (!m_agtr[index].isAggregated) {
                        m_agtr[index].ecn = atpHeader.GetEcn();
                        m_agtr[index].bitmap |= atpHeader.GetBitmap();
                    }
                    atpHeader.SetBitmap(m_agtr[index].bitmap);
                    atpHeader.SetEcn(m_agtr[index].ecn);
                    DeallocateAgtr(index);
                }
                // send to PS with the highest priority
                packet->AddHeader(atpHeader);
                packet->AddHeader(ipHeader);
                packet->AddPacketTag(tag);
                SendToDev(packet, dst.Get(), false, 1);
                return true;
            }
            if (m_agtr[index].isEmpty || (m_agtr[index].appID == atpHeader.GetAppID() && m_agtr[index].seqNum == atpHeader.GetSeqNum() && m_agtr[index].key == atpHeader.GetKey())) {
                m_agtr[index].ecn = atpHeader.GetEcn();
                if (m_agtr[index].bitmap & atpHeader.GetBitmap()) {
                    // drop the dup packet
                    return false;
                }
                m_agtr[index].bitmap |= atpHeader.GetBitmap();
                m_agtr[index].counter = calculateBitSum(m_agtr[index].bitmap);
                m_agtr[index].timestamp = Simulator::Now().GetSeconds();
                m_agtr[index].isEmpty = false;
                m_agtr[index].appID = atpHeader.GetAppID();
                m_agtr[index].key = atpHeader.GetKey();
                m_agtr[index].seqNum = atpHeader.GetSeqNum();
                if (m_agtr[index].counter == atpHeader.GetFanInDegree()) {
                    m_agtr[index].isAggregated = true;
                    atpHeader.SetAck(true);
                    atpHeader.SetBitmap(m_agtr[index].bitmap);
                    atpHeader.SetEcn(m_agtr[index].ecn);
                    packet->AddHeader(atpHeader);
                    packet->AddHeader(ipHeader);
                    packet->AddPacketTag(tag);
                    SendToDev(packet, dst.Get(), false, 0);
                    return true;
                } else {
                    // drop the packet
                    return false;
                }
            } else {
                // Collision
                atpHeader.SetCollision(true);
                atpHeader.SetResend(true);
                packet->AddHeader(atpHeader);
                packet->AddHeader(ipHeader);
                packet->AddPacketTag(tag);
                SendToDev(packet, dst.Get(), false, 0);
                return true;
            }
        } else {
            // Received from PS
            if (m_agtr[index].appID == atpHeader.GetAppID() && m_agtr[index].seqNum == atpHeader.GetSeqNum() && m_agtr[index].key == atpHeader.GetKey()) {
                NS_ASSERT_MSG(m_agtr[index].isAggregated, "The packet is not aggregated");
                NS_ASSERT_MSG(m_agtr[index].counter == atpHeader.GetFanInDegree(), "The counter is not equal to the fan-in-degree");
                DeallocateAgtr(index);
            }
            packet->AddHeader(atpHeader);
            packet->AddHeader(ipHeader);
            SendToDev(packet, dst.Get(), true, (uint32_t) atpHeader.GetResend());
            return true;
        }
        exit(-1);
    #endif

        if (tag.GetQueueSize() >= 1000) {
            NS_LOG_UNCOND(SW << "sets ECN mark at time: " << Simulator::Now().GetNanoSeconds() << "ns, seq: " << atpHeader.GetSeqNum() << " from " << device->GetObject<SimNetDevice>()->GetRemoteNodeId());
            atpHeader.SetEcn(1);
        }
        if (atpHeader.GetIsAck()) {
            // broadcast ack
            packet->AddHeader(atpHeader);
            packet->AddHeader(ipHeader);
            SendToDev(packet, dst.Get(), true, 0);
        } else {
            packet->AddHeader(atpHeader);
            packet->AddHeader(ipHeader);
            SendToDev(packet, dst.Get(), false, 0);
            // if (atpHeader.GetSeqNum() == 100) NS_LOG_UNCOND(SW << " At time: " << Simulator::Now().GetNanoSeconds() << "ns, transmit seq: " << atpHeader.GetSeqNum() 
            // << " from " << device->GetObject<SimNetDevice>()->GetRemoteNodeId() 
            // << " to " << GetDevice(itf)->GetObject<SimNetDevice>()->GetRemoteNodeId() << " Qsize: " << QueueSize);
        }
	    return true;
    }

    uint8_t calculateBitSum(uint32_t data) {
            uint8_t sum = 0;
            while (data > 0) {
                sum += data & 1;  
                data >>= 1;       
            }
            return sum;
        }
};

}

#endif