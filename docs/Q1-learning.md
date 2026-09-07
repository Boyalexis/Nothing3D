# Q1：窗口中的第一次 Vulkan 绘制

本阶段只画纯色背景。它证明 Nothing3D 已能向显卡发出绘制命令，并将结果显示在 Qt 界面中。

## 谁负责什么

- `MainWindow`：Qt 菜单、左右面板、视口容器。
- `QVulkanInstance`：程序与 Vulkan 的连接入口，生命周期长于窗口。
- `QVulkanWindow`：选择设备、管理交换链、帧同步以及窗口大小变化。
- `ClearRenderer`：Nothing3D 自己编写的绘制命令，清除颜色和深度。
- `QtPresentSync`：修复 Qt 6.10.3 的呈现信号量复用，让每张交换链图像有独立信号量。

因此现在仍然使用 Qt 的底层资源管理封装；“自研渲染器”在本阶段指自行记录绘制命令，并非已经独立管理整个 Vulkan 生命周期。

## 四个新概念

1. 物理设备（Physical Device）：真实的显卡，例如 RTX 4060 或 Intel 集显。
2. 逻辑设备（Device）：程序打开的 GPU 使用接口。
3. 交换链（Swapchain）：轮流绘制和显示的一组图像，缩放窗口时需要重新调整。
4. 命令缓冲（Command Buffer）：送给 GPU 执行的一份工作清单。

`startNextFrame()` 中，先指定清屏颜色，再开始和结束渲染通道，最后调用 `frameReady()`，通知 Qt 本帧命令已准备好。现在没有三角形，所以只会显示深蓝背景。

## 为什么窗口关闭也要测试

显卡可能还在执行之前的任务。窗口、交换链、逻辑设备和 Vulkan 实例必须按正确顺序释放。本项目把窗口作用域放在实例内部，保证窗口先销毁、实例最后销毁。

## 运行与验收

- 双击 `scripts/run.bat`，查看深蓝色中央区域和状态栏显卡名。
- 调整窗口大小、最大化、最小化后恢复；画面应完整，无残影。
- 关闭窗口，程序应正常退出。
- 使用 `scripts/run.bat --integrated` 请求集成显卡；以状态栏实际显示为准。
- `scripts/run.bat --gpu-test` 自动检查真实 GPU 帧、缩放、最小化恢复、读回图像，以及包含关闭阶段的验证层错误。
- `scripts/test.bat` 仅检查无需显卡的 Qt 窗口构造，不能代替 GPU 测试。

验证层相当于 Vulkan 调用的检查员，帮助发现参数和资源生命周期错误。GPU 测试要求验证层可用；正常运行允许缺少验证层，并会在状态栏说明。

本次同步修复的关键：GPU 画完一帧，不代表显示系统已经用完这张图像。只有重新取得那张交换链图像，并等待其获取信号后，才能安全复用与它关联的呈现信号量。所以不能单靠“第几帧”决定复用哪个信号量。

## 亲手练习

打开 `src/render/VulkanViewport.cpp`，找到 `clears[0].color`。四个数依次表示红、绿、蓝、不透明度，范围通常为 0 到 1。尝试改成 `{0.15f, 0.04f, 0.04f, 1.0f}`，重新运行，看背景是否变为暗红色。

参考：[Qt Vulkan 窗口说明](https://doc.qt.io/qt-6/qvulkanwindow.html)、[LunarG SDK 安装说明](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html)。
