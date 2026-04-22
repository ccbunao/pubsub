// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#pragma once

#include <cstdint>

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

namespace shm_pubsub::detail
{
  inline std::uint64_t load_u64_acquire(const volatile std::uint64_t* ptr)
  {
#if defined(_MSC_VER)
    return static_cast<std::uint64_t>(_InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(const_cast<volatile std::uint64_t*>(ptr)), 0, 0));
#else
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
#endif
  }

  inline void store_u64_release(volatile std::uint64_t* ptr, std::uint64_t value)
  {
#if defined(_MSC_VER)
    (void)_InterlockedExchange64(reinterpret_cast<volatile long long*>(ptr), static_cast<long long>(value));
#else
    __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
#endif
  }

  inline std::uint64_t fetch_add_u64_acq_rel(volatile std::uint64_t* ptr, std::uint64_t add)
  {
#if defined(_MSC_VER)
    return static_cast<std::uint64_t>(_InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(ptr), static_cast<long long>(add)));
#else
    return __atomic_fetch_add(ptr, add, __ATOMIC_ACQ_REL);
#endif
  }

  inline bool compare_exchange_u64_acq_rel(volatile std::uint64_t* ptr, std::uint64_t& expected, std::uint64_t desired)
  {
#if defined(_MSC_VER)
    const std::uint64_t prev =
      static_cast<std::uint64_t>(
        _InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(ptr),
                                      static_cast<long long>(desired),
                                      static_cast<long long>(expected)));
    const bool ok = (prev == expected);
    expected = prev;
    return ok;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
  }
}
