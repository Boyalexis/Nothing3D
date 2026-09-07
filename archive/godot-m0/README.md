# Nothing3D

Nothing3D 是一个用于边做边学的三维参数化场景编辑器。第一阶段使用 Godot 4 和 GDScript，目标范围见 [Nothing3D-Spec.md](Nothing3D-Spec.md)。

## 本地运行

本项目当前使用项目内的 Godot 4.7.2 标准便携版：

```powershell
.\tools\godot\Godot_v4.7.2-stable_win64.exe --path . --editor
```

直接运行项目：

```powershell
.\tools\godot\Godot_v4.7.2-stable_win64.exe --path .
```

运行自动测试：

```powershell
.\tools\godot\Godot_v4.7.2-stable_win64_console.exe --headless --path . --script res://tests/run_tests.gd
```

## 当前进度

- M0：环境与空应用（进行中）
- M1：三维视口与相机（未开始）
