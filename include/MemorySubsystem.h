#pragma once
/**
 * @file MemorySubsystem.h
 * @brief Memory Subsystem Singleton
 *
 * Wraps AXI_Interconnect + SimDDR for use by ICache/DCache/MMU.
 * Provides single integration point for main simulation loop.
 */

// Build defaults to AXI4 (32-bit). Define USE_SIM_DDR_AXI3 to select AXI3 (256-bit).
#ifndef USE_SIM_DDR_AXI4
#include "AXI_Interconnect_AXI3.h"
#include "AXI_Router_AXI3.h"
#include "MMIO_Bus_AXI3.h"
#include "UART16550_Device.h"
#include "SimDDR_AXI3.h"
#else
#include "AXI_Interconnect.h"
#include "SimDDR.h"
#endif
#include <config.h>
#include <cstdint>

class MemorySubsystem {
public:
  // Singleton accessor
  static MemorySubsystem &getInstance() {
    static MemorySubsystem instance;
    return instance;
  }

  // Initialize subsystem
  void init() {
    interconnect.init();
#ifndef USE_SIM_DDR_AXI4
    router.init();
    mmio.init();
    mmio.add_device(UART_BASE, 0x1000, &uart0);
#endif
    ddr.init();
    clear_inputs();
  }

  // ========================================================================
  // Two-Phase Combinational Logic
  // ========================================================================

  // Phase 1: Output signals for CPU (run BEFORE cpu.cycle())
  // Sets: resp.valid, resp.data, req.ready
  void comb_outputs() {
    // Reset upstream inputs each cycle; masters will drive them in comb.
    clear_inputs();

    // First get DDR/MMIO outputs for interconnect
    ddr.comb_outputs();
#ifndef USE_SIM_DDR_AXI4
    mmio.comb_outputs();
#endif

#ifndef USE_SIM_DDR_AXI4
    // Route DDR/MMIO responses to interconnect inputs via router
    router.comb_outputs(interconnect.axi_io, ddr.io, mmio.io);
#else
    // Wire DDR responses to interconnect inputs
    interconnect.axi_io.ar.arready = ddr.io.ar.arready;
    interconnect.axi_io.r.rvalid = ddr.io.r.rvalid;
    interconnect.axi_io.r.rid = ddr.io.r.rid;
    interconnect.axi_io.r.rdata = ddr.io.r.rdata;
    interconnect.axi_io.r.rlast = ddr.io.r.rlast;
    interconnect.axi_io.r.rresp = ddr.io.r.rresp;
    interconnect.axi_io.aw.awready = ddr.io.aw.awready;
    interconnect.axi_io.w.wready = ddr.io.w.wready;
    interconnect.axi_io.b.bvalid = ddr.io.b.bvalid;
    interconnect.axi_io.b.bid = ddr.io.b.bid;
    interconnect.axi_io.b.bresp = ddr.io.b.bresp;
#endif

    // Now run interconnect output phase
    interconnect.comb_outputs();
  }

  // Phase 2: Input signals from CPU (run AFTER cpu.cycle())
  // Processes: req.valid, req.addr, drives AR/AW/W signals to DDR
  void comb_inputs() {
    // Run interconnect input phase (arbiter, write request)
    interconnect.comb_inputs();

#ifndef USE_SIM_DDR_AXI4
    // Route interconnect -> DDR/MMIO inputs via router
    router.comb_inputs(interconnect.axi_io, ddr.io, mmio.io);
#else
    // Wire interconnect to DDR inputs
    ddr.io.ar.arvalid = interconnect.axi_io.ar.arvalid;
    ddr.io.ar.araddr = interconnect.axi_io.ar.araddr;
    ddr.io.ar.arid = interconnect.axi_io.ar.arid;
    ddr.io.ar.arlen = interconnect.axi_io.ar.arlen;
    ddr.io.ar.arsize = interconnect.axi_io.ar.arsize;
    ddr.io.ar.arburst = interconnect.axi_io.ar.arburst;

    ddr.io.aw.awvalid = interconnect.axi_io.aw.awvalid;
    ddr.io.aw.awaddr = interconnect.axi_io.aw.awaddr;
    ddr.io.aw.awid = interconnect.axi_io.aw.awid;
    ddr.io.aw.awlen = interconnect.axi_io.aw.awlen;
    ddr.io.aw.awsize = interconnect.axi_io.aw.awsize;
    ddr.io.aw.awburst = interconnect.axi_io.aw.awburst;

    ddr.io.w.wvalid = interconnect.axi_io.w.wvalid;
    ddr.io.w.wdata = interconnect.axi_io.w.wdata;
    ddr.io.w.wstrb = interconnect.axi_io.w.wstrb;
    ddr.io.w.wlast = interconnect.axi_io.w.wlast;
#ifndef USE_SIM_DDR_AXI4
    ddr.io.w.wid = interconnect.axi_io.w.wid;
#endif

    ddr.io.r.rready = interconnect.axi_io.r.rready;
    ddr.io.b.bready = interconnect.axi_io.b.bready;
#endif

    // Run DDR input phase
    ddr.comb_inputs();
#ifndef USE_SIM_DDR_AXI4
    mmio.comb_inputs();
#endif
  }

