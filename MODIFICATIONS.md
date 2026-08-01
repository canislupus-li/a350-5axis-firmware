# A350 + 5-axis V10 修改说明

> **V10 = 5-axis 端口修复 + 标准 packaging**
> 基于 Snapmaker2-Controller **v5.2.1** (commit 5.2.1, 2025-11-26) fork。
> 目标:在不破坏 A350 现有功能（3D 打印、CNC、激光、HMI、CAN）的前提下,给主控加第 5 个 axis (A 轴),G-code 字符 `A` 与 A400 完全兼容。

---

## 1. 设计概要

### 1.1 轴命名:用 `J_AXIS=4`,不用 `A_AXIS`

A350 固件 (Marlin 2.0 era) **已经用 `B_AXIS=3` 作为 rotary module 的 CAN 协议握手值**。rotary module 固件是固化产品(我们改不了),只识别 `B_AXIS=3`。

如果直接用 Marlin 2.1+ 的 `A_AXIS` 命名,需要改 rotary module 固件——**做不到**。

**解决方案**:
- `B_AXIS = 3` 保持不变(rotary module 协议)
- 内部新增 `J_AXIS = 4` 表示 A 轴(Marlin 2.0 era 的 IJK 模式命名)
- G-code 字符 `A`(因为 `axis_codes[4] = 'A'`)

G-code 表现:
- `A` 字符 → A 轴电机
- `B` 字符 → B 轴电机
- Marlin 内部把 `A` 解析成 `J_AXIS=4`,把 `B` 解析成 `B_AXIS=3`
- CAN 协议**永远发 `B_AXIS=3`**(rotary module 固件不区分 A/B)

### 1.2 多实例 RotaryModule

`rotary_module.cpp` 现在有两个实例:
```cpp
RotaryModule rotaryModuleB(B_AXIS);  // 现有 - P6
RotaryModule rotaryModuleA(J_AXIS);  // 新增  - P2
```

每个实例独立处理自己的 `axis_` 字段(Marlin 内部路由),但 CAN 协议都发 `B_AXIS=3`。
`#define rotaryModule rotaryModuleB` 保留向后兼容。

### 1.3 端口分配 (V10)

| 轴 | G-code 字符 | Enum | 8-pin 端口 |
|---|---|---|---|
| X | X | 0 | PORT_8PIN_3 |
| Y | Y | 1 | PORT_8PIN_4 |
| Z | Z | 2 | PORT_8PIN_5 |
| **A (5-axis 新增)** | **A** | **4 (J_AXIS)** | **PORT_8PIN_2** |
| B (现有 rotary) | B | 3 | PORT_8PIN_6 |
| E | E | 5 | PORT_8PIN_1 (CNC) |

A 轴的 STEP/DIR/EN 信号 (`J_STEP_PIN/J_DIR_PIN/J_ENABLE_PIN`):
- `J_STEP_PIN` = `j_step_pin` (kernel 变量,默认 PE13)
- `J_DIR_PIN`  = `j_dir_pin`   (kernel 变量,默认 PC10)
- `J_ENABLE_PIN` = `j_enable_pin` (kernel 变量,默认 PC11)

具体 GPIO 引脚由 kernel 根据 `DEFAULT_AXIS_TO_PORT` 分配。

---

## 2. 修改的文件清单 (相对 v5.2.1)

### 2.1 核心 axis 框架

| 文件 | 改动 |
|---|---|
| `Marlin/src/core/enum.h` | 加 `J_AXIS = 4`, `E_AXIS` 5→5(原 4) |
| `Marlin/src/core/macros.h` | `NUM_AXIS` 5→6, `X_TO_E` 5→6, `XN` 4→5, `ABCE` 4→5 |
| `Marlin/src/core/types.h` | `NUM_AXES` fallback 4→5 |
| `Marlin/Configuration.h` | 3 个数组加第 5 轴 (A 在第 4 位 X/Y/Z/B/A/E) |
| `Marlin/Configuration_adv.h` | 加 `INVERT_J_STEP_PIN false` |

