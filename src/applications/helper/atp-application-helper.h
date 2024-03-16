#ifndef ATP_APP_HELPER_H
#define ATP_APP_HELPER_H

#include "ns3/application-container.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/atp-application.h"

#include <stdint.h>

namespace ns3
{
class AtpApplicationHelper
{
  public:
    AtpApplicationHelper();

    AtpApplicationHelper(Address address, uint16_t port, uint32_t totalSize, uint16_t jobId, uint16_t workerId);
   
    void SetAttribute(std::string name, const AttributeValue& value);

    ApplicationContainer Install(NodeContainer c);

  private:
    ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* ATP_Application_SERVER_H */
