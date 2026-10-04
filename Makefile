# ===========================================================================
# QLWT 视觉小说引擎 —— Linux / macOS 构建（与 build.ps1 对应）
#
#   make            增量编译，产物 ./qlwt
#   make -j8        并行编译（快很多）
#   make clean      清理目标文件与可执行文件
#   make run        编译并启动游戏
#   make check      无头自检：脚本能跑完 / 每个 chapter 能跑完 / 存读档
#   make font       从系统字体抽取一个中文 .ttf 到 assets/fonts/（Linux/macOS 首次需要）
#   make wayland    自己编一个带 Wayland 的 raylib 并链接（Wayland 会话下原生运行）
#   make clean-wayland  丢掉本地那份 Wayland raylib，回到系统 raylib
#
# 依赖：g++（C++20）+ raylib。
#   Debian/Ubuntu: sudo apt install g++ libraylib-dev
#   Arch:          sudo pacman -S gcc raylib
#   macOS:         brew install raylib
#
# 注意：引擎启动时会把工作目录切到可执行文件所在目录，而素材是按
# assets/... 相对路径读取的，所以 qlwt 必须和 assets/ 放在同一层
# （也就是仓库根目录，和 Windows 下的 qlwt.exe 一样）。
# ===========================================================================

CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -I.
LDFLAGS  ?=

UNAME_S  := $(shell uname -s)
RAYLIB_WL_DIR := build/raylib-wayland
RAYLIB_INC :=

ifeq ($(UNAME_S),Darwin)
  RAYLIB_LIBS := -lraylib -framework OpenGL -framework Cocoa -framework IOKit \
                 -framework CoreVideo -lm -lpthread
else
  # 优先用 raylib 自己的 pkg-config（会带上 GL / X11 等传递依赖）
  RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
  ifeq ($(strip $(RAYLIB_LIBS)),)
    RAYLIB_LIBS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
  endif
endif

# 跑过 `make wayland` 之后，本地这份带 Wayland 的 raylib 自动优先生效
ifneq ($(wildcard $(RAYLIB_WL_DIR)/lib/libraylib.so),)
  RAYLIB_INC  := -I$(RAYLIB_WL_DIR)/src
  RAYLIB_LIBS := -L$(RAYLIB_WL_DIR)/lib -lraylib \
                 -Wl,-rpath,$(CURDIR)/$(RAYLIB_WL_DIR)/lib \
                 -lGL -lm -lpthread -ldl -lrt -lX11
endif

SRCS    := $(wildcard core/*.cpp) $(wildcard game/*.cpp) $(wildcard renderer/*.cpp) main.cpp
OBJDIR  := build/obj
OBJS    := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)
TARGET  := qlwt

.PHONY: all clean clean-wayland run check font wayland help

NPROC := $(shell nproc 2>/dev/null || echo 4)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(RAYLIB_LIBS)
	@echo "build OK -> ./$(TARGET)"

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(RAYLIB_INC) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

check: $(TARGET)
	@for f in assets/scripts/*.gal; do \
	    case "$$(basename $$f)" in test*|_*) continue;; esac; \
	    echo "== $$f =="; \
	    ./$(TARGET) --check-script   $$f || exit 1; \
	    ./$(TARGET) --check-chapters $$f || exit 1; \
	    ./$(TARGET) --check-save     $$f || exit 1; \
	done
	@echo "== 剧情树 =="
	@./$(TARGET) --tree
	@echo "== 素材 =="
	@./$(TARGET) --check-assets
	@echo "== 观测战 =="
	@./$(TARGET) --check-battle
	@echo "== 触摸板手势 =="
	@./$(TARGET) --check-gesture

# 系统里的中文字体基本都是 .ttc，而 raylib 只认 .ttf/.otf，所以抽一份出来。
font:
	@mkdir -p assets/fonts
	python3 tools/ttc2ttf.py "$$(fc-match -f '%{file}' :lang=zh-cn)" \
	    assets/fonts/NotoSansSC-Regular.ttf SC

# ---------------------------------------------------------------------------
# Wayland
#
# 发行版预编译的 raylib 在 Linux 上通常只编了 X11（GLFW 默认只要 X11），
# 于是 Wayland 会话里其实是跑在 XWayland 上。要原生 Wayland 就得自己编一份
# 带 Wayland 的 raylib：GLFW 3.4 会读 XDG_SESSION_TYPE 自动选平台，
# X11 仍然保留作兜底。
#
# 需要 raylib 源码。用 RAYLIB_SRC 指定，或放在下面几个常见位置之一：
#   make wayland
#   make wayland RAYLIB_SRC=~/src/raylib/src
# ---------------------------------------------------------------------------
RAYLIB_SRC ?= $(firstword $(wildcard \
    $(HOME)/project/sea_of_stars/third_party/raylib/src \
    $(HOME)/raylib/src \
    /usr/local/src/raylib/src \
    external/raylib/src))

wayland:
	@test -f "$(RAYLIB_SRC)/Makefile" || { \
	    echo "找不到 raylib 源码（RAYLIB_SRC=$(RAYLIB_SRC)）"; \
	    echo "用法: make wayland RAYLIB_SRC=/path/to/raylib/src"; \
	    exit 1; }
	@echo "== 1/2 编译带 Wayland 的 raylib（源码复制到 $(RAYLIB_WL_DIR)/src，不动原目录）=="
	rm -rf $(RAYLIB_WL_DIR)
	mkdir -p $(RAYLIB_WL_DIR)
	cp -a "$(RAYLIB_SRC)" $(RAYLIB_WL_DIR)/src
	-$(MAKE) -C $(RAYLIB_WL_DIR)/src clean >/dev/null 2>&1
	@# raylib 5.5 在 Wayland + 显示缩放下有毛病：不启用 GLFW_SCALE_FRAMEBUFFER
	@# （帧缓冲不跟随缩放 → 被合成器放大后发虚），而且只在建窗口时同步一次 HiDPI
	@# 状态（运行时改窗口大小内容会画不满）。这里给它的平台层打补丁。
	python3 tools/patch_raylib_wayland.py $(RAYLIB_WL_DIR)/src
	$(MAKE) -C $(RAYLIB_WL_DIR)/src -j$(NPROC) PLATFORM=PLATFORM_DESKTOP \
	    GLFW_LINUX_ENABLE_WAYLAND=TRUE RAYLIB_LIBTYPE=SHARED \
	    CUSTOM_LDFLAGS="$$(pkg-config --libs wayland-client wayland-cursor wayland-egl xkbcommon)"
	mkdir -p $(RAYLIB_WL_DIR)/lib
	cp -P $(RAYLIB_WL_DIR)/src/libraylib.so* $(RAYLIB_WL_DIR)/lib/
	-$(MAKE) -C $(RAYLIB_WL_DIR)/src clean >/dev/null 2>&1
	@echo "== 2/2 用它重新编译游戏 =="
	$(MAKE) clean
	$(MAKE)
	@echo "完成：./qlwt 现在跑原生 Wayland（非 Wayland 会话会自动退回 X11）"

clean-wayland:
	rm -rf $(RAYLIB_WL_DIR)
	$(MAKE) clean
	$(MAKE)
	@echo "已回到系统 raylib"

clean:
	rm -rf $(OBJDIR) $(TARGET)
	@echo "clean OK"

help:
	@sed -n '2,14p' Makefile

-include $(DEPS)
