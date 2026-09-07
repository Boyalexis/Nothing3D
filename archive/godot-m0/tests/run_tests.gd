extends SceneTree


func _init() -> void:
	var failures: Array[String] = []
	_check(ProjectSettings.get_setting("application/config/name") == "Nothing3D", "项目名称应为 Nothing3D", failures)
	_check(ResourceLoader.exists("res://scenes/main.tscn"), "主场景文件应存在", failures)

	var main_scene := load("res://scenes/main.tscn") as PackedScene
	_check(main_scene != null, "主场景应能成功加载", failures)

	if failures.is_empty():
		print("PASS: M0 smoke tests (3/3)")
		quit(0)
		return

	for failure in failures:
		push_error("FAIL: " + failure)
	quit(1)


func _check(condition: bool, message: String, failures: Array[String]) -> void:
	if not condition:
		failures.append(message)

