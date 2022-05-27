// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <spawn.h>
#include <sstream>
#include <cstring>
#include <signal.h>
#include <algorithm>
#include <list>
#include <map>

#include <uv.h>

// Tells the shared headers that we are the host;
// e.g., don't try to use Monza kabort()
#define CNET_HOST

// CNet API header
#include <cnet_api.h>

// modified CCF ring buffer header
#include <ring_buffer.h>

#define GUEST_LOG_PATH "/tmp/guest.log"
#define SHM_FILE_NAME "/cnet_shmem"

extern char **environ;

static pid_t qemu_guest_pid;
static bool done = false, siginted = false;
static ringbuffer::Circuit circuit;

// force big endianness (network byte ordering)
// TODO this could probably be cleaner
static constexpr uint8_t _loopback_ip[] = {127, 0, 0, 1};
static uint32_t loopback_ip = * (uint32_t*) _loopback_ip;

static bool KVM_ENABLED = false;
static bool GDB_ENABLED = false;
static bool BENCHMARK_MODE_ENABLED = false;
static bool PING_ON_BIND_ENABLED = false;
static bool HEAVY_DEBUG = false;

static constexpr int MIN_NUMBER_QEMU_CORES = 2;
static int NUMBER_QEMU_CORES = MIN_NUMBER_QEMU_CORES;

uv_loop_t *loop;

std::map<uint16_t, uv_udp_t*> bindMap;

static constexpr int ringbuffer_tx_pkt_processing_limit = 1;

class SharedMemory
{
  static constexpr char const *memfile_name = SHM_FILE_NAME;
  size_t size;
  volatile uint8_t* ptr;

public:
  // note: this should map values set in the guest, pagetable.cc for Monza.
  static constexpr uint64_t GUEST_GPA = (1UL << 39) /* max mappable address in QEMU */ -
                                        (64 * 1024 * 1024) /* size of the shared memory */;
  SharedMemory(size_t size) :
    size(size),
    ptr(nullptr)
  {
    int shmid = -1, retry = 0;
    while (shmid < 0 && retry < 5)
    {
      // if we fail here, this was a race condition with QEMU creating the file
      shmid = shm_open(memfile_name, O_RDWR, 0);
      retry++;
      if (shmid < 0)
        sleep(1);
    }

    if (shmid < 0)
    {
      // we really didn't manage to open this file
      perror("shm_open");
      std::cerr << "[E] Unable to open shared memory." << std::endl;
      exit(1);
    }

    ptr = (volatile uint8_t*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shmid, 0);

    if (ptr == MAP_FAILED)
    {
      perror("mmap");
      std::cerr << "[E] Unable to map shared memory." << std::endl;
      exit(1);
    }
  }

  SharedMemory(const SharedMemory&) = delete;

  ~SharedMemory()
  {
    munmap((void*) ptr, size);
    remove(SHM_FILE_NAME);
  }

  volatile uint8_t *get_ptr()
  {
    return ptr;
  }

  size_t get_size()
  {
    return size;
  }

  static const char *get_memfile_name()
  {
    return memfile_name;
  }
};

static inline void _benchmark_send_loop(cnet::UDPDataCommand *cmd,
  void *data_buffer, size_t send_iter)
{
  long long int i = 0;
  while (i != send_iter && !done)
  {
    bool success = circuit.write_to_inside().try_write(CNetMessageType,
      {(uint8_t*)cmd, cmd->size()},
      {(uint8_t*)data_buffer, cmd->get_data_length()}
    );
    if (success) { i++; }
  }

  if (done)
    exit(1);
}

