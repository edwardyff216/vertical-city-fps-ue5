#include "Abilities/CityAbilityCatalog.h"

// Ported from active_skill_catalog.gd, expansion/skills.json and ultimate_catalog.gd.
const TArray<FCityAbilityDef>& FCityAbilityCatalog::All()
{
    static const TArray<FCityAbilityDef> Definitions = {
        {TEXT("echo_pin"), TEXT("回声钉"), TEXT("标记敌人 4 秒。枪击标记会向附近最多三个目标传导伤害。"), 9.00f, FColor::FromHex(TEXT("FF6EC7")), false, 0.00f, 0.00f},
        {TEXT("cinch_post"), TEXT("收束桩"), TEXT("投出一根桩，落地后收拢附近敌人并打断普通敌人的攻击。"), 14.00f, FColor::FromHex(TEXT("E4FF5C")), false, 0.00f, 0.00f},
        {TEXT("ricochet_disc"), TEXT("弹壁飞盘"), TEXT("掷出贴墙反弹的锯盘，穿透敌人；用枪打中飞盘可当场引爆。"), 8.00f, FColor::FromHex(TEXT("FFD166")), false, 0.00f, 0.00f},
        {TEXT("shrapnel_mine"), TEXT("碎片雷"), TEXT("把感应雷贴在墙或地面；敌人靠近或你开枪打它都会引爆。"), 10.00f, FColor::FromHex(TEXT("EF8354")), false, 0.00f, 0.00f},
        {TEXT("arc_lance"), TEXT("电弧枪刺"), TEXT("向准星发出一道瞬间电弧，打穿第一个目标并跳到附近两名敌人。"), 7.00f, FColor::FromHex(TEXT("6EC6FF")), false, 0.00f, 0.00f},
        {TEXT("overclock_round"), TEXT("超载弹匣"), TEXT("接下来6发子弹大幅增伤并穿透一名敌人。"), 12.00f, FColor::FromHex(TEXT("FF5D8F")), false, 0.00f, 0.00f},
        {TEXT("seeker_swarm"), TEXT("蜂群微弹"), TEXT("从肩部射出6枚追踪微弹，优先咬住准星附近的敌人。"), 11.00f, FColor::FromHex(TEXT("C77DFF")), false, 0.00f, 0.00f},
        {TEXT("frost_floor"), TEXT("冷凝地面"), TEXT("抛出冷凝罐铺出一片冰面：敌人减速，你在上面滑铲更远。"), 14.00f, FColor::FromHex(TEXT("9BF6FF")), false, 0.00f, 0.00f},
        {TEXT("smoke_veil"), TEXT("遮断烟幕"), TEXT("炸开一团烟幕：外面的敌人看不见你，里面的敌人打不准。"), 16.00f, FColor::FromHex(TEXT("A8A8C8")), false, 0.00f, 0.00f},
        {TEXT("shock_fence"), TEXT("电栅"), TEXT("在准星处横拉一道电栅，穿过的敌人受伤并瘫痪。"), 15.00f, FColor::FromHex(TEXT("4D96FF")), false, 0.00f, 0.00f},
        {TEXT("decoy_holo"), TEXT("诱饵全息"), TEXT("在准星处投出你的全息分身，吸引敌人火力，被打爆时炸开。"), 14.00f, FColor::FromHex(TEXT("72EFDD")), false, 0.00f, 0.00f},
        {TEXT("grapple_hook"), TEXT("钩锁"), TEXT("射出钩锁把自己拉向墙面；钩中敌人则拉到他面前并打晕。"), 9.00f, FColor::FromHex(TEXT("B5E48C")), false, 0.00f, 0.00f},
        {TEXT("phase_step"), TEXT("相位步"), TEXT("向移动方向瞬移一小段，穿过敌人，原地留下一个替你挨打的残影。"), 6.00f, FColor::FromHex(TEXT("E0AAFF")), false, 0.00f, 0.00f},
        {TEXT("vault_jet"), TEXT("喷射跃升"), TEXT("竖直喷射升空，随后缓降滑翔，滞空时开枪不扩散。"), 10.00f, FColor::FromHex(TEXT("FFB703")), false, 0.00f, 0.00f},
        {TEXT("slide_surge"), TEXT("滑铲推进"), TEXT("立刻进入一次加长加速的滑铲，撞飞路上的敌人并顺手换弹。"), 8.00f, FColor::FromHex(TEXT("90BE6D")), false, 0.00f, 0.00f},
        {TEXT("deflect_plate"), TEXT("偏转护板"), TEXT("在身前展开护板1.5秒，把打中的敌方子弹原路反弹回去。"), 12.00f, FColor::FromHex(TEXT("48CAE4")), false, 0.00f, 0.00f},
        {TEXT("bullet_sink"), TEXT("吸弹球"), TEXT("抛出一颗吸收敌方子弹的球，结束时把吸进去的子弹化作碎片射回。"), 15.00f, FColor::FromHex(TEXT("B388EB")), false, 0.00f, 0.00f},
        {TEXT("patch_kit"), TEXT("速修"), TEXT("边打边回血；回血期间没有受伤还会获得一层护盾。"), 18.00f, FColor::FromHex(TEXT("80ED99")), false, 0.00f, 0.00f},
        {TEXT("barrier_wall"), TEXT("掩体立墙"), TEXT("在准星处立起一面挡敌方子弹的单向墙，你的子弹可以从后面穿过去。"), 18.00f, FColor::FromHex(TEXT("5FA8D3")), false, 0.00f, 0.00f},
        {TEXT("magnet_reload"), TEXT("磁轨换弹"), TEXT("瞬间装满弹匣并提高射速；空弹匣时换弹还会炸开冲击波。"), 10.00f, FColor::FromHex(TEXT("FF9E00")), false, 0.00f, 0.00f},
        {TEXT("finisher_lens"), TEXT("处决视界"), TEXT("4秒内看穿残血敌人，打残血目标必定暴击，击杀返还冷却。"), 13.00f, FColor::FromHex(TEXT("FF006E")), false, 0.00f, 0.00f},
        {TEXT("turret_drone"), TEXT("随从炮台"), TEXT("把小炮台贴在墙上或地上，自动射击，并优先打你正在打的敌人。"), 20.00f, FColor::FromHex(TEXT("43AA8B")), false, 0.00f, 0.00f},
        {TEXT("barrage"), TEXT("飞刃齐射"), TEXT("九枚旋刃从左右绕出，合击瞄准区域"), 12.00f, FColor::FromHex(TEXT("ffcc79")), true, 0.38f, 0.12f},
        {TEXT("storm"), TEXT("雷刃跃斩"), TEXT("6秒内按G最多三次突进环斩 · 近敌/撞墙停止"), 16.00f, FColor::FromHex(TEXT("66bcff")), true, 0.32f, 6.00f},
        {TEXT("charge"), TEXT("破阵冲击"), TEXT("向前冲锋，接敌或撞墙爆发 · 最大18米"), 16.00f, FColor::FromHex(TEXT("81dfff")), true, 0.45f, 0.80f},
        {TEXT("anchor"), TEXT("锚定箭"), TEXT("命中生成紫色锚点，限制附近普通敌人6秒"), 14.00f, FColor::FromHex(TEXT("b984ff")), true, 0.42f, 0.18f},
        {TEXT("well"), TEXT("战斗领域"), TEXT("插剑生成8秒领域 · 圈内回血并强化普通武器"), 18.00f, FColor::FromHex(TEXT("ffd679")), true, 0.52f, 0.24f},
        {TEXT("beam"), TEXT("聚束轰击"), TEXT("引导2.4秒光束 · 鼠标转向 · G提前结束"), 14.00f, FColor::FromHex(TEXT("80d6ff")), true, 0.38f, 2.40f},
        {TEXT("nova"), TEXT("崩解核心"), TEXT("投出慢速能量球 · 碰撞爆发，掩体阻挡范围伤害"), 16.00f, FColor::FromHex(TEXT("d389ff")), true, 0.48f, 0.18f},
        {TEXT("hammer"), TEXT("炽锤姿态"), TEXT("6秒内左键投锤，最多4次 · G提前结束"), 16.00f, FColor::FromHex(TEXT("ff9b4d")), true, 0.40f, 6.00f},
        {TEXT("ice"), TEXT("霜脊震地"), TEXT("砸地向前生成三列冰脊 · 冻结普通敌人，不堵门"), 15.00f, FColor::FromHex(TEXT("85eeff")), true, 0.50f, 0.28f},
        {TEXT("frenzy"), TEXT("双刃狂袭"), TEXT("6秒双刃姿态 · 左键三段连斩 · G提前结束"), 16.00f, FColor::FromHex(TEXT("a3ff7a")), true, 0.36f, 6.00f},
    };
    return Definitions;
}

