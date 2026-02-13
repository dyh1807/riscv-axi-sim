/**
 * @file SimDDR.cpp
 * @brief SimDDR Implementation - DDR Memory Simulator with AXI4 Interface
 *
 * Implementation with outstanding transaction support and read interleaving.
 * Uses vector + round-robin for fair interleaved data delivery.
 */

#include "SimDDR.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

// Use the global p_memory from the simulator
extern uint32_t *p_memory;
extern long long sim_time;

namespace sim_ddr {

// ============================================================================
// Initialization
// ============================================================================
void SimDDR::init() {
  // Clear write channel state
  w_active = false;
  w_current = {};

  // Clear queues
  while (!w_resp_queue.empty())
    w_resp_queue.pop();

  // Clear read transactions
  r_transactions.clear();
  r_rr_index = 0;
  r_selected_idx = -1;

  // Initialize IO outputs
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = AXI_RESP_OKAY;
  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata = 0;
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;
}

// ============================================================================
// Two-Phase Combinational Logic
// ============================================================================

// Phase 1: Output signals (run BEFORE cpu.cycle())
// Sets: arready, rvalid, rdata, bvalid, bresp
void SimDDR::comb_outputs() {
  // Read channel outputs (rvalid, rdata, arready)
  comb_read_channel();
  // Write channel outputs (awready, wready, bvalid, bresp)
  comb_write_channel();
}

// Phase 2: Input processing (run AFTER cpu.cycle())
// Note: For SimDDR, both channels compute based on current io inputs,
// which are already set. The wiring order in MemorySubsystem handles
// the dependency. This function is a no-op for now but kept for symmetry.
void SimDDR::comb_inputs() {
  // Input processing is already handled in comb_outputs for SimDDR
  // because the ready/valid signals are computed in the same pass.
  // The two-phase split is primarily for AXI_Interconnect's complex logic.
}

// ============================================================================
// Combinational Logic - Write Channel
// ============================================================================
void SimDDR::comb_write_channel() {
  // Default outputs
  io.aw.awready = false;
  io.w.wready = false;
  io.b.bvalid = false;
  io.b.bid = 0;
  io.b.bresp = AXI_RESP_OKAY;

  // --- AW Channel: Accept new write address if no active transaction ---
  if (!w_active) {
    io.aw.awready = true;
  }

  // --- W Channel: Accept write data if active transaction ---
  if (w_active && !w_current.data_done) {
    io.w.wready = true;
  }

  // --- B Channel: Send response if latency complete ---
  if (!w_resp_queue.empty()) {
    WriteRespPending &front =
        const_cast<WriteRespPending &>(w_resp_queue.front());
    if (front.latency_cnt >= SIM_DDR_LATENCY) {
      io.b.bvalid = true;
      io.b.bid = front.id;
      io.b.bresp = AXI_RESP_OKAY;
    }
  }
}

// ============================================================================
// Find next ready transaction using round-robin (for interleaving)
// ============================================================================
int SimDDR::find_next_ready_transaction() {
  if (r_transactions.empty())
    return -1;

  size_t count = r_transactions.size();

  // Start from current rr_index and search for a ready transaction
  for (size_t i = 0; i < count; i++) {
    size_t idx = (r_rr_index + i) % count;
    if (r_transactions[idx].in_data_phase && !r_transactions[idx].complete) {
      return static_cast<int>(idx);
    }
  }

  return -1; // No ready transactions
}

// ============================================================================
// Combinational Logic - Read Channel with Interleaving
// ============================================================================
void SimDDR::comb_read_channel() {
  // Default outputs
  io.ar.arready = false;
  io.r.rvalid = false;
  io.r.rid = 0;
  io.r.rdata = 0;
  io.r.rresp = AXI_RESP_OKAY;
  io.r.rlast = false;

  // Reset selection
  r_selected_idx = -1;

  // --- AR Channel: Accept new read address if not full ---
  if (r_transactions.size() < SIM_DDR_MAX_OUTSTANDING) {
    io.ar.arready = true;
  }

  // --- R Channel: Interleaved data from any ready transaction ---
  r_selected_idx = find_next_ready_transaction();

  if (r_selected_idx >= 0) {
    ReadTransaction &txn = r_transactions[r_selected_idx];
    uint32_t current_addr = txn.addr + (txn.beat_cnt << txn.size);

    io.r.rvalid = true;
    io.r.rid = txn.id;
    io.r.rdata = do_memory_read(current_addr);
    io.r.rresp = AXI_RESP_OKAY;
    io.r.rlast = (txn.beat_cnt == txn.len);
  }
}

// ============================================================================
// Sequential Logic
// ============================================================================
void SimDDR::seq() {
  // ========== Write Channel Sequential Logic ==========

  // AW handshake: Start new write transaction
  if (io.aw.awvalid && io.aw.awready) {
    w_active = true;
    w_current.addr = io.aw.awaddr;
    w_current.id = io.aw.awid;
    w_current.len = io.aw.awlen;
    w_current.size = io.aw.awsize;
    w_current.burst = io.aw.awburst;
    w_current.beat_cnt = 0;
    w_current.data_done = false;
  }

  // W handshake: Process write data
  if (io.w.wvalid && io.w.wready && w_active) {
    uint32_t current_addr =
        w_current.addr + (w_current.beat_cnt << w_current.size);
    do_memory_write(current_addr, io.w.wdata, io.w.wstrb);
    w_current.beat_cnt++;

    if (io.w.wlast) {
      w_current.data_done = true;
      WriteRespPending resp;
      resp.id = w_current.id;
      resp.latency_cnt = 0;
      w_resp_queue.push(resp);
      w_active = false;
    }
  }

  // B handshake: Complete response
  if (io.b.bvalid && io.b.bready) {
    w_resp_queue.pop();
  }

  // Increment latency counters for pending responses
  std::queue<WriteRespPending> temp_queue;
  while (!w_resp_queue.empty()) {
    WriteRespPending resp = w_resp_queue.front();
    w_resp_queue.pop();
    resp.latency_cnt++;
    temp_queue.push(resp);
  }
  w_resp_queue = temp_queue;

  // ========== Read Channel Sequential Logic ==========

  // AR handshake: Start new read transaction
  if (io.ar.arvalid && io.ar.arready) {
    ReadTransaction txn;
    txn.addr = io.ar.araddr;
    txn.id = io.ar.arid;
    txn.len = io.ar.arlen;
    txn.size = io.ar.arsize;
    txn.burst = io.ar.arburst;
    txn.beat_cnt = 0;
    txn.latency_cnt = 0;
    txn.in_data_phase = false;
    txn.complete = false;
    r_transactions.push_back(txn);
  }

  // R handshake: Advance data beat on selected transaction
  if (io.r.rvalid && io.r.rready && r_selected_idx >= 0) {
    ReadTransaction &txn = r_transactions[r_selected_idx];
    txn.beat_cnt++;

    if (io.r.rlast) {
      txn.complete = true;
    }

    // Advance round-robin index for interleaving
    r_rr_index =
        (r_selected_idx + 1) % std::max((size_t)1, r_transactions.size());
  }

  // Update all read transactions: increment latency, mark data phase
  for (auto &txn : r_transactions) {
    if (!txn.in_data_phase && !txn.complete) {
      txn.latency_cnt++;
      if (txn.latency_cnt >= SIM_DDR_LATENCY) {
        txn.in_data_phase = true;
      }
    }
  }

  // Remove completed transactions
  r_transactions.erase(
      std::remove_if(r_transactions.begin(), r_transactions.end(),
                     [](const ReadTransaction &t) { return t.complete; }),
      r_transactions.end());

  // Adjust rr_index if vector size changed
  if (!r_transactions.empty() && r_rr_index >= r_transactions.size()) {
    r_rr_index = 0;
  }
}

// ============================================================================
// Helper Functions
// ============================================================================

void SimDDR::do_memory_write(uint32_t addr, uint32_t data, uint8_t wstrb) {
  uint32_t word_addr = addr >> 2;

  if (p_memory == nullptr) {
    return;
  }

  uint32_t old_data = p_memory[word_addr];
  uint32_t mask = 0;

  if (wstrb & 0x1)
    mask |= 0x000000FF;
  if (wstrb & 0x2)
    mask |= 0x0000FF00;
  if (wstrb & 0x4)
    mask |= 0x00FF0000;
  if (wstrb & 0x8)
    mask |= 0xFF000000;

  p_memory[word_addr] = (data & mask) | (old_data & ~mask);

  if (DCACHE_LOG) {
    printf("[SimDDR] Write: addr=0x%08x data=0x%08x wstrb=0x%x\n", addr, data,
           wstrb);
  }
}

uint32_t SimDDR::do_memory_read(uint32_t addr) {
  uint32_t word_addr = addr >> 2;

  if (p_memory == nullptr) {
    return 0xDEADBEEF;
  }

  uint32_t data = p_memory[word_addr];

  if (DCACHE_LOG) {
    printf("[SimDDR] Read: addr=0x%08x -> 0x%08x\n", addr, data);
  }

  return data;
}

// ============================================================================
// Debug
// ============================================================================
void SimDDR::print_state() {
  printf("[SimDDR] Write: active=%d resp_pending=%zu\n", w_active,
         w_resp_queue.size());
  printf("[SimDDR] Read: txn_count=%zu rr_index=%zu\n", r_transactions.size(),
         r_rr_index);
}

} // namespace sim_ddr
