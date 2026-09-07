# Nothing3D

Nothing3D 是一个边开发边学习的三维参数化场景编辑器。当前技术路线为 C++、Qt 6 和自研 Vulkan 渲染器。

Qt 负责桌面窗口、菜单、属性面板，以及现阶段的 Vulkan 窗口、设备和交换链管理。Nothing3D 编写几何、相机控制、着色器和绘制管线；矩阵与向量运算暂时使用 Qt 的数学类型。场景数据和拾取将在后续阶段实现。

## 当前环境

- Visual Studio Build Tools 2022 / MSVC 14.44
- Qt 6.10.3（项目本地）
- CMake 4.4.3（项目本地）
- C++20
- Vulkan SDK 1.4.357.0（项目本地，copy-only 安装）

## 构建与运行

```bat
scripts\build.bat
scripts\run.bat
scripts\test.bat
```

## 当前进度

- Q0：Qt/C++ 环境和空窗口（自动测试通过）
- Q1：Qt 中的 Vulkan 清屏（集显同步问题已修复，双显卡回归通过）
- Q2：Vulkan 三角形和着色器（已实现；学习说明见下）
- Q3：三维长方体、深度遮挡、轨道相机和自动旋转（已实现，双显卡回归通过）
- Q3 补充：XZ 无限网格（100 mm / 1 m / 10 m 层级，远处渐隐）、RGB 世界轴和右上角方向立方体。
- 演示场景新增青色圆柱体：直径 800 mm、高 1400 mm，位于长方体左侧；两者作为演示组一起自动旋转，尚不可选中或编辑尺寸。
- 下一阶段 Q4：场景和参数化对象（尚未开始）

旧 Godot 原型保存在 `archive/godot-m0`，不再作为当前实现。

## 当前操作与学习

顶部已加入首页 / 模型 / 视图三个可切换功能区。首页选择左键旋转、平移、缩放模式，视图页独立控制网格、世界轴和方向立方体。模型创建入口暂时禁用。详见 [功能区操作与源码说明](docs/Ribbon-learning.md)。

双击 `scripts/run.bat` 运行。中央显示一个彩色长方体，状态栏显示实际显卡和验证层状态。中键拖动绕模型观察，Shift + 中键平移，滚轮缩放。在视口中按 F 重置，空格切换模型自动旋转，也可以使用工具栏按钮。当前是固定尺寸演示体，尚不支持创建、选中和修改尺寸。

`scripts/run.bat --integrated` 请求集显；`scripts/run.bat --gpu-test` 执行需要真实桌面的 GPU 测试，结果写入 `build/q3-gpu-test.txt`，并保存视口和窗口截图。

本阶段先阅读 [Q3 学习说明](docs/Q3-learning.md)，测试详情见 [Q3 验证记录](docs/Q3-validation.md)。

辅助线约定为 X 红、Y 绿（向上）、Z 蓝。地面网格没有固定方形边界，按屏幕密度和距离渐隐。右上角方向立方体只跟随相机方向，带六面名称、方位环及 Home 复位按钮；面本身暂不支持点击切换视图。网格不代表吸附功能。实现和检查见 [视口辅助线说明](docs/Q3-guides.md)。

学习说明见 [Q1-learning.md](docs/Q1-learning.md)。

Q2 的顶点缓冲、GLSL 着色器、图形管线和颜色练习见 [Q2-learning.md](docs/Q2-learning.md)。着色器自动编译为 SPIR-V 并嵌入程序，修改源码后运行构建脚本即可更新。双显卡各三轮验证通过，详细数据见 [Q2-validation.md](docs/Q2-validation.md)。

验证结果：MSVC Debug 构建及无显卡烟雾测试通过；Intel UHD 与 RTX 4060 各三轮连续帧、缩放、最小化恢复和读回检查通过，包含退出阶段的验证错误均为 0。使用 `scripts/test-gpu.ps1 -Rounds 3` 可重复执行双显卡测试。

同步修复集中在 `src/render/QtPresentSync.cpp`，采用每张交换链图像独立的呈现信号量。它依赖 Qt 私有接口，构建与运行时锁定 Qt 6.10.3；升级 Qt 前必须重新审核，不能忽略版本保护。详见 [Q1 验证记录](docs/Q1-validation.md)。
