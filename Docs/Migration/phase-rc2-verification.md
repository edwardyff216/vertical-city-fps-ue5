# 垂直城寨 FPS · RC2 实机交付

玩家版 [Win64 Shipping ZIP](</D:/UE5/VerticalCityBuilds/RC2/VerticalCityFPS-Win64-Shipping-0.6.0-rc.2.zip>)，SHA-256 为 `1bc15743925dcc4f44afab289476a84fbd06fa01324e39ee9bd53c123fea45b3`。已安装版本可直接打开 [游戏程序](</D:/UE5/VerticalCityBuilds/RC2/Shipping/Windows/VerticalCityFPS.exe>)。UE 工程位于 [VerticalCityFPS.uproject](</D:/UE5/VerticalCityFPS/VerticalCityFPS.uproject>)。版本为 0.6.0-rc.2，构建环境为本机 UE 5.9 Preview 2。

## 玩法与美术

[正式单人玩法逐项对照](</D:/UE5/VerticalCityFPS/Docs/Migration/mechanism-parity-rc2.md>)。Godot 原作以恢复副本执行 51 项正式自动化，记录见 [原版结果](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Godot/verification.json>)。开发工作树 gameplay/iteration-02@42eb0cd，发布对照 main@0319f2f。独立试验场景和设计原案的范围见机制对照末段。

准备页可进入词条测试房，也可选择 S-07 或白角色。白角色使用原作蒙皮和七个材质槽，两角色共享动画。三个房卡继续预选后穿门，普通房两把武器各自抽取。首领按承诺种类将两件武器存入背包，再开启撤离门。格挡后的一次攻击可用于群体或贯穿，加成随该次攻击消耗。滑铲回弹、换枪疾行、吸弹、反射、烟幕和立墙已接入正常战斗。原作的像素序列与从已拥有 Fab 包挑选的 Niagara 同时使用。

## 实测与录像

| 测试 | 本版证据 |
|---|---|
| 八个方向组合，调试伤害清房、角色实体过门并撤离 | [八路线逐项结果](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/EightRoutes/20261121-014249-299252/report.json>) |
| 三套词条组合，控制器移动、开火、换弹、施放技能与正常敌人战斗 | [三条主线结果](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/Combat/20261121-014844-319932/report.json>) |
| 七把枪、握点、滑铲、换枪及刀技 | [30 / 60 / 120 fps 与白角色单独运行](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Arsenal/20261121-014224/results.json>) |
| 技能、普通 AI、大招选择与防御物理 | [已打包技能夹具及原始日志](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Skills>) |
| 110 个词条、实验房、重启与中断恢复 | [实测记录、游戏窗口截图](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Progression>) |
| 像素图、Niagara、默认与低画质 | [来源核对、原作参考和 UE 实机片段](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Visuals>) |
| Shipping 正常菜单，新档与 Godot 历史档 | [两次启动与结算](</D:/UE5/VerticalCityBuilds/RC2/Evidence/ShippingSmoke/result.json>) |

[三场首领实机合辑](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/ThreeBosses/three-bosses.mp4>)。完整原速对局如下。

- [UUU · spire](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/Combat/20261121-014844-319932/UUU/UUU-gameplay.mp4>)
- [DDD · breaker](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/Combat/20261121-014844-319932/DDD/DDD-gameplay.mp4>)
- [UDU · relay](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/Combat/20261121-014844-319932/UDU/UDU-gameplay.mp4>)

## 本机性能

1280 × 800，RTX 3060 Ti，默认画质、同一 UUU 种子与起始装备，关闭帧率上限和垂直同步，无录制。冷、热表示两个顺序启动的独立游戏进程。两次都保留显卡和操作系统缓存。准备页面、玩家点出战和战斗分别观察，所有帧的总耗时都保留。

| 运行 | 战斗 p50 / p95 ms | 首房首秒峰值 ms | 其余四房首秒峰值 ms | 全程峰值 ms | 请求出战到进入战斗 ms |
|---|---:|---:|---|---:|---:|
| 第1次 | 8.98 / 11.91 | 117.35 | 15.37, 28.63, 17.89, 13.9 | 1262.79 | 635.86 |
| 第2次 | 9.04 / 11.61 | 122.67 | 15.91, 14.82, 15.34, 18.98 | 636.97 | 866.78 |

RC1 首房两轮分别为 509.67 / 520.13 ms。RC2 在准备页构建实际首房并复用敌人与房壳，重复装饰进行实例化绘制。[逐帧统计和进入战斗的耗时](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Campaign/Performance/first-room.json>)。

范围为本机 Windows 单人游戏。自动控制器实战用于复现关卡、人机交互和画面，不能推算真人平均局长；实测配装未穷举所有词条组合。[源文件和整包哈希](</D:/UE5/VerticalCityBuilds/RC2/Evidence/manifest.json>)。

## 画面和实测边界

[原作 / UE 默认 / UE 低画质同步动作对照，25 秒](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Visuals/Godot-UE5-default-low.mp4>)。原作和 UE 在各自房间演示相同动作。低画质下 S-07 背部和偏转护板在暗处辨识度偏低。默认画质专门演示中，突进斩从标记到伤害为 1.000 秒。三场首领合辑为正常伤害、自动输入的游戏录像，包含实机音轨。全上路线遇到高处敌人时也使用步枪，首个突进未标记敌人。

Godot 51 项基线通过。UE 最终 Development 包通过 171 项技能断言、10 项敌人行为断言和 110 个词条的专项验证。三条代表路线完成正常战斗并撤离；八种方向组合额外用调试伤害复核分支。玩家 Shipping 包另外通过独立新档和导入档的正常菜单走查。[词条测试房配装截图](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Progression/Lab-20261121-014224-0d2c514a/loadout.png>)。

本机先后两次启动，准备页单帧峰值分别为 286.52 和 516.76 ms。战斗首秒、准备页峰值和点出战后的等待分别统计。
