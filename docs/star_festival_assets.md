# 《星屑祭典》素材需求清单

> 对应剧本：`assets/scripts/star_festival.gal`（大章节「星屑祭典」，完整版共 8 个小章节、782 行）
> 引擎：QLWT 视觉小说引擎 ｜ 基准分辨率 1280×720 ｜ 素材路径必须为 **ASCII**（引擎在 Windows 下读不了中文路径）
> 说明：旧的示例剧本 `demo.gal`、`after_school.gal` 已删除，本作现在是唯一的剧情脚本，也是引擎默认脚本。

---

## 0. 故事梗概

私立青云高中天文社只剩两个人。文化祭「青云祭」后社团将被废部，唯一的转机是——在祭典当晚办成一场公开天象观测会：到场超过一百人，并留下一份有效的天象记录。

从收到废部通知到流星落下的十四天里，主角**吴鸿韬**拉起了五个人，也解开了三个人各自的心结：十年前那场没有观众的观测会、一颗缺了三十一年的齿轮、一副再也跑不出十一秒二的腿。

### 章节一览

| 章 | 标题 | 一句话内容 |
| --- | --- | --- |
| 1 | 废部通知 | 废部信送达；张誉腾开出复社条件；评估人梁知奕登场 |
| 2 | 凑齐五个人 | 招人：尹博涛负责搬运，李俊辰负责海报 |
| 3 | 缺齿的赤道仪 | 李君浩入社；梁知奕用「署名」换一颗 3D 打印齿轮 |
| 4 | 雨 | 连日暴雨、士气崩盘；黎璘拿出两年的观测日志；邵清和登场劝退 |
| 5 | 签字 | 邵清和的过去揭开；前任社长魏思远带回原始图纸；顾问签字 |
| 6 | 一百二十七张海报 | 海报被撕；张誉腾坦白自己当年想进天文社；全员通宵重画 |
| 7 | 星屑祭典 | 十一分钟的放晴窗口，一百零三人到场，第一颗流星落下 |
| 8 | 猎户座下的约定 | 复社通过；天台上，十年的一句话终于说出口 |

---

## 1. 角色一览

| 角色 | 定位 | 名字颜色 | 性格关键词 | 立绘 ID | 表情目录 |
| --- | --- | --- | --- | --- | --- |
| **吴鸿韬** | 主角 · 天文社代理社长（高二） | `#6FA8FF` | 寡言、固执、嘴笨心细 | `wu_hongtao` | `assets/char/wu_hongtao_face` |
| 黎璘 | 女主 · 青梅竹马 · 图书馆委员 | `#FF8FB0` | 温柔、可靠、一个人记了两年云 | `li_lin` | `assets/char/li_lin_face` |
| 张誉腾 | 学生会会长 | `#E0B34C` | 公私分明、外冷内热 | `zhang_yuteng` | `assets/char/zhang_yuteng_face` |
| 梁知奕 | 转校生 · 物理竞赛部 | `#9B7BE0` | 毒舌、傲娇、用数据当盔甲 | `liang_zhiyi` | `assets/char/liang_zhiyi_face` |
| 尹博涛 | 田径队王牌（跟腱受伤） | `#F0724A` | 热血、直率、在找新的赛道 | `yin_botao` | `assets/char/yin_botao_face` |
| 李君浩 | 高一学弟 · 钟表店之子 | `#4CC0A8` | 怯懦、机械宅、手很巧 | `li_junhao` | `assets/char/li_junhao_face` |
| 李俊辰 | 高三 · 问题学生 | `#5A6B8C` | 冷淡、毒舌、画得一手好海报 | `li_junchen` | `assets/char/li_junchen_face` |
| 邵清和 | **新角色** · 物理老师 · 天文社顾问 | `#7FB3D5` | 倦怠、回避、十年前的那场雨 | `shao_qinghe` | `assets/char/shao_qinghe_face` |
| 魏思远 | **新角色** · 前任社长 · 天文系在读 | `#D98C5F` | 洒脱、念旧、带回原始图纸 | `wei_siyuan` | `assets/char/wei_siyuan_face` |

