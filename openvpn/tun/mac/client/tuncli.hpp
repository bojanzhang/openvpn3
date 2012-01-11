#ifndef OPENVPN_TUN_MAC_CLIENT_TUNCLI_H
#define OPENVPN_TUN_MAC_CLIENT_TUNCLI_H

#include <openvpn/tun/mac/tun.hpp>
#include <openvpn/tun/client/tunbase.hpp>
#include <openvpn/netconf/mac/route.hpp>

namespace openvpn {
  namespace TunMac {

    class ClientConfig : public TunClientFactory
    {
    public:
      typedef boost::intrusive_ptr<ClientConfig> Ptr;

      Layer layer;
      unsigned int mtu;

      int n_parallel;
      Frame::Ptr frame;
      ProtoStats::Ptr stats;

      static Ptr new_obj()
      {
	return new ClientConfig;
      }

      virtual TunClient::Ptr new_client_obj(boost::asio::io_service& io_service,
					    TunClientParent& parent);
    private:
      ClientConfig()
	: mtu(1500), n_parallel(8) {}
    };

    class Client : public TunClient
    {
      friend class ClientConfig;  // calls constructor
      friend class Tun<Client*>;  // calls tun_read_handler

      typedef Tun<Client*> TunImpl;

    public:
      virtual void client_start(const OptionList& opt, TransportClient& transcli)
      {
	if (!impl)
	  {
	    halt = false;
	    try {
	      // start tun
	      impl.reset(new TunImpl(io_service,
				     this,
				     config->frame,
				     config->stats,
				     config->layer
				     ));
	      impl->start(config->n_parallel);

	      // do ifconfig
	      impl->ifconfig(opt, config->mtu);

	      // add routes
	      route_list.reset(new RouteListMac(opt, transcli.server_endpoint_addr()));

	      // signal that we are connected
	      parent.tun_connected();
	    }
	    catch (std::exception& e)
	      {
		config->stats->error(ProtoStats::TUN_ERROR);
		stop();
		parent.tun_error(e);
	      }
	  }
      }

      virtual bool tun_send(BufferAllocated& buf)
      {
	return send(buf);
      }

      virtual std::string tun_name() const
      {
	if (impl)
	  return impl->name();
	else
	  return "UNDEF_TUN";
      }

      virtual void stop() { stop_(); }
      virtual ~Client() { stop_(); }

    private:
      Client(boost::asio::io_service& io_service_arg,
	     ClientConfig* config_arg,
	     TunClientParent& parent_arg)
	:  io_service(io_service_arg),
	   config(config_arg),
	   parent(parent_arg),
	   halt(false)
      {
      }

      bool send(const Buffer& buf)
      {
	if (impl)
	  return impl->write(buf);
	else
	  return false;
      }

      void tun_read_handler(PacketFrom::SPtr& pfp) // called by TunImpl
      {
	parent.tun_recv(pfp->buf);
      }

      void stop_()
      {
	// remove added routes
	if (route_list)
	  {
	    route_list->stop();
	    route_list.reset();
	  }

	// stop tun
	if (impl)
	  {
	    impl->stop();
	    impl.reset();
	  }
	halt = true;
      }

      boost::asio::io_service& io_service;
      ClientConfig::Ptr config;
      TunClientParent& parent;
      TunImpl::Ptr impl;
      RouteListMac::Ptr route_list;
      bool halt;
    };

    inline TunClient::Ptr ClientConfig::new_client_obj(boost::asio::io_service& io_service,
						       TunClientParent& parent)
    {
      return TunClient::Ptr(new Client(io_service, this, parent));
    }

  }
} // namespace openvpn

#endif // OPENVPN_TUN_MAC_CLIENT_TUNCLI_H
