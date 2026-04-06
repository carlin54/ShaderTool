# ShaderTool

Vulkan-focused shader IDE: edit **HLSL**, compile with **DXC** to **SPIR-V**, and preview in a **Qt** **Vulkan** window.

## Source layout

| Directory | Contents |
|-----------|----------|
| **`src/app/`** | `main.cpp`, `MainWindow` |
| **`src/ui/`** | Stage tabs, `ShaderPlainTextEdit` (line numbers), `HlslHighlighter`, find, export dialog, examples |
| **`src/project/`** | JSON project model (`ShaderProject`), stage enums, path helpers |
| **`src/compiler/`** | DXC subprocess (`ShaderCompiler`), SPIRV-Reflect helpers |
| **`src/preview/`** | Raster `QVulkanWindow` path: pipeline, renderer, mesh, textures, `hostuniforms.h` |
| **`src/rt/`** | `CustomVulkanDevice`, RT preview window/engine, ray-trace stage metadata |

`resources.qrc`, `resources/`, `schema/`, and top-level docs stay at the repository root.

## Requirements

- **CMake** 3.16+, **Git** (CMake **FetchContent** pulls [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) at configure time; network required once)
- **Qt 6** (Widgets + Gui; Vulkan via `QVulkanInstance`; raster preview uses `QVulkanWindow`, ray-tracing preview uses a separate `QWindow` + custom `VkDevice`)
- **Vulkan** development headers and loader (`find_package(Vulkan)`). The app requests **Vulkan 1.2** at the instance (`src/app/main.cpp`) so **ray tracing** + **buffer device address** can be enabled on the RT preview path. A **Vulkan-capable GPU and driver** with `VK_KHR_ray_tracing_pipeline` (and related KHR extensions) is required for **GPU** ray-tracing preview, not just compile-only.
- **DXC** — the `dxc` executable on `PATH`, or set **`SHADERTOOL_DXC`** to its full path
- **ffmpeg** on `PATH` — for **Export → Record animation** (GIF / animated WebP)

Optional: set **`QT_VULKAN_DEBUG=1`** to request the Khronos validation layer when available.

### Project extras

- **`meshPath`**: optional path to a **Wavefront OBJ** file (relative to the `.json` project or absolute). Shaders should use a mesh vertex layout (position, normal, UV) when this is set.
- **`textures`**: JSON array of `{ "path": "...", "slot": 0 }`. Images are bound as **combined image samplers** on **Vulkan descriptor set 1**, bindings `0 … N-1` (fragment stage). Declare matching `[[vk::binding(i, 1)]]` resources in HLSL.
- **Ray tracing** projects: set **Pipeline → Ray trace** and edit RT stages; **Compile** runs **DXC** with `lib_6_3` and appends **SPIRV-Reflect** output. **GPU ray-tracing preview** uses a **custom `VkDevice`** (`CustomVulkanDevice` + `RtPreviewWindow`) with **buffer device address**, **`VK_KHR_ray_tracing_pipeline`**, and related extensions — separate from the raster **`QVulkanWindow`** (Qt does not expose those feature chains on the default device). The minimal example expects **raygen**, **miss**, and **closest_hit** stages; output is **blitted** to the swapchain after `vkCmdTraceRaysKHR`. **Host uniform catalog** matches raster: **`[[vk::binding(0, 0)]]`** output image, **`[[vk::binding(1, 0)]] cbuffer Host`** (same `HostUniforms` layout as [`src/preview/hostuniforms.h`](src/preview/hostuniforms.h)); **`[[vk::binding(2, 0)]] RaytracingAccelerationStructure`** for the built-in **TLAS** when using **`TraceRay`**; the preview fills the UBO every frame.
- **Host catalog include** (optional): embedded at `:/include/host_catalog.hlsl` — copy members into your shader `cbuffer` or include via tooling; offsets must match [hostuniforms.h](src/preview/hostuniforms.h).

A JSON Schema draft lives at [schema/shaderproject.schema.json](schema/shaderproject.schema.json); it is embedded in the binary as `:/schema/shaderproject.schema.json` and used to validate projects on **Open** / **Examples** (in addition to semantic checks).

## Install / packaging

```bash
cmake --install build --prefix /usr/local
```

**CI** (GitHub Actions) runs `cmake --install` and uploads install artifacts, and also builds portable bundles with the deploy scripts:
- Linux: `ShaderTool-linux-install` + `ShaderTool-linux-portable`
- Windows: `ShaderTool-windows-install` + `ShaderTool-windows-portable`

On pushes to `master`, GitHub also publishes/updates a rolling **Latest master build** prerelease (tag `latest`) with downloadable portable assets. This is the standard approach instead of committing executables into git history.

