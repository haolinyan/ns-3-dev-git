#ifndef ATP_PS_SERVER_H
#define ATP_PS_SERVER_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/ATPCommon.h"
#include "ns3/HashTable.h"
#include <queue>
#include "ns3/p4ml_struct.h"
#include "ns3/atp-header.h"
namespace ns3
{

class Socket;
class Packet;

struct Aggregator {
    uint32_t bitmap;
    uint8_t counter;
    bool ecn;
    uint32_t appID;
    uint32_t seqNum;
    double timestamp;
    bool isExpired;
    bool isAggregated;
    bool isCollision;
    uint64_t key;
};

class AtpServer : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    AtpServer();
    ~AtpServer() override;
    void Stats();
  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;
    void HandleRead(Ptr<Socket> socket);
    void main_loop();
    void ResetAggregator(uint16_t index);
    bool isCollision(uint32_t index, PacketInfo* packetInfo);
    void SendParameterPacket(PacketInfo &packetInfo, Address to);
    uint8_t calculateBitSum(uint32_t data);
    TracedCallback<double, double> m_agg_thoughtput;
    uint16_t m_port;       //!< Port on which we listen for incoming packets.
    uint8_t m_tos;         //!< The packets Type of Service
    Ptr<Socket> m_socket;  //!< IPv4 Socket
    Address m_local;       //!< local multicast address
    uint16_t m_appID; //!< Application ID
    HashTable* hash_table;
    int UsedSwitchAGTRcount{MAX_AGTR_COUNT};
    std::queue<std::pair<PacketInfo, Address>> TxQueue;
    std::queue<std::pair<PacketInfo, Address>> RxQueue;
    uint32_t m_sent{0};
    uint32_t m_received{0};
    EventId m_statsEvent;
    EventId m_mainEvent;
    double TimeInterval{1e-6};
    Aggregator* m_agtr;
    int* next_agtr_index;
    double running_time{0};
};

} // namespace ns3

#endif /* ATP_PS_SERVER_H */
