#pragma once
/**
 * @file SimDDR.h
 * @brief SimDDR - DDR Memory Simulator with AXI4 Interface
 *
 * This module simulates DDR memory with standard AXI4 interface.
 * It follows the simulator's comb/seq pattern for cycle-accurate behavior.
 *
 * Features:
 * - 5 AXI4 channels (AW, W, B, AR, R)
 * - Configurable memory latency
 * - Outstanding transaction support (multiple in-flight transactions)
 * - Read data interleaving (can switch between transactions mid-burst)
 * - INCR burst mode support
 * - Uses external p_memory for storage (shared with main simulator)
 */

#include "SimDDR_IO.h"
#include <config.h>
#include <cstdint>
#include <queue>
#include <vector>

namespace sim_ddr {

// ============================================================================
// SimDDR Configuration
// ============================================================================
constexpr uint32_t SIM_DDR_LATENCY = ICACHE_MISS_LATENCY;
constexpr uint32_t SIM_DDR_MAX_BURST = 256;     // Max burst length (AXI4 limit)
constexpr uint32_t SIM_DDR_MAX_OUTSTANDING = 8; // Max outstanding transactions

// ============================================================================
// Transaction Structures for Outstanding Support
// ============================================================================

// Write transaction (after AW handshake, waiting for W data or in latency)
struct WriteTransaction {
  uint32_t addr;
  uint8_t id;
  uint8_t len; // Burst length - 1
  uint8_t size;
  uint8_t burst;
  uint8_t beat_cnt; // Current beat received
  bool data_done;   // All W beats received
};

// Write response pending (in latency phase after W complete)
struct WriteRespPending {
  uint8_t id;
  uint32_t latency_cnt;
};

// Read transaction (after AR handshake, in latency or sending data)
struct ReadTransaction {
  uint32_t addr;
  uint8_t id;
  uint8_t len; // Burst length - 1
  uint8_t size;
  uint8_t burst;
  uint8_t beat_cnt; // Current beat sent
  uint32_t latency_cnt;
  bool in_data_phase; // True if latency done, sending data
  bool complete;      // True when all beats sent and rlast accepted
};

// ============================================================================
// SimDDR Class with Outstanding + Interleaving Support
// ============================================================================
class SimDDR {
public:
  // ========== Simulator Interface ==========
  void init();

  // Two-phase combinational logic for proper signal timing
  void comb_outputs(); // Phase 1: arready, rvalid, rdata, bvalid, bresp
  void comb_inputs();  // Phase 2: Process arvalid, awvalid, wvalid
  void comb() {
    comb_outputs();
    comb_inputs();
  } // Convenience wrapper

  void seq();

  // ========== IO Ports ==========
  SimDDR_IO_t io;

  // ========== Debug ==========
  void print_state();

private:
  // ========== Write Channel ==========
  // Active write transaction (receiving W data)
  bool w_active;
  WriteTransaction w_current;

  // Pending write responses (in latency)
  std::queue<WriteRespPending> w_resp_queue;

  // ========== Read Channel with Interleaving ==========
  // Vector allows access to any transaction for interleaving
  std::vector<ReadTransaction> r_transactions;

  // Round-robin index for fair interleaving
  size_t r_rr_index;

  // Currently selected transaction index for this cycle (-1 if none)
  int r_selected_idx;

  // ========== Combinational Logic Functions ==========
  void comb_write_channel();
  void comb_read_channel();

  // ========== Helper Functions ==========
  void do_memory_write(uint32_t addr, uint32_t data, uint8_t wstrb);
  uint32_t do_memory_read(uint32_t addr);

  // Find next ready transaction using round-robin
  int find_next_ready_transaction();
};

} // namespace sim_ddr
