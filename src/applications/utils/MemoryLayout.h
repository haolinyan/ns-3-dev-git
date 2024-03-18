#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H
#include <iostream>
#include "ns3/nstime.h"
#include "ns3/atp-header.h"
#include "ATPCommon.h"
#include "ns3/simulator.h"
#include "ns3/HashTable.h"
#define ATP_TIMEOUT 0.0005
#define PS_BUFFER_SIZE 65536
using namespace ns3;
struct Aggregator {
    uint32_t bitmap;
    uint8_t counter;
    bool ecn;
    uint32_t jobId;
    uint32_t seqNum;
    double timestamp;
    bool isExpired;
    bool isAggregated;
};

class ParameterServer {
public:
    ParameterServer() {}
    void Setup(uint32_t m_max_agtr_size, uint16_t m_appId) {
        m_agtr = new Aggregator[PS_BUFFER_SIZE];
        for (size_t i = 0; i < PS_BUFFER_SIZE; i++)
        {
            ResetAggregator(i);
        }
        
    }
    ~ParameterServer() {
        delete[] m_agtr;
    }

    void ResetAggregator(uint16_t index) {
        m_agtr[index].bitmap = 0;
        m_agtr[index].counter = 0;
        m_agtr[index].ecn = false;
        m_agtr[index].jobId = 0;
        m_agtr[index].seqNum = 0;
        m_agtr[index].timestamp = 0.;
        m_agtr[index].isExpired = true;
        m_agtr[index].isAggregated = false;
    }

    bool isCollision(uint32_t index, AtpHeader &atpHeader) {
        if (m_agtr[index].isExpired) {
            return false;
        }

        if (m_agtr[index].timestamp + ATP_TIMEOUT >= Simulator::Now().GetSeconds()) {
            return true;
        }

        if (m_agtr[index].isAggregated && (m_agtr[index].jobId != atpHeader.GetJobId() || m_agtr[index].seqNum != atpHeader.GetSeqNum())) {
            ResetAggregator(index);
            return false;
        }

        if (!m_agtr[index].isAggregated && (m_agtr[index].jobId != atpHeader.GetJobId() || m_agtr[index].seqNum != atpHeader.GetSeqNum())) {
            std::cout << "WARNNING: Unaggregated ATP packet is expired, seqNum: " << m_agtr[index].seqNum << " jobId: " << m_agtr[index].jobId << std::endl;
            return true;
        }
        if (m_agtr[index].isAggregated) {
            std::cout << "WARNNING: Received a packet for the same task, but the task has expired, either because the ATP_TIMEOUT is too small or the duplicated SeqNum loop!" << std::endl;
            std::cout << m_agtr[index].seqNum << " jobId: " << m_agtr[index].jobId << std::endl;
            ResetAggregator(index);
            return false;
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

    int RecvAtpHeader(AtpHeader &atpHeader) {
        uint32_t index = atpHeader.GetSeqNum() % PS_BUFFER_SIZE;
        if (isCollision(index, atpHeader)) {
            if (m_agtr[index].jobId != atpHeader.GetJobId() || m_agtr[index].seqNum != atpHeader.GetSeqNum()) {
                return -1;
            }
        }
        Aggregator agtr = m_agtr[index];
        if (agtr.counter == atpHeader.GetFanInDegree()) {
            m_agtr[index].timestamp = Simulator::Now().GetSeconds();
            m_agtr[index].isAggregated = true;
            WriteAtpHeader(atpHeader, agtr);
            return 1;
        }
        if ((agtr.bitmap & atpHeader.GetBitmap()) == 1) {
            return 2;
        }
        m_agtr[index].bitmap |= atpHeader.GetBitmap();
        m_agtr[index].counter = calculateBitSum(m_agtr[index].bitmap);
        m_agtr[index].ecn |= atpHeader.GetEcn();
        m_agtr[index].jobId = atpHeader.GetJobId();
        m_agtr[index].seqNum = atpHeader.GetSeqNum();
        m_agtr[index].timestamp = Simulator::Now().GetSeconds();
        m_agtr[index].isExpired = false;
        if (m_agtr[index].counter == atpHeader.GetFanInDegree()) {
            m_agtr[index].isAggregated = true;
            WriteAtpHeader(atpHeader, agtr);
            return 3;
        }
        return 4;
        }
    void WriteAtpHeader(AtpHeader &atpHeader, Aggregator &agtr) {
        NS_ASSERT_MSG(atpHeader.GetIsAck() == false, "AtpHeader is not an ACK");
        atpHeader.setAck(true);
        atpHeader.SetBitmap(agtr.bitmap);
    }
private:
    Aggregator* m_agtr;
};

#endif // MEMORY_LAYOUT_H