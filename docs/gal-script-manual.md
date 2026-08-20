# QLWT `.gal` 脚本语言说明书

`.gal` 是 QLWT 引擎的剧本语言，一个 `.gal` 文件就是一个**大章节**，文件里的 `chapter` 块是**小章节**。引擎启动时会扫描 `assets/scripts/` 下的所有 `.gal` 文件（文件名以 `test` 开头或 `_` 开头的会被跳过，仅作测试用）。

---

## 1. 基本语法

| 规则 | 说明 |
| --- | --- |
| 编码 | UTF-8（兼容带 BOM 的文件） |
| 行结构 | 一行一条命令，命令与参数用空格分隔 |
| 注释 | `#` 开头到行尾。**例外**：`#` 后面恰好跟 6 位十六进制（如 `#6FA8FF`）会被识别为颜色 |
| 字符串 | 双引号 `"..."`，内部可以用 `\"` 表示引号；含空格的内容必须加引号 |
| 标签 | `@名字` 或 `@名字:`，作为跳转目标 |
| 大小写 | 命令均为小写英文，角色名、场景 ID、标签名区分大小写 |

所有命令都是**按顺序执行的**。除了 `init_character`、`init_character_face`、`init_bg`、`title` 这类初始化命令外，大部分命令会阻塞执行，直到玩家操作或条件满足后才继续下一行。

---

## 2. 章节结构

```gal
chapter <编号> <名称> [picture <封面图>] {
    ...小章节内容...
}
```

- 一个脚本可以写多个 `chapter` 块，每块是一个独立的小章节。
- `picture` 可选，指定该章节的封面图，也作为大章节的封面（取第一个小章节的封面）。
- 章节体结束时自动结束本次游玩（等同 `game_end`），**不会**继续往下流入下一个章节。
- 章节编号必须是整数。
- 章节进度记录在 `progress.dat` 文件中，每行格式为 `脚本路径|章节编号`。进入一个章节时会自动标记为已观看。
- 章节选择界面里：已观看的章节正常显示，未观看的显示为灰度，紧跟在已看章节之后的下一章会有金色描边提示。

---

## 3. 全部命令

### 3.1 元信息

#### `title`
```gal
title <文字>
```
设置大章节的标题（可以不加引号，也可以加）。它只是元信息，运行时不会显示。没有 `title` 时，章节选择界面用脚本文件名代替。

```gal
title 追光的人
```

---

### 3.2 初始化

#### `init_character`
```gal
init_character <角色名> <贴图路径> <#RRGGBB> <位置>
```
注册一个角色：立绘贴图、名字颜色（对话名字框用这个颜色）、初始站位。

位置有三种写法：

| 写法 | 水平位置 |
| --- | --- |
| `left` | 0.18（约左侧 1/5） |
| `center` | 0.50（正中） |
| `right` | 0.82（约右侧 4/5） |
| `0` ~ `1` 小数 | 直接指定屏幕宽度比例 |

```gal
init_character 晨星 assets/char/hero_body.png #6FA8FF center
init_character 小惠 assets/char/heroine_body.png #FF8FB0 right
```

#### `init_character_face`
```gal
init_character_face <角色名> <表情目录>
```
指定角色的表情贴图目录，之后 `change_face` 从这个目录里读文件。如果目录里有 `normal.png`，角色入场时会自动使用。

```gal
init_character_face 晨星 assets/char/hero_face
```

#### `init_bg`
```gal
init_bg <场景ID> <贴图路径>
```
注册一个背景。场景 ID 是自定义字符串，之后用 `show_bg` 引用。

```gal
init_bg 天台 assets/bg/classroom.png
init_bg 黄昏 assets/bg/street.png
```

---

### 3.3 场景与角色演出

#### `show_bg`
```gal
show_bg <场景ID>
```
显示背景，带淡入淡出过渡。

```gal
show_bg 天台
```

#### `show_character`
```gal
show_character <角色名>
```
角色入场，带平滑的入场动画。

#### `hide_character`
```gal
hide_character <角色名>
```
角色退场：先变黑，再完全消失后才继续执行后面的命令。

#### `move`
```gal
move <角色名> <位置>
```
把角色**平滑移动**到新位置，位置写法与 `init_character` 相同（left / center / right / 0~1 小数）。

```gal
move 晨星 left
```

#### `change_face`
```gal
change_face <角色名> <表情文件名>
```
从 `init_character_face` 指定的目录里切换表情，文件名形如 `happy.png`、`sad.png`。

```gal
change_face 小惠 happy.png
```

---

### 3.4 台词与演出

#### `say`
```gal
say <角色名> "<台词>"
```
显示角色对话：底部对话框 + 角色名（用该角色的名字颜色）。打字结束后点击 / 回车推进。

```gal
say 晨星 "又是一个平凡的放学铃。"
```

#### `narrate`
```gal
narrate "<旁白>"
```
顶部旁白，没有名字框，淡入淡出，点击推进。

```gal
narrate "放学后的天台，风把课本吹得唰唰作响。"
```

#### `card`
```gal
card <文字>
```
全屏中央的章节卡片文字（可以加引号），淡入后点击继续，用于章节开场/转场。

