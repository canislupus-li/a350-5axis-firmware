# A350 + 5-axis 修改说明 (vs. v5.2.1 原厂)

> 基于 Snapmaker2-Controller **v5.2.1** (commit 5.2.1, 2025-11-26) 做的所有修改。
> 目标:在不破坏 A350 现有功能（3D 打印、CNC、激光、HMI、CAN）的前提下,给主控加第 5 个 axis (A 轴),G-code 字符 `A` 与 A400 完全兼容。

---

## 1. 关键设计决策

### 1.1 为什么不用 `A_AXIS`,用 `J_AXIS`?

A350 固件 (Marlin 2.0 era) **已经用 `B_AXIS=3` 作为 rotary module 的 CAN 协议握手值**。rotary module 固件是固化产品(我们改不了),它只识别 `B_AXIS=3` 这个 magic value。

如果直接用 Marlin 2.1+ 的 `A_AXIS` 命名,需要改 rotary module 固件——**做不到**。

**解决方案**:
- `B_AXIS = 3` 保持不变(继续走 rotary module 协议)
- 内部新增 `J_AXIS = 4` 表示 A 轴(Marlin 2.0 era 的 IJK 模式命名)
- G-code 字符保留 `A`(因为 `axis_codes[4] = 'A'`)

G-code 表现:
- `A` 字符驱动 A 轴电机
- `B` 字符驱动 B 轴电机
- Marlin 内部把 `A` 解析成 `J_AXIS=4`,把 `B` 解析成 `B_AXIS=3`
- CAN 协议**永远发 `B_AXIS=3`**(rotary module 固件不区分 A/B)

### 1.2 多实例 RotaryModule

`snapmaker/src/module/rotary_module.cpp` 现在有两个实例:
```cpp
RotaryModule rotaryModuleB(B_AXIS);  // 现有
RotaryModule rotaryModuleA(J_AXIS);  // 新增 (5-axis upgrade)
```

每个实例独立处理自己的 `axis_` 字段(Marlin 内部路由),但 CAN 协议都发 `B_AXIS=3`。
宏 `#define rotaryModule rotaryModuleB` 保留向后兼容。

### 1.3 硬件引脚 (A350 GD32F305VG)

| 轴 | 字符 | Enum | Pin | 8-pin 端口 |
|---|---|---|---|---|
| X | X | 0 | PC6 | PORT_8PIN_3 |
| Y | Y | 1 | PB4 | PORT_8PIN_4 |
| Z | Z | 2 | PB7 | PORT_8PIN_5 |
| B (现有 rotary) | B | 3 | PA1 | PORT_8PIN_6 |
| **A (新增)** | **A** | **4 (J_AXIS)** | **PE13** | **PORT_8PIN_2** |
| E | E | 5 | PE14 | PORT_8PIN_1 (CNC) |

**注意**: A 轴的 STEP/DIR/EN 三个信号是:
- STEP = PE13
- DIR  = PC10
- EN   = PC11

(走 PORT_8PIN_2 8-pin 端口,需要在 A350 硬件层把 8-pin 端口 2 接到第二个 rotary module 上。)

---

## 2. 修改的文件清单

### 2.1 核心 axis 框架

| 文件 | 改动 |
|---|---|
| `Marlin/src/core/enum.h` | 加 `J_AXIS = 4`, `E_AXIS = 5`(原来是 4) |
| `Marlin/src/core/macros.h` | `NUM_AXIS 5→6`, `X_TO_E 5→6`, `XN 4→5`, `ABCE 4→5` |
| `Marlin/src/core/utility.cpp` | `axis_codes[6] = { 'X', 'Y', 'Z', 'B', 'A', 'E' }`(A 在第 4 位) |
| `Marlin/src/core/types.h` | `NUM_AXES` fallback `4→5`(5 linear axes: X/Y/Z/I/J) |

### 2.2 Stepper 控制

| 文件 | 改动 |
|---|---|
| `Marlin/src/module/stepper.cpp` | 加 `j_step_pin/j_dir_pin/j_enable_pin` 变量;加 `SET_AXIS_VALUE(j, J)`;加 `J_APPLY_DIR/J_APPLY_STEP` 宏;删 orphan `SET_AXIS_VALUE(a, A)`(A_AXIS 在 A350 Marlin 2.0 era 不存在) |
| `Marlin/src/module/stepper_indirection.h` | 加 `J_ENABLE_INIT/WRITE/READ`, `J_DIR_INIT/WRITE/READ`, `J_STEP_INIT/WRITE/READ` 宏 |

### 2.3 引脚定义 (A350 GD32F305VG)

