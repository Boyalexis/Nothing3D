# 当前版本提交验证

日期：2026-09-09。

本次版本包含工作区已实现的 Q4 场景基础、Q5 选择/尺寸/变换/创建放置功能，以及有限网格死代码清理和新的 [中央视图学习说明](Viewport-learning.md)。

## 构建与自动测试

`scripts/test.bat` 执行成功，Debug 构建完成，7/7 自动测试通过：ground_placement、transform_editor、dimension_editor、scene_picking、scene_objects、camera_math、m0_smoke_test。

构建保留既有 Qt GuiPrivate 版本绑定提示，当前使用项目锁定的 Qt 6.10.3。

## 双显卡回归

`scripts/test-gpu.ps1 -Rounds 1` 完成，每块显卡一轮：

| 显卡 | 帧数 | 交换链代数 | 含退出阶段验证错误 | 结果 |
| --- | ---: | ---: | ---: | --- |
| Intel UHD Graphics | 424 | 10 | 0 | PASS |
| NVIDIA GeForce RTX 4060 Laptop GPU | 405 | 10 | 0 | PASS |

两轮均报告 Render/resize/readback checks: PASS，reference_images=3，camera/model/input_checks=1。此次采用自动绘制、窗口调整和读回检查，没有额外宣称人工截图验收。

本地证据位于 `build/q3-intel-1.txt`、`build/q3-intel-1.log`、`build/q3-nvidia-1.txt`、`build/q3-nvidia-1.log`。构建产物和截图按项目忽略规则不提交。

有限网格 `Guides::ground()` 及其四行测试已移除，源码和测试中无剩余引用。当前无限网格着色器与无限 Y 轴实现保留。
