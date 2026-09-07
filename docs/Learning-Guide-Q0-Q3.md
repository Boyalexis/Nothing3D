# Nothing3D：从零到双基本体三维视口

截至 2026-09-06，基于当前项目源码整理。面向 Python 入门、尚未系统学习 C++ / Qt / Vulkan / 三维数学的读者。

这不是把所有源文件逐行翻译一遍，而是一条可以沿着代码亲自走通的学习路线。正文中的短代码块取自当前实现或明确标为示意；文件链接可直接打开源码。

## 1. 我们到底完成了什么

一句话：已经做出一个用 Qt 承载、自行编写 Vulkan 绘制逻辑的三维演示视口，尚未完成可以创建和编辑对象的三维编辑器。

最初话题是公司研发类似 E3D 的国产三维工厂设计软件；随后明确 Nothing3D 是你自己的边做边学项目，不作为公司的商业交付，也不承担完整替代 E3D 的目标。Python 二次开发是后续方向，不是当前功能。

技术路线经历了 Godot 原型探索，随后根据你的选择切换为 C++20 + Qt 6 Widgets + 自研 Vulkan 渲染逻辑。Godot 原型保留在 [历史归档](E:/djh/Nothing3D/archive/godot-m0)，不参与当前程序构建。后面提出“类似 Godot 的无限网格”，指的是视觉和交互效果，并没有重新使用 Godot。

### 当前能力与边界
| 已实现 | 不应误认为已经实现 |
| --- | --- |
| Qt 三栏界面、工具栏、状态栏 | 完整可用的业务界面；新建/打开/保存仍没有对应业务逻辑 |
| 长方体、圆柱体演示 | 参数化对象创建、独立对象编辑、工程模型内核 |
| 相机绕转、平移、缩放 | 对象移动手柄、拾取、选择高亮 |
| 演示组自动旋转 | 每个对象拥有独立的变换组件 |
| 无限网格、世界轴、方向立方体 | 网格吸附、点击立方体面切换视图 |
| Home / F 复位 | 工程撤销/重做 |
| 构建、基础测试和真实 GPU 检查 | 工业级稳定性、千对象性能达标或跨平台认证 |

当前单位约定：内部几何使用米，界面说明使用毫米；X 红、Y 绿且向上、Z 蓝，地面是 XZ 平面。暂未建立完整的工程单位系统。

长方体尺寸为 X=2000、Y=1500、Z=1000 mm，底面中心在原点；圆柱体直径 800、高 1400 mm，初始底面中心为 (-1600, 0, 0) mm。

## 2. 开发历程：每一步解决了什么

| 阶段 | 当时解决的问题 | 应掌握的知识 | 当前阅读入口 |
| --- | --- | --- | --- |
| 路线确认 | 用现成引擎，还是学习底层组成 | 工具选择与学习目标之间的取舍 | [路线规格](E:/djh/Nothing3D/Nothing3D-Spec.md) |
| Q0 | C++ 程序如何变成桌面窗口 | 编译、链接、CMake、Qt 布局与事件循环 | [MainWindow.cpp](E:/djh/Nothing3D/src/ui/MainWindow.cpp:43) |
| Q1 | Qt 中能否稳定显示 Vulkan 画面 | 设备、交换链、命令缓冲、资源生命周期 | [main.cpp](E:/djh/Nothing3D/src/app/main.cpp:17) |
| 同步修复 | 为什么集显出现呈现同步问题 | GPU 绘制结束与呈现完成不是同一件事 | [QtPresentSync.cpp](E:/djh/Nothing3D/src/render/QtPresentSync.cpp:16) |
| Q2 | 显卡如何画第一个三角形 | 顶点缓冲、着色器、SPIR-V、图形管线 | [Triangle.cpp，历史实现](E:/djh/Nothing3D/src/render/Triangle.cpp:22) |
| Q3 | 平面三角形如何变成可观察的三维物体 | 几何网格、矩阵、透视和深度 | [BoxGeometry.h](E:/djh/Nothing3D/src/render/BoxGeometry.h:8) |
| 视口辅助 | 如何判断大小、位置和方向 | 世界空间与屏幕空间的区别 | [GuideGeometry.h](E:/djh/Nothing3D/src/render/GuideGeometry.h:58) |
| 无限网格 | 如何去掉固定网格边界 | 逆投影、射线与平面求交、渐隐 | [grid.frag](E:/djh/Nothing3D/shaders/grid.frag) |
| 方向立方体 | 如何在角落显示独立的方向提示 | 小视口、正交投影、局部深度隔离 | [ViewCubeGeometry.h](E:/djh/Nothing3D/src/render/ViewCubeGeometry.h:43) |
| 竖直轴修复 | 如何去掉绿色轴的固定长度限制 | 齐次裁剪、保留正确深度 | [InfiniteAxis.h](E:/djh/Nothing3D/src/render/InfiniteAxis.h:10) |
| 圆柱演示 | 如何从公式生成另一个基本体 | 圆周分段、侧壁与端盖三角化 | [CylinderGeometry.h](E:/djh/Nothing3D/src/render/CylinderGeometry.h:7) |

