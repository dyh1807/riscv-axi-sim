#pragma once
/**
 * @file AXI_Interconnect_IO.h
 * @brief AXI-Interconnect Upstream Interface Definitions
 *
 * Simplified master interfaces for icache/dcache/mmu:
 * - Single-beat wide data (up to 256-bit = 8 x 32-bit words)
 * - total_size specifies transfer width (0=1B, 31=32B)
 * - ID for out-of-order response routing
 */

#include <config.h>
#include <cstdint>

namespace axi_interconnect {

// ============================================================================
// Configuration
// ============================================================================
constexpr uint8_t NUM_READ_MASTERS = 3;  // icache, dcache, mmu
constexpr uint8_t NUM_WRITE_MASTERS = 1; // dcache only
constexpr uint8_t MAX_OUTSTANDING = 8;
constexpr uint8_t CACHELINE_WORDS = 8; // 256-bit = 8 x 32-bit

// Master IDs
constexpr uint8_t MASTER_ICACHE = 0;
constexpr uint8_t MASTER_DCACHE_R = 1;
constexpr uint8_t MASTER_MMU = 2;
constexpr uint8_t MASTER_DCACHE_W = 3;

// ============================================================================
// Wide Data Type (256-bit = 8 x 32-bit words)
// ============================================================================
struct WideData256_t {
  uint32_t words[CACHELINE_WORDS];

  void clear() {
    for (int i = 0; i < CACHELINE_WORDS; i++)
      words[i] = 0;
  }

  uint32_t &operator[](int idx) { return words[idx]; }
  const uint32_t &operator[](int idx) const { return words[idx]; }
};

// ============================================================================
// Read Master Interface (for icache/dcache/mmu)
// ============================================================================

// Read Request: Master → Interleaver
struct ReadMasterReq_t {
  wire1_t valid;
  wire1_t ready;      // ← Output from interleaver
  wire32_t addr;      // Byte address
  wire5_t total_size; // 0=1B, 3=4B, 7=8B, 15=16B, 31=32B
  wire4_t id;         // Transaction ID (for out-of-order)
};

// Read Response: Interleaver → Master
struct ReadMasterResp_t {
  wire1_t valid; // ← Output from interleaver
  wire1_t ready;
  WideData256_t data; // Wide data (up to 256-bit cacheline)
  wire4_t id;         // Matching transaction ID
};

// Combined Read Master Port
struct ReadMasterPort_t {
  ReadMasterReq_t req;
  ReadMasterResp_t resp;
};

// ============================================================================
// Write Master Interface (for dcache only)
// ============================================================================

// Write Request: Master → Interleaver (AW+W combined)
struct WriteMasterReq_t {
  wire1_t valid;
  wire1_t ready;       // ← Output from interleaver
  wire32_t addr;       // Byte address
  WideData256_t wdata; // Wide write data
  wire32_t wstrb;      // Byte strobe (32 bits for 256-bit data)
  wire5_t total_size;  // 0=1B, 3=4B, 31=32B
  wire4_t id;          // Transaction ID
};

// Write Response: Interleaver → Master
struct WriteMasterResp_t {
  wire1_t valid; // ← Output from interleaver
  wire1_t ready;
  wire4_t id;   // Matching transaction ID
  wire2_t resp; // AXI response (OKAY, SLVERR, etc.)
};

// Combined Write Master Port
struct WriteMasterPort_t {
  WriteMasterReq_t req;
  WriteMasterResp_t resp;
};

// ============================================================================
// AXI-Interconnect Combined IO
// ============================================================================

struct AXI_Interconnect_IO_t {
  // Upstream: Read Masters (3 ports)
  ReadMasterPort_t read_masters[NUM_READ_MASTERS];

  // Upstream: Write Master (1 port - dcache)
  WriteMasterPort_t write_master;

  // Downstream: AXI4 to SimDDR
  // (Use SimDDR_IO_t from sim_ddr module)
};

} // namespace axi_interconnect
