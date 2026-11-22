# 垂直城寨 FPS · UE 5.9 Win64 迁移验收

构建号 `0.6.0-rc.1`。本次交付面向 Windows 单人主线。项目和着色器按本机 Unreal Engine 5.9.0 Preview 2 构建，编辑器与玩家默认地图均为 `/Game/VerticalCity/Maps/L_Campaign`。

## 来源与恢复

| 对象 | 实际位置与版本 |
|---|---|
| UE 工程 | `D:/UE5/VerticalCityFPS/VerticalCityFPS.uproject`，独立工作目录，本版以构建号、源码和资源 SHA-256 定位 |
| Godot 开发工作树 | `D:/Game 仓库，一些点子/vertical-city-fps-gameplay-02`，分支 `gameplay/iteration-02`，提交 `42eb0cd8754f5ac81e2d1c54d809423140a71e7f`，含未提交素材与修改，已按目录快照保全 |
| Godot 发布基准 | 同级 `vertical-city-fps-gameplay-02-github`，分支 `main`，提交 `0319f2fe00c40c2f6c52401f25e7d0eab50c8257` |
| 可恢复副本 | `D:/UE5/VerticalCityBaseline-20261120`，含两份 Godot 源树、Git bundles、发布侧 LFS 对象和 UE 开工前快照。四份清单分别记录相对路径、大小和 SHA-256 |
| 恢复验证 | `D:/UE5/VerticalCityRestoreTest-20261120/project`，Godot 4.4.1 从独立用户目录启动。51 项测试通过。渲染实战 UUU 完成四普通房、首领和撤离，开火 135 次。Godot 采用固定 30 Hz 仿真，其帧时只作基线参考 |

[原项目五份审查文档](</D:/Game 仓库，一些点子/vertical-city-fps-gameplay-02/docs/migration>) · [Godot 恢复测试](</D:/UE5/VerticalCityRestoreTest-20261120/project/evidence/verification.json>) · [Godot 实战日志](</D:/UE5/VerticalCityBuilds/Evidence/Godot/UUU-232546-log.txt>)

## 验收范围

| 阶段 | 工程中的结果 | 验收方式 |
|---|---|---|
| 0 | Godot 玩法、脏工作树、DCC 素材与可恢复基准 | 逐文件校验、恢复副本、51 项自动化、渲染实战 |
| 1 | 第一人称移动、冲刺、跳跃、滑铲与下视身体，第一和第三人称切换 | 在 30、60、120 fps 夹具中实际检测低顶、起身、坡道、奔跑速度与取消输入 |
| 2 | 七种枪、武士刀、两小技一大招、受击反馈和掉落，八种武器的核心参数可用原生 Data Table 编辑 | 实际开火耗弹和换弹，刀技穿过目标 600 cm、遇墙停止、收招后 1.00 秒结算；换枪、菜单取消缓冲 |
| 3 | 四普通房加一首领房、十种房壳、三卡预选、对应门提交、三类遭遇 | 路线枚举结合实际跨门、错门阻挡、第五房奖励、撤离结算 |
| 4 | 三类首领、装备对比与换装、制造/批拆/重抽、两页技能树、独立旧档导入、战斗配装 | 真实主线、110 个词条逐项结算、损档和杀进程恢复 |
| 5 | S-07、七种枪身及独立装填零件、武士刀、35 段施放动作、技能图集、20 段武器与反馈音效、仓库美术构件 | 导入时核对包路径、尺寸及材质，渲染实战审看右下枪位；默认、低画质技能短片与游戏原声首领实录 |
| 6 | 本机 Win64 Development、Shipping、可分发的整包目录、对应构建证据 | 见下方候选包检查和性能表 |

[30/60/120 帧运动和武器记录](</D:/UE5/VerticalCityBuilds/Evidence/ArsenalClearance>) · [可编辑的武器表](</D:/UE5/VerticalCityFPS/Content/VerticalCity/Data/DT_Arsenal.uasset>) · [主线关卡](</D:/UE5/VerticalCityFPS/Content/VerticalCity/Maps/L_Campaign.umap>)

## 工程入口

启动编辑器打开 `L_Campaign`。在 World Outliner 选中 `Campaign Room Preview`，可在 Details 的 Campaign Preview 中切换十种布局和三种首领场地。编辑器预览与游戏运行共用同一房壳生成逻辑，玩家进入关卡时只生成运行时房间。武器射速、伤害、装填与持枪视图参数见 `/Game/VerticalCity/Data/DT_Arsenal`。主线规则、成长和技能分别位于 `Source/VerticalCityFPS/Campaign`、`Progression`、`Abilities`。

用 `Build/Package-City.ps1 -Configuration Development` 或 `-Configuration Shipping` 复现构建。可通过 `-EngineRoot` 和 `-ArchiveRoot` 指定本地引擎与产物位置。烹制包含 EFIGSCJK 字体、原作动作和图集、PixelFX 的 MIT 署名及 Windows x64 运行库。详情见 [打包脚本](</D:/UE5/VerticalCityFPS/Build/Package-City.ps1>)、[玩家操作](</D:/UE5/VerticalCityFPS/Build/README-zh-CN.md>)。

## 候选包实测

