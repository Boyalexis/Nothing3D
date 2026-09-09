# Q4 首个小目标验证记录

验证日期：2026-09-07。环境沿用项目锁定的 Qt 6.10.3、MSVC 2022、Vulkan SDK 1.4.357.0。

## 构建及无显卡测试

`scripts/test.bat`：Debug 构建成功，3/3 测试通过。

- `scene_objects`：对象独立性、稳定编号、不存在的编号、删除与清空、非法尺寸和非有限变换的原子拒绝、毫米到米换算、底面中心、尺寸及旋转顺序、圆柱直径、类型替换、共享网格范围及 1000 个 CPU 对象。
- `camera_math`：原有相机、投影、基本体几何及辅助线回归。
- `m0_smoke_test`：无显卡构造窗口、功能区与两对象场景列表。

## 真实桌面双显卡回归

执行 `scripts/test-gpu.ps1 -Rounds 1`，每块显卡一轮：

| 显卡 | 连续帧 | 交换链代数 | 含退出阶段的验证错误 | 结果 |
| --- | ---: | ---: | ---: | --- |
| Intel UHD Graphics | 425 | 3 | 0 | PASS |
| NVIDIA GeForce RTX 4060 Laptop GPU | 405 | 3 | 0 | PASS |

原有三张基准画面、导航、缩放、最小化恢复、预览旋转、重置、无限网格、方向立方体和功能区显示组合检查全部通过。

每块显卡新增 12 次实际视口读回：四种示例各切换两次，再执行长方体替换为圆柱、删除一个对象、清空、恢复初始场景。所有画面均与 CPU 生成的实际尺寸几何参考匹配；列表对象数量、名称和编号同步检查通过。CPU 参考按实际尺寸生成顶点，不调用渲染器的单位几何缩放函数。

已人工查看界面截图与六对象视口截图：场景列表、示例选择和参数说明清晰，多个基本体与辅助线正常显示。Qt 的 QWidget 截图不包含嵌入的原生 Vulkan 视口，所以窗口截图用于界面布局检查，三维画面使用独立的 viewport grab 截图验证。

## 本地证据和复现

- `build/q3-intel-1.txt`、`build/q3-nvidia-1.txt`：双显卡汇总。
- `build/q3-intel-1.log`、`build/q3-nvidia-1.log`：含 `Q4 ... pixels=1` 的逐项结果。
- `build/q4-example-1-1.png`：只改变长方体参数。
- `build/q4-example-1-2.png`：六对象画面。
- `build/q4-example-1-3.png`：空场景画面。
- `build/q4-replace-type.png`、`build/q4-remove-object.png`、`build/q4-clear.png`、`build/q4-restored.png`：类型、删除、清空和恢复。
- `build/q4-window.png`：界面布局。

测试保留历史 `q3-` 汇总命名，新增截图采用 `q4-`；截图会被最后一块显卡覆盖，逐显卡日志分别保留。`build` 为本机生成产物，不提交仓库。

本轮没有进行 1000 对象 GPU 性能验收，也没有验证 Q5 的拾取、创建和属性编辑功能。
