#ifndef CC_MANAGER_H
#define CC_MANAGER_H
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include "Common.h"

using namespace std;
#define do_div(n, base) ({            \
    uint32_t __base = (base);         \
    uint32_t __rem;                   \
    __rem = ((uint64_t)(n)) % __base; \
    (n) = ((uint64_t)(n)) / __base;   \
    __rem;                            \
})
#define GET_MIN(a, b) (a < b ? a : b)
#define GET_MAX(a, b) (a > b ? a : b)

class CC_manager {

public:
    CC_manager()
    {
    }

    void SetCwnd(int cwnd, int max_window_size)
    {
        cwnd_bytes = cwnd * P4ML_PACKET_SIZE;
        m_max_window_size = (uint64_t) max_window_size * P4ML_PACKET_SIZE;
    }

    int adjustWindow(bool isECN)
    {
        if (isECN)
        {
            std::cout << "ECN detected, reducing window size" << std::endl;
            cwnd_bytes /= 2;
        }
        else
        {
            cwnd_bytes += 1500;
        }

        if (cwnd_bytes < P4ML_PACKET_SIZE)
            cwnd_bytes = P4ML_PACKET_SIZE;
        if (cwnd_bytes > m_max_window_size)
            cwnd_bytes = m_max_window_size;
        if (cwnd_bytes > P4ML_PACKET_SIZE)
            cwnd_bytes = (cwnd_bytes / P4ML_PACKET_SIZE) * P4ML_PACKET_SIZE;
        return cwnd_bytes / P4ML_PACKET_SIZE;
    }

private:
    uint64_t cwnd_bytes;
    uint64_t m_max_window_size;
};

#endif