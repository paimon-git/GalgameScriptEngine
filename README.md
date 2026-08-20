# QLWT 视觉小说引擎

基于 C++20 + [raylib](https://www.raylib.com/) 的 galgame 引擎，使用一套自制的 `.gal` 脚本语言编写剧本。现代蔚蓝档案风格 UI：章节选择、圆角矩形控件、平滑缓动动画、全屏 CG（图片 / GIF / 视频）、存档读档、BGM / 音效。

## 快速开始

### 环境要求

- Windows，已安装 [MinGW-w64](https://www.mingw-w64.org/)（本项目使用 `C:\msys64\mingw64` 的 g++）
- raylib（已链接到 `C:\msys64\mingw64`）
- ffmpeg（可选，仅 MP4 等视频 CG 需要；引擎按 `QLWT_FFMPEG` 环境变量 > PATH > 常见安装位置 > 剪映自带 的顺序自动查找）

### 构建与运行

```bat
powershell -ExecutionPolicy Bypass -File build.ps1   :: 编译（无需 CMake）
run.bat                                             :: 启动游戏
```

也可以直接运行 `qlwt.exe`。编译前请先关闭正在运行的游戏，否则 exe 会被占用无法覆盖。

项目自带一套由脚本生成的占位素材（背景 / 角色 / 表情 / CG / 音频），需要重新生成时运行：

```bat
powershell -ExecutionPolicy Bypass -File tools/make_assets.ps1
```

### 无头自检 / 调试命令

```bat
qlwt.exe --script 脚本.gal            :: 指定脚本进入游戏
qlwt.exe --check-script 脚本.gal      :: 无窗口校验：脚本能执行到 game_end
qlwt.exe --check-chapters 脚本.gal    :: 无窗口校验：每个 chapter 都能跑完
qlwt.exe --check-save 脚本.gal        :: 无窗口校验：中途存档后恢复能跑完
qlwt.exe --selftest [--frames N]      :: 自动跑完整流程并截图（默认 20000 帧）
qlwt.exe --help                       :: 查看全部参数
```

## 操作

| 按键 | 功能 |
| --- | --- |
| 空格 / 回车 / 鼠标左键 | 推进对话（打字中按下 = 直接显示全文） |
| Ctrl（按住） | 快进 |
| A | 自动播放开关 |
| L | 对话历史（↑/↓ 或滚轮翻页） |
| F5 / F9 | 存档 / 读档菜单（6 个槽位，点击或按 1-6 执行） |
| Esc | 剧情中打开暂停菜单（继续 / 存档 / 读档 / 返回标题） |
| 1-9 / 鼠标 | 选择分支选项 |
| Enter / Esc | 章节结束询问：继续下一章 / 返回章节选择 |

设置从右侧滑出抽屉面板：文字速度、音乐音量、音效音量、全屏模式，自动保存到 `settings.cfg`。

## `.gal` 脚本语言

完整命令说明书见 [docs/gal-script-manual.md](docs/gal-script-manual.md)。

基本规则：

- 一行一条命令，命令与参数用空格分隔
- `#` 开头是注释；`#RRGGBB`（恰好 6 位十六进制）会被识别为颜色
- 字符串用双引号 `"..."`，内部 `\"` 转义；含空格的内容必须加引号
- 标签 `@名字`（或 `@名字:`）与 `jump` 配合跳转，标签不能重复
- 脚本使用 UTF-8 编码（兼容 BOM）
- **素材路径不要使用中文**（引擎在 Windows 下读不了中文路径）

一个最小示例：

```gal
title 追光的人

init_character 晨星 assets/char/hero_body.png #6FA8FF center
init_bg 天台 assets/bg/classroom.png

show_bg 天台
show_character 晨星
say 晨星 "又是一个平凡的放学铃。"

choice 晨星 "要一起去吗？" {
    "当然！", jump yes
    "下次吧……", jump no
}

@yes
say 晨星 "好，那说定了！"
jump end

@no
say 晨星 "……好吧。"

@end
game_end
```

### 章节

每个 `.gal` 脚本是一个**大章节**（名字取 `title`），脚本内的 `chapter` 块是**小章节**：

```gal
chapter 1 "第一章 · 教室" picture assets/bg/classroom.png {
    show_bg 天台
    say 晨星 "……"
    game_end
}
```

- 章节体结束时自动结束本次游玩，不会流入下一个章节
- 章节进度记录在 `progress.dat`（每行 `脚本路径|章节编号`）
- **完整看完**一章（走到结尾 / `game_end`）才会标记为已观看；中途退出不标记
- 章节选择界面：未观看章节不可进入，只有已观看与“下一章”（金色描边）可进入
- 章节结束后弹出询问：继续下一章 / 返回章节选择

### 指令一览

| 指令 | 说明 |
| --- | --- |
| `title <文本>` | 大章节标题（显示在 END 画面与章节选择） |
| `init_character <名字> <贴图> <#RRGGBB> <位置>` | 注册角色（位置：left / center / right 或 0..1） |
| `init_character_face <名字> <目录>` | 设置表情贴图目录（`change_face` 从这里读取） |
| `init_bg <场景ID> <贴图>` | 预加载背景 |
| `show_bg <场景ID>` | 切换背景（淡入淡出） |
| `show_character <名字>` | 角色入场 |
| `hide_character <名字>` | 角色退场（先变黑再完全消失） |
| `move <名字> <位置>` | 角色平滑移动到新位置 |
| `change_face <名字> <文件名>` | 切换表情 |
| `say <名字> "<台词>"` | 对话框台词 |
| `narrate "<旁白>"` | 顶部旁白（无名字框） |
| `choice <名字> "<提问>" { "选项", jump <标签> ... }` | 分支选项 |
| `@<标签名>` / `jump <标签名>` | 标签与跳转 |
| `wait <秒数>` | 停顿（支持小数） |
| `card <文本>` | 章节 / 地点卡片 |
| `show_cg <文件>` | 全屏 CG：静态图 3 秒自动结束；GIF/APNG 动图、MP4/MOV/WebM 视频播完自动结束；点击可跳过 |
| `hide_cg` | 手动提前结束 CG |
| `play_bgm <文件>` / `stop_bgm` / `play_se <文件>` | 音频 |
| `game_end` | 结束游戏 |

## 素材目录约定

```
assets/
  scripts/   所有 .gal 脚本（文件名以 test 开头或 _ 开头会被章节选择跳过）
  bg/        背景图
  char/      角色立绘与表情目录
  cg/        全屏 CG（图片 / GIF / MP4）
  audio/     BGM 与音效
```

以上素材全部由 `tools/make_assets.ps1` 生成（纯合成占位图与合成音频，不包含任何第三方素材），可以随意替换成自己的图片 / 音乐。

立绘采用“本体 + 表情分离”模型：`init_character` 注册本体贴图，`init_character_face` 指定表情目录（内含 `normal.png`、`happy.png` 等），`change_face` 随时切换。没有表情素材时可以只用本体贴图，不调用表情指令即可。

## 特性

- 打字机对话、点击全文、名字牌、自动换行、对话历史、自动播放、Ctrl 快进
- 选项面板（鼠标 / 数字键）、分支淡入淡出
- 立绘淡入淡出、呼吸动画、说话角色轻微前移；角色退场走黑屏过渡
- 对话框背景随说话角色颜色平滑过渡（千恋万花风格）
- 顶部旁白、章节 / 地点卡片、章节结束询问
- 全屏 CG：静态图（3 秒自动结束）、GIF/APNG 内置解码、MP4/MOV/WebM 通过 ffmpeg 管道解码；黑屏无缝衔接过渡；重复播放 / 读档后视频从头开始
- 剧情中 Esc 暂停菜单（继续 / 存档 / 读档 / 返回标题）
- 存档 / 读档：6 个槽位（`saves/slot1.dat` ~ `slot6.dat`），带预览文本与时间
- BGM（循环）/ SE 音效，支持 WAV / OGG / MP3 等 raylib 支持格式
- 章节进度系统与蓝档案风格章节选择（封面卡片、小章节缩略图、金色“下一章”）
- 全部按钮 / 对话框 / 面板为 12px 圆角矩形，圆角同心统一
- 2x 超采样抗锯齿（离屏渲染目标 + 双线性下采样）
- 所有动画使用 smoothstep / easeOut 缓动，非线性不生硬
- 中文字体自动收集字形（脚本 + 界面文案），按需生成字体图集
- 设置持久化（`settings.cfg`）与全屏切换
- VSCode 语法高亮扩展（`.vscode/gal-lang/`，或安装 `qlwt-gal-lang-0.1.0.vsix`）
- 无头脚本自检（`--check-script` / `--check-chapters` / `--check-save` / `--selftest`）

## 项目结构

```
core/      引擎主循环、输入、时间、设置、字体、脚本解析器与解释器、视频播放、存档
game/      章节选择、标题 / 设置、游戏场景、角色、对话框、选项
renderer/  UI 绘制辅助（圆角面板 / 按钮 / 滑块 / 渐变）
assets/    示例脚本与美术素材
docs/      脚本语言说明书
tools/     素材生成与测试工具（make_assets.ps1、gen_gif.cpp 等）
.vscode/   gal 语言扩展（gal-lang/）与 VSIX 安装包
```

## 常见问题

- **编译报 `Permission denied`**：游戏正在运行占用了 `qlwt.exe`，关闭游戏后重新编译。
- **MP4 CG 不播放**：缺少 ffmpeg。设置环境变量 `QLWT_FFMPEG` 指向 ffmpeg.exe，或安装到常见位置。
- **素材加载失败（`texture not found`）**：路径含中文。把文件 / 文件夹改成英文名。
- **脚本报错**：引擎解析时会给出具体行号与原因（未知命令、跳转目标不存在、重复标签、未闭合块等）。
- **存档 / 进度在哪**：`saves/`（6 个槽位）、`progress.dat`（章节进度）、`settings.cfg`（设置），都在游戏目录下。