For portable bundles, use:
- [scripts/deploy_linux.sh](scripts/deploy_linux.sh) (linuxdeploy + Qt plugin; produces `ShaderTool-linux-portable.tar.gz`)
- [scripts/deploy_windows.bat](scripts/deploy_windows.bat) (`windeployqt`; copies `dxc.exe` when available)

Ship **dxc** / **dxcompiler** next to the app when redistributing; see [NOTICE](NOTICE).

### Optional: `dxcompiler` shared library

ShaderTool now supports an optional **in-process** backend using Microsoft’s **`dxcompiler`** (runtime-loaded), with automatic fallback to CLI `dxc`.

Compiler discovery/order:
1. In-process `dxcompiler` (when `dxcapi.h` was available at build time and runtime library is found)
2. `SHADERTOOL_DXC` (explicit path to `dxc`)
3. `SHADERTOOL_DXC_LIB` directory hint (looks for sibling `dxc` / `dxc.exe`)
4. `dxc` on `PATH`

Runtime library discovery for in-process path:
1. `SHADERTOOL_DXC_LIB` (explicit path to `dxcompiler` shared library)
2. app directory sibling (`dxcompiler.dll` / `libdxcompiler.so`)

### Preview dock (raster vs ray tracing)

The main window uses a **`QStackedWidget`**: **Raster** shows a `PreviewVulkanWindow` (`QVulkanWindow`, Qt’s Vulkan device). **Ray trace** shows `RtPreviewWindow` (a `QWindow` with a **custom `VkDevice`** that enables ray-tracing extensions). Only one stack page is visible; both can be docked/floated like any `QDockWidget`. **Export → Record animation** uses the **raster** preview path only (same sim-time override as the docked raster window).

**RT preview scope:** The GPU path builds a **ray-tracing pipeline** + **SBT** and runs **`vkCmdTraceRaysKHR`**. The engine builds a **minimal BLAS/TLAS** (single triangle + one instance), binds the **TLAS** at **`[[vk::binding(2, 0)]]`**, and the **Examples → Ray tracing → Minimal** sample uses **`TraceRay`** from **raygen**. Ray-trace projects must declare **exactly one** **raygen**, at least one **miss**, and at least one **closest_hit** (validated on load and before compile). Additional **miss**, **closest_hit**, **any_hit**, **intersection**, and **callable** stages are accepted and packed into RT groups/SBT in stage order. Optional project JSON field **`maxPipelineRayRecursionDepth`** (0–31, **0** = default) maps to **`VkRayTracingPipelineCreateInfoKHR::maxPipelineRayRecursionDepth`** (clamped to the device limit).

Use **Build → Set RT preferred GPU…** to choose a substring filter for RT-capable device names (stored in `QSettings`). Leave empty to auto-pick the first compatible device.

For project safety, mesh/texture paths in JSON must be **relative to the project directory** and must not escape it (`..` traversal and absolute paths are rejected on open).

## Build (Linux)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/ShaderTool
```

Install distro packages as needed (names vary), e.g. Qt6 development packages and `libvulkan-dev`.

### Install DXC on Linux (example)

If `dxc` is not available from your distro packages, you can install it from Microsoft's release tarball:

```bash
wget https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2407/linux_dxc_2024_07_31.x86_64.tar.gz
tar -xzf linux_dxc_2024_07_31.x86_64.tar.gz
sudo cp bin/dxc /usr/local/bin/
sudo cp lib/libdxcompiler.so* /usr/local/lib/
sudo ldconfig
dxc --version
```

If you install `dxc` in a non-standard location, set:

```bash
export SHADERTOOL_DXC="/full/path/to/dxc"
```

## Build (Windows)

Install the **Vulkan SDK** (headers + loader), **Qt 6** (MSVC or MinGW kit matching your toolchain), and **CMake** 3.16+. Point CMake at Qt with **`CMAKE_PREFIX_PATH`** (the directory that contains `Qt6Config.cmake`). Then configure and build from a **Developer Command Prompt** (or MSVC environment) if you use the MSVC Qt build:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

GitHub Actions runs **Linux** (apt packages) and **Windows** (Qt via **install-qt-action**, **Vulkan SDK** via Chocolatey, **MSVC**). If `find_package(Vulkan)` fails on a machine, set **`VULKAN_SDK`** to your SDK root.

## Project files

Shader projects are JSON (`.json`) with `formatVersion`, `pipelineKind`, and a `stages` array. Use **File → Open / Save** in the app.

Built-in **Examples → Basics** samples load from embedded resources under `resources/examples/` (see `resources.qrc`).

## License

ShaderTool source is under the **MIT License** ([LICENSE](LICENSE)). Qt and other runtime dependencies have their own terms; see [NOTICE](NOTICE).
