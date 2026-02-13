#pragma once
/**
 * @file AXI_Interconnect.h
 * @brief AXI-Interconnect Layer
 *
 * Converts simplified master interfaces (single-beat, wide data) to
 * AXI4 protocol (multi-beat bursts) for connection to SimDDR.
 *
 * AXI Protocol Compliance:
 * - AR/AW valid signals are latched until ready handshake
 * - Upstream req_valid can be deasserted without affecting AXI valid
 */

#include "AXI_Interconnect_IO.h"
#include "SimDDR_IO.h"
#include <config.h>
#include <queue>
#include <vector>

namespace axi_interconnect {

// ============================================================================
// Latched AR/AW Requests (for AXI compliance)
// ============================================================================

// Latched AR request - holds values until arready
struct ARLatch_t {
  bool valid;
  uint32_t addr;
  uint8_t len;
  uint8_t size;
  uint8_t burst;
  uint8_t id;
  uint8_t master_id;
  uint8_t orig_id;
};

// Latched AW request - holds values until awready
struct AWLatch_t {
  bool valid;
  uint32_t addr;
  uint8_t len;
  uint8_t size;
  uint8_t burst;
  uint8_t id;
};

// ============================================================================
// Pending Transaction Tracking
// ============================================================================

struct ReadPendingTxn {
  uint8_t master_id;
  uint8_t orig_id;
  uint8_t total_beats;
  uint8_t beats_done;
  WideData256_t data;
};

struct WritePendingTxn {
  uint8_t master_id;
  uint8_t orig_id;
  uint32_t addr;
  WideData256_t wdata;
  uint32_t wstrb;
  uint8_t total_beats;
  uint8_t beats_sent;
  bool aw_done;
  bool w_done;
};

// ============================================================================
// AXI_Interconnect Class
// ============================================================================

class AXI_Interconnect {
public:
  void init();

  // Two-phase combinational logic for proper signal timing
  void comb_outputs(); // Phase 1: Update resp signals for masters, req.ready
  void comb_inputs();  // Phase 2: Process req from masters, drive DDR AR/AW/W
  void comb() {
    comb_outputs();
    comb_inputs();
  } // Convenience wrapper

  void seq();

  void debug_print();

  // Upstream IO (Masters)
  ReadMasterPort_t read_ports[NUM_READ_MASTERS];
  WriteMasterPort_t write_port;

  // Downstream IO (to SimDDR)
  sim_ddr::SimDDR_IO_t axi_io;

private:
  // Read arbiter state
  uint8_t r_arb_rr_idx;
  int r_current_master;

  // Registered req.ready for each master (persists until handshake)
  bool req_ready_r[NUM_READ_MASTERS];
  uint32_t r_pending_age[NUM_READ_MASTERS];
  bool r_pending_warned[NUM_READ_MASTERS];
  bool req_drop_warned[NUM_READ_MASTERS];
  bool w_req_ready_r;

  // AR latch for AXI compliance
  ARLatch_t ar_latched;

  // Pending read transactions
  std::vector<ReadPendingTxn> r_pending;

  // Write state
  bool w_active;
  WritePendingTxn w_current;
  bool w_resp_valid;
  uint8_t w_resp_id;
  uint8_t w_resp_resp;

  // AW latch for AXI compliance
  AWLatch_t aw_latched;

  void comb_read_arbiter();
  void comb_read_response();
  void comb_write_request();
  void comb_write_response();

  uint8_t calc_burst_len(uint8_t total_size);
};

} // namespace axi_interconnect
