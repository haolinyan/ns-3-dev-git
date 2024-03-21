#ifndef ATP_HEADER_H
#define ATP_HEADER_H
#include "ns3/header.h"
#include <stdint.h>

namespace ns3
{
class AtpHeader : public Header
{
    public:
        static TypeId GetTypeId();
        AtpHeader();
        TypeId GetInstanceTypeId() const override;
        void Print(std::ostream& os) const override;
        void FillHeader(uint32_t bitmap, 
                        uint8_t fanInDegree, 
                        bool overflow, 
                        bool resend, 
                        bool collision, 
                        bool ecn, 
                        bool isAck, 
                        uint16_t aggregatorIndex, 
                        uint16_t appID,
                        uint16_t seqNum,
                        uint64_t key,
                        uint32_t len_tensor);

        uint32_t GetBitmap() const;
        uint8_t GetFanInDegree() const;
        bool GetOverflow() const;
        bool GetResend() const;
        bool GetCollision() const;
        bool GetEcn() const;
        bool GetIsAck() const;
        uint16_t GetAggregatorIndex() const;
        uint16_t GetSeqNum() const;
        uint16_t GetAppID() const;
        uint64_t GetKey() const;
        uint32_t GetLenTensor() const;
        void SetAck(bool isAck);
        void SetBitmap(uint32_t bitmap);
        void SetCollision(bool state);
        void SetEcn(bool state);
        void SetResend(bool state);

        
        uint32_t GetSerializedSize() const override;
        void Serialize(Buffer::Iterator start) const override;
        uint32_t Deserialize(Buffer::Iterator start) override;
    
    private:
        uint32_t m_bitmap;
        uint8_t m_fanInDegree;
        // flags
        bool m_overflow;
        bool m_resend;
        bool m_collision; // dynamic hash scheme
        bool m_ecn; // CC
        bool m_isAck;
        uint16_t m_appID;
        uint16_t m_seqNum;
        uint16_t m_aggregatorIndex;
        uint64_t m_key;
        uint32_t m_len_tensor; 
        
};
}
#endif /* ATP_HEADER */