#ifndef ATP_SERVER_H
#define ATP_SERVER_H
#include <ns3/node.h>
#include <queue>
#include <ns3/ipv4-header.h>
#include "Common.h"
#include "ns3/atp-tag.h"
#include "atp-header.h"
#include "ns3/simulator.h"
#include "ns3/HashTable.h"
namespace ns3 {
struct Aggregator {
    uint32_t bitmap;
    uint8_t counter;
    bool ecn;
    uint32_t appID;
    uint32_t seqNum;
    double timestamp;
    bool isExpired;
    bool isAggregated;
    bool isCollision;
    uint64_t key;
};
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
    void Reset() {
        if (hash_table != nullptr) {
            delete hash_table;
        }
        if (m_agtr != nullptr) {
            delete[] m_agtr;
        }

        hash_table = new HashTable(UsedSwitchAGTRcount);
        m_agtr = new Aggregator[PS_BUFFER_SIZE];
        next_agtr_index = new int[MAX_AGTR_COUNT];
        for (int i = 0; i < MAX_AGTR_COUNT; i++) {
            next_agtr_index[i] = -1;
        }
        for (size_t i = 0; i < PS_BUFFER_SIZE; i++)
            ResetAggregator(i);
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
        uint16_t aggregatorIndex = atpHeader.GetAggregatorIndex();
        uint16_t appID = atpHeader.GetAppID();
        uint16_t seqNum = atpHeader.GetSeqNum();
        uint64_t key = atpHeader.GetKey();
        uint8_t fanInDegree = atpHeader.GetFanInDegree();
        uint32_t bitmap = atpHeader.GetBitmap();
        bool isResend = atpHeader.GetResend();
        if (!hash_table->isAlreadyDeclare[aggregatorIndex]) 
            hash_table->isAlreadyDeclare[aggregatorIndex] = true;
        uint32_t index = (uint32_t) seqNum % PS_BUFFER_SIZE;
        bool collision = isCollision(index, appID, seqNum, key);
        if (collision) {
            if (m_agtr[index].appID != appID || m_agtr[index].seqNum != seqNum || m_agtr[index].key != key) {
                return;
            }
        }
        Aggregator agtr = m_agtr[index];
        if (agtr.counter == fanInDegree && isResend) {
            // resend the aggregated packet (send the parameter packet immediately)
            m_agtr[index].timestamp = Simulator::Now().GetSeconds();
            m_agtr[index].isAggregated = true;
            m_agtr[index].ecn |= atpHeader.GetEcn();
            m_agtr[index].isCollision |= atpHeader.GetCollision();
            atpHeader.SetBitmap(m_agtr[index].bitmap);
            atpHeader.SetEcn(m_agtr[index].ecn);
            atpHeader.SetAck(true);
            atpHeader.SetCollision(m_agtr[index].isCollision);
            ipHeader.SetSource(dst);
            ipHeader.SetDestination(src);
            ipHeader.SetTtl(64);
            packet->AddHeader(atpHeader);
            packet->AddHeader(ipHeader);
            AtpTag tag;
            tag.SetSeq(seqNum);
            packet->AddPacketTag(tag);
            m_SendPacketCallback(packet, src, 1); 
            NS_LOG_UNCOND(PS << "[RT] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
            return;
        }

        if ((agtr.bitmap & bitmap) == 1) {
            // duplicate packet
            NS_LOG_UNCOND(PS << "[Dup] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
            return;
        }

        m_agtr[index].bitmap |= bitmap;
        m_agtr[index].isCollision |= atpHeader.GetCollision();
        m_agtr[index].counter = calculateBitSum(m_agtr[index].bitmap);
        m_agtr[index].ecn |= atpHeader.GetEcn();
        m_agtr[index].appID = appID;
        m_agtr[index].seqNum = seqNum;
        m_agtr[index].timestamp = Simulator::Now().GetSeconds();
        m_agtr[index].isExpired = false;
        m_agtr[index].key = key;

        if (m_agtr[index].counter == fanInDegree) {
            m_agtr[index].isAggregated = true;
            if (m_agtr[index].isCollision) {
                // Check if new agtr is already hashed
                int new_hash_agtr;
                if (next_agtr_index[aggregatorIndex] == -1) {
                    new_hash_agtr = hash_table->HashNew_predefine();
                    if (new_hash_agtr != -1) {
                        next_agtr_index[aggregatorIndex] = new_hash_agtr;
                        hash_table->hash_map[aggregatorIndex] = new_hash_agtr;
                    } else {
                        new_hash_agtr = aggregatorIndex;
                    }
                } else {
                    new_hash_agtr = next_agtr_index[aggregatorIndex];
                }
                atpHeader.SetLen((uint32_t) new_hash_agtr);
            }
            atpHeader.SetBitmap(m_agtr[index].bitmap);
            atpHeader.SetEcn(m_agtr[index].ecn);
            atpHeader.SetAck(true);
            atpHeader.SetCollision(m_agtr[index].isCollision);
            ipHeader.SetSource(dst);
            ipHeader.SetDestination(src);
            ipHeader.SetTtl(64);
            packet->AddHeader(atpHeader);
            packet->AddHeader(ipHeader);
            AtpTag tag;
            tag.SetSeq(seqNum);
            packet->AddPacketTag(tag);
            m_TxQueue.push(std::make_pair(packet, src));
            Dequeue(1); 
        } 
        NS_LOG_UNCOND(PS << "[Recv] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
        
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

    uint8_t calculateBitSum(uint32_t data) {
        uint8_t sum = 0;
        while (data > 0) {
            sum += data & 1;  
            data >>= 1;       
        }
        return sum;
    }

    void ResetAggregator(uint16_t index) {
        m_agtr[index].bitmap = 0;
        m_agtr[index].counter = 0;
        m_agtr[index].ecn = false;
        m_agtr[index].appID = 0;
        m_agtr[index].seqNum = 0;
        m_agtr[index].timestamp = 0.;
        m_agtr[index].isExpired = true;
        m_agtr[index].isAggregated = false;
        m_agtr[index].key = 0;
        m_agtr[index].isCollision = false;
    }

    bool isCollision(uint32_t index, uint16_t appID, uint16_t seqNum, uint64_t key) 
    {
        if (m_agtr[index].isExpired) {
            return false;
        }

        if (m_agtr[index].timestamp + PS_BUFFER_TIMEOUT >= Simulator::Now().GetSeconds()) {
            return true;
        }

        if (m_agtr[index].isAggregated && (m_agtr[index].appID != appID || m_agtr[index].seqNum != seqNum || m_agtr[index].key != key)) {
            ResetAggregator(index);
            return false;
        }

        if (!m_agtr[index].isAggregated && (m_agtr[index].appID != appID || m_agtr[index].seqNum != seqNum || m_agtr[index].key != key)) {
            std::cout << "WARNNING: Unaggregated ATP packet is expired, seqNum: " << m_agtr[index].seqNum << " appID: " << m_agtr[index].appID << std::endl;
            return true;
        }
        if (m_agtr[index].isAggregated) {
            ResetAggregator(index);
            return false;
        }
        return true;
    }

    SendPacketCallback m_SendPacketCallback;

private:
    std::queue<std::pair<Ptr<Packet>, Ipv4Address>> m_TxQueue;
    Ptr<Node> m_node;
    HashTable* hash_table{nullptr};
    int UsedSwitchAGTRcount{MAX_AGTR_COUNT};
    Aggregator* m_agtr{nullptr};
    int* next_agtr_index{nullptr};
};

}
#endif