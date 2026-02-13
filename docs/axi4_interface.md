# AXI4 接口说明（按通道划分）

本文档描述 `sc_sim_step()` 的 AXI4 交互接口，信号定义来源于 `include/sc_axi4_sim_api.h`。

## 1. 接口总览

- `sc_axi4_out_t`：**本项目输出给外部设备**（Master -> Slave）
- `sc_axi4_in_t`：**外部设备输入给本项目**（Slave -> Master）

这里“Master/Slave”均按 AXI 协议角色定义，CPU+Interconnect 一侧为 Master。

## 2. `sc_axi4_out_t`（Master -> Slave）

### AR 通道（读地址）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `arvalid` | 1 | Master->Slave | 读地址有效 |
| `arid` | 8 | Master->Slave | 读事务 ID |
| `araddr` | 32 | Master->Slave | 读地址（字节地址） |
| `arlen` | 8 | Master->Slave | burst 长度减 1 |
| `arsize` | 8 | Master->Slave | 每拍字节数编码 |
| `arburst` | 8 | Master->Slave | burst 类型 |

### AW 通道（写地址）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `awvalid` | 1 | Master->Slave | 写地址有效 |
| `awid` | 8 | Master->Slave | 写事务 ID |
| `awaddr` | 32 | Master->Slave | 写地址（字节地址） |
| `awlen` | 8 | Master->Slave | burst 长度减 1 |
| `awsize` | 8 | Master->Slave | 每拍字节数编码 |
| `awburst` | 8 | Master->Slave | burst 类型 |

### W 通道（写数据）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `wvalid` | 1 | Master->Slave | 写数据有效 |
| `wdata` | 32 | Master->Slave | 写数据 |
| `wstrb` | 8 | Master->Slave | 写字节掩码（低 4bit 有效） |
| `wlast` | 1 | Master->Slave | burst 最后一拍 |

### R 通道握手（读数据就绪）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `rready` | 1 | Master->Slave | Master 可接收读数据 |

### B 通道握手（写响应就绪）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `bready` | 1 | Master->Slave | Master 可接收写响应 |

## 3. `sc_axi4_in_t`（Slave -> Master）

### AR 通道握手（读地址接收）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `arready` | 1 | Slave->Master | Slave 可接收读地址 |

### AW 通道握手（写地址接收）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `awready` | 1 | Slave->Master | Slave 可接收写地址 |

### W 通道握手（写数据接收）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `wready` | 1 | Slave->Master | Slave 可接收写数据 |

### R 通道（读数据响应）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `rvalid` | 1 | Slave->Master | 读数据有效 |
| `rid` | 8 | Slave->Master | 读响应 ID |
| `rdata` | 32 | Slave->Master | 读返回数据 |
| `rresp` | 8 | Slave->Master | 读响应状态 |
| `rlast` | 1 | Slave->Master | burst 最后一拍 |

### B 通道（写响应）

| 信号 | 位宽 | 方向 | 说明 |
|---|---:|---|---|
| `bvalid` | 1 | Slave->Master | 写响应有效 |
| `bid` | 8 | Slave->Master | 写响应 ID |
| `bresp` | 8 | Slave->Master | 写响应状态 |

## 4. 每拍调用时序建议

每个仿真周期建议按以下顺序调用：

1. 外设采样并填充 `sc_axi4_in_t`
2. 调用 `sc_sim_step(handle, &in, &out, &status)`
3. 外设消费 `sc_axi4_out_t`，完成本拍握手与状态推进
4. 进入下一拍

这保证“`sc_sim_step` 一次调用 = 一个周期”。
