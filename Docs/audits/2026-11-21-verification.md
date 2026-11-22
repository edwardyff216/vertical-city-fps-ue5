# 垂直城寨 FPS · 覆盖与验证

工程 D:/UE5/VerticalCityFPS，无 Git 分支或 HEAD，输入指纹 e0090782758d6c32150fed581a4eef7586c628eff9bc6841e8482ded8d5cc272。两个独立副本从同一套作者输入复制并逐文件校验。测试档写入副本和审计临时目录。下面分别记录静态核验、已有编辑器二进制、从源编译的 Shipping 和历史实机。

## 方法与测试入口

- 从 Source、Config、Content、SourceArt、Build、Plugins、Docs、.vsconfig、.uproject 建清单。排除 .vs、编译产物、Saved、派生缓存、pyc 和新报告。Win64 Shipping 在 D:/UEAuditCity-pga2akdw 用 MSVC 14.44.35228、Windows 10.0.26100.0 SDK 从当前源编译成功。初次尝试在有空格和中文的临时目录发生 MSVC C1083，换简短 ASCII 路径后成功。首次使用 -skipbuildeditor 打包时副本少复制 VerticalCityFPSEditor.target，补齐原工程已有回执后重试。这两次准备问题单独记录。
- 编辑器运行使用原项目 UnrealEditor.modules 指向的 UnrealEditor-VerticalCityFPS-7400.dll，在副本使用隔离存档。该 DLL 与当前每个 .cpp 的编译对应关系不能仅凭文件名证明。路线采用种子 17967、-game -nullrhi -nosound -benchmark -fps=60；受控组加 -DisablePlugins=Kimodo。NullRHI 只验证逻辑。
- 项目既有入口包括 Build/Tests/verify_campaign.py（通过路线、门与种子、结算检查结果），Progression/Verify-CityProgression.ps1（跨进程中断恢复），Verify-CityAffixes.ps1、CityAffixLabTest.cpp（词条、实验室），Abilities/CityAbilityVerification.cpp（技能/敌人），Variant_Shooter/CityArsenalProbe.cpp（枪与动作），CityFrameProbe.cpp（帧时）。列出入口并不表示本轮把每种模式全部执行。
- 本轮在副本调用 UE AssetRegistry 和 DataTableFunctionLibrary。Campaign/CityCampaignMode.cpp:77 选 BP_ShooterCharacter、BP_ShooterPlayerController；前者的直接依赖包括 ABP_Unarmed、SKM_Manny_Simple、ABP_FP_Copy 和输入动作，后者包括该角色、输入映射和弹数 UI。L_Campaign 的静态直接依赖按 /Game 过滤后为空，运行时仍由 C++ 创建和加载内容。仅以地图的依赖闭包判断孤儿资产会漏报正式用料。

## 本轮专项结果

| 入口 | 结果 | 证据边界 |
|---|---|---|
| 默认编辑器 -game，Kimodo 开，UUU 快速路线 | 240 秒超时，末条是 Python 下载 numpy，未达到游戏断言 | 冷副本、同机并发编译；没有复现历史动画断言，也无法断言所有开发机均卡住 |
| VerticalCity.Weapons.AffixCombat.AllInstances | Automation Success；数值 50/50，效果 60/60。CITY_AFTERCUT_SWAP 标记 1、延后 1、实伤和期望均为 82.300 | 逐项及切枪后已刻痕的逻辑；未穷举组合、未复核画面 |
| -CityAbilityVerify | 32 次施放、171 条 CITY_SKILL_CHECK PASS、10 条敌人检查；技能及敌人汇总均为 0 failures | 能力夹具通过。该夹具检查诱饵瞄点，不能证明诱饵是实际受击者 |
| VerticalCity.Progression.RecoveryAndTwoPages、WorkshopAndSettlement | 两条均为 Automation Success | 覆盖恢复、工坊经济、双页和结算；导入槽仅余 .bak 时的正式 UI 入口另计 |
| Verify-CityProgression.ps1 的 Write → Recover → Clean | 本轮完成三进程中断重启，用例核实写入 active 后中断，重启按阵亡结算，再次正常退出。最终 active=false、parts=4、last_result=defeat，导入源文件哈希未变 | 使用脚本的临时副本，仅追加 -DisablePlugins=Kimodo。原始结果见 D:/UEAuditCity-pga2akdw/restart-proof/6438ca1facb04a478cd0a9f0052a363c/process-results.json |
| /Game 编辑器注册表 + DT_Arsenal | 918 个注册资产。其中 10 World、26 Blueprint、6 WidgetBlueprint、6 AnimBlueprint、117 AnimSequence、9 SkeletalMesh、81 StaticMesh、20 NiagaraSystem、2 DataTable | 表内实读 8 行：rifle、shotgun、sword、hand_cannon、scout、pulse、smg、sidearm。注册项数与磁盘文件数的口径不同。禁用 Kimodo 后有未知自定义版本提示 |
| 源码硬写路径抽查 | C++ 源码中双引号整段 /Game 路径词元去重 55 个，54 个存在对应包。余下 FieldModels/SM_ 核对为拼接前缀 | 不能覆盖 Blueprint、材质内部、动态字符串或每个运行时实例；未逐节点审阅全部蓝图、Niagara 和材质 |

