# Ubuntu 22.04 + GNOME X11 兼容性进度

最后更新：2026-08-05

## 1. 目标

本分支以 `Almamu/linux-wallpaperengine` 为基础，目标是在保持现有 Scene、Video 和 Web 壁纸渲染能力的前提下，改善 Ubuntu 22.04、GNOME Shell 42 和 X11 环境中的构建与桌面集成体验。

当前开发分支：`agent/ubuntu-22-04-compatibility`

当前兼容性修复提交：`de7dd54 Fix Ubuntu 22.04 build compatibility`

该分支已经推送至 Fork，但尚未创建 PR，也尚未合并到 `main`。

## 2. 已验证环境

| 项目 | 环境 |
| --- | --- |
| 操作系统 | Ubuntu 22.04.5 LTS（Jammy Jellyfish） |
| 内核 | Linux 6.8.0-136-generic，x86_64 |
| 桌面 | GNOME Shell 42.9 |
| 会话 | X11 |
| CPU | 13th Gen Intel Core i7-13700 |
| GPU | Intel 集成显卡 `8086:a780` + NVIDIA 独立显卡 `10de:2582` |
| 编译器 | GCC/G++ 11.4.0 |
| CMake | 3.22.1 |
| Steam Wallpaper Engine | 已通过 Proton Experimental 安装 |
| Workshop 测试资源 | 17 个 Scene、5 个 Video、1 个 Web 项目 |

已完成以下无界面验证：

- Release 构建完整成功。
- `linux-wallpaperengine --help` 正常运行。
- `ldd` 未发现缺失的动态库。
- `git diff --check` 通过。
- Steam Wallpaper Engine 的 `assets` 目录能够被找到。

已完成以下桌面实测：

- Workshop 项目 `2955458015` 能在 `--window 0x0x1280x720` 模式下正常显示，Scene Shader 依赖 `LightingV1` 能够解析。
- `Ctrl+C` 产生 `SIGINT` 后，程序输出 `Stop requested by signal 2` 和 `Stopping`，可以受控退出。

音频、鼠标交互、Video、Web、多显示器、全屏暂停和长时间资源占用仍需继续在真实桌面会话中验证。

## 3. 已知构建兼容性问题

### 3.1 MediaSource 头文件依赖不完整

状态：**已修复并验证**

问题：

- `MediaSource.h` 直接使用 `std::optional`、`std::shared_ptr` 和 `uint32_t`，但没有直接包含 `<optional>`、`<memory>` 和 `<cstdint>`。
- `DBusMediaSource.h` 直接使用 `std::optional`，但依赖其他头文件的传递包含。
- GCC 11 在当前包含顺序下无法解析这些类型，并产生大量后续连锁错误。

解决方案：

- 在声明这些类型的头文件中直接包含对应标准库头文件。
- 避免依赖不稳定的传递包含。

验证：修复后相关源文件通过编译，原来的 `url`、`m_aliveFlag` 和 lambda 捕获错误全部消失。

### 3.2 未使用的 GMP 头文件阻止构建

状态：**已修复并验证**

问题：`FBOProvider.cpp` 包含 `<gmpxx.h>`，但文件中没有使用任何 GMP 类型或函数。Ubuntu 22.04 未安装 `libgmp-dev` 时构建失败。

解决方案：删除未使用的 GMP 头文件，而不是新增无意义的构建依赖。

验证：删除后该编译单元正常构建，不需要安装 `libgmp-dev`。

### 3.3 GCC 11 缺少可用的 `<format>`

状态：**已修复并验证**

问题：Ubuntu 22.04 的 GCC/libstdc++ 11 无法提供项目所需的 `std::format`，导致 `ColorBuilder.cpp` 编译失败。

解决方案：

- 使用简单的十六进制字符表展开 CSS `#RGB` 和 `#RGBA` 短颜色。
- 将 alpha 限制在 `[0, 1]`，再转换成两个十六进制字符。
- 不引入额外格式化库，也不要求用户升级系统编译器。

验证：GCC 11.4 下完整链接成功。

### 3.4 Ubuntu 依赖清单遗漏 D-Bus 开发包

状态：**README 已修复**

问题：当前 CMake 使用 `find_package(DBus REQUIRED)`，但 Ubuntu 安装命令没有包含 `libdbus-1-dev`。

解决方案：在 Ubuntu 22.04 和 Ubuntu 24.04 的 README 安装命令中加入 `libdbus-1-dev`。

### 3.5 GCC 收到 Clang 专用警告选项

状态：**已修复并验证**

现象：GCC 输出：

```text
cc1plus: note: unrecognized command-line option '-Wno-undefined-var-template'
```

影响：当前只是提示，不会导致构建失败，但会污染日志并掩盖真正警告。

解决方案：CEF 的 `cef_variables.cmake` 用 `CHECK_CXX_COMPILER_FLAG` 检测该 Clang 专用选项，GCC 对未知 `-Wno-*` 只打 note 被误判为支持。在 `CMakeLists.txt` 的 CEF flags 清理区块按 `CMAKE_CXX_COMPILER_ID STREQUAL "GNU"` 剔除该选项。

验证：重新配置 + 增量构建后日志不再出现 `unrecognized command-line option '-Wno-undefined-var-template'`。

## 4. 已知运行与桌面集成问题

### 4.1 GNOME X11 会覆盖根窗口壁纸

状态：**已确认根因，尚未在核心程序中解决**

问题：现有 `--screen-root` 使用 X11 根窗口 Pixmap。GNOME Shell 自己绘制桌面背景层，并将根窗口内容覆盖，因此渲染器可能正常运行，但用户看不到动态壁纸。

临时方案：使用 `--window XxYxWxH` 在普通窗口中预览和调试壁纸。

不推荐的临时方案：使用外部脚本调用 `xdotool` 和 `wmctrl`，把普通窗口设置为置底、所有工作区可见并从任务栏隐藏。这种方式可以工作，但依赖窗口标题匹配和启动时序，稳定性及安全性不足。