- [八种方向组合](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Campaign/EightRoutes/20261120-235729-678952/report.json>) 全部检查错门、实体跨门、房卡绑定、五房奖励和撤离。矩阵以调试伤害缩短清房，角色实际移动过门。
- [三种首领实战](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Campaign/Combat/20261121-000438-931756/report.json>) 各自用常速玩家控制器驱动正常移动、开火和换弹，真实敌人行动并可造成伤害。从准备、通过四个普通房，到第五房击败首领和撤离均通过。[72.6 秒三场带游戏原声的首领合集](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Campaign/ThreeBosses/three-bosses.mp4>)。
- [武器和运动夹具](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Arsenal/results.json>) 三档帧率各 43 项；[技能包内证据](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Skills/README.md>) 32 招与 131 条断言；[110 个词条与局外回归](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Progression/verification.json>) 逐项通过。测试使用真实主线出战初始化，非主线 GameMode 的技能夹具首跑缺少装备初始化，两个枪械相关断言失败，原始日志保留在 Skills 中。
- Win64 Development 在三次独立启动中完成工坊与换装、保存、强制中断、损档回退、连续阵亡配发和读档退出。右下枪位经过画面复核，两档技能短片均为固定 30 fps 原速无声。[技能回归与四片](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Skills/README.md>)。
- Shipping 通过正常菜单用新档和导入的 Godot 实战旧档各进入一房，从暂停菜单结算并退出。导入后背包 10 件、主副手各 2 级、3 个技能点。导入后重算了实际装备和主动技能，原 Godot JSON 两次校验相同。[两次运行记录](</D:/UE5/VerticalCityBuilds/RC1/Evidence/ShippingSmoke/result.json>)、[导入界面](</D:/UE5/VerticalCityBuilds/RC1/Evidence/ShippingSmoke/shipping-Legacy-import.jpg>)、[新档实机](</D:/UE5/VerticalCityBuilds/RC1/Evidence/ShippingSmoke/shipping-Fresh-combat.jpg>)。

## 同机性能

Intel Core i5-13400、RTX 3060 Ti 8 GB、约 32 GB 内存、D3D12。1280 × 800、同一 UUU 种子、原始默认画质，不录视频，无垂直同步和帧率上限。冷启动指首轮新游戏进程与隔离配置，热启动指紧接着新开进程复跑同路线。保留系统和显卡已有缓存。表中总帧时为正常战斗阶段第 95 百分位，各线程可并行工作。

| 轮次 | 战斗 p50 / p95 毫秒 | 游戏 / 渲染 / GPU p95 毫秒 | 整局进程 / 贴图内存峰值 MiB | 跨门进入四个后续房间后首秒最大帧时 毫秒 |
|---|---:|---:|---:|---:|
| 冷启动 | 8.88 / 11.31 | 5.95 / 10.90 / 7.92 | 1699 / 938 | 20.7、24.8、27.6、22.2 |
| 热启动 | 9.07 / 11.58 | 6.01 / 11.33 / 7.97 | 1698 / 938 | 20.0、24.8、20.6、18.8 |

逐帧 CSV 与每次过门的数据均保留。首次进入战斗的一秒内，两轮分别出现 509.67 与 520.13 毫秒的峰值；后续过门峰值为 27.55 与 24.79 毫秒。两轮整局分别有 22 帧和 18 帧超过 16.67 毫秒。首样本渲染线程计数初始化异常，统计时已剔除该线程大于 1000 毫秒的值，总帧时保留全部样本。Godot 恢复版同机渲染共采集 2407 帧，帧时中位数 5.78 毫秒、p95 为 12.48 毫秒，固定仿真步长、房间表现和渲染管线均不同。[UE 完整统计](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Campaign/Performance/frame-analysis.json>)。

## 交付与适用边界

- [开始游戏](</D:/UE5/VerticalCityBuilds/RC1/Shipping/Windows/VerticalCityFPS.exe>)，或解压[完整玩家 ZIP](</D:/UE5/VerticalCityBuilds/RC1/VerticalCityFPS-Win64-Shipping-0.6.0-rc.1.zip>) 后打开 Windows 文件夹下的同名程序。ZIP 逐项 CRC 已通过，SHA-256 为 `c95ddd82c4b7ddc2980c39efc5da217e801a6cfa857cc44c78e1b4dd37cbc16f`。
- [构建与内容 SHA-256](</D:/UE5/VerticalCityBuilds/RC1/Evidence/manifest.json>) 记录 Development 和 Shipping 的独立 Win64 二进制、打包内容与 UE 输入文件。[原作音效、模型及武器表导入记录](</D:/UE5/VerticalCityBuilds/RC1/Evidence/Assets>)。
- 当前适合单人内部试玩和继续地编。普通敌人与部分场地保留灰盒造型；吸弹、反射、烟幕及立墙行为使用 UE 近似实现。自动化覆盖单个词条，未穷举全部叠加顺序；手部握点抽样及技能画面通过不等同于所有动作逐帧美术定稿。准备页窄标签在 1280 × 800 会竖排。
- 本次仅在这台已配置 UE 的 Windows 电脑验证。第二台干净系统、其他显卡、后续正式版引擎、合作模式、独立实验测试房和全部美术资源的对外再发布权均未作发行认证。Godot 恢复备份与 UE 项目位于同一台机器。
