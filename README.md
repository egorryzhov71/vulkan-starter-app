# Vulkan Starter App

Lab 2, variant 6: a square pyramid with a movable camera and directional
Blinn-Phong lighting. The base assignment for grade 3 includes procedural face
normals and ImGui controls for the camera, light, ambient light, and material.

## Requirements

- A C++20 compiler and CMake 3.20 or newer.
- Git; CMake downloads GLFW, vk-bootstrap, Vulkan Memory Allocator, and ImGui.
- The Vulkan SDK, including `glslc`, plus a working Vulkan driver. On macOS,
  configure the SDK environment and MoltenVK before building and running.
- GLM installed with its CMake package configuration (`glmConfig.cmake`). If CMake
  cannot find it, pass `-Dglm_DIR=/path/to/glm/cmake/config/directory` when configuring.
- Make for the `debug` and `release` presets, Visual Studio 2022 for the MSVC presets,
  or MinGW with `mingw32-make` for the MinGW presets.

## Build and run

Run these commands from the repository root on macOS or Linux:

```sh
cmake --preset debug
cmake --build build-debug --parallel
./build-debug/vulkan-starter-app
```

For a release build, use `--preset release`, build `build-release`, and run
`./build-release/vulkan-starter-app`.

On Windows with Visual Studio 2022:

```powershell
cmake --preset msvc-debug
cmake --build build-debug --config Debug --parallel
.\build-debug\Debug\vulkan-starter-app.exe
```

For release, use `msvc-release`, `build-release`, `--config Release`, and
`.\build-release\Release\vulkan-starter-app.exe`.

On Windows with MinGW (compiler, `mingw32-make`, and Vulkan SDK tools on `PATH`):

```powershell
cmake --preset mingw-debug
cmake --build build-debug --parallel
.\build-debug\vulkan-starter-app.exe
```

For release, use `mingw-release`, build `build-release`, and run
`.\build-release\vulkan-starter-app.exe`. Use a separate build directory when
switching between MinGW and Visual Studio generators.

The build compiles `shaders/pyramid.vert` and `shaders/pyramid.frag` into `.spv`
files. Keep the working directory at the repository root when launching: the
application loads those files through relative paths.

The Unix presets use Makefiles. If a build directory was previously configured
with Ninja or another generator, move it aside before configuring with these presets.

## Controls

| Keys | Action |
| --- | --- |
| Arrow keys or right mouse drag | Turn the camera |
| W / A / S / D | Move the camera forward / left / back / right |
| Q / E | Move the camera down / up |
| P | Toggle perspective and orthographic projection |
| R | Reset the camera, lighting, material, and model |

Camera controls pause while ImGui captures the corresponding keyboard or mouse
input. Use the ImGui panel to edit camera position, orientation, field of view,
clipping planes, light direction/color/intensity, ambient color/intensity,
material albedo/specular color/shininess, and model transforms.

## Geometry and camera tests

The build includes tests for pyramid winding, procedural normals, the camera
basis, and Vulkan projection depth:

```sh
ctest --test-dir build-debug --output-on-failure
```

## Smoke test

The smoke test opens the window, renders 120 frames, and exits:

```sh
./build-debug/vulkan-starter-app --smoke-test
```

The application requests Vulkan validation layers when available. With the SDK's
validation layer installed, enable synchronization validation for an additional check:

```sh
VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT ./build-debug/vulkan-starter-app --smoke-test
```

Inspect the validation output as well as the exit status; validation messages do
not automatically make the smoke test fail.