路线规格中的 `.n3d` 文件、1000 对象、Python API 等属于规划，不是当前完成清单。旧阶段文档描述的是当时画面，当前运行时应以本指南和实际代码为准。

## 3. “自研 Vulkan 渲染器”在这里是什么意思

不是不用任何外部库，也不是我们已经从零实现所有底层组件。

| 部分 | 当前由谁负责 |
| --- | --- |
| 窗口、按钮、布局、信号与槽 | Qt Widgets |
| Vulkan 窗口集成、设备、交换链、命令缓冲及主要提交/呈现流程 | Qt 的 QVulkanWindow |
| 向量、矩阵基础计算 | Qt 的 QVector3D、QMatrix4x4 |
| 基本体顶点生成、相机行为、网格与方向控件 | Nothing3D 代码 |
| 顶点缓冲、着色器、管线及具体绘制命令 | Nothing3D 代码 |
| 特定版本的呈现同步适配 | Nothing3D 的 QtPresentSync |
| 最终执行图形命令 | Vulkan 驱动与显卡 |

因此“用 Qt 窗口”和“自己编写 Vulkan 渲染器”可以同时成立。Qt 提供窗口与部分基础设施，当前项目没有使用 Qt 3D 或 Godot 的场景渲染器。

## 4. 从程序启动开始读源码

### 4.1 CMake 不是渲染器，而是构建说明书

源码：[CMakeLists.txt](E:/djh/Nothing3D/CMakeLists.txt)。

编译把 `.cpp` 变成机器代码，链接把这些代码和所需库组合成可执行程序。CMake 描述参与构建的文件、依赖库与编译规则；MSVC 实际编译 C++。

重点读三处：

```cmake
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Widgets GuiPrivate)
target_link_libraries(Nothing3D PRIVATE Qt6::Widgets Qt6::GuiPrivate Vulkan::Headers)
```

第一行选择语言标准，第二行要求特定版本的 Qt，第三行连接程序需要的组件。Qt 版本锁定不是随意限制，原因见后面的同步修复。

构建文件还会把 `box`、`grid` 两组 GLSL 着色器编译成 SPIR-V，再作为 Qt 资源嵌入程序。当前 Vulkan API 和着色器目标是 1.0；本地 SDK 的版本号并不等于程序必须使用的 API 版本。

`Triangle.cpp` 和 `Box.cpp` 虽然还在目录里，但没有列入当前可执行程序的源文件清单。现在实际使用的是 `ColorMesh.cpp`。头文件中的内联几何函数通过 `#include` 被当前源文件使用。

### 4.2 main.cpp：把应用、Vulkan 和窗口连接起来

源码：[main.cpp](E:/djh/Nothing3D/src/app/main.cpp:17)。

正常启动流程如下；GPU 自动测试的定时器分支第一遍可以跳过：

```text
QApplication
  → 创建 QVulkanInstance，按可用情况启用验证层
  → 创建 MainWindow
  → 创建并嵌入 VulkanViewport
  → window.show()
  → application.exec() 处理事件
  → 窗口先销毁，Vulkan 实例后销毁
```

`application.exec()` 不是持续调用你写的某个大循环，而是进入 Qt 事件循环：等待并处理鼠标、键盘、窗口变化、定时器等事件。

