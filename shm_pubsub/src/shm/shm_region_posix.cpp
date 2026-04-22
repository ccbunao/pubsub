// Copyright (c) Continental. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for details.

#if !defined(_WIN32)

#include "shm_region.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace shm_pubsub::detail
{
  static std::string normalize_name(std::string name)
  {
    if (name.empty())
      return name;
    if (name.front() != '/')
      name.insert(name.begin(), '/');
    return name;
  }

  ShmRegion create_or_open(const std::string& name, std::size_t size)
  {
    ShmRegion out;
    out.name = normalize_name(name);
    out.size = size;

    const int fd = ::shm_open(out.name.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd != -1)
    {
      out.owner = true;
      out.fd = fd;
      if (::ftruncate(fd, static_cast<off_t>(size)) == -1)
      {
        close_region(out);
        return ShmRegion{};
      }
    }
    else if (errno == EEXIST)
    {
      out.owner = false;
      out.fd = ::shm_open(out.name.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
      if (out.fd == -1)
        return ShmRegion{};

      struct stat st{};
      if (::fstat(out.fd, &st) == -1)
      {
        close_region(out);
        return ShmRegion{};
      }
      if (static_cast<std::size_t>(st.st_size) < size)
      {
        close_region(out);
        return ShmRegion{};
      }
      out.size = static_cast<std::size_t>(st.st_size);
    }
    else
    {
      return ShmRegion{};
    }

    void* addr = ::mmap(nullptr, out.size, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd, 0);
    if (addr == MAP_FAILED)
    {
      close_region(out);
      return ShmRegion{};
    }
    out.addr = addr;
    return out;
  }

  ShmRegion open_existing(const std::string& name)
  {
    ShmRegion out;
    out.name = normalize_name(name);

    out.fd = ::shm_open(out.name.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (out.fd == -1)
      return ShmRegion{};

    struct stat st{};
    if (::fstat(out.fd, &st) == -1)
    {
      close_region(out);
      return ShmRegion{};
    }
    out.size = static_cast<std::size_t>(st.st_size);

    void* addr = ::mmap(nullptr, out.size, PROT_READ | PROT_WRITE, MAP_SHARED, out.fd, 0);
    if (addr == MAP_FAILED)
    {
      close_region(out);
      return ShmRegion{};
    }

    out.addr = addr;
    out.owner = false;
    return out;
  }

  void close_region(ShmRegion& region)
  {
    if (region.addr != nullptr && region.size != 0)
      ::munmap(region.addr, region.size);
    region.addr = nullptr;
    region.size = 0;

    if (region.fd != -1)
      ::close(region.fd);
    region.fd = -1;
  }
}

#endif
