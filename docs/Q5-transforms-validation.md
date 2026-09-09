# Q5 位置与旋转编辑验证记录

验证日期：2026-09-08。环境沿用 Qt 6.10.3、MSVC 2022、Vulkan SDK 1.4.357.0。

## 构建及自动测试

`scripts/test.bat`：最终 Debug 构建成功，6/6 测试通过，无新增 C++ 编译警告。保留既有 Qt 私有模块版本绑定提示。

新增 `transform_editor` 通过无显卡的已布局窗口验证：

- 六轴正负输入、回车及失焦应用，键入过程中不提前提交。
- 移动并旋转后底面中心坐标正确，尺寸及相邻对象不变。
- 保留选中编号和列表条目，悬停参数同步更新。
- NaN、Inf、空文本和超范围值恢复原值；位置与角度的正负边界可输入。
- 多圈角度保留输入值；普通位置/角度步进为 1 mm / 1°。
- Ctrl 键盘步进为 100 mm / 15°，正反向正确；Ctrl 鼠标步进按钮同样为 15°，不受 Qt 额外倍率影响。
- 未聚焦的滚轮不修改参数；切换选择时未提交文本不影响新对象。
- 圆柱体独立变换；取消选择、空场景、重新加载预设正确刷新输入框。
- 显示舍入不改写参数；单轴编辑及尺寸编辑都保留其他变换分量的原始精度。

其余 `dimension_editor`、`scene_picking`、`scene_objects`、`camera_math`、`m0_smoke_test` 全部通过。

## 最终双显卡回归

最终控件样式下执行 `scripts/test-gpu.ps1 -Rounds 1`，每块显卡一轮：

| 显卡 | 连续帧 | 交换链代数 | 含退出阶段验证错误 | 结果 |
| --- | ---: | ---: | ---: | --- |
| Intel UHD Graphics | 430 | 8 | 0 | PASS |
| NVIDIA GeForce RTX 4060 Laptop GPU | 417 | 8 | 0 | PASS |

原有相机、辅助线、窗口调整、场景切换、单选高亮和尺寸编辑回归通过。新增 GPU 与窗口检查通过：

- 业务场景与渲染快照一致，保留默认及自定义相机、预览旋转角度和选择。
- 屏幕采样确认变换后的旧区域不再命中，新增区域可以命中。
- 长方体移动倾斜、圆柱体独立变换、预览旋转中修改、最小窗口四组图像与 CPU 几何及颜色参考匹配。
- 960 × 600 窗口的属性面板可滚动访问全部输入框。

已人工查看最终长方体变换图像和属性面板顶部、旋转区域截图：高亮和遮挡正确；所有输入框统一深色，文本、单位及按钮可辨认，下方字段可通过滚动访问。

## 本地证据与范围

- `build/q3-intel-1.txt`、`build/q3-nvidia-1.txt`：逐显卡汇总。
- `build/q3-intel-1.log`、`build/q3-nvidia-1.log`：含 `Transform ...: 1` 和 `Transform overall: 1` 的结果。
- `build/q5-transform-box-edited.png`、`build/q5-transform-cylinder-edited.png`：两类对象的独立变换。
- `build/q5-transform-preview-edited.png`、`build/q5-transform-minimum-window.png`：预览姿态及最小窗口下的三维画面。
- `build/q5-transform-panel-top.png`、`build/q5-transform-panel-rotation.png`：属性面板顶部和滚动后的旋转输入区。

文本报告逐显卡保留，截图由最后一块显卡覆盖。边界测试验证输入及数据更新，不承诺极端坐标的画面精度。本轮未进行 1000 对象性能、创建放置、移动手柄、网格吸附、保存或撤销验收。