为什么窗口被放在内层大括号里？C++ 的局部对象离开作用域时会析构；这种排列保证 Vulkan 窗口及其资源先结束，再销毁外层 Vulkan 实例。

### 4.3 MainWindow：界面不是业务数据

源码：[buildWorkspace()](E:/djh/Nothing3D/src/ui/MainWindow.cpp:101)、[buildToolbar()](E:/djh/Nothing3D/src/ui/MainWindow.cpp:77)。

`QSplitter` 管理左右可调整的三栏布局。`VulkanViewport` 是一个窗口对象，通过 `QWidget::createWindowContainer()` 放入 Widgets 界面。

工具栏的连接例子：

```cpp
connect(reset, &QAction::triggered, viewport_, &VulkanViewport::resetView);
```

含义是：用户点击重置按钮，Qt 发出 `triggered` 信号，然后调用视口的 `resetView()`。这就是“信号与槽”，可以先把它理解成事件与处理函数的连接。

左侧两条树节点现在只是禁用的演示文字，右侧是说明标签。它们没有连接到真正的场景对象数据，所以显示两条树节点不等于实现了场景管理。

## 5. 几何：把形状变成一组数字

### 5.1 一个顶点保存什么

源码：[BoxGeometry.h](E:/djh/Nothing3D/src/render/BoxGeometry.h)。

```cpp
struct BoxVertex { float position[3]; float color[3]; };
```

可以把 `struct` 理解成一个简单的数据记录：三个位置分量 X/Y/Z，三个颜色分量 R/G/B。`float` 是单精度浮点数。当前没有法线、纹理坐标或对象 ID。

`BoxVertex` 是历史命名，现在长方体、圆柱体和辅助几何都会使用它，并非只允许保存长方体。

### 5.2 长方体：八个角，不等于只上传八个顶点

长方体有六个面，每个面拆成两个三角形，因此有 12 个三角形。当前不用索引缓冲，每个三角形直接保存三个顶点，共 36 条顶点记录。

`p` 数组定义八个角；`faces` 指定各面的角；`corners = {0,1,2,0,2,3}` 把一个四边形拆成两个三角形。不同面使用不同颜色。

重复顶点是当前为了直观而采用的方案。后续可以学习索引缓冲，但“八个角”也不总意味着渲染数据只需要八个顶点：同一空间位置可能需要不同面法线、颜色或纹理坐标。

### 5.3 圆柱体：从圆公式到三角形

源码：[cylinderVertices()](E:/djh/Nothing3D/src/render/CylinderGeometry.h:7)。

圆周位置使用：

```cpp
x = radius * std::cos(angle);
z = radius * std::sin(angle);
```

这是公式示意，Y 分别取底部的 0 和顶部的 `height`。由于 Y 向上，圆周位于 XZ 平面。

圆周分 64 段，每段包含侧壁的两个三角形、顶部和底部各一个三角形，总计 `64 × 4 = 256` 个三角形、768 条顶点记录。端盖不是空的，侧面也不是一张真正的数学曲面，而是很多小平面近似。

代码中的 `shade` 按圆周角度预先改变侧面颜色，让轮廓更容易看清。它不是实时光照：我们尚未给顶点增加法线，也没有光源或材质系统。

### 5.4 两个物体现在怎样组合

源码：[demoVertices()](E:/djh/Nothing3D/src/render/CylinderGeometry.h:30)。

```cpp
auto cylinder = cylinderVertices();
for (auto& vertex : cylinder) vertex.position[0] -= 1.6f;
mesh.insert(mesh.end(), cylinder.begin(), cylinder.end());
```

`auto&` 表示直接引用容器里的元素，因此这行确实修改了每个圆柱顶点的 X 坐标。将圆柱移动到左侧后，把它的顶点接到长方体后面。

当前两者合计 804 条顶点记录，共用一个演示缓冲和同一模型矩阵。因此自动旋转时，圆柱会随整个演示组绕原点转动，不是独立绕自己的底心自转。这正是下一阶段需要建立“对象与独立变换”的原因。

## 6. 从 CPU 顶点到 GPU 画面

