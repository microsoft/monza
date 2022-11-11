// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

/**
 * This file uses the "enclave" terminology as it might be reused as a generic
 * enclave management API.
 */

#include <chrono>
#include <filesystem>
#include <list>
#include <memory>
#include <new>
#include <span>

#ifdef MONZA_HOST_SUPPORTS_QEMU
#  include <fcntl.h>
#  include <random>
#  include <signal.h>
#  include <spawn.h>
#  include <sstream>
#  include <sys/mman.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

#ifdef MONZA_HOST_SUPPORTS_HCS
#  include <hcs_enclave.h>
#endif

namespace monza::host
{
  template<typename T>
  struct SharedMemoryObject
  {
    T& host_object;
    uintptr_t enclave_start_address;
  };

  template<typename T>
  struct SharedMemoryArray
  {
    std::span<T> host_span;
    uintptr_t enclave_start_address;
  };

  enum class EnclaveType
  {
    QEMU,
    HCS,
    HCS_ISOLATED
  };

  template<typename InitializerTuple>
  class EnclavePlatform
  {
  protected:
    size_t num_threads;

    EnclavePlatform(size_t num_threads) : num_threads(num_threads) {}

  private:
    /**
     * Underlying shared memory allocator implementation.
     * Returns non-owning non-null pointer to uninitialized memory and the
     * corresponding enclave address. Range will be unmapped on destruction of
     * EnclavePlatform instance.
     */
    virtual std::pair<void*, uintptr_t>
    allocate_shared_inner(size_t size, size_t alignment) = 0;

  public:
    virtual ~EnclavePlatform() {}

    /**
     * Factory method to create an EnclavePlatform instance of a given type.
     */
    static std::unique_ptr<EnclavePlatform>
    create(EnclaveType type, const std::string& path, size_t num_threads);

    /**
     * Returns non-owning reference to typed shared memory constructed with
     * arguments. Range will be unmapped on destruction of EnclavePlatform
     * instance.
     */
    template<typename T, typename... Args>
    SharedMemoryObject<T> allocate_shared(Args&&... args)
    {
      auto allocation_tuple = allocate_shared_inner(sizeof(T), alignof(T));
      auto& host_ref = *(new (allocation_tuple.first) T(args...));
      return SharedMemoryObject<T>{host_ref, allocation_tuple.second};
    }

    /**
     * Returns non-owning span to typed shared memory array constructed with
     * arguments. Range will be unmapped on destruction of EnclavePlatform
     * instance.
     */
    template<typename T, typename... Args>
    SharedMemoryArray<T> allocate_shared_array(size_t count, Args&&... args)
    {
      auto allocation_tuple =
        allocate_shared_inner(count * sizeof(T), alignof(T));
      for (size_t i = 0; i < count; ++i)
      {
        new (&((static_cast<T*>(allocation_tuple.first))[i])) T(args...);
      }
      return SharedMemoryArray<T>{
        std::span<T>(static_cast<T*>(allocation_tuple.first), count),
        allocation_tuple.second};
    }

    virtual void initialize(InitializerTuple initArgs) = 0;
    virtual void async_run() = 0;
    virtual void join() = 0;
  };

#ifdef MONZA_HOST_SUPPORTS_QEMU

  template<typename InitializerTuple>
  class QemuEnclavePlatform : public EnclavePlatform<InitializerTuple>
  {
    static constexpr size_t SHMEM_SIZE = 64 * 1024 * 1024;
    static constexpr size_t SHMEM_START = (1ULL << 40) - SHMEM_SIZE;

    static inline std::default_random_engine id_generator;
    static inline std::uniform_int_distribution<uint64_t> id_distribution;

    uint64_t instance_id;
    std::string shmem_file;
    std::string monitor_file;
    pid_t qemu_pid = 0;

    int shmem_file_id = 0;
    uint8_t* shmem_base;
    size_t shmem_offset;

    bool joined = false;

