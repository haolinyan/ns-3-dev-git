#ifndef SLIDING_W_H
#define SLIDING_W_H
#include <iostream>
#include <deque>
#include "ns3/sequence-number.h"
#include "ns3/ipv4-address.h"
#include "ns3/atp-header.h"
namespace ns3 {
struct PacketBuffer {
    bool isAcked;
    uint32_t jobId;
    SequenceNumber16 seqNum;
    uint32_t bitmap;
    uint16_t aggregatorIndex;
    uint8_t fanInDegree;
    bool overflow;
    bool resend;
    bool collision;
    bool ecn;
    Address from;
    Address to;
    int retransmation; // retransmation count
};

class WindowManager {
    public:
        WindowManager() {}

        // Add a packet to the transmission buffer
        void AddToTxBuffer(PacketBuffer &packet) {
            NS_ASSERT_MSG(packet.isAcked == false, "packet.isAck must be false");
            if (TxRxBuffer.size() !=0 ) NS_ASSERT_MSG(packet.seqNum > TxRxBuffer.back().seqNum, "packet.seqNum must be greater than the last packet in the buffer");
            TxRxBuffer.push_back(packet);
        }

        // Recv a seqNum and return the type of the seqNum
        int RecvAck(SequenceNumber16 seqNum) {
            NS_ASSERT_MSG(!TxRxBuffer.empty(), "TxRxBuffer must not be empty");
            int deqNum = 0;
            if (seqNum < TxRxBuffer.front().seqNum) {
                return -1; // duplicate ack
            } else if (seqNum == TxRxBuffer.front().seqNum) {
                TxRxBuffer.pop_front();
                m_windowShift ++;
                deqNum ++;
                while (!TxRxBuffer.empty() && TxRxBuffer.front().isAcked) {
                    TxRxBuffer.pop_front();
                    m_windowShift ++;
                    deqNum ++;
                }
                return deqNum;  
            } else {
                uint16_t index = seqNum.GetValue() - TxRxBuffer.front().seqNum.GetValue();
                NS_ASSERT_MSG(index < TxRxBuffer.size(), "index must be less than the size of the buffer");
                TxRxBuffer[index].isAcked = true;
                consecutiveOod ++;
                return deqNum;
            }
        }

        // Get the indices of the packets that need to be retransmitted for the timeout event
        int GetTimeOutPkt(int start_pos, int end_pos, int* timeout_pkt) {
            int count = 0;
            start_pos -= m_windowShift;
            end_pos -= m_windowShift;
            if (end_pos < 0) {
                return 0;
            } else if (start_pos < 0) {
                start_pos = 0;
            } 
            for (int i = start_pos; i <= end_pos; i++) {
                if (TxRxBuffer[i].isAcked == false) {
                    timeout_pkt[count] = i;
                    count ++;
                }
            }
            return count;
        }

        // for fast retransmission
        uint8_t GetConsecutiveOod() {
            return consecutiveOod;
        }
        // set consecutiveOod to 0
        void ResetConsecutiveOod() {
            consecutiveOod = 0;
        }

        SequenceNumber16 GetNextSeq() {
            return seqNum++;
        }

        uint64_t GetWindowShift() {
            return m_windowShift;
        }

        std::deque<PacketBuffer> TxRxBuffer;

    private:
        uint64_t m_windowShift{0}; // window shift count
        uint8_t consecutiveOod{0}; // consecutive out of order count
        SequenceNumber16 seqNum{0};
};




}
#endif