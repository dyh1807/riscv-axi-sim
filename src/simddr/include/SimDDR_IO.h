#pragma once
/**
 * @file SimDDR_IO.h
 * @brief AXI4 interface signal type definitions for SimDDR
 *
 * Standard AXI4 protocol with 5 channels:
 * - Write Address (AW): Master -> Slave
 * - Write Data (W): Master -> Slave
 * - Write Response (B): Slave -> Master
 * - Read Address (AR): Master -> Slave
 * - Read Data (R): Slave -> Master
 */

#include <config.h>

namespace sim_ddr {

// ============================================================================
// AXI4 Configuration
// ============================================================================
constexpr uint8_t AXI_ID_WIDTH =
    4; // 4-bit ID, supports up to 16 outstanding transactions

// ============================================================================
// AXI4 Burst Types
// ============================================================================
constexpr uint8_t AXI_BURST_FIXED = 0b00;
constexpr uint8_t AXI_BURST_INCR = 0b01;
constexpr uint8_t AXI_BURST_WRAP = 0b10;

// ============================================================================
// AXI4 Response Types
// ============================================================================
constexpr uint8_t AXI_RESP_OKAY = 0b00;
constexpr uint8_t AXI_RESP_EXOKAY = 0b01;
constexpr uint8_t AXI_RESP_SLVERR = 0b10;
constexpr uint8_t AXI_RESP_DECERR = 0b11;

// ============================================================================
// AXI4 Write Address Channel (AW)
// Master -> Slave
// ============================================================================
struct AXI4_AW_t {
  // Handshake signals
  wire1_t awvalid; // Address write valid (Master output)
  wire1_t awready; // Address write ready (Slave output)

  // Transaction ID
  wire4_t awid; // Write transaction ID (Master output)

  // Address and control
  wire32_t awaddr; // Write address (byte address)
  wire8_t awlen;   // Burst length - 1 (0 = 1 beat, 255 = 256 beats)
  wire3_t awsize;  // Burst size (0=1B, 1=2B, 2=4B, 3=8B...)
  wire2_t awburst; // Burst type (0=FIXED, 1=INCR, 2=WRAP)
};

// ============================================================================
// AXI4 Write Data Channel (W)
// Master -> Slave
// ============================================================================
struct AXI4_W_t {
  // Handshake signals
  wire1_t wvalid; // Write data valid (Master output)
  wire1_t wready; // Write data ready (Slave output)

  // Data
  wire32_t wdata; // Write data
  wire4_t wstrb;  // Write strobes (byte enables)
  wire1_t wlast;  // Last beat of burst (Master output)
};

// ============================================================================
// AXI4 Write Response Channel (B)
// Slave -> Master
// ============================================================================
struct AXI4_B_t {
  // Handshake signals
  wire1_t bvalid; // Write response valid (Slave output)
  wire1_t bready; // Write response ready (Master output)

  // Transaction ID
  wire4_t bid; // Write response ID (Slave output, matches awid)

  // Response
  wire2_t bresp; // Write response (OKAY/EXOKAY/SLVERR/DECERR)
};

// ============================================================================
// AXI4 Read Address Channel (AR)
// Master -> Slave
// ============================================================================
struct AXI4_AR_t {
  // Handshake signals
  wire1_t arvalid; // Address read valid (Master output)
  wire1_t arready; // Address read ready (Slave output)

  // Transaction ID
  wire4_t arid; // Read transaction ID (Master output)

  // Address and control
  wire32_t araddr; // Read address (byte address)
  wire8_t arlen;   // Burst length - 1 (0 = 1 beat, 255 = 256 beats)
  wire3_t arsize;  // Burst size (0=1B, 1=2B, 2=4B, 3=8B...)
  wire2_t arburst; // Burst type (0=FIXED, 1=INCR, 2=WRAP)
};

// ============================================================================
// AXI4 Read Data Channel (R)
// Slave -> Master
// ============================================================================
struct AXI4_R_t {
  // Handshake signals
  wire1_t rvalid; // Read data valid (Slave output)
  wire1_t rready; // Read data ready (Master output)

  // Transaction ID
  wire4_t rid; // Read data ID (Slave output, matches arid)

  // Data and response
  wire32_t rdata; // Read data
  wire2_t rresp;  // Read response (OKAY/EXOKAY/SLVERR/DECERR)
  wire1_t rlast;  // Last beat of burst (Slave output)
};

// ============================================================================
// Combined SimDDR IO Interface
// ============================================================================
struct SimDDR_IO_t {
  // Write channels (Master signals as input, Slave signals as output)
  AXI4_AW_t aw;
  AXI4_W_t w;
  AXI4_B_t b;

  // Read channels
  AXI4_AR_t ar;
  AXI4_R_t r;
};

} // namespace sim_ddr