static void benchmark_cnet_buffer(uint16_t port)
{
  size_t send_iter = 10000000;

  std::cout << "[I] Starting ring buffer benchmark." << std::endl;

  std::cout << std::endl << "    [I] Ring buffer size  : " << CNET_SHMEM_SINGLE_RINGBUFFER_SIZE << " Byte";
  std::cout << std::endl << "    [I] Packets per round : " << send_iter << " packets" << std::endl;

  std::cout.precision(3); /* precision at 3 decimals */

  std::list<int> sizes = {10, 100, 500, 1000, 1518, 2500, 5000, 10000, 20000, 30000};

  for (auto it = sizes.begin(); it != sizes.end(); ++it)
  {
    auto send_size = *it;
    std::cout << std::endl << "    ~ Packet size " << send_size << " Byte" << std::endl;

    cnet::UDPDataCommand cmd(0, 0, port, send_size);

    void *data_buffer = calloc(1, send_size);

    if (!data_buffer)
    {
      std::cerr << "[E] Unable to allocate send buffer, OOM." << std::endl;
      return;
    }

    // the guest should not answer
    *((uint8_t*) data_buffer) = 0x1;

    _benchmark_send_loop(&cmd, data_buffer, send_iter);

    std::cout << "    [I] Done with warmup round." << std::endl;

    for (int round = 0; round < 3; round++)
    {
      auto tb = std::chrono::steady_clock::now();
      _benchmark_send_loop(&cmd, data_buffer, send_iter);
      auto te = std::chrono::steady_clock::now();

      std::chrono::duration<double, std::milli> fp_ms = te - tb;
      auto ps = (    1000 /* ms -> s */
                  *  (double)(send_iter)   /* how many packets we sent */
                ) / (
                     (double)fp_ms.count() /* how much time we took in ms */
                );
      auto tx = (    8          /* Byte -> bit */
                  *  ps         /* packets per second */
                  *  send_size  /* Bytes per packet */
                ) / (
                    1000000000 /* bit -> Gbit */
                );
      auto mps = ps / 1000000; /* packets/second -> million packets/second */
      std::cout << "    [I] Round " << round + 1 << ": " << std::fixed
                << tx << " Gbit/s (" << std::fixed << mps << " million packets/s)" << std::endl;
    }

    free(data_buffer);
  }

  std::cout << std::endl;
}

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
  buf->base = static_cast<char*>(malloc(suggested_size));
  if (!buf->base)
  {
    std::cerr << "[E] Failed to allocate libuv buffer, OOM!" << std::endl;
    buf->len = 0;
    return;
  }

  ((uv_udp_send_t *)handle)->data = buf->base;

  buf->len = suggested_size;
}

/**
 * @brief Process incoming packets from the host network and transmit them to the guest.
 */
static void on_uv_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf,
  const struct sockaddr *addr, unsigned flags)
{
  if (nread <= 0)
  {
    if (!addr || nread < 0)
    {
      // !addr: no more data to read
      // nread < 0: failure
      if (buf->len)
        free(buf->base);
      return;
    }

    // nread = 0 and addr != NULL: empty datagram
    // -> continue processing
  }

  uint32_t ip_client = ((struct sockaddr_in *)addr)->sin_addr.s_addr;
  uint16_t port_client = ntohs(((struct sockaddr_in *)addr)->sin_port);

  struct sockaddr_in local;
  int xx = sizeof(local);
  uv_udp_getsockname(req, (struct sockaddr *) &local, &xx);
  uint16_t port_server = ntohs(local.sin_port);

  cnet::UDPDataCommand cmd(ip_client, port_client, port_server, nread);

  if (HEAVY_DEBUG)
  {
    char text_ip[16];
    uv_ip4_name((const struct sockaddr_in *) addr, text_ip, 16);
    std::cout << "[D] Received packet from the network (" << text_ip << ":"
              << port_client << " -> " << port_server << ", payload size "
              << cmd.get_data_length() << " Byte)." << std::endl;
  }

  bool rc;

  if (cmd.get_data_length())
  {
    rc = circuit.write_to_inside().try_write(CNetMessageType,
      {(uint8_t*)&cmd, cmd.size()},
      {(uint8_t*)buf->base, cmd.get_data_length()});
    free(buf->base);
  }
  else
  {
    // empty UDP packet
    rc = circuit.write_to_inside().try_write(CNetMessageType,
      {(uint8_t*)&cmd, cmd.size()});
  }

  if (!rc && HEAVY_DEBUG)
  {
    // unfortunately we couldn't write the packet and have to drop it
    std::cerr << "[E] Failed to write packet to the guest." << std::endl;
  }
}

