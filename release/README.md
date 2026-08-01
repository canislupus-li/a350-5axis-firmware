# Release Archive

> 这个目录是 V10 (5-axis) 的烧录 bin 存档。后续版本 (V11, V12, ...) 各自独立记录。

## V10 (2026-08-01) - 5-axis A350 端口修复

**烧录目标**:
- `SM2_V5.2.1-5AXIS-V10_20260801.bin_20260801.bin` (301,608 B) — Luban / HMI 烧录用

**包内字段**:
- PACKAGE: `Snapmaker_V5.2.1-5AXIS-V10`
- Filename: `SM2_V5.2.1-5AXIS-V10_20260801.b` (31 字符, 屏幕显示完整)
- SHORT_BUILD_VERSION (M115): `SM2-5.2.1-5AXIS-V10`
- 编译时间: Aug 1 2026, 11:36:54

**功能**:
- A G-code (`G0 A10`) → 驱动 A 模组 (PORT_8PIN_2)
- B G-code (`G0 B10`) → 驱动 B 模组 (PORT_8PIN_6)
- 保留 v5.2.1 所有原生功能 (3D / CNC / 激光 / HMI / CAN)

**已知限制**:
- HMI 屏幕不显示 A 轴 (V1.21.0 HMI apk 是 4-axis,不知道 J_AXIS=4)
- controller 端完全正常,HMI 是独立工程

详见根目录 `../MODIFICATIONS.md`。

## 历史测试 bin (`tests/` 子目录)

早期调试用的中间 bin (5.2.2, 5.2.3, 5.2.4, V9S 等),不参与正式 release。保留以供回溯参考。

## 烧录方式

### HMI 屏幕 (普通用户)
1. 把 `SM2_V5.2.1-5AXIS-V10_20260801.bin_20260801.bin` 拷到 U 盘
2. HMI 设置 → 升级 → 选 bin 文件
3. 等待烧完,自动重启

### SWD / J-Link (开发)
```
connect           # 选 GD32F305VE
erase
loadfile firmware.bin 0x800B800    # MARLIN slot A
reset
go
```

详见 `../MODIFICATIONS.md` §5。
