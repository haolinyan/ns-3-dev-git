#ifndef PROGRAMMABLE_SWITCH_H
#define PROGRAMMABLE_SWITCH_H
#include <iostream>
#include <ns3/packet.h>
#include <ns3/ipv4-header.h>
#include <ns3/udp-header.h>
#include "ns3/atp-header.h"
#include <bitset>
#include "ATPCommon.h"

using namespace ns3;
// namespace ns3 {

struct SWAggregator {
    uint32_t bitmap;
    uint8_t counter;
    bool ecn;
    uint32_t jobId;
    uint32_t seqNum;
    double timestamp;
    bool isEmpty;
    bool isAggregated;
};


class ProgrammableSwitch : public Object
{
    public:
        static TypeId GetTypeId() {
            static TypeId tid = TypeId("ns3::ProgrammableSwitch")
                                .SetParent<Object>()
                                .SetGroupName("Applications")
                                .AddConstructor<ProgrammableSwitch>();
            return tid;
        }
        enum pkt_type_t {
            OTHER=1,
            UDP=2,
            ATP=3,
            COLLISION=4,
            MULTICAST=5,
            DUPLICATE=6,
            NORMAL_DROP=7,
            SENDTOPS=8
        };
        ProgrammableSwitch() {}
        ~ProgrammableSwitch() {
            delete[] m_agtr;
        }

        void SetUp(int max_agtr_count) {
            m_agtr = new SWAggregator[max_agtr_count];
            for (size_t i = 0; i < max_agtr_count; i++)
            {
                DeallocateAgtr(i);
            }
        }

        void Parser(Ptr<Packet> &p,
                    Ipv4Header& ipHeader) {                 
            if (ipHeader.GetProtocol() != 17) {
                m_pktType = OTHER;
                return;
            }
            m_pktType = UDP;
            p->RemoveHeader(udpHeader);
            uint16_t srcPort = udpHeader.GetSourcePort();
            uint16_t dstPort = udpHeader.GetDestinationPort();
            if (srcPort != 9 && dstPort != 9) {
                return;
            }
            m_pktType = ATP;
            p->RemoveHeader(atpHeader);
            return;
        }

        void Ingress() {
            NS_ASSERT_MSG(m_pktType == ATP, "The packet type is not ATP");
            uint16_t index = atpHeader.GetAggregatorIndex();
            if (!atpHeader.GetIsAck()) {
                // m_pktType = COLLISION;
                // return;
                if (m_TxQueueSize >= TXDEVICE_THRESHOLD) {
                    atpHeader.SetEcn(true);
                }

                if (atpHeader.GetResend()) {
                    if (m_agtr[index].jobId == atpHeader.GetJobId() && m_agtr[index].seqNum == atpHeader.GetSeqNum()) {
                        // loss recovery
                        if (!m_agtr[index].isAggregated) {
                            m_agtr[index].ecn = atpHeader.GetEcn();
                            m_agtr[index].bitmap |= atpHeader.GetBitmap();
                        }
                        atpHeader.SetBitmap(m_agtr[index].bitmap);
                        atpHeader.SetEcn(m_agtr[index].ecn);
                        DeallocateAgtr(index);
                    }
                    m_pktType = SENDTOPS; 
                    return;
                }

                // std::cout << "At time " << Simulator::Now().GetNanoSeconds()
                // << "ns programmable switch received a packet: " << atpHeader.GetSeqNum()
                // << " SeqNum=" << atpHeader.GetSeqNum() << " index=" << index << std::endl;

                if (m_agtr[index].isEmpty || (m_agtr[index].jobId == atpHeader.GetJobId() && m_agtr[index].seqNum == atpHeader.GetSeqNum())) {
                    m_agtr[index].ecn = atpHeader.GetEcn();
                    if (m_agtr[index].bitmap & atpHeader.GetBitmap()) {
                        m_pktType = DUPLICATE;
                        return;
                    }
                    m_agtr[index].bitmap |= atpHeader.GetBitmap();
                    m_agtr[index].counter = calculateBitSum(m_agtr[index].bitmap);
                    m_agtr[index].timestamp = Simulator::Now().GetSeconds();
                    m_agtr[index].isEmpty = false;
                    m_agtr[index].jobId = atpHeader.GetJobId();
                    m_agtr[index].seqNum = atpHeader.GetSeqNum();
                    if (m_agtr[index].counter == atpHeader.GetFanInDegree()) {
                        m_agtr[index].isAggregated = true;
                        atpHeader.setAck(true);
                        atpHeader.SetBitmap(m_agtr[index].bitmap);
                        atpHeader.SetEcn(m_agtr[index].ecn);
                        m_pktType = SENDTOPS;
                        return;
                    } else {
                        m_pktType = NORMAL_DROP;
                        return;
                    }
                } else {
                    atpHeader.SetCollision(true);
                    atpHeader.SetResend(true);
                    std::cout << "At time " << Simulator::Now().GetNanoSeconds() << "ns switch send a collision packet"
                    << " SeqNum=" << atpHeader.GetSeqNum() << " index=" << index << std::endl;
                    m_pktType = COLLISION;
                    return;
                }  
            } else {
                if (m_agtr[index].jobId == atpHeader.GetJobId() && m_agtr[index].seqNum == atpHeader.GetSeqNum()) {
                   NS_ASSERT_MSG(m_agtr[index].isAggregated, "The packet is not aggregated");
                   NS_ASSERT_MSG(m_agtr[index].counter == atpHeader.GetFanInDegree(), "The counter is not equal to the fan-in-degree");
                   DeallocateAgtr(index);
                }
                m_pktType = MULTICAST;
                return;
            }
        }

