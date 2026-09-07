// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT
#pragma once

#include "core.h"
#include "pal/threading.h"

#ifdef USE_SYSTEM_MONITOR
#  include "sysmonitor.h"
#endif

namespace verona::rt
{
  template<class P>
  class CorePool
  {
  private:
    friend P;

#ifdef USE_SYSTEM_MONITOR
    friend SysMonitor<P>;
#endif

    inline static Singleton<Topology, &Topology::init> topology;
    Core* first_core = nullptr;
    size_t core_count = 0;

  public:
    constexpr CorePool() = default;

    void init(size_t count)
    {
      core_count = count;
      // TODO mjp: review allocation.
      first_core = new Core;
      Core* t = first_core;
      size_t index = 0;
      size_t current_domain = -1;
      Core* domain_first = nullptr;
      Core* domain_last = nullptr;

      while (true)
      {
        t->affinity = topology.get().get(index);
        t->domain = topology.get().get_domain(index);
        if (t->domain != current_domain)
        {
          current_domain = t->domain;
          domain_first = t;
          domain_last = t;
          t->domain_next.store(t, std::memory_order_relaxed);
        }
        else
        {
          t->domain_next.store(domain_first, std::memory_order_relaxed);
          domain_last->domain_next.store(t, std::memory_order_relaxed);
          domain_last = t;
        }
        index++;
        if (count > 1)
        {
          t->next = new Core;
          t = t->next;
          count--;
        }
        else
        {
          t->next = first_core;
          break;
        }
      }
    }

    void clear()
    {
      if (first_core == nullptr)
        return;
      size_t count = 0;
      Core* core = first_core->next;
      while (core != first_core)
      {
        Core* next = core->next;
        delete core;
        count++;
        core = next;
      }
      delete first_core;
      count++;
      first_core = nullptr;
      assert(count == core_count);
      core_count = 0;
    }

#ifndef NDEBUG
    ~CorePool()
    {
      assert(first_core == nullptr);
    }
#endif
  };
}
