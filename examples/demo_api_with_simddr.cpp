#include "SimDDR.h"
#include "sc_axi4_sim_api.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

bool parse_u64(const char *str, uint64_t &value) {
  if (str == nullptr || *str == '\0') {
    return false;
  }
  char *end = nullptr;
  value = std::strtoull(str, &end, 0);
  return end != nullptr && *end == '\0';
}

void print_usage(const char *argv0) {
  std::cout << "Usage: " << argv0 << " <image> [--max-inst N] [--max-cycles N]\n";
}

void sample_ddr_outputs(const sim_ddr::SimDDR_IO_t &ddr_io, sc_axi4_in_t &in) {
  in.arready = ddr_io.ar.arready;
  in.awready = ddr_io.aw.awready;
  in.wready = ddr_io.w.wready;
  in.rvalid = ddr_io.r.rvalid;
  in.rid = ddr_io.r.rid;
  in.rdata = ddr_io.r.rdata;
  in.rresp = ddr_io.r.rresp;
  in.rlast = ddr_io.r.rlast;
  in.bvalid = ddr_io.b.bvalid;
  in.bid = ddr_io.b.bid;
  in.bresp = ddr_io.b.bresp;
}

void drive_ddr_inputs(sim_ddr::SimDDR_IO_t &ddr_io, const sc_axi4_out_t &out) {
  ddr_io.ar.arvalid = out.arvalid;
  ddr_io.ar.arid = out.arid;
  ddr_io.ar.araddr = out.araddr;
  ddr_io.ar.arlen = out.arlen;
  ddr_io.ar.arsize = out.arsize;
  ddr_io.ar.arburst = out.arburst;

  ddr_io.aw.awvalid = out.awvalid;
  ddr_io.aw.awid = out.awid;
  ddr_io.aw.awaddr = out.awaddr;
  ddr_io.aw.awlen = out.awlen;
  ddr_io.aw.awsize = out.awsize;
  ddr_io.aw.awburst = out.awburst;

  ddr_io.w.wvalid = out.wvalid;
  ddr_io.w.wdata = out.wdata;
  ddr_io.w.wstrb = out.wstrb;
  ddr_io.w.wlast = out.wlast;

  ddr_io.r.rready = out.rready;
  ddr_io.b.bready = out.bready;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  std::string image_path;
  uint64_t max_inst = 150000000ULL;
  uint64_t max_cycles = 12000000000ULL;

  image_path = argv[1];
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--max-inst") == 0 && i + 1 < argc) {
      if (!parse_u64(argv[++i], max_inst)) {
        std::cerr << "invalid --max-inst" << std::endl;
        return 1;
      }
      continue;
    }
    if (std::strcmp(argv[i], "--max-cycles") == 0 && i + 1 < argc) {
      if (!parse_u64(argv[++i], max_cycles)) {
        std::cerr << "invalid --max-cycles" << std::endl;
        return 1;
      }
      continue;
    }
    print_usage(argv[0]);
    return 1;
  }

  sc_sim_handle *sim = sc_sim_create();
  if (sim == nullptr) {
    std::cerr << "create simulator failed" << std::endl;
    return 1;
  }

  sc_sim_set_limits(sim, max_inst, max_cycles);

  uint64_t image_size = 0;
  if (sc_sim_load_image(sim, image_path.c_str(), &image_size) != 0) {
    std::cerr << "load image failed: " << sc_sim_last_error(sim) << std::endl;
    sc_sim_destroy(sim);
    return 1;
  }

  sim_ddr::SimDDR ddr;
  ddr.init();
  ddr.comb_outputs();

  std::cout << "[demo-api] image=" << image_path
            << " size=" << image_size
            << " max_inst=" << max_inst
            << " max_cycles=" << max_cycles << std::endl;

  sc_axi4_in_t axi_in{};
  sc_axi4_out_t axi_out{};
  sc_sim_status_t status{};
  int rc = 0;
  while (true) {
    sample_ddr_outputs(ddr.io, axi_in);
    rc = sc_sim_step(sim, &axi_in, &axi_out, &status);

    if (status.uart_valid) {
      std::cout << static_cast<char>(status.uart_ch) << std::flush;
    }

    drive_ddr_inputs(ddr.io, axi_out);
    ddr.comb_inputs();
    ddr.seq();
    ddr.comb_outputs();

    if (rc != 0) {
      break;
    }
  }

  if (rc > 0 && status.success) {
    std::cout << "\n[demo-api] success inst=" << status.inst_count
              << " cycle=" << status.sim_time << std::endl;
    sc_sim_destroy(sim);
    return 0;
  }

  std::cout << "\n[demo-api] failed inst=" << status.inst_count
            << " cycle=" << status.sim_time
            << " err=" << sc_sim_last_error(sim) << std::endl;
  sc_sim_destroy(sim);
  return 1;
}
