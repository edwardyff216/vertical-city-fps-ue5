# 垂直城寨 FPS 项目健康审计

2026-11-21 开始取样，以文件指纹标识基线。主审对象是玩家现在实际开发的 Windows 单人 **UE 工程** [VerticalCityFPS.uproject](</D:/UE5/VerticalCityFPS/VerticalCityFPS.uproject>)。Godot 开发树、发布树、迁移文档、RC2 包和近期动作录像日志用于交叉验证。以下「当前」均指本次封存的源输入。工程级覆盖、运行结果、资产类型、原始证据位置和未验证事项见同目录《覆盖与验证》。逐文件 SHA-256 可用于复查资产是否一致。

## 结论

**可以继续开发这套玩法，建议在下一轮增加武器、技能或房型前安排一次有边界的整理。** 玩家已有从准备、出发、四个普通房、选择实体门、路线首领到撤离或阵亡结算的链路。局外装备事务、可重复的种子、实机验证入口都是继续开发的基础。最紧迫的问题是很难回答「玩家运行的包、验收通过的代码和今天编辑的工程，究竟是不是同一版」。现有 RC2 能拿回来，但不能用 RC2 成绩给今天的源码签字。

**本轮实测。** 当期源码成功构建并封包，新的 Shipping 在 D3D12 下从独立新档到准备阶段采集 300 帧、正常退出。另用现有编辑器游戏模块在隔离副本完成九局，覆盖八种方向，其中三类首领各有正常伤害实战；九局的过门、种子、终局发奖和胜利存档全部相符。存档中断后的三进程恢复也通过。默认启用 Kimodo 的冷副本在 240 秒内未进入路线测试。

第二个问题是**同一条规则由多个对象分别记一部分**。背包把两把枪视为两个物品，角色却用一个运行时枪 Actor 表示两把枪，拾弹又按 Actor 发放。AI 知道诱饵在哪里，但伤害仍发给真人。两者各自的局部测试均可能通过。这类问题比类的行数更值得先解决。

## 基线与工程边界

| 对象 | 实际状态 | 如何使用证据 |
|---|---|---|
| D:/UE5/VerticalCityFPS | 不在 Git 工作树，无可报告的分支、HEAD 或 Git 未提交列表。1096 个输入文件，427,217,734 字节，规范化清单 SHA-256 为 `e0090782758d6c32150fed581a4eef7586c628eff9bc6841e8482ded8d5cc272` | 清单包括源码、配置、Content、插件源、Build、SourceArt 和原有 Docs。排除引擎、编译缓存、用户存档和新审计报告。编辑器旧 DLL 单独识别 |
| D:/Game 仓库，一些点子/vertical-city-fps-gameplay-02 | Godot 开发工作树；历史原作测试记录引擎 4.4.1.stable.official.49a5bc7b6。gameplay/iteration-02，42eb0cd8754f5ac81e2d1c54d809423140a71e7f。264 个已跟踪状态条目，3321 个未跟踪文件 | 对照实际脚本、设计及未提交迭代。不能靠 checkout 该提交得到这整个目录 |
| 同级 vertical-city-fps-gameplay-02-github | 本地 main，0319f2fe00c40c2f6c52401f25e7d0eab50c8257，工作树干净。origin 指向 edwardyff216/vertical-city-fps-gameplay-02，HEAD 的树中无 .uproject | 本地历史 42eb0cd → 275ff07（角色、动画）→ 94ffb76（工坊、双页天赋）→ 0319f2f（路线首领、实体门）。未联网推断远端最新状态 |
| D:/UE5/VerticalCityBaseline-20261120 | UE 文件快照有 681 条哈希，另有 Godot bundle / LFS 资料。VerticalCityRestoreTest-20261120/project 是 Godot 恢复目录 | 旧版保护有效；未见当前 UE 全量作者源在另一台机器恢复演练的证据 |
| D:/UE5/VerticalCityBuilds/RC2 | Win64 Shipping ZIP 481,272,499 字节，本轮重算 SHA-256 为 1bc15743925dcc4f44afab289476a84fbd06fa01324e39ee9bd53c123fea45b3，与交付记录一致 | RC2 的 848 项输入与本轮有 802 项同路径同哈希、45 项同路径内容改变；本轮独有 249 项，RC2 独有一项 pyc。清单收录范围有差别，不能把差集全当新增功能 |

