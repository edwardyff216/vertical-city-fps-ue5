# 垂直城寨 FPS · 正式单人机制对照

原作开发工作树位于 `D:/Game 仓库，一些点子/vertical-city-fps-gameplay-02`，分支 `gameplay/iteration-02`，提交 `42eb0cd`，含本地素材。正式玩法复核使用独立恢复副本与发布侧 `main@0319f2f`。目标工程位于 `D:/UE5/VerticalCityFPS`，以独立目录、版本 `0.6.0-rc.2` 和文件哈希标识。Godot 路径相对原作根目录。下表的 UE 文件位于 `Source/VerticalCityFPS`。两端采用各自的原生角色运动、碰撞和渲染管线。

| 玩家操作或规则 | 原作证据 | UE 入口、实现与验收方法 |
|---|---|---|
| 准备、出发、普通房、首领、撤离、阵亡 | scripts/main.gd、run_state.gd | Campaign/CityCampaignMode 与 Progression/CityProgression。校验错门、撤离区、胜负结算和中断恢复 |
| 选择房间及配装倾向 | scripts/room_pool.gd、room_navigation.gd | 四个普通房，清房按入口承诺发奖，自动呈现三张卡。实体穿门后提交，前三次方向决定第五房首领。普通武器奖励两件分别随机，终局按卡面种类生成两件、直接入包 |
| 遭遇和首领 | scripts/encounter.gd、enemy.gd、enemy_tactics.gd、route_boss.gd | Campaign/CityCampaignEnemy、CityEnemyBolt。高台、围攻、支援三类遭遇。全上对哨卫的瞄准与弱点，全下对冲锋的打断与恢复窗口，混合路线先破护盾或击倒支援者 |
| 移动与第一人称 | scripts/player.gd、data/tuning.json | Variant_Shooter/CityLocomotion、ShooterCharacter。走 700、跑 1050、滑铲初速 1400 cm/s，滑铲 0.7 秒，冷却 1 秒，低顶保持蹲姿。世界镜头按实际宽高比换算原作垂直 FOV；枪模采用独立视角参数 |
| 七把枪、双持槽、换弹、瞄准 | scripts/player.gd、data/tuning.json、scripts/art/ | ShooterWeapon、S07RiflePresentation 与 DT_Arsenal。实弹耗弹、散布、连发、换弹守恒。逐帧取掌骨和实际移动弹匣中心，检验装填接触。霰弹枪没有独立弹匣，用装弹动作与实际弹药数核验 |
| 武士刀 | scripts/sword_dash.gd、katana_aftercut.gd、combat_input_buffer.gd | Weapons/CityKatana、KatanaDashView。穿过敌人、受墙体阻挡、突进结束后一秒结算。区分切枪前尚未释放的输入缓冲与已刻下的延迟斩 |
| 两小技、一大招和动作时间轴 | scripts/active_skill_*.gd、blade_barrage.gd、flying_blade.gd、s07_attack_timeline.gd | Abilities/CityAbility*、CityCastView。验证出手、持续时间、范围、命中、能量与冷却。飞刃离手后旋转并从两侧夹击，出招期间使用第三人称 |
| 防御技能 | scripts/active_skill_defense.gd、active_skill_fields.gd、projectile.gd | CityEnemyBolt 与 CityAbilityFields。吸弹引力与捕获分半径、限额后返弹、反射受掩体阻挡，烟幕影响敌人选点，立墙检查部署与实体碰撞 |
| 随机掉落、六类房间被动 | scripts/run_state.gd、player.gd | CityProgression、ShooterCharacter。快速装填、强化躯体、滑铲回弹、换枪疾行、防反反击、蓄势反击。滑铲后首枪命中才增加弹匣，未命中也消耗机会；换枪疾行两秒且随等级对数增长 |
| 配装及词条 | data/、scripts/weapon_affixes.gd、weapon_effect_runtime.gd | Progression/CityItemCatalog、CityItemData、Variant_Shooter/CityWeaponEffects。基础 50、特效 50、奥术 10，按原档位生成。打击、护盾、状态叠加和触发链分别核对，使用三套组合在正常主线行进 |
| 局外经济和配置页 | scripts/run_state.gd、skill_tree.gd | CityProgression、CityProfileIO、CityPreparation。换装对比、批拆、制造、重抽、生存与枪械加点页、连续阵亡的基础配发，使用独立存档重启核对 |
| 词条测试房 | scripts/affix_lab*.gd、scenes/affix_lab.tscn | Progression/CityAffixLabMode、CityAffixLabEditor。从正常准备页进入，自选装备、词条和预设，使用独立状态。返回时核对正式存档字节 |
| 角色选择 | scripts/art/character_skin.gd、tests/character_selection_test.gd | 准备页选择 S-07 或白角色。白角色使用原作 white_body.glb、七张原作贴图和共享动画骨架，实测模型身高 180 cm。选择应用于持枪身体、刀技和大招镜头 |
| 环境与特效 | scripts/pixel_vfx.gd、frost_rim.gdshader、art/ 与原作序列清单 | CitySkillVisuals、CityRoomShell。像素图保留原帧序，UE 材质、Niagara 和房间绘制原生实现。Fab 实际引用的特效包为 Absorption FX 与 Slash Trail FX Elemental。走廊柱、货箱、支援机组源自 Sci-Fi Corridor - Low Poly Environment，保留模型比例并单独设置碰撞 |
| 存档兼容 | scripts/run_state.gd、tests/save_test.gd | Progression/CityProfileIO。原 Godot 档导入独立槽，写失败回滚，正式槽与实验房分离。Shipping 使用独立新档和旧档走普通菜单检查 |

## 如何阅读证据

恢复副本的 51 项测试结果见 [Godot 对照记录](</D:/UE5/VerticalCityBuilds/RC2/Evidence/Godot/verification.json>)。UE 的编辑器夹具、已打包 Development 测试与正常 Shipping 启动分别记录，最终运行结果见交付报告。房间随机算法按种子在本引擎内可复现，不承诺同一个数字种子生成跨引擎完全相同的房间序列。

Godot 独立地编试验、T01/T08、强制首房美术试片、开发观察器快捷键和多人设计原案另见[原始审查](</D:/Game 仓库，一些点子/vertical-city-fps-gameplay-02/docs/migration/godot-audit.md>)。组合作用采用代表性配装和针对性断言验证，逐项验收覆盖面见本版记录。