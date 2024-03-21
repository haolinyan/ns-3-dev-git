#include "atp-server-app.h"
#include "ns3/address-utils.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/uinteger.h"
#include "ns3/atp-header.h"
#include "ns3/simulator.h"
#include "ns3/ATPCommon.h"
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AtpServer");

NS_OBJECT_ENSURE_REGISTERED(AtpServer);

TypeId
AtpServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AtpServer")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<AtpServer>()
            .AddAttribute("Port",
                          "Port on which we listen for incoming packets.",
                          UintegerValue(9),
                          MakeUintegerAccessor(&AtpServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Tos",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AtpServer::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddTraceSource("AggThroughputTrace",
                          "Aggregator throughput to trace.",
                          MakeTraceSourceAccessor(&AtpServer::m_agg_thoughtput),
                          "ns3::AtpApplication::AggThroughputTracedCallback")
            .AddAttribute("AppID",
                            "Application ID",
                            UintegerValue(0),
                            MakeUintegerAccessor(&AtpServer::m_appID),
                            MakeUintegerChecker<uint16_t>());
    return tid;
}

AtpServer::AtpServer()
{
    NS_LOG_FUNCTION(this);
}

AtpServer::~AtpServer()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;
}

void
AtpServer::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}


void 
AtpServer::ResetAggregator(uint16_t index) {
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

void
AtpServer::StartApplication()
{
    NS_LOG_FUNCTION(this);
    hash_table = new HashTable(UsedSwitchAGTRcount);
    m_agtr = new Aggregator[PS_BUFFER_SIZE];
    next_agtr_index = new int[MAX_AGTR_COUNT];
    for (int i = 0; i < MAX_AGTR_COUNT; i++) {
        next_agtr_index[i] = -1;
    }
    for (size_t i = 0; i < PS_BUFFER_SIZE; i++)
        ResetAggregator(i);
        
    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        if (addressUtils::IsMulticast(m_local))
        {
            Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket>(m_socket);
            if (udpSocket)
            {
                // equivalent to setsockopt (MCAST_JOIN_GROUP)
                udpSocket->MulticastJoinGroup(0, m_local);
            }
            else
            {
                NS_FATAL_ERROR("Error: Failed to join multicast group");
            }
        }
    }

    m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
    m_socket->SetRecvCallback(MakeCallback(&AtpServer::HandleRead, this));
    m_statsEvent = Simulator::Schedule(Seconds(TimeInterval), &AtpServer::Stats, this);
    m_mainEvent = Simulator::Schedule(Seconds(0.0), &AtpServer::main_loop, this);
}

void
AtpServer::Stats() {
    double RX = (double) ((m_received * P4ML_DATA_SIZE * 8) / 1e+9) / TimeInterval;
    double TX = (double) ((m_sent * P4ML_DATA_SIZE * 8) / 1e+9) / TimeInterval;
    m_received = 0;
    m_sent = 0;
    m_agg_thoughtput(TX, RX);
    m_statsEvent = Simulator::Schedule(Seconds(TimeInterval), &AtpServer::Stats, this);
}

void
AtpServer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    Simulator::Cancel(m_statsEvent);
    Simulator::Cancel(m_mainEvent);
    delete[] m_agtr;
    delete hash_table;
}

void
AtpServer::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        socket->GetSockName(localAddress);
        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_DEBUG("At time " << Simulator::Now().As(Time::S) << " server received "
                                   << packet->GetSize() << " bytes from "
                                   << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(from).GetPort());
        }

        packet->RemoveAllPacketTags();
        packet->RemoveAllByteTags();
        AtpHeader atpHeader;
        packet->RemoveHeader(atpHeader);
        PacketInfo packetInfo;
        packetInfo.bitmap = atpHeader.GetBitmap();
        packetInfo.fanInDegree = atpHeader.GetFanInDegree();
        packetInfo.overflow = atpHeader.GetOverflow();
        packetInfo.resend = atpHeader.GetResend();
        packetInfo.collision = atpHeader.GetCollision();
        packetInfo.ecn = atpHeader.GetEcn();
        packetInfo.isAcked = atpHeader.GetIsAck();
        packetInfo.appID = atpHeader.GetAppID();
        packetInfo.seqNum = atpHeader.GetSeqNum();
        packetInfo.aggregatorIndex = atpHeader.GetAggregatorIndex();
        packetInfo.key = atpHeader.GetKey();
        packetInfo.len_tensor = atpHeader.GetLenTensor();
        if (packetInfo.appID == m_appID)
        {
            RxQueue.push(std::make_pair(packetInfo, from));
        }
    }
    m_received ++;
    if (!m_mainEvent.IsRunning()) {
        m_mainEvent = Simulator::Schedule(NanoSeconds(0.0), &AtpServer::main_loop, this);
    }
}


uint8_t 
AtpServer::calculateBitSum(uint32_t data) {
    uint8_t sum = 0;
    while (data > 0) {
        sum += data & 1;  
        data >>= 1;       
    }
    return sum;
}

