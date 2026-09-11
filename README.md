# Nothing3D

Nothing3D 是一个边开发边学习的三维参数化场景编辑器。当前技术路线为 C++、Qt 6 和自研 Vulkan 渲染器。

Qt 负责桌面窗口、菜单、属性面板，以及现阶段的 Vulkan 窗口、设备和交换链管理。Nothing3D 编写场景数据、几何、相机控制、拾取、着色器和绘制管线；矩阵与向量运算暂时使用 Qt 的数学类型。

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
- Q4 首个小目标（已实现）：独立场景对象、毫米尺寸参数、位置与旋转、稳定编号和参数校验；多个对象共用基本体显卡资源，按各自变换绘制。3/3 自动测试与双显卡各一轮回归通过，见 [Q4 验证记录](docs/Q4-validation.md)。
- 右侧可切换初始场景、只修改长方体参数、六对象和空场景。
- Q5 首个小目标（已实现）：视口三角形拾取、单选金色高亮、场景列表双向同步和所选对象参数查看。左键拖动保留导航，空白单击 / Esc 取消选择，见 [选择验证记录](docs/Q5-selection-validation.md)。
- Q5 第二个小目标（已实现）：所选长方体宽高深、圆柱体直径和高度编辑；回车或失焦应用，保持选择、视角和其他对象，拾取同步更新，见 [尺寸编辑验证记录](docs/Q5-dimensions-validation.md)。
- Q5 第三个小目标（已实现）：位置与旋转 X/Y/Z 编辑，支持负值、多圈角度和 Ctrl 步进（位置 100 mm / 旋转 15°），见 [变换编辑验证记录](docs/Q5-transforms-validation.md)。
- Q5 第四个小目标（已实现）：输入尺寸创建长方体、圆柱体，金色线框预览，Y=0 地面单击放置，Ctrl 100 mm 网格吸附，确认后自动选中，Esc 取消。7/7 自动测试与双显卡回归通过，见 [创建放置验证记录](docs/Q5-creation-validation.md)。
- Q5 第五个小目标（已实现）：底心三轴移动手柄，世界 X/Y/Z 单轴拖动，Ctrl 100 mm 吸附、Esc / 右键恢复起点，位置与属性实时同步。见 [移动手柄说明](docs/Q5-move-learning.md) 和 [验证记录](docs/Q5-move-validation.md)。
- Q6 第一个小目标（已实现）：创建、属性编辑、整次三轴拖动支持撤销/重做；首页按钮与 Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z，恢复编号、参数、选择和列表，切换场景清空历史。见 [编辑历史说明](docs/Q6-history-learning.md)。
- Q6 第二个小目标（已实现）：模型页原位复制、删除所选对象；视口 / 场景列表支持 Ctrl+D / Delete，复制独立编号和名称，删除可按原编号、原列表位置撤销恢复。见 [复制与删除说明](docs/Q6-object-actions-learning.md)。
- Q6 第三个小目标（已实现）：`.n3d` 工程打开、保存、另存为和未保存提示；文件完整校验、原子提交、编号分配器恢复以及历史保存点判断。见 [工程文件说明](docs/Q6-files-learning.md)。

旧 Godot 原型保存在 `archive/godot-m0`，不再作为当前实现。

## 当前操作与学习

编程初学者请先读 [从零认识 Nothing3D：Qt 窗口与 Vulkan 圆柱体绘制](docs/Beginner-learning.md)。说明从基本术语讲起，逐步解释 Qt 与 Vulkan 的连接，以及一个圆柱体从尺寸参数、三角形到屏幕像素的全过程。

中央三维视图的完整实现说明见 [Qt 视图、无限网格、XYZ 坐标轴与相机](docs/Viewport-learning.md)，包含绘制流程、矩阵关系、网格射线求交和源码阅读顺序。旧有限网格生成函数及对应测试已移除，当前使用着色器无限网格。

顶部已加入首页 / 模型 / 视图三个可切换功能区。首页选择左键旋转、平移、缩放模式，模型页输入基本体尺寸后在地面单击创建，视图页独立控制网格、世界轴和方向立方体。详见 [功能区操作与源码说明](docs/Ribbon-learning.md) 与 [创建放置说明](docs/Q5-creation-learning.md)。Q4 场景基础见 [Q4 学习说明](docs/Q4-learning.md)。

