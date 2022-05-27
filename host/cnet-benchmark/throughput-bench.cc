// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#include <iostream>
#include <unistd.h>
#include <chrono>
#include <sys/mman.h>

#include <uv.h>

//
// Benchmark constants
//

// hardcoded IP and port of the server
static constexpr char SERVER_IP[] = "127.0.0.1";
static constexpr uint16_t SERVER_PORT = 9000;

static constexpr int DEFAULT_BATCH_SIZE = 1;

static constexpr bool DEFAULT_PING_PONG_ENABLED = true;
static constexpr bool DEFAULT_FLOOD_ENABLED = false;
static constexpr bool DEFAULT_PING_PONG_FLOOD_ENABLED = false;

static constexpr uint8_t RETURN_COUNTER_MAGIC = 0xff;
static constexpr uint8_t PONG_MAGIC = 0x00;
static constexpr uint8_t FLOOD_MAGIC = 0x01;

//
// Benchmark parameters
//

// different benchmarking approaches
static bool PING_PONG_ENABLED = false;
static bool FLOOD_ENABLED = false;
static bool PING_PONG_FLOOD_ENABLED = false;

// workers should increment this to use 12000 + id
static uint16_t CLIENT_PORT = 12000;

// default length of the benchmark in seconds
static double BENCHMARK_LENGTH = 20;

// how many packets each client maintains "in flight"
static int BATCH_SIZE = DEFAULT_BATCH_SIZE;

// maximum number of clients
static constexpr uint16_t MAX_CLIENT_ID = 100;
// client id is used to (1) identify the lead client (id = 0) and (2) send
// results to the lead client via local_cnet_benchmark_out[id]
static uint16_t CLIENT_ID = 0;

static bool DEBUGGING_ENABLED = false;

// size of packets sent by each client
static int PKT_SIZE = 0;

//
// Benchmark internal state
//

bool running = false;

uv_buf_t STATIC_BUF;
static struct sockaddr_in SERVER_ADDRESS;

static uv_udp_t client;

uv_idle_t send_pkt;
uv_idle_t idle_sync;

// how many packets sent by this client since the beginning of the benchmark
static uint64_t client_counter = 0;
static uint64_t server_counter = 0;

// timestamp taken at the beginning of the benchmark, used only by client 0
static std::chrono::time_point<std::chrono::steady_clock> timestamp;

// final length of the benchmark, used only by client 0
std::chrono::duration<double, std::milli> final_benchmark_length;

// struct used by clients to output their results
// aggregated by client 0
struct cnet_benchmark_out
{
  uint64_t client_counter;
  uint64_t server_counter;
};

//
// pointers to shared memory
//

// output struct for this client
static struct cnet_benchmark_out *local_cnet_benchmark_out;

// syncronization byte
static volatile uint8_t *sync_byte;

//
// Shared memory implementation
//

