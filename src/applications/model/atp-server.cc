#include "atp-server.h"
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
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AtpServerApplication");

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
            .AddAttribute("maxAgtrSize",
                          "Maximum AGTR size",
                          UintegerValue(65536),
                          MakeUintegerAccessor(&AtpServer::m_max_agtr_size),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("appId",
                            "Application ID",
                            UintegerValue(0),
                            MakeUintegerAccessor(&AtpServer::m_appId),
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
AtpServer::StartApplication()
{
    NS_LOG_FUNCTION(this);
    m_ps.Setup(m_max_agtr_size, m_appId);
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
        int flag = m_ps.RecvAtpHeader(atpHeader);
        if (flag == -1) {
            NS_LOG_UNCOND("[Error] Hash collision detected in the PS"<< " seqNum= " << atpHeader.GetSeqNum());
            return;
        } else if (flag == 1) {
            NS_LOG_INFO("[RT]At time " << Simulator::Now().As(Time::S) << " Resent the gradient packet"<< " seqNum= " << atpHeader.GetSeqNum());
        } else if (flag == 2) {
            NS_LOG_INFO("[Dup]At time " << Simulator::Now().As(Time::S) << " Drop the redundant packet" << " seqNum= " << atpHeader.GetSeqNum());
            return;
        } else if (flag == 3) {
            NS_LOG_DEBUG("At time " << Simulator::Now().As(Time::S) << " Sent the parameter packet" << " seqNum= " << atpHeader.GetSeqNum());
        } else if (flag == 4) {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " Received a grad packet" << " seqNum= " << atpHeader.GetSeqNum());
            return;
        }

        packet->AddHeader(atpHeader);

        socket->SendTo(packet, 0, from);

        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_DEBUG("At time " << Simulator::Now().As(Time::S) << " server sent "
                                   << packet->GetSize() << " bytes to "
                                   << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(from).GetPort());
        }
    }
}

} 
