/**
 * @file AXI_Interconnect.cpp
 * @brief AXI-Interconnect Layer Implementation
 *
 * AXI Protocol Compliance:
 * - AR/AW valid signals are latched until ready handshake
 * - Upstream req_valid can be deasserted without affecting AXI valid
 */

#include "AXI_Interconnect.h"
#include <algorithm>
#include <cstdio>

namespace axi_interconnect {

// ============================================================================
// Initialization
// ============================================================================
void AXI_Interconnect::init() {
  r_arb_rr_idx = 0;
  r_current_master = -1;
  r_pending.clear();

  // Clear AR latch
  ar_latched.valid = false;
  ar_latched.addr = 0;
  ar_latched.len = 0;
  ar_latched.size = 2;
  ar_latched.burst = sim_ddr::AXI_BURST_INCR;
  ar_latched.id = 0;
  ar_latched.master_id = 0;
  ar_latched.orig_id = 0;

  w_active = false;
  w_current = {};
  w_req_ready_r = false;
  w_resp_valid = false;
  w_resp_id = 0;
  w_resp_resp = 0;

  // Clear AW latch
  aw_latched.valid = false;
  aw_latched.addr = 0;
  aw_latched.len = 0;
  aw_latched.size = 2;
  aw_latched.burst = sim_ddr::AXI_BURST_INCR;
  aw_latched.id = 0;

  // Clear registered req.ready signals
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    req_ready_r[i] = false;
    r_pending_age[i] = 0;
    r_pending_warned[i] = false;
    req_drop_warned[i] = false;
  }

  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].req.ready = false;
    read_ports[i].resp.valid = false;
    read_ports[i].resp.data.clear();
    read_ports[i].resp.id = 0;
  }
  write_port.req.ready = false;
  write_port.resp.valid = false;
  write_port.resp.id = 0;
  write_port.resp.resp = 0;

  axi_io.ar.arvalid = false;
  axi_io.ar.arid = 0;
  axi_io.ar.araddr = 0;
  axi_io.ar.arlen = 0;
  axi_io.ar.arsize = 2;
  axi_io.ar.arburst = sim_ddr::AXI_BURST_INCR;
  axi_io.r.rready = true;

  axi_io.aw.awvalid = false;
  axi_io.aw.awid = 0;
  axi_io.aw.awaddr = 0;
  axi_io.aw.awlen = 0;
  axi_io.aw.awsize = 2;
  axi_io.aw.awburst = sim_ddr::AXI_BURST_INCR;
  axi_io.w.wvalid = false;
  axi_io.w.wdata = 0;
  axi_io.w.wstrb = 0xF;
  axi_io.w.wlast = false;
  axi_io.b.bready = true;
}

// ============================================================================
// Two-Phase Combinational Logic
// ============================================================================

// Phase 1: Output signals for masters (run BEFORE cpu.cycle())
// Sets: port.resp.valid/data, port.req.ready (from register), DDR rready
void AXI_Interconnect::comb_outputs() {
  // Response path: DDR → masters
  comb_read_response();
  comb_write_response();

  // Use registered req.ready values (set by previous cycle's comb_inputs)
  // This ensures ICache sees req.ready in the same cycle as it transitions
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].req.ready = req_ready_r[i];
  }

  // If AR is latched (waiting for arready), also keep req.ready true
  if (ar_latched.valid) {
    read_ports[ar_latched.master_id].req.ready = true;
  }

  // Registered write req.ready (two-phase timing)
  write_port.req.ready = w_req_ready_r;
}

// Phase 2: Input signals from masters (run AFTER cpu.cycle())
// Processes: port.req.valid/addr, drives DDR AR/AW/W
void AXI_Interconnect::comb_inputs() {
  comb_read_arbiter();
  comb_write_request();
}

