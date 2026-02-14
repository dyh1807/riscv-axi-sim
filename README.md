# 单周期 CPU + AXI4 周期接口库

这个项目提供两种使用方式：

1. 可执行程序：`single_cycle_axi4.out`（用于本地回归，内部连接 `SimDDR`）  
2. 可链接库：`libsingle_cycle_axi4.a` / `libsingle_cycle_axi4.so`（用于外部系统按周期驱动）

核心目标是把 CPU+Interconnect 封装为黑盒：外部每拍给 AXI 输入，模块每拍返回 AXI 输出。

## 架构连线（ASCII）

### 1) CLI 本地回归模式（`single_cycle_axi4.out` + `SimDDR`）

```text
      (周期推进: main.cpp for-loop)
 ┌──────────────────────┐            AXI4 master/slave handshake
 │  RV32 单周期核心     │  AR/AW/W  ───────────────────────────▶
 │  (SingleCycleCpu)    │  R/B      ◀───────────────────────────
 └──────────┬───────────┘
            │
            ▼
 ┌──────────────────────┐
 │ AXI Interconnect     │
 └──────────┬───────────┘
            │ AXI4 总线
            ▼
 ┌──────────────────────┐
 │ SimDDR(AXI4 Slave)   │
 └──────────┬───────────┘
            │
            ▼
       DDR/内存镜像(p_memory)
```

### 2) 库集成模式（外部 SoC/DDR 驱动）

```text
     +--------------------------------------------------------------+
     |                    你的 SoC/验证环境                         |
     |  (DDR 控制器 / 外设模型 / FPGA 侧 AXI 逻辑，按周期驱动)     |
     +--------------------------┬-----------------------------------+
                                │  sc_axi4_in_t / sc_axi4_out_t
                                │  (AR/AW/W/R/B 全通道)
                                ▼
                    +-------------------------------+
                    |  sc_sim_step(...) 黑盒        |
                    |  [RV32 Core + Interconnect]   |
                    +-------------------------------+
```

说明：
- 在库模式下，`sc_sim_step` 一次调用对应一个仿真周期。
- 外部系统只需要关心 AXI4 端口输入输出，不需要感知内部 CPU/Interconnect 实现细节。

## 目录

```text
singlecycle-axi4-sim/
├── CMakeLists.txt
├── Makefile
├── examples/
│   └── demo_api_with_simddr.cpp  # 库调用示例（同一源码可链接 .a/.so）
├── include/
│   ├── sc_axi4_sim_api.h        # 对外 C API（周期步进）
│   └── ...
├── src/
│   ├── sc_axi4_sim_api.cpp      # API 实现（非阻塞状态机）
│   ├── main.cpp                 # CLI：API + SimDDR 适配器
│   ├── axi/
│   ├── simddr/
│   └── cpu/
├── third_party/softfloat/softfloat.a
├── tools/run_regression.sh
└── bin/                         # 示例镜像（包含 coremark/dhrystone，不包含 linux）
```

## 环境依赖

- C++20 编译器（`g++` 或 `clang++`）
- `cmake >= 3.16`（推荐）或 `make`
- `bash`、`timeout`
- `zlib`

## 构建

### 1) CMake（推荐）

```bash
cmake -S . -B build
cmake --build build -j8
```

产物：

- `build/single_cycle_axi4.out`
- `build/libsingle_cycle_axi4.a`
- `build/libsingle_cycle_axi4.so`

### 2) Makefile

```bash
make -j8                 # 构建可执行程序
make libs -j8            # 构建静态库+动态库
```

产物：

- `single_cycle_axi4.out`
- `libsingle_cycle_axi4.a`
- `libsingle_cycle_axi4.so`
- `examples/demo_api_static.out`
- `examples/demo_api_shared.out`

说明：

- `libsingle_cycle_axi4.a` 可直接静态链接。  
- 由于仓库内 `softfloat.a` 非 PIC，`libsingle_cycle_axi4.so` 采用未解析 softfloat 符号方式构建，宿主程序需提供 softfloat 符号（推荐直接使用静态库方案）。

### Demo（调用静态库/动态库）

Makefile:

```bash
make demos -j8
```

运行：

```bash
./examples/demo_api_static.out bin/dhrystone.bin --max-inst 2000000
./examples/demo_api_shared.out bin/dhrystone.bin --max-inst 2000000
```

两者都调用同一个 C API：`sc_sim_create/sc_sim_load_image/sc_sim_step`。

## 运行可执行程序

```bash
./single_cycle_axi4.out bin/dhrystone.bin
./single_cycle_axi4.out bin/coremark.bin
./single_cycle_axi4.out --max-inst 20000000 --max-cycles 3000000000 bin/linux.bin
```

可选参数：

- `--max-inst <N>`
- `--max-cycles <N>`
- `TARGET_INST=<N>`（环境变量，覆盖最大提交指令数）

## 对外周期 API

头文件：`include/sc_axi4_sim_api.h`

AXI4 信号明细（按 AR/AW/W/R/B 通道划分 + 方向）：`docs/axi4_interface.md`

关键接口：

- `sc_sim_create/sc_sim_destroy`：创建/销毁句柄  
- `sc_sim_load_image`：加载镜像并复位内部状态  
- `sc_sim_set_limits`：设置停止条件  
- `sc_sim_step`：推进 **一个周期**  
- `sc_sim_get_status`：读取状态  

`sc_sim_step` 的语义：

- 输入 `sc_axi4_in_t`：外部设备在本周期给出的 AXI 从设备信号（`arready/rvalid/...`）  
- 输出 `sc_axi4_out_t`：CPU+Interconnect 在本周期给出的 AXI 主设备信号（`arvalid/araddr/...`）  
- 输出 `sc_sim_status_t`：周期计数、指令计数、停机状态、UART 事件等

返回值：

- `0`：继续运行
- `1`：正常停止（成功）
- `-1`：异常停止（失败）

## 周期对齐说明（面向 FPGA 协同）

- API 模式下，模型已改为非阻塞状态机：一次 `sc_sim_step` 严格对应一拍。  
- 不再使用原先 `main` 里多拍阻塞的 `axi_blocking_read/write` 方式。  
- 因此可以直接在外部按固定时钟驱动输入并采样输出。

## UART 输出

- `sc_sim_status_t` 带有 `uart_valid` 与 `uart_ch`。  
- CLI 会把 UART 字符打印到 `stdout`。  
- 外部系统可根据状态自行消费 UART 事件。

## AXI 波形跟踪（可选）

运行 CLI 时可开启：

- `AXI_TRACE=1`
- `AXI_TRACE_FILE=axi4_trace.csv`（可选）
- `AXI_TRACE_MAX_CYCLES=<N>`（可选）

## 回归脚本

```bash
tools/run_regression.sh
```

脚本会优先使用 CMake；若找不到 `cmake`，会回退到 `make`。

## Commit 规范与硬性检查

本仓库提供版本化 `commit-msg` hook 与 lint 工具：

- `.githooks/commit-msg`
- `tools/commit_msg_lint.py`

一次性启用：

```bash
bash tools/setup_githooks.sh
```

或：

```bash
git config core.hooksPath .githooks
```

启用后，`git commit` 会自动检查提交信息格式，并拒绝字面量 `\\n`（必须使用真实换行）。
