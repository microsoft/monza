// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

/**
 * This file needs to mix C++20 and Win32 to access HostComputeServices APIs.
 * As such, the functionality is encapsulated in a source file to avoid
 * impacting other files. No other file needs to include "windows.h" or other
 * Win32 headers.
 */

#include <ComputeCore.h>
#include <aclapi.h>
#include <array>
#include <filesystem>
#include <format>
#include <functional>
#include <hcs_enclave.h>
#include <iostream>
#include <stdexcept>
#include <windows.h>

template<typename T>
using RaiiHandle = std::
  unique_ptr<typename std::remove_pointer<T>::type, std::function<void(T)>>;

/**
 * Convert HRESULT to system error message.
 * Uses Win32 naming scheme as it is helper for Win32.
 */
std::string GetErrorMessage(DWORD error_code)
{
  char character_array[1024];
  auto characters = FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE |
      FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    error_code,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    character_array,
    std::size(character_array),
    NULL);
  if (characters > 0)
  {
    return std::string(character_array, characters);
  }
  else
  {
    return std::format(
      "Failed to retrieve error message string for error {:#x}.",
      static_cast<uint32_t>(error_code));
  }
}

/**
 * Converts a GUID to a UNICODE string.
 * Uses Win32 naming scheme as it is helper for Win32.
 */
static std::wstring GuidToString(REFGUID guid)
{
  RPC_WSTR guid_string{};
  RPC_STATUS rpc_status = UuidToStringW(&guid, &guid_string);
  if (rpc_status != RPC_S_OK)
  {
    throw std::runtime_error("Out of memory when converting GUID to string.");
  }
  try
  {
    std::wstring result(reinterpret_cast<LPCWSTR>(guid_string));
    RpcStringFreeW(&guid_string);
    return result;
  }
  catch (...)
  {
    RpcStringFreeW(&guid_string);
    throw;
  }
}

/**
 * Call HcsWaitForOperationResult and process any reported errors.
 * Uses Win32 naming scheme as it is helper for Win32.
 */
static void HcsWaitForOperationResultAndReportError(HCS_OPERATION operation)
{
  PWSTR report_raw = nullptr;
  HRESULT result = HcsWaitForOperationResult(operation, INFINITE, &report_raw);
  if (FAILED(result))
  {
    RaiiHandle<PWSTR> report(report_raw, LocalFree);
    std::wstring report_string(report.get());
    throw std::runtime_error(std::format(
      "HcsWaitForOperationResult failed. {}",
      std::string(report_string.begin(), report_string.end())));
  }
}

/**
 * Based on
 * https://learn.microsoft.com/en-us/windows/win32/secauthz/creating-a-security-descriptor-for-a-new-object-in-c--
 */
class SecurityAttributes
{
  RaiiHandle<PSID> sid_everyone{nullptr, FreeSid};
  RaiiHandle<PACL> acl{nullptr, LocalFree};
  RaiiHandle<PSECURITY_DESCRIPTOR> descriptor{
    LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH), LocalFree};
  SECURITY_ATTRIBUTES implementation{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                     .lpSecurityDescriptor = descriptor.get(),
                                     .bInheritHandle = false};

  SecurityAttributes() {}

public:
  PSECURITY_ATTRIBUTES get_pointer()
  {
    return &implementation;
  }

  static SecurityAttributes everyone_full()
  {
    SecurityAttributes result{};

    // Single ACL entry to authorise the "everyone" SID.
    EXPLICIT_ACCESS access[1]{};

    // Create the SID object for the "everyone".
    SID_IDENTIFIER_AUTHORITY sid_auth_world = SECURITY_WORLD_SID_AUTHORITY;
    PSID sid_raw = nullptr;
    if (!AllocateAndInitializeSid(
          &sid_auth_world,
          1,
          SECURITY_WORLD_RID,
          0,
          0,
          0,
          0,
          0,
          0,
          0,
          &sid_raw))
    {
      throw std::runtime_error(std::format(
        "AllocateAndInitializeSid failed. {}",
        GetErrorMessage(::GetLastError())));
    }
    result.sid_everyone.reset(sid_raw);

    access[0].grfAccessPermissions = FILE_ALL_ACCESS;
    access[0].grfAccessMode = SET_ACCESS;
    access[0].grfInheritance = NO_INHERITANCE;
    access[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    access[0].Trustee.ptstrName =
      reinterpret_cast<LPTSTR>(result.sid_everyone.get());

    // Create the ACL out of the entries.
    PACL acl_raw;
    auto response = SetEntriesInAcl(std::size(access), access, NULL, &acl_raw);
    if (response != ERROR_SUCCESS)
    {
      throw std::runtime_error(std::format(
        "SetEntriesInAcl failed. {}", GetErrorMessage(::GetLastError())));
    }
    result.acl.reset(acl_raw);

    // Initialize the security descriptor.
    if (!InitializeSecurityDescriptor(
          result.descriptor.get(), SECURITY_DESCRIPTOR_REVISION))
    {
      throw std::runtime_error(std::format(
        "InitializeSecurityDescriptor failed. {}",
        GetErrorMessage(::GetLastError())));
    }

    // Add the ACL to the security descriptor.
    if (!SetSecurityDescriptorDacl(
          result.descriptor.get(),
          TRUE, // bDaclPresent flag
          result.acl.get(),
          FALSE)) // not a default DACL
    {
      throw std::runtime_error(std::format(
        "SetSecurityDescriptorDacl failed. {}",
        GetErrorMessage(::GetLastError())));
    }

    return result;
  }
};

