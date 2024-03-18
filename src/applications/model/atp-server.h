#ifndef ATP_PS_SERVER_H
#define ATP_PS_SERVER_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/MemoryLayout.h"
namespace ns3
{

class Socket;
class Packet;


class AtpServer : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    AtpServer();
    ~AtpServer() override;

  protected:
    void DoDispose() override;

  private:
    void StartApplication() override;
    void StopApplication() override;
    ParameterServer m_ps;
    /**
     * \brief Handle a packet reception.
     *
     * This function is called by lower layers.
     *
     * \param socket the socket the packet was received to.
     */
    void HandleRead(Ptr<Socket> socket);

    uint16_t m_port;       //!< Port on which we listen for incoming packets.
    uint8_t m_tos;         //!< The packets Type of Service
    Ptr<Socket> m_socket;  //!< IPv4 Socket
    Address m_local;       //!< local multicast address
    uint32_t m_max_agtr_size; //!< Maximum AGTR size
    uint16_t m_appId; //!< Application ID
};

} // namespace ns3

#endif /* ATP_PS_SERVER_H */