---

## 2. 立绘（身体）

- **规格**：480×900，PNG，背景透明，脚底贴图像底边
- **数量**：9 张（每个角色 1 张）
- **命名**：`assets/char/<角色ID>_body.png`

| # | 文件路径 | 状态 | 备注 |
| --- | --- | --- | --- |
| 1 | `assets/char/wu_hongtao_body.png` | 待制作 | 蓝白校服 + 旧外套，背一个帆布包 |
| 2 | `assets/char/li_lin_body.png` | 待制作 | 长发、围巾，手边常有书本 |
| 3 | `assets/char/zhang_yuteng_body.png` | 待制作 | 学生会臂章，领带系得很正 |
| 4 | `assets/char/liang_zhiyi_body.png` | 待制作 | 外套随性，手里总在转笔 |
| 5 | `assets/char/yin_botao_body.png` | 待制作 | 运动服，小腿贴着肌内效贴 |
| 6 | `assets/char/li_junhao_body.png` | 待制作 | 个子偏小，书包上挂齿轮钥匙扣 |
| 7 | `assets/char/li_junchen_body.png` | 待制作 | 衬衫敞开，肩挂画笔袋 |
| 8 | `assets/char/shao_qinghe_body.png` | 待制作 | 白衬衫 + 深色马甲，袖口卷起，胸前别教师证 |
| 9 | `assets/char/wei_siyuan_body.png` | 待制作 | 大学外套 + 围巾，背一个塞满图纸的旧登山包 |

---

## 3. 表情

- **规格**：480×900，PNG，透明，需与对应角色的立绘**严格对齐**（同一张脸的位置与比例）
- **命名**：`assets/char/<角色ID>_face/<表情>.png`
- **入场默认**：目录里存在 `normal.png` 时，角色入场会自动使用它

### 3.1 本剧本实际用到的表情（必须制作，共 31 张）

| 角色 | 需要的表情 | 小计 |
| --- | --- | ---: |
| 吴鸿韬 | `normal` `happy` `sad` `angry` | 4 |
| 黎璘 | `normal` `happy` `sad` `blush` | 4 |
| 张誉腾 | `normal` `happy` `sad` | 3 |
| 梁知奕 | `normal` `happy` `sad` `blush` | 4 |
| 尹博涛 | `normal` `happy` `sad` `angry` | 4 |
| 李君浩 | `normal` `happy` `sad` | 3 |
| 李俊辰 | `normal` `happy` `angry` | 3 |
| 邵清和 | `normal` `happy` `sad` `angry` | 4 |
| 魏思远 | `normal` `happy` | 2 |

### 3.2 建议一次性画全的套装（每个角色 6 张，共 54 张）

`normal` `happy` `sad` `angry` `blush` `surprise`

> 本剧本没用到 `surprise`，但张誉腾念出「一百零三个」、梁知奕报出「十一分钟」、李君浩听到「你写」这些桥段，都适合补一张惊讶脸。

---

## 4. 背景

- **规格**：1280×720，PNG（JPG 亦可）
- **数量**：10 张，其中 2 张已有
- **命名**：`assets/bg/<英文ID>.png`