/**
 * Create a memory section in the global space with full access to "everyone".
 * Uses Win32 naming scheme as it is helper for Win32.
 */
static HANDLE CreateSection(std::wstring name, size_t size)
{
  auto security_attributes = SecurityAttributes::everyone_full();
  LARGE_INTEGER object_size{};
  object_size.QuadPart = size;
  auto section = CreateFileMappingW(
    INVALID_HANDLE_VALUE,
    security_attributes.get_pointer(),
    PAGE_READWRITE | SEC_COMMIT,
    object_size.HighPart,
    object_size.LowPart,
    name.c_str());
  if (section == NULL)
  {
    throw std::runtime_error(std::format(
      "Creating section failed. {}", GetErrorMessage(::GetLastError())));
  }

  return section;
}

/**
 * Adds escape characters to a file path for use in a JSON document.
 */
static std::wstring escape_file_path(const std::wstring& FilePath)
{
  std::wostringstream out;
  for (auto& c : FilePath)
  {
    switch (c)
    {
      case L'\\':
        out << L"\\\\";
        break;
      default:
        out << c;
        break;
    }
  }
  return out.str();
}

namespace monza::host
{
  class HCSEnclave : public HCSEnclaveAbstract
  {
    static constexpr size_t RAM_SIZE_IN_MB = 1 * 1024;
    static constexpr std::wstring_view SECTION_TEMPLATE =
      L"hcsenclave-memory-{}";
    static constexpr std::wstring_view HCS_SECTION_TEMPLATE =
      L"\\Sessions\\{}\\BaseNamedObjects\\{}";
    static constexpr std::wstring_view PIPE_TEMPLATE =
      L"\\\\.\\pipe\\hcsenclave-{}";
    static constexpr std::wstring_view CONFIG_TEMPLATE =
      LR"***(
{{
  "Owner": "HCSEnclave",
  "SchemaVersion": {{
    "Major": 2,
    "Minor": 6
  }},
  "VirtualMachine": {{
    "StopOnReset": true,
    "Chipset": {{
      "LinuxKernelDirect": {{
        "KernelFilePath": "{}"
      }}
    }},
    "ComputeTopology": {{
      "Memory": {{
        "SizeInMB": {},
        "AllowOvercommit": true
      }},
      "Processor": {{
        "Count": {}
      }}
    }},
    "Devices": {{
      "ComPorts": {{
        "0": {{
          "NamedPipe": "{}"
        }}
      }},
      "SharedMemory": {{
        "Regions": [{{
          "SectionName": "{}",
          "StartOffset": 0,
          "Length": {},
          "AllowGuestWrite": false
        }}]
      }}
    }}
  }},
  "ShouldTerminateOnLastHandleClosed": true
}}
)***";

    /**
     * The constructor of the class might throw exceptions.
     * All class members managing resources need automatic clean-up.
     */

    GUID system_id{};
    RaiiHandle<HCS_SYSTEM> hcs_system = {nullptr, HcsCloseComputeSystem};
    RaiiHandle<HANDLE> shared_section = {nullptr, CloseHandle};
    RaiiHandle<uint8_t*> shared_memory_mapping = {nullptr, UnmapViewOfFile};
    std::atomic<bool> finished = false;
    // Make sure that the thread can join on destruction.
    RaiiHandle<std::thread*> pipe_listener{nullptr,
                                           [finished = &finished](auto t) {
                                             finished->store(true);
                                             t->join();
                                             delete (t);
                                           }};
    bool started = false;

