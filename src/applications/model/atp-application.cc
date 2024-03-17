#include "ns3/atp-application.h"
#include "ns3/atp-header.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include "ns3/sequence-number.h"
#define FAST_RESTRANSMATION true
#define CHECK_ACK false

namespace ns3
{
HashTable* AtpApplication::hash_table;
NS_LOG_COMPONENT_DEFINE("AtpApplication");
NS_OBJECT_ENSURE_REGISTERED(AtpApplication);

TypeId
AtpApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AtpApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<AtpApplication>()
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&AtpApplication::m_RemoteAddress),
                          MakeAddressChecker())
            .AddAttribute("Tos",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AtpApplication::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&AtpApplication::m_RemotePort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("totalSize",
                          "Total size of data to be sent in this application",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&AtpApplication::m_totalSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("MaxAGTRSize",
                          "Maximumn number of AGTRs used in this app",
                          UintegerValue(100),
                          MakeUintegerAccessor(&AtpApplication::m_max_agtr_size),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("appId",
                          "Application ID",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AtpApplication::m_appId),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("jobId",
                          "Job ID",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AtpApplication::m_jobId),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("workerId",
                          "Worker ID",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AtpApplication::m_workerId),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("NumWokers",
                          "Number of workers",
                          UintegerValue(2),
                          MakeUintegerAccessor(&AtpApplication::num_workers),
                          MakeUintegerChecker<uint16_t>());
    return tid;
}

AtpApplication::AtpApplication() {
    m_socket = nullptr;
}

AtpApplication::~AtpApplication()
{
    NS_LOG_FUNCTION(this);
}

void
AtpApplication::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void
AtpApplication::StartApplication() {
    NS_LOG_FUNCTION(this);

    hash_table = new HashTable(MAX_AGTR_COUNT);
    cc_manager = new CC_manager(m_max_agtr_size);
    // set the hashtable
    for (size_t i = 0; i < m_max_agtr_size; i++)
    {
        hash_table->HashNew_crc(m_appId, i);
    }
    window_manager = WindowManager();
    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_RemoteAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_RemoteAddress), m_RemotePort));
        }
        else if (InetSocketAddress::IsMatchingType(m_RemoteAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
            m_socket->Connect(m_RemoteAddress);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_RemoteAddress);
        }
    }

    std::stringstream peerAddressStringStream;
    if (Ipv4Address::IsMatchingType(m_RemoteAddress))
    {
        peerAddressStringStream << Ipv4Address::ConvertFrom(m_RemoteAddress);
    }
    else if (Ipv6Address::IsMatchingType(m_RemoteAddress))
    {
        peerAddressStringStream << Ipv6Address::ConvertFrom(m_RemoteAddress);
    }
    else if (InetSocketAddress::IsMatchingType(m_RemoteAddress))
    {
        peerAddressStringStream << InetSocketAddress::ConvertFrom(m_RemoteAddress).GetIpv4();
    }
    else if (Inet6SocketAddress::IsMatchingType(m_RemoteAddress))
    {
        peerAddressStringStream << Inet6SocketAddress::ConvertFrom(m_RemoteAddress).GetIpv6();
    }
    m_RemoteAddressString = peerAddressStringStream.str();

    m_socket->SetRecvCallback(MakeCallback(&AtpApplication::HandleRead, this));
    m_socket->SetAllowBroadcast(true);
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &AtpApplication::SechduleTx, this);
}

