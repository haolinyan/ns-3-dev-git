#include "atp-client-app.h"
#include "ns3/atp-header.h"
#include <cstdlib>
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("AtpClient");

NS_OBJECT_ENSURE_REGISTERED(AtpClient);

TypeId AtpClient::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AtpClient")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<AtpClient>()
                            .AddAttribute("Port",
                                          "The port number of the server",
                                          UintegerValue(9),
                                          MakeUintegerAccessor(&AtpClient::m_port),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("RemoteAddress",
                                            "The destination Address of the outbound packets",
                                            AddressValue(),
                                            MakeAddressAccessor(&AtpClient::m_peerAddress),
                                            MakeAddressChecker())
                            .AddAttribute("Host",
                                            "The host number of the server",
                                            UintegerValue(0),
                                            MakeUintegerAccessor(&AtpClient::m_host),
                                            MakeUintegerChecker<uint32_t>())
                            .AddAttribute("NumWorker",
                                            "The number of workers",
                                            UintegerValue(0),
                                            MakeUintegerAccessor(&AtpClient::m_num_worker),
                                            MakeUintegerChecker<uint32_t>())
                            .AddAttribute("AppID",
                                            "The application ID",
                                            UintegerValue(0),
                                            MakeUintegerAccessor(&AtpClient::m_appID),
                                            MakeUintegerChecker<uint16_t>())
                            .AddAttribute("NumPS",
                                            "The number of parameter servers",
                                            UintegerValue(0),
                                            MakeUintegerAccessor(&AtpClient::m_num_PS),
                                            MakeUintegerChecker<uint32_t>())
                            .AddAttribute("Key",
                                            "The key of the tensor",
                                            UintegerValue(0),
                                            MakeUintegerAccessor(&AtpClient::m_key),
                                            MakeUintegerChecker<uint64_t>())
                            .AddTraceSource("WindowSizeTrace",
                                            "Window size to trace.",
                                            MakeTraceSourceAccessor(&AtpClient::m_windowSizeTrace),
                                            "ns3::AtpClient::ValueTracedCallback")
                            .AddTraceSource("AggThroughputTrace",
                                          "Aggregator throughput to trace.",
                                          MakeTraceSourceAccessor(&AtpClient::m_agg_thoughtput),
                                          "ns3::AtpClient::AggThroughputTracedCallback")
                            .AddAttribute("TensorSize",
                                            "The size of the tensor",
                                            UintegerValue(0),
                                            MakeUintegerAccessor(&AtpClient::m_tensor_size),
                                            MakeUintegerChecker<uint64_t>());
    return tid;

}

AtpClient::AtpClient()
{
    NS_LOG_FUNCTION(this);
    window_manager = new WindowManager();
}


void
AtpClient::Stats() {
    double RX = (double) ((m_received * P4ML_DATA_SIZE * 8) / 1e+9) / TimeInterval;
    double TX = (double) ((m_sent * P4ML_DATA_SIZE * 8) / 1e+9) / TimeInterval;
    m_received = 0;
    m_sent = 0;
    m_agg_thoughtput(TX, RX);
    m_statsEvent = Simulator::Schedule(Seconds(TimeInterval), &AtpClient::Stats, this);
}


AtpClient::~AtpClient()
{
    NS_LOG_FUNCTION(this);
}

void
AtpClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    hash_table = new HashTable(UsedSwitchAGTRcount);
    // set initial window size as max_agtr_size_per_thread
    // TODO:: set the initial window size as new variable name
    cc_manager.SetCwnd(max_agtr_size_per_thread); 
    send_pointer = max_agtr_size_per_thread;
    finish_window_seq = max_agtr_size_per_thread;
    window = max_agtr_size_per_thread;
    m_windowSizeTrace((uint32_t) window, false);

    for (int i = 0; i < max_agtr_size_per_thread; i++)  {
        hash_table->HashNew_crc(m_appID, i); 
    }

    if (!m_socket) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_port));
        } else if (InetSocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        } else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
    }
    m_socket->SetRecvCallback(MakeCallback(&AtpClient::RecvPacket, this));
    m_sendEvent = Simulator::Schedule(Seconds(0.0), &AtpClient::ScheduleTx, this);
    m_statsEvent = Simulator::Schedule(Seconds(TimeInterval), &AtpClient::Stats, this);
}