## 历史实证与剩余盲区

| 证据位置 | 能支持的判断 | 尚未验证 |
|---|---|---|
| D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/EightRoutes/20261121-014249-299252/report.json 及 Combat/20261121-014844-319932/report.json，ShippingSmoke/result.json | 历史 Development 八方向快速和三场正常伤害；历史 Shipping 新档、导入档的正常菜单与结算 | 清单与当前源码不同，不能把旧版选门交互、录像或成绩直接作当前验收 |
| RC2/Evidence/Godot/verification.json、Progression/、Skills/、Arsenal/、Visuals/ | 原作 51 项，UE 逐项词条、代表组合、角色动作、跨进程持久化等已有回归资料 | 本轮未重跑全部 Godot 测试、每个动作和词条组合、全部真人交互 |
| RC2/Evidence/Campaign/Performance/first-room.json | RTX 3060 Ti、1280×800、UUU、无录像。两次战斗 p50 为 8.98/9.04 ms，p95 为 11.91/11.61 ms；首房首秒峰值 117.35/122.67 ms，全程峰值 1262.79/636.97 ms。请求出战到开战 635.86/866.78 ms | 确认旧包有阶段切换长帧。当前实时 AI、粒子、IO 的各自成本以及渲染版 p99 未测 |
| 近期 s07-prism-dash/take-vferdftn/test.log、swap-1kj_3hf0/test.log | 两个目标均于 4.1667 秒留痕，5.1667 秒结算；另有已刻痕后切枪记录 | 本轮无逐帧动作、美术或镜头评测。两目标特效可读性仍需原速真人反馈 |
| 资产与原案 | 默认地图与角色入口核对到实际 C++、武器行和角色蓝图依赖；Quinn、NativeIK、DeepMotion、S07 对照地图在 KatanaReview 下可识别 | 未逐一验材质、继承蓝图的每个 override、骨权重、碰撞、音量与 LOD。主线没使用某个模板入口不足以断言模板可删 |
| 设备、恢复和长期运行 | 只审本机 Windows 单人，旧 ZIP 重算哈希一致，全部测试隔离真实玩家档 | 无异机从当前源版本库完整恢复、冷机离线依赖、长时间内存曲线、低端 GPU、多人同步、手柄全矩阵或真实断电试验 |

原始证据基址见 [历史 RC2](</D:/UE5/VerticalCityBuilds/RC2/Evidence>)、[本轮日志](</D:/Game 仓库，一些点子/tmp/city-health-audit-20261121>)、[本轮构建与补充路线](</D:/UEAuditCity-pga2akdw>)、[粉晶实机](</C:/Users/OEM/.codex/visualizations/2026/11/15/01a1e0f6-14e8-70f1-8b38-ff17665a7ff2/s07-prism-dash>)。临时目录可能随机器维护而清理，关键结果与文件指纹随本报告保存。

## 本轮主线路线明细

每局的日志与独立 JSON 档交叉比对。检查 4 普通房 + 指定首领、12 个门承诺、4 次过门、方向和种子与实际房间一致、撤离后胜利落盘及背包末两把符合终局承诺。每次记录使用独立档，本轮没有覆盖所有随机种子。正常伤害组由控制器自动走位、开火和施法，快速组使用 99999 调试伤害。

|前三方向|战斗方式|实际首领|交叉检查|
|---|---|---|---|
|UDU|调试伤害|relay|6/6|
|DDD|正常伤害|breaker|6/6|
|UUU|正常伤害|spire|6/6|
|UUD|调试伤害|relay|6/6|
|UDD|调试伤害|relay|6/6|
|DUU|调试伤害|relay|6/6|
|DUD|调试伤害|relay|6/6|
|DDU|调试伤害|relay|6/6|
|UDU|正常伤害|relay|6/6|

## 内容与模块覆盖

全量文件计数、字节和已识别文本行按目录如下。源码/文本扫描用于定位与度量。人工沿调用链核对主线、战斗、数据、UI 和表现的重点函数。模板、开发录制路径、第三方插件没有做逐行语义审查。运行时组件并不等同于目录。

