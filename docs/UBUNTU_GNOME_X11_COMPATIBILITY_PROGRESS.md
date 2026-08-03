# Ubuntu 22.04 + GNOME X11 兼容性进度

最后更新：2026-08-03

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

状态：**已发现，尚未修复；不阻止构建**

现象：GCC 输出：

```text
cc1plus: note: unrecognized command-line option '-Wno-undefined-var-template'
```

影响：当前只是提示，不会导致构建失败，但会污染日志并掩盖真正警告。

建议方案：在 CMake 中根据 `CMAKE_CXX_COMPILER_ID` 添加编译器专用选项，或使用 `CheckCXXCompilerFlag` 检查后再加入该参数。

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

### 4.7 混合显卡选择尚未验证

状态：**待验证**

环境同时包含 Intel 集成显卡和 NVIDIA 独立显卡。当前尚未确认 GLFW/OpenGL 默认选择的 GPU，也未测量 Scene、Video 和 Web 壁纸在两块 GPU 上的资源占用。

建议方案：

- 先记录默认渲染器和 OpenGL 版本。
- 分别测试默认 GPU、`DRI_PRIME` 和 NVIDIA PRIME Render Offload。
- 将 GPU 选择保留为启动环境配置，不在第一阶段硬编码厂商逻辑。

## 5. 内容类型兼容性

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
- 应验证本地资源加载、Wallpaper Engine JavaScript API、音频、鼠标输入和 GPU 加速。

## 6. 安装和构建方面的其他风险

### 6.1 CEF 在配置阶段联网下载

状态：**现有行为**

CMake 会从 CEF 分发服务器下载指定版本并使用 SHA-1 校验。首次配置需要网络、额外磁盘空间和较长时间。

建议方案：

- 在 README 中明确说明首次配置会联网下载 CEF。
- CI 中缓存 CEF 文件。
- 后续考虑提供可重复构建说明或已校验的缓存机制。

### 6.2 自动测试缺失

状态：**已确认**

执行 `ctest --test-dir build` 显示没有注册测试。当前只能证明项目编译、链接和命令行解析成功，不能自动证明颜色解析、媒体元数据、渲染结果或桌面行为正确。

建议优先补充：

1. `ColorBuilder` 的 `#RGB`、`#RGBA`、`#RRGGBB`、`#RRGGBBAA` 单元测试。
2. MediaSource 头文件独立编译测试，防止再次依赖传递包含。
3. 命令行参数解析测试。
4. 基于 Xvfb 的 X11 窗口属性测试。
5. 选定 Scene 的截图基线测试；允许为不同 GPU 设置合理容差。

## 7. 外部 GNOME X11 桥接脚本的问题

已审查的第三方 GNOME X11 桥接脚本可以证明“置底窗口”方案可行，但不适合作为最终实现，原因包括：

- 使用 `pkill -9 -f linux-wallpaperengine`，可能误杀无关进程，且不给程序清理资源的机会。
- 使用可执行 Shell 配置并通过 `source` 加载，路径转义不足。
- 将 Workshop 属性写入自启动 Shell 命令，存在命令注入风险。
- 使用窗口标题搜索，可能误匹配或因窗口创建时序而失败。
- 多显示器启动多个进程。
- 覆盖自启动和用户脚本时缺少备份与卸载机制。

解决方案是将必要的 X11 窗口属性操作放入 C++ 后端，并使用结构化配置及明确的进程生命周期管理，而不是继续扩展外部 Shell 桥接层。

## 8. 推荐开发阶段

### 阶段 0：本机运行基线

状态：**进行中；Scene 显式窗口已通过**

- 已在真实桌面终端运行 Scene 窗口预览，显示和受控退出正常。
- 继续对 Video 和 Web 各选择至少一个项目。
- 记录 OpenGL 渲染器、控制台日志、CPU/GPU/内存占用。
- 验证音频、鼠标、暂停和退出清理。

### 阶段 1：GNOME X11 单显示器桌面窗口

优先级：**最高**

- 新增独立桌面窗口模式。
- 在 C++ 中设置 EWMH 窗口状态。
- 禁止抢占焦点，隐藏任务栏和分页器入口。
- 支持干净退出，不依赖外部进程搜索和强制终止。

验收标准：注销/登录、切换工作区、打开 Overview、显示桌面图标和普通窗口时，壁纸层级均正确。

### 阶段 2：多显示器、热插拔与全屏暂停

- 支持每屏不同壁纸和跨屏壁纸。
- 监听 XRandR 输出变化。
- 验证不同缩放、排列和全屏组合。
- 验证 Intel/NVIDIA 混合显卡。

### 阶段 3：安全的用户服务和配置

- 使用 JSON/INI 等非可执行格式保存配置。
- 使用 systemd 用户服务或受控的 XDG autostart。
- 通过 PID/服务管理停止实例，不使用广泛 `pkill`。
- 提供日志、状态、重启和卸载方法。

### 阶段 4：GUI 与壁纸管理

- 扫描 Steam Workshop 目录并显示标题、预览和类型。
- 配置显示器、缩放、FPS、音量和属性。
- 对不支持的内容类型给出明确提示。
- GUI 只负责配置和控制，不承载渲染生命周期。

## 9. 下一步建议

下一步先不要继续扩大代码改动，应在真实桌面会话完成阶段 0，建立可重复的运行基线。确认窗口渲染、内容加载和混合显卡行为正常后，再开始 GNOME X11 桌面窗口后端；否则桌面集成问题可能与渲染器或驱动问题混在一起，增加调试成本。

建议首个测试命令：

```bash
cd ~/linux-wallpaperengine
./build/output/linux-wallpaperengine \
  --assets-dir ~/.local/share/Steam/steamapps/common/wallpaper_engine/assets \
  --window 0x0x1280x720 \
  --fps 30 \
  --silent \
  2955458015
```

需要可拖动、可缩放的普通测试窗口时，去掉 `--window`：

```bash
cd ~/linux-wallpaperengine
./build/output/linux-wallpaperengine \
  --assets-dir ~/.local/share/Steam/steamapps/common/wallpaper_engine/assets \
  --fps 30 \
  --silent \
  2955458015
```

运行后应记录：是否显示、是否正确播放、退出是否干净、终端日志、`glxinfo -B` 输出以及 `nvidia-smi`/系统监视器中的 GPU 占用。
