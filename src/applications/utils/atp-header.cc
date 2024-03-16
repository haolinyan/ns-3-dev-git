#include "atp-header.h"
#include <bitset>
#include "ns3/address-utils.h"

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("AtpHeader");
NS_OBJECT_ENSURE_REGISTERED(AtpHeader);

TypeId
AtpHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::AtpHeader")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<AtpHeader>();
    return tid;
}

TypeId
AtpHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}


AtpHeader::AtpHeader()
    : m_bitmap(0),
      m_fanInDegree(0),
      m_overflow(false),
      m_resend(false),
      m_collision(false),
      m_ecn(false),
      m_isAck(false),
      m_aggregatorIndex(0),
      m_jobId(0),
      m_seqNum(0)
{
}

void 
AtpHeader::setAck(bool isAck) {
    m_isAck = isAck;
}

void 
AtpHeader::FillHeader(uint32_t bitmap, uint8_t fanInDegree, bool overflow, bool resend, bool collision, bool ecn, bool isAck, uint16_t aggregatorIndex, uint32_t jobId, uint32_t seqNum)
{
    m_bitmap = bitmap;
    m_fanInDegree = fanInDegree;
    m_overflow = overflow;
    m_resend = resend;
    m_collision = collision;
    m_ecn = ecn;
    m_isAck = isAck;
    m_aggregatorIndex = aggregatorIndex;
    m_jobId = jobId;
    m_seqNum = seqNum;
}

void
AtpHeader::Print(std::ostream& os) const
{
    os << "ATP Header: ";
    os << "bitmap=" << std::bitset<32>(m_bitmap) << std::endl;
    os << "fanInDegree=" << static_cast<int>(m_fanInDegree) << std::endl;
    os << "overflow=" << m_overflow << std::endl;
    os << "resend=" << m_resend << std::endl;
    os << "collision=" << m_collision << std::endl;
    os << "ecn=" << m_ecn << std::endl;
    os << "isAck=" << m_isAck << std::endl;
    os << "aggregatorIndex=" << m_aggregatorIndex << std::endl;
    os << "jobId=" << m_jobId << std::endl;
    os << "seqNum=" << m_seqNum << std::endl;
}

uint32_t
AtpHeader::GetSerializedSize() const
{
    return 12;
}

void
AtpHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    // bitmap 32bits
    i.WriteHtonU32(m_bitmap); 
    // fanInDegree 5bits
    i.WriteU8(m_fanInDegree);
    /*
        bool overflow 1bit
        bool resend 1bit
        bool collision 1bit
        bool ecn 1bit
        bool isAck 1bit
    */
    uint8_t flags = 0;
    flags |= (m_overflow << 7);
    flags |= (m_resend << 6);
    flags |= (m_collision << 5);
    flags |= (m_ecn << 4);
    flags |= (m_isAck << 3);
    i.WriteU8(flags);
    i.WriteHtonU16(m_aggregatorIndex);
    i.WriteHtonU16(m_jobId);
    i.WriteHtonU16(m_seqNum);
}

uint32_t
AtpHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    m_bitmap = i.ReadNtohU32();
    m_fanInDegree = i.ReadU8();
    uint8_t flags = i.ReadU8();
    m_overflow = (flags >> 7) & 0x01;
    m_resend = (flags >> 6) & 0x01;
    m_collision = (flags >> 5) & 0x01;
    m_ecn = (flags >> 4) & 0x01;
    m_isAck = (flags >> 3) & 0x01;
    m_aggregatorIndex = i.ReadNtohU16();
    m_jobId = i.ReadNtohU16();
    m_seqNum = i.ReadNtohU16();
    return GetSerializedSize();
}


uint32_t
AtpHeader::GetBitmap() const
{
    return m_bitmap;
}

uint8_t
AtpHeader::GetFanInDegree() const
{
    return m_fanInDegree;
}

bool
AtpHeader::GetOverflow() const
{
    return m_overflow;
}

bool
AtpHeader::GetResend() const
{
    return m_resend;
}

bool
AtpHeader::GetCollision() const
{
    return m_collision;
}

bool
AtpHeader::GetEcn() const
{
    return m_ecn;
}

bool
AtpHeader::GetIsAck() const
{
    return m_isAck;
}

uint16_t
AtpHeader::GetAggregatorIndex() const
{
    return m_aggregatorIndex;
}

uint32_t
AtpHeader::GetJobId() const
{
    return m_jobId;
}

uint32_t
AtpHeader::GetSeqNum() const
{
    return m_seqNum;
}

}
