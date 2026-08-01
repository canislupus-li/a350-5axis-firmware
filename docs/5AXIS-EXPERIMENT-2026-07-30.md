# 5AXIS 实验记录 — 2026-07-30

> 目标：让 A350 主控跑我们的 5AXIS 改造 Marlin 固件（v0-v9S）
> 结果：**A350 4.7.2 闭源 bootloader 完全不接受任何修改**。pack.py 流程整体被拒，主控段有签名/校验无法绕过。
> 当前状态：机器稳在 V4.7.2 production（V1.21.0 .bin），功能正常。

---

## 时间线（13 小时）

1. **早**：从 v0-v9S 的 GitHub Actions 自动 build 开始，pack.py 输出 300K 左右 .bin
2. **中段**：每次烧录后机器回退到 4.7.2 backup，发现 bootloader 拒所有 .bin
3. **下午**：发现 production V1.21.0 .bin 是 78MB，16896 字节头，结构完全不同
4. **分析**：production .bin = bootloader (16896) + 主控 raw (363K) + ZIP1 模组 + ZIP2 HMI APK + META-INF/CERT.RSA
5. **TAMPER 测试**：改 V1.21.0 主控段 9 字节，bootloader 拒了 → 主控段有强校验
6. **REPLACE 测试**：主控整段换 v9S，bootloader 拒了 + 触屏段被刷了 + 触发了"模组过期"循环
7. **恢复**：烧原版 V1.21.0 .bin 拉回稳态
8. **晚**：试本地 VSCode + PlatformIO 编译 4.7.2，下载太慢卡住；用现成 v0-v5.2.1-original-packed.bin 测 pack.py 流程，也被拒

---

## 关键发现（硬事实）

### Production V1.21.0 .bin 结构（78,001,373 字节）

```
[0..16895]       Bootloader (16896 字节) - 闭源 ARM 代码，含版本 metadata
[16896..379985]  主控 Marlin raw firmware (363090 字节) - 含 SM2-4.7.2
[379986..67290011] ZIP1 (409 项) - 模组/资源/签名 - 含 META-INF/CERT.RSA
[67290012..78001372] ZIP2 (1681 项) - HMI Android APK - 含 fabscreen_updating_1_3.apk
```

### 4.7.2 源码已知（开源部分）

- `Marlin/src/inc/Version.h`: SHORT_BUILD_VERSION "SM2-4.7.2"
- `Marlin/src/core/macros.h`: Flash 布局
  - BOOT_CODE: 32K (0x8000000-0x8008000) - bootloader
  - BOOT_PARA: 4K (0x8008000) - 启动参数
  - MARLIN: 488K (0x800B800+) - 主 app 区
  - UPDATE_CONTENT: 488K - OTA 升级缓冲
- `snapmaker/src/service/upgrade.cpp`: bootloader 升级逻辑（包写到 UPDATE_CONTENT，校验后复制到 MARLIN）
- `snapmaker/scripts/pack.py`: 开源打包工具（V1.1.0），只能产 48 字节大镜像头 + 2048 字节小镜像头

### Luban `firmware-build.js`（也是开源）

- `src/server/lib/firmware-build.js`
- **也是 48 字节大镜像头**（与 pack.py 一模一样）
- `feature-v4.4.0` 分支代码跟 main 一致
- Luban 不是答案，pack.py 流程就是 Luban 在用的流程

### 各 .bin 格式对比

| 格式 | Major header | 来源 | bootloader 接受 |
|------|-------------|------|----------------|
| pack.py 输出 (300K) | 48 字节 (39+9×1) | 我们/GitHub Actions 编译 | ✗ **拒**（快速拒，无串口输出）|
| Production V1.21.0 (78MB) | **16896 字节** | Snapmaker 内部工具（闭源）| ✓ 唯一接受 |
| TAMPER (V1.21.0 改 9 字节) | 16896 字节 | 我们修改主控段 | ✗ 拒 |
| REPLACE (V1.21.0 主控换 v9S) | 16896 字节 | 我们整体替换 | ✗ 拒 + 触发模组循环 |

### 版本字符串传递链

- pack.py 产物的 `version` 字段在第 5-36 字节（32 字节）
- 实际 Marlin `SHORT_BUILD_VERSION` 在 firmware 段内
- bootloader 似乎从 firmware 段内某处读 `SHORT_BUILD_VERSION` 而不是 header 的 version
- bootloader 校验会检查整段（CHANGELOG / UUID 等位置都有强校验）

### 关键观察