推荐的核心解决方案：增加 GNOME X11 桌面窗口模式，例如 `--gnome-x11`：

1. 使用 GLFW 创建无边框、不获取焦点的渲染窗口。
2. 通过 Xlib/EWMH 直接设置 `below`、`sticky`、`skip_taskbar` 和 `skip_pager` 状态。
3. 直接调用 `XLowerWindow`，不再依赖 `wmctrl` 或 `xdotool`。
4. 根据实际测试决定是否使用 `_NET_WM_WINDOW_TYPE_DESKTOP`；如果该类型仍被 GNOME 背景层覆盖，则保留普通窗口类型并仅设置置底状态。
5. 为窗口设置稳定且唯一的 `WM_CLASS`，避免误操作其他窗口。

### 4.2 显式窗口模式不是真正的壁纸模式

状态：**已实测并确认，待改进**

问题：`--window` 的设计目标是固定几何位置的预览。当前代码会设置 `GLFW_DECORATED = false`、`GLFW_RESIZABLE = false` 和 `GLFW_FLOATING = true`，创建无边框、不可调整大小且始终置顶的窗口。由于没有标题栏，GNOME 窗口管理器没有常规拖动区域；程序也没有实现客户端鼠标拖动，所以窗口不能直接拖动。这不是渲染故障。

现有使用方案：

- 需要可拖动、可缩放的测试窗口时，不传 `--window`，使用默认的 `NORMAL_WINDOW` 模式。
- 必须使用显式几何窗口时，可以通过改变 `--window XxYxWxH` 中的 `X`、`Y` 后重启来定位；GNOME 的窗口移动快捷键也可能临时移动该窗口，但不应作为正式功能依赖。

建议方案：将“普通预览窗口”“固定几何窗口”和“桌面窗口”保持为三个明确模式。普通预览保留窗口装饰和缩放；固定几何窗口维持当前行为；桌面窗口使用独立的 GNOME X11/EWMH 集成，避免把相互冲突的行为堆叠到同一选项。

### 4.3 普通窗口比例和运行时缩放异常

状态：**已定位最终根因并在本机 GNOME X11 桌面验证通过**

本机现象：普通窗口默认没有按素材比例完整显示；拖动窗口边缘改变尺寸后，画面损坏且恢复原尺寸也不能恢复。

已确认的问题：

- 普通窗口默认尺寸为 `640x480`，默认 `DefaultUVs + ClampUVs` 不保证完整显示素材比例，越界纹理还会延展边缘像素。
- GLFW 事件原先在本帧绘制和交换缓冲之后才处理，`GLFWWindowOutput` 也在绘制之后才读取 framebuffer 尺寸，因此 resize 时会用旧 viewport 绘制到新 framebuffer。
- 没有过滤最小化或窗口管理器调整过程中出现的零尺寸，异常尺寸可能进入 viewport 和 UV 状态。
- 更关键的是，普通预览窗口也创建了 X11 全屏检测器。当检测器把当前桌面的其他窗口判断为全屏后，应用进入暂停分支，不再调用 GLFW 事件处理和绘制；此时扩大窗口只会暴露从未重绘的 back buffer 区域。

已实施的修复：

- 当用户没有显式提供 `--scaling` 和 `--clamp` 时，普通窗口和固定几何窗口默认使用 `fit + border`，完整保持素材比例并用边框填充空白区域；桌面背景模式继续保持原有默认值。
- 在窗口模式的每帧绘制前处理事件并更新 viewport，使本帧 framebuffer 与 viewport 一致。
- framebuffer 宽或高为零时保留上一个有效 viewport、等待恢复事件并跳过绘制，避免污染 UV 状态。
- 同步更新窗口 viewport 的物理尺寸和逻辑尺寸。
- X11 根窗口输出仍在绘制之后复制图像，未改变其输出时序。
- 普通和显式预览窗口改用无操作的全屏检测器；只有真正的 `DESKTOP_BACKGROUND` 模式才启用全屏暂停。

第一次桌面复测仍能复现：窗口右侧出现重复的竖条，底部出现重复横条，恢复尺寸后仍然存在。禁用视差后现象不变，因此排除了 Scene 鼠标视差。

追加修复：

- 在最终合成前显式绑定默认 framebuffer，禁用 scissor，恢复 RGBA 和深度写入，设置覆盖完整 framebuffer 的 viewport，然后清理颜色和深度缓冲，避免新扩展区域保留旧 back buffer 或 effect pass 状态。
- 在真实桌面分别记录 X11 客户区、GLFW framebuffer 和 GLX drawable。默认运行时 resize 后没有新日志，证明渲染循环已暂停；添加 `--no-fullscreen-pause` 后三层尺寸同时从 `640x480` 更新为 `800x600`，完整重绘。
- 根据该 A/B 结果，将全屏暂停限制到桌面背景模式，而不是继续修改 framebuffer 尺寸来源。

验证：完整构建和链接成功。在真实 GNOME X11 桌面使用不带 `--no-fullscreen-pause`、不带 `--disable-parallax` 的普通命令启动，将窗口从 `640x480` 自动调整为 `800x600` 后截图完整，比例正确且没有竖条或横条。

### 4.4 GNOME Wayland 不支持现有 Layer Shell 路径

状态：**架构限制**

问题：现有 Wayland 后端依赖 `wlr-layer-shell-unstable`。GNOME Mutter 不提供该协议，因此切换到 GNOME Wayland 并不能直接解决壁纸集成。

解决方向：

- 当前优先支持 GNOME X11。
- 如果未来支持 GNOME Wayland，需要 GNOME Shell 扩展、专用 Mutter 集成或其他 GNOME 可接受的嵌入机制，不能直接复用 wlroots Layer Shell 方案。

### 4.5 多显示器桌面窗口管理不足

