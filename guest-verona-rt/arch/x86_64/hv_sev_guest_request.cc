#include <algorithm>
#include <hypervisor.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <sev.h>
#include <spinlock.h>
#include <type_traits>

namespace monza
{
  constexpr uint8_t GUEST_REQUEST_VMPCK = 0;

  enum class GuestRequestStatusCode : uint32_t
  {
    STATUS_SUCCESS = 0x00,
    ERROR_INVALID_PLATFORM_STATE = 0x01,
    ERROR_INVALID_GUEST_STATE = 0x02,
    ERROR_INVALID_CONFIG = 0x03,
    ERROR_INVALID_LENGTH = 0x04,
    ERROR_ALREADY_OWNED = 0x05,
    ERROR_INVALID_CERTIFICATE = 0x06,
    ERROR_POLICY_FAILURE = 0x07,
    ERROR_INACTIVE = 0x08,
    ERROR_INVALID_ADDRESS = 0x09,
    ERROR_BAD_SIGNATURE = 0x0A,
    ERROR_BAD_MEASUREMENT = 0x0B,
    ERROR_ASID_OWNED = 0x0C,
    ERROR_INVALID_ASID = 0x0D,
    ERROR_WBINVD_REQUIRED = 0x0E,
    ERROR_DF_FLUSH_REQUIRED = 0x0F,
    ERROR_INVALID_GUEST = 0x10,
    ERROR_INVALID_COMMAND = 0x11,
    ERROR_ACTIVE = 0x12,
    ERROR_HWERROR_PLATFORM = 0x13,
    ERROR_HWERROR_UNSAFE = 0x14,
    ERROR_UNSUPPORTED = 0x15,
    ERROR_INVALID_PARAM = 0x16,
    ERROR_RESOURCE_LIMIT = 0x17,
    ERROR_SECURE_DATA_INVALID = 0x18,
    // SNP-specific
    ERROR_INVALID_PAGE_SIZE = 0x19,
    ERROR_INVALID_PAGE_STATE = 0x1A,
    ERROR_INVALID_MDATA_ENTRY = 0x1B,
    ERROR_INVALID_PAGE_OWNER = 0x1C,
    ERROR_AEAD_OFLOW = 0x1D,
    // Missing 0x1E
    ERROR_RING_BUFFER_EXIT = 0x1F,
  };

  /**
   * Payload and properties for MSG_REPORT_REQ.
   */
  class ReportRequestPayload
  {
  public:
    constexpr static uint8_t TYPE = 5;
    constexpr static uint8_t VERSION = 1;
    constexpr static uint16_t SIZE = 0x60;
    constexpr static bool REQUEST = true;

  private:
    uint8_t user_data[0x40];
    uint32_t vmpl;
    uint8_t reserved[SIZE - 0x44];

  public:
    /**
     * Request constructor ensures that all fields initialized.
     */
    ReportRequestPayload(std::span<const uint8_t> user_data)
    : user_data(), vmpl(0), reserved()
    {
      if (std::size(user_data) > std::size(this->user_data))
      {
        LOG_MOD(ERROR, SNP)
          << "Requested user data of size " << std::size(user_data)
          << " does not fit attestation report." << LOG_ENDL;
        kabort();
      }
      std::copy(std::begin(user_data), std::end(user_data), this->user_data);
    }

    /**
     * Read-only byte view used by encryption routine to consume content.
     */
    std::span<const uint8_t> raw() const
    {
      return std::span(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
    }
  } __attribute__((packed));
  static_assert(sizeof(ReportRequestPayload) == ReportRequestPayload::SIZE);

  /**
   * Payload and properties for MSG_REPORT_RSP.
   */
  class ReportResponsePayload
  {
  public:
    constexpr static uint8_t TYPE = 6;
    constexpr static uint8_t VERSION = 1;
    constexpr static uint16_t SIZE = 0x20 + 0x4a0;
    constexpr static bool REQUEST = false;

  private:
    uint32_t status;
    uint32_t report_size;
    uint8_t reserved[0x18];
    uint8_t report[0x4a0];