class SharedMemory
{
  static constexpr char const *memfile_name = "/cnet_benchmark_shmem";
  static constexpr const size_t size = sizeof(uint8_t) + MAX_CLIENT_ID * sizeof(struct cnet_benchmark_out);
  volatile uint8_t* ptr;

public:
  SharedMemory()
  {
    int shmid = shm_open(memfile_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (shmid < 0)
    {
      perror("shm_open");
      std::cerr << "[E] Unable to open shared memory." << std::endl;
      exit(1);
    }

    if (ftruncate(shmid, size) == -1)
    {
      perror("ftruncate");
      std::cerr << "[E] Unable to set shared memory size." << std::endl;
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
    if (CLIENT_ID == 0)
      remove(memfile_name);
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

//
// Client coordination routines
//

/**
 * @brief check if the benchmark should end now.
 *
 * @return true if it ended, false otherwise.
 */
static inline bool check_end(void)
{
  if (!running)
    return true;

  if (CLIENT_ID == 0)
  {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> l = now - timestamp;

    // benchmark length is in seconds
    if ((double)l.count() >= BENCHMARK_LENGTH * 1000)
    {
      // signal the end of the benchmark
      *sync_byte = 0;
      running = false;

      final_benchmark_length = l;

      local_cnet_benchmark_out->client_counter = client_counter;
      local_cnet_benchmark_out->server_counter = server_counter;

      if (PING_PONG_ENABLED || PING_PONG_FLOOD_ENABLED)
      {
        uv_stop(uv_default_loop());
      }
      else
      {
        uv_idle_stop(&send_pkt);

        // request counter from the application
        std::cout << "[I] Done, retrieving data from the guest now." << std::endl << std::endl;

        // wait a second for the server to flush its RX queue,
        // we don't want this packet to be dropped
        sleep(1);

        *((uint8_t*)STATIC_BUF.base) = RETURN_COUNTER_MAGIC;

        if (uv_udp_try_send(&client, &STATIC_BUF, 1, (const sockaddr *) &SERVER_ADDRESS) < 0)
        {
          std::cerr << "[W] Failed to send counter get packet." << std::endl;
        }
      }

      return true;
    }
  }
  else if (!*sync_byte)
  {
    running = false;
    local_cnet_benchmark_out->client_counter = client_counter;
    local_cnet_benchmark_out->server_counter = server_counter;
    uv_stop(uv_default_loop());
    return true;
  }

  return false;
}

static inline void coordinate_start(void)
{
  if (!running)
  {
    if (CLIENT_ID == 0)
    {
      timestamp = std::chrono::steady_clock::now();
      // signal the start of the benchmark
      *sync_byte = 1;
    }
    else
    {
      // spin until main process signals
      while (!*sync_byte)
        continue;
    }

    running = true;
  }
}

//
// libuv callbacks

static void ping_pong_on_uv_send(uv_udp_send_t *req, int status)
{
  coordinate_start();

  if (DEBUGGING_ENABLED)
    std::cout << "[D] Sent packet to " << SERVER_IP << ":" << SERVER_PORT << std::endl;

  server_counter++;

  delete req;
}

static void ping_pong_on_uv_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
  // Free early, we don't do anything with the data anyways.
  if (buf->len)
    free(buf->base);

  if(!nread && !addr)
  {
    // No more data to read.
    return;
  }

  if (DEBUGGING_ENABLED)
  {
    uint16_t server_port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    char server_ip_txt[16];
    uv_ip4_name((const struct sockaddr_in *)addr, server_ip_txt, 16);
    std::cout << "[D] Read packet from " << server_ip_txt << ":" << server_port << std::endl;
  }

  client_counter++;

  if(check_end())
    return;

  if (uv_udp_try_send(&client, &STATIC_BUF, 1, (const sockaddr *) &SERVER_ADDRESS) < 0)
  {
    std::cerr << "[E] Fatal, failed to send packet to server." << std::endl;
    exit(1);
  }

  if (DEBUGGING_ENABLED)
    std::cout << "[D] Sent packet to " << SERVER_IP << ":" << SERVER_PORT << std::endl;
}

static void flood_on_uv_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
  if (!*sync_byte && buf->len > 0)
  {
    // get the counter from the guest
    uint64_t cnt = *((uint64_t*)buf->base);
    if (cnt)
    {
      local_cnet_benchmark_out->client_counter = cnt;
      uv_stop(uv_default_loop());
      return;
    }
  }

  // Free early, we don't do anything with the data anyways.
  if (buf->len)
    free(buf->base);

  if(check_end())
    return;

  if(!nread && !addr)
  {
    // No more data to read.
    return;
  }

  if (DEBUGGING_ENABLED)
  {
    uint16_t server_port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    char server_ip_txt[16];
    uv_ip4_name((const struct sockaddr_in *)addr, server_ip_txt, 16);
    std::cout << "[D] Read packet from " << server_ip_txt << ":" << server_port << std::endl;
  }
}

// similar to ping_pong_on_uv_read but we don't reply
static void ping_pong_flood_on_uv_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags)
{
  // Free early, we don't do anything with the data anyways.
  if (buf->len)
    free(buf->base);

  if(!nread && !addr)
  {
    // No more data to read.
    return;
  }

  if (DEBUGGING_ENABLED)
  {
    uint16_t server_port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    char server_ip_txt[16];
    uv_ip4_name((const struct sockaddr_in *)addr, server_ip_txt, 16);
    std::cout << "[D] Read packet from " << server_ip_txt << ":" << server_port << std::endl;
  }

  client_counter++;

  if(check_end())
    return;
}

//
// CNet benchmark main
//

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
  buf->base = static_cast<char*>(malloc(suggested_size));
  if (!buf->base)
  {
    std::cerr << "[E] Failed to allocate libuv buffer, OOM!" << std::endl;
    buf->len = 0;
    return;
  }