状态：**待设计和验证**

问题：显式窗口模式当前只允许一个窗口。外部 GNOME X11 桥接脚本通常为每个显示器启动一个进程，增加 GPU 上下文、内存占用、进程管理和启动时序问题。

推荐方案：

- 第一阶段使用覆盖整个 X11 虚拟桌面的单个窗口，并为每个 XRandR 输出创建独立 viewport。
- 复用现有 `screenBackgrounds`、`screenScalings`、`screenClamps` 和 span group 配置。
- 如果不同刷新率或 GPU 驱动导致单窗口方案异常，再实现同一进程管理多个 GLFW 窗口，而不是多个独立进程。
- 监听 XRandR 输出变化，显示器热插拔后重新计算几何布局。

### 4.6 全屏暂停行为需要桌面实测

状态：**崩溃和预览窗口误暂停均已修复；桌面背景模式仍需后续验证**

本机现象：默认 `NORMAL_WINDOW` 模式启动后，X server 报告 `BadWindow (invalid Window parameter)`，失败请求为 `X_QueryTree`，程序随即退出。

已确认并修复的代码问题：

- `X11FullScreenDetector::anythingFullscreen()` 原先使用 `reinterpret_cast<Window>(glfwWindow)`，错误地把 GLFW 对象指针当成 X11 `Window` ID；现已改为通过 `glfwGetX11Window()` 获取原生 X11 窗口。
- 查询当前窗口的子节点后，代码原先在 `schildren` 非空时错误释放外层的 `children`，随后仍读取并再次释放 `children`；现已改为分别释放 `schildren` 和 `children`。
- 已增加 X display、根窗口、GLFW 窗口和原生 X11 窗口的空值检查；无法打开 X display 时禁用全屏检测，而不是继续解引用空指针。

新增确认的问题：全屏检测本来是桌面壁纸的节能功能，却也应用在 `NORMAL_WINDOW` 和 `EXPLICIT_WINDOW`。一旦检测到其他全屏窗口，预览窗口停止处理事件和绘制，resize 后出现未重绘条纹。

修复验证：普通和显式预览窗口现在使用无操作检测器，只有 `DESKTOP_BACKGROUND` 创建平台全屏检测器。已在真实桌面确认默认普通窗口不会再因其他窗口触发暂停，resize 能继续处理并完整重绘。

临时回退：如果未来桌面背景模式的全屏检测仍有问题，可以添加 `--no-fullscreen-pause`；普通预览窗口不再需要该参数。

建议方案：

- 分别测试 GNOME 原生应用、Steam/Proton 游戏、浏览器全屏和多显示器全屏。
- 增加调试日志，记录活动窗口、`_NET_WM_STATE_FULLSCREEN` 和暂停状态变化。
- 后续提供应用忽略列表，并将 Wayland 专用的过滤能力扩展到 X11。

### 4.7 混合显卡选择（已验证）

状态：**已验证**

环境同时包含 Intel 集成显卡（`8086:a780`）和 NVIDIA 独立显卡（RTX 3050，`10de:2582`）。已在真实 GNOME X11 桌面用 Scene 壁纸 `2955458015` 实测：

| 启动方式 | OpenGL 渲染器 | 结果 |
| --- | --- | --- |
| 默认（无环境变量） | `NVIDIA GeForce RTX 3050/PCIe/SSE2`，GL 3.3.0 NVIDIA 595.84 | ✅ Scene 渲染正常（`--screenshot` 输出 640x480 PNG，非黑屏，约 2.9 万种颜色） |
| `DRI_PRIME=1` | 仍为 NVIDIA RTX 3050 | ✅ 正常；此机默认即为 NVIDIA，非 PRIME offload 配置下 `DRI_PRIME` 不切换 |
| `__GLX_VENDOR_LIBRARY_NAME=mesa` | `llvmpipe`（LLVM 15.0.7，GL 4.5 Core，软件回退） | ⚠️ Intel 硬件 GLX（dri3）在当前 X 会话不可用，回退软件渲染；属环境 GLX 配置限制，非程序问题 |

结论：本机 X11 GLX 下硬件渲染统一走 NVIDIA，无厂商冲突；GPU 选择保留为启动环境配置，未硬编码厂商逻辑。已在 `GLFWOpenGLDriver` 上下文初始化后输出 `OpenGL renderer/vendor/version` 诊断日志，便于在其他机器确认 GPU 选择。

## 4.8 Alt+Tab 露出系统壁纸（已知限制）

状态：**已确认根因，暂不修复**

现象：当前使用 `_NET_WM_WINDOW_TYPE_DOCK` 类型。Alt+Tab 切换窗口期间 Shell compositor 可能短暂露出底层背景（DOCK 窗口虽不在 Alt+Tab 列表，但切换动画期间合成器行为受版本影响）。

历史注记：早期曾使用 `_NET_WM_WINDOW_TYPE_DESKTOP`，该类型在 GNOME 42 下会被 Shell 背景层完全盖住（黑屏），且 Alt+Tab 切换时被整体隐藏，已弃用（见 11.4 节）。

根因：这是 GNOME Shell compositor 的硬编码行为——切换模式下部分底层窗口被整体隐藏，应用进程无法通过 X11 窗口属性完全绕开。

可行修复方案：编写 GNOME Shell Extension，在 Shell 进程内部拦截切换事件，但技术代价大（JS 技术栈分离、Shell 版本耦合、额外部署步骤），当前优先推进核心桌面功能，后续再考虑用小扩展精细打磨。

## 5. 内容类型兼容性


### 5.1 壁纸切换时的渲染崩溃

状态：**已确认，待修复**

现象：循环模式下，部分壁纸（包含不支持的 puppet 格式 MDLV0016、Light objects、未知对象类型）在渲染帧中触发 SIGSEGV 崩溃。