// ============================================================================
// Read Arbiter with Latched AR (AXI Compliant)
// ============================================================================
void AXI_Interconnect::comb_read_arbiter() {
  bool req_ready_curr[NUM_READ_MASTERS];
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    req_ready_curr[i] = req_ready_r[i];
    req_ready_r[i] = false;
  }

  // Detect ready pulses without a corresponding valid (possible dropped req).
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    if (req_ready_curr[i] && !read_ports[i].req.valid && !req_drop_warned[i] &&
        DEBUG) {
      printf("[axi] ready without valid (drop) master=%d\n", i);
      req_drop_warned[i] = true;
    }
  }

  // Default: don't accept new requests
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].req.ready = false;
  }

  // If AR is latched (waiting for arready), output latched values
  if (ar_latched.valid) {
    axi_io.ar.arvalid = true; // MUST stay valid until handshake!
    axi_io.ar.araddr = ar_latched.addr;
    axi_io.ar.arlen = ar_latched.len;
    axi_io.ar.arsize = ar_latched.size;
    axi_io.ar.arburst = ar_latched.burst;
    axi_io.ar.arid = ar_latched.id;
    // Also need to keep req.ready=true for the master whose request is latched
    // so the handshake completes and ICache knows request was accepted
    read_ports[ar_latched.master_id].req.ready = true;
    req_ready_r[ar_latched.master_id] = true;
    return; // Cannot accept new requests while AR pending
  }

  // No latched AR, can accept new request
  axi_io.ar.arvalid = false;

  // If a master saw ready last cycle, complete that handshake first.
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    if (!req_ready_curr[i] || !read_ports[i].req.valid) {
      continue;
    }
    bool has_pending = false;
    for (const auto &txn : r_pending) {
      if (txn.master_id == i) {
        has_pending = true;
        break;
      }
    }
    if (has_pending) {
      continue;
    }

    r_current_master = i;
    axi_io.ar.arvalid = true;
    axi_io.ar.araddr = read_ports[i].req.addr;
    axi_io.ar.arlen = calc_burst_len(read_ports[i].req.total_size);
    axi_io.ar.arsize = 2;
    axi_io.ar.arburst = sim_ddr::AXI_BURST_INCR;
    axi_io.ar.arid = (i << 2) | (read_ports[i].req.id & 0x3);
    read_ports[i].req.ready = true;
    return;
  }

  // Round-robin search for valid request
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    int idx = (r_arb_rr_idx + i) % NUM_READ_MASTERS;

    if (read_ports[idx].req.valid) {
      // Check if this master already has a pending transaction (only one
      // outstanding per master for correctness with simple masters)
      bool has_pending = false;
      for (const auto &txn : r_pending) {
        if (txn.master_id == idx) {
          has_pending = true;
          break;
        }
      }
      if (has_pending) {
        continue; // Skip, transaction in flight
      }

      r_current_master = idx;

      // Raise ready first, then issue AR on following cycle when ready is seen.
      if (!req_ready_curr[idx]) {
        req_ready_r[idx] = true;
        read_ports[idx].req.ready = true;
        break;
      }

      // Output AR (will be latched in seq if not immediately ready)
      axi_io.ar.arvalid = true;
      axi_io.ar.araddr = read_ports[idx].req.addr;
      axi_io.ar.arlen = calc_burst_len(read_ports[idx].req.total_size);
      axi_io.ar.arsize = 2;
      axi_io.ar.arburst = sim_ddr::AXI_BURST_INCR;
      axi_io.ar.arid = (idx << 2) | (read_ports[idx].req.id & 0x3);

      read_ports[idx].req.ready = true; // Also set for immediate use
      break;
    }
  }
}

void AXI_Interconnect::comb_read_response() {
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    read_ports[i].resp.valid = false;
  }
  axi_io.r.rready = true;

  // Find first complete response for EACH master (not just first overall)
  bool master_has_resp[NUM_READ_MASTERS] = {false};
  for (auto &txn : r_pending) {
    if (txn.beats_done == txn.total_beats) {
      uint8_t master = txn.master_id;
      if (!master_has_resp[master]) { // Only first complete per master
        read_ports[master].resp.valid = true;
        read_ports[master].resp.data = txn.data;
        read_ports[master].resp.id = txn.orig_id;
        master_has_resp[master] = true;
        // No break - continue to find responses for other masters
      }
    }
  }
}

