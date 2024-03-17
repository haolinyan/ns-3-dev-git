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
        ProgrammableSwitch() {}
        ~ProgrammableSwitch() {}
        uint8_t calculateBitSum(uint32_t data) {
            uint8_t sum = 0;
            while (data > 0) {
                sum += data & 1;  // 检查最低位是否为1并加到sum上
                data >>= 1;       // 将data右移一位
            }
            return sum;
            }
        int Pipeline(Ptr<Packet> &p,
                    Ipv4Header& ipHeader, 
                    std::vector<Ipv4Header>& ipHeaderList,
                    std::vector<Packet>& packetList
                            ) {

            if (ipHeader.GetProtocol() != 17) {
                return 0;
            }
            UdpHeader udpHeader;
            p->PeekHeader(udpHeader);
            uint16_t srcPort = udpHeader.GetSourcePort();
            uint16_t dstPort = udpHeader.GetDestinationPort();
            if (srcPort != 9 && dstPort != 9) {
                return 0;
            }
            
            p->RemoveHeader(udpHeader);
            AtpHeader atpHeader;
            p->RemoveHeader(atpHeader);
            
            uint32_t bitmap = atpHeader.GetBitmap();
            if ((bitmap & m_bitmap) == 0) {
                m_bitmap |= bitmap;
            }

            if (calculateBitSum(m_bitmap) == atpHeader.GetFanInDegree()) {
                m_bitmap = 0;
                m_workerIp.push_back(ipHeader.GetSource());
                Ipv4Address src = ipHeader.GetDestination();
                for (int i = 0; i < atpHeader.GetFanInDegree(); i++) {
                    ipHeader.SetDestination(m_workerIp[i]);
                    ipHeader.SetSource(src);
                    udpHeader.SetSourcePort(dstPort);
                    udpHeader.SetDestinationPort(srcPort);
                    udpHeader.InitializeChecksum(src, m_workerIp[i], 17);
                    p->AddHeader(atpHeader);
                    p->AddHeader(udpHeader);
                    ipHeaderList.push_back(ipHeader);
                    packetList.push_back(*p);
                }
                return 2;
            }
            m_workerIp.push_back(ipHeader.GetSource());
            // std::cout << std::bitset<32>(m_bitmap) << std::endl;
            // std::cout << "At time " << Simulator::Now().GetSeconds() << "s programmable switch received a packet: " 
            // << "from " << ipHeader.GetSource() << " to " << ipHeader.GetDestination() << std::endl;
            return 1;
        }
    private:
        uint32_t m_bitmap{0};
        std::vector<Ipv4Address> m_workerIp;
};


// }
#endif