  protected:
    QemuEnclavePlatform(
      EnclaveType type, const std::string& path, size_t num_threads)
    : EnclavePlatform<InitializerTuple>(num_threads),
      instance_id(id_distribution(id_generator))
    {
      std::stringstream shmem_file_stream;
      shmem_file_stream << "monza-qemu-shmem-" << instance_id;
      shmem_file = shmem_file_stream.str();

      std::stringstream monitor_file_stream;
      monitor_file_stream << "/tmp/monza-qemu-socket-" << instance_id;
      monitor_file = monitor_file_stream.str();

      // Remove the shmem or monitor files left over from a previous run.
      if (std::filesystem::exists(monitor_file))
      {
        std::filesystem::remove(monitor_file);
      }

      // Spawn QEMU process.
      constexpr auto binary = "/usr/bin/qemu-system-x86_64";
      std::stringstream cores_argument_builder;
      cores_argument_builder << "cores=" << num_threads;
      auto cores_argument = cores_argument_builder.str();
      std::stringstream shmem_file_argument_builder;
      shmem_file_argument_builder
        << "memory-backend-file,id=shmem,share=on,size=" << SHMEM_SIZE
        << ",mem-path=/dev/shm/" << shmem_file;
      auto shmem_file_argument = shmem_file_argument_builder.str();
      std::stringstream shmem_device_argument_builder;
      shmem_device_argument_builder << "pc-dimm,memdev=shmem,addr="
                                    << SHMEM_START;
      auto shmem_device_argument = shmem_device_argument_builder.str();
      std::stringstream monitor_file_argument_builder;
      monitor_file_argument_builder << "unix:" << monitor_file
                                    << ",server,nowait";
      auto monitor_file_argument = monitor_file_argument_builder.str();
      const char* args[] = {binary,       "-enable-kvm",
                            "-cpu",       "host,+invtsc",
                            "-no-reboot", "-nographic",
                            "-smp",       cores_argument.c_str(),
                            "-m",         "1G,slots=2,maxmem=1T",
                            "-object",    shmem_file_argument.c_str(),
                            "-device",    shmem_device_argument.c_str(),
                            "-monitor",   monitor_file_argument.c_str(),
                            "-S",         "-kernel",
                            path.c_str(), nullptr};
      posix_spawn(
        &qemu_pid, binary, nullptr, nullptr, const_cast<char**>(args), nullptr);

      // Wait for QEMU to create the shmem and monitor files.
      while (!std::filesystem::exists(monitor_file))
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      // Map shared memory into host process.
      shmem_file_id = shm_open(shmem_file.c_str(), O_RDWR, 0);
      if (shmem_file_id == -1)
      {
        cleanup();
        throw std::runtime_error(
          "Failed to map enclave shared memory to host.");
      }
      shmem_base = static_cast<uint8_t*>(mmap(
        0, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmem_file_id, 0));
      if (shmem_base == MAP_FAILED)
      {
        cleanup();
        throw std::runtime_error(
          "Failed to map enclave shared memory to host.");
      }
      shmem_offset = 0;

      // Reserve space for InitializerTuple.
      if (SHMEM_SIZE > sizeof(InitializerTuple))
      {
        shmem_offset += sizeof(InitializerTuple);
      }
      else
      {
        cleanup();
        throw std::runtime_error(
          "No enough enclave shared memoy for initialization arguments.");
      }
    }

    void cleanup()
    {
      if (qemu_pid != 0)
      {
        kill(qemu_pid, SIGTERM);
      }

      if (shmem_file_id != 0)
      {
        close(shmem_file_id);
      }

      std::filesystem::remove(monitor_file);
    }

  public:
    ~QemuEnclavePlatform() override
    {
      if (!joined)
      {
        join();
      }

      munmap(shmem_base, SHMEM_SIZE);

      cleanup();
    }

  public:
    void initialize(InitializerTuple initArgs) override
    {
      *reinterpret_cast<InitializerTuple*>(shmem_base) = initArgs;
    }

    void async_run() override
    {
      std::stringstream socat_command_builder;
      socat_command_builder << "echo \"cont\" | socat - unix-connect:"
                            << monitor_file;
      auto socat_command = socat_command_builder.str();
      std::system(socat_command.c_str());
    }

    void join() override
    {
      if (!joined)
      {
        int status;
        waitpid(qemu_pid, &status, 0);
      }
      joined = true;
    }

  protected:
    std::pair<void*, uintptr_t>
    allocate_shared_inner(size_t size, size_t alignment) override
    {
      auto aligned_shmem_offset =
        ((shmem_offset + alignment - 1) / alignment) * alignment;
      if (
        aligned_shmem_offset >= shmem_offset &&
        aligned_shmem_offset + size > aligned_shmem_offset &&
        aligned_shmem_offset + size < SHMEM_SIZE)
      {
        memset(shmem_base + aligned_shmem_offset, 0, size);
        auto result = std::make_pair(
          shmem_base + aligned_shmem_offset,
          SHMEM_START + aligned_shmem_offset);
        shmem_offset = aligned_shmem_offset + size;
        return result;
      }
      else
      {
        throw std::runtime_error(
          "Not enough enclave shared memory for allocation.");
      }
    }