// ============================================================================
// Write Request with Latched AW (AXI Compliant)
// ============================================================================
void AXI_Interconnect::comb_write_request() {
  bool w_req_ready_curr = w_req_ready_r;
  w_req_ready_r = false;

  if (w_req_ready_curr && !write_port.req.valid && DEBUG) {
    printf("[axi] write ready without valid (drop)\n");
  }

  axi_io.w.wvalid = false;

  // If AW is latched (waiting for awready), output latched values
  if (aw_latched.valid) {
    axi_io.aw.awvalid = true; // MUST stay valid until handshake!
    axi_io.aw.awaddr = aw_latched.addr;
    axi_io.aw.awlen = aw_latched.len;
    axi_io.aw.awsize = aw_latched.size;
    axi_io.aw.awburst = aw_latched.burst;
    axi_io.aw.awid = aw_latched.id;
  } else {
    axi_io.aw.awvalid = false;

    // Ready-first handshake: raise req.ready one cycle before accepting.
    // write_port.req.ready is driven from w_req_ready_r in comb_outputs().
    if (!w_active && !w_resp_valid && write_port.req.valid &&
        !w_req_ready_curr) {
      w_req_ready_r = true;
    }
  }

  // W channel: send data after AW done
  if (w_active && w_current.aw_done && !w_current.w_done) {
    axi_io.w.wvalid = true;
    axi_io.w.wdata = w_current.wdata[w_current.beats_sent];
    axi_io.w.wstrb = (w_current.wstrb >> (w_current.beats_sent * 4)) & 0xF;
    axi_io.w.wlast = (w_current.beats_sent == w_current.total_beats - 1);
  }
}

void AXI_Interconnect::comb_write_response() {
  write_port.resp.valid = w_resp_valid;
  write_port.resp.id = w_resp_id;
  write_port.resp.resp = w_resp_resp;

  // Backpressure DDR if upstream hasn't consumed the response yet.
  axi_io.b.bready = !w_resp_valid;
}