### 6.1 上传：ColorMesh::initialize()

源码：[ColorMesh.cpp](E:/djh/Nothing3D/src/render/ColorMesh.cpp:19)。

先按以下顺序理解，不必立即记住全部 Vulkan 参数：

```text
创建顶点缓冲 → 查询内存要求 → 选择内存类型
→ 分配内存 → 绑定缓冲与内存 → 映射到 CPU
→ memcpy 复制顶点 → 解除映射
```

“创建缓冲”与“分配内存”是两件事：前者建立资源及其用途，后者提供实际存储空间，再将两者绑定。

当前选择 CPU 可访问且 coherent 的内存，简化上传。coherent 有助于处理 CPU 写入的可见性，但不意味着可以不顾 GPU 是否仍在使用就随意修改数据。当前顶点在初始化时上传，不在每帧并发改写。

`std::span<const BoxVertex>` 是一段只读连续数据的视图，不拥有数据；函数在返回前已将内容复制到 Vulkan 内存，因此几何生成函数返回的临时容器不会被 GPU 长期引用。

### 6.2 管线：提前约定怎么解释和绘制

源码：[createPipeline()](E:/djh/Nothing3D/src/render/ColorMesh.cpp:72)。

图形管线组合了顶点格式、着色器、三角形或线段的组织方式、光栅化、深度测试和颜色混合等规则。

当前普通模型读取位置和颜色；网格只读取屏幕三角形的位置，颜色由片元着色器计算。模型开启深度测试和写入；网格开启深度测试、不写入深度，并启用透明混合。

`VK_COMPARE_OP_LESS` 表示更近的片元才能通过。当前没有依靠背面剔除来代替深度测试。

### 6.3 着色器：在显卡上运行的小程序

源码：[box.vert](E:/djh/Nothing3D/shaders/box.vert)、[box.frag](E:/djh/Nothing3D/shaders/box.frag)。

```glsl
gl_Position = transform.mvp * vec4(position, 1.0);
vertexColor = color;
```

顶点着色器将三维位置变换到裁剪空间，并传递颜色。GPU 随后把三角形转成片元并插值颜色，片元着色器输出最终颜色。

`vec4(position, 1.0)` 为三维点补上齐次坐标 w=1，使平移能统一写进矩阵乘法。先知道“位置用 w=1，方向常用 w=0”即可，后面再理解推导。

### 6.4 绘制命令不是立即画完

源码：[draw()](E:/djh/Nothing3D/src/render/ColorMesh.cpp:142)。

代码设置视口和裁剪矩形，绑定管线、顶点缓冲，推送矩阵，再记录 `vkCmdDraw()`。这些调用是在命令缓冲里记录任务，不等于 CPU 调用返回时 GPU 已经完成绘制。

普通模型每次推送一个 4×4 矩阵，16 个 float，共 64 字节；网格需要正、逆两个矩阵，共 128 字节。它们通过 push constants 传入，不必为了转动相机而重新上传整组顶点。

## 7. 三维数学与相机：先理解意义，再理解公式

源码：[OrbitCamera.h](E:/djh/Nothing3D/src/render/OrbitCamera.h)、[modelViewProjection()](E:/djh/Nothing3D/src/render/VulkanViewport.cpp:126)。

### 7.1 三个矩阵分别改变什么

| 矩阵 | 直观问题 | 当前作用 |
| --- | --- | --- |
| 模型 M | 物体放在哪里、朝向哪里 | 整个演示组绕 Y 轴旋转 |
| 观察 V | 从哪里看、朝哪里看 | 相机绕转、平移、拉近拉远 |
| 投影 P | 三维怎样投到画面 | 透视、宽高比、近远裁剪 |

实际组合：

```cpp
return clipCorrectionMatrix() * camera.projection(aspect)
    * camera.view() * model;
```

上面把宽高比计算简写为 `aspect`。对当前列向量写法，最右边的变换先作用：先模型、再观察、再投影，最后做 Qt 到 Vulkan 裁剪约定的修正。顺序不能随意交换。

### 7.2 相机不是一个必须画出来的实体

它目前主要是四项状态：水平角 `yaw`、俯仰角 `pitch`、距离 `distance`、观察中心 `target`。