| # | 场景 ID | 文件路径 | 状态 | 使用章节 | 画面要点 |
| --- | --- | --- | --- | --- | --- |
| 1 | 活动室 | `assets/bg/clubroom.png` | 待制作 | 1、2 | 旧教室改的社团活动室，黄铜星图，角落堆器材 |
| 2 | 走廊 | `assets/bg/corridor.png` | 待制作 | 1、6 | 旧校舍走廊，尽头挂钟；第 6 章需"海报被撕"前的版本 |
| 3 | 教室 | `assets/bg/classroom.png` | ✅ 已有 | 2 | 复用现有占位图，发传单场景 |
| 4 | 操场 | `assets/bg/playground.png` | 待制作 | 2 | 午休、梧桐落叶、远处跑道 |
| 5 | 天台 | `assets/bg/rooftop.png` | 待制作 | 2、8 | 傍晚 → 夜，铁丝网，风大 |
| 6 | 观测台 | `assets/bg/observatory.png` | 待制作 | 3、4 | 圆顶观测室内景，中央赤道仪，第 4 章需漏水状态 |
| 7 | 办公室 | `assets/bg/faculty_room.png` | 待制作 | 5 | 教师办公室，窗台晒着旧相册 |
| 8 | 雨中 | `assets/bg/rain_street.png` | 待制作 | 4 | 暴雨中的校园外景，冷色调 |
| 9 | 祭典夜 | `assets/bg/festival_night.png` | 待制作 | 7 | 青云祭夜景，灯笼、观测台圆顶、人群 |
| 10 | 街道 | `assets/bg/street.png` | ✅ 已有 | 8 | 复用现有占位图，尾声过场 |

---

## 5. 全屏 CG

- **规格**：1280×720（建议按 1920×1080 制作再缩，便于以后高清化）
- **格式**：静态图 PNG/JPG（显示约 3 秒自动继续）｜ 动图 GIF ｜ 视频 MP4（需系统有 ffmpeg）
- **数量**：9 张

| # | 文件路径 | 使用章节 | 画面要点 | 建议形式 |
| --- | --- | --- | --- | --- |
| 1 | `assets/cg/dismissal_notice.png` | 1 | 学生会废部通知特写，火漆印压在旧课桌上 | 静态 |
| 2 | `assets/cg/broken_telescope.png` | 3 | 锈迹斑斑的老赤道仪，镜筒霉斑，缺齿的传动齿轮 | 静态 |
| 3 | `assets/cg/gear_fixed.png` | 3 | 齿轮卡进传动轴、赤道仪重新转动，暖光 | 静态（可微动） |
| 4 | `assets/cg/rain_observatory.png` | 4 | 漏雨的圆顶内景，水顺着内壁淌，地上七个接水铁桶 | 静态 |
| 5 | `assets/cg/old_logbook.png` | 5 | 摊开的旧相册，照片里一群人挤在崭新的望远镜前 | 静态 |
| 6 | `assets/cg/poster_wall.png` | 6 | 走廊海报被撕光的狼藉，碎片堆在垃圾桶里 | 静态 |
| 7 | `assets/cg/clouds_parting.png` | 7 | 夜空云层从中间缓缓裂开一条墨蓝色的缝 | GIF 或 MP4 |
| 8 | `assets/cg/starry_night.png` | 7 | 满天星 + 第一颗流星划过 | GIF 或 MP4（记忆点） |
| 9 | `assets/cg/rooftop_promise.png` | 8 | 两个背影坐在天台边，脚下是祭典灯海，头顶银河 | 静态（可微动） |

---

## 6. 音频

> **现状（最新）**：作品**只用音效，没有 BGM**。
> 剧本里的 `play_bgm` / `stop_bgm` 全部用 `#` 注释掉了，`assets/audio/` 里只有音效文件。
> 想恢复音乐：`python3 tools/audio/make_audio.py bgm` 重新生成，再把剧本里的注释取消即可。

### 6.1 背景音乐 BGM（循环播放，WAV/OGG/MP3）

**当前状态：已弃用（脚本中已注释，文件已删除，生成器仍保留）**

| # | 文件路径 | 状态 | 使用章节 | 情绪 |
| --- | --- | --- | --- | --- |
| 1 | `assets/audio/bgm_daily.ogg` | ⏸ 已弃用 | 1、2 | 平淡的日常，轻微怀旧 |
| 2 | `assets/audio/bgm_tension.ogg` | ⏸ 已弃用 | 3、5、6 | 悬着心、对峙、倒计时 |
| 3 | `assets/audio/bgm_rain.ogg` | ⏸ 已弃用 | 4 | 压抑、湿冷、低气压 |
| 4 | `assets/audio/bgm_festival.ogg` | ⏸ 已弃用 | 7 前半 | 祭典喧闹、期待与紧张 |
| 5 | `assets/audio/bgm_starry.ogg` | ⏸ 已弃用 | 7 后半 | 温柔、辽阔、释然 |
| 6 | `assets/audio/bgm_warm.ogg` | ⏸ 已弃用 | 8 | 尾声，安心与一点点心动 |