    friend class EnclavePlatform<InitializerTuple>;
  };

#endif

#ifdef MONZA_HOST_SUPPORTS_HCS

  template<typename InitializerTuple>
  class HcsEnclavePlatform : public EnclavePlatform<InitializerTuple>
  {
    static constexpr size_t SHMEM_SIZE = 64 * 1024 * 1024;

    std::unique_ptr<HCSEnclaveAbstract> instance;

    size_t shmem_guest_base;
    uint8_t* shmem_base;
    size_t shmem_offset;

    bool joined;

  protected:
    HcsEnclavePlatform(
      EnclaveType type,
      const std::string& path,
      size_t num_threads,
      bool is_isolated)
    : EnclavePlatform<InitializerTuple>(num_threads),
      instance(
        HCSEnclaveAbstract::create(path, num_threads, SHMEM_SIZE, is_isolated)),
      shmem_guest_base(instance->shared_memory_guest_base()),
      shmem_base(instance->shared_memory().data()),
      shmem_offset(0),
      joined(false)
    {
      // Reserve space for InitializerTuple.
      if (SHMEM_SIZE > sizeof(InitializerTuple))
      {
        shmem_offset += sizeof(InitializerTuple);
      }
      else
      {
        throw std::runtime_error(
          "No enough enclave shared memory for initialization arguments.");
      }
    }

    void cleanup() {}

  public:
    ~HcsEnclavePlatform() override
    {
      if (!joined)
      {
        join();
      }
    }

  public:
    void initialize(InitializerTuple initArgs) override
    {
      *reinterpret_cast<InitializerTuple*>(shmem_base) = initArgs;
    }

    void async_run() override
    {
      instance->async_run();
    }

    void join() override
    {
      if (!joined)
      {
        instance->join();
      }
      joined = true;
    }

  protected:
    std::pair<void*, uintptr_t>
    allocate_shared_inner(size_t size, size_t alignment) override
    {
      auto aligned_shmem_offset =
        ((shmem_offset + alignment - 1) / alignment) * alignment;
      if (
        aligned_shmem_offset >= shmem_offset &&
        aligned_shmem_offset + size > aligned_shmem_offset &&
        aligned_shmem_offset + size < SHMEM_SIZE)
      {
        memset(shmem_base + aligned_shmem_offset, 0, size);
        auto result = std::make_pair(
          shmem_base + aligned_shmem_offset,
          shmem_guest_base + aligned_shmem_offset);
        shmem_offset = aligned_shmem_offset + size;
        return result;
      }
      else
      {
        throw std::runtime_error(
          "No enough enclave shared memory for allocation.");
      }
    }

    friend class EnclavePlatform<InitializerTuple>;
  };

#endif

  template<typename T>
  std::unique_ptr<EnclavePlatform<T>> EnclavePlatform<T>::create(
    EnclaveType type, const std::string& path, size_t num_threads)
  {
    if (!std::filesystem::exists(path))
    {
      throw std::logic_error(fmt::format("No enclave file found at {}", path));
    }
    switch (type)
    {
      case EnclaveType::QEMU:
#ifdef MONZA_HOST_SUPPORTS_QEMU
        return std::unique_ptr<EnclavePlatform<T>>(
          new QemuEnclavePlatform<T>(type, path, num_threads));
#else
        throw std::logic_error(
          "QEMU Monza enclaves are not supported in current build");
#endif // MONZA_HOST_SUPPORTS_QEMU
      case EnclaveType::HCS:
#ifdef MONZA_HOST_SUPPORTS_HCS
        return std::unique_ptr<EnclavePlatform<T>>(
          new HcsEnclavePlatform<T>(type, path, num_threads, false));
#else
        throw std::logic_error(
          "HCS Monza enclaves are not supported in current build");
#endif // MONZA_HOST_SUPPORTS_HCS
      case EnclaveType::HCS_ISOLATED:
#ifdef MONZA_HOST_SUPPORTS_HCS
        return std::unique_ptr<EnclavePlatform<T>>(
          new HcsEnclavePlatform<T>(type, path, num_threads, true));
#else
        throw std::logic_error(
          "HCS Isolated Monza enclaves are not supported in current build");
#endif // MONZA_HOST_SUPPORTS_HCS
    }
  }
}