根因：崩溃发生在渲染循环（RenderContext::render / CScene 着色器编译），不在 loadBackground 中。现有的 advancePlaylist try-catch 无法捕获 SIGSEGV。

修复方案（按推荐顺序）：

1. **上游渲染容错（推荐）**：在 `CObject`/`CPass`/着色器编译等路径中，对不支持的格式返回空占位对象而非崩溃。改动分散在 `src/WallpaperEngine/Render/` 多个文件，需要逐个类型处理。

2. **SIGSEGV 信号处理器**：注册 SIGSEGV handler，用 sigsetjmp/siglongjmp 回到主循环，标记当前壁纸失败并跳过。实现约 50 行但需谨慎处理 OpenGL 状态恢复。

3. **预扫描过滤**：在 `listWorkshopWallpapers()` 中解析 project.json 的 objects/effects，预判是否包含不支持特性，提前过滤。需要维护黑名单且可能误杀。

### Scene

- 当前本机有 17 个 Scene 项目，可作为主要回归测试集。
- 基础图层、粒子、材质和常见 Shader 需要逐个验证。
- 复杂 Shader、SceneScript、音频响应和鼠标交互可能仍存在上游未实现能力。

### Video

- 当前本机有 5 个 Video 项目。
- 依赖 MPV、FFmpeg 和对应硬件解码能力。
- 应验证循环、音量、静音、暂停恢复、硬件解码和多显示器行为。

### Web

- 当前本机有 1 个 Web 项目。
- 构建时会下载并打包 CEF。
- 首次本机测试只有空白画面。根因是 CEF `OnPaint()` 调用 `glBindTexture()` 时传入了 wallpaper framebuffer ID，而不是其颜色纹理 ID，导致浏览器像素无法上传到最终采样的纹理。
- 已将 `RenderHandler::texture()` 改为返回 `getWallpaperTexture()`，完整构建和链接成功。
- 修复后的桌面复测仍未映射 GLFW 窗口，进程停留在输出初始化之前，也没有创建可见的 CEF 子进程。这表明纹理 ID 错误之外还存在独立的 CEF 初始化问题；必须先解决初始化，才能验证 `OnPaint`、本地资源加载、Wallpaper Engine JavaScript API、音频、鼠标输入、resize 和 GPU 加速。

## 6. 安装和构建方面的其他风险

### 6.1 CEF 在配置阶段联网下载

状态：**现有行为**

CMake 会从 CEF 分发服务器下载指定版本并使用 SHA-1 校验。首次配置需要网络、额外磁盘空间和较长时间。

建议方案：

- 在 README 中明确说明首次配置会联网下载 CEF。
- CI 中缓存 CEF 文件。
- 后续考虑提供可重复构建说明或已校验的缓存机制。

### 6.2 自动测试缺失

状态：**部分完成（纯单元测试）**

已完成并接入 `ctest`（`include(CTest)` + `catch_discover_tests`）：

1. `ColorBuilder` 测试：`#RGB`、`#RGBA`、`#RRGGBB`、`#RRGGBBAA` 解析 + alpha 参数 + 非法格式异常（修复过程中还发现并修正了 6 位颜色丢红色、8 位 hex 超出 `int` 溢出的上游 bug）。
2. MediaSource 头文件独立编译测试（`MediaSourceHeaders.cpp`），防止再次依赖传递包含。
3. 命令行参数解析测试（`CommandLineParsing.cpp`）：窗口/GNOME X11/互斥/`--fps`/`--cycle`/`--config` 合并。
4. `ControlServer` 命令队列测试（`ControlServerQueue.cpp`）：验证网络线程只入队、主线程消费、按序取出。

当前 `ctest` 共 32 个用例全部通过。仍建议后续补充：

1. 基于 Xvfb 的 X11 窗口属性集成测试。
2. 选定 Scene 的截图基线测试；允许为不同 GPU 设置合理容差。

## 7. 外部 GNOME X11 桥接脚本的问题

已审查的第三方 GNOME X11 桥接脚本可以证明“置底窗口”方案可行，但不适合作为最终实现，原因包括：

- 使用 `pkill -9 -f linux-wallpaperengine`，可能误杀无关进程，且不给程序清理资源的机会。
- 使用可执行 Shell 配置并通过 `source` 加载，路径转义不足。
- 将 Workshop 属性写入自启动 Shell 命令，存在命令注入风险。
- 使用窗口标题搜索，可能误匹配或因窗口创建时序而失败。
- 多显示器启动多个进程。
- 覆盖自启动和用户脚本时缺少备份与卸载机制。

解决方案是将必要的 X11 窗口属性操作放入 C++ 后端，并使用结构化配置及明确的进程生命周期管理，而不是继续扩展外部 Shell 桥接层。

## 8. GNOME X11 原生桌面窗口实施顺序

### 8.1 实现原则和范围

目标层级：

```text
GNOME 原生背景 < linux-wallpaperengine 桌面窗口 < 普通应用窗口
```

第一版只支持 GNOME X11，不尝试解决 GNOME Wayland。实现必须遵守以下原则：

- 不再写入 X11 根窗口 Pixmap。
- 不依赖 `wmctrl`、`xdotool` 或窗口标题搜索。
- 不使用 `override_redirect` 绕过 Mutter。
- 使用一个受 Mutter 管理的 GLFW/X11 窗口。
- 第一版不设置 `_NET_WM_WINDOW_TYPE_DESKTOP`，避免窗口再次落到 GNOME 自己的背景层后面。
- 单个进程负责所有显示器，不为每个显示器启动独立进程。
- 预览模式、固定几何窗口模式和桌面窗口模式保持独立。
- 暂停渲染不能停止 X11/GLFW 事件处理。

### 8.2 步骤 1：新增独立模式和命令行入口

新增窗口模式：

```cpp
GNOME_X11_DESKTOP_WINDOW
```

新增参数：

```text
--gnome-x11
```

预计修改范围：

