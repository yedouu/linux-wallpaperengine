# 技术架构文档

最后更新：2026-08-04

## 1. 项目概述

`linux-wallpaperengine` 是一个在 Linux 上运行 Steam Wallpaper Engine 动态壁纸的 OpenGL 渲染器。它解析 Wallpaper Engine 的 `project.json`，加载场景（Scene）、视频（Video）和网页（Web）壁纸资源，并通过 OpenGL 渲染到桌面或预览窗口。

## 2. 源码目录结构

```
src/
├── main.cpp                          # 入口
├── recording.cpp/.h                  # 录制（Demo 模式）
├── WallpaperEngine/                  # 核心引擎
│   ├── Application/                  # 应用上下文、状态、主循环
│   │   ├── ApplicationContext.h/.cpp  # CLI 解析、模式枚举、配置
│   │   ├── ApplicationState.h        # 运行时状态
│   │   └── WallpaperApplication.h/.cpp # 主循环、子系统和背景管理
│   ├── Assets/                       # 资源定位（VFS 挂载）
│   ├── Audio/                        # 音频处理（PulseAudio + SDL）
│   ├── Data/                         # JSON 解析、项目解析器、数据模型
│   ├── Debugging/                    # OpenGL 调用栈
│   ├── FileSystem/                   # 虚拟文件系统（Container + VFS）
│   ├── Input/                        # 鼠标输入（X11/Wayland）
│   ├── Logging/                      # 日志系统
│   ├── Maths/                        # 数学工具
│   ├── Media/                        # D-Bus 媒体源（MPRIS 播放信息）
│   ├── Render/                       # 渲染核心
│   │   ├── Drivers/                  # 平台驱动（GLFW/Wayland OpenGL）
│   │   │   ├── Output/               # 输出目标（窗口/桌面）
│   │   │   └── Detectors/            # 全屏检测器
│   │   ├── Wallpapers/               # 壁纸类型（CScene/CVideo/CWeb）
│   │   ├── Objects/                  # 渲染对象（Image/Text/Sound/Particle）
│   │   └── Shaders/                  # GLSL 着色器
│   ├── Scripting/                    # JavaScript 引擎（QuickJS）
│   ├── VideoPlayback/                # 视频播放（MPV + OpenGL）
│   └── WebBrowser/                   # Web 渲染（CEF）
├── Steam/                            # Steam 文件系统接口
│   └── FileSystem/                   # 查找 Steam 安装和 Workshop 目录
└── External/                         # 第三方库
    ├── quickjs/                      # QuickJS ES2020 引擎
    ├── argparse/                     # CLI 参数解析
    ├── json/                         # nlohmann/json
    ├── kissfft/                      # FFT 库
    ├── glslang-WallpaperEngine/      # GLSL 编译器
    ├── SPIRV-Cross-WallpaperEngine/  # SPIR-V 交叉编译
    └── stb/                          # stb_image/image_write
```

## 3. 核心模块关系

```
main.cpp
  └─ WallpaperApplication
       ├── ApplicationContext      # CLI 参数 → 配置
       ├── VideoDriver (平台驱动)   # GLFW/Wayland OpenGL
       │    ├── Output             # GLFWWindowOutput / X11Output
       │    ├── Detectors          # FullScreenDetector (X11/Wayland)
       │    └── InputContext       # MouseInput
       ├── RenderContext           # 渲染管线
       │    ├── CFBO               # OpenGL 帧缓冲对象管理
       │    ├── CWallpaper         # 壁纸实例（Scene/Video/Web）
       │    ├── CObject            # 渲染对象
       │    ├── CTexture           # 纹理
       │    └── Camera             # 相机/视差
       ├── AudioContext            # 音频播放和 FFT 处理
       ├── MediaSource             # D-Bus MPRIS 媒体信息
       ├── WebBrowserContext       # CEF Web 渲染
       └── ScriptEngine            # QuickJS 脚本执行
```

## 4. 窗口模式体系

项目通过 `WINDOW_MODE` 枚举区分四种运行模式：

| 模式 | 枚举值 | 触发参数 | 输出目标 | 用途 |
|------|--------|----------|---------|------|
| `NORMAL_WINDOW` | 0 | 默认 | `GLFWWindowOutput` | 可缩放、带装饰的预览窗口 |
| `DESKTOP_BACKGROUND` | 1 | `--screen-root` | `X11Output`（X11）或 Wayland Layer Shell | 桌面背景（写 X11 根窗口 Pixmap 或 wlr-layer） |
| `EXPLICIT_WINDOW` | 2 | `--window XxYxWxH` | `GLFWWindowOutput` | 固定几何位置的无边框置顶预览窗口 |
| `GNOME_X11_DESKTOP_WINDOW` | 3 | `--gnome-x11` | `GLFWWindowOutput`（当前），后续替换为 `GNOMEX11WindowOutput` | GNOME X11 原生桌面窗口（Managed Window 层级） |

### 4.1 模式驱动的路由机制

```
CLI 解析 (ApplicationContext)
    │
    ├── WINDOW_MODE 设置
    │
    ▼
VideoFactories (驱动工厂)
    │
    ├── NORMAL_WINDOW + EXPLICIT_WINDOW
    │   └── lookup key: DEFAULT_WINDOW_NAME → GLFWOpenGLDriver
    │
    ├── DESKTOP_BACKGROUND
    │   └── lookup key: XDG_SESSION_TYPE
    │       ├── "x11"     → GLFWOpenGLDriver → X11Output
    │       └── "wayland" → WaylandOpenGLDriver → Wayland Layer Shell
    │
    └── GNOME_X11_DESKTOP_WINDOW
        └── lookup key: XDG_SESSION_TYPE
            └── "x11"     → GLFWOpenGLDriver → GLFWWindowOutput (→ GNOMEX11WindowOutput)
```