  public:
    /**
     * Response constructor could chose to not initialize the fields as the data
     * will be filled using decryption, but we choose to do it for safety
     * against errors.
     */
    ReportResponsePayload() : status(0), report_size(0), reserved(), report() {}

    /**
     * Mutable byte view used by decrypt routine to populate content.
     */
    std::span<uint8_t> raw()
    {
      return std::span(reinterpret_cast<uint8_t*>(this), sizeof(*this));
    }

    /**
     * Access to the underlying report data up to the length specified by the
     * firmware.
     */
    std::span<const uint8_t> report_data()
    {
      return std::span<const uint8_t>(report, report_size);
    }
  } __attribute__((packed));
  static_assert(sizeof(ReportResponsePayload) == ReportResponsePayload::SIZE);

  /**
   * Payload and properties for MSG_TSC_INFO_REQ.
   */
  class TscRequestPayload
  {
  public:
    constexpr static uint8_t TYPE = 17;
    constexpr static uint8_t VERSION = 1;
    constexpr static uint16_t SIZE = 0x80;
    constexpr static bool REQUEST = true;

  private:
    uint8_t reserved[0x80];

  public:
    /**
     * Request constructor ensures that all fields initialized.
     */
    TscRequestPayload() : reserved() {}

    /**
     * Read-only byte view used by encryption routine to consume content.
     */
    std::span<const uint8_t> raw() const
    {
      return std::span(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
    }
  } __attribute__((packed));
  static_assert(sizeof(TscRequestPayload) == TscRequestPayload::SIZE);

  /**
   * Payload and properties for MSG_REPORT_RSP.
   */
  class TscResponsePayload
  {
  public:
    constexpr static uint8_t TYPE = 18;
    constexpr static uint8_t VERSION = 1;
    constexpr static uint16_t SIZE = 0x80;
    constexpr static bool REQUEST = false;

  private:
    uint32_t status;
    uint32_t reserved;
    uint64_t tsc_scale;
    uint64_t tsc_offset;
    uint32_t tsc_factor;
    uint8_t reserved2[0x64];

  public:
    /**
     * Response constructor could chose to not initialize the fields as the data
     * will be filled using decryption, but we choose to do it for safety
     * against errors.
     */
    TscResponsePayload()
    : status(0),
      reserved(),
      tsc_scale(0),
      tsc_offset(0),
      tsc_factor(0),
      reserved2()
    {}

    /**
     * Mutable byte view used by decrypt routine to populate content.
     */
    std::span<uint8_t> raw()
    {
      return std::span(reinterpret_cast<uint8_t*>(this), sizeof(*this));
    }

    TscState state()
    {
      return TscState{tsc_scale, tsc_offset, tsc_factor};
    }
  } __attribute__((packed));
  static_assert(sizeof(TscResponsePayload) == TscResponsePayload::SIZE);

  enum class GuestRequestAlgorithm : uint8_t
  {
    AES_256_GCM = 1,
  };

  enum class GuestRequestVersion : uint8_t
  {
    Current = 1,
  };

  /**
   * Guest request message templated with payload to correctly set header fields
   * and payload size.
   */
  template<typename PayloadType>
  class GuestRequestMessage
  {
    uint8_t auth_tag[0x20];
    uint64_t msg_seq_num;
    uint64_t reserved;
    GuestRequestAlgorithm algorithm;
    GuestRequestVersion header_version;
    uint16_t header_size;
    uint8_t message_type;
    uint8_t message_version;
    uint16_t message_size;
    uint32_t reserved2;
    uint8_t message_vmpck;
    uint8_t reserved3;
    uint16_t reserved4;
    uint8_t reserved5[0x20];
    uint8_t payload_array[PayloadType::SIZE];

