// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <ossia/detail/config.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/common/debug.hpp>
#include <ossia/network/oscquery/oscquery_mirror_asio.hpp>
#include <ossia/network/oscquery/detail/http_client.hpp>
#include <ossia/network/base/parameter_data.hpp>
#include <ossia/network/base/osc_address.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <iostream>
#include <memory>
#include <functional>
#include <ossia/network/context.hpp>

using namespace ossia;
using namespace ossia::net;
using namespace std;

void explore(const node_base& node);
void printDomain(const domain& d);
void printValueCallback(const value& v);

auto ctx = std::make_shared<ossia::net::network_context>();

int main()
{

  // Create a protocol that will connect to the given websocket address
  auto protocol = new ossia::oscquery::oscquery_mirror_asio_protocol{ctx, "ws://127.0.0.1:5678"};
  protocol->set_logger(network_logger{ossia::logger_ptr(), ossia::logger_ptr()});

  // Create a device that wil attach to this protocol
  ossia::net::generic_device device{std::unique_ptr<protocol_base>(protocol), "B"};

  // Explore the tree of B
  {
    auto fut = device.get_protocol().update_async(device);

    while(fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        ctx->context.poll_one();
    }
  }

  // Display the tree in console
  explore(device.get_root_node());

  ctx->context.run();

  ossia::net::find_node(device, "/test/my_string")->get_parameter()->push_value("fheakoezp");


  auto node = ossia::net::find_node(device, "/test/my_float");
  // Request to add an instance :
  protocol->request_add_node(*node, parameter_data{"layer"});

  // Again : (will become layer.1)
  protocol->request_add_node(*node, parameter_data{"layer"});

  // Wait a bit to get a reply
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Wait a bit
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Display the tree with the new nodes
  explore(device.get_root_node());
}

void explore(const ossia::net::node_base& node)
{
  for (const auto& child : node.children_copy())
  {
    if (auto addr = child->get_parameter())
    {
      // attach to callback to display value update
      addr->add_callback([=] (const value& v) {
        std::cerr << "[message] " << osc_parameter_string(*addr)
                  << " <- " <<  value_to_pretty_string(v) << std::endl;
      });

      // update the value
      auto fut = addr->pull_value_async();

      while(fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
      {
          ctx->context.poll_one();
      }
    }

    using namespace fmt;
    fmt::print(stderr, "{}\n", *child);

    explore(*child);
  }
}
