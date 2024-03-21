#ifndef P4ML_MANAGER_H
#define P4ML_MANAGER_H
#include "ns3/core-module.h"
// #include "ns3/object-factory.h"
#include "ns3/application-container.h"
#include "ns3/p4ml_struct.h"
#include "ns3/ATPCommon.h"
#include "ns3/HashTable.h"
#include "ns3/CC_manager.h"
#include "ns3/WindowManager.h"
// #include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
// #include "ns3/ptr.h"
// #include "ns3/traced-callback.h"
// #include "ns3/socket-factory.h"
#include <queue>
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/sequence-number.h"
#include "ns3/traced-value.h"
#define CHANGE_AGTR_ENABLE true
#define CC_ENABLE true
#define TIME_INTERVAL 1
namespace ns3
{
class AtpClient : public Application
{
public:
    static TypeId GetTypeId();
    AtpClient();
    ~AtpClient() override;
    typedef void (*ValueTracedCallback)(uint32_t, bool);
    typedef void (*AggThroughputTracedCallback)(double, double);
    typedef void (*SendTimeIntervalTracedCallback)(double);

private:
    void StartApplication() override;
    void StopApplication() override;
    void ScheduleTx();
    void SendPakcet(int num_packet);
    void RecvPacket(Ptr<Socket> socket);
    void main_receive_packet_loop();
    void Report();
    uint16_t m_port;
    uint32_t m_host;
    uint32_t m_num_worker;
    uint16_t m_appID;
    uint32_t m_num_PS;
    uint64_t m_tensor_size;
    Ptr<Socket> m_socket;  
    Address m_peerAddress; 
    uint64_t m_key;
    HashTable* hash_table;
    int UsedSwitchAGTRcount{MAX_AGTR_COUNT};
    int max_agtr_size_per_thread{USE_AGTR_COUNT};
    EventId m_sendEvent;
    EventId m_loopEvent;
    EventId m_statsEvent;
    WindowManager* window_manager;
    CC_manager cc_manager;
    SequenceNumber16 seqNum{0};
    std::queue<PacketInfo> TxQueue;
    std::queue<PacketInfo> RxQueue;
    std::queue<PacketInfo> PendingQueue;
    TracedCallback<uint32_t, bool> m_windowSizeTrace;
    TracedCallback<double, double> m_agg_thoughtput;
    double TimeInterval{1e-6}; // 1us
    uint32_t m_received{0};
    uint32_t m_sent{0};
    void Stats();

    // for receive/send loop
    double m_timestamp;
    double m_timeout{0.05};
    uint64_t send_pointer;
    uint64_t finish_window_seq;
    uint16_t window;

    // for stats
    uint32_t total_dup_packet{0};
    uint32_t collision_times{0};
    TracedCallback<double> m_send_time_interval;
    




};

}
#endif //P4ML_MANAGER_H