**技术栈。** EngineAssociation=5.9，安装引擎 E:/UE_5.9 的 Engine/Build/Build.version 为 5.9.0、CL 60738873。RC2 环境记录将其标作 Preview 2。一个 C++ Runtime 模块 VerticalCityFPS，Enhanced Input、CharacterMovement、Slate/UMG、Niagara、AudioMixer、JSON；使用内容包、骨骼网格和动画资产，另有 UFS 原始图集及 S07Casts.bin。Kimodo 配置仅对 Editor 目标启用，StateTree/GameplayStateTree 有模板代码；主线敌人用自定义 C++。地图默认入口见 Config/DefaultEngine.ini，构建与原始数据暂存见 Source/VerticalCityFPS/VerticalCityFPS.Build.cs。当前会话默认工作目录 D:/godot_zhanqi/zhanqi 与该 FPS 工程分开。

## 当前架构与实际数据流

下文源码路径相对 **D:/UE5/VerticalCityFPS/Source/VerticalCityFPS/**。配置、Content、Docs、Build 相对 **D:/UE5/VerticalCityFPS/**。「原作」路径相对上述 Godot 开发树。文件指纹附件给出每条实际目录路径。

```mermaid
flowchart TD
    Map[L_Campaign / DefaultEngine.ini] --> GM[CityCampaignMode / Phase]
    IO[CityProfileIO / JSON 主档与备份] <--> Prog[CityProgression / GameInstance 子系统]
    Prep[CityPreparation / 装备、工坊、双页天赋] <--> Prog
    Prep --> GM
    GM --> Shell[CityRoomShell / 掩体、三道门、拾取、撤离区]
    Shell -->|实际过门| GM
    GM -->|房间种子、波次| Enemy[CityCampaignEnemy / CityEnemyBolt]
    GM -->|清房、承诺奖励、胜败| Prog
    Prog -->|两槽物品、技能、节点| P[ShooterCharacter / 当前手与战斗状态]
    P --> Move[CityLocomotion / 输入与移动]
    P --> Guns[ShooterWeapon / CityKatana]
    P --> Skills[CityAbilities / 移动、持续场、飞行物与大招]
    Guns --> Enemy
    Skills <--> Enemy
    Guns --> Fx[CityWeaponEffects / Niagara / 打击反馈]
    Skills --> View[CitySkillVisuals / CityCastView]
    P --> Pose[S07RiflePresentation / 第一、三人称呈现]
    GM --> HUD[CityCampaignPanel / 战斗 HUD 与阶段交互]
    Lab[CityAffixLabMode / 草稿与试射] --> Prog
```

| 系统 | 正式入口与所有者 | 现状和依赖 |
|---|---|---|
| Core / 交互 | Campaign/CityCampaignMode.cpp 的 SetPhase、Enter、CleanupRoom、TogglePause；Progression/CityPreparation.cpp；Campaign/CityCampaignPanel.cpp | 模式持有本局深度、方向、敌人、奖励阶段和暂停标记。准备页也控制暂停及输入模式。过门提交房间，结算经持久化后才开放下一步 |
| Player | Variant_Shooter/ShooterCharacter.cpp、CityLocomotion.cpp、S07RiflePresentation.cpp | 角色路由主副手、技能、瞄准、生命和输入取消。移动组件实现跑、滑、贴墙探测和墙跳。枪姿、可见身体、摄像机和装备属性在不同入口取值 |
| Combat | Variant_Shooter/Weapons/ShooterWeapon.cpp、CityKatana.cpp、KatanaDashView.cpp；Abilities/CityAbilities.cpp、CityAbilitySmall/Ultimate/Fields/Flights.cpp | 两槽装备、七种枪、三刀普攻、缓冲、穿越后延迟斩、两个小技槽和一个大招槽。技能目录有 22 个小技、10 个大招；目录项数与玩家装备槽数不同。直伤、词条触发和场伤有各自入口 |
| World | Campaign/CityRoomShell.cpp、CityCampaignMode.cpp、Progression/CityProgression.cpp | 四普通房加一路线首领，三种遭遇、承诺奖励及实体三门。十个命名模板共用六种掩体布局和共享高台等几何；首房候选收窄。受种子控制，可复现 |
| AI | Campaign/CityCampaignEnemy.cpp、CityEnemyCues.cpp、CityEnemyBolt.cpp | 普通敌人、近战/射手/支援及三种路线首领。目标选点依赖技能组件，攻防与玩家类直接交互，移动使用局部探测。模板 Variant_Shooter/AI 的 StateTree/EQS 代码另有用途 |
| UI | Progression/CityPreparation.cpp、CityAffixLabEditor.cpp、Campaign/CityCampaignPanel.cpp、Variant_Shooter/UI/CityHitMarker.cpp | 准备、奖励、装备比较、技能树、试射编辑、打击标记。菜单的代次检查可拦截旧按钮。战利品清单可比较绝对值，对比交互深度低于原作 |
| Presentation | Abilities/CitySkillVisuals.cpp、CityCastView.cpp；Variant_Shooter/CityWeaponEffects.cpp、S07RiflePresentation.cpp；Campaign/CityRoomShell.cpp | 骨架、动作、程序化枪手对位、材质、图集、Niagara、镜头共存。粉晶拖尾/剑气/延迟斩有近期主线采集证据。Quinn/重定向与 Kimodo 试验资产不能等同于每种动作均在正式角色路径播放 |
| Data / QA | Progression/CityProgression.h/.cpp、CityItemData.cpp、CityItemCatalog.cpp、CityProfileIO.cpp、CityArsenalRow.cpp；Build/Tests/verify_campaign.py、内置 Automation 和各 Probe | 两装备槽、背包、零件、天赋双页、技能、角色、运行标记、奖励账本和试射覆盖。枪械表、词条/能力代码目录并存。普通输入、自动驾驶、调试伤害和录制各有不同证明范围 |

**依赖的强弱。** 对项目源内可解析的直接 include 作目录图，Abilities、Campaign、Progression、Variant_Shooter 四者组成双向可达的一组。例：Abilities → Variant_Shooter 19 条、反向 9 条；Campaign → Abilities 8 条、反向 13 条。这包含测试和 .cpp 实现依赖，不能据此断言存在头文件死循环。更强的证据是，改一次拾弹需要同步考虑装备实例 ID、当前手、缓存和房间拾取；改一次技能取消需要处理枪械定时器、动作状态与 UI。仅把大类分成多个 .cpp 无法解决这些具体依赖。

## 实现、历史验证与设计分歧

| 项目 | 可证实的实际行为 | 与历史设计的差异或待决策 |
|---|---|---|
| 随机选房 | CityCampaignMode.cpp 的门创建及提交与 CityRoomShell.cpp:294 的穿门检测联动；三门附遭遇和奖励，玩家穿门时确定选择 | 最新要求已接入。Docs/Migration/phase-rc2-verification.md 及 mechanism-parity-rc2.md 仍描述弹卡、预选后过门。后续不要据旧文档恢复弹窗 |
| 奖励与终局 | 武器房两件、准备/领奖 UI 中换装、按承诺奖励、前三次方向决定首领，击败后入包并撤离。能力等奖励阶段仍可有 UI | 选房面板与领奖面板功能不同。门上目前展示遭遇、奖励类；是否提前展示具体品质/词条倾向由设计决定。原作 scripts/room_pool.gd:122 提供更丰富的预告 |
| 首房与导航 | 十个模板 ID；首房从受限候选取，三类遭遇，通用坡道与掩体。AI 依赖局部方向和避障 | 原作 scripts/main.gd:146、room_navigation.gd:3。首房固定难度可服务教学；若期望十套独立地编或多层寻路，现有数据不能证明已交付 |
| 武士刀与角色 | 右键穿过目标，命中标记与延迟伤害分别发生，历史双目标记录相差 1.000 秒；切枪后的已刻痕结算有专项日志。角色选择、双手对位、第一/三人称技能和飞刃均有具体运行入口 | 旧迁移对照写作突进结束后计时。当前 CityKatana.cpp:296 逐目标从实际留痕起计时，符合最近的穿过后一秒结算要求；历史同帧双目标用例不能验证前后错时。动作是否像原片、人物是否穿模、所有武器与两角色的镜头遮挡仍需实机美术验收。试片或资源存在不足以证明当前每个动作都已替换 |
| 持久化与局外 | 双存档槽与备份、工坊批拆/重掷/制造、付费重置与免费切页、两套加点、阵亡基础配发、词条实验房均有调用链；110 个词条按 50 基础、50 特效、10 奥术组织 | 双枪拾弹、滑铲换匣、临时大招、仅余备份的恢复入口见问题表。不能把逐项词条验证推广到所有叠加组合 |
| 长期设想 | 现有入口和包以单人为目标。设计原案 docs/design-summary-2026-11-16.md 有多人及目标局长；正式迁移对照末段明确另列试验/原案 | 本轮未发现可作为已交付的多人主线、联网存档同步或真人局长验收。未发现这段单人阶段要求交付上述内容，不计缺陷 |

## 七个维度判断

| 维度 | 工程判断 |
|---|---|
| A 架构 | 地图、持久化、表现、输入已有边界；目标身份、过场暂停、装备实例与共享 Actor 是三个薄弱接口。主线模式同时承担波次、房间、交互和验证调度 |
| B 质量 | 字面技能 ID、地图名判断、伤害分支、定时器和手动取消有明确反例。程序化灰盒尺寸、单机局部 AI、少量常数属于合理原型取舍 |
| C 一致性 | 奖励提交和阵亡处理有跨阶段约束；准星、弹药、诱饵、技能取消等存在局部通过但组合错误的路径 |
| D 延展性 | 枪表、技能目录、词条 ID、种子和表现组件值得复用。增加一种具有特殊换弹、取消或锁枪规则的武器仍须跨角色、武器、技能和测试同时改动 |
| E 性能 | 旧包有实测帧时间和首房峰值；当前源码可见多次全场扫描、脉冲绘制和整页重建。没有当前同硬件 GPU/内存归因，不能据此决定对象池或删特效 |
| F 可靠性 | 有包、有历史测试、有存档备份机制；发布清单、当前开发身份和默认编辑器依赖未形成连续的可重建链。本轮编译、Cook 和功能测试分别列证 |
| G 设计一致性 | 实体三门是最新设计；旧卡牌文档、门的奖励透露粒度、首房随机范围、Boss 可打断窗口及临时大招生命周期必须显式区分，避免后续开发把旧规则重新接回 |

## 增加下一项内容的实际成本

| 新增内容 | 可以沿用 | 会牵动的边界 | 何时值得整理 |
|---|---|---|---|
| 一把常规枪 / 一把特殊枪 | DT_Arsenal 已实读，有伤害、射速与枪型入口，P/CityArsenalRow.cpp 按 Kind 查行 | 特殊装填、连发、枪口与左手对位还涉及 W/ShooterWeapon.cpp、S/S07RiflePresentation.cpp、CityArsenalProbe.cpp 和装备发放 | 下一把枪若有独立弹匣状态，先完成 03–05；只调既有行数字无需建立新框架 |
| 一个敌人与一个首领 | C/CityCampaignEnemy.cpp 的敌种、瞄准预警、受击与波次入口可用 | 新机制要同步 TakeDamage/Tick、Status、首领路由、传达提示和控制器测试。近战诱饵已展示只传坐标的局限 | 先解决 02、13，再抽第二个真正重复的敌人行为。单机小房现在没有强制切 StateTree 的证据 |
| 一个小技或大招 | A/CityAbilityCatalog.cpp 提供文案与配置，CityAbilitySmall、Ultimate、Fields、Flights 和表现组件已有职责分工 | 唯一 ID 在行为、效果、取消和验证中分别使用；新位移技能覆盖 Motion 时可能遗留姿态，新防御技能要处理在途余弹 | 先确立进入/退出与硬取消约定；保持战斗事件和 Niagara 表现分离，无需全量改写技能目录 |
| 一个战斗房 | C/CityRoomShell.cpp 布置几何，C/CityCampaignMode.cpp 选布局、遭遇与奖励 | 命名、掩体、刷怪点、门位置、可走坡道和简单 AI 几何假设需联调。复制正式地图还受地图名判断影响 | 多层新地编先做导航与门可达试验，再决定是否把模板拆成资产。沿用已验证的门/奖励合同 |
| 一个局外或战斗面板 | P/CityPreparation.cpp 的按钮代次、事务入口以及 C/CityCampaignPanel.cpp 的阶段展示可用 | 同时获得暂停、输入模式及焦点的多个面板可能留下不一致的关闭状态。每次背包操作重建长列表的代价未采样 | 若增加第二种重叠弹窗，先验证 12 的边界。制造结果定位可单独迭代，无需更换整套 UI |


## 分级问题清单

下表「确认」指静态调用链或文件/运行事实已核对。未做玩家复现的条目会明确指出。「风险」指触发条件或用户可见影响尚待验证。「建议」指工程改善。风险级别指优先顺序。**没有确认整个单人游戏普遍不能运行或正式存档普遍损坏的 P0。**

问题的详细证据、受影响接口、建议和回归用例见同目录《问题与验收》，编号在三份文档中通用。优先级不依赖是否来自 AI。


| 分级 | 问题编号及主题 |
|---|---|
| P1 | **01** 当前 UE 作者工程的发布身份与恢复；**02** 诱饵近战伤害对象；**03** 双枪拾弹；**04** 滑铲换匣绕过前置；**05** 技能锁枪后的连发余弹；**06** 编辑器冷启动与动画插件环境 |
| P2 | **07** ADS 散布规则；**08** 推进滑铲中断后的姿态；**09** 贯穿第二段判定；**10** 临时大招遮住存档配置；**11** 导入槽仅剩备份的入口；**12** 出发与暂停竞态；**13** Boss 防反提示窗口；**14** 场脉冲帧率差异；**15** 按地图名打开装备规则；**16** 制作用资源与正式烘焙边界；**17** 性能热点待量测 |
| P3 | **18** 武器比较与制造后定位 |

**可用的回归基础。** 本轮受控编辑器副本通过逐项词条、技能、敌人、工坊、存档恢复与双页天赋测试。清房门的广告、过门种子、终局发奖和胜利落盘再以路线日志和独立存档对照。正常伤害与快速路线、当前作者输入编译结果及默认冷启动失败记录都在验证附录，防止把测试捷径或历史录像算作当前真人验收。

## 优先顺序与保留项

| 顺序 | 工作范围 | 交付与停止条件 |
|---|---|---|
| ① 建立可恢复基线（01、06、16） | 冻结本次指纹，做一次完整 UE 作者工程的异地或独立盘恢复。记录 Preview 引擎 CL、VS/SDK、Kimodo 版本、Fab 来源、必需的原始图集。区分可再生成缓存、应版本化的资产和另行保管的制作源 | 从指定版本生成包，确认使用对应输入；默认制作环境和受控无 Kimodo 环境测试分别挂到同一版本。不要覆盖现有 RC2。达到这一点即可继续小步开发 |
| ② 建立跨系统回归用例，再修玩家能碰见的问题（02–05、07、09、13） | 对目标身份、装备实例、装填完成、连发中断先写失败用例，再各改一个接口。先由设计确认 ADS、贯穿比例及首领窗口 | 从正常操作入口覆盖双枪拾弹、滑铲换匣、诱饵近战、脉冲枪开盾、Boss 按 F。检查血量/弹量/动作时间戳和可见提示 |
| ③ 收拢状态交界（08、10–12、15） | 区分「当前会话」「持久配置」「测试房覆盖」，明确动作、暂停、输入焦点的拥有者。先修可复现交界，避免把所有逻辑搬入新 Manager | 进出准备、实战、领奖和实验房，热切换与冷重启结果一致；额外做一轮阵亡及存档写失败重试 |
| ④ 在有数据后做性能及资产整理（14、16–18） | 对固定种子采集当前渲染版和长背包基准；核对真实 cooked 资产集与正式引用，更新控制文档 | 可重放的 before/after 指标，掉帧可定位到 CPU/GPU/IO，合法的制作对照关仍能打开。没有可测收益的重构从本轮移除 |

**保留的设计。** P/CityProgression.cpp:178、200、378 和 P/CityProfileIO.cpp:212 先构造候选状态，验证临时档，保留可读备份，成功后发布状态。奖励票据与奖励本身同时提交，重复发放有防护；首领奖励落盘后才开放撤离，写失败有重试。P/CityAffixLabMode.cpp:122、170 的草稿/生效配置和 EndPlay 清理可以保留。P/CityAffixLabTest.cpp:225 检查正式档字节，尤其值得保留。实验室仍有一条公共 API 边界值得在下一次复用工坊时检查：IsPrep 看正式状态，SetCharacter 对 LabProfile 有专门检查，制造等入口依赖 IsPrep；当前实验室 UI 走自己的草稿，不据此判断正式档已污染。

**为期一个月的开发判断。**

1. **最大的问题**是当前 UE 作者工程缺少可回退到指定改动、可绑定测试包的版本身份。包可恢复和源工程可恢复是两种能力。
2. **能否看出 AI 临时补丁式演化？** 能看出相同风格的工程迹象：按地图名启用装备、制作采集开关进入正式模式源码、同一把枪同时按 Actor 与物品 ID 管理。许多采集逻辑已有非 Shipping 宏保护。没有 UE 提交历史，无法归因给 AI 或某位开发者。
3. **架构是否在变乱？** 局部已出现。装备实例、技能中断、目标选择三个交界均能给出具体反例。没有连续 UE 提交，无法定量断言每天都在恶化。存档事务和奖励链反而已有较清晰的约束。
4. **先整理什么？** 工程恢复链 → 战斗目标与装备实例 → 输入中断与状态退出 → 会话/永久配置的界面。先以行为回归约束改动。
5. **哪些不用动？** 保留四房加首领的房间循环、三实体门、确定性种子、存档事务与票据、试射草稿隔离、现有枪表及专项测试。无证据支持全面转 GAS、重写所有 AI 或替换整个 UI。
6. **按现状再做一个月的最大风险**是出现「局部全绿、真实组合出错、又无法定位是哪个版本引入」的回归。加入更多共享弹药的枪或有硬中断的新技能会放大该风险。
7. **现在如何选？** 先做上表 ① 和 ② 的小范围整理。准备页、实验房和三种首领已能承载下一次迭代，无需停下来重做整套游戏。

**阅读顺序。** 本文给出决策依据；同目录 [问题与验收](2026-11-21-findings.md) 用于排期，[覆盖与验证](2026-11-21-verification.md) 用于复跑、解释证据边界；[输入指纹清单](2026-11-21-input-sha256.json) 供程序比对。