  public:
    /**
     * Constructor for a request type payload which populates the header fields
     * as needed and initializes the payload to all zero. SFINAE disabled for
     * response type payloads.
     */
    template<
      typename PayloadType_ = PayloadType,
      typename = std::enable_if_t<PayloadType_::REQUEST == true>>
    GuestRequestMessage(uint64_t msg_seq_num)
    : auth_tag(),
      reserved(0),
      msg_seq_num(msg_seq_num),
      algorithm(GuestRequestAlgorithm::AES_256_GCM),
      header_version(GuestRequestVersion::Current),
      // This field includes everything up to the actual payload.
      header_size(offsetof(GuestRequestMessage<PayloadType>, payload_array)),
      message_type(PayloadType::TYPE),
      message_version(PayloadType::VERSION),
      message_size(PayloadType::SIZE),
      reserved2(0),
      message_vmpck(GUEST_REQUEST_VMPCK),
      reserved3(0),
      reserved4(0),
      reserved5(),
      payload_array()
    {
      static_assert(
        std::is_same<PayloadType_, PayloadType>::value,
        "Don't set SFINAE template parameter!");
    }

    /**
     * Constructor for a reponse type payload which copies the raw data from
     * shared memory into a structured type in guest memory. SFINAE disabled for
     * request type payloads.
     */
    template<
      size_t RawSize,
      typename PayloadType_ = PayloadType,
      typename = std::enable_if_t<PayloadType_::REQUEST == false>>
    GuestRequestMessage(std::span<const uint8_t, RawSize> raw_data)
    {
      static_assert(
        std::is_same<PayloadType_, PayloadType>::value,
        "Don't set SFINAE template parameter!");
      static_assert(RawSize >= sizeof(GuestRequestMessage));
      memcpy(this, raw_data.data(), sizeof(GuestRequestMessage));
    }

    /**
     * Read-only view to authentication data, which is the byte range [0x30 -
     * 0x5F] from the message.
     */
    std::span<const uint8_t> authentication_data() const
    {
      return std::span<const uint8_t>(
        snmalloc::pointer_offset<const uint8_t>(this, 0x30), 0x30);
    }

    /**
     * Mutable view to authentication tag populated during encryption and read
     * during decryption.
     */
    std::span<uint8_t> authentication_tag()
    {
      return std::span(auth_tag);
    }

    /**
     * Mutable view to payload data populated during encryption and read during
     * decryption.
     */
    std::span<uint8_t> payload()
    {
      return std::span(payload_array);
    }

    /**
     * Accessor for sequence data used to check response correctness.
     */
    uint64_t sequence_number() const
    {
      return this->msg_seq_num;
    }
  } __attribute__((packed));

  class GuestRequestCrypto
  {
    using sequence_number_t = uint64_t;
    constexpr static char CIPHER_NAME[] = "AES-256-GCM";
    EVP_CIPHER_CTX* ctx = nullptr;
    EVP_CIPHER* cipher = nullptr;
    UniqueArray<uint8_t> iv{};
    size_t tag_length = 0;

    /**
     * Init method instead of constructor to allow using with
     * snmalloc::Singleton.
     */
    static void init(GuestRequestCrypto* ptr) noexcept
    {
      ptr->ctx = EVP_CIPHER_CTX_new();
      if (ptr->ctx == nullptr)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to initialize encryption context for guest request."
          << LOG_ENDL;
        kabort();
      }

      ptr->cipher = EVP_CIPHER_fetch(nullptr, CIPHER_NAME, nullptr);
      if (ptr->cipher == nullptr)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to fetch cipher for guest request." << LOG_ENDL;
        kabort();
      }

      // OpenSSL does not support dynamic retrieval of tag length for
      // AES-GCM so hardcode it.
      ptr->tag_length = 16;
      size_t iv_length;
      OSSL_PARAM params[] = {
        OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_IVLEN, &iv_length),
        OSSL_PARAM_END};
      if (EVP_CIPHER_get_params(ptr->cipher, params) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to get IV length of cipher for guest request." << LOG_ENDL;
        kabort();
      }
      if (iv_length < sizeof(sequence_number_t))
      {
        LOG_MOD(ERROR, SNP)
          << "IV length of cipher too small for guest request." << LOG_ENDL;
        kabort();
      }
      ptr->iv = std::move(UniqueArray<uint8_t>(iv_length));
    }

  public:
    static GuestRequestCrypto& get()
    {
      return snmalloc::
        Singleton<GuestRequestCrypto, &GuestRequestCrypto::init>::get();
    }

