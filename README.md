# vita-mono-port

在 PS Vita (PCH-1000) 上运行 .NET/Mono 程序，最终目标：移植 Stardew Valley 1.6。

## 目录结构

- `vita-hello/`   流水线验证工程（已上真机跑通）
- `patches/`      对上游 mono 的补丁（eglib Vita 后端、sceKernel shim 等）
- `docs/`         决策记录与调研笔记

## 环境

- 主机：Arch 系 (Omarchy)，GCC 16，12 核
- VitaSDK 2026.08 @ `/usr/local/vitasdk`（vdpm/pacman 包管理）
- PC Mono 6.12.0（Arch 包，用于编译 BCL）
- 目标：`arm-vita-eabi`（thumbv7, hard-float, newlib）

## 里程碑

- [x] M0 VitaSDK 安装 + hello-world VPK 真机验证（流水线：编译→vpk→ux0→VitaShell 安装）
- [x] M1 libmonosgen 交叉编译成 Vita 静态库
- [ ] M2 C# hello world + 一致性电池在 Vita 上运行（模拟器 76/2 已通；**真机验收进行中**，已修 exec 内存/icache/对齐三个真机特有 bug，见 `docs/PROGRESS.md`）
- [ ] M3 MonoGame 最小渲染（vitaGL 后端）
- [ ] M4 Stardew Valley 1.6 启动

## 技术决策记录

### 为什么选 Mono 6.12 而不是 .NET NativeAOT
- VitaSDK 提供 pthread/signal/poll/dirent POSIX 兼容层，Mono 的 eglib 移植可行性较高
- .NET 9+ 虽支持 linux-arm NativeAOT，但深绑 glibc，无社区先例
- Unity 官方曾为 Vita 交付 Mono 运行时（历史证明可行），但无公开源码可复用

### 运行模式
- 首选 full-aot 或 interpreter 模式：规避 JIT 的可执行内存分配复杂度
- Vita 允许 RWX 内存（PPSSPP 动态重编译先例），JIT 作为备选优化路径

### 版本选择
- Mono 6.12.0.99（6.x 末代稳定版，支持 net_4_x profile，星露谷所需 C# 5+ 特性全覆盖）