void AtpApplication::SechduleTx() {
    NS_LOG_FUNCTION(this);
    m_total_pkt = ceil((double) m_totalSize / P4ML_DATA_SIZE); 
    pending_pkt = m_total_pkt;
    int num_first_time_sending = (m_total_pkt <= m_max_agtr_size) ? m_total_pkt : m_max_agtr_size;
    NS_LOG_INFO("Total packets: " << m_total_pkt << " (" << m_totalSize << ")"
    " First time sending: " << num_first_time_sending);
    pending_pkt -= num_first_time_sending;
    window_manager.ResetConsecutiveOod();
    for (int i = 0; i < num_first_time_sending; i++) {
        SequenceNumber16 seqNum = window_manager.GetNextSeq();
        uint16_t switch_agtr_pos = hash_table->hash_map[i];
        Address from;
        Address to;
        m_socket->GetSockName(from);
        m_socket->GetPeerName(to);

        PacketBuffer packetBuffer;
        packetBuffer.bitmap = (1 << m_workerId);
        packetBuffer.aggregatorIndex = switch_agtr_pos;
        packetBuffer.fanInDegree = num_workers;
        packetBuffer.overflow = false;
        packetBuffer.resend = false;
        packetBuffer.collision = false;
        packetBuffer.ecn = false;
        packetBuffer.isAcked = false;
        packetBuffer.jobId = m_jobId;
        packetBuffer.seqNum = seqNum;
        packetBuffer.from = from;
        packetBuffer.to = to;
        packetBuffer.retransmation = 0;

        window_manager.AddToTxBuffer(packetBuffer);

        if(Send(&packetBuffer))
        {
            m_totalTx += P4ML_PACKET_SIZE;
            NS_LOG_INFO("[firstSent] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " sent " 
            << P4ML_DATA_SIZE << " bytes to " << m_RemoteAddressString << " Seq= " << seqNum);
        }
    }
    m_timeoutEvent = Simulator::Schedule(Seconds(TIMEOUT), &AtpApplication::CheckTimeout, this, 0, num_first_time_sending - 1, window_manager.GetWindowShift());
}

void
AtpApplication::CheckTimeout(int pos_start, int pos_end, uint64_t window_shift) {
    // NS_LOG_INFO("At time: " << Simulator::Now().GetNanoSeconds() << "ns Check the range: " << pos_start << " " << pos_end);
    int* timeout_pkt = new int[pos_end - pos_start + 1];
    int count = window_manager.GetTimeOutPkt(pos_start + window_shift, pos_end + window_shift, timeout_pkt);
    if (count == 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        PacketBuffer packetBuffer = window_manager.TxRxBuffer[timeout_pkt[i]];
        packetBuffer.resend = true;
        window_manager.TxRxBuffer[timeout_pkt[i]].retransmation ++;
        if(Send(&packetBuffer))
        {
            m_totalTx += P4ML_PACKET_SIZE;
            NS_LOG_INFO("[Timeout] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " resent " 
            << P4ML_DATA_SIZE << " bytes to " << m_RemoteAddressString << " Seq= " << packetBuffer.seqNum);
        }

    }
    m_timeoutEvent = Simulator::Schedule(Seconds(TIMEOUT), &AtpApplication::CheckTimeout, this, pos_start, pos_end, window_shift);
    delete[] timeout_pkt;   
}

int AtpApplication::Send(PacketBuffer* packetBuffer)
{
    NS_LOG_FUNCTION(this);
    AtpHeader atpHeader;
    atpHeader.FillHeader(
        packetBuffer->bitmap,
        packetBuffer->fanInDegree,
        packetBuffer->overflow,
        packetBuffer->resend,
        packetBuffer->collision,
        packetBuffer->ecn,
        packetBuffer->isAcked,
        packetBuffer->aggregatorIndex,
        packetBuffer->jobId,
        (uint32_t) packetBuffer->seqNum.GetValue()
    );
    Ptr<Packet> p = Create<Packet>(P4ML_DATA_SIZE);
    p->AddHeader(atpHeader);
    if(m_socket->Send(p) >=0) return 1;
    return 0;
}

void
AtpApplication::StopApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_sendEvent);
    Simulator::Cancel(m_timeoutEvent);
    if (m_socket)
    {
        m_socket->Close();
    }
    NS_LOG_UNCOND("[AppEnd] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId  << " Total Tx: " << m_totalTx << " bytes Total Rx: " << m_totalRx << " bytes");

}


