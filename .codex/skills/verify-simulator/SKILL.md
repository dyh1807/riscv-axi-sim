---
name: verify-simulator
description: "Simulator validation and test runs for Linux boot, CoreMark, and Dhrystone in this project; use when verifying simulator correctness or running quick regression commands."
---

# 模拟器验证技能 / Simulator Verification Skills

本文档记录了项目编译后的常用验证测试方法。

## 1. Linux Kernel 启动测试
**用途**: 验证模拟器对 Linux 内核启动的支持，涉及复杂的虚实地址转换 (MMU) 和长时间运行。

*   **基本命令**:
    ```bash
    ./a.out baremetal/linux.bin
    ```
*   **超时验证**:
    由于 Linux 启动耗时较长，可以使用 `timeout` 命令（如设置 60 秒）来验证模拟器能否在指定时间内稳定运行，用于快速回归测试。
    ```bash
    timeout 60s ./a.out baremetal/linux.bin
    ```

## 2. Baremetal 基准测试 (CoreMark & Dhrystone)
**用途**: 验证 CoreMark 和 Dhrystone 基准测试正确性。相比 Linux，这些是较小的裸机程序，不涉及复杂的地址转换，适合快速验证 CPU 核心逻辑。

*   **Dhrystone 测试**:
    ```bash
    ./a.out baremetal/new_dhrystone/dhrystone.bin
    ```
*   **CoreMark 测试**:
    ```bash
    ./a.out baremetal/new_coremark/coremark.bin
    ```