// ============================================================================
// Sequential Logic
// ============================================================================
void AXI_Interconnect::seq() {
  constexpr uint32_t kPendingTimeout = 100000;

  // ========== AR Channel with Latch ==========

  // If new AR request and NOT immediately ready, latch it
  if (axi_io.ar.arvalid && !ar_latched.valid && !axi_io.ar.arready) {
    // Latch the request
    ar_latched.valid = true;
    ar_latched.addr = axi_io.ar.araddr;
    ar_latched.len = axi_io.ar.arlen;
    ar_latched.size = axi_io.ar.arsize;
    ar_latched.burst = axi_io.ar.arburst;
    ar_latched.id = axi_io.ar.arid;
    ar_latched.master_id = (axi_io.ar.arid >> 2) & 0x3;
    ar_latched.orig_id = axi_io.ar.arid & 0x3;
  }

  // AR handshake complete
  if (axi_io.ar.arvalid && axi_io.ar.arready) {
    ReadPendingTxn txn;
    if (ar_latched.valid) {
      // Use latched values
      txn.master_id = ar_latched.master_id;
      txn.orig_id = ar_latched.orig_id;
      txn.total_beats = ar_latched.len + 1;
      ar_latched.valid = false; // Clear latch
    } else {
      // Direct handshake (same cycle)
      txn.master_id = (axi_io.ar.arid >> 2) & 0x3;
      txn.orig_id = axi_io.ar.arid & 0x3;
      txn.total_beats = axi_io.ar.arlen + 1;
    }
    txn.beats_done = 0;
    txn.data.clear();
    r_pending.push_back(txn);
    r_arb_rr_idx = (txn.master_id + 1) % NUM_READ_MASTERS;

    // req_ready_r is recomputed in comb_read_arbiter.
  }

  // R handshake
  if (axi_io.r.rvalid && axi_io.r.rready) {
    uint8_t master = (axi_io.r.rid >> 2) & 0x3;
    for (auto &txn : r_pending) {
      if (txn.master_id == master && txn.beats_done < txn.total_beats) {
        txn.data[txn.beats_done] = axi_io.r.rdata;
        txn.beats_done++;
        break;
      }
    }
  }

  // Response handshake
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    if (read_ports[i].resp.valid && read_ports[i].resp.ready) {
      r_pending.erase(std::remove_if(r_pending.begin(), r_pending.end(),
                                     [i](const ReadPendingTxn &t) {
                                       return t.master_id == i &&
                                              t.beats_done == t.total_beats;
                                     }),
                      r_pending.end());
    }
  }

  // Monitor long-lived pending reads per master.
  for (int i = 0; i < NUM_READ_MASTERS; i++) {
    bool has_pending = false;
    int beats_done = 0;
    int total_beats = 0;
    for (const auto &txn : r_pending) {
      if (txn.master_id == i) {
        has_pending = true;
        beats_done = txn.beats_done;
        total_beats = txn.total_beats;
        break;
      }
    }
    if (has_pending) {
      r_pending_age[i]++;
      if (r_pending_age[i] > kPendingTimeout && !r_pending_warned[i]) {
        printf("[axi] pending read timeout master=%d beats=%d/%d\n", i,
               beats_done, total_beats);
        r_pending_warned[i] = true;
      }
    } else {
      r_pending_age[i] = 0;
      r_pending_warned[i] = false;
    }
  }

  // ========== Write Channel ==========

  // Accept new write request
  if (!w_active && write_port.req.valid && write_port.req.ready) {
    w_active = true;
    w_current.master_id = MASTER_DCACHE_W;
    w_current.orig_id = write_port.req.id;
    w_current.addr = write_port.req.addr;
    w_current.wdata = write_port.req.wdata;
    w_current.wstrb = write_port.req.wstrb;
    w_current.total_beats = calc_burst_len(write_port.req.total_size) + 1;
    w_current.beats_sent = 0;
    w_current.aw_done = false;
    w_current.w_done = false;

    // Immediately latch AW (will stay valid until awready)
    aw_latched.valid = true;
    aw_latched.addr = w_current.addr;
    aw_latched.len = w_current.total_beats - 1;
    aw_latched.size = 2;
    aw_latched.burst = sim_ddr::AXI_BURST_INCR;
    aw_latched.id = (MASTER_DCACHE_W << 2) | (w_current.orig_id & 0x3);
  }

  // AW handshake
  if (axi_io.aw.awvalid && axi_io.aw.awready) {
    aw_latched.valid = false; // Clear latch
    w_current.aw_done = true;
  }

  // W handshake
  if (axi_io.w.wvalid && axi_io.w.wready) {
    w_current.beats_sent++;
    if (axi_io.w.wlast) {
      w_current.w_done = true;
    }
  }

  // B handshake
  if (axi_io.b.bvalid && axi_io.b.bready) {
    w_resp_valid = true;
    w_resp_id = axi_io.b.bid & 0x3;
    w_resp_resp = axi_io.b.bresp;
  }

  // Upstream response handshake (only one write outstanding is supported)
  if (write_port.resp.valid && write_port.resp.ready) {
    w_resp_valid = false;
    w_active = false;
    w_current = {};
  }
}

void AXI_Interconnect::debug_print() {
  printf("  interconnect: ar_latched=%d r_pending=%zu w_active=%d\n",
         ar_latched.valid, r_pending.size(), w_active);
  if (ar_latched.valid) {
    printf("    ar_latched: master=%u addr=0x%08x len=%u id=0x%02x\n",
           ar_latched.master_id, ar_latched.addr, ar_latched.len,
           ar_latched.id);
  }
  if (!r_pending.empty()) {
    for (const auto &txn : r_pending) {
      printf("    r_pending: master=%u beats=%u/%u\n", txn.master_id,
             txn.beats_done, txn.total_beats);
    }
  }
}

// ============================================================================
// Helpers
// ============================================================================
uint8_t AXI_Interconnect::calc_burst_len(uint8_t total_size) {
  uint8_t bytes = total_size + 1;
  uint8_t beats = (bytes + 3) / 4;
  return beats > 0 ? beats - 1 : 0;
}

} // namespace axi_interconnect