|目录组|文件|MiB|计入的文本行|
|---|---:|---:|---:|
|.vsconfig|1|0.00|0|
|Build|11|0.41|570|
|Config|5|0.02|265|
|Content/AbsorbtionFX|23|31.62|0|
|Content/ArrowTrail|27|44.64|0|
|Content/Characters|128|125.29|0|
|Content/FirstPerson|7|0.41|0|
|Content/Free_Magic|26|64.99|0|
|Content/Input|9|0.31|0|
|Content/Knife_light|17|22.21|0|
|Content/LevelPrototyping|29|2.73|0|
|Content/Low_Poly_Sci-Fi_Corridor|21|3.38|0|
|Content/NW_MuzzleFX|16|3.87|0|
|Content/SlashTrail_SoftTofu|24|4.98|0|
|Content/StylizedDomesShader|11|15.62|0|
|Content/Variant_Horror|15|0.50|0|
|Content/Variant_Shooter|37|3.68|0|
|Content/Vefects|11|1.60|0|
|Content/VerticalCity|199|63.09|0|
|Content/VerticalCityArt|5|1.50|0|
|Content/Weapons|27|13.90|0|
|Content/__ExternalActors__|278|1.36|0|
|Content/__ExternalObjects__|12|0.02|0|
|Docs|4|0.02|218|
|Plugins|31|0.56|2770|
|Source|13|0.01|505|
|Source/VerticalCityFPS/Abilities|15|0.15|3293|
|Source/VerticalCityFPS/Campaign|17|0.12|2671|
|Source/VerticalCityFPS/Progression|19|0.16|2800|
|Source/VerticalCityFPS/Variant_Horror|8|0.02|569|
|Source/VerticalCityFPS/Variant_Shooter/AI|10|0.05|1471|
|Source/VerticalCityFPS/Variant_Shooter|16|0.12|3070|
|Source/VerticalCityFPS/Variant_Shooter/UI|6|0.01|210|
|Source/VerticalCityFPS/Variant_Shooter/Weapons|15|0.07|2077|
|SourceArt|2|0.01|0|
|VerticalCityFPS.uproject|1|0.00|55|

较大的实现包括 ShooterCharacter.cpp（641 行）、S07RiflePresentation.cpp（593 行）、CitySkillVisuals.cpp（575 行）、CityCampaignMode.cpp（560 行）、CityKatana.cpp（468 行）、CityAbilityFields.cpp（467 行）、ShooterWeapon.cpp（448 行）。ShooterCharacter 涉及输入、装备、伤害和姿态，CityCampaignMode 涉及阶段、房间、菜单和调试。如果新增改动同时穿过这些职责，先补接口测试。行数单独不作为严重性证据。

## 本轮打包及运行

- **编译、烘焙、封包通过。** 从审计指纹对应源码生成 Win64 Shipping 可执行文件。UAT 在补齐现有编辑器回执后执行 BuildCookRun（cook、stage、pak、iostore、package、archive），Cook 汇总 0 errors、0 warnings，UAT 返回 0，BuildCookRun 1301.01 秒。Cook 使用已复制的开发机编辑器游戏模块，玩家 Shipping 可执行文件是本轮新编。未做全新引擎源码或全新编辑器目标编译。
- **新包正常渲染启动通过。** 直接运行归档内 VerticalCityFPS-Win64-Shipping.exe，1280×800、D3D12、RTX 3060 Ti、i5-13400，使用独立新档和独立 UserDir。CityBench 取得 300 帧均为阶段 0（Preparation），正式 JSON 档为 active=false、默认步枪及霰弹枪，进程返回 0。该次从启动到退出用时 65.9 秒，包含启动、采样及退出，不能作为进准备页的耗时。未在这次 Shipping 冒烟中用真人输入完成一局，前文九局路线来自受控编辑器副本。可执行文件 186,027,520 字节，SHA-256 为 3ad2781a5c59b34405b829540caf099bd848c2ebf1bd7b7a1a55b69c999f0763。
- **成品内容核对。** 对实际 VerticalCityFPS-Windows.utoc 用 UnrealPak -ListContainer 导出 CSV。KatanaReview 下有 28 个不同文件的容器记录，压缩数据项合计 1,147,828 字节。含 L_KatanaTripleCut、L_Quinn_DeepMotion、L_Quinn_NativeIK、L_QuinnFitting、L_S07_QuinnTripleCut、L_S07_TripleCut 六张 .umap，以及对照序列 LS_S07_TripleCut；另见 Kimodo/LS_RunHardStop.uasset。没有按引用去重或做移除前后的包体对照，不能把所有文件直接判为可删。项目主 .ucas 为 413,726,672 字节。
- **复查位置。** [本轮 Shipping 归档](</D:/UEAuditCity-pga2akdw/package/Windows>)、[IoStore 文件清单](</D:/UEAuditCity-pga2akdw/cooked-files.csv>)、[新包帧记录](</D:/UEAuditCity-pga2akdw/shipping-smoke/render.csv>)、[重启恢复结果](</D:/UEAuditCity-pga2akdw/restart-proof/6438ca1facb04a478cd0a9f0052a363c/process-results.json>)。本轮收尾时对原工程的 1096 个作者输入重算并与起始哈希匹配。