- `ApplicationContext.h/.cpp`：增加枚举、参数解析和互斥校验。
- `VideoFactories.cpp`：为新模式注册 GLFW/OpenGL driver。
- 帮助文本和 README：说明仅支持 X11；在 Wayland 会话中给出明确错误。

示例命令：

```bash
linux-wallpaperengine \
  --assets-dir ~/.local/share/Steam/steamapps/common/wallpaper_engine/assets \
  --gnome-x11 \
  --fps 30 \
  --silent \
  2955458015
```

验收标准：新模式可以独立解析和进入专用输出路径，不改变 `NORMAL_WINDOW`、`EXPLICIT_WINDOW` 和现有 `DESKTOP_BACKGROUND` 的行为。

### 8.3 步骤 2：拆分事件处理和渲染暂停 ✅

状态：**已实现**

已从 `dispatchEventQueue()` 中提取 `pumpEvents()` 纯虚方法到 `VideoDriver` 基类。暂停时调用 `pumpEvents()` 确保窗口响应，只跳过渲染和 buffer 交换。

当前 `dispatchEventQueue()` 同时承担事件处理、绘制和交换缓冲。桌面模式实现前应先拆分为类似流程：

```cpp
pumpEvents();
updateWindowState();

if (!paused) {
    renderFrame();
    presentFrame();
}
```

即使处于全屏暂停、最小化或零尺寸状态，也必须继续处理：

- GLFW/X11 事件。
- 退出请求和信号。
- XRandR 显示器变化。
- 窗口位置、尺寸及层级恢复。

验收标准：人为暂停渲染后，窗口仍可移动、恢复、退出，显示器事件不会堆积；恢复后首帧完整，不出现旧 back buffer 条纹。

### 8.4 步骤 3：实现单显示器 GNOME X11 输出类 ✅

状态：**已实现**

新增 `GNOMEX11WindowOutput` 类，通过 Xlib/EWMH 设置：
- `_NET_WM_STATE_BELOW` / `_NET_WM_STATE_STICKY` / `_NET_WM_STATE_SKIP_TASKBAR` / `_NET_WM_STATE_SKIP_PAGER`
- `_NET_WM_DESKTOP = 0xFFFFFFFF`
- `WM_HINTS.input = False`
- 窗口几何覆盖 XRandR 活动输出的包围盒

建议新增独立输出类，例如：

```text
GNOMEX11WindowOutput
```

窗口创建要求：

- 无边框、不可调整大小、非 floating。
- 创建时隐藏，完成 X11 属性配置后再显示。
- `GLFW_FOCUS_ON_SHOW = false`。
- 设置唯一 `WM_CLASS=linux-wallpaperengine-desktop`。

通过 Xlib/EWMH 设置：

```text
_NET_WM_STATE_BELOW
_NET_WM_STATE_STICKY
_NET_WM_STATE_SKIP_TASKBAR
_NET_WM_STATE_SKIP_PAGER
_NET_WM_DESKTOP = 0xFFFFFFFF
```

同时设置 `WM_HINTS.input = false`，并调用 `XLowerWindow()`。

窗口类型采用 **`_NET_WM_WINDOW_TYPE_DOCK`**（2026-08-05 定案，见 11.4 节）：DOCK 窗口可见于 GNOME 背景之上、免疫 Show Desktop（Win+D 不消失），配合 `_NET_WM_STATE_BELOW` 和 `XLowerWindow` 保持在普通应用之下。不采用 `_NET_WM_WINDOW_TYPE_DESKTOP`（被背景层盖住导致黑屏，已实验确认无法突破）。

第一版先使用当前活动显示器或 XRandR 虚拟桌面边界作为窗口几何，不处理热插拔。

验收标准：窗口显示在 GNOME 背景之上、普通应用之下；不出现在任务栏、分页器和 Alt+Tab；显示时不抢焦点；Win+D 后壁纸保持显示。

### 8.5 步骤 4：点击穿透和全局鼠标位置

桌面窗口默认应点击穿透，避免拦截桌面图标、右键菜单和其他桌面交互。建议通过 XFixes/Shape 将窗口输入区域设为空。

点击穿透后，视差仍可通过 `XQueryPointer()` 获取全局鼠标位置，不需要窗口接收点击事件。

第一版行为：

- 默认点击穿透。
- 支持全局鼠标位置和 Scene 视差。
- 不承诺 Scene/Web 点击交互。

后续可增加：

```text
--interactive
```

该选项允许窗口接收点击，但必须明确提示可能遮挡桌面图标。若未来需要“观察点击但不阻止下层窗口”，再研究 XI2 旁路事件，不在第一版实现。

验收标准：桌面图标和右键菜单正常；壁纸窗口不获取键盘焦点；视差不会因虚拟桌面坐标产生跳变。

### 8.6 步骤 5：单显示器桌面验收

完成以上代码后，先冻结功能范围并执行单显示器测试：

1. 登录后前台启动和干净退出。
2. 打开、最小化、最大化和切换普通应用。
3. 切换 GNOME 工作区。
4. 打开 Activities Overview。
5. 使用桌面图标和右键菜单。
6. 锁屏、解锁及注销前退出。
7. 运行 Scene 和 Video；Web 暂列为低优先级。

验收标准：层级始终正确，不抢焦点、不出现在窗口列表、不遮挡桌面交互，`Ctrl+C` 后 GNOME 原背景自然露出且无残留窗口。

### 8.7 步骤 6：单窗口多显示器 viewport

通过 XRandR 获取全部活动输出并计算虚拟桌面包围盒：

```text
minX, minY, maxX, maxY
```

桌面窗口几何：

```text
(minX, minY, maxX - minX, maxY - minY)
```

每个 XRandR 输出对应窗口内一个 viewport，坐标转换为相对包围盒位置。复用现有：

- `screenBackgrounds`
- `screenScalings`
- `screenClamps`
- `spanGroups`