### 6.2 音效 SE（单次播放）

| # | 文件路径 | 状态 | 使用章节 | 使用场景 |
| --- | --- | --- | --- | --- |
| 1 | `assets/audio/se_bell.wav` | ✅ 已有 | 1 | 走廊尽头的钟敲第七下 |
| 2 | `assets/audio/se_door.wav` | ✅ 已有 | 2、4、5 | 推开天台铁门 / 观测室门 / 办公室门 |
| 3 | `assets/audio/se_thunder.wav` | ✅ 已有 | 4 | 暴雨与闷雷 |
| 4 | `assets/audio/se_meteor.wav` | ✅ 已有 | 7 | 第一颗流星落下 |
| 5 | `assets/audio/se_click.wav` | ✅ 已有 | 3、5 | 齿轮卡入传动轴 / 钢笔落笔签字 |
| 6 | `assets/audio/se_shutter.wav` | ✅ 已有 | 卷二 2、支线《拍星星的人》 | 相机快门（两段机械声） |
| 7 | `assets/audio/se_page.wav` | ✅ 已有 | 卷一 3/5/7、支线《云志》《那场雨》 | 翻观测日志的纸声 |
| 8 | `assets/audio/se_wind.wav` | ✅ 已有 | 卷一 2/7、支线《十一秒二》《拍星星的人》 | 一阵风 |
| 9 | `assets/audio/se_clock.wav` | ✅ 已有 | 支线《齿轮与发条》 | 钟表滴答 |

**音频来源**：全部由 `tools/audio/make_audio.py` 用程序合成（numpy 生成波形 +
ffmpeg 编码 Vorbis），**不含任何第三方素材**，可以随意商用 / 替换。
BGM 每首是一个完整的和声循环（首尾各 0.6s 淡入淡出，可循环），
想要别的风格就改脚本里对应那首的和声进行重新跑一次：

```bash
python3 tools/audio/make_audio.py          # 全部重做
python3 tools/audio/make_audio.py bgm      # 只做 BGM
python3 tools/audio/make_audio.py se_rain  # 单做一条
./qlwt --check-assets                      # 素材自检（存在性 + 能否解码）
```

---

## 7. 字体

| 文件 | 状态 | 说明 |
| --- | --- | --- |
| `assets/fonts/NotoSansSC-Regular.ttf` | ✅ 已有 | 中文正文 / 对话字体，无需改动 |

---

## 8. 逐章素材对照

| 章节 | 背景 | 出场角色 | CG | 音频 |
| --- | --- | --- | --- | --- |
| 1 废部通知 | 活动室、走廊 | 吴鸿韬、黎璘、张誉腾、梁知奕 | 废部通知 | bgm_daily、se_bell |
| 2 凑齐五个人 | 教室、操场、天台、活动室 | 吴鸿韬、黎璘、尹博涛、李俊辰 | — | bgm_daily、se_door |
| 3 缺齿的赤道仪 | 观测台 | 吴鸿韬、尹博涛、李君浩、梁知奕 | 缺齿赤道仪、齿轮修好 | bgm_tension、se_click |
| 4 雨 | 雨中、观测台 | 吴鸿韬、尹博涛、黎璘、邵清和 | 漏雨的圆顶 | bgm_rain、se_thunder、se_door |
| 5 签字 | 办公室 | 吴鸿韬、邵清和、魏思远 | 旧相册 | bgm_tension、se_door、se_click |
| 6 一百二十七张海报 | 走廊 | 吴鸿韬、张誉腾、李俊辰、尹博涛、李君浩、黎璘 | 被撕的海报墙 | bgm_tension |
| 7 星屑祭典 | 祭典夜 | 全员 9 人 | 云开、流星 | bgm_festival、bgm_starry、se_meteor |
| 8 猎户座下的约定 | 街道、天台 | 吴鸿韬、黎璘 | 天台约定 | bgm_warm |