```cpp
result.lookAt(target + direction() * distance, target, {0,1,0});
```

第一项算相机位置，第二项是看向的点，第三项规定上方。

当前默认水平角 35°、俯仰角 25°、距离 5.5 m；透视垂直视场角 45°，近裁剪 0.1 m，远裁剪 100 m。俯仰限制在 ±85°，避免看向正上/正下时上方向计算退化；缩放距离限制在 2.5–30 m。

滚轮操作改变的是相机距离，不是模型尺寸。平移则沿相机的右、上方向改变观察中心，并按距离与窗口高度换算鼠标移动量。

### 7.3 鼠标是怎样带动画面的

源码：[mouseMoveEvent()](E:/djh/Nothing3D/src/render/VulkanViewport.cpp:164)。

```text
鼠标移动 → 算鼠标位移 → 修改相机状态
→ requestUpdate() 请求重绘 → 计算新矩阵 → GPU 使用新矩阵画同一组顶点
```

中键改变观察方向，Shift + 中键改变观察中心。空格启动/停止的是模型角度定时更新，不是中键相机绕转。

动画按实际经过时间计算角度：`milliseconds * 0.03f`，相当于每秒约 30°。定时器间隔 16 ms 是调度请求，并不能保证设备一定达到固定帧率。

## 8. 无限网格、世界轴与方向立方体

### 8.1 无限网格不是创建无限数量的线

源码：[grid.vert](E:/djh/Nothing3D/shaders/grid.vert)、[grid.frag](E:/djh/Nothing3D/shaders/grid.frag)。

先画一个覆盖屏幕的三角形。顶点着色器用逆观察投影矩阵得到对应近、远平面的世界坐标，片元着色器沿这条射线求地面交点。

核心代码：

```glsl
float dy = farPoint.y - nearPoint.y;
float t = -nearPoint.y / dy;
vec3 p = mix(nearPoint, farPoint, t);
```

因为地面 Y=0，沿射线的 Y 分量满足 `nearY + t × (farY-nearY) = 0`，解出来就是这段代码。实际实现还会排除平行地面和无效交点，不能只复制上面三行而忽略检查。

接下来，根据 `p.xz` 到各格线的距离决定颜色。0.1、1、10 m 三种间距形成不同层级，`fwidth` 估计屏幕像素对应的变化尺度，用于平滑线边缘和隐藏过密细线。距离约 35–95 m 时再渐隐。

网格重新计算交点深度并赋给 `gl_FragDepth`，因此不会无条件画在模型前面。“无限”表示没有固定方形边界，不表示无限可见距离或无限数值精度。

### 8.2 为什么绿色轴没有简单改成一个很大的数字

源码：[infiniteYAxis()](E:/djh/Nothing3D/src/render/InfiniteAxis.h:10)。

旧方案只有从原点到 Y=3 m 的线，所以长度固定。新方案把整条世界直线 `(0,t,0)` 与当前可见体积求交，只画可见部分。

Vulkan 齐次裁剪条件为 `-w≤x≤w`、`-w≤y≤w`、`0≤z≤w`。把直线代进去，就得到关于 t 的六个不等式；逐个收紧 t 的上下限，得到有效端点。

返回的矩阵将一条单位线变成裁剪后的屏幕线段，同时保留深度。看不到世界原点轴时，它不会强行变成屏幕中央的一根装饰线；与近远裁剪面相交时，也不保证总能延伸到二维画面的上下边缘。

### 8.3 右上角是另一套小型绘制视图

源码：[ViewCubeGeometry.h](E:/djh/Nothing3D/src/render/ViewCubeGeometry.h:43)、[compassMatrix()](E:/djh/Nothing3D/src/render/GuideGeometry.h:63)。

方向立方体只使用相机方向，不使用它的平移和距离，因此始终保持稳定大小。它采用正交投影，不受主视图透视缩放影响。

绘制前只清除右上角区域的深度，让控件覆盖主场景；随后控件自己的面和文字仍参与深度测试，避免背面的字透出来。Home 图标最后以不受深度遮挡的方式绘制。