| 文件 | 改动 |
|---|---|
| `Marlin/src/pins/pins_GD32F1.h` | 加 `j_step_pin` extern;加 `J_STEP_PIN/J_DIR_PIN/J_ENABLE_PIN` 宏;`axis_to_port[6]` 包含 `PORT_8PIN_2` |

### 2.4 Configuration

| 文件 | 改动 |
|---|---|
| `Marlin/Configuration.h` | `DEFAULT_AXIS_STEPS_PER_UNIT`、`DEFAULT_MAX_FEEDRATE`、`DEFAULT_MAX_ACCELERATION` 三个数组 5→6 元素 (X,Y,Z,B,A,E 格式),A 插在第 4 位 |
| `Marlin/Configuration_adv.h` | 加 `INVERT_J_STEP_PIN false` |

### 2.5 Snapmaker rotary module

| 文件 | 改动 |
|---|---|
| `snapmaker/src/module/rotary_module.h` | `static_modules` 数组加 `&rotaryModuleA, &rotaryModuleB`(顺序:B 后 A) |
| `snapmaker/src/module/rotary_module.cpp` | 多实例化,`rotaryModuleA(J_AXIS)`,Init 阶段 A 轴用绝对引脚 `PE13/PC10/PC11` 驱动 DIR;CAN 协议**总是发 `B_AXIS=3`**(rotary module 固件不区分 A/B) |

### 2.6 Planner (junction 速度计算)

| 文件 | 改动 |
|---|---|
| `Marlin/src/module/planner.cpp` | `_populate_block`:加 `dj = target[J_AXIS] - position[J_AXIS]` 变量;`delta_mm[J_AXIS] = dj * steps_to_mm[J_AXIS]` 初始化;`unit_vec[]`、`junction_cos_theta`、`junction_unit_vec[]` 三处加 J_AXIS 项(5-axis 联动 junction 速度) |

### 2.7 linear.h (snapmaker)

| 文件 | 改动 |
|---|---|
| `snapmaker/src/module/linear.h` | `axis_steps_per_unit[5]` → `[NUM_AXIS]`(数组大小跟 axis 数对齐) |

### 2.8 编译/CI

| 文件 | 改动 |
|---|---|
| `.github/workflows/build.yml` | 加 `pack.py` 步骤,自动产出 Snapmaker 标准的 `SM2_MC_APP_*.bin`(Luban 可识别) |
| `README-GITHUB.md` | 项目说明文档 |

---

## 3. 编译错误修复历史 (踩坑记录)

按 commit 顺序记录——给后人(和我自己)一个 debug 参考。

| # | Commit | 错误 | 修复 |
|---|---|---|---|
| 1 | `4de397f` | `snapmaker/src/module/linear.h:122: array subscript is above array bounds` | `axis_steps_per_unit[5]` → `[NUM_AXIS]` |
| 2 | `f704be3` | `rotary_module.cpp:34: 'digitalWriteFast' was not declared` | `digitalWriteFast` → `digitalWrite`(snapmaker 模块没 include Marlin fastio) |
| 3 | `67aace0` | `types.h:122: 'delta_mm[4]' may be used uninitialized` + `ft_motion.cpp:881: array subscript is above array bounds` | `NUM_AXES 4→5`(5 linear axes: X/Y/Z/I/J) + `planner.cpp` `unit_vec[]` 加 J_AXIS 项 |
| 4 | `944e9f1` | `stepper.cpp:3432: 'J_APPLY_STEP' was not declared` | 加 `J_APPLY_DIR/J_APPLY_STEP` 宏(参考 B_APPLY_STEP 复制) |
| 5 | `d50c902` | `stepper.cpp:2220: 'A_AXIS' was not declared` + `planner.cpp:2483: 'delta_mm[4]' may be used uninitialized` | 删 orphan `SET_AXIS_VALUE(a, A)` + `planner.cpp` 加 `dj` 变量和 `delta_mm[J_AXIS]` 初始化 |
| 6 | `492fad1` | (CI) `release/SM2_MC_APP_*.bin not found` | pack.py 默认输出到 `dirname(release/) /release` = `release/release/`,加 `-d .` 参数修正 |

---

## 4. 构建流程

### 4.1 云端编译 (推荐)

```bash
git push origin main
# 5-10 分钟后,GitHub Actions 自动跑完
# 在 https://github.com/canislupus-li/a350-5axis-firmware/actions 下载 artifact
```

**3 个 artifact**:
- `firmware-GD32F105` (194KB) — raw PlatformIO 输出
- `firmware-GD32F105-elf` (3.8MB) — 调试用
- `firmware-SM2-MC-APP-GD32F105` (194KB) — **Luban 可识别的标准格式**(pack.py 加 2048 字节 header)