    void encrypt(
      sequence_number_t sequence_number,
      std::span<const uint8_t> source,
      std::span<uint8_t> destination,
      std::span<const uint8_t> aad,
      std::span<uint8_t> tag)
    {
      if (source.size() != destination.size() || tag.size() < tag_length)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed encryption preconditions for guest request." << LOG_ENDL;
        kabort();
      }

      auto iv_span = std::span(iv);
      std::fill(iv_span.begin(), iv_span.end(), 0);
      memcpy(iv_span.data(), &sequence_number, sizeof(sequence_number_t));

      // Initialize encryption.
      if (
        EVP_EncryptInit_ex2(
          ctx,
          cipher,
          sev_secret_page->vmpck[GUEST_REQUEST_VMPCK],
          iv_span.data(),
          nullptr) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed encryption initialization for guest request." << LOG_ENDL;
        kabort();
      }

      // Specify AAD.
      int tmplen;
      if (EVP_EncryptUpdate(ctx, NULL, &tmplen, aad.data(), aad.size()) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to specify encryption AAD for guest request." << LOG_ENDL;
        kabort();
      }

      // Encrypt.
      if (
        EVP_EncryptUpdate(
          ctx,
          destination.data(),
          &tmplen,
          source.data(),
          static_cast<int>(source.size())) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed encryption for guest request." << LOG_ENDL;
        kabort();
      }

      // Finalize encryption.
      if (EVP_EncryptFinal_ex(ctx, destination.data(), &tmplen) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed encryption finalization for guest request." << LOG_ENDL;
        kabort();
      }