---

## 9. 汇总统计

| 类别 | 已有 | 待制作 | 合计 |
| --- | ---: | ---: | ---: |
| 角色立绘 | 0 | 9 | 9 |
| 表情（本剧本用到） | 0 | 31 | 31 |
| 背景 | 2 | 8 | 10 |
| CG | 0 | 9 | 9 |
| BGM | 0（已弃用） | 0 | 6（生成器保留） |
| 音效 | 9 | 0 | 9 |
| **合计** | **3** | **67** | **70** |

---

## 10. 制作优先级

1. **P0 · 能跑通即可**：9 张立绘 + 9 张 `normal` 表情 + 8 张背景 + 3 段 BGM（daily / tension / starry）。到这一步整条主线就有完整观感。
2. **P1 · 情绪到位**：其余 22 张表情 + 雨天与祭典的 BGM + `se_meteor` / `se_thunder` / `se_door` / `se_bell`。
3. **P2 · 记忆点**：9 张 CG，其中 `clouds_parting` 与 `starry_night` 建议做动图或多帧。

---

## 11. 命名与校验规范

- 立绘 `assets/char/<角色ID>_body.png`，表情 `assets/char/<角色ID>_face/<表情>.png`，全部小写英文 + 下划线。
- 背景 `assets/bg/<英文ID>.png`，CG `assets/cg/<英文ID>.png`，音频按 `bgm_` / `se_` 前缀。
- **路径禁止中文**；角色名、场景 ID、章节标题可以中文，只出现在 `.gal` 脚本里。
- 表情文件名必须与脚本 `change_face` 的实参完全一致（如 `happy.png`）。
- 素材缺失不会崩溃：引擎只在控制台打印 `WARNING: xxx not found` 并留空渲染，所以美术可以分批交付、边做边测。

### 校验命令

```bash
make check                                                     # 扫描 assets/scripts/*.gal，逐个跑三项自检
./qlwt --check-script   assets/scripts/star_festival.gal       # 能执行到 game_end
./qlwt --check-chapters assets/scripts/star_festival.gal       # 8 个小章节都能跑完
./qlwt --check-save     assets/scripts/star_festival.gal       # 中途存档/读档后能跑到结尾
./qlwt --selftest                                              # 带窗口自动跑一遍并截图
```

---

## 12. 第三部《星屑祭典 · 盛夏》

> 对应剧本：`assets/scripts/star_festival_summer.gal`（8 个小章节）
> 前两部：`star_festival.gal`（秋 · 8 章）、`star_festival_winter.gal`（冬 · 8 章）
> **原有角色全部沿用，未做任何修改**；第三部新增 1 位角色：沈砚。

### 12.1 故事梗概

天文馆开馆后的第一个暑假。一家文旅公司想把整馆承包下来做「星空灯光秀」，
每晚七点半到十点——正好压在英仙座流星雨的观测时间上。

吴鸿韬和社员们只有十三天：补完三十天的光害观测记录，办一场千人级的公开观测，
再让整个镇子愿意在一个晚上把灯关掉。

### 12.2 章节一览

| 章 | 标题 | 一句话内容 |
| --- | --- | --- |
| 1 | 灯 | 灯光秀试运营；沈砚登场；文旅合同与签字日浮出水面 |
| 2 | 沈砚的星图 | 一百四十天的手绘观测笔记；他因为结巴被嘲笑 |
| 3 | 光害计 | 李君浩做出光害计；天文馆屋顶亮度是城外的 46 倍；暗夜科普基地的条件 |
| 4 | 台风 | 台风「南屏」登陆，记录断档；沈砚在讲解台上被赶下台 |
| 5 | 三十天 | 全员通宵补记录；沈砚把本子放在门口；周老师的旧笔记 |
| 6 | 讲解 | 沈砚第一次讲完全场；两页纸换一次「关灯三十分钟」 |
| 7 | 英仙座 | 8 月 13 日，1126 人；全镇照明负荷下降 41% |
| 8 | 盛夏之后 | 暗夜科普基地挂牌；社长交接；高三的志愿表 |