void
AtpServer::main_loop() 
{
    int to_be_sent = 0;
    while (!RxQueue.empty()) {
        running_time = Simulator::Now().GetSeconds();
        std::pair<PacketInfo, Address> p = RxQueue.front();
        if (!hash_table->isAlreadyDeclare[p.first.aggregatorIndex]) 
                hash_table->isAlreadyDeclare[p.first.aggregatorIndex] = true;

        uint32_t index = p.first.seqNum % PS_BUFFER_SIZE;
        if (isCollision(index, &p.first)) {
            if (m_agtr[index].appID != p.first.appID || m_agtr[index].seqNum != p.first.seqNum || m_agtr[index].key != p.first.key) {
                RxQueue.pop();
                NS_LOG_INFO("At time " << Simulator::Now().GetNanoSeconds() << " ns server received an expired packet, seqNum: " << p.first.seqNum << " appID: " << p.first.appID);
                continue;
            }
        }
        Aggregator agtr = m_agtr[index];
        if (agtr.counter == p.first.fanInDegree && p.first.resend) {
            // resend the aggregated packet (send the parameter packet immediately)
            m_agtr[index].timestamp = Simulator::Now().GetSeconds();
            m_agtr[index].isAggregated = true;
            m_agtr[index].ecn |= p.first.ecn;
            PacketInfo resent_packet = p.first;
            resent_packet.bitmap = m_agtr[index].bitmap;
            resent_packet.ecn = m_agtr[index].ecn;
            resent_packet.isAcked = true;
            resent_packet.collision = m_agtr[index].isCollision;
            SendParameterPacket(resent_packet, p.second);
            RxQueue.pop();
            NS_LOG_INFO("At time " << Simulator::Now().GetNanoSeconds() << " ns server resent the aggregated packet, seqNum: " << p.first.seqNum << " appID: " << p.first.appID);
            continue;
        }

        if ((agtr.bitmap & p.first.bitmap) == 1) {
            // duplicate packet
            RxQueue.pop();
            NS_LOG_INFO("At time " << Simulator::Now().GetNanoSeconds() << " ns server received a duplicate packet, seqNum: " << p.first.seqNum << " appID: " << p.first.appID);
            continue;
        }
        
        m_agtr[index].bitmap |= p.first.bitmap;
        m_agtr[index].isCollision |= p.first.collision;
        m_agtr[index].counter = calculateBitSum(m_agtr[index].bitmap);
        m_agtr[index].ecn |= p.first.ecn;
        m_agtr[index].appID = p.first.appID;
        m_agtr[index].seqNum = p.first.seqNum;
        m_agtr[index].timestamp = Simulator::Now().GetSeconds();
        m_agtr[index].isExpired = false;
        m_agtr[index].key = p.first.key;
        if (m_agtr[index].counter == p.first.fanInDegree) {
            m_agtr[index].isAggregated = true;
            PacketInfo param_packet = p.first;
            param_packet.collision = m_agtr[index].isCollision;
            if (param_packet.collision) {
                // Check if new agtr is already hashed
                int new_hash_agtr;
                if (next_agtr_index[param_packet.aggregatorIndex] == -1) {
                    new_hash_agtr = hash_table->HashNew_predefine();
                    if (new_hash_agtr != -1) {
                        next_agtr_index[param_packet.aggregatorIndex] = new_hash_agtr;
                        hash_table->hash_map[param_packet.aggregatorIndex] = new_hash_agtr;
                    } else {
                        new_hash_agtr = param_packet.aggregatorIndex;
                    }
                } else {
                    new_hash_agtr = next_agtr_index[param_packet.aggregatorIndex];
                }
                param_packet.len_tensor = (uint32_t) new_hash_agtr;
            }
            param_packet.bitmap = m_agtr[index].bitmap;
            param_packet.ecn = m_agtr[index].ecn;
            param_packet.isAcked = true;
            TxQueue.push(std::make_pair(param_packet, p.second));
            to_be_sent ++;
        }
        RxQueue.pop();
    }
    if (to_be_sent > 0) {
        while (!TxQueue.empty()) {
            std::pair<PacketInfo, Address> p = TxQueue.front();
            SendParameterPacket(p.first, p.second);
            TxQueue.pop();
        }
        NS_LOG_INFO("At time " << Simulator::Now().GetNanoSeconds() << " ns server sent " << to_be_sent << " packets");
    }


    if (Simulator::Now().GetSeconds() - running_time > 0.001) {
       m_mainEvent.Cancel();
       return;
    } else {
        m_mainEvent = Simulator::Schedule(NanoSeconds(1.0), &AtpServer::main_loop, this);
    }
}

void
AtpServer::SendParameterPacket(PacketInfo &packetInfo, Address to) {
    Ptr<Packet> packet = Create<Packet>(P4ML_DATA_SIZE);
    AtpHeader atpHeader;
    atpHeader.FillHeader(
        packetInfo.bitmap,
        packetInfo.fanInDegree,
        packetInfo.overflow,
        packetInfo.resend,
        packetInfo.collision,
        packetInfo.ecn,
        packetInfo.isAcked,
        packetInfo.aggregatorIndex,
        packetInfo.appID,
        packetInfo.seqNum,
        packetInfo.key,
        packetInfo.len_tensor
        );
    packet->AddHeader(atpHeader);
    m_socket->SendTo(packet, 0, to);
    m_sent++;
}



bool 
AtpServer::isCollision(uint32_t index, PacketInfo* packetInfo) 
{
    if (m_agtr[index].isExpired) {
        return false;
    }

    if (m_agtr[index].timestamp + PS_BUFFER_TIMEOUT >= Simulator::Now().GetSeconds()) {
        return true;
    }

    if (m_agtr[index].isAggregated && (m_agtr[index].appID != packetInfo->appID || m_agtr[index].seqNum != packetInfo->seqNum || m_agtr[index].key != packetInfo->key)) {
        ResetAggregator(index);
        return false;
    }

    if (!m_agtr[index].isAggregated && (m_agtr[index].appID != packetInfo->appID || m_agtr[index].seqNum != packetInfo->seqNum || m_agtr[index].key != packetInfo->key)) {
        std::cout << "WARNNING: Unaggregated ATP packet is expired, seqNum: " << m_agtr[index].seqNum << " appID: " << m_agtr[index].appID << std::endl;
        return true;
    }
    if (m_agtr[index].isAggregated) {
        ResetAggregator(index);
        return false;
    }
    return true;
}

} // namespace ns3
 
