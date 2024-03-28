#ifndef ATP_CLIENT_H
#define ATP_CLIENT_H
#include <ns3/core-module.h>
#include <ns3/node.h>
#include <queue>
#include "Common.h"
#include "CC_manager.h"
#include "HashTable.h"
#include "WindowManager.h"
#include "ns3/sequence-number.h"
#include "ns3/traced-value.h"
#include "ns3/simulator.h"
#include "atp-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/atp-tag.h"
namespace ns3 {
class AtpClient : public Object {
public:
    static TypeId GetTypeId (void) {
        static TypeId tid = TypeId ("ns3::AtpClient")
            .SetParent<Object> ()
            .AddConstructor<AtpClient>()
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
            .AddAttribute("Host",
                            "The host number of the server",
                            UintegerValue(0),
                            MakeUintegerAccessor(&AtpClient::m_host),
                            MakeUintegerChecker<uint32_t>())
            .AddAttribute("NumPS",
                            "The number of parameter servers",
                            UintegerValue(0),
                            MakeUintegerAccessor(&AtpClient::m_num_PS),
                            MakeUintegerChecker<uint32_t>())
            .AddAttribute("BaseRTT",
                            "The base RTT",
                            UintegerValue(1048),
                            MakeUintegerAccessor(&AtpClient::m_base_rtt),
                            MakeUintegerChecker<uint32_t>())
            .AddAttribute("Key",
                            "The key of the tensor",
                            UintegerValue(0),
                            MakeUintegerAccessor(&AtpClient::m_key),
                            MakeUintegerChecker<uint64_t>());
        return tid;
    }
    AtpClient() {}