### 12.3 新增角色

| 角色 | 定位 | 名字颜色 | 性格关键词 | 立绘 ID | 表情目录 |
| --- | --- | --- | --- | --- | --- |
| **沈砚** | 高一 · 天文馆志愿讲解员 | `#9DBF5C` | 结巴、认真、一个人记了一百四十天云 | `shen_yan` | `assets/char/shen_yan_face` |

表情使用：`normal` `happy` `sad` `blush` `surprise`（`angry` 也已生成备着）。

### 12.4 新增背景（6 张，1280×720）

| 场景 ID | 文件路径 | 画面要点 |
| --- | --- | --- |
| 夏日街道 | `assets/bg/summer_street.png` | 暑假正午的小镇街道，蝉、电线、旧公交站 |
| 天文馆展厅 | `assets/bg/planetarium_hall.png` | 灯光秀期间的展厅，彩灯与投影 |
| 夏日观测台 | `assets/bg/summer_observatory.png` | 夏夜圆顶内景，闷热，风扇与月光 |
| 城外山丘 | `assets/bg/summer_hill.png` | 日落后的山坡，镇子的橙色光晕 |
| 台风街 | `assets/bg/typhoon_street.png` | 台风中的街道，横着打的雨 |
| 夏日天台 | `assets/bg/summer_rooftop.png` | 夏夜天台，银河与远处小镇灯火 |

### 12.5 新增 CG（5 张，1280×720）

| 文件路径 | 使用章节 | 画面要点 |
| --- | --- | --- |
| `assets/cg/lightshow.png` | 1 | 被灯带缠满的圆顶，星空几乎看不见 |
| `assets/cg/shen_yan_notebook.png` | 2 | 摊开的手绘星图笔记本 |
| `assets/cg/light_meter.png` | 3 | 山坡上的两个人举着光害计，远处城镇光晕 |
| `assets/cg/perseid_shower.png` | 7 | 英仙座流星雨，前景是圆顶剪影 |
| `assets/cg/summer_promise.png` | 8 | 山坡上一排背影，满天流星 |

### 12.6 复用的素材

背景复用：活动室、走廊、教室、操场、天台、观测台、办公室、雨中、祭典夜、街道、春天天文馆、冬夜观测台。
CG 复用：`old_logbook.png`（周老师的旧笔记）。
音频走 `se_*.wav`（**9 个音效已生成**）；BGM 暂时不用，剧本里的 `play_bgm` 已注释。

### 12.7 素材生成

整套背景 / CG 用同一套管线生成，提示词与画风记录在脚本里：

```bash
# 换画风重出全部背景与 CG（会先把旧图备份到 gen_preview/old_style/）
/home/hoshino/project/ai-imagegen/cutout/.venv/bin/python tools/imagegen/make_scenes.py --force
# 只补缺失的
... make_scenes.py
# 新角色立绘 + 表情（出图 → 抠透明底 → 480×900）
... make_character.py shen_yan
# 旧画风 vs 新画风对照图
... make_scenes.py --sheet
```

画风说明：**animagine-xl-3.1 + 自然语言美术说明 + CFG 4.5 + 轻微去饱和（0.90）**。
早先用的是 tag 堆砌写法（`masterpiece, absurdres, ...`）+ CFG 5.5，出图高饱和高对比，
"AI 味"很重；对照实验（`gen_preview/style_compare.png`）后换成了现在的写法。

---

## 13. 支线（剧情树侧枝）

主线是"一条脊"，支线挂在主线某一章后面解锁；总纲见 [story_bible.md](story_bible.md)。

