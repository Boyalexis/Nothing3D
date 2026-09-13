# 1,000 对象性能测试

## 本轮学习目标

先建立可重复的基线，再针对测量结果优化。使用 40 × 25 排列的 500 个箱子和 500 个 64 边圆柱体，共 134,000 个三角形。所有对象共用单位网格，但仍逐个提交绘制。

## 如何运行

在项目根目录运行：

```powershell
.\scripts\build-release.bat
.\scripts\test-performance.ps1
```

脚本依次启动 Intel 集显与 NVIDIA 独显的真实窗口，每个进程自动退出。透视和正交各预热 2 秒、采样 6 秒。窗口请求大小为 1600 × 1000，报告记录实际三维视口像素尺寸。不要在测量期间调整窗口、最小化程序或同时运行其他重负载工作。

```powershell
.\scripts\test-performance.ps1 -Validation
```

这次运行开启 Vulkan 验证层，专门检查 1,000 对象路径及资源退出；其帧率不用于性能达标判定。平时运行仍使用原有按需重绘，只有 `--performance-test` 才持续请求新帧。

## 报告和手动体验

`build/performance-intel-optimized.json` 与 `build/performance-nvidia-optimized.json` 保存性能结果；`*-validation.json` 保存验证运行。`*-baseline.json` 是优化前记录。

生成的 `build/performance-1000.n3d` 可以在正常程序的“打开工程”里打开。用滚轮拉远查看全部对象，选择、修改尺寸、移动、撤销和保存操作与普通工程一致。截图位于 `build/performance-intel.png` 和 `build/performance-nvidia.png`。

## 怎样理解指标

- `submitted_fps`：完成 CPU 帧记录并交给 Qt 的帧节奏，包含交换链与显示呈现的等待，不是 GPU 时间戳，也不是无同步的显卡峰值吞吐。
- `frame_p50_ms / frame_p95_ms`：帧间隔中位数及 95 分位数，后者帮助识别较慢的帧。
- `cpu_record_*`：CPU 构建一帧 Vulkan 命令的耗时，不包含 `frameReady()` 内的提交时间。
- `pick_*`：40 次射线选择的耗时，包含矩阵构造和精确三角形相交。
- `scene_setup_ms`：主窗口、场景树和视口接收千对象场景的耗时；不包含首次 GPU 初始化。
- `save_ms / load_ms`：实际工程写入和读取，随后比较编码内容，确认对象参数完整。

正常性能运行关闭验证层，其错误计数为 0 不能单独证明 Vulkan 正确；需结合独立验证运行。脚本要求集显 30 FPS、独显 60 FPS，未达标会返回失败。这里只覆盖一个固定布局与两种投影，不能推广成所有场景下的性能保证。

## 优化原理

原先每次选择都会逐个检查所有对象的三角形。现在先把射线变换到对象局部坐标，与单位包围盒相交；碰不到包围盒的对象直接跳过。

碰到包围盒后仍执行原来的精确三角形检查，所以圆柱包围盒角落不会被错误选中，也保留最近表面、旋转缩放、相机位于对象内部和近远裁剪行为。小容差只用于粗筛，最终表面判定不变。

代码入口：`src/render/ScenePicking.h`、`src/scene/PerformanceScene.h`、`src/app/PerformanceRun.h`。本轮保留现有逐对象绘制方式，因为基线已达到帧率目标，CPU 绘制记录没有成为主要耗时。