    void SetNode(Ptr<Node> node) {
        m_node = node;
    }
    void ReceivePacket(Ptr<Packet> packet) {
        m_timestamp = Simulator::Now().GetNanoSeconds();    
        AtpHeader atpHeader;
        Ipv4Header ipv4Header;
        AtpTag tag;
        packet->RemoveHeader(ipv4Header);
        packet->RemoveHeader(atpHeader);
        packet->RemovePacketTag(tag);
        // NS_LOG_UNCOND(CLI << "::Recv At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
        m_timeout = (int64_t) m_base_rtt * 2;
        int to_be_sent = 0;
        bool is_resend_packet = atpHeader.GetResend();
        bool is_ecn_mark_packet = atpHeader.GetEcn();
        bool is_collision_packet = atpHeader.GetCollision();
        uint16_t seqNum = atpHeader.GetSeqNum();
        totalRx ++;
        if (atpHeader.GetKey() != m_key) {
            return;
        }

        if (is_resend_packet && window_manager->isACKed[seqNum]) {
            NS_LOG_UNCOND(CLI << "[Dup] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
            return;
        }

        if (!window_manager->isACKed[seqNum]) {
            if (is_collision_packet) {
                    collision_times ++;
                    if (CHANGE_AGTR_ENABLE) {
                        uint16_t new_agtr = (uint16_t) atpHeader.GetLenTensor();
                        if (!hash_table->isAlreadyDeclare[new_agtr]) {
                            int hash_table_index = (seqNum - 1) % max_agtr_size_per_thread;
                            hash_table->hash_map[hash_table_index] = new_agtr;
                        }
                    }
            }
            window_manager->UpdateWindow(&seqNum);
            uint16_t next_seq_num = seqNum + window;
            if (next_seq_num > window_manager->last_ACK + window) {
                PendingQueue.push(atpHeader);
                NS_LOG_UNCOND(CLI << "[Ood] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum()
                << " last_ACK: " << window_manager->last_ACK);
            } else {
                NS_LOG_UNCOND(CLI << "[Recv] At time " << Simulator::Now().GetNanoSeconds() << " ns Node " << m_node->GetId() << " <- " << (int) tag.GetSendNode() << " seq " << atpHeader.GetSeqNum());
            }
            /* Send Next Packet */
            if (next_seq_num <= window_manager->total_ACK && next_seq_num <= window_manager->last_ACK + window && next_seq_num > send_pointer) {
                int packet_to_process = next_seq_num - send_pointer;
                for (int i = packet_to_process - 1; i >= 0; i--) {
                    uint16_t process_next_seq_num = next_seq_num - i;
                    uint16_t switch_agtr_pos = hash_table->hash_map[(process_next_seq_num - 1) % max_agtr_size_per_thread];
                    Ptr<Packet> pkt = GeneratePacket(process_next_seq_num, switch_agtr_pos, false, false, false);
                    m_TxQueue.push(std::make_pair(pkt, m_dip));
                    to_be_sent ++;
                    totalTx ++;
                }
                send_pointer = next_seq_num;   
            }

            /* Check If packet in Pending Queue is Ready to send */
            while(!PendingQueue.empty()) {
                AtpHeader pending_hdr = PendingQueue.front();
                uint16_t seqNum_pending = pending_hdr.GetSeqNum();
                if (window_manager->last_ACK < seqNum_pending) {
                        break;
                }
                
                uint16_t next_seq_num = seqNum_pending + window;
                if (next_seq_num <= window_manager->last_ACK + window && next_seq_num > send_pointer) {
                    if (next_seq_num <= window_manager->total_ACK) {
                        int packet_to_process = std::abs(next_seq_num - (uint16_t) send_pointer);
                        for (int i = packet_to_process - 1; i >= 0; i--) {
                            uint16_t process_next_seq_num = next_seq_num - i;
                            uint16_t switch_agtr_pos = hash_table->hash_map[(process_next_seq_num - 1) % max_agtr_size_per_thread];
                            Ptr<Packet> pkt = GeneratePacket(process_next_seq_num, switch_agtr_pos, false, false, false);
                            m_TxQueue.push(std::make_pair(pkt, m_dip));
                            to_be_sent ++;
                            totalTx ++;
                        }
                    }
                    send_pointer = next_seq_num;
                }
                PendingQueue.pop();
            }

            if (LOSS_RECOVERY_ENABLE) {
                if (!PendingQueue.empty()) {
                    AtpHeader pending_hdr = PendingQueue.front();
                    uint16_t seqNum_front = pending_hdr.GetSeqNum();
                    if (window_manager->last_ACK + 1 > resend_waiting) {
                        for (uint16_t resend_seq = window_manager->last_ACK + 1; resend_seq < seqNum_front; resend_seq++) {
                            uint16_t switch_agtr_pos = hash_table->hash_map[(resend_seq - 1) % max_agtr_size_per_thread];
                            if (resend_seq <= window_manager->total_ACK) {
                                Ptr<Packet> pkt = GeneratePacket(resend_seq, switch_agtr_pos, false, false, true);
                                NS_LOG_UNCOND(CLI << "[Resend] At time " << Simulator::Now().GetNanoSeconds() 
                                << " ns Node " << m_node->GetId() << " has prepared to resend pkt to " << (int) tag.GetSendNode() << " seq " << resend_seq);
                                // direct send in the highest priority
                                m_SendPacketCallback(pkt, m_dip, 1); 
                                total_resend ++;
                                totalTx ++;
                            }
                            resend_waiting = resend_seq;
                        }
                    }
                }
            }

            if (CC_ENABLE) {
                if (seqNum == finish_window_seq) {
                    int new_window = cc_manager.adjustWindow(is_ecn_mark_packet);
                    if (send_pointer + new_window > window_manager->total_ACK)
                        window = window_manager->total_ACK - send_pointer;
                    else
                        window = new_window;
                    finish_window_seq += window;
                }
            }
        }

        if (to_be_sent)
            NS_ASSERT(Dequeue(to_be_sent));

        if (window_manager->last_ACK == window_manager->total_ACK) {
            m_notifyAppFinish();
            timeout_check.Cancel();
            m_CancelNICQueueCallback();
            Report();
        }
    }
    void AddTask(uint64_t size, Ipv4Address _sip, Ipv4Address _dip, Callback<void> notifyAppFinish) {
        ResetClient();
        m_size = size;
        m_sip = _sip;
        m_dip = _dip;
        m_notifyAppFinish = notifyAppFinish;
        int total_packet = ceil((float) m_size / MAX_ENTRIES_PER_PACKET);
        window_manager->Reset(total_packet);
        int num_first_time_sending;
        if (max_agtr_size_per_thread * MAX_ENTRIES_PER_PACKET > m_size)
            num_first_time_sending = ceil((float) m_size / MAX_ENTRIES_PER_PACKET);
        else
            num_first_time_sending = max_agtr_size_per_thread;
        if(m_host == 0) NS_LOG_UNCOND("Total packet: " << total_packet << " num_first_time_sending: " << num_first_time_sending);

       
        for (int i = 0; i < num_first_time_sending; i++) {
            seqNum ++;
            uint16_t switch_agtr_pos = hash_table->hash_map[i];
            if ((int) seqNum.GetValue() <= total_packet) {
                Ptr<Packet> pkt = GeneratePacket(seqNum.GetValue(), switch_agtr_pos, false, false, false);
                m_TxQueue.push(std::make_pair(pkt, m_dip));
                totalTx ++;
            }   
        }
        m_timestamp = Simulator::Now().GetNanoSeconds();
        Dequeue(num_first_time_sending);
        timeout_check = Simulator::Schedule(NanoSeconds(m_timeout), &AtpClient::CheckTimeout, this);
    }

    void CheckTimeout() {
        int64_t now = Simulator::Now().GetNanoSeconds();
        uint64_t next;
        if (now - m_timestamp >= m_timeout) {
            NS_ASSERT(window_manager->last_ACK < window_manager->total_ACK);
            uint16_t timeout_end_seq;
            if (!PendingQueue.empty()) {
                AtpHeader atpHeader = PendingQueue.front(); 
                timeout_end_seq = atpHeader.GetSeqNum();
            } else {
                timeout_end_seq = window_manager->last_ACK + 1; // expected seqNum
                timeout_end_seq = timeout_end_seq + 1;
            }
            for (uint16_t timeout_seq = window_manager->last_ACK + 1; timeout_seq < timeout_end_seq; timeout_seq++) {
                uint16_t switch_agtr_pos = hash_table->hash_map[(timeout_seq - 1) % max_agtr_size_per_thread];
                Ptr<Packet> pkt = GeneratePacket(timeout_seq, switch_agtr_pos, false, false, true);
                m_SendPacketCallback(pkt, m_dip, 1); 
                totalTx ++;
                total_timeout ++;
                total_resend ++;
            }
            NS_LOG_UNCOND(TIMEOUT << CLI << " At time " << Simulator::Now().GetNanoSeconds() 
            << " ns Node " << m_node->GetId() << " seq " << window_manager->last_ACK + 1 << " to " << timeout_end_seq - 1 << " last_ACK: " << window_manager->last_ACK);
            timeout_check = Simulator::Schedule(NanoSeconds(m_timeout), &AtpClient::CheckTimeout, this);
        } else {
            next = m_timeout - (now - m_timestamp);
            timeout_check = Simulator::Schedule(NanoSeconds(next), &AtpClient::CheckTimeout, this);
        }
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

    Ptr<Packet> GeneratePacket(
        uint16_t seqNum, 
        uint16_t switch_agtr_pos, 
        bool collision,
        bool ecn,
        bool resend) 
    {
        Ptr<Packet> packet = Create<Packet>(P4ML_DATA_SIZE);
        AtpHeader atpHeader;
        atpHeader.FillHeader(
            (1 << m_host), 
            m_num_worker, 
            false, // overflow
            resend, // resend
            collision, // collision
            ecn, // ecn
            false, // isAck
            switch_agtr_pos, 
            m_appID, 
            seqNum, 
            m_key, 
            m_size
        );
        packet->AddHeader(atpHeader);
        Ipv4Header ipv4Header;
        ipv4Header.SetSource(m_sip);
        ipv4Header.SetDestination(m_dip);
        ipv4Header.SetProtocol(18);
        ipv4Header.SetTtl(64);
        packet->AddHeader(ipv4Header);
        AtpTag tag;
        tag.SetSeq(seqNum);
        packet->AddPacketTag(tag);
        return packet;
    }

    void ResetClient() {
        m_size = 0;
        while (!m_TxQueue.empty()) {
            m_TxQueue.pop();
        }

        while (!PendingQueue.empty()) {
            PendingQueue.pop();
        }

        if (hash_table) 
            delete hash_table;
        if (window_manager) 
            delete window_manager;
    
        hash_table = new HashTable(UsedSwitchAGTRcount);
        window_manager = new WindowManager();
        cc_manager.SetCwnd(max_agtr_size_per_thread, max_agtr_size_per_thread); 
        send_pointer = max_agtr_size_per_thread;
        window = max_agtr_size_per_thread;
        finish_window_seq = max_agtr_size_per_thread;
        for (int i = 0; i < max_agtr_size_per_thread; i++)  {
        hash_table->HashNew_crc(m_appID, i); 
        }

        m_timestamp = 0;
        m_timeout = (int64_t) m_base_rtt * 3;
        collision_times = 0;
        resend_waiting = 0;

    }
    
    void Report() {
        NS_LOG_UNCOND(CLI << "Total Tx:" << totalTx << " Total Rx: " << totalRx << " Total resend: " << total_resend << " Total Timeout: " << total_timeout);
    }
    
    SendPacketCallback m_SendPacketCallback;
    CancelNICQueueCallback m_CancelNICQueueCallback;

private:
    Ptr<Node> m_node;
    std::queue<std::pair<Ptr<Packet>, Ipv4Address>> m_TxQueue;
    uint16_t m_appID;
    uint32_t m_num_PS;
    uint32_t m_num_worker;
    uint64_t m_key;
    uint64_t m_size{0};
    Ipv4Address m_sip;
    Ipv4Address m_dip;
    Callback<void> m_notifyAppFinish;
    HashTable* hash_table{nullptr};
    WindowManager* window_manager{nullptr};
    CC_manager cc_manager;
    int UsedSwitchAGTRcount{MAX_AGTR_COUNT};
    int max_agtr_size_per_thread{USE_AGTR_COUNT};

    // for receive/send loop
    int64_t m_timestamp;
    int64_t m_timeout;
    uint64_t m_base_rtt;
    uint64_t send_pointer;
    uint16_t window;
    SequenceNumber16 seqNum{0};
    uint32_t m_host;
    uint32_t collision_times{0};
    std::queue<AtpHeader> PendingQueue;
    int resend_waiting{0};
    uint64_t finish_window_seq;
    EventId timeout_check;

    // metrics
    uint64_t totalTx{0};
    uint64_t totalRx{0};
    uint32_t total_resend{0};
    uint32_t total_timeout{0};
};





}

#endif