| 脚本 | 标题 | 章数 | 解锁条件 | 新增素材 |
| --- | --- | ---: | --- | --- |
| `side_eleven.gal` | 支线 · 十一秒二（尹博涛） | 3 | 卷一 第 3 章 | 无（复用操场 / 活动室 / 街道） |
| `side_yunzhi.gal` | 支线 · 云志（黎璘） | 3 | 卷二 第 8 章 | 背景 `assets/bg/library_old.png`（图书馆旧刊室） |

支线目前都只复用已有立绘 / 表情 / CG（`old_logbook.png` 等），所以没有额外生成成本。
以后加支线只要：写 `.gal` → 头部写 `series / branch side / order / subtitle / requires` →
缺什么素材就往 `tools/imagegen/make_scenes.py` 的表里加一行。

---

## 14. 卷四《终章》与卷一修订

### 14.1 卷四新增素材（5 张）

| 类型 | 文件 | 使用章节 | 画面要点 |
| --- | --- | --- | --- |
| 背景 | `assets/bg/town_dark.png` | 7 | 全镇熄灯后的镇子，只剩几盏灯，满天天河 |
| 背景 | `assets/bg/hill_night.png` | 4、7 | 夜里从山上看镇子的橙色光晕 |
| 背景 | `assets/bg/lens_shop.png` | 6 | 镇上磨镜铺内景，工作台上摊着半磨完的镜片 |
| CG | `assets/cg/lights_out.png` | 7 | 最后一排灯灭下去、银河浮出来的那一刻 |
| CG | `assets/cg/grad_orion.png` | 7、8 | 夏夜东南方的猎户座，回收第一部的天台约定 |

卷四没有新增角色，全员沿用现有立绘与表情；
新出现的"周文远（周老师）"**不出场**，只存在于旁白、刻字和那本编号 097 的日志里，
所以不需要立绘。

### 14.2 卷一《星屑祭典》第二稿

剧情上加了周文远暗线（详见 [story_bible.md](story_bible.md) 6.4.1），
素材与指令没有新增，只是把同一批背景 / CG / 表情用在了更狠的地方；
文件从 788 行涨到 966 行，`--check-chapters` 仍全绿。

---

## 15. 角色支线（7 条，23 章）

支线全部复用现有立绘 / 表情，只补了各自专属的场景：

| 脚本 | 标题 | 章数 | 解锁 | 新增素材 |
| --- | --- | ---: | --- | --- |
| `side_gear.gal` | 李君浩 · 齿轮与发条 | 3 | 卷一 第 3 章 | 背景 `clock_shop`、CG `gear_watch` |
| `side_eleven.gal` | 尹博涛 · 十一秒二 | 3 | 卷一 第 3 章 | 无 |
| `side_poster.gal` | 李俊辰 · 一百二十七张 | 4 | 卷一 第 8 章 | 背景 `art_room`、CG `wall_mural` |
| `side_rain.gal` | 邵清和 · 那场雨 | 3 | 卷一 第 8 章 | 无（复用雨中 / 观测台 / 城外山丘） |
| `side_yunzhi.gal` | 黎璘 · 云志 | 3 | 卷二 第 8 章 | 背景 `library_old` |
| `side_signature.gal` | 张誉腾 · 签字的人 | 4 | 卷二 第 8 章 | 背景 `student_council` |
| `side_star.gal` | 顾星禾 & 沈砚 · 拍星星的人 | 3 | 卷三 第 8 章 +《云志》第 3 章 | 背景 `darkroom`、CG `darkroom_print` |

> `side_star.gal` 是"树枝上的树枝"：它同时要求主线卷三和另一条支线《云志》，
> 用来验证引擎的多前置（AND）解锁。

卷四《终章》的解锁 = 卷三第 8 章 +《云志》第 3 章 +《十一秒二》第 3 章。

生成命令（缺什么补什么，已有文件自动跳过）：

```bash
/home/hoshino/project/ai-imagegen/cutout/.venv/bin/python tools/imagegen/make_scenes.py
```