      // Get the authentication tag.
      OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(
          OSSL_CIPHER_PARAM_AEAD_TAG, tag.data(), tag_length),
        OSSL_PARAM_END};
      if (EVP_CIPHER_CTX_get_params(ctx, params) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to get authentication tag for guest request." << LOG_ENDL;
        kabort();
      }
    }

    void decrypt(
      sequence_number_t sequence_number,
      std::span<const uint8_t> source,
      std::span<uint8_t> destination,
      std::span<const uint8_t> aad,
      std::span<const uint8_t> tag)
    {
      if (source.size() != destination.size() || tag.size() < tag_length)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed encryption preconditions for guest request." << LOG_ENDL;
        kabort();
      }

      auto iv_span = std::span(iv);
      std::fill(iv_span.begin(), iv_span.end(), 0);
      memcpy(iv_span.data(), &sequence_number, sizeof(sequence_number_t));

      // Initialize decryption.
      if (
        EVP_DecryptInit_ex2(
          ctx,
          cipher,
          sev_secret_page->vmpck[GUEST_REQUEST_VMPCK],
          iv_span.data(),
          nullptr) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed decryption initialization for guest request." << LOG_ENDL;
        kabort();
      }

      // Specify AAD.
      int tmplen;
      if (EVP_DecryptUpdate(ctx, NULL, &tmplen, aad.data(), aad.size()) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to specify decryption AAD for guest request." << LOG_ENDL;
        kabort();
      }

      // Decrypt.
      if (
        EVP_DecryptUpdate(
          ctx,
          destination.data(),
          &tmplen,
          source.data(),
          static_cast<int>(source.size())) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed decryption for guest request." << LOG_ENDL;
        kabort();
      }

      // Set the authentication tag.
      // Cast away constness since parameter will only be read.
      OSSL_PARAM params[] = {OSSL_PARAM_construct_octet_string(
                               OSSL_CIPHER_PARAM_AEAD_TAG,
                               const_cast<uint8_t*>(tag.data()),
                               tag_length),
                             OSSL_PARAM_END};
      if (EVP_CIPHER_CTX_set_params(ctx, params) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed to set authentication tag for guest request." << LOG_ENDL;
        kabort();
      }

      // Finalize decryption.
      if (EVP_DecryptFinal_ex(ctx, destination.data(), &tmplen) == 0)
      {
        LOG_MOD(ERROR, SNP)
          << "Failed decryption finalization for guest request." << LOG_ENDL;
        kabort();
      }
    }
  };

  // Should not carry ownership like this, but these pages require the shared
  // memory allocator, so we apply an exception.
  static void* request_page;
  static void* response_page;

  // Guest requests and their responses must be strictly ordered so we serialize
  // them with a spinlock.
  static constinit Spinlock request_lock;

  /**
   * Sequence number of messages that needs to be strictly controlled to match
   * PSP. Needs to start from 1.
   */
  static uint64_t msg_seq_num = 1;

  static void sev_guest_request()
  {
    auto ghcb = get_ghcb();

    ghcb->suffix.format = SevFormat::BASE;
    ghcb->base.exit_code = SevExitCode::GUEST_REQUEST;
    ghcb->base.exit_info1 = snmalloc::address_cast(request_page);
    ghcb->base.exit_info2 = snmalloc::address_cast(response_page);
    ghcb->base.valid_bitmap = SevGhcbValidBitmapData::initial_guest();

    vmgexit();

    auto status = static_cast<GuestRequestStatusCode>(ghcb->base.exit_info2);
    if (status != GuestRequestStatusCode::STATUS_SUCCESS)
    {
      LOG_MOD(ERROR, SNP) << "Failed SEV-SNP guest request with exit code "
                          << status << "." << LOG_ENDL;
      kabort();
    }
  }

  /**
   * Templated helper for all types of guest requests.
   * Takes a read-only request payload and populates a mutable response payload.
   * Encapsulates all the crypto and state tracking.
   */
  template<typename RequestPayload, typename ResponsePayload>
  static void sev_typed_guest_request(
    const RequestPayload& request_payload, ResponsePayload& response_payload)
  {
    // Start of critical section.
    // Initial encryption needs to be in critical section, since sequence number
    // used as IV.
    ScopedSpinlock scoped_request_lock(request_lock);
    using Request = GuestRequestMessage<RequestPayload>;
    static_assert(sizeof(Request) <= HV_PAGE_SIZE);
    // Create a copy of the request in guest memory as it is used as input for
    // the autentication data.
    Request request(msg_seq_num);
    GuestRequestCrypto::get().encrypt(
      msg_seq_num,
      request_payload.raw(),
      request.payload(),
      request.authentication_data(),
      request.authentication_tag());
    // Copy finished request out to shared memory page.
    memcpy(request_page, &request, sizeof(Request));
    sev_guest_request();
    // Copy reponse from shared memory into guest memory.
    using Response = GuestRequestMessage<ResponsePayload>;
    static_assert(sizeof(Response) <= HV_PAGE_SIZE);
    Response response(std::span<const uint8_t>(
      static_cast<uint8_t*>(response_page), HV_PAGE_SIZE));
    // Track sequence number change. Response also increments sequence number.
    auto response_seq_num = response.sequence_number();
    if (response_seq_num != msg_seq_num + 1)
    {
      LOG_MOD(ERROR, SNP)
        << "Host responded with invalid sequence number to guest request."
        << LOG_ENDL;
      kabort();
    }
    msg_seq_num += 2;
    // End of critical section
    scoped_request_lock.release();
    GuestRequestCrypto::get().decrypt(
      response_seq_num,
      response.payload(),
      response_payload.raw(),
      response.authentication_data(),
      response.authentication_tag());
  }

  void setup_sev_guest_request()
  {
    request_page = allocate_visible(HV_PAGE_SIZE);
    response_page = allocate_visible(HV_PAGE_SIZE);
  }

  UniqueArray<uint8_t>
  generate_attestation_report_sev(std::span<const uint8_t> user_data)
  {
    ReportRequestPayload request_payload(user_data);
    ReportResponsePayload response_payload{};
    sev_typed_guest_request(request_payload, response_payload);
    return UniqueArray<uint8_t>(response_payload.report_data());
  }

  TscState get_current_tsc_state_sev()
  {
    TscRequestPayload request_payload{};
    TscResponsePayload response_payload{};
    sev_typed_guest_request(request_payload, response_payload);
    return response_payload.state();
  }
}
