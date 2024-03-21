#ifndef P4ML_STRUCT_H
#define P4ML_STRUCT_H
#include <inttypes.h>
struct ThreadInfo
{
    int thread_id;
    int agtr_start_pos;
};

struct Job
{
    uint64_t key;
    float *float_data;
    int32_t *int_data;
    uint32_t len;
    int cmd;
};

struct AppInfo
{
    uint32_t host;
    uint16_t appID;
    uint8_t num_worker;
    uint8_t num_PS;
};

struct PacketInfo {
    uint32_t bitmap;
    uint8_t fanInDegree;
    bool overflow;
    bool resend;
    bool collision;
    bool ecn;
    bool isAcked;
    uint16_t appID;
    uint16_t seqNum;
    uint16_t aggregatorIndex;
    uint64_t key;
    uint32_t len_tensor; 
};


#endif