# 正交与标准视图验证

日期：2026-09-12。

## 最终结果

- `scripts/test.bat`：14/14 通过，5.37 秒。
- `scripts/test-gpu.ps1 -Rounds 1`：Intel UHD 与 RTX 4060 Laptop 均 PASS。
- Intel：482 帧，13 次交换链创建，包含退出阶段的 Vulkan 验证错误为 0。
- NVIDIA：470 帧，13 次交换链创建，包含退出阶段的 Vulkan 验证错误为 0。
- 两张显卡日志均包含 `Orthographic ribbon and rendered views: 1`。
- `git diff --check` 无空白错误。

## 覆盖范围

扩展 `camera_math`：目标平面投影切换比例、正交远近等比例、三种标准方向的矩阵可逆性、对象拾取、顶视地面交点与 Ctrl 吸附、前/右视不可放置、可见移动轴、237 mm 吸附到 200 mm、两种视口尺寸、顶视平移、继续旋转和重置透视。

真实窗口测试通过五个视图按钮切换，检查相机模式并读取 GPU 图像；检查重置后透视按钮状态。既有导航、选择、尺寸、变换、创建、移动、历史、复制删除及工程保存测试继续通过。

人工检查 GPU 截图：顶视可见箱子矩形和圆柱圆形顶面，右视箱子遮挡后方圆柱；指南针方向正确。修正侧视下相反方位字母重叠后，重新完成两张显卡验证。

## 证据

- `build/topView.png`、`build/frontView.png`、`build/rightView.png`
- `build/orthographicView.png`、`build/perspectiveView.png`
- `build/q3-intel-1.txt`、`build/q3-intel-1.log`
- `build/q3-nvidia-1.txt`、`build/q3-nvidia-1.log`

截图保留最后一次显卡运行结果。帧数是验证过程的累计渲染帧数，不是性能指标；本轮没有执行 1,000 对象性能验收。
