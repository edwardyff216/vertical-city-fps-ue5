# 垂直城寨 FPS · 第四项特效还原与资产库存

2026-11-21。工程位于 D:/UE5/VerticalCityFPS，当前基线为 0.6.0-rc.1，UE 5.9 Preview 2。后续工作顺序为战斗与操作手感、技能与词条交互、掉落与局外成长、特效还原。第四项随对应机制一起验收。

## 已有素材与接入判断

| 来源 | 当前可用程度 | 第一处用途 |
|---|---|---|
| UE 工程 Content/VerticalCity/Abilities/SkillAtlas.png | 已在 Shipping 包校验且有实录。44 段原作技能序列合成一张图集 | 保留现有冰面与电弧的起手、命中时点，作为与新增效果对比的基准 |
| Godot art/vfx/pixel-combat | 30 张直接可导入的 PNG，须按 manifest 还原朝向和取帧 | hit_spark、kill_burst、dash_cut、katana_arc。将短促的命中闪光和移动中的刀光分别触发 |
| Godot art/vfx/weapon-affixes | 60 张 768×96 的八帧序列，可按原图直接导入。表现需要绑定现有词条触发事件 | 奥术刻印、连锁、爆发的命中点提示。多词条同时触发时核对遮挡 |
| Slash Trail FX Elemental | Fab 库候选，产品含 Niagara 武器拖尾、旋转斩击投射物和命中粒子。5.9 Preview 2 实装待验证 | 首试武士刀。沿现有动作窗口播放拖尾，收招后一秒的伤害事件再触发命中。飞刃只借轻拖尾，保留刀身自转与双侧轨迹 |
| Absorption FX | Fab 启动器可见，产品含十种默认持续吸收的 Niagara 效果，支持参数调整 | 以吸弹球实际作用半径缩放，被吸收时增加一股向心能量，在技能结束时收束 |
| Combat Magic Animations | Fab 动作候选，需给 S-07 重定向，再校准手部和施法事件 | 优先补缺失的第三人称施法姿势 |
| Ruined Modern Buildings Pack | 本机 VaultCache 有 UE 格式的下载记录与数据，约 2.9 GiB | 楼体、远景与封锁区候选；表面材质、比例、碰撞仍需验看 |
| Sci-Fi Corridor - Low Poly Environment、Construction PROPS、Realistic Lab. Laboratory Equipment | 在库存索引有记录，尚待本项目验看 | 连接走廊、仓库陈设、支援者实验室道具 |

首轮对照片采用同一个房间、武器和技能配置，正常速度移动和开枪，分别看低画质和默认画质。核对刀身轮廓、冰面远端、延迟命中和吸弹半径，同时看开火时准星是否被遮住。

[刀光包产品说明](https://www.fab.com/listings/a4b04697-30c1-4745-ae2f-a706278300bd) · [吸收包产品说明](https://www.fab.com/listings/81550176-a90e-4e07-8817-c8d1a1d3884b) · [施法动作产品说明](https://www.fab.com/listings/05d90ca6-d294-4b93-a1b7-232214057850) · [RC1 技能实录](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Skills/README.md>)

## 本机 Fab 库索引

以下条目取自 Epic 启动器本地商品目录，与本机已拥有内容索引按商品 ID 相交，只计 type/asset 类型的不同标题。启动器 Fab 库另行目视抽核。本机 Fab 下载数据库仅查到 Ruined Modern Buildings Pack 一条下载记录。未在本轮逐包安装、编译或打包，云端库存可能另有条目。

共 55 项。

- 2 in 1 Nanite Environment Bundle 1
- Abandoned Cathedral
- Abandoned Lighthouse Island ? Modular Environment Pack
- Absorption FX
- Advanced Asymmetrical Multiplayer Template
- Advanced Companion Drone System | AI & Multiplayer
- Advanced Flock System ? Multithreaded Fish AI and Reactive School Behavior
- Advanced Phone System V3 Replicated
- Ancient Temple Ruins - Modular Environment
- Armored Stone Golem - Dark Fantasy Boss
- Audius Music
- Celebrations All In One Bundle ? VFX & Material for Christmas Halloween Holidays
- Combat Magic Animations
- Construction PROPS
- Dark Knight - Male and Female
- DieselPunk
- Dodge It Component
- Dwarven City Modular Environment
- Enhanced Code Flow
- Environments Bundle 6 Maps
- Laketown
- Led Light Generator
- Level Bookmarks
- LXR - Light Detection
- Match 3 RPG Template
- Matte Painting Skybox Pack: Chaotic Skies
- Mechanic Girl
- Medieval Castle
- Medieval Modular Wall Pack - Kitbash Collection
- Medieval Village
- Metasounds Audio Math Utilities
- Modular Post Apocalyptic Desert Environment (Desert City,Desert Village)
- Modular Rural Cabins
- Modular Sci-Fi Indoor/Outdoor environment pack - Rocky Swampy Planet
- Necropolis - Graveyard Kit
- Pawnshop System
- PCG Layered Biomes
- Platformer 8 Underworld - Low Poly 3D Models Pack
- Realistic Lab. Laboratory Equipment
- Replicated Grab System
- ResourcesExpansions
- Robot 3
- Ruined Modern Buildings Pack
- SCI FI: MEGASPIKAN
- Sci-Fi Corridor - Low Poly Environment
- Sci-fi Mech
- Science Fiction Desert City Kit
- Slash Trail FX Elemental
- Stylized Fantasy Pottery & Vessels: 22 Low-Poly Game-Ready Assets
- Stylized Log Cabin
- Stylized Spruce Forest
- Tokyo Stylized Environment
- UniversalFunctionLibrary
- Voxel Plugin Installer
- Warrior
