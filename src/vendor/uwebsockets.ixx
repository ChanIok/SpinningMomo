module;

#include <uwebsockets/App.h>

export module Vendor.UWebSockets;

namespace Vendor::UWebSockets {

export using App = ::uWS::App;
export using Loop = ::uWS::Loop;
export using HttpRequest = ::uWS::HttpRequest;
export using ListenSocket = ::us_listen_socket_t;

export template <bool SSL>
using HttpResponse = ::uWS::HttpResponse<SSL>;

export auto close_listen_socket(ListenSocket* socket) -> void {
  ::us_listen_socket_close(0, socket);
}

}  // namespace Vendor::UWebSockets
