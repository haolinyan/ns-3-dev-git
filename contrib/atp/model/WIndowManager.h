#ifndef SLIDING_W_H
#define SLIDING_W_H

#include "CC_manager.h"


class WindowManager {
    public:
        bool* isACKed;
        int total_ACK;
        int last_ACK;

        WindowManager() {
            last_ACK = 0;
        }
        ~WindowManager() {
            delete[] isACKed;
        }

        bool inline UpdateWindow(uint16_t* seq_num)
        {
            bool isLastAckUpdated = false;
            isACKed[*seq_num] = true;
            while (isACKed[last_ACK + 1]) {
                last_ACK++;
                isLastAckUpdated = true;
            }
            return isLastAckUpdated;
        }

        void inline Reset(int packet_total)
        {
            last_ACK = 0;
            total_ACK = packet_total;
            isACKed = new bool[packet_total];
            for (size_t i = 0; i < packet_total; i++)
            {
                isACKed[i] = false;
            }
        }
};

#endif