### 2.2 Stepper / Planner

| 文件 | 改动 |
|---|---|
| `Marlin/src/module/stepper.cpp` | 加 `j_step_pin/j_dir_pin/j_enable_pin` 变量;加 `J_APPLY_DIR/J_APPLY_STEP` 宏;`SET_AXIS_VALUE(j, J)` 初始化;`delta_error[]` `advance_dividend[]` 加 J 项;`set_position` 加 `j` 参数 |
| `Marlin/src/module/stepper.h` | `set_position` 签名加 `j` 参数 |
| `Marlin/src/module/stepper_indirection.h` | 加 `J_ENABLE_INIT/WRITE/READ`, `J_DIR_INIT/WRITE/READ`, `J_STEP_INIT/WRITE/READ` 宏 |
| `Marlin/src/module/planner.cpp` | `_populate_block`: `delta_mm[J_AXIS]`, `unit_vec[]`, `junction_cos_theta`, `junction_unit_vec[]` 4 处加 J 项(5-axis 联动 junction 速度) |
| `Marlin/src/module/planner.h` | `J` 项相关声明 |
| `Marlin/src/module/motion.cpp` | `J` 项相关 |
| `Marlin/src/module/motion.h` | `J` 项相关 |
| `Marlin/src/gcode/calibrate/G28.cpp` | G28 加 J axis homing(`set_axis_is_at_home`) |
| `Marlin/src/gcode/control/M17_M18_M84.cpp` | M17/M18/M84 加 `axis_codes[J_AXIS]` |

### 2.3 引脚定义 (A350 GD32F305VG)

| 文件 | 改动 |
|---|---|
| `Marlin/src/pins/pins_GD32F1.h` | 加 `j_step_pin/j_dir_pin/j_enable_pin` extern;`J_STEP_PIN/J_DIR_PIN/J_ENABLE_PIN` 宏;`DEFAULT_AXIS_TO_PORT` 加第 5 轴 (V10: B=P6, A=P2) |

### 2.4 Snapmaker 模块层

| 文件 | 改动 |
|---|---|
| `snapmaker/src/module/can_host.cpp` | rotary module 探测循环,try 所有 matching instances(每个 instance 探自己的物理端口) |
| `snapmaker/src/module/rotary_module.h` | `RotaryModule` 构造加 `AxisEnum axis_` 参数;`rotaryModuleA(J_AXIS)` / `rotaryModuleB(B_AXIS)` 实例;`#define rotaryModule rotaryModuleB` 向后兼容 |
| `snapmaker/src/module/rotary_module.cpp` | A/B 实例化,probe 用各自 axis 的 DIR pin,日志按 axis 区分,所有 CAN 命令发 `B_AXIS=3` |
| `snapmaker/src/module/linear.h` | `axis_steps_per_unit[5]` → `[NUM_AXIS]` |
| `snapmaker/src/module/module_base.h` | 必要的声明更新 |
| `snapmaker/src/module/module_base.cpp` | `static_modules[]` 加 `&rotaryModuleA, &rotaryModuleB` |

### 2.5 Build infrastructure (V10 新增)

| 文件 | 改动 |
|---|---|
| `snapmaker/scripts/pack.py` | major image filename 前缀 `Snapmaker_` → `SM2_`(屏幕显示完整);PACKAGE 字段保持 `Snapmaker_` 前缀(V1.x 78MB factory bin 约定,改了烧不上) |
| `.github/workflows/build.yml` | artifact upload 路径改成 `release/SM2_*.bin` |

---

## 3. V10 烧录 bin

```
release/SM2_V5.2.1-5AXIS-V10_20260801.bin_20260801.bin   301,608 B
```