### 4.2 模式在关键路径中的行为差异

| 行为 | NORMAL_WINDOW | EXPLICIT_WINDOW | DESKTOP_BACKGROUND | GNOME_X11_DESKTOP_WINDOW |
|------|:---:|:---:|:---:|:---:|
| 窗口装饰 | 有 | 无 | N/A | 无 |
| 可缩放 | 是 | 否 | N/A | 否 |
| 窗口置顶 | 否 | 是 | N/A | 否 |
| 初始可见 | 否（手动 show） | 否 | N/A | 否 |
| Viewport 更新时机 | 渲染前 | 渲染前 | 渲染后（X11 copy） | 渲染前 |
| 背景加载 | 单默认背景 | 单默认背景 | 多屏 + span | 多屏（回退到 sys_default） |
| 默认缩放 | fit+border | fit+border | default | default |
| 全屏检测器 | 无操作 | 无操作 | 真实检测器 | 真实检测器 |
| 窗口标题 | wallpaperengine | wallpaperengine | wallpaperengine | wallpaperengine-desktop |

## 5. 输出类架构

```
Output (基类)
├── GLFWWindowOutput         # 用于 NORMAL_WINDOW / EXPLICIT_WINDOW
│   └── GLFWOutputViewport   # 单视口映射
├── X11Output                # 用于 DESKTOP_BACKGROUND + X11
│   └── X11OutputViewport    # X11 屏幕视口（写根窗口 Pixmap）
└── GNOMEX11WindowOutput     # 用于 GNOME_X11_DESKTOP_WINDOW
    └── GLFWOutputViewport   # 通过 EWMH 属性实现桌面窗口层级
```

## 6. 渲染管线（简化）

```
dispatchEventQueue()
    │
    ├── glfwPollEvents()                        # 处理窗口事件
    │
    ├── if window mode: updateRender()          # 更新 viewport
    │
    ├── glBindFramebuffer(0)                    # 恢复默认 framebuffer
    ├── glDisable(GL_SCISSOR_TEST)              # 清除场景残留状态
    ├── glViewport(...) + glClear(...)          # 清空 back buffer
    │
    ├── for each viewport:
    │   └── WallpaperApplication::update()      # 渲染壁纸
    │       └── RenderContext::render()
    │           ├── CWallpaper::render()        # 具体壁纸渲染
    │           │   ├── CScene: FBO → Effect Passes → 合成
    │           │   ├── CVideo: MPV → 纹理 → 绘制
    │           │   └── CWeb: CEF → OnPaint → 纹理 → 绘制
    │           └── glReadPixels() [可选，screenshot]
    │
    ├── if desktop mode: updateRender()         # X11 copy 到根窗口
    │
    └── glfwSwapBuffers()                       # 交换前后缓冲
```

## 7. 事件循环与暂停状态

已从 `dispatchEventQueue()` 提取 `pumpEvents()` 到 VideoDriver 基类（步骤 2）。

```
pumpEvents()          # GLFW/X11 事件（始终运行，暂停时也会调用）
dispatchEventQueue()  # 事件 + 渲染 + buffer 交换（暂停时跳过）
```

暂停时只停止场景更新和 OpenGL 绘制，事件循环和退出处理继续运行。

## 8. GNOME X11 原生桌面窗口实施路线

完整路线见 `docs/UBUNTU_GNOME_X11_COMPATIBILITY_PROGRESS.md` 第 8 节。已完成的步骤：

| 步骤 | 状态 | 说明 |
|------|------|------|
| 构建兼容性修复 | ✅ | GCC 11 编译、头文件、格式兼容 |
| 窗口缩放/resize 修复 | ✅ | viewport 更新时序、全屏暂停误判 |
| X11 全屏检测修复 | ✅ | 窗口指针/句柄混淆、双重释放 |
| CEF 纹理目标修复 | ✅ | framebuffer ID → 纹理 ID |
| **步骤 1：新模式和 CLI** | ✅ | `GNOME_X11_DESKTOP_WINDOW` + `--gnome-x11` |
| **步骤 2：事件循环拆分** | ✅ | `pumpEvents()` 纯虚方法，暂停时窗口保持响应 |
| **步骤 3：GNOMEX11WindowOutput** | ✅ | EWMH below/sticky/skip-taskbar/skip-pager, XRandR 单屏 |
| 步骤 4：点击穿透 | 🔜 | XFixes/Shape 空输入区域 + 全局鼠标位置 |
| 步骤 5-10 | 🔜 | 多显示器、热插拔、全屏暂停优化、服务化、GUI |

## 9. 依赖关系

```
应用层:        linux-wallpaperengine
               ┌─────────┼─────────┐
渲染:        OpenGL    GLFW3    GLEW   GLM
               │
视频:        MPV + FFmpeg (avcodec/avformat/avutil/swscale)
               │
音频:        SDL2 + PulseAudio + FFTW3
               │
脚本:        QuickJS (内嵌)
               │
Web:         CEF (Chromium Embedded Framework)，构建时下载
               │
X11:         XRandR, Xinerama, XCursor, Xi
               │
Wayland:     wlr-layer-shell-unstable, xdg-output-unstable
               │
其他:        LZ4, Zlib, FreeType, D-Bus, stb_image
```

## 10. 构建系统

- **CMake** 配置，`CMakeLists.txt` 约 31KB
- 构建类型：`Release` / `Debug`
- CEF 在 CMake 配置阶段从官方分发服务器下载（需网络）
- 输出目录：`build/output/`，包含 `linux-wallpaperengine` 二进制和 `liblinux-wallpaperengine-lib.so`