void 
AtpApplication::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address to;
    Address localAddress;
    
    while ((packet = socket->RecvFrom(from)))
    {

        socket->GetSockName(localAddress);
        m_totalRx += packet->GetSize();
        if (packet->GetSize() > 0) {
            uint32_t receivedSize = packet->GetSize();
            AtpHeader atpHeader;
            packet->RemoveHeader(atpHeader); 
            if (!atpHeader.GetIsAck() && CHECK_ACK) {
                NS_LOG_WARN("[NotAck] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " received " << receivedSize << " bytes from " << from);
                continue;
            }
            isECN = atpHeader.GetEcn();
            SequenceNumber16 seqNum(atpHeader.GetSeqNum());
            int deqNum = window_manager.RecvAck(seqNum);
            if (deqNum == -1) {
                NS_LOG_WARN("[DupAck] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " received " << receivedSize << " bytes from " << from << " Seq= " << seqNum);
            } else if (deqNum == 0) {
                NS_LOG_INFO("[OodAck] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " received " << receivedSize 
                << " bytes from " << from << " Seq= " << seqNum << " Expected= " << window_manager.TxRxBuffer.front().seqNum);
            } else {
                NS_LOG_DEBUG("[Ack] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " received " << receivedSize << " bytes from " << from << " Seq= " << seqNum);
            }
        } else {
            return;
        }
        if (pending_pkt == 0) {
            if (window_manager.TxRxBuffer.size() == 0) {
                StopApplication();
            }
            return;
        }


        if (FAST_RESTRANSMATION && window_manager.GetConsecutiveOod() >= FAST_TRANSMISSION_THRESHOLD) {
            // resend the first packet
            PacketBuffer packetBuffer = window_manager.TxRxBuffer.front();
            packetBuffer.resend = true;
            if (window_manager.TxRxBuffer.front().retransmation < MAX_RETRANSMISSION_TIMES) {
                if(Send(&packetBuffer))
                {
                    m_totalTx += P4ML_PACKET_SIZE;
                    NS_LOG_INFO("[FastRetrans] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " resent " 
                    << P4ML_DATA_SIZE << " bytes to " << m_RemoteAddressString << " Seq= " << packetBuffer.seqNum);
                }
            window_manager.ResetConsecutiveOod();
            window_manager.TxRxBuffer.front().retransmation ++;
            }
        }


        int new_window_size = cc_manager->adjustWindow(isECN);
        if (isECN) isECN = false;
        int available_window = new_window_size - window_manager.TxRxBuffer.size();
        // NS_LOG_INFO("New window size: " << new_window_size << " Available window: " << available_window);
        if (available_window <= 0) {
            return;
        }
        int num_sending = (pending_pkt <= available_window) ? pending_pkt : available_window;
        pending_pkt -= num_sending;
        for (int i = 0; i < num_sending; i++) {
            SequenceNumber16 seqNum = window_manager.GetNextSeq();
            uint16_t switch_agtr_pos = hash_table->hash_map[i];
            Address from;
            Address to;
            m_socket->GetSockName(from);
            m_socket->GetPeerName(to);

            PacketBuffer packetBuffer;
            packetBuffer.bitmap = (1 << m_workerId);
            packetBuffer.aggregatorIndex = switch_agtr_pos;
            packetBuffer.fanInDegree = num_workers;
            packetBuffer.overflow = false;
            packetBuffer.resend = false;
            packetBuffer.collision = false;
            packetBuffer.ecn = false;
            packetBuffer.isAcked = false;
            packetBuffer.jobId = m_jobId;
            packetBuffer.seqNum = seqNum;
            packetBuffer.from = from;
            packetBuffer.to = to;
            packetBuffer.retransmation = 0;

            window_manager.AddToTxBuffer(packetBuffer);

            if(Send(&packetBuffer))
            {
                m_totalTx += P4ML_PACKET_SIZE;
                NS_LOG_INFO("[Sent] At time: " << Simulator::Now().GetNanoSeconds() << "ns Worker: " << m_workerId << " sent " 
                << P4ML_DATA_SIZE << " bytes to " << m_RemoteAddressString << " Seq= " << seqNum);
            }
        }
        m_timeoutEvent = Simulator::Schedule(Seconds(TIMEOUT), &AtpApplication::CheckTimeout, this, window_manager.TxRxBuffer.size() - num_sending, window_manager.TxRxBuffer.size() - 1, window_manager.GetWindowShift());
    }
}

}