void 
AtpClient::RecvPacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    Address to;
    Address localAddress;
    AtpHeader atpHeader;
    PacketInfo packetInfo;
    while ((packet = socket->RecvFrom(from))) {
        socket->GetSockName(localAddress);
       
        packet->RemoveHeader(atpHeader);
        
        packetInfo.bitmap = atpHeader.GetBitmap();
        packetInfo.fanInDegree = atpHeader.GetFanInDegree();
        packetInfo.overflow = atpHeader.GetOverflow();
        packetInfo.resend = atpHeader.GetResend();
        packetInfo.collision = atpHeader.GetCollision();
        packetInfo.ecn = atpHeader.GetEcn();
        packetInfo.isAcked = atpHeader.GetIsAck();
        packetInfo.aggregatorIndex = atpHeader.GetAggregatorIndex();
        packetInfo.appID = atpHeader.GetAppID();
        packetInfo.seqNum = atpHeader.GetSeqNum();
        packetInfo.key = atpHeader.GetKey();
        packetInfo.len_tensor = atpHeader.GetLenTensor();
        RxQueue.push(packetInfo);
    }
    m_received ++;
    NS_LOG_DEBUG("At time " << Simulator::Now().GetNanoSeconds() << " ns client received, seqNum= " << packetInfo.seqNum);
}

void
AtpClient::ScheduleTx()
{
    int total_packet = ceil((float)m_tensor_size / MAX_ENTRIES_PER_PACKET);
    window_manager->Reset(total_packet);
    int num_first_time_sending;
    if (max_agtr_size_per_thread * MAX_ENTRIES_PER_PACKET > m_tensor_size)
        num_first_time_sending = ceil((float)m_tensor_size / MAX_ENTRIES_PER_PACKET);
    else
        num_first_time_sending = max_agtr_size_per_thread;
    if(m_host == 0) NS_LOG_UNCOND("Total packet: " << total_packet << " num_first_time_sending: " << num_first_time_sending);
    for (int i = 0; i < num_first_time_sending; i++) {
        seqNum ++;
        uint16_t switch_agtr_pos = hash_table->hash_map[i];
        if ((int) seqNum.GetValue() <= total_packet) {
            PacketInfo packetInfo;
            packetInfo.bitmap = (1 << m_host);
            packetInfo.fanInDegree = m_num_worker;
            packetInfo.overflow = false;
            packetInfo.resend = false;
            packetInfo.collision = false;
            packetInfo.ecn = false;
            packetInfo.isAcked = false;
            packetInfo.appID = m_appID;
            packetInfo.seqNum = (uint16_t) seqNum.GetValue();
            packetInfo.aggregatorIndex = switch_agtr_pos;
            packetInfo.key = m_key;
            packetInfo.len_tensor = m_tensor_size;
            TxQueue.push(packetInfo); 
        }
    }
    SendPakcet(num_first_time_sending);
    m_timestamp = Simulator::Now().GetNanoSeconds();
    m_loopEvent = Simulator::Schedule(NanoSeconds(1), &AtpClient::main_receive_packet_loop, this);
}