        int Deparser(Ptr<Packet> &p,
                    Ipv4Header& ipHeader, 
                    std::vector<Ipv4Header>& ipHeaderList,
                    std::vector<Packet>& packetList
                            ) {
            if (m_pktType == OTHER) {
                return 0;
            } else if (m_pktType == UDP) {
                p->AddHeader(udpHeader);
                return 0;
            } else if (m_pktType == COLLISION || m_pktType == SENDTOPS) {
                p->AddHeader(atpHeader);
                p->AddHeader(udpHeader);
                return 1;
            } else if (m_pktType == MULTICAST) {
                Ipv4Address src = ipHeader.GetDestination();
                for (int i = 0; i < atpHeader.GetFanInDegree(); i++) {
                    ipHeader.SetDestination(m_workerIp[i]);
                    udpHeader.InitializeChecksum(src, m_workerIp[i], 17);
                    p->AddHeader(atpHeader);
                    p->AddHeader(udpHeader);
                    ipHeaderList.push_back(ipHeader);
                    packetList.push_back(*p);
                }
                return 2;
            } else if (m_pktType == DUPLICATE || m_pktType == NORMAL_DROP) {
                return 3;
            } else {
                return 0;
            }
            }

        int Pipeline(Ptr<Packet> &p,
                    Ipv4Header& ipHeader, 
                    std::vector<Ipv4Header>& ipHeaderList,
                    std::vector<Packet>& packetList,
                    uint32_t queueSize
                            ) {
            m_TxQueueSize = queueSize;                        
            Parser(p, ipHeader);
            if (m_pktType == ATP) {
                Ingress();
            }
            
            std::cout << "At time " << Simulator::Now().GetNanoSeconds() 
            << "ns programmable switch received a packet: " << atpHeader.GetSeqNum() 
            << " from " << ipHeader.GetSource() << " m_pkt_type= " << m_pktType << std::endl;
            
            return Deparser(p, ipHeader, ipHeaderList, packetList);
        }

        uint8_t calculateBitSum(uint32_t data) {
            uint8_t sum = 0;
            while (data > 0) {
                sum += data & 1;  // 检查最低位是否为1并加到sum上
                data >>= 1;       // 将data右移一位
            }
            return sum;
        }

        void AddWorkerIp(Ipv4Address ip) {
            m_workerIp.push_back(ip);
        }

    private:
        void DeallocateAgtr(uint16_t index) {
            m_agtr[index].bitmap = 0;
            m_agtr[index].counter = 0;
            m_agtr[index].ecn = false;
            m_agtr[index].jobId = 10;
            m_agtr[index].seqNum = 0;
            m_agtr[index].timestamp = 0.;
            m_agtr[index].isEmpty = true;
            m_agtr[index].isAggregated = false;
        }
        std::vector<Ipv4Address> m_workerIp;
        UdpHeader udpHeader;
        AtpHeader atpHeader;
        SWAggregator* m_agtr;

    public:
        pkt_type_t m_pktType{OTHER};
        uint32_t m_TxQueueSize{0};
};

NS_OBJECT_ENSURE_REGISTERED(ProgrammableSwitch);
// }
#endif