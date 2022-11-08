// Copyright Microsoft and Project Monza Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

/**
 * This file contains the definitions corresponding to the SEV-SNP GHCB MSR
 * Protocol. Based on: https://developer.amd.com/wp-content/resources/56421.pdf
 * Section 2.3.1
 */

namespace monza
{
  enum SevGhcbInfo : uint64_t
  {
    SEV_GHCB_INFO_NORMAL = 0x000,
    SEV_GHCB_INFO_REGISTER_REQUEST = 0x012,
    SEV_GHCB_INFO_REGISTER_RESPONSE = 0x013,
    SEV_GHCB_INFO_PAGE_STATE_REQUEST = 0x014,
    SEV_GHCB_INFO_PAGE_STATE_RESPONSE = 0x015,
    SEV_GHCB_INFO_FEATURES_REQUEST = 0x080,
    SEV_GHCB_INFO_FEATURES_RESPONSE = 0x081,
    SEV_GHCB_INFO_TERMINATION_REQUEST = 0x100
  };

  /**
   * Generic root implementation for request taking a 48-bit input.
   * Combines the 12-bit info field with the input to form 64-bit value.
   */
  template<SevGhcbInfo Info>
  class SevGhcbMsrSimpleRequest
  {
    uint64_t content;

  public:
    SevGhcbMsrSimpleRequest(uint64_t input) : content(Info | (input << 12)) {}

    uint64_t raw() const
    {
      return content;
    }
  };

  /**
   * Generic root implementation fro response.
   * Success is determined by 12-bit info field matching expectations.
   * Ability to retrieve 48-bit response.
   */
  template<SevGhcbInfo Info>
  class SevGhcbMsrSimpleResponse
  {
    uint64_t content;

  public:
    SevGhcbMsrSimpleResponse(uint64_t response) : content(response) {}

    bool success() const
    {
      return ((content & 0xFFF) == Info);
    }

    uint64_t value() const
    {
      return content >> 12;
    }
  };

  /**
   * Input is the 48-bit guest physical page index of the GHCB.
   */
  using SevGhcbMsrNormal = SevGhcbMsrSimpleRequest<SEV_GHCB_INFO_NORMAL>;

  /**
   * Input is the 48-bit guest physical page index of the GHCB.
   */
  using SevGhcbMsrRegisterRequest =
    SevGhcbMsrSimpleRequest<SEV_GHCB_INFO_REGISTER_REQUEST>;

  /**
   * Output is the 48-bit guest physical page index of the GHCB.
   */
  using SevGhcbMsrRegisterResponse =
    SevGhcbMsrSimpleResponse<SEV_GHCB_INFO_REGISTER_RESPONSE>;

  /**
   * Input is the guest physical page number (expressed in HV_PAGE_SIZE) and
   * flag to signal new state.
   */
  class SevGhcbMsrPageStateRequest
  {
    uint64_t content;

  public:
    SevGhcbMsrPageStateRequest(
      uint64_t guest_physical_page_number, bool is_shared)
    : content(
        SEV_GHCB_INFO_PAGE_STATE_REQUEST | guest_physical_page_number << 12 |
        ((is_shared ? 0x002ULL : 0x001ULL) << 52))
    {}

    uint64_t raw() const
    {
      return content;
    }
  };

  /**
   * No output, but stronger success check includes error code checking.
   */
  class SevGhcbMsrPageStateResponse
  {
    uint64_t content;

  public:
    SevGhcbMsrPageStateResponse(uint64_t response) : content(response) {}

    bool success() const
    {
      return ((content & 0xFFF) == SEV_GHCB_INFO_PAGE_STATE_RESPONSE) &&
        ((content >> 32) == 0);
    }
  };

  /**
   * No inputs.
   */
  class SevGhcbMsrFeaturesRequest
  : public SevGhcbMsrSimpleRequest<SEV_GHCB_INFO_FEATURES_REQUEST>
  {
  public:
    SevGhcbMsrFeaturesRequest()
    : SevGhcbMsrSimpleRequest<SEV_GHCB_INFO_FEATURES_REQUEST>(0)
    {}
  };

  /**
   * Output is the features bitmap.
   */
  using SevGhcbMsrFeaturesResponse =
    SevGhcbMsrSimpleResponse<SEV_GHCB_INFO_FEATURES_RESPONSE>;

  /**
   * Input is the up to 4-bit error code.
   */
  class SevGhcbMsrTerminationRequest
  : public SevGhcbMsrSimpleRequest<SEV_GHCB_INFO_TERMINATION_REQUEST>
  {
  public:
    SevGhcbMsrTerminationRequest(uint8_t reason)
    : SevGhcbMsrSimpleRequest<SEV_GHCB_INFO_TERMINATION_REQUEST>(reason & 0xF)
    {}
  };
}