static bool on_ringbuffer_udp_bind(uint16_t port)
{
  if (bindMap.find(port) != bindMap.end())
  {
    std::cerr << "[W] Guest already bound to " << port << std::endl;
    return false;
  }

  uv_udp_t *new_recv_socket = new uv_udp_t;

  if (uv_udp_init(loop, new_recv_socket))
  {
    std::cerr << "[W] Failed call to uv_udp_init when binding to " << port << std::endl;
    return false;
  }

  struct sockaddr_in recv_addr;
  uv_ip4_addr("0.0.0.0", port, &recv_addr);

  if (uv_udp_bind(new_recv_socket, (const struct sockaddr *)&recv_addr, 0))
  {
    std::cerr << "[W] Failed call to uv_udp_bind when binding to " << port << std::endl;
    return false;
  }

  if (uv_udp_recv_start(new_recv_socket, alloc_buffer, on_uv_read))
  {
    std::cerr << "[W] Failed call to uv_udp_recv_start when binding to " << port << std::endl;
    return false;
  }

  // register the new socket in the bind map
  bindMap[port] = new_recv_socket;

  if (PING_ON_BIND_ENABLED)
  {
    std::cout << "[I] Ping on bind enabled - sending a packet to the guest." << std::endl;

    // from ip and from port map to our own ip and the guest's port - if the
    // guest pings back there, it will receive its own packet back
    cnet::UDPDataCommand cmd(loopback_ip, port, port, 0);
    auto rc = circuit.write_to_inside().try_write(CNetMessageType,
      {(uint8_t*)&cmd, cmd.size()});

    if (!rc)
    {
      std::cerr << "[E] Failed to write ping packet to the guest." << std::endl;
      return false;
    }
  }

  return true;
}

static bool on_ringbuffer_udp_close(uint16_t port)
{
  auto it = bindMap.find(port);
  if (it == bindMap.end())
  {
    std::cerr << "[W] Guest not bound to " << port << std::endl;
    return false;
  }

  uv_udp_t *recv_socket = it->second;

  uv_udp_recv_stop(recv_socket);

  delete recv_socket;

  // update the bind map
  bindMap.erase(port);

  return true;
}

static bool on_ringbuffer_udp_data(uint32_t to_ip, uint16_t to_port, uint16_t from_port, ringbuffer::RawBuffer payload)
{
  auto it = bindMap.find(from_port);
  if (it == bindMap.end())
  {
    std::cerr << "[W] Guest not bound to requested output port " << from_port << std::endl;
    return false;
  }

  uv_udp_t *send_socket = it->second;

  uv_buf_t pkt;
  pkt.base = (char *) payload.data;
  pkt.len = payload.size;

  struct sockaddr_in send_addr;
  send_addr.sin_family = AF_INET;
  // make sure everything is in network byte order
  send_addr.sin_addr.s_addr = to_ip;
  send_addr.sin_port = htons(to_port);

  if (HEAVY_DEBUG)
  {
    char text_ip[16];
    uv_ip4_name(&send_addr, text_ip, 16);
    std::cout << "[D] Sending packet to " << text_ip << ":"
              << to_port << "." << std::endl;
  }

  if (uv_udp_try_send(send_socket, &pkt, 1, (const struct sockaddr *)&send_addr) < 0)
  {
    std::cerr << "[W] Failure in uv_udp_send sending UDP data." << std::endl;
    return false;
  }

  return true;
}

/**
 * @brief Process CNet packets from the guest's CNet RX ring buffer.
 */
static void process_cnet_tx(uv_idle_t *handle)
{
  // no buffer can be bigger than the ring buffer itself
  uint8_t header_buffer[cnet::Command::get_maximum_header_length()];

  circuit.read_from_inside().read(
    ringbuffer_tx_pkt_processing_limit,
    [&](ringbuffer::Message m, uint8_t* buf, size_t size) mutable {
      if (m != CNetMessageType)
      {
        std::cerr << "[W] Received message of incorrect type " << m
                  << " from guest." << LOG_ENDL;
        done = true;
        return;
      }

      memcpy(header_buffer, buf, std::min(size, std::size(header_buffer)));

      cnet::Command* parsed_header = cnet::Command::parse_raw_command(header_buffer, size);

      if (!parsed_header)
      {
        std::cerr << "[E] Received invalid/malicious CNet packet from the guest." << std::endl;
        done = true;
        return;
      }

      // at this point, the Command object has been sanitized

      switch (parsed_header->get_command_id()) {
        case cnet::UDPBindCommand::ID:
        {
          auto bind_cmd = static_cast<cnet::UDPBindCommand*>(parsed_header);

          std::cout << "[I] Received UDP Bind command for port "
                    << bind_cmd->get_port() << "." << std::endl;

          if (BENCHMARK_MODE_ENABLED)
          {
            benchmark_cnet_buffer(bind_cmd->get_port());
            done = true;
            break;
          }

          on_ringbuffer_udp_bind(bind_cmd->get_port());

          break;
        }
        case cnet::UDPCloseCommand::ID:
        {
          auto close_cmd = static_cast<cnet::UDPCloseCommand*>(parsed_header);
          std::cout << "[I] Received UDP Close command for port " << close_cmd->get_port() << "." << std::endl;

          on_ringbuffer_udp_close(close_cmd->get_port());

          break;
        }
        case cnet::UDPDataCommand::ID:
        {
          auto data_cmd = static_cast<cnet::UDPDataCommand*>(parsed_header);

          if (HEAVY_DEBUG)
          {
            std::cout << "[D] Received UDP Data command (" << (void*) buf << ") from port "
                      << data_cmd->get_server_port() << "." << std::endl;
          }

          ringbuffer::RawBuffer payload;
          payload.size = data_cmd->get_data_length();
          if (payload.size)
          {
            payload.data = buf + data_cmd->size();
          }

          on_ringbuffer_udp_data(data_cmd->get_client_ip(),
            data_cmd->get_client_port(), data_cmd->get_server_port(), payload);

          break;
        }
        default:
        {
          std::cerr << "[E] BUG! Received invalid message type that wasn't "
                    << "caught by the Message validator." << std::endl;
          break;
        }
      }
    });

  if (done)
  {
    // stop the loop
    std::cout << "[I] End signaled. Stopping the loop." << std::endl;
    uv_stop(loop);
  }
}

