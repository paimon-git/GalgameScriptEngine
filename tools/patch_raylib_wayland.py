#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
给 raylib 5.5 的 GLFW 平台层打三个小补丁，让它在 Wayland（含显示缩放）下正确工作。

背景：
  * raylib 5.5 从不设置 GLFW_SCALE_FRAMEBUFFER，Wayland 下帧缓冲停留在逻辑尺寸，
    画面被合成器放大 → 整屏发虚（150% 缩放的屏幕上尤其明显）。
  * 它只在 InitWindow 里同步一次 HiDPI 状态，运行时窗口尺寸变化（拖动 / 全屏）
    不会重算缩放矩阵，内容会画不满窗口。

用法: python3 tools/patch_raylib_wayland.py <raylib_src_dir>
     （<raylib_src_dir> 里应有 platforms/rcore_desktop_glfw.c）
"""

import os
import sys

SCALE_HINT = "    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);   // Wayland HiDPI: framebuffer follows display scale\n"

FB_CALLBACK = """
// [patched] 帧缓冲尺寸变化（显示缩放变化 / HiDPI）时同步视口与缩放矩阵
static void WindowFramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    int winWidth = 1, winHeight = 1;
    glfwGetWindowSize(window, &winWidth, &winHeight);
    SetupViewport(width, height);
    CORE.Window.currentFbo.width = width;
    CORE.Window.currentFbo.height = height;
    CORE.Window.screenScale = MatrixScale((float)width/(float)winWidth,
                                          (float)height/(float)winHeight, 1.0f);
    CORE.Window.resizedLastFrame = true;
}
"""

NEW_SIZE_CALLBACK = """static void WindowSizeCallback(GLFWwindow *window, int width, int height)
{
    // [patched] 帧缓冲可能按显示缩放变大：视口/投影用帧缓冲尺寸，逻辑尺寸记到 screen
    int fbWidth = width, fbHeight = height;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    SetupViewport(fbWidth, fbHeight);
    CORE.Window.currentFbo.width = fbWidth;
    CORE.Window.currentFbo.height = fbHeight;
    CORE.Window.screenScale = MatrixScale((float)fbWidth/(float)width,
                                          (float)fbHeight/(float)height, 1.0f);
    CORE.Window.resizedLastFrame = true;

    // 注意：这里不再像原版那样在 fullscreen 时提前返回——screen 必须始终等于
    // 逻辑窗口尺寸，否则运行时进全屏后画布缩放会一直停在旧值（内容画不满）。
    CORE.Window.screen.width = width;
    CORE.Window.screen.height = height;
}
"""


def patch(path):
    src = open(path, encoding="utf-8").read()
    changed = []

    # 1) 打开 GLFW_SCALE_FRAMEBUFFER
    if "GLFW_SCALE_FRAMEBUFFER" not in src:
        anchor = "    glfwWindowHint(GLFW_AUTO_ICONIFY, 0);\n"
        if anchor not in src:
            raise SystemExit("找不到插入 GLFW_SCALE_FRAMEBUFFER 锚点（raylib 版本不同？）")
        src = src.replace(anchor, anchor + SCALE_HINT, 1)
        changed.append("GLFW_SCALE_FRAMEBUFFER")

    # 2) 替换 WindowSizeCallback：帧缓冲尺寸与逻辑尺寸分开处理
    head = "static void WindowSizeCallback(GLFWwindow *window, int width, int height)\n{"
    i = src.find(head)
    if i < 0:
        raise SystemExit("找不到 WindowSizeCallback（raylib 版本不同？）")
    end = src.find("\n}\n", i)
    if end < 0:
        raise SystemExit("WindowSizeCallback 结构异常")
    if "[patched]" not in src[i:end]:
        src = src[:i] + NEW_SIZE_CALLBACK.rstrip("\n") + src[end + 3:]
        changed.append("WindowSizeCallback")

    # 3) 注册帧缓冲尺寸回调
    reg = "    glfwSetWindowSizeCallback(platform.handle, WindowSizeCallback);"
    if "glfwSetFramebufferSizeCallback" not in src:
        if reg not in src:
            raise SystemExit("找不到 glfwSetWindowSizeCallback 注册处")
        # 新回调要插在 WindowSizeCallback 定义之前
        j = src.find("static void WindowSizeCallback(GLFWwindow *window, int width, int height)")
        src = src[:j] + FB_CALLBACK.strip("\n") + "\n\n" + src[j:]
        src = src.replace(
            reg,
            reg + "\n    glfwSetFramebufferSizeCallback(platform.handle, WindowFramebufferSizeCallback);",
            1)
        changed.append("FramebufferSizeCallback")

    if not changed:
        print(f"raylib 平台层已打过补丁，跳过: {path}")
        return

    open(path, "w", encoding="utf-8").write(src)
    print(f"已给 {path} 打补丁: {', '.join(changed)}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    path = os.path.join(sys.argv[1], "platforms", "rcore_desktop_glfw.c")
    if not os.path.isfile(path):
        print(f"找不到 {path}")
        return 1
    patch(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
