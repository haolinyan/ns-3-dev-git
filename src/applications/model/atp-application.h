#ifndef ATP_APP_H
#define ATP_APP_H
#include "ns3/sequence-number.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ptr.h"
#include <ns3/traced-callback.h>
#include "ns3/HashTable.h"
#include "ns3/ATPCommon.h"
#include "ns3/window_manager.h"
#include "ns3/CC_manager.h"

namespace ns3 {
class Socket;
class Packet;

class AtpApplication : public Application {
public:
    static TypeId GetTypeId();
    AtpApplication();
    ~AtpApplication() override;
    int Send(PacketBuffer* packetBuffer);
    void SechduleTx();
    void HandleRead(Ptr<Socket> socket);
    void CheckTimeout(int pos_start, int pos_end, uint64_t window_shift);

protected:
    void DoDispose() override;

private:
    void StartApplication() override;
    void StopApplication() override;
    uint64_t m_totalTx{0};    //!< Total bytes sent
    uint64_t m_totalRx{0};    //!< Total bytes received
    uint64_t m_total_pkt{0};
    uint64_t pending_pkt{0};
    uint32_t m_totalSize{0};
    Ptr<Socket> m_socket;
    Address m_RemoteAddress;
    uint16_t m_RemotePort;
    uint16_t m_jobId;
    uint16_t m_workerId;
    uint32_t m_max_agtr_size;
    uint16_t m_appId;
    uint8_t num_workers;
    EventId m_sendEvent;
    EventId m_timeoutEvent;
    static HashTable* hash_table;
    CC_manager* cc_manager;
    WindowManager window_manager;
    uint8_t m_tos;    
    std::string m_RemoteAddressString;
};
}
#endif