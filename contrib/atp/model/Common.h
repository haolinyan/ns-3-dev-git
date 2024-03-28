#ifndef ATP_COMMON_H
#define ATP_COMMON_H
#include <ns3/node.h>
#include <inttypes.h>
#define MAX_AGTR_COUNT 20000
#define USE_AGTR_COUNT 100

#define P4ML_PACKET_SIZE 272 // 248 + 24 = 272
#define P4ML_DATA_SIZE 248
#define MAX_ENTRIES_PER_PACKET 62
#define PS_BUFFER_TIMEOUT 0.5
#define CHANGE_AGTR_ENABLE 0
#define LOSS_RECOVERY_ENABLE 1
#define CC_ENABLE 0
#define CLI "\033[32m[CLI]\033[0m"
#define PS "\033[34m[PS]\033[0m"
#define SW "\033[33m[SW]\033[0m"
#define TIMEOUT "\033[31m[TIMEOUT]\033[0m"
#define PS_BUFFER_SIZE 65536
#define ATP_ENABLE 1
namespace ns3 {
typedef Callback<void, Ptr<Packet>, Ipv4Address, uint32_t> SendPacketCallback;
typedef Callback<void> CancelNICQueueCallback;
}
#endif