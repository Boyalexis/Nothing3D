# Q5 尺寸编辑验证记录

验证日期：2026-09-08。环境为项目锁定的 Qt 6.10.3、MSVC 2022 和 Vulkan SDK 1.4.357.0。

## 构建与自动测试

`scripts/test.bat`：Debug 构建成功，5/5 测试通过，无新增 C++ 编译警告。Qt 私有模块的既有版本绑定提示仍保留。

新增 `dimension_editor` 无显卡界面测试，通过实际输入事件验证：

- 输入时不提前提交，回车及焦点移出正确提交。
- 长方体三维尺寸、圆柱体直径与高度可分别编辑；步进立即应用。
- 修改保持位置、旋转、相邻对象、列表条目身份及选中编号，悬停说明更新。
- 零、负数、NaN、Inf、超上限和空文本结束编辑时恢复旧值；最小和最大边界可接受。
- 切换类型时正确隐藏深度字段；旧对象未提交的文本不影响新对象。
- 取消选择、空场景及重新加载预设时的输入状态正确。
- 仅显示舍入不改写数据，编辑一个字段不损失其他字段的原始精度。

其余四项 `scene_picking`、`scene_objects`、`camera_math`、`m0_smoke_test` 通过。

## 双显卡回归

执行 `scripts/test-gpu.ps1 -Rounds 1`，每块显卡一轮：

| 显卡 | 连续帧 | 交换链代数 | 含退出阶段验证错误 | 结果 |
| --- | ---: | ---: | ---: | --- |
| Intel UHD Graphics | 407 | 6 | 0 | PASS |
| NVIDIA GeForce RTX 4060 Laptop GPU | 402 | 6 | 0 | PASS |

既有 Q3 导航和辅助线、Q4 场景变更、Q5 选择与高亮回归均通过。尺寸编辑新增检查通过：

- 业务对象和渲染快照尺寸一致，保留选中状态、默认相机、自定义视角及预览旋转角度。
- 长方体编辑、圆柱体编辑、旋转后编辑、最小窗口四组 GPU 读回与 CPU 几何和面色参考匹配。
- 屏幕采样确认缩小后的区域不再命中长方体，增高后新增区域可以命中。
- 在 960 × 600 最小窗口下检查属性面板，并恢复原测试场景完成退出验证。

已人工查看属性面板、长方体和圆柱体编辑后的截图：尺寸输入、单位与说明清晰，模型大小正确、高亮保留，未见裁切或重叠。

## 本地证据与限制

- `build/q3-intel-1.txt`、`build/q3-nvidia-1.txt`：逐显卡汇总。
- `build/q3-intel-1.log`、`build/q3-nvidia-1.log`：含 `Dimensions ...: 1` 和 `Dimensions overall: 1` 的逐项结果。
- `build/q5-dimensions-box-edited.png`、`build/q5-dimensions-cylinder-edited.png`：两类实体的尺寸变化。
- `build/q5-dimensions-rotated-edited.png`、`build/q5-dimensions-minimum-window.png`：视角保持及窗口调整后的三维画面。
- `build/q5-dimensions-panel.png`：最小窗口下的属性面板。

截图由最后测试的显卡覆盖，逐显卡文本报告分别保留。测试验证了范围边界输入，不包含极端尺寸的画面质量或 1000 对象性能验收；位置旋转编辑、保存和撤销不属于本轮范围。
