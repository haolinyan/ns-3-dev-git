#ifndef ATP_DRIVER_HELPER_H
#define ATP_DRIVER_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/queue.h"
#include "ns3/trace-helper.h"
#include "ns3/sim-channel.h"
#include "ns3/sim-net-device.h"
#include "atp-driver-helper.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/packet.h"
#include <string>

namespace ns3
{

class NetDevice;
class Node;


class AtpDriverHelper 
{
  public:
    AtpDriverHelper() {
       m_queueFactory.SetTypeId("ns3::DropTailQueue<Packet>");
      m_deviceFactory.SetTypeId("ns3::SimNetDevice");
      m_channelFactory.SetTypeId("ns3::SimChannel");
    }
   
    void SetDeviceAttribute(std::string n1, const AttributeValue& v1) {
      m_deviceFactory.Set(n1, v1);
    }

    
    void SetChannelAttribute(std::string n1, const AttributeValue& v1) {
      m_channelFactory.Set(n1, v1);
    }

  
    NetDeviceContainer Install(NodeContainer c) {
      NS_ASSERT(c.GetN() == 2);
      return Install(c.Get(0), c.Get(1));
    }

  
    NetDeviceContainer Install(Ptr<Node> a, Ptr<Node> b) {
      NetDeviceContainer container;

      Ptr<SimNetDevice> devA = m_deviceFactory.Create<SimNetDevice>();
      a->AddDevice(devA);
      Ptr<Queue<Packet>> queueA = m_queueFactory.Create<Queue<Packet>>();
      devA->SetQueue(queueA);
      Ptr<Queue<Packet>> queueA_pri = m_queueFactory.Create<Queue<Packet>>();
      devA->SetPriorityQueue(queueA_pri);

      Ptr<SimNetDevice> devB = m_deviceFactory.Create<SimNetDevice>();
      b->AddDevice(devB);
      Ptr<Queue<Packet>> queueB = m_queueFactory.Create<Queue<Packet>>();
      devB->SetQueue(queueB);
      Ptr<Queue<Packet>> queueB_pri = m_queueFactory.Create<Queue<Packet>>();
      devB->SetPriorityQueue(queueB_pri);

      Ptr<SimChannel> channel = m_channelFactory.Create<SimChannel>();

      devA->Attach(channel);
      devB->Attach(channel);

      container.Add(devA);
      container.Add(devB);

      return container;
    }

    template <typename... Ts>
    void SetQueue(std::string type, Ts&&... args) {
      QueueBase::AppendItemTypeIfNotPresent(type, "Packet");

      m_queueFactory.SetTypeId(type);
      m_queueFactory.Set(std::forward<Ts>(args)...);
    }
    
  private:

    ObjectFactory m_queueFactory;   //!< Queue Factory
    ObjectFactory m_channelFactory; //!< Channel Factory
    ObjectFactory m_deviceFactory;  //!< Device Factory
};

} // namespace ns3

#endif 