- **PACKAGE** (offset 53): `Snapmaker_V5.2.1-5AXIS-V10` — bootloader 烧写后写到 BOOT_PARA+2048
- **Filename** (offset 2): `SM2_V5.2.1-5AXIS-V10_20260801.b` (截断到 31 字符)
- **SHORT_BUILD_VERSION**: `SM2-5.2.1-5AXIS-V10` (M115 返回)
- **编译时间**: `Aug 1 2026, 11:36:54`

### 3.1 烧录验证 (logbin/521AB test.txt, 2026-08-01)

| 测试 | 0x206073B4 (B) | 0x20609302 (A) | 结果 |
|---|---|---|---|
| AB 都接 | B detected / A unusable | B unusable / A detected | ✓ 两个都识别 |
| 只接 B (P6) | B detected | (缺席) | ✓ B 识别 |
| 只接 A (P2) | (缺席) | B unusable / **A detected** | ✓ A 识别 |
| G0 A60 | — | — | A:60.00 ✓ |
| G0 B60 | — | — | B:60.00 ✓ |
| M114 位置 | A:60.00 B:60.00 | — | ✓ |
| M114 计数 | B:53333 J:53333 | — | ✓ |

---

## 4. 兼容性 / 已知限制

### 4.1 完全保留 (跟 v5.2.1 一致)

| 功能 | 状态 | 验证 |
|---|---|---|
| 3D 打印 (XYZE) | ✅ | Luban 加载 bin,运行标准 3D G-code |
| CNC (XYZE + spindle) | ✅ | 同上 |
| 激光 (XY + laser) | ✅ | 同上 |
| B 轴 rotary module | ✅ | B=G-code 字符,B=P6 物理端口 |
| Luban / HMI 烧录 | ✅ | V1.21.0 HMI apk 接受 `Snapmaker_*` PACKAGE 的 bin |
| HMI 屏幕 | ✅ | 不报错(但有限制,见下) |

### 4.2 新增 (V10)

| 功能 | 状态 | 验证 |
|---|---|---|
| A G-code (字符) | ✅ | `G0 A10` 驱动 A 模组 |
| A 模组 (P2) 单独识别 | ✅ | logbin 测试 3 |
| AB 双模组独立控制 | ✅ | logbin 测试 1 |
| 5-axis 联动 junction 速度 | ✅ | planner 加 J 项,5-axis 联动插补 |

### 4.3 已知限制

- **HMI apk 显示**:V1.21.0 HMI apk (4-axis) 不知道 `J_AXIS=4` (A-axis),屏幕不显示 A 轴模组。Controller 端完全正常,只是 HMI UI 没渲染 A。这是 HMI apk 限制,不是 controller bug。要修 HMI 是个独立工程(改 HMI apk 源码重打包签名)。
- **CAN 协议固定为 B_AXIS=3**:rotary module 固件是固化产品,只识别 `B_AXIS=3`。A 和 B 模块物理独立(各自独立的 8-pin 端口),但 CAN 协议握手值相同。Marlin 内部按 `axis_` 字段路由到正确步进电机。

---

## 5. 编译和烧写

### 5.1 本地编译 (PlatformIO)

```bash
# 编译
pio run -e GD32F105

# 打包 (pack.py 从 Version.h 读 SHORT_BUILD_VERSION 自动生成文件名)
python snapmaker/scripts/pack.py -c stage/firmware.bin -d . -vc "V$(grep -oP 'SM2-\K[0-9.][^"\s]*' Marlin/src/inc/Version.h | head -1)"
```

### 5.2 烧录

- **HMI 屏幕烧录**:在 HMI 设置里选 bin 文件,正常烧录流程
- **SWD/J-Link (开发用)**:`loadfile firmware.bin 0x800B800`(MARLIN slot A)

---

## 6. 后续修改 / 新版本

这个文档是 V10 的修改说明。后续添加功能 / 修改,会作为新版本(V11, V12, ...)记录,各自有独立的修改说明。
