cmake_minimum_required(VERSION 3.25)

project(Nothing3D VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Vulkan REQUIRED)
find_package(Qt6 6.10.3 EXACT REQUIRED COMPONENTS Widgets GuiPrivate)

qt_standard_project_setup()

qt_add_executable(Nothing3D WIN32
    src/app/main.cpp
    src/ui/MainWindow.cpp
    src/ui/Ribbon.cpp
    src/ui/MainWindow.h
    src/render/VulkanViewport.cpp
    src/render/VulkanViewport.h
    src/render/QtPresentSync.cpp
    src/render/QtPresentSync.h
    src/render/ColorMesh.cpp
    src/render/ColorMesh.h
)

# Compile GLSL at build time; embed SPIR-V so runtime does not depend on cwd.
if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "Vulkan SDK glslc is required for shader compilation.")
endif()
set(SHADER_OUTPUTS)
foreach(SHADER box grid)
foreach(STAGE vert frag)
    set(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${SHADER}.${STAGE}")
    set(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.${STAGE}.spv")
    add_custom_command(OUTPUT "${OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" --target-env=vulkan1.0 "${SOURCE}" -o "${OUTPUT}"
        DEPENDS "${SOURCE}" VERBATIM)
    list(APPEND SHADER_OUTPUTS "${OUTPUT}")
endforeach()
endforeach()
qt_add_resources(Nothing3D box_shaders PREFIX "/shaders"
    BASE "${CMAKE_CURRENT_BINARY_DIR}/shaders" FILES ${SHADER_OUTPUTS})

target_include_directories(Nothing3D PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(Nothing3D PRIVATE Qt6::Widgets Qt6::GuiPrivate Vulkan::Headers)

if(MSVC)
    target_compile_options(Nothing3D PRIVATE /W4 /permissive- /utf-8)
endif()

enable_testing()
add_executable(camera_tests tests/camera_tests.cpp)
target_include_directories(camera_tests PRIVATE src)
target_link_libraries(camera_tests PRIVATE Qt6::Gui)
add_test(NAME camera_math COMMAND ${CMAKE_COMMAND} -E env
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}" $<TARGET_FILE:camera_tests>)

add_test(
    NAME m0_smoke_test
    COMMAND ${CMAKE_COMMAND} -E env
        "QT_QPA_PLATFORM=offscreen"
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
        $<TARGET_FILE:Nothing3D> --smoke-test
)