  // Convenience wrapper (for tests that don't need two-phase)
  void comb() {
    comb_outputs();
    comb_inputs();
  }

  // Sequential logic (call in main loop after comb)
  void seq() {
    ddr.seq();
#ifndef USE_SIM_DDR_AXI4
    mmio.seq();
    router.seq(interconnect.axi_io, ddr.io, mmio.io);
#endif
    interconnect.seq();
  }

  // Access interconnect ports (for ICache/DCache/MMU)
  axi_interconnect::ReadMasterPort_t &icache_port() {
    return interconnect.read_ports[axi_interconnect::MASTER_ICACHE];
  }

  axi_interconnect::ReadMasterPort_t &dcache_read_port() {
    return interconnect.read_ports[axi_interconnect::MASTER_DCACHE_R];
  }

  axi_interconnect::ReadMasterPort_t &mmu_port() {
    return interconnect.read_ports[axi_interconnect::MASTER_MMU];
  }

  axi_interconnect::WriteMasterPort_t &dcache_write_port() {
    return interconnect.write_port;
  }

#ifdef USE_SIM_DDR_AXI4
  const sim_ddr::SimDDR_IO_t &axi_io_debug() const { return interconnect.axi_io; }
#endif

  // Debug print for stall diagnosis
  void debug_print() {
    printf("=== MemorySubsystem Debug ===\n");
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      printf("  read_port[%d]: req_v=%d req_rdy=%d addr=0x%08x resp_v=%d "
             "resp_rdy=%d\n",
             i, interconnect.read_ports[i].req.valid,
             interconnect.read_ports[i].req.ready,
             interconnect.read_ports[i].req.addr,
             interconnect.read_ports[i].resp.valid,
             interconnect.read_ports[i].resp.ready);
    }
    printf("  axi_io: arvalid=%d arready=%d rvalid=%d rready=%d\n",
           interconnect.axi_io.ar.arvalid, interconnect.axi_io.ar.arready,
           interconnect.axi_io.r.rvalid, interconnect.axi_io.r.rready);
    interconnect.debug_print();
  }

private:
  MemorySubsystem() = default;
  MemorySubsystem(const MemorySubsystem &) = delete;
  MemorySubsystem &operator=(const MemorySubsystem &) = delete;

#ifdef USE_SIM_DDR_AXI4
  axi_interconnect::AXI_Interconnect interconnect;
  sim_ddr::SimDDR ddr;
#else
  axi_interconnect::AXI_Interconnect_AXI3 interconnect;
  sim_ddr_axi3::SimDDR_AXI3 ddr;
  axi_interconnect::AXI_Router_AXI3 router;
  mmio::MMIO_Bus_AXI3 mmio;
  mmio::UART16550_Device uart0{UART_BASE};
#endif

  void clear_inputs() {
    // Clear all upstream ports
    for (int i = 0; i < axi_interconnect::NUM_READ_MASTERS; i++) {
      interconnect.read_ports[i].req.valid = false;
      interconnect.read_ports[i].req.addr = 0;
      interconnect.read_ports[i].req.total_size = 0;
      interconnect.read_ports[i].req.id = 0;
      interconnect.read_ports[i].resp.ready = false;
    }
    interconnect.write_port.req.valid = false;
    interconnect.write_port.req.addr = 0;
    interconnect.write_port.req.wdata.clear();
    interconnect.write_port.req.wstrb = 0;
    interconnect.write_port.req.total_size = 0;
    interconnect.write_port.req.id = 0;
    interconnect.write_port.resp.ready = false;
  }
};

// Convenience accessor
inline MemorySubsystem &mem_subsystem() {
  return MemorySubsystem::getInstance();
}