优先使用单个 GLFW/OpenGL context。如果不同刷新率、混合显卡或驱动行为证明单窗口不可行，再考虑同一进程管理多个窗口；不退回多进程方案。

验收标准：支持每屏独立壁纸和跨屏壁纸，显示器位于负坐标或上下排列时 viewport 仍正确。

### 8.8 步骤 7：XRandR 热插拔和布局变化

监听：

```text
RRScreenChangeNotify
RROutputChangeNotify
RRCrtcChangeNotify
```

收到变化后：

1. 保持事件循环运行并暂缓绘制。
2. 重新读取活动输出及包围盒。
3. 调整桌面窗口位置和尺寸。
4. 重建 viewport 映射。
5. 检查 framebuffer 尺寸有效后恢复绘制。

验收标准：连接、断开显示器及改变排列后无需重启；不会出现零尺寸计算、旧 framebuffer 条纹或错误的跨屏裁切。

### 8.9 步骤 8：桌面模式全屏暂停

全屏暂停只应用于 `GNOME_X11_DESKTOP_WINDOW` 和需要保留的旧桌面背景模式，绝不应用于预览窗口。

检测策略应从“窗口几何等于显示器”逐步改为读取 EWMH `_NET_WM_STATE_FULLSCREEN`，并测试：

- GNOME 原生应用。
- 浏览器全屏。
- Steam/Proton 游戏。
- 无边框全屏。
- 多显示器仅一屏全屏。

暂停时只停止场景更新、视频播放和 OpenGL 绘制，事件循环、XRandR 和退出处理必须继续运行。

验收标准：全屏应用出现时壁纸降低资源占用；退出全屏后自动恢复，首帧完整；不会因最大化窗口、桌面外框或其他特殊窗口误暂停。

### 8.10 步骤 9：安全的用户服务和配置

状态：**已实现**

- ✅ `--config <path>`：程序支持从 JSON 读取启动参数（键 → CLI 参数，显式 CLI 覆盖），见 `ApplicationContext::loadConfigFileArguments`。
- ✅ systemd 用户服务：`packaging/linux/linux-wallpaperengine.service`。
- ✅ 管理脚本：`packaging/linux/lwe` 提供 `install|start|stop|restart|status|enable|disable|uninstall`，全程 `systemctl --user`，不使用 `pkill`。
- ✅ 示例配置：`packaging/linux/config.example.json`，安装时复制到 `~/.config/linux-wallpaperengine/config.json`。
- ✅ README 已补充 systemd 用法；卸载（`lwe uninstall`）会停服务并移除 unit。
- ✅ 开发模式：`LWE_BIN=$PWD/build/output/linux-wallpaperengine LWE_SHARE_DIR=$PWD/packaging/linux lwe install` 可无需 sudo 直接指向 build 目录跑服务；注意后续重新构建会覆盖该二进制，正式使用应切到 `/opt`（详见 README）。

仍为前台运行的替代方式：`./linux-wallpaperengine --config ~/.config/linux-wallpaperengine/config.json`。

### 8.11 步骤 10：GUI 和壁纸管理

最后再实现 Workshop 扫描、预览、显示器映射、FPS、音量和属性配置。GUI 只负责结构化配置和控制服务，不承载渲染生命周期。对尚未解决的 Web/CEF 支持给出明确状态，不阻塞 Scene 和 Video 桌面模式发布。

## 9. 当前功能状态

| 功能 | 状态 | 说明 |
|------|:---:|------|
| GNOME X11 桌面窗口 | ✅ | 全屏、置底、不抢焦点、Alt+Tab不出现在列表 |
| Win+D 不消失 | ✅ | `_NET_WM_WINDOW_TYPE_DOCK` + `_NET_WM_STATE_BELOW`（详见 11 节） |
| 壁纸循环切换 | ✅ | `--cycle` 每 `--cycle-interval` 秒准时切换，Win+D 期间不冻结 |
| 点击穿透 | ✅ | XShape 空输入区域，桌面右键正常 |
| 循环播放 | ✅ | `--cycle` + `--cycle-interval`，自动扫描 Workshop |
| 崩溃黑名单 | ✅ | 记录到 `/tmp/lwe-failed`，重启自动跳过（崩溃根因已修复，作防御保留） |
| 全屏暂停 | ✅ | 跳过 `override_redirect` 窗口（mutter guard window）修复误判 |
| Web 壁纸 | ⚠️ | CEF 初始化卡死，`--cycle` 自动跳过；显式指定仍卡（见 11.6.3） |
| puppet 模型 | ⚠️ | 仅支持 MDLV0021/0023，Sea Train 挂环等 MDLV0013 渲染不完整（见 11.8） |
| Alt+Tab 露原壁纸 | ⚠️ | GNOME compositor 限制，需 Shell Extension |
| 视差效果 | ⚠️ | `XQueryPointer` 已就绪，需有 parallax 的壁纸验证 |
| 托盘控制面板 | ✅ | `ControlServer` 改为命令队列，主线程消费，消除后台线程数据竞争 |
| 混合显卡 | ✅ | X11 GLX 下硬件渲染统一走 NVIDIA（见 4.7），默认渲染器有诊断日志 |

### 4.9 崩溃重启后线程/实例堆积（已加单实例锁）

状态：**已定位并加固**

现象：htop 中看到大量 linux-wallpaperengine 线程，怀疑崩溃重启后旧线程未销毁、内存暴涨。

排查结论：

- 单个实例本身就有约 28 个线程（主渲染线程 + `ControlServer` socket 线程 + SDL/音频回调 + FFmpeg 解码 + GLFW 等），这是**单实例的正常线程数**；`--cycle` 连续切换 18 次后线程数**保持 28 不变**，`ControlServer`/`AudioStream` 均有 join/wait 清理，**壁纸切换本身不泄漏**。
- 进程一旦死亡（SIGSEGV/`kill`），内核必然回收其全部线程与内存。"旧线程不销毁"的实质是**多个实例同时共存**（崩溃后启动新实例而旧实例未退出/未停止，或 systemd 重启与手动启动并存），每个实例各自再带一批线程与数百 MB 内存。