const FCityAbilityDef* FCityAbilityCatalog::Find(FName Id)
{
    return All().FindByPredicate([Id](const FCityAbilityDef& Def) { return Def.Id == Id; });
}

const FCitySkillSheet* FCityAbilityCatalog::Sheet(FName Id, bool bCast)
{
    static const TArray<FCitySkillSheet> Sheets = {
        {TEXT("echo_pin"), true, 0, 0, 32, 32, 4, 30.0f},
        {TEXT("echo_pin"), false, 128, 0, 32, 32, 8, 24.0f},
        {TEXT("cinch_post"), true, 384, 0, 32, 32, 6, 20.0f},
        {TEXT("cinch_post"), false, 576, 0, 32, 32, 4, 12.0f},
        {TEXT("ricochet_disc"), true, 704, 0, 48, 48, 6, 30.0f},
        {TEXT("ricochet_disc"), false, 992, 0, 96, 96, 10, 30.0f},
        {TEXT("shrapnel_mine"), true, 0, 96, 48, 48, 6, 30.0f},
        {TEXT("shrapnel_mine"), false, 288, 96, 96, 96, 10, 30.0f},
        {TEXT("arc_lance"), true, 1248, 96, 48, 48, 6, 30.0f},
        {TEXT("arc_lance"), false, 0, 192, 96, 96, 10, 30.0f},
        {TEXT("overclock_round"), true, 960, 192, 48, 48, 6, 30.0f},
        {TEXT("overclock_round"), false, 0, 288, 96, 96, 10, 30.0f},
        {TEXT("seeker_swarm"), true, 960, 288, 48, 48, 6, 30.0f},
        {TEXT("seeker_swarm"), false, 0, 384, 96, 96, 10, 30.0f},
        {TEXT("frost_floor"), true, 960, 384, 48, 48, 6, 30.0f},
        {TEXT("frost_floor"), false, 0, 480, 96, 96, 10, 30.0f},
        {TEXT("smoke_veil"), true, 960, 480, 48, 48, 6, 30.0f},
        {TEXT("smoke_veil"), false, 0, 576, 96, 96, 10, 30.0f},
        {TEXT("shock_fence"), true, 960, 576, 48, 48, 6, 30.0f},
        {TEXT("shock_fence"), false, 0, 672, 96, 96, 10, 30.0f},
        {TEXT("decoy_holo"), true, 960, 672, 48, 48, 6, 30.0f},
        {TEXT("decoy_holo"), false, 0, 768, 96, 96, 10, 30.0f},
        {TEXT("grapple_hook"), true, 960, 768, 48, 48, 6, 30.0f},
        {TEXT("grapple_hook"), false, 0, 864, 96, 96, 10, 30.0f},
        {TEXT("phase_step"), true, 960, 864, 48, 48, 6, 30.0f},
        {TEXT("phase_step"), false, 0, 960, 96, 96, 10, 30.0f},
        {TEXT("vault_jet"), true, 960, 960, 48, 48, 6, 30.0f},
        {TEXT("vault_jet"), false, 0, 1056, 96, 96, 10, 30.0f},
        {TEXT("slide_surge"), true, 960, 1056, 48, 48, 6, 30.0f},
        {TEXT("slide_surge"), false, 0, 1152, 96, 96, 10, 30.0f},
        {TEXT("deflect_plate"), true, 960, 1152, 48, 48, 6, 30.0f},
        {TEXT("deflect_plate"), false, 0, 1248, 96, 96, 10, 30.0f},
        {TEXT("bullet_sink"), true, 960, 1248, 48, 48, 6, 30.0f},
        {TEXT("bullet_sink"), false, 0, 1344, 96, 96, 10, 30.0f},
        {TEXT("patch_kit"), true, 960, 1344, 48, 48, 6, 30.0f},
        {TEXT("patch_kit"), false, 0, 1440, 96, 96, 10, 30.0f},
        {TEXT("barrier_wall"), true, 960, 1440, 48, 48, 6, 30.0f},
        {TEXT("barrier_wall"), false, 0, 1536, 96, 96, 10, 30.0f},
        {TEXT("magnet_reload"), true, 960, 1536, 48, 48, 6, 30.0f},
        {TEXT("magnet_reload"), false, 0, 1632, 96, 96, 10, 30.0f},
        {TEXT("finisher_lens"), true, 960, 1632, 48, 48, 6, 30.0f},
        {TEXT("finisher_lens"), false, 0, 1728, 96, 96, 10, 30.0f},
        {TEXT("turret_drone"), true, 960, 1728, 48, 48, 6, 30.0f},
        {TEXT("turret_drone"), false, 0, 1824, 96, 96, 10, 30.0f},
    };
    return Sheets.FindByPredicate([Id, bCast](const FCitySkillSheet& S) { return S.Id == Id && S.bCast == bCast; });
}