pid_t spawn_qemu(char *guest_path, const char *shmfile_name, uint64_t shmfile_size, uint64_t guest_gpa)
{
  pid_t pid;
  int status;

  std::stringstream ss, ss2, ss3;
  ss << "memory-backend-file,id=shmem,share=on,size=" << shmfile_size
     << ",mem-path=/dev/shm/" << shmfile_name;
  char* qemu_obj_str = new char[ss.str().length()+1];
  ss >> qemu_obj_str;

  ss2 << "pc-dimm,memdev=shmem,addr=" << guest_gpa;
  char* qemu_dimm_str = new char[ss2.str().length()+1];
  ss2 >> qemu_dimm_str;

  ss3 << "cores=" << NUMBER_QEMU_CORES;
  char* cores = new char[ss3.str().length()+1];
  ss3 >> cores;

  std::vector<char const*> argv_vector {"/usr/bin/qemu-system-x86_64", "-no-reboot",
    "-nographic", "-smp", cores, "-m", "1G,slots=2,maxmem=1T", "-object",
    qemu_obj_str, "-device", qemu_dimm_str, "-kernel", guest_path};

  if (KVM_ENABLED)
  {
    argv_vector.push_back("-enable-kvm");
    argv_vector.push_back("-cpu");
    argv_vector.push_back("host,+invtsc");
  }
  else
  {
    argv_vector.push_back("-cpu");
    argv_vector.push_back("IvyBridge");
  }

  if (GDB_ENABLED)
  {
    argv_vector.push_back("-S");
    argv_vector.push_back("-s");
  }

  argv_vector.push_back(NULL);

  auto arg_v = &argv_vector[0];

  if (HEAVY_DEBUG)
  {
    std::cout << " - [D] Calling as following:" << std::endl << "   " << arg_v[0];
    for (int i = 1; arg_v[i] != NULL; i++) {
      std::cout << " " << arg_v[i];
    }
    std::cout << std::endl;
  }

  posix_spawn_file_actions_t action;
  posix_spawn_file_actions_init(&action);
  posix_spawn_file_actions_addclose(&action, 0);
  posix_spawn_file_actions_addopen(&action, 1, GUEST_LOG_PATH, O_CREAT|O_WRONLY|O_APPEND, S_IRWXU);
  posix_spawn_file_actions_adddup2(&action, 1, 2);

  status = posix_spawn(&pid, arg_v[0], &action, NULL, const_cast<char **>(arg_v), environ);
  posix_spawn_file_actions_destroy(&action);
  delete[] qemu_obj_str;
  delete[] qemu_dimm_str;

  if (status != 0) {
    errno = status;
    perror("posix_spawn");
    std::cerr << "[E] Failed to spawn QEMU guest." << std::endl;
    return -1;
  }

  return pid;
}