双击 `scripts/run.bat` 运行。默认显示一个彩色长方体和一个青色圆柱体，状态栏显示实际显卡和验证层状态。左键单击模型或左侧名称进行选择，右侧输入尺寸、位置或旋转后按回车或移开焦点应用；空白单击或 Esc 取消选择。变换输入框支持 Ctrl 步进：位置 100 mm、旋转 15°。模型页支持创建基本体，放置时显示线框预览，Ctrl 对齐到 100 mm 地面网格，Esc 取消放置。左键拖动按首页模式导航，中键拖动绕模型观察，Shift + 中键平移，滚轮缩放。在视口中按 F 重置，空格切换场景预览旋转；放置期间暂时禁用预览旋转。创建和参数修改支持撤销/重做；切换右侧示例会重新载入预设并清空历史，目前尚不支持保存。

`scripts/run.bat --integrated` 请求集显；`scripts/run.bat --gpu-test` 执行需要真实桌面的 GPU 测试，结果写入 `build/q3-gpu-test.txt`，并保存视口和窗口截图。

选中对象后，拖动底心的红 / 绿 / 蓝箭头，分别修改世界 X / Y / Z 坐标，右侧实时更新。拖动时 Ctrl 将该轴吸附到 100 mm 坐标；Esc 或右键恢复起点，松开左键保留结果。“视图”页可隐藏移动手柄。拖动期间冻结相机并停止自动预览旋转，保留当前预览角度；窗口失焦或大小变化会取消本次拖动。

首页“撤销 / 重做”对应 Ctrl+Z / Ctrl+Y（或 Ctrl+Shift+Z）。一次完整拖动只记录一步，Esc 取消不留记录；撤销后提交新修改会丢弃旧重做分支。创建和拖动期间先确认或取消，再使用编辑历史。

模型页支持复制和删除所选对象。视口或场景列表中 Ctrl+D 原位复制，Delete 删除；参数输入框中 Delete 仍删除文字。副本自动选中，最初与原件重叠，可拖动箭头移开。两项操作均支持撤销/重做。

“工程 Project”页支持 Ctrl+O 打开、Ctrl+S 保存、Ctrl+Shift+S 另存为。标题中的 * 表示未保存；打开、切换示例或关闭前可选择保存、不保存或取消。示例文件见 [getting-started.n3d](examples/getting-started.n3d)。

本阶段先阅读 [Q6 工程文件说明](docs/Q6-files-learning.md)，测试详情见 [工程文件验证](docs/Q6-files-validation.md)。对象操作见 [复制与删除说明](docs/Q6-object-actions-learning.md)，历史原理见 [编辑历史说明](docs/Q6-history-learning.md)，移动操作见 [Q5 移动手柄说明](docs/Q5-move-learning.md)，创建入口见 [创建放置说明](docs/Q5-creation-learning.md)，参数编辑见 [位置与旋转说明](docs/Q5-transforms-learning.md) 和 [尺寸编辑说明](docs/Q5-dimensions-learning.md)。

辅助线约定为 X 红、Y 绿（向上）、Z 蓝。地面网格没有固定方形边界，按屏幕密度和距离渐隐。右上角方向立方体只跟随相机方向，带六面名称、方位环及 Home 复位按钮；面本身暂不支持点击切换视图。网格不代表吸附功能。实现和检查见 [视口辅助线说明](docs/Q3-guides.md)。

学习说明见 [Q1-learning.md](docs/Q1-learning.md)。

Q2 的顶点缓冲、GLSL 着色器、图形管线和颜色练习见 [Q2-learning.md](docs/Q2-learning.md)。着色器自动编译为 SPIR-V 并嵌入程序，修改源码后运行构建脚本即可更新。双显卡各三轮验证通过，详细数据见 [Q2-validation.md](docs/Q2-validation.md)。

验证结果：MSVC Debug 构建及无显卡烟雾测试通过；Intel UHD 与 RTX 4060 各三轮连续帧、缩放、最小化恢复和读回检查通过，包含退出阶段的验证错误均为 0。使用 `scripts/test-gpu.ps1 -Rounds 3` 可重复执行双显卡测试。

同步修复集中在 `src/render/QtPresentSync.cpp`，采用每张交换链图像独立的呈现信号量。它依赖 Qt 私有接口，构建与运行时锁定 Qt 6.10.3；升级 Qt 前必须重新审核，不能忽略版本保护。详见 [Q1 验证记录](docs/Q1-validation.md)。