### 4.2 本地编译

需要 ARM toolchain + PlatformIO。网络不好的话(国内常见),用云端编译更稳。

```bash
# 安装一次
pip install -U platformio

# 编译
pio run -e GD32F105
# 产物:.pioenvs/GD32F105/firmware.bin

# 打包成 Luban 格式
python snapmaker/scripts/pack.py -c .pioenvs/GD32F105/firmware.bin -d . -vc V5.2.1-5AXIS
# 产物:release/SM2_MC_APP_V5.2.1-5AXIS_YYYYMMDD.bin
```

---

## 5. 烧写

### 5.1 J-Link (开发/调试用)

```
connect          # 选 GD32F305VE
erase
loadfile firmware.bin 0x8000000
reset
go
```

### 5.2 Luban (生产/普通用户)

Luban 打开 `SM2_MC_APP_V5.2.1-5AXIS_YYYYMMDD.bin`,识别为 controller 固件,正常刷入。

---

## 6. 测试顺序

1. **不开模块,先上电** —— 主控启动正常,HMI 不报错(EEPROM、SD card)
2. `M114` —— 5 个 axis (X/Y/Z/B/A) 位置都该有
3. `M119` —— endstop 状态合理
4. `G0 A10 F1000` —— A 轴转 10 度,看方向对不对
5. 空跑 3D 打印 G-code —— 验证 XYZB 联动没坏
6. 接上第二个 rotary module —— `M1005` 检测应该 `online`

---

## 7. 已知限制

- **A 和 B 物理上独立**,但 CAN 协议**都用 `B_AXIS=3` 发给 rotary module 固件**。这意味着 A 和 B 实际**不能同时**通过两路 CAN——这取决于 rotary module 固件如何处理多 host。目前假设用户用两路独立的 rotary module CAN 通道(从 PORT_8PIN_2 出来的 8-pin 端口接第二个 rotary module)。
- **Junction 速度计算**已加 J 项,5-axis 联动插补时 J 轴参与速度限制。如果实际运动有问题,plan B 是让 J 不参与(改 `unit_vec[]` 加 J 项但 junction_cos_theta 不加)。
- **Axis 命名混乱**:`J_AXIS=4` 是 Marlin 2.0 era 的 IJK 模式术语,`A_AXIS` 是 Marlin 2.1+ 的术语。A350 用的 Marlin 2.0 兼容老 fork,所以保留 `J_AXIS` 命名(改了会破坏其他 IJK 模式代码)。

---

## 8. 兼容性矩阵

| 功能 | v5.2.1 原厂 | 本 fork | 验证方式 |
|---|---|---|---|
| 3D 打印 (XYZE) | ✅ | ✅ | 跑标准 3D 打印 G-code |
| CNC (XYZE + spindle) | ✅ | ✅ | 跑 CNC 测试 G-code |
| 激光 (XY + laser) | ✅ | ✅ | 跑激光测试 G-code |
| B 轴 rotary module | ✅ | ✅ | 接 B rotary module, `M1005` 检测 |
| **A 轴 (新)** | ❌ | ✅ | `G28 A` + `G0 A10` |
| 5-axis 联动插补 | ❌ | ✅ | `G1 X10 Y10 Z10 A45 B90` |
| Luban OTA 升级 | ✅ | ✅ | Luban 加载 `SM2_MC_APP_*.bin` 正常刷入 |
| HMI (屏幕) | ✅ | ✅ | 上电 HMI 不报错 |

---

## 9. 贡献 / 修改建议

如果你想改这个 fork:

- **改 axis 数**:同步改 `Marlin/src/core/enum.h` (J_AXIS/E_AXIS 值)、`Marlin/src/core/macros.h` (NUM_AXIS/X_TO_E)、`Marlin/src/core/types.h` (NUM_AXES)、`Marlin/Configuration.h` (3 个数组)、所有用到这些宏的代码
- **改引脚**:改 `Marlin/src/pins/pins_GD32F1.h` 的 J_STEP_PIN/J_DIR_PIN/J_ENABLE_PIN + `axis_to_port[]` 数组 + `port_to_pin` 在 `stepper.cpp:2208` 的 PORT_TO_STEP_PIN 映射
- **改 rotary module 协议**:**做不到**——rotary module 固件是固化产品,只识别 `B_AXIS=3`
- **Bump 版本号**:改 `Marlin/src/inc/Version.h` 的 `SHORT_BUILD_VERSION`(`SM2-x.x.x` 格式)——pack.py 会自动读它生成文件名