void SIGCHLDhandler(int sig, siginfo_t *info, void *ucontext)
{
  // Note here: SA_NOCLDWAIT is set, so no need to wait() on children to
  // reap them.

  if (info->si_code == CLD_TRAPPED || info->si_code == CLD_STOPPED ||
    info->si_code == CLD_CONTINUED) {
    // ignore these (they shouldn't come because of
    // SA_NOCLDSTOP though)
    return;
  }

  if (qemu_guest_pid == info->si_pid) {
    done = true;
  }
}

void SIGINThandler(int signum)
{
  siginted = true;
  done = true;
}

void usage(char *exec)
{
  std::cout << "Usage: " << exec << " [-k] [-b] [-P] [-D] [-G] [-c <number of cores>] <QEMU guest image>"<< std::endl;
  std::cout << "Optional parameters:" << std::endl;
  std::cout << "      -D : Enable host debugging output (default " << HEAVY_DEBUG << ")" << std::endl;
  std::cout << "      -k : Enable KVM (default " << KVM_ENABLED << ")" << std::endl;
  std::cout << "      -c <number of cores> : Set the number of cores passed to the guest (default " << MIN_NUMBER_QEMU_CORES;
  std::cout << ", minimum " << MIN_NUMBER_QEMU_CORES  << ")" << std::endl;
  std::cout << "      -G : Append GDB options (default " << GDB_ENABLED << ")" << std::endl;
  std::cout << "      -P : Ping on bind - send a packet to the guest just after binding (default " << PING_ON_BIND_ENABLED << ")" << std::endl;
  std::cout << "      -b : Benchmark the ring buffer (default " << BENCHMARK_MODE_ENABLED << ")" << std::endl;
  std::cout << "           Note: incompatible with -P." << std::endl;
}

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::cerr << "[E] Invalid number of arguments." << std::endl;
    usage(argv[0]);
    return 1;
  }

  int c;
  while ((c = getopt (argc, argv, "kbGPDc:")) != -1) {
    switch (c)
    {
      case 'G':
        GDB_ENABLED = true;
        break;
      case 'k':
        KVM_ENABLED = true;
        break;
      case 'b':
        BENCHMARK_MODE_ENABLED = true;
        break;
      case 'P':
        PING_ON_BIND_ENABLED = true;
        break;
      case 'D':
        HEAVY_DEBUG = true;
        break;
      case 'c':
        if (optarg[0] == '-') {
          std::cerr << "Invalid value passed to -c." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        try {
          NUMBER_QEMU_CORES = std::atoi(optarg);
        } catch (...) {
          std::cerr << "Invalid value passed to -c (" << optarg << ")." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        if (NUMBER_QEMU_CORES < MIN_NUMBER_QEMU_CORES)
        {
          std::cerr << "Number of guest cores must be > " << MIN_NUMBER_QEMU_CORES << "." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        break;
      case '?':
        if (isprint (optopt)) {
          std::cerr << "[E] Unknown option `-" << optopt << "'." << std::endl;
        } else {
          std::cerr << "[E] Unknown option character `" << optopt << "'." << std::endl;
        }
        /* intentional fall through */
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if (argc < optind + 1 /* a mandatory image path */) {
    std::cerr << "[E] Not enough arguments supplied; is the "
              << "guest image path missing?" << std::endl;
    usage(argv[0]);
    return 1;
  }

  if (BENCHMARK_MODE_ENABLED && PING_ON_BIND_ENABLED)
  {
    BENCHMARK_MODE_ENABLED = false;
    PING_ON_BIND_ENABLED = false;
    std::cerr << "[E] Benchmark mode and ping on bind are incompatible options." << std::endl;
    usage(argv[0]);
    return 1;
  }

  if (BENCHMARK_MODE_ENABLED)
  {
    std::cout << "[W] ! Important: this benchmark might require modifications      !" << std::endl;
    std::cout << "[W] ! in the guest. If you want results without processing, you  !" << std::endl;
    std::cout << "[W] ! need to manually disable the process_new_packet() when()   !" << std::endl;
    std::cout << "[W] ! block, or comment out the processing lambda in poll()      !" << std::endl;
    std::cout << "[W] ! altogether.                                                !" << std::endl;
    std::cout << std::endl;
  }

  // TODO in the KVM case, check that the proper bits are set for Monza to boot (regarding MSRs)

  char *guest_img_path = argv[optind];

  std::cout << "[I] Setting up event loop..." << std::endl;

  loop = uv_default_loop();

  if (!loop)
  {
    std::cerr << "[E] Failed to allocate loop (OOM?)" << std::endl;
    return 1;
  }

  // add CNet ring buffer TX handler to the loop
  uv_idle_t check_tx;
  uv_idle_init(loop, &check_tx);

  if(uv_idle_start(&check_tx, process_cnet_tx))
  {
    std::cerr << "[E] Failed to start TX handler." << std::endl;
    return 1;
  }

  std::cout << "[I] Cleaning stale run data..." << std::endl;

  remove(GUEST_LOG_PATH);
  remove(SHM_FILE_NAME);

  // we start QEMU first for it to create the shared memory file
  std::cout << "[I] Spawning QEMU/KVM guest..." << std::endl;

  qemu_guest_pid = spawn_qemu(guest_img_path, SharedMemory::get_memfile_name(),
    CNET_SHMEM_SIZE, SharedMemory::GUEST_GPA);

  if (qemu_guest_pid == -1)
    return 1;

  std::cout << "[I] Setting up shared memory..." << std::endl;

  SharedMemory sharedMemory(CNET_SHMEM_SIZE);

  std::cout << "[I] Setting up ring buffer..." << std::endl;

  // create Circuit object - describes in and out ring buffers
  circuit = cnet_build_circuit_from_base_address(sharedMemory.get_ptr());

  // zero-out offsets memory
  // we have to use fill_n and not memset because it support volatile targets
  std::fill_n((volatile uint8_t*) CNET_SHMEM_ADDRESS_OFFSET_OUT(sharedMemory.get_ptr()),
    sizeof(ringbuffer::Offsets), 0);
  std::fill_n((volatile uint8_t*) CNET_SHMEM_ADDRESS_OFFSET_IN(sharedMemory.get_ptr()),
    sizeof(ringbuffer::Offsets), 0);

  // zero-out ring buffers
  std::fill_n(CNET_SHMEM_ADDRESS_RING_OUT(sharedMemory.get_ptr()),
    CNET_SHMEM_SINGLE_RINGBUFFER_SIZE, 0);
  std::fill_n(CNET_SHMEM_ADDRESS_RING_IN(sharedMemory.get_ptr()),
    CNET_SHMEM_SINGLE_RINGBUFFER_SIZE, 0);

  std::cout << "[I] Registering handlers..." << std::endl;

  // Install signal handler to detect child death.
  struct sigaction sa;
  sa.sa_sigaction = SIGCHLDhandler;
  sigemptyset(&sa.sa_mask);
  /* SA_SIGINFO   = the handler should get a siginfo_t *
   * SA_NOCLDSTOP = we're only interested in children dying
   * SA_NOCLDWAIT = always reap children automatically */
  sa.sa_flags = SA_SIGINFO | SA_NOCLDSTOP | SA_NOCLDWAIT;
  if (sigaction(SIGCHLD, &sa, NULL) == -1)
  {
      std::cerr << "[E] Failed to install SIGCHLD handler" << std::endl;
      return 1;
  }

  signal(SIGINT, SIGINThandler);

  // This acts as a signal for the guest to actually start using the shared memory.
  cnet_write_host_magic_value(sharedMemory.get_ptr());

  if (!cnet_check_host_magic_value(sharedMemory.get_ptr()))
  {
    std::cerr << "[E] Failing to read what we just wrote: shared memory is not"
              << " sane." << std::endl;
    done = true;
    exit(1);
  }

  std::cout << "[I] All done setting up, guest logs under " << GUEST_LOG_PATH << "." << std::endl;

  // Start the event loop. This should only return when we're done.
  int rc = uv_run(loop, UV_RUN_DEFAULT);

  if (rc && !siginted)
  {
    std::cerr << "[W] Event loop returned with error code " << rc << ", this looks like a failure." << std::endl;
  }

  // kill QEMU if we reach this
  kill(qemu_guest_pid, SIGKILL);

  if (cnet_check_guest_magic_value(sharedMemory.get_ptr()))
  {
    std::cout << "[D] Guest wrote magic value; this looks like a sane exit." << std::endl;
  }
  else if (cnet_check_host_magic_value(sharedMemory.get_ptr()))
  {
    std::cout << "[D] Guest did not write magic value: early error or"
              << " issue with shared memory?" << std::endl;
  }
  else
  {
    std::cout << "[D] Guest did not write correct magic value ("
              << * (uint64_t*) sharedMemory.get_ptr()
              << "); this might signal a guest bug?" << std::endl;
  }

  std::cout << "[I] Exiting." << std::endl;
  return 0;
}