文字在初始化时通过 Qt 字体画入小图，再转成彩色微小四边形上传显卡。最终仍由 Vulkan 绘制，截图也能包含它；这是学习用实现，并不是成熟高效的文字图集系统。

Home 按钮命中检测会用屏幕缩放比例，把鼠标的逻辑坐标转换到实际像素坐标。它调用与 F 相同的 `resetView()`；立方体各面目前没有点击切视图的逻辑。

## 9. 每帧绘制与资源生命周期

源码：[VulkanViewport.cpp](E:/djh/Nothing3D/src/render/VulkanViewport.cpp:14)。类还叫 `BoxRenderer`，但当前它已经负责整个演示视口，不只是长方体。

### 当前一帧的顺序

```text
选择本张交换链图像的呈现信号量
  → 开始渲染通道并清除背景、深度
  → 画长方体与圆柱体演示组
  → 画无限网格及竖直世界轴
  → 清除角落区域深度
  → 画方向立方体、方位文字、Home
  → 结束渲染通道
  → frameReady() 交回 Qt，后续提交并呈现
```

绘制顺序与深度测试一起决定结果，不是简单“最后画的就一定挡住前面的”。例如网格虽然后画，仍必须通过模型已经写入的深度测试。

### 三种时机不要混在一起

| 时机 | 回调/函数 | 做什么 |
| --- | --- | --- |
| 设备资源初始化 | `initResources()` | 生成几何，创建并上传缓冲 |
| 交换链资源初始化 | `initSwapChainResources()` | 建同步适配和图形管线 |
| 每次重绘 | `startNextFrame()` | 记录当前帧绘制命令 |
| 交换链释放 | `releaseSwapChainResources()` | 释放管线、恢复并释放同步适配 |
| 设备资源释放 | `releaseResources()` | 销毁缓冲并释放内存 |

窗口大小变化通常需要重建交换链相关资源，而不是每帧重建全部顶点。最小化等事件是否触发更广的释放由 Qt 生命周期决定，所以恢复路径也必须能重新初始化资源。

## 10. 集显同步问题给我们的教训

源码：[QtPresentSync.cpp](E:/djh/Nothing3D/src/render/QtPresentSync.cpp:16)。这是进阶章节，第一遍只需理解问题，不要把它当作常规练习修改。

需要区分三个时刻：CPU 记录好了命令、GPU 完成了绘制、呈现系统不再使用相关同步对象。它们不是同一件事。

当前适配为每张交换链图像创建独立的呈现信号量，并按 `currentSwapChainImageIndex()` 选择，而不是单靠 CPU 帧槽来轮换。

释放时先恢复 Qt 原有句柄，再释放我们创建的信号量，避免 Qt 和我们重复释放同一个对象。这个过程依赖已经检查过的 Qt 6.10.3 内部实现，所以同时存在编译版本断言、运行时版本检查和 CMake 精确版本依赖。

不要通过关闭验证层或随意加入每帧等待，把错误表面压下去。升级 Qt 时也不能直接删除版本检查。当前适配是特定版本下的解决方案，不是所有 Vulkan 程序都应照抄的通用模板。

## 11. 测试到底证明了什么

源码：[camera_tests.cpp](E:/djh/Nothing3D/tests/camera_tests.cpp)、[BoxImageCheck.h](E:/djh/Nothing3D/src/render/BoxImageCheck.h:11)、[GuideImageCheck.h](E:/djh/Nothing3D/src/render/GuideImageCheck.h:6)、[GPU 测试脚本](E:/djh/Nothing3D/scripts/test-gpu.ps1)。

当前分为三层：

1. 基础测试：相机范围、矩阵、几何尺寸、非退化三角形、坐标轴裁剪和角落布局。
2. 无显卡界面烟雾测试：确认窗口可构造，并且空白图不会被误认为有效场景。
3. 真实 GPU 回归：实际绘制，调整窗口、最小化恢复、改变视角和模型角度、读回图像并检查退出阶段。

`hasBox()` 名字沿用早期代码，当前检查对象已扩展到长方体和圆柱体。它把模型三角形投影到图像上，寻找采样点处最近的表面，再比较颜色；因此不只是判断“画面不为空”。辅助图像检查还覆盖方向控件和移到 (50, 0.75, 50) m 后的网格。