加固：`main.cpp` 增加**单实例锁**（`XDG_RUNTIME_DIR/linux-wallpaperengine.lock` 上的 `flock`，在 `loadSettingsFromArgv` 后获取，`--help`/`--list-properties` 不受影响）。第二个实例启动即被拒绝并提示用 `systemctl --user stop linux-wallpaperengine` 停止旧实例；`flock` 随进程退出由内核自动释放，崩溃后重启不受影响。

验证：双实例启动被拒；`kill -9` 后立即可重启；操作后无残留实例。

## 10. 下一步建议

Scene、Video、窗口比例、运行时 resize、受控退出、预览窗口全屏误暂停和 `--cycle` 切换已经完成本机验证；Web/CEF 初始化仍有问题，但用户使用频率低，不作为 GNOME X11 桌面模式的前置条件。长时间资源占用和更多全屏组合测试暂后置。

已完成步骤 1-4 和桌面集成修复后，后续按顺序推进：

1. **步骤 5**：单显示器桌面验收（层级、焦点、锁屏、Ctrl+C 干净退出）。
2. **步骤 6**：单窗口多显示器 viewport（XRandR 包围盒 + 每屏独立壁纸/span）。
3. **步骤 7**：XRandR 热插拔（`RRScreenChangeNotify` / `RROutputChangeNotify`）。
4. **步骤 8**：桌面模式全屏暂停优化（读取 `_NET_WM_STATE_FULLSCREEN` 而非几何匹配）。
5. **步骤 9**：systemd 用户服务 + 结构化配置（JSON/INI），不用 `pkill` 管理生命周期。
6. **步骤 10**：GUI 和壁纸管理；Web/CEF 支持给出明确状态。

## 11. 2026-08-05 桌面集成修复记录

本轮在真实 GNOME 42 X11 桌面完成 `--cycle` 循环模式的完整实测，修复了以下问题：

### 11.1 `--cycle` 启动崩溃（filesystem error）

- **现象**：`--gnome-x11 --cycle` 启动即报 `The specified mount cannot be handled by any of the filesystem adapters`。
- **根因**：`WallpaperApplication` 构造函数先执行 `loadBackgrounds()` 后执行 `initializePlaylists()`。`--cycle` 未指定壁纸时 `defaultBackground` 为空，`setupAssetLocator("")` 尝试挂载空路径失败。
- **修复**：将 `initializePlaylists()` 提前到 `loadBackgrounds()` 之前，cycle 模式先扫描 Workshop 并设置 `defaultBackground`，随后加载正常。

### 11.2 首帧拖影

- **现象**：壁纸窗口下方出现未初始化拖影。
- **根因**：`configureDesktopWindow()` 用 `XMoveResizeWindow` 把 640x480 的 GLFW 窗口直接拉到桌面尺寸，但 GLFW 未收到 `ConfigureNotify`，framebuffer 仍是 640x480，`glClear` 只清除了左上角。
- **修复**：`XFlush` 后调用 `glfwPollEvents()`，让 GLFW 在首帧渲染前同步 framebuffer 尺寸。

### 11.3 壁纸不切换 + 动态壁纸不动的根因：mutter guard window 误判全屏

- **现象**：循环模式下壁纸既不切换、画面也不动。
- **根因**：Mutter 合成器有一个全屏守护窗口 `"mutter guard window"`（`override_redirect=1`，尺寸等于屏幕），`X11FullScreenDetector::anythingFullscreen()` 把它判为"全屏应用"，壁纸永久进入暂停分支（mpv 暂停 + `updatePlaylists()` 不执行）。
- **修复**：`anythingFullscreen()` 跳过所有 `override_redirect` 窗口（合成器/OSD 特征；真正的全屏应用受 WM 管理，不会是 override_redirect），同时保留对 `_NET_WM_WINDOW_TYPE_DESKTOP` / `DOCK` 的跳过。

### 11.4 Win+D 壁纸消失：DESKTOP 类型两难与 DOCK 类型方案

- **现象**：按 Win+D 后壁纸窗口消失，且切换严重延迟（5 秒间隔变成 15 秒+）。
- **根因**：GNOME 42 的 Show Desktop 会最小化所有非 DOCK/DESKTOP 窗口。`_NET_WM_WINDOW_TYPE_BELOW` 的普通窗口被持续 unmap，`ensureVisible()` 每帧恢复又被打回，窗口在 map/unmap 间抖动、framebuffer 为 0，渲染冻结。
- **窗口类型穷举实验**（Xlib 实测采样根窗口像素）：
  | 类型 | 可见性 | Win+D | 结论 |
  |------|--------|-------|------|
  | `_NET_WM_WINDOW_TYPE_DESKTOP` | ❌ 被 GNOME 背景层完全盖住（黑屏，XMapRaised/XRaiseWindow 均无法突破） | ✅ 不消失 | 不可行 |
  | `_NET_WM_WINDOW_TYPE_BELOW`（普通） | ✅ 可见 | ❌ 被最小化 + 渲染冻结 | 不可行 |
  | **`_NET_WM_WINDOW_TYPE_DOCK` + `BELOW`** | ✅ 可见 | ✅ **不消失** | ✅ **采用** |
- **验证**：DOCK 窗口位于 `_NET_CLIENT_LIST_STACKING` 底部（普通应用之下，不遮挡 Clash/终端），可见且免疫 Show Desktop。Win+D 后窗口 `map_state=2` 保持 viewable，切换每 5 秒准时。
- **修复**：`setupEWMHProperties()` 设置 `_NET_WM_WINDOW_TYPE_DOCK`，配合已有的 `_NET_WM_STATE_BELOW/STICKY/SKIP_TASKBAR/SKIP_PAGER` 与 `XLowerWindow`。

