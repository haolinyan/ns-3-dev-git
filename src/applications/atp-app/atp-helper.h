#ifndef ATP_HELPER_H
#define ATP_HELPER_H
#include "ns3/application-container.h"
#include "ns3/object-factory.h"
#include "ns3/uinteger.h"
#include "ns3/atp-client-app.h"
#include "ns3/atp-server-app.h"
/*
为多个节点安装ATP应用程序
*/

namespace ns3
{
class AtpClientHelper
{
    public:
        AtpClientHelper(uint16_t port, 
                        Address address,
                        uint32_t host, 
                        uint32_t num_worker, 
                        uint16_t appID, 
                        uint32_t num_PS, 
                        uint64_t key, 
                        uint64_t tensor_size) {
            m_factory.SetTypeId(AtpClient::GetTypeId());
            SetAttribute("Port", UintegerValue(port));
            SetAttribute("RemoteAddress", AddressValue(address));
            SetAttribute("Host", UintegerValue(host));
            SetAttribute("NumWorker", UintegerValue(num_worker));
            SetAttribute("AppID", UintegerValue(appID));
            SetAttribute("NumPS", UintegerValue(num_PS));
            SetAttribute("Key", UintegerValue(key));
            SetAttribute("TensorSize", UintegerValue(tensor_size));

        }
        void SetAttribute(std::string name, const AttributeValue& value) {
             m_factory.Set(name, value);
        }
        ApplicationContainer Install(Ptr<Node> node) const {
            return ApplicationContainer(InstallPriv(node));
        }
    private:
        Ptr<Application> InstallPriv(Ptr<Node> node) const {
            Ptr<Application> app = m_factory.Create<AtpClient>();
            node->AddApplication(app);
            return app;
        }
        ObjectFactory m_factory;
};


class AtpServerHelper
{
    public:
        AtpServerHelper(uint16_t port, 
                        uint16_t appID) {
            m_factory.SetTypeId(AtpServer::GetTypeId());
            SetAttribute("Port", UintegerValue(port));
            SetAttribute("AppID", UintegerValue(appID));
        }
        void SetAttribute(std::string name, const AttributeValue& value) {
             m_factory.Set(name, value);
        }
        ApplicationContainer Install(Ptr<Node> node) const {
            return ApplicationContainer(InstallPriv(node));
        }
    private:
        Ptr<Application> InstallPriv(Ptr<Node> node) const {
            Ptr<Application> app = m_factory.Create<AtpServer>();
            node->AddApplication(app);
            return app;
        }
        ObjectFactory m_factory;
};


}
#endif 