```gal
card 第一话 · 教室
```

#### `wait`
```gal
wait <秒数>
```
等待指定秒数后再继续，用于控制演出节奏（支持小数）。

```gal
wait 0.4
```

---

### 3.5 分支

#### `choice`
```gal
choice <角色名> "<提问>" {
    "<选项文字>", jump <标签>
    "<选项文字>", jump <标签>
}
```
显示分支选择。提问照常显示在对话框里，选项至少一个；玩家选中后跳转到对应标签继续。

```gal
choice 小惠 "要不要一起去？" {
    "当然要去！", jump yes1
    "我还有作业……", jump no1
}

@yes1
say 小惠 "太好了！那放学后天台见！"
jump end1

@no1
say 小惠 "好吧……那我自己去啦。"
jump end1

@end1
say 晨星 "……放学后，再说吧。"
```

#### `jump` 与标签
```gal
jump <标签名>
@<标签名>
```
`@标签` 定义跳转目标，`jump` 跳到那里。标签名不能重复；跳转到不存在的标签会在解析时报错。

---

### 3.6 全屏 CG

#### `show_cg`
```gal
show_cg <文件路径>
```
全屏显示 CG，进入时先黑屏再直接衔接。支持三种素材：

| 素材 | 行为 |
| --- | --- |
| 静态图片（png / jpg 等） | 显示约 3 秒后自动继续 |
| 动图（gif） | 播放动画，结束后继续 |
| 视频（mp4） | 播放到结尾后自动继续 |

播放中点击可以随时跳过。**注意**：mp4 需要系统里有 ffmpeg，否则会提示找不到。

```gal
show_cg assets/cg/school.png
show_cg assets/cg/sky.mp4
```

#### `hide_cg`
```gal
hide_cg
```
手动关闭当前 CG（一般不需要，`show_cg` 播完会自动关）。

---

### 3.7 音频

#### `play_bgm`
```gal
play_bgm <音频文件>
```
播放背景音乐（循环播放）。切换 BGM 时直接再写一条即可。

#### `stop_bgm`
```gal
stop_bgm
```
停止背景音乐。

#### `play_se`
```gal
play_se <音频文件>
```
播放一次音效。

---

### 3.8 结束

#### `game_end`
```gal
game_end
```
结束当前章节 / 剧本，回到章节选择或询问是否继续。脚本走到结尾而没有 `game_end` 也等同于结束。

---

## 4. 运行机制与文件

| 项目 | 说明 |
| --- | --- |
| 章节进度 | `progress.dat`，每行 `脚本路径\|章节编号`，标记已观看章节 |
| 存档 | `saves/slot1.dat` ~ `saves/slot6.dat`，共 6 个存档位 |
| 存档 / 读档 | 游戏内按 `F5` 存档、`F9` 读档 |
| 推进 | 点击 / 回车推进；打字中点一下直接显示全文 |
| 自动模式 | 按 `A` 开关自动播放 |
| 日志 | 按 `L` 打开对话历史，`Esc` 关闭 |

脚本在解析时会做完整性检查，出错会报具体行号，例如：

- 未知命令 / 参数缺失
- 字符串没闭合、块没闭合
- `jump` 或 `choice` 跳转到不存在的标签
- 标签重复定义

---

## 5. 完整示例

```gal
# =====================
# 追光的人（大章节）
# =====================
title 追光的人

# ---- 初始化 ----
init_character 晨星 assets/char/hero_body.png #6FA8FF center
init_character 小惠 assets/char/heroine_body.png #FF8FB0 right
init_character_face 晨星 assets/char/hero_face
init_character_face 小惠 assets/char/heroine_face

init_bg 天台 assets/bg/classroom.png
init_bg 黄昏 assets/bg/street.png
init_bg 夜空 assets/bg/title.png

# ---- 第一话 ----
chapter 1 "第一话 · 教室" picture assets/bg/classroom.png {
    show_bg 天台
    show_character 晨星
    wait 0.4
    say 晨星 "又是一个平凡的放学铃。"

    move 晨星 left
    wait 0.8
    say 晨星 "但今天，好像有点不一样。"

    show_character 小惠
    say 小惠 "喂，你听说了吗？天文社今晚有流星雨观测活动。"

    choice 小惠 "要不要一起去？" {
        "当然要去！", jump yes1
        "我还有作业……", jump no1
    }

    @yes1
    change_face 小惠 happy.png
    say 小惠 "太好了！那放学后天台见！"
    jump end1

    @no1
    change_face 小惠 sad.png
    say 小惠 "好吧……那我自己去啦。"
    jump end1

    @end1
    say 晨星 "……放学后，再说吧。"
    game_end
}

# ---- 第二话 ----
chapter 2 "第二话 · 放学后" picture assets/cg/school.png {
    show_bg 黄昏
    show_character 晨星
    change_face 晨星 happy.png
    wait 0.4

    narrate "放学后的天台，风把课本吹得唰唰作响。"
    say 晨星 "黄昏的光线把影子拉得很长。"

    show_cg assets/cg/school.png
    narrate "远处传来广播体操的旋律。"
    say 晨星 "……还是去吧。有些事情，错过了就真的错过了。"
    game_end
}
```