    HCSEnclave(
      const std::string& image_path,
      size_t num_threads,
      size_t shared_memory_size)
    : HCSEnclaveAbstract(image_path, num_threads, shared_memory_size)
    {
      HRESULT result = S_OK;
      // Unique id to allow multiple instances on the same machine.
      result = CoCreateGuid(&system_id);
      if (FAILED(result))
      {
        throw std::runtime_error(
          std::format("CoCreateGuid failed. {}", GetErrorMessage(result)));
      }
      auto id_string = GuidToString(system_id);
      std::wcout << "Compute system ID: " << id_string << std::endl;

      // The operation is used to track completion of the async methods.
      RaiiHandle<HCS_OPERATION> operation(
        HcsCreateOperation(nullptr, nullptr), HcsCloseOperation);
      if (operation.get() == nullptr)
      {
        throw std::runtime_error(std::format(
          "HcsCreateOperation failed. {}", GetErrorMessage(result)));
      }

      // Create named section to be used as shared region.
      auto section_name = std::format(SECTION_TEMPLATE, id_string);
      shared_section.reset(CreateSection(section_name, shared_memory_size));
      shared_memory_mapping.reset(static_cast<uint8_t*>(MapViewOfFile(
        shared_section.get(), FILE_MAP_ALL_ACCESS, 0, 0, shared_memory_size)));

      // Fill in the details of the config template.
      auto path = std::filesystem::canonical(std::filesystem::path(image_path));
      auto pipe_name = std::format(PIPE_TEMPLATE, id_string);
      DWORD session_id;
      ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
      auto hcs_section_name =
        std::format(HCS_SECTION_TEMPLATE, session_id, section_name);
      auto config = std::format(
        CONFIG_TEMPLATE,
        escape_file_path(path.wstring()),
        RAM_SIZE_IN_MB,
        num_threads,
        escape_file_path(pipe_name),
        escape_file_path(hcs_section_name),
        shared_memory_size);
      std::wcout << "Compute system config: " << config << std::endl;

      // Create the compute system and wait for the operation to finish.
      HCS_SYSTEM system_handle;
      result = HcsCreateComputeSystem(
        id_string.c_str(),
        config.c_str(),
        operation.get(),
        nullptr,
        &system_handle);
      if (FAILED(result))
      {
        throw std::runtime_error(std::format(
          "HcsCreateComputeSystem failed. {}", GetErrorMessage(result)));
      }
      HcsWaitForOperationResultAndReportError(operation.get());
      hcs_system.reset(system_handle);

      // Create a listener thread for the named pipe used for debug output.
      pipe_listener.reset(new std::thread([pipe_name = std::move(pipe_name),
                                           &finished = finished]() {
        RaiiHandle<HANDLE> pipe(
          CreateFileW(
            pipe_name.c_str(),
            GENERIC_READ,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL),
          CloseHandle);
        if (pipe.get() == INVALID_HANDLE_VALUE)
        {
          throw std::runtime_error(std::format(
            "Opening named pipe failed. {}", GetErrorMessage(GetLastError())));
        }
        while (!finished.load())
        {
          char buffer[1024];
          DWORD bytes_read = 0;
          // Read up to size - 1 bytes to allow space for NUL terminator.
          if (!ReadFile(
                pipe.get(), buffer, std::size(buffer) - 1, &bytes_read, NULL))
          {
            break;
          }
          if (bytes_read > 0)
          {
            buffer[bytes_read] = '\0';
            std::cout << buffer;
          }
        }
        std::cout << std::endl;
        std::cout << "Finished reading named pipe." << std::endl;
      }));
    }

  public:
    size_t shared_memory_guest_base() override
    {
      return RAM_SIZE_IN_MB * 1024 * 1024;
    }

    std::span<uint8_t> shared_memory() override
    {
      return std::span(shared_memory_mapping.get(), shared_memory_size);
    }

    void async_run() override
    {
      HRESULT result = S_OK;
      // The operation is used to track completion of the async methods.
      RaiiHandle<HCS_OPERATION> operation(
        HcsCreateOperation(nullptr, nullptr), HcsCloseOperation);
      if (operation.get() == nullptr)
      {
        throw std::runtime_error(std::format(
          "HcsCreateOperation failed. {}", GetErrorMessage(result)));
      }
      result =
        HcsStartComputeSystem(hcs_system.get(), operation.get(), nullptr);
      if (FAILED(result))
      {
        throw std::runtime_error(std::format(
          "HcsStartComputeSystem failed. {}", GetErrorMessage(result)));
      }
      HcsWaitForOperationResultAndReportError(operation.get());
      started = true;
    }

    void join() override
    {
      if (started)
      {
        HRESULT result = S_OK;
        // Create a Win32 Event to wait on.
        RaiiHandle<HANDLE> system_exit(
          CreateEventEx(nullptr, nullptr, 0, MAXIMUM_ALLOWED), CloseHandle);
        if (system_exit.get() == nullptr)
        {
          throw std::runtime_error(std::format(
            "CreateEventEx failed. {}", GetErrorMessage(GetLastError())));
        }
        // Convert specific HCS_EVENTs into the Win32 event firing.
        result = HcsSetComputeSystemCallback(
          hcs_system.get(),
          HcsEventOptionNone,
          system_exit.get(),
          [](HCS_EVENT* event, void* context) {
            if (
              event->Type == HcsEventSystemExited ||
              event->Type == HcsEventServiceDisconnect)
            {
              SetEvent(reinterpret_cast<HANDLE>(context));
            }
          });
        // Waiting on all possible stopping conditions.
        // For now this is only the Win32 event, but future-proofing.
        const HANDLE stopping_conditions[] = {system_exit.get()};
        WaitForMultipleObjects(
          static_cast<DWORD>(std::size(stopping_conditions)),
          stopping_conditions,
          FALSE,
          INFINITE);
      }
      started = false;
      // Notify the pipe listener that execution finished.
      finished.store(true);
    }

    friend HCSEnclaveAbstract;
  };

  std::unique_ptr<HCSEnclaveAbstract> HCSEnclaveAbstract::create(
    const std::string& image_path,
    size_t num_threads,
    size_t shared_memory_size)
  {
    return std::unique_ptr<HCSEnclave>(
      new HCSEnclave(image_path, num_threads, shared_memory_size));
  }
}
