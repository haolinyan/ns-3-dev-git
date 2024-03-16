#include "atp-application-helper.h"
#include "ns3/string.h"
#include "ns3/atp-application.h"
#include "ns3/uinteger.h"


namespace ns3
{

AtpApplicationHelper::AtpApplicationHelper()
{
  m_factory.SetTypeId(AtpApplication::GetTypeId());
}

AtpApplicationHelper::AtpApplicationHelper(Address address, uint16_t port, uint32_t totalSize, uint16_t jobId, uint16_t workerId)
{
  m_factory.SetTypeId(AtpApplication::GetTypeId());
  m_factory.Set("RemoteAddress", AddressValue(address));
  m_factory.Set("RemotePort", UintegerValue(port));
  m_factory.Set("totalSize", UintegerValue(totalSize));
  m_factory.Set("jobId", UintegerValue(jobId));
  m_factory.Set("workerId", UintegerValue(workerId));
}

void
AtpApplicationHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
AtpApplicationHelper::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;
        Ptr<AtpApplication> app = m_factory.Create<AtpApplication>();
        node->AddApplication(app);
        apps.Add(app);
    }
    return apps;
}
}