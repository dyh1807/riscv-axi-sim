---
name: simddr-backend-dev
description: "Workflow for adding/refactoring SimDDR + interconnect backends (AXI variants) in this repo; covers two-phase comb/seq, build wiring, unit tests, and regressions."
---

# SimDDR 后端开发工作流（AXI3/AXI4/自定义总线）

适用场景：
- 新增/替换 SimDDR 后端（例如 AXI4 → AXI3、增加 interconnect、增加端口）
- 修复 ready/valid 时序、latch/backpressure、burst 拆分/拼接等协议问题
- 为新后端补齐单测 + 快速回归（CoreMark/Dhrystone/Linux smoke）

## 0. 约束先行（写在代码/注释/commit body 里）
- **数据宽度/beat 大小**：例如 32b/256b
- **ID 宽度/交错支持**：是否允许 R/B interleave？是否允许写命令/写数据交错？
- **burst 限制**：仅 INCR？len 上限？是否支持窄 burst？
- **上游接口语义**：上游是否只发 1~32B？`resp.data[0]` 对应 `req.addr` 的字节序/对齐语义是什么？

## 1. 模块形态（two-phase comb/seq）
建议所有新模块都提供：
- `init()`：清状态/IO 默认值
- `comb_outputs()`：只根据“当前寄存器状态 + 当前输入”计算 `ready/valid/data`
- `comb_inputs()`：只做“输入采样/组合逻辑”（尽量薄，必要时 no-op）
- `seq()`：只在握手条件下更新寄存器（flip-flop 行为）

关键原则：
- **comb 不修改状态**；状态只在 `seq()` 改
- **同一个周期**通常按：`ddr.comb_outputs → intlv.comb_outputs → cpu.cycle → intlv.comb_inputs → ddr.comb_inputs → ddr.seq/intlv.seq`

## 2. SimDDR 模型实现要点
- **响应生命周期**：如果要支持 backpressure，`bvalid/rvalid` 必须在 `ready=0` 时保持（通过 pending 队列/active state 实现）
- **握手时机**：测试 backpressure 时，`*_ready` 要在 `*_valid` 出现前就拉低，否则会发生“valid 首次出现即被同周期消费”的误判
- **写掩码**：建议用 byte-level reference 或按 nibble/bit 展开（参考 `sim_ddr/sim_ddr_test.cpp` 与 `sim_ddr/sim_ddr_axi3_test.cpp`）

## 3. Interconnect/Bridge 实现要点
- **ready-first**：上游 `req.ready` 常用寄存器打一拍（避免 two-phase 下游看到 ready 的周期错位）
- **latch 合规**：AR/AW `valid` 必须在下游 `ready=0` 时保持，直到握手
- **元信息保活**：如果下游 backpressure 会导致上游请求信息无法再读取，优先：
  - 打包到 AXI ID（简单且鲁棒），或
  - 在 bridge 内部寄存器 latch（需要处理并发/覆盖）
- **响应清除时序**：避免“同周期产生 resp 且同周期被 ready 清掉导致上游看不到”，可在 `seq()` 里对 `resp_valid` 做 **snapshot** 再处理握手

## 4. 接入点：MemorySubsystem
- 只在 `include/MemorySubsystem.h` 做一次“后端选择/IO wiring”
- 默认选择策略建议放在这里（例如 `#ifndef USE_SIM_DDR_AXI4` 默认走 AXI3）

## 5. 构建与开关
推荐策略：
- **默认不注入宏**，由头文件 `#ifndef` 给默认后端；需要切换时再显式定义宏
- CMake/Makefile 只提供 **可选**开关（例如 `USE_SIM_DDR_AXI4`）

参考：
- CMake：`CMakeLists.txt`
- Makefile：`Makefile`

## 6. 单测与压力测试（强制项）
至少覆盖：
- AR/AW latch + 下游 ready backpressure
- R/B backpressure（ready 先拉低，再等待 valid）
- 非对齐访问（offset）与跨 beat（最多 2 beat）拼接/拆分
- ID 路由/打包解码正确
- 随机读写 stress（byte 级 reference model）

可以直接复用现有 harness 结构：
- AXI4：`sim_ddr/sim_ddr_test.cpp`、`axi_interconnect/axi_interconnect_test.cpp`
- AXI3：`sim_ddr/sim_ddr_axi3_test.cpp`、`axi_interconnect/axi_interconnect_axi3_test.cpp`

## 7. 回归建议（最小集合）
- 跑单测：`sim_ddr_*_test` + `axi_interconnect_*_test`
- 跑 baremetal：CoreMark/Dhrystone（见 `$verify-simulator`）
- Linux smoke：`timeout 60s ./a.out baremetal/linux.bin`

## 8. 提交（避免 commit 换行变成字面量 \\n）
建议用多 `-m` 或 `$'...'`：
```bash
git commit -m "feat(simddr): xxx" -m $'- line1\n- line2\n- line3'
```