最新圆柱体版本的既有结果：基础测试 2/2 通过，Intel UHD 与 RTX 4060 Laptop 各一轮 PASS，包含退出的 Vulkan 验证错误为 0。更早无限网格版本做过双显卡各三轮，不能把它写成最新圆柱版本也做过三轮。本次整理文档没有重新运行构建或 GPU 测试。

这些测试不等于证明所有视角、所有显卡和所有输入都正确。图像参考还复用了生产几何生成函数，独立的是投影、可见性与颜色检查逻辑，因此仍需尺寸测试和人工看图补充。测试报告里的帧数也不是 FPS 性能结论。

## 12. 现在需要读懂的少量 C++

| 写法 | 在本项目中先怎样理解 |
| --- | --- |
| `.h` / `.cpp` | 通常分别放声明与实现；内联的小函数也可能直接写在头文件 |
| `struct` / `class` | 组织数据和行为；先看成员是什么、函数改变了什么 |
| `std::array` / `std::vector` | 固定长度数组 / 可变长度数组 |
| `const` | 当前接口不允许修改这个值或通过它修改对象 |
| `auto` | 编译器推断类型，不等于 Python 式动态类型 |
| `&` / `*` | 在声明中常表示引用/指针；在表达式中也可能表示取地址/解引用 |
| `window_->device()` | 通过窗口指针调用成员函数 |
| `[&](...) { ... }` | lambda 匿名函数，按引用使用外部变量；必须留意变量是否仍然存活 |
| `override` | 明确这是重写基类的回调函数 |
| `{}` | 常用于初始化；局部变量离开作用域会析构 |
| `std::optional` | 结果可能不存在；如轴线完全在视野外 |

第一轮不要求掌握模板元编程、复杂继承或手写矩阵库。先能解释“一段代码读哪些数据、改哪些数据、何时被调用”。

## 13. 适合每周约 6 小时的复盘安排

以下是学习建议，不是必须在一周内全部完成的进度承诺。每次可按 20 分钟看效果、40 分钟读代码、40 分钟动手、20 分钟记录来安排。

第一组：界面与几何。读 MainWindow、BoxGeometry、CylinderGeometry，能指出三个尺寸分别沿哪条轴。理解 12 个长方体三角形和 256 个圆柱三角形从哪里来。

第二组：相机与绘制。读 OrbitCamera、模型矩阵、box 两个着色器和 ColorMesh::draw，能说清“滚轮没有改顶点，画面为什么仍变大”。

第三组：生命周期与辅助视图。读每帧绘制顺序、无限网格交点、方向立方体的小视口，再读同步问题的概念解释。最后才深入 Vulkan 结构体字段。

### 三个小练习

1. 在草稿里画出圆柱的一段，标出四个角和两个侧壁三角形，不改代码也能完成。
2. 只在一次练习中修改自动旋转速度，预测两秒后大约转多少角度，再观察验证。
3. 临时把圆柱分段数改小，观察轮廓为什么变成多边形；完成后还原修改。

尺寸、颜色、分段数和默认相机被一些测试固定使用。练习改动可能导致测试失败，应区分“改了需求造成预期不同”和“实现真的出错”，不要直接删掉断言。先记录原值，练习结束还原自己那几处修改；不要用整目录重置覆盖其他工作。

## 14. 下一阶段的学习重点

接下来最有价值的不是继续增加更多演示形状，而是让显示出来的物体成为真正的独立对象。

建议按这个顺序推进，但尚未在本次实现：

1. 建立独立于 Qt 控件的场景数据：对象 ID、类型、尺寸、位置和旋转。
2. 每个对象保留自己的变换，而不是把所有顶点合成一个演示组。
3. 让参数变化触发几何或渲染数据更新。
4. 再连接场景树、属性面板、创建与选择。
5. 完成命令、保存和编辑闭环后，再设计 Python API。

学习验收问题：如果我要只把圆柱移动 100 mm，而不动长方体，当前需要改哪里的顶点？将来为什么应该改“对象的位置”而不是直接改顶点数组？

能回答这个问题，就开始从“让显卡画出形状”走向“设计一个三维编辑器的数据结构”。