  buf->len = suggested_size;
}

static bool udp_bind(uint16_t port, bool debug = true)
{
  if (uv_udp_init(uv_default_loop(), &client))
  {
    if (debug)
      std::cerr << "[W] Failed call to uv_udp_init when binding to " << port << std::endl;
    return false;
  }

  struct sockaddr_in recv_addr;
  uv_ip4_addr("0.0.0.0", port, &recv_addr);

  if (uv_udp_bind(&client, (const struct sockaddr *)&recv_addr, 0))
  {
    if (debug)
      std::cerr << "[W] Failed call to uv_udp_bind when binding to " << port << std::endl;
    return false;
  }

  auto on_read_func = ping_pong_on_uv_read;
  if (FLOOD_ENABLED)
    on_read_func = flood_on_uv_read;
  else if (PING_PONG_FLOOD_ENABLED)
    on_read_func = ping_pong_flood_on_uv_read;

  if (uv_udp_recv_start(&client, alloc_buffer, on_read_func))
  {
    if (debug)
      std::cerr << "[W] Failed call to uv_udp_recv_start when binding to " << port << std::endl;
    return false;
  }

  return true;
}

static void flood_function_dbg(uv_idle_t *handle)
{
  int failed = 0;

  for (int i = 0; i < BATCH_SIZE; i++)
  {
    // don't check every round to limit overhead
    if (i % 10 == 0 && check_end())
      break;

    if (uv_udp_try_send(&client, &STATIC_BUF, 1, (const sockaddr *) &SERVER_ADDRESS) < 0)
      failed++;
    else
      std::cout << "[D] Sent packet to " << SERVER_IP << ":" << SERVER_PORT << std::endl;
  }

  server_counter += BATCH_SIZE - failed;
}

static void flood_function(uv_idle_t *handle)
{
  int failed = 0;

  for (int i = 0; i < BATCH_SIZE; i++)
  {
    // don't check every round to limit overhead
    if (i % 10 == 0 && check_end())
      break;

    if (uv_udp_try_send(&client, &STATIC_BUF, 1, (const sockaddr *) &SERVER_ADDRESS) < 0)
      failed++;
  }

  server_counter += BATCH_SIZE - failed;
}

static void flood_sync_function(uv_idle_t *handle)
{
  coordinate_start();
  uv_idle_stop(&idle_sync);

  int rc;
  if (DEBUGGING_ENABLED)
    rc = uv_idle_start(&send_pkt, flood_function_dbg);
  else
    rc = uv_idle_start(&send_pkt, flood_function);

  if(rc)
  {
    std::cerr << "[E] Failed to start flood function." << std::endl;
    exit(1);
  }
}

