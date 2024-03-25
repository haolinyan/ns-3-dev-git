#ifndef BROADCOM_EGRESS_H
#define BROADCOM_EGRESS_H

#include "queue.h"

namespace ns3
{
class BEgressQueue : public QueueBase {
	public:
		static TypeId GetTypeId(void);
		static const unsigned fCnt = 128; //max number of queues, 128 for NICs
		static const unsigned qCnt = 8; //max number of queues, 8 for switches
        
		BEgressQueue();
		~BEgressQueue() override;
        bool DoEnqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DoDequeueRR(bool paused[]);
		Ptr<Packet> DequeueRR(bool paused[]);
		Ptr<Packet> DoDequeue(void);

		bool Enqueue(Ptr<Packet> p, uint32_t qIndex);

		uint32_t GetNBytes(uint32_t qIndex) const;
		uint32_t GetNBytesTotal() const;
		uint32_t GetLastQueue();

		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqEnqueue;
		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqDequeue;
        typedef void (*QueueCallback)(Ptr<Packet>, uint32_t);

        double m_maxBytes; //total bytes limit
		uint32_t m_bytesInQueue[fCnt];
		uint32_t m_bytesInQueueTotal;
		uint32_t m_rrlast;
		uint32_t m_qlast;
		std::vector<Ptr<QueueBase>> m_queues; // uc queues


		/// Traced callback: fired when a packet is enqueued
    	TracedCallback<Ptr<const Packet>> m_traceEnqueue;
    	/// Traced callback: fired when a packet is dequeued
    	TracedCallback<Ptr<const Packet>> m_traceDequeue;

};
}
#endif /* DROPTAIL_H */