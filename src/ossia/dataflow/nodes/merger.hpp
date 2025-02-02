#pragma once
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

namespace ossia::nodes
{
class merger final : public ossia::graph_node
{
  int m_count{};
public:

  merger(int count)
    : m_count{count}
  {
    for (int i = 0; i < count; i++)
    {
      auto inl = new ossia::audio_inlet;
      inl->target<ossia::audio_port>()->samples.resize(2);
      for (auto& channel : inl->target<ossia::audio_port>()->samples)
      {
        channel.reserve(512);
      }
      m_inlets.push_back(std::move(inl));
    }

    m_outlets.push_back(new ossia::audio_outlet);
    m_outlets.back()->target<ossia::audio_port>()->samples.resize(
        2 * count);
    for (auto& channel :
         m_outlets.back()->target<ossia::audio_port>()->samples)
    {
      channel.reserve(512);
    }
  }

  ~merger() override
  {
  }

  void
  run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    auto& out = m_outlets.back()->target<ossia::audio_port>()->samples;
    std::size_t cur = 0;
    for (int i = 0; i < m_count; i++)
    {
      auto& in = m_inlets[i]->target<ossia::audio_port>()->samples;

      switch(in.size())
      {
        case 1:
          out[cur++] = in[0];
          out[cur++] = in[0];
          break;
        case 2:
          out[cur++] = in[0];
          out[cur++] = in[1];
          break;
        default:
          cur += 2;
          break;
      }
    }
  }

  std::string label() const noexcept override
  {
    return "Stereo Merger";
  }
};
}