### 11.5 GNOME X11 模式缩放默认值

- **现象**：`--cycle` 提前初始化后 `screenBackgrounds` 非空，GNOME 模式原有的 fit+border 默认缩放被跳过，壁纸用 `DefaultUVs` 渲染。
- **修复**：GNOME X11 模式下对任何未显式指定缩放/clamp 的屏幕强制 `ZoomFitUVs + ClampUVsBorder`。

### 11.6 运行期崩溃修复（2026-08-05 晚）

`--cycle` 长跑（random 顺序、5-10 秒间隔）暴露了 3 个独立故障，均已定位并修复。

**11.6.1 ScriptEngine 悬垂 album-art 监听器（SIGSEGV）**

- **现象**：运行数分钟后进程 SIGSEGV。gdb 堆栈：`ScriptEngine::notifyMediaUpdate → VectorAdapter<3>::instantiate → ObjectAdapter::instantiate`，调用链从 `render() → DBusMediaSource::update() → fireAlbumArtListeners()`。
- **根因**：`ScriptEngine` 构造函数注册两个监听器（metadata + album-art），析构只移除了 metadata 监听器，**漏掉 `m_unregisterAlbumArtUpdateCallback()`**。`ScriptEngine` 由 `CScene` 持有，每次切换壁纸（销毁旧 CScene）都会留下一个悬垂的 album-art 监听器。系统音乐/媒体信息变化（MPRIS）触发 `fireAlbumArtListeners()` 时调用悬垂 `this` → 崩溃。累积多个悬垂监听器后崩溃概率随切换次数增加。
- **修复**：`ScriptEngine::~ScriptEngine()` 补调 `this->m_unregisterAlbumArtUpdateCallback ()`。

**11.6.2 AlbumTexture::copyContents 空 GL 函数指针（SIGSEGV）**

- **现象**：同上运行期 SIGSEGV，堆栈 `AlbumTexture::copyContents → TextureCache 监听器 lambda → fireAlbumArtListeners`。
- **根因**：`copyContents()` 直接调用 `glGetnTexImage`（GL 4.3+ API），但 GLFW 请求的是 **GL 3.3 core** context，GLEW 不会加载该函数指针（null）。媒体信息更新触发专辑封面复制时调用空指针崩溃。项目其它 `glReadnPixels` 用法都有 `GLEW_VERSION_4_5` 检查并 fallback，唯独这里漏了。
- **修复**：改用 GL 3.3 可用的 `glGetTexImage`（bufferSize 恰好为 RGBA 数据量）。

**11.6.3 Web 壁纸触发 CEF 初始化卡死（死锁）**

- **现象**：`--cycle` 切换到 Web 壁纸（如 1082586397）时主线程卡死。逐步加日志定位到 `ProjectParser::parse` 正常完成后 `setupBrowser()` 的 CEF 初始化，gdb 堆栈显示卡在动态链接器 `dlopen`（加载 libcef.so）。这是文档 5 节已知的 CEF 初始化问题。
- **修复**：`Steam::FileSystem::listWorkshopWallpapers()` 跳过 `type == "web"` 的壁纸（日志 `Skipping web wallpaper ... (CEF not functional)`），`--cycle` 不再加载 Web 壁纸。显式指定 Web 壁纸仍会卡在 CEF 初始化（已知限制）。
- 验证：过滤后 22 个壁纸，200 秒切换 39 次全部准时、无卡住、无崩溃。

### 11.7 音频并发修复（防御性）

- **`dequeuePacket` 无限等待**：`SDL audio_callback` 线程持 `m_streamListMutex` 遍历流时调用 `decodeFrame → dequeuePacket`，队列空时 `SDL_CondWait` 无限阻塞，主线程 `removeStream` 等同一把锁 → 死锁。改为**非阻塞**：队列空时 unref 残留包并返回，音频回调对无数据流直接跳过（填充静音），避免长时间持锁。
- **`static int audio_pkt_size`**：`decodeFrame` 里是 static 局部变量，所有 `AudioStream` 实例共享 → 数据竞争/逻辑错误。改为成员变量 `m_audioPktSize`。
- **`~AudioStream` 的 `SDL_CondWait`**：原实现未先 lock mutex 且无限等待，改为 `SDL_LockMutex` + `SDL_CondWaitTimeout(…, 100)`。
- **`SDLAudioDriver::removeStream`**：原来无锁直接 `m_streams.erase()`，与音频回调遍历竞争；改为持 `m_streamListMutex`。

### 11.8 挂环错位：puppet 模型格式限制（已知）

- **现象**：Sea Train（3029316591）壁纸中扶手挂环（Handles）渲染不完整、部分出现在半空。
- **根因**：挂环是 puppet 骨骼动画模型（`Handles 1/2_puppet.mdl`），格式为 `MDLV0013`。`CImage::loadPuppetMesh` 只支持 `MDLV0021/MDLV0023`，对 `MDLV0013` 打印 `Unsupported puppet model header` 并返回 false → 挂环网格不加载，回退基本四边形；叠加 `autosize: true`（上游未实现）与部分 shadow 对象负坐标 origin → 挂环不完整/悬空。
- **状态**：上游 puppet 渲染支持限制，需逆向 `MDLV0013` 格式（大工程），暂不修复。

### 11.9 实测环境（更新）

- 桌面：GNOME Shell 42.9（X11），单显示器 2560x1440。
- `--cycle` 扫描到 22 个 Workshop 壁纸（过滤 Web 后），5 秒间隔下 200 秒切换 39 次准时、进程存活、无崩溃、无卡死。
- 遗留：清理残留实例时发现多个 `--cycle` 测试进程可能残留，需用 `pkill -9 -f "output/linux-wallpaperengine"` 或按 PID 精确清理。