void 
AtpClient::main_receive_packet_loop()
{
    double current_time = Simulator::Now().GetNanoSeconds();
    if(RxQueue.size() == 0) {
        if (current_time - m_timestamp > m_timeout * 1e+9) {
            // Trigger the recovery mechanism
            uint16_t timeout_end_seq;
            if (!PendingQueue.empty()) {
                PacketInfo packetInfo_ = PendingQueue.front(); 
                timeout_end_seq = packetInfo_.seqNum;
            } else {
                timeout_end_seq = window_manager->last_ACK + 1; // expected seqNum
                timeout_end_seq = timeout_end_seq + 1;
            }
            int resent_to_be_sent = 0;
            NS_LOG_INFO("[Timeout] At time " << Simulator::Now().GetNanoSeconds() << " ns client timeout, resent from " <<  window_manager->last_ACK + 1 << " to "
            << timeout_end_seq - 1 << " last_ACK: " << window_manager->last_ACK);

            for (uint16_t timeout_seq = window_manager->last_ACK + 1; timeout_seq < timeout_end_seq; timeout_seq++) {
                uint16_t switch_agtr_pos = hash_table->hash_map[(timeout_seq - 1) % max_agtr_size_per_thread];
                PacketInfo packetInfo_resend;
                packetInfo_resend.bitmap = (1 << m_host);
                packetInfo_resend.fanInDegree = m_num_worker;
                packetInfo_resend.overflow = false;
                packetInfo_resend.resend = true;
                packetInfo_resend.collision = false;
                packetInfo_resend.ecn = false;
                packetInfo_resend.isAcked = false;
                packetInfo_resend.appID = m_appID;
                packetInfo_resend.seqNum = timeout_seq;
                packetInfo_resend.aggregatorIndex = switch_agtr_pos;
                packetInfo_resend.key = m_key;
                packetInfo_resend.len_tensor = m_tensor_size;
                TxQueue.push(packetInfo_resend);
                resent_to_be_sent++;
            }
            SendPakcet(resent_to_be_sent);
            m_timestamp = current_time;
        }
        m_loopEvent = Simulator::Schedule(NanoSeconds(TIME_INTERVAL), &AtpClient::main_receive_packet_loop, this);
        return;
    } else {
        m_timeout = 0.0002;
        int to_be_sent = 0;
        int to_be_received = RxQueue.size();
        for (int i = 0; i < to_be_received; i++) {
            PacketInfo packetInfo = RxQueue.front();
            bool is_resend_packet = packetInfo.resend;
            bool is_ecn_mark_packet = packetInfo.ecn;
            bool is_collision_packet = packetInfo.collision;
            // If that is resend packet from last tensor, ignore it
            if (packetInfo.key != m_key) {
                RxQueue.pop();
                continue;
            }
            // If that is duplicate resend packet, ignore it
            if (is_resend_packet && window_manager->isACKed[packetInfo.seqNum]) {
                total_dup_packet ++;
                RxQueue.pop();
                continue;
            }

            if (!window_manager->isACKed[packetInfo.seqNum]) {
                if (is_collision_packet) {
                    collision_times ++;
                    if (CHANGE_AGTR_ENABLE) {
                        uint16_t new_agtr = (uint16_t) packetInfo.len_tensor;
                        if (!hash_table->isAlreadyDeclare[new_agtr]) {
                            int hash_table_index = (packetInfo.seqNum - 1) % max_agtr_size_per_thread;
                            hash_table->hash_map[hash_table_index] = new_agtr;
                        }
                    }
                }
                window_manager->UpdateWindow(&packetInfo.seqNum);
                uint16_t next_seq_num = packetInfo.seqNum + window;
                if (next_seq_num > window_manager->last_ACK + window) {
                    PendingQueue.push(packetInfo);
                }
                /* Send Next Packet */
                if (next_seq_num <= window_manager->total_ACK && next_seq_num <= window_manager->last_ACK + window && next_seq_num > send_pointer) {
                    int packet_to_process = next_seq_num - send_pointer;
                    for (int i = packet_to_process - 1; i >= 0; i--) {
                        uint16_t process_next_seq_num = next_seq_num - i;
                        uint16_t switch_agtr_pos = hash_table->hash_map[(process_next_seq_num - 1) % max_agtr_size_per_thread];
                        PacketInfo packetInfo_;
                        packetInfo_.bitmap = (1 << m_host);
                        packetInfo_.fanInDegree = m_num_worker;
                        packetInfo_.overflow = false;
                        packetInfo_.resend = false;
                        packetInfo_.collision = false;
                        packetInfo_.ecn = false;
                        packetInfo_.isAcked = false;
                        packetInfo_.appID = m_appID;
                        packetInfo_.seqNum = process_next_seq_num;
                        packetInfo_.aggregatorIndex = switch_agtr_pos;
                        packetInfo_.key = m_key;
                        packetInfo_.len_tensor = m_tensor_size;
                        TxQueue.push(packetInfo_);
                        to_be_sent ++;
                    }
                    send_pointer = next_seq_num;
                }
                
                /* Check If packet in Pending Queue is Ready to send */
                while(!PendingQueue.empty()) {
                    PacketInfo packetInfo_ = PendingQueue.front();
                    if (window_manager->last_ACK < packetInfo_.seqNum) {
                        break;
                    }
                    uint16_t next_seq_num = packetInfo_.seqNum + window;
                    if (next_seq_num <= window_manager->last_ACK + window && next_seq_num > send_pointer) {
                        if (next_seq_num <= window_manager->total_ACK) {
                            int packet_to_process = std::abs(next_seq_num - (uint16_t) send_pointer);
                            for (int i = packet_to_process - 1; i >= 0; i--) {
                                uint16_t process_next_seq_num = next_seq_num - i;
                                uint16_t switch_agtr_pos = hash_table->hash_map[(process_next_seq_num - 1) % max_agtr_size_per_thread];
                                PacketInfo packetInfo_;
                                packetInfo_.bitmap = (1 << m_host);
                                packetInfo_.fanInDegree = m_num_worker;
                                packetInfo_.overflow = false;
                                packetInfo_.resend = false;
                                packetInfo_.collision = false;
                                packetInfo_.ecn = false;
                                packetInfo_.isAcked = false;
                                packetInfo_.appID = m_appID;
                                packetInfo_.seqNum = process_next_seq_num;
                                packetInfo_.aggregatorIndex = switch_agtr_pos;
                                packetInfo_.key = m_key;
                                packetInfo_.len_tensor = m_tensor_size;
                                TxQueue.push(packetInfo_);
                                to_be_sent ++;
                            }
                            send_pointer = next_seq_num;
                        }
                        
                    } 
                    PendingQueue.pop();
                }

                if (CC_ENABLE) {
                    if (packetInfo.seqNum == finish_window_seq) {
                        int new_window = cc_manager.adjustWindow(is_ecn_mark_packet);
                        if (send_pointer + new_window > window_manager->total_ACK)
                            window = window_manager->total_ACK - send_pointer;
                        else
                            window = new_window;
                        m_windowSizeTrace((uint32_t) window, is_ecn_mark_packet);
                        finish_window_seq += window;
                    }
                }


            }
            RxQueue.pop();
        }

        if (to_be_sent > 0) {
            SendPakcet(to_be_sent);
        }
    }


    m_timestamp = current_time;
    if (window_manager->last_ACK < window_manager->total_ACK) {
        m_loopEvent = Simulator::Schedule(NanoSeconds(TIME_INTERVAL), &AtpClient::main_receive_packet_loop, this);
    } else {
        NS_LOG_INFO("At time " << Simulator::Now().GetNanoSeconds() << " ns client " << m_host <<  " has received all parameter packets");
        StopApplication();
    }
}


void 
AtpClient::SendPakcet(int num_packet)
{
    NS_ASSERT_MSG(m_socket, "m_socket is null");
    for (int i = 0; i < num_packet; i++) {
        PacketInfo packetInfo = TxQueue.front();
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
        Ptr<Packet> p = Create<Packet>(P4ML_DATA_SIZE);
        p->AddHeader(atpHeader);
        m_socket->Send(p);
        TxQueue.pop();
        m_sent ++;
    }
    NS_LOG_INFO("At time " << Simulator::Now().GetNanoSeconds() << " ns client sent " << num_packet << " packets");
}

void
AtpClient::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }
    if (m_socket)
    {
        m_socket->Close();
    }
    delete hash_table;
    delete window_manager;
    m_loopEvent.Cancel();
    m_statsEvent.Cancel();
    Report();
}

void 
AtpClient::Report()
{
    NS_LOG_INFO("*** Report for Client " << m_host << " ***");
    NS_LOG_INFO("Host: " << m_host);
    NS_LOG_INFO("Total duplicate packet: " << total_dup_packet);
    NS_LOG_INFO("Total collision times: " << collision_times);
}
}