1. **pack.py .bin 烧录几秒就完成 + 串口无输出** = bootloader 看到 header 后**直接拒，没开始数据传输**
2. **TAMPER/REPLACE 烧录有完整流程 + 串口有输出** = bootloader 走完了升级流程，写完后才拒
3. **触屏 APK 段无校验**：TAMPER 时触屏版本变了
4. **ZIP1 模组段必须有配套版本**：REPLACE 时触发了"模组过期"循环
5. **机器有 4.6.6 backup 槽位**：多次失败后 bootloader 可能降级到 4.6.6

### HMI 字段含义

- **固件版本** = .bin header 的 version 字符串（bootloader 写入 BOOT_PARA）
- **触屏版本** = HMI Android app 自己的版本
- **控制器版本** = 当前跑 Marlin 的 `SHORT_BUILD_VERSION`（SM2-X.Y.Z）

---

## 资源（已下载到本地）

| 路径 | 内容 |
|------|------|
| `F:\孤狼\minmax\A350 add 5-axis\Snapmaker2-Controller-4.4.11\` | 4.4.11 源码（更老） |
| `F:\孤狼\minmax\A350 add 5-axis\Snapmaker2-Controller-4.7.2\` | 4.7.2 源码（**当前生产 backup 用的 Marlin**） |
| `F:\孤狼\minmax\A350 add 5-axis\Snapmaker2-Controller-5.2.1\` | 5.2.1 源码（实验性公开）|
| `F:\孤狼\minmax\A350 add 5-axis\Snapmaker2-Controller-5axis\` | **我们 5axis fork**（active）|
| `F:\孤狼\minmax\A350 add 5-axis\Snapmaker2-Modules-main\` | 模组固件源码 |
| `F:\孤狼\minmax\A350 add 5-axis\Controller2022-Marlin-a400-dev\` | A400 参考（用 J_AUXIS 字符）|
| `F:\孤狼\minmax\A350 add 5-axis\Luban-main\` | Luban 源码（git clone，浅）|
| `F:\孤狼\minmax\A350 add 5-axis\release\Snapmaker2_V1.21.0.bin` | production 78MB .bin |
| `F:\孤狼\minmax\A350 add 5-axis\release\v0..v9S` | 5axis 历版 pack.py 产物 |
| `F:\孤狼\minmax\A350 add 5-axis\release\experiment1_rename\` | 实验1：仅改文件名的 v9S .bin |
| `F:\孤狼\minmax\A350 add 5-axis\release\experiment3_tamper\` | TAMPER 版（拒了）|
| `F:\孤狼\minmax\A350 add 5-axis\release\experiment3_replace\` | REPLACE 版（拒了）|
| `F:\孤狼\minmax\A350 add 5-axis\release\experiment_baseline\` | pack.py baseline 验证（拒了）|
| `F:\孤狼\minmax\A350 add 5-axis\release\extracted_v1210\` | V1.21.0 解出的 ZIP1/ZIP2 |

### 提取的 V1.21.0 .bin 结构文件

- `zip1_modules.zip` (66MB) - 模组/资源 ZIP
- `zip2_hmi.zip` (10MB) - HMI Android APK

---

## 5axis 项目当前代码状态（5axis repo）

- 端口分配：A 在 PORT_8PIN_2（PE13/PC10/PC11），B 在 PORT_8PIN_6（PA1/PC12/PD2）
- 轴映射：J_AXIS=4 是 A，B_AXIS=3 是 B
- SHORT_BUILD_VERSION = "SM2-5.2.1-5AXIS-V9S"
- pack.py 已改：`Snapmaker_` → `Snapmaker2_` 前缀（Luban 兼容）
- build.yml artifact path 改：`release/Snapmaker2_*.bin`（从 SM2_MC_APP 改）
- 改动未 commit

---

## 路径分析：5AXIS 的真正障碍

| 路径 | 状态 |
|------|------|
| ① 自己产 16896 字节头 .bin | ✗ RSA 签名 + 闭源头格式 |
| ② 找老 Luban 能产 16896 头 | ✗ feature-v4.4.0 也是 48 字节头 |
| ③ SWD/DFU 直刷 | ✗ 用户决定不走 |
| ④ 找 Snapmaker 内部 pack 工具 | ✗ 闭源 |

**5AXIS 主控代码 = 这条路在 A350 + 4.7.2 闭源 bootloader 下，走不通**

---

## 真要 5AXIS，可选路径

1. **Klipper 外部控制**（最现实）
   - 树莓派跑 Klipper
   - 5 轴 G-code 翻译为 4 轴（机器）+ 外挂 A 轴步进电机
   - 工具链成熟，社区方案多

2. **HMI 改写**（很折腾）
   - 改 HMI APK 拦截 G-code，模拟 A 轴
   - 控制器不识别 A 字面，但 HMI 可以做翻译
   - 工作量大

3. **等 Snapmaker**（几乎不可能）
   - 5AXIS 不在 A350 路线图

4. **保留 4 轴 + B 轴 rotary**
   - 4.7.2 production 已经支持 B 轴
   - 大多数场景够用

---

## 待办 / 下次继续

### 短期（机器稳态相关）

- [ ] **刷回原版 V1.21.0 修 HMI**（如果不显示实时坐标）— 但目前机器还能用，可延后
- [ ] 5axis 仓库 commit 未提交的 pack.py / build.yml 改动（虽然现在看用不上）

### 中期（继续探索 5AXIS）

- [ ] **本地编译 4.7.2 + pack.py 出 .bin 烧录验证**（确认 pack.py 流程被拒的结论）
  - PlatformIO 已装：`pip install platformio`（6.1.19）
  - pio.exe: `C:\Users\李磊\AppData\Roaming\Python\Python312\Scripts\pio.exe`
  - VSCode + PlatformIO IDE 扩展也装了
  - 卡在下载 ststm32 platform + ARM GCC 工具链（网速问题）
  - **国内镜像设置**：PLATFORMIO_PYPI_URL=https://pypi.tuna.tsinghua.edu.cn/simple
  - 工具链下载没找到直接镜像，可能要等更快的网
- [ ] 试 Snapmaker2-Modules 编译出 raw module.bin
- [ ] 试 pack.py 完整三件套（-c -m -s），虽然分析过 production 不是这个格式

### 长期（如果坚持 5AXIS）

- [ ] Klipper 方案调研
- [ ] 联系 Snapmaker 看有没有 5AXIS 内部工具
- [ ] 找 SWD 测试点（如果将来改主意）

---

## 关键代码位置备忘

| 文件 | 行号 | 内容 |
|------|------|------|
| `Marlin/src/inc/Version.h` (5axis) | 41 | `SHORT_BUILD_VERSION "SM2-5.2.1-5AXIS-V9S"` |
| `Marlin/src/inc/Version.h` (4.7.2) | 41 | `SHORT_BUILD_VERSION "SM2-4.7.2"` |
| `Marlin/src/inc/Version.h` (4.4.11) | 41 | `SHORT_BUILD_VERSION "SM2-4.4.11"` |
| `Marlin/src/inc/Version.h` (5.2.1) | 41 | `SHORT_BUILD_VERSION "SM2-5.2.1"` |
| `Marlin/src/core/macros.h` (4.7.2) | 295-310 | Flash 布局 |
| `snapmaker/scripts/pack.py` | 84-175 | `pack_major_image` 函数（48 字节头）|
| `snapmaker/scripts/platformio-targets.py` | 30-37 | `pack` target（调 pack.py -d -c）|
| `Luban/src/server/lib/firmware-build.js` | 84-175 | Luban 的 `packAll`（与 pack.py 类似）|
| `snapmaker/src/service/upgrade.cpp` | 全文 | bootloader 升级逻辑 |

---

## 测试矩阵（备忘）

| 测试 | 期望 | 实际 | 结论 |
|------|------|------|------|
| 实验1：改 v9S 文件名 `Snapmaker_` → `Snapmaker2_` | 接受 | 拒（V4.7.2 backup）| 文件名不是关键 |
| TAMPER：V1.21.0 改主控 9 字节 | 接受 | 拒（V4.7.2 backup）| **主控段有强校验** |
| REPLACE：V1.21.0 主控整段换 v9S | 接受 | 拒 + 触屏被刷 + 模组循环 | 同上 |
| V1.21.0 原版 | 接受 | 接受（V4.7.2 production）| **唯一接受格式** |
| pack.py 干净产物 | 接受 | 拒（V4.7.2 backup）| **pack.py 格式 bootloader 不认** |

---

## 用户沟通要点（下次注意）

- 用户极简环境：能不装的软件不装（已装 PlatformIO / Python / Git）
- 用户喜欢看 README 原文（让我反复确认关键步骤）
- 用户对"我们自己生成的 .bin"和"production .bin"区分一开始不清楚
- 用户能自己烧 .bin，串口会看 M115 / M114
- 用户跑 TAMPER / REPLACE 时遇到过"模组过期"循环，**恢复靠刷原版 V1.21.0**
- 用户对"小 bug"会观察但不会恐慌（屏幕不显示坐标）
- 重大 bug 要立即建议"先救机器"
- 用户对话风格：直接、给指令、不啰嗦，倾向列选项
- 重要发现用户希望我**自己 push 到 GitHub 备份**

---

> 下次回来先看这页，能省 3 小时。
