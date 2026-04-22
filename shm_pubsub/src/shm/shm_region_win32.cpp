// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#if defined(_WIN32)

#include "shm_region.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

namespace shm_pubsub::detail
{
  ShmRegion create_or_open(const std::string& name, std::size_t size)
  {
    ShmRegion out;
    out.name = name;
    out.size = size;

    HANDLE hmap = ::CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), name.c_str());
    if (hmap == nullptr)
      return ShmRegion{};

    out.owner = (::GetLastError() != ERROR_ALREADY_EXISTS);

    void* addr = ::MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (addr == nullptr)
    {
      ::CloseHandle(hmap);
      return ShmRegion{};
    }

    out.handle = hmap;
    out.addr = addr;
    return out;
  }

  ShmRegion open_existing(const std::string& name)
  {
    ShmRegion out;
    out.name = name;

    HANDLE hmap = ::OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name.c_str());
    if (hmap == nullptr)
      return ShmRegion{};

    void* addr = ::MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (addr == nullptr)
    {
      ::CloseHandle(hmap);
      return ShmRegion{};
    }

    out.handle = hmap;
    out.addr = addr;
    out.owner = false;
    out.size = 0;
    return out;
  }

  void close_region(ShmRegion& region)
  {
    if (region.addr != nullptr)
      ::UnmapViewOfFile(region.addr);
    region.addr = nullptr;

    if (region.handle != nullptr)
      ::CloseHandle(reinterpret_cast<HANDLE>(region.handle));
    region.handle = nullptr;

    region.size = 0;
  }
}

#endif
