# Vulkan Starter App

A Vulkan square pyramid demo with vertex colors, depth testing, keyboard controls,
and an ImGui overlay.

## Requirements

- A C++20 compiler and CMake 3.20 or newer.
- Git; CMake downloads GLFW, vk-bootstrap, Vulkan Memory Allocator, and ImGui.
- The Vulkan SDK, including `glslc`, plus a working Vulkan driver. On macOS,
  configure the SDK environment and MoltenVK before building and running.
- GLM installed with its CMake package configuration (`glmConfig.cmake`). If CMake
  cannot find it, pass `-Dglm_DIR=/path/to/glm/cmake/config/directory` when configuring.
- Make for the `debug` and `release` presets, or Visual Studio 2022 for the MSVC presets.

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

The build compiles `shaders/pyramid.vert` and `shaders/pyramid.frag` into `.spv`
files. Keep the working directory at the repository root when launching: the
application loads those files through relative paths.

The Unix presets use Makefiles. If a build directory was previously configured
with Ninja or another generator, move it aside before configuring with these presets.

## Controls

| Keys | Action |
| --- | --- |
| Arrow keys | Rotate the pyramid |
| W / A / S / D | Move the pyramid |
| Q / E | Decrease / increase scale |
| P | Toggle perspective and orthographic projection |
| R | Reset the transform and projection |

Keyboard controls pause while ImGui captures keyboard input.

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
