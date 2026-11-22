#include "Progression/CityItemCatalog.h"
#include "Progression/CityArsenalRow.h"

const TArray<FCityChoice>& FCityItemCatalog::Nodes()
{
    static const TArray<FCityChoice> Entries = {
        {TEXT("endurance"), TEXT("体能训练"), TEXT("最大生命 +15")},
        {TEXT("parry_aid"), TEXT("借势回稳"), TEXT("成功防反恢复 8 点生命")},
        {TEXT("last_chance"), TEXT("绝境求生"), TEXT("每趟首次受到致命伤害时，以 1 点生命存活")},
        {TEXT("reload_drill"), TEXT("装填训练"), TEXT("枪械换弹耗时缩短 10%")},
        {TEXT("kill_feed"), TEXT("击破补弹"), TEXT("击杀时从备弹向手持枪械弹匣补 1 发")},
        {TEXT("slide_reload"), TEXT("滑铲换匣"), TEXT("换弹过程中发动滑铲，立即用备弹完成装填")}
    };
    return Entries;
}

FName FCityItemCatalog::Prerequisite(FName Id)
{
    const int32 Index = Nodes().IndexOfByPredicate([Id](const FCityChoice& N){return N.Id == Id;});
    return Index > 0 && Index != 3 ? Nodes()[Index-1].Id : NAME_None;
}

const FCityKind* FCityItemCatalog::Kind(FName Id)
{
    return Kinds().FindByPredicate([Id](const FCityKind& K){return K.Id == Id;});
}

const FCityAffix* FCityItemCatalog::Affix(FName Id)
{
    return Affixes().FindByPredicate([Id](const FCityAffix& A){return A.Id == Id;});
}

TArray<FName> FCityItemCatalog::Pool(FName Weapon, int32 Rarity)
{
    TArray<FName> Result;
    for (const FCityAffix& A : Affixes())
        if (A.Tier <= Rarity && (A.Family == TEXT("all") || A.Family == Weapon || (A.Family == TEXT("gun") && Weapon != TEXT("sword"))))
            Result.Add(A.Id);
    return Result;
}

FCityItem FCityItemCatalog::Generate(FName KindId, FRandomStream& Rng)
{
    FCityItem W;
    if (!Kind(KindId)) return W;
    W.Id = FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    W.Kind = KindId;
    const int32 Roll = Rng.RandRange(1,100);
    W.Rarity = Roll <= 60 ? 1 : Roll <= 85 ? 2 : Roll <= 95 ? 3 : Roll <= 99 ? 4 : 5;
    TArray<FName> PoolIds = Pool(W.Kind, W.Rarity);
    for (int32 I=1; I<W.Rarity; ++I)
    {
        const int32 Pick = Rng.RandRange(0, PoolIds.Num()-1);
        W.Affixes.Add(PoolIds[Pick]);
        PoolIds.RemoveAt(Pick);
    }
    W.DamageRoll = Rng.RandRange(9200,10800);
    return W;
}

float FCityItemCatalog::Amount(const FCityItem& W, FName Id)
{
    const FCityAffix* A = W.Affixes.Contains(Id) ? Affix(Id) : nullptr;
    return A ? A->Value : 0.f;
}

FCityWeaponStats FCityItemCatalog::Stats(const FCityItem& W)
{
    const FCityKind* K = Kind(W.Kind);
    if (!K) return {};
    FCityWeaponStats S = K->Base;
    if (const auto* Row=FCityArsenalRow::Find(W.Kind))
        S={Row->Damage,Row->Impact,Row->Interval,Row->Pellets,Row->Spread,Row->RangeMetres,Row->Magazine,Row->Reserve,Row->Reload};
    S.Damage *= (W.DamageRoll / 10000.f) * (1.f + .08f * (W.Level-1)) * (1.f + Amount(W,TEXT("damage")));
    S.Impact *= 1.f + Amount(W,TEXT("impact"));
    S.Interval /= 1.f + Amount(W,TEXT("rapid")) + Amount(W,TEXT("quick_blade"));
    S.Pellets += int32(Amount(W,TEXT("buckshot")));
    S.Spread *= 1.f - Amount(W,TEXT("precision"));
    S.RangeMetres *= 1.f + Amount(W,TEXT("reach")) + Amount(W,TEXT("long_barrel"));
    S.Magazine = FMath::RoundToInt(S.Magazine * (1.f + Amount(W,TEXT("magazine"))));
    S.Reserve = FMath::RoundToInt(S.Reserve * (1.f + Amount(W,TEXT("reserve"))));
    S.Reload *= 1.f - Amount(W,TEXT("reload"));
    return S;
}

FString FCityItemCatalog::Describe(const FCityItem& W)
{
    const FCityKind* K = Kind(W.Kind);
    if (!K) return TEXT("空槽");
    const FCityWeaponStats S = Stats(W);
    FString Out = FString::Printf(TEXT("%s   Lv.%lld   T%d\n伤害 %.1f × %d    射程 %.1f m    间隔 %.2f s"), *K->Label, W.Level, W.Rarity, S.Damage, S.Pellets, S.RangeMetres, S.Interval);
    if (S.Magazine > 0) Out += FString::Printf(TEXT("    弹药 %d/%d    换弹 %.2f s"), S.Magazine, S.Reserve, S.Reload);
    for (FName Id : W.Affixes)
        if (const FCityAffix* A = Affix(Id)) Out += TEXT("\n") + A->Label + TEXT(" · ") + A->Description;
    return Out;
}

