#ifndef PROGRAMMABLE_SWITCH_H
#define PROGRAMMABLE_SWITCH_H
#include <iostream>
#include <ns3/packet.h>
#include <ns3/ipv4-header.h>
#include <ns3/udp-header.h>
#include "ns3/atp-header.h"
#include <bitset>
using namespace ns3;
// namespace ns3 {
class ProgrammableSwitch {
    public:
        enum pkt_type_t {
            OTHER,
            UDP,
            ATP,
            COLLISION,
            BROADCAST
        };
        ProgrammableSwitch() {}
        ~ProgrammableSwitch() {}

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
            if (!atpHeader.GetIsAck()) {
                // atpHeader.SetCollision(true);
                m_pktType = COLLISION;
            } else {
                m_pktType = BROADCAST;
            }
            // m_pktType = COLLISION;
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
            } else if (m_pktType == COLLISION) {
                p->AddHeader(atpHeader);
                p->AddHeader(udpHeader);
                return 1;
            } else if (m_pktType == BROADCAST) {
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
            } else {
                return 0;
            }
            }

        int Pipeline(Ptr<Packet> &p,
                    Ipv4Header& ipHeader, 
                    std::vector<Ipv4Header>& ipHeaderList,
                    std::vector<Packet>& packetList
                            ) {

            Parser(p, ipHeader);
            if (m_pktType == ATP) {
                Ingress();
            }

            std::cout << "At time " << Simulator::Now().GetNanoSeconds() 
            << "ns programmable switch received a packet: " << atpHeader.GetSeqNum() 
            << " from " << ipHeader.GetSource() << std::endl;

            return Deparser(p, ipHeader, ipHeaderList, packetList);
            // uint32_t bitmap = atpHeader.GetBitmap();
            // if ((bitmap & m_bitmap) == 0) {
            //     m_bitmap |= bitmap;
            // }

            // if (calculateBitSum(m_bitmap) == atpHeader.GetFanInDegree()) {
            //     m_bitmap = 0;
            //     m_workerIp.push_back(ipHeader.GetSource());
            //     Ipv4Address src = ipHeader.GetDestination();
            //     for (int i = 0; i < atpHeader.GetFanInDegree(); i++) {
            //         ipHeader.SetDestination(m_workerIp[i]);
            //         ipHeader.SetSource(src);
            //         udpHeader.SetSourcePort(dstPort);
            //         udpHeader.SetDestinationPort(srcPort);
            //         udpHeader.InitializeChecksum(src, m_workerIp[i], 17);
            //         p->AddHeader(atpHeader);
            //         p->AddHeader(udpHeader);
            //         ipHeaderList.push_back(ipHeader);
            //         packetList.push_back(*p);
            //     }
            //     return 2;
            // }
            // m_workerIp.push_back(ipHeader.GetSource());
            // std::cout << std::bitset<32>(m_bitmap) << std::endl;
            // std::cout << "At time " << Simulator::Now().GetSeconds() << "s programmable switch received a packet: " 
            // << "from " << ipHeader.GetSource() << " to " << ipHeader.GetDestination() << std::endl;
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
        std::vector<Ipv4Address> m_workerIp;
        UdpHeader udpHeader;
        AtpHeader atpHeader;

    public:
        pkt_type_t m_pktType{OTHER};
};


// }
#endif