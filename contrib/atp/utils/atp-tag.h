#ifndef ATP_TAG_H
#define ATP_TAG_H

#include "ns3/tag.h"

namespace ns3
{

class AtpTag : public Tag
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("ns3::AtpTag")
                            .SetParent<Tag>()
                            .AddConstructor<AtpTag>();
        return tid;
    }
    TypeId GetInstanceTypeId() const override {
        return GetTypeId();
    }
    uint32_t GetSerializedSize() const override {
        return 4;
    }
    void Serialize(TagBuffer buf) const override {
        buf.WriteU16(m_seq);
        buf.WriteU8(m_send_node);
        buf.WriteU8(m_recv_node);
    }
    void Deserialize(TagBuffer buf) override {
        m_seq = buf.ReadU16();
        m_send_node = buf.ReadU8();
        m_recv_node = buf.ReadU8();
    }
    void Print(std::ostream& os) const override {
        os << "seq=" << m_seq;
    }
    AtpTag() {}
    AtpTag(uint16_t seq) : m_seq(seq) {}
    void SetSeq(uint16_t seq) {
        m_seq = seq;
    }
    void SetNode(uint8_t send_node, uint8_t recv_node) {
        m_send_node = send_node;
        m_recv_node = recv_node;
    }
    uint8_t GetSendNode() const {
        return m_send_node;
    }
    uint8_t GetRecvNode() const {
        return m_recv_node;
    }
    uint16_t GetSeq() const {
        return m_seq;
    }
private:
    uint16_t m_seq{0};
    uint8_t m_send_node{0};
    uint8_t m_recv_node{0};
};
}




#endif /* ATP_TAG_H */