void usage(char *exec)
{
  std::cout << "Usage: " << exec << " [-P/-F] [-d] [-b <batch size>] [-m <id>] [-l <length in seconds>] -s <packet size>"<< std::endl;
  std::cout << "Mandatory parameters:" << std::endl;
  std::cout << "      -s : Specify packet size (integer > 0, in Bytes)" << std::endl;
  std::cout << "Mode parameters (mutually exclusive):" << std::endl;
  std::cout << "      -P : Ping-Pong (synchronous) benchmark (default: " << DEFAULT_PING_PONG_ENABLED << ")" << std::endl;
  std::cout << "      -T : Ping-Pong (async flood) benchmark (default: " << DEFAULT_PING_PONG_FLOOD_ENABLED << ")" << std::endl;
  std::cout << "      -F : Flood benchmark (default: " << DEFAULT_FLOOD_ENABLED << ")" << std::endl;
  std::cout << "Optional parameters:" << std::endl;
  std::cout << "      -m : Enable multiprocess mode (default disabled)" << std::endl;
  std::cout << "           The user starts multiple clients, each passed -m <id>." << std::endl;
  std::cout << "           For each client, passed id must be unique and within [0, " << MAX_CLIENT_ID << "]." << std::endl;
  std::cout << "           The last client started should have id 0: it is the coordinating" << std::endl;
  std::cout << "           process that starts the benchmark." << std::endl;
  std::cout << "      -b : Specify batch size (integer > 0, default: " << DEFAULT_BATCH_SIZE << ")" << std::endl;
  std::cout << "      -l : Length of the benchmark in seconds (integer > 0, default: " << BENCHMARK_LENGTH << "s)" << std::endl;
  std::cout << "           Make sure to choose something 'high enough' to obtain reliable results." << std::endl;
  std::cout << "      -d : Enable debugging (default: " << DEBUGGING_ENABLED << ")" << std::endl;
}

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    std::cerr << "[E] Invalid number of arguments." << std::endl;
    usage(argv[0]);
    return 1;
  }

  int c;
  while ((c = getopt (argc, argv, "PTFds:b:m:l:")) != -1) {
    switch (c)
    {
      case 's':
        if (optarg[0] == '-') {
          std::cerr << "Invalid value passed to -s." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        try {
          PKT_SIZE = std::atoi(optarg);
        } catch (...) {
          std::cerr << "Invalid value passed to -s (" << optarg << ")." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        if (PKT_SIZE <= 0)
        {
          std::cerr << "Packet size must be > 0." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        break;
      case 'm':
        if (optarg[0] == '-') {
          std::cerr << "Invalid value passed to -m." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        try {
          CLIENT_ID = std::atoi(optarg);
        } catch (...) {
          std::cerr << "Invalid value passed to -m (" << optarg << ")." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        if (CLIENT_ID < 0 || CLIENT_ID > MAX_CLIENT_ID)
        {
          std::cerr << "Client id must be within [0, " << MAX_CLIENT_ID << "]." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        break;
      case 'l':
        if (optarg[0] == '-') {
          std::cerr << "Invalid value passed to -l." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        try {
          BENCHMARK_LENGTH = std::atoi(optarg);
        } catch (...) {
          std::cerr << "Invalid value passed to -l (" << optarg << ")." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        if (BENCHMARK_LENGTH < 1)
        {
          std::cerr << "Benchmark length must be > 1." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        break;
      case 'b':
        if (optarg[0] == '-') {
          std::cerr << "Invalid value passed to -b." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        try {
          BATCH_SIZE = std::atoi(optarg);
        } catch (...) {
          std::cerr << "Invalid value passed to -b (" << optarg << ")." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        if (BATCH_SIZE <= 0)
        {
          std::cerr << "Number of clients must be > 0." << std::endl;
          usage(argv[0]);
          return EXIT_FAILURE;
        }
        break;
      case 'd':
        DEBUGGING_ENABLED = true;
        break;
      case 'F':
        FLOOD_ENABLED = true;
        break;
      case 'P':
        PING_PONG_ENABLED = true;
        break;
      case 'T':
        PING_PONG_FLOOD_ENABLED = true;
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

  if (PING_PONG_ENABLED + FLOOD_ENABLED + PING_PONG_FLOOD_ENABLED > 1)
  {
    std::cerr << "[E] Only one mode can be enabled at a same time." << std::endl;
    return 1;
  }

  if (!PING_PONG_ENABLED && !FLOOD_ENABLED && !PING_PONG_FLOOD_ENABLED)
  {
    PING_PONG_ENABLED = DEFAULT_PING_PONG_ENABLED;
    FLOOD_ENABLED = DEFAULT_FLOOD_ENABLED;
    PING_PONG_FLOOD_ENABLED = !DEFAULT_PING_PONG_FLOOD_ENABLED;
  }

  if (DEBUGGING_ENABLED)
    std::cout << "[I] Opening shared memory..." << std::endl;

  SharedMemory shmem;

  sync_byte = shmem.get_ptr();
  local_cnet_benchmark_out = (struct cnet_benchmark_out *) (shmem.get_ptr() + sizeof(uint8_t) + CLIENT_ID * sizeof(struct cnet_benchmark_out));

  if (DEBUGGING_ENABLED)
    std::cout << "[I] Setting up event loop..." << std::endl;

  int rc = uv_udp_init(uv_default_loop(), &client);

  if (rc)
  {
    std::cerr << "[E] Failed to allocate loop (OOM?)" << std::endl;
    return 1;
  }

  // Make sure that the server is up and running.
  if (udp_bind(SERVER_PORT, false))
  {
    std::cerr << "[E] No server listening to " << SERVER_PORT << "." << std::endl;
    return 1;
  }

  // make sure that all clients use a different port
  CLIENT_PORT += CLIENT_ID;

  if (!udp_bind(CLIENT_PORT))
  {
    std::cerr << "[E] Failed to bind." << std::endl;
    return 1;
  }

  uv_ip4_addr(SERVER_IP, SERVER_PORT, &SERVER_ADDRESS);

  void *_buffer = calloc(1, PKT_SIZE);

  if (!_buffer)
  {
    std::cerr << "Unable to allocate packet buffer (OOM?)" << std::endl;
    return 1;
  }

  // set magic numbers to get the correct behaviour from the guest
  if (PING_PONG_ENABLED || PING_PONG_FLOOD_ENABLED)
    *((uint8_t*)_buffer) = PONG_MAGIC;
  else if (FLOOD_ENABLED)
    *((uint8_t*)_buffer) = FLOOD_MAGIC;

  STATIC_BUF = uv_buf_init((char *)_buffer, PKT_SIZE);

  if (PING_PONG_ENABLED)
  {
    for (int i = 0; i < BATCH_SIZE; i++)
    {
      // here we really want to use the uv_udp_send API as we use the callback to synchronize clients
      uv_udp_send_t *req = new uv_udp_send_t;
      if (uv_udp_send(req, &client, &STATIC_BUF, 1, (const sockaddr *) &SERVER_ADDRESS, ping_pong_on_uv_send))
      {
        std::cerr << "[W] Failed to send packet for batch element #" << i << "." << std::endl;
        return 1;
      }
    }
  }
  else if (FLOOD_ENABLED || PING_PONG_FLOOD_ENABLED)
  {
    uv_idle_init(uv_default_loop(), &send_pkt);

    uv_idle_init(uv_default_loop(), &idle_sync);

    if(uv_idle_start(&idle_sync, flood_sync_function))
    {
      std::cerr << "[E] Failed to start flood function." << std::endl;
      return 1;
    }
  }

  std::cout << std::endl;

  if (CLIENT_ID == 0)
  {
    std::cout << "Benchmark length: " << BENCHMARK_LENGTH << "s" << std::endl;
    std::cout << "Batch size: " << BATCH_SIZE << " packet(s)" << std::endl;
    std::cout << "Packet size: " << PKT_SIZE << " Byte" << std::endl << std::endl;
    std::cout << "[I] Starting benchmark." << std::endl << std::endl;
  }
  else if (DEBUGGING_ENABLED)
  {
    std::cout << "[I] Starting benchmark." << std::endl << std::endl;
  }

  rc = uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  // if we are the coordinating process, output a performance report
  if (CLIENT_ID == 0)
  {
    // make sure that all workers finished and output their results, probably overkill
    sleep(1);

    if (PING_PONG_ENABLED)
    {
      std::cout << "=============================================================" << std::endl;
      std::cout << "Client ID.\t| Throughput (pkt/s)\t| Throughput (Gbit/s)" << std::endl;
    }
    else if (FLOOD_ENABLED || PING_PONG_FLOOD_ENABLED)
    {
      std::cout << "===============================================================================" << std::endl;
      std::cout << "Throughput (pkt/s)\t| Throughput (Gbit/s)\t| Loss (pkt/s)\t| Loss (Gbit/s)" << std::endl;
    }

    double total_tx_gbps = 0;
    double total_tx_pkts = 0;
    double total_loss_tx_gbps = 0;
    double total_loss_tx_pkts = 0;

    int no_clients = 0;

    std::cout.precision(3); /* precision at 3 decimals */
    for (int client = 0; client < MAX_CLIENT_ID + 1; client++)
    {
      struct cnet_benchmark_out *out = (struct cnet_benchmark_out *) (shmem.get_ptr() + sizeof(uint8_t) + client * sizeof(struct cnet_benchmark_out));

      if (out->client_counter == 0 && out->server_counter == 0)
        continue;

      no_clients++;

      double tx_pkts =
        (    1000                                    /* ms -> s */
          *  (double)(out->client_counter)           /* how many packets the client sent */
        ) / (
             (double) final_benchmark_length.count() /* how much time we took in ms */
        );
      total_tx_pkts += tx_pkts;
      double tx_gbps =
        (    8          /* Byte -> bit */
          *  tx_pkts    /* packets per second */
          *  PKT_SIZE   /* Bytes per packet */
        ) / (
            1000000000  /* bit -> Gbit */
        );
      total_tx_gbps += tx_gbps;

      if (out->server_counter > out->client_counter)
      {
        double loss_tx_pkts =
          (    1000                                                /* ms -> s */
            *  (double)(out->server_counter - out->client_counter) /* how many packets the client sent */
          ) / (
               (double) final_benchmark_length.count()             /* how much time we took in ms */
          );
        total_loss_tx_pkts += loss_tx_pkts;
        double loss_tx_gbps =
          (    8            /* Byte -> bit */
            *  loss_tx_pkts /* packets per second */
            *  PKT_SIZE     /* Bytes per packet */
          ) / (
              1000000000  /* bit -> Gbit */
          );
        total_loss_tx_gbps += loss_tx_gbps;
      }

      if (PING_PONG_ENABLED)
      {
        std::cout << client << "\t\t| "
                  << std::fixed << tx_pkts << "\t\t| "
                  << std::fixed << tx_gbps << std::endl;
      }
    }

    if (PING_PONG_ENABLED)
      std::cout << "TOTAL" << "\t\t| ";

    std::cout << std::fixed << total_tx_pkts << "\t\t| "
              << std::fixed << total_tx_gbps;

    if (FLOOD_ENABLED || PING_PONG_FLOOD_ENABLED)
      std::cout << "\t\t| " << std::fixed << total_loss_tx_pkts << "\t\t| "
                << std::fixed << total_loss_tx_gbps;

    std::cout << std::endl;

    std::cout << std::endl << "~> " << no_clients << " clients" << std::endl;
  }

  std::cout << std::endl;

  if (rc && *sync_byte)
  {
    std::cerr << "[W] Event loop returned with error code " << rc << ", this looks like a failure." << std::endl;
  }

  std::cout << "[I] Exiting." << std::endl;

  // shared memory should get deleted automatically by the main process

  return 0;
}
