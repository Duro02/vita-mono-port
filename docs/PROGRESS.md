# 进度（2026-09-14 更新，Vita3K 模拟器上验证）

最终目标：Stardew Valley 1.6 在 hacked PS Vita (PCH-1000, Enso) 上运行。
当前阶段：M2 收尾（Mono 运行时 + C# 一致性电池），M3 未开始。

## 里程碑

- [x] M0：VitaSDK 工具链 + hello-world VPK，真机（PCH-1000）跑通
- [x] M1：`libmonosgen-2.0.a` 等静态库交叉编译通过（Mono 6.12.0.99，interpreter-only）
- [x] M2-sim：C# hello 在 Vita3K 跑通（`Main returned`，返回值 42，`out.txt` 落盘）
- [x] M2-battery：C# 一致性电池 **76 passed / 2 failed**（见下）
- [ ] M2-hw：同一 VPK 在真机验收一次（USB 推包 → VitaShell 安装 → 运行）
- [ ] M3：MonoGame 最小渲染（vitaGL 后端）
- [ ] M4：Stardew Valley 1.6 启动

## 工作目录

```
~/Projects/vita-port/
  vita-mono-port/   本仓库（构建脚本、补丁、shim、launcher、电池、探针）
  mono/             上游 Mono 6.12.0.99 checkout + 本地修改（非 git， curated diff 见 patches/）
  vita3k/           Vita3K AppImage + 3.74 固件 PUP
```

构建：`./build-mono-vita.sh [configure|make|all]`（mono 树）→ `./build-launcher.sh`（出 `vita-launcher/build/MONO00002.vpk`）。
测试：解包 VPK 到 `~/.local/share/Vita3K/Vita3K/ux0/app/MONO00002/`，
`ux0/data/monoapp/run.txt` 第一行指定要跑的 exe（缺省 hello），
`Vita3K -l 0 -r MONO00002` 启动。只在模拟器跑，攒到里程碑才请真机验收。

## 电池 verdict（v3bat19，Main 正常返回）

T01 异常/ T02 线程锁+Interlocked/ T03 GC（minor+major+终结器）/
T04 泛型/ T05 委托闭包事件/ T06 反射/ T07 值类型 decimal Guid/
T08 文件 IO 全套/ T09 数组/ T10 数学（除 round-bank）/ T11 编码/
T12 集合/ T13 字符串/ T14 日期/ T15 装箱/ T16 线程池 —— 全部 PASS，
除以下两项（均为 P3，已定性，见“已知问题”）：

- `FAIL thread-lock-interlocked`：偶发（约首轮 10~25% 概率丢 1~3/2000 个计数），重读不恢复，真丢非 stale read；只发生在进程第一次高竞争，后续永远稳定
- `FAIL round-bank`：仅 `Math.Round(2.5字面量直传)` 一种形状错（返 3 应返 2），中转局部变量即对；icall/fmod/floor/汇编/IL 字节全部验过没问题，疑 Dynarmic 特异，真机复验即定

## 修过的 bug（症状 → 根因 → 修法 → 位置）

1. `thumb_supported` assert → hwcap 在 Vita 上为空 → 硬编码 Cortex-A9 特性（`mono_hwcap_arch_init`）
2. `g_assert(staddr)` → 新线程栈边界未知 → `sceKernelGetThreadInfo` 取栈界
3. `MONO_PATH` 冒号切分 `ux0:` → 改 `mono_set_dirs("ux0:data/monoapp")`
4. BCL Unix IO 全挂 → 手写 28 个 `System.Native` 入口 + `mono_dl_fallback_register`（`vita-shim/vita-system-native.c`）
5. fd 1/2 黑洞 → stdio 无缓冲 + `sceIoWrite` 追踪
6. `getrusage` 返回 EINVAL → 线程池 hill-climbing 经 `mono_cpu_usage` → `g_error` 炸进程 → shim 返回全零（`--wrap=getrusage`）
7. `pthread_kill` 不存在 → 完整 GC 时混合挂起 abort → 缺席分支假装送达（`patches/013`，配合下条）
8. Vita 无异步信号，挂起靠协作式安全点 → sem/cond 包 10ms 量子 + 自挂起表（后证实 parked 线程多为 BLOCKING，本机制极少触发，留作保险）
9. cond 定时等待秒回 → Vita `pte_relmillisecs` 用 `ftime`（纪元时）算相对超时，Mono 给的是 MONOTONIC → 恒负钳 0ms → busy-spin + 丢唤醒（Join/lock 挂死）→ 包里把剩余时间换算成 realtime deadline 再调原语（反汇编实锤）
10. major GC 静默挂 → 默认 `SERIAL mark + CONCURRENT sweep`，sweep job 扔给 SGen 线程池但 worker 从不起 → `MONO_GC_PARAMS=no-concurrent-sweep`
11. major 后 verify 报堆坏 → `munmap` 超额释放（要 256KB 把整块 528KB 还了，活堆被踩）→ 子区间进空闲链表复用（地址原地保留）+ 整块释放 purge 重叠项
12. `Sharing violation` → Vita 无文件锁，`fcntl(F_SETLK)` 回 EINVAL → Mono 当锁冲突 → 假装加锁成功（单应用无竞争者）
13. 路径双 `app0:/` 前缀（managed 把 `ux0:` 当相对路径反复拼接）→ libc 层统一剥前缀（open/stat/mkdir/rmdir/unlink/rename/opendir）
14. `st_ino` 常量致 share 误判（所有文件像同一个）→ stat 按规范化路径哈希合成 ino，fstat 走 fd→ino 表
15. `GetFiles` 无限循环 → CoreFX 约定到尾返 **-1**（我们返 0，managed 当成功死循环）→ `SystemNative_ReadDirR` 到尾返 -1 + 结构体按官方补齐
16. newlib `readdir` 不终止 → `opendir/readdir/closedir/rewinddir` 直调 `sceIoDopen/Dread/Dclose`

## 已知问题（不阻塞 M2，真机/M3 前重估）

- P3 `round-bank`（上）、P3 首轮 thread 偶发丢数（上）
- ThreadPool worker 存活时 `GC.Collect` 可能挂 STW（worker 被点名但永不自停；pool 测试已移到电池最后，绕行）
- `munmap-nomatch` 子区间泄漏（只增不减，256MB 水位内安全）
- SGen `verify-before-collections` 默认关（`main.c` 里 `#if 0`，排查时开）
- 模拟器宿主偶发崩溃（Trace/breakpoint、SIGSEGV，多与退出清理相关；以 guest 文件日志为准）
- 真机 VPK 还是 9 月 9 日的旧包，M2-hw 待推新包验收

## 探针索引（`probe/`，纯本地，不上真机）

p0 异常 / p1 基础线程 / p2 线程分步 / p4 高竞争锁 / p7/p8/p9 GC 二分 /
p10 T02 复刻 / p11 TryEnter 计时 / p12 无锁 Join（钉死 Join bug）/
p13 目录枚举 / p14-p19 fmod/Round 排查 / p22 Round 位+线程十连 / p23 中转+预热 / p24 60 轮 / p25 重读验证

## 下一步

1. 真机 M2-hw 验收（USB 推最新 `MONO00002.vpk` + dlls，跑电池 + p23）
2. M3 调研：MonoGame→vitaGL 后端（图形/音频/输入/存储路径策略：相对路径 + 数据目录，cwd 方案）
