#pragma once
#include "CoreMinimal.h"

struct FCityItem
{
    FString Id;
    FName Kind;
    int64 Level = 1;
    int32 Rarity = 1;
    int32 DamageRoll = 10000;
    TArray<FName> Affixes;
};

struct FCityWeaponStats
{
    float Damage = 0, Impact = 0, Interval = 0;
    int32 Pellets = 0;
    float Spread = 0, RangeMetres = 0;
    int32 Magazine = 0, Reserve = 0;
    float Reload = 0;
};

struct FCityAffix
{
    FName Id;
    FString Label, Description;
    FName Family;
    int32 Tier;
    float Value;
    FName Event, Shape, School;
    float Damage, RadiusMetres, Cooldown;
    int32 SealGain;
    FString Category;
};
struct FCityKind
{
    FName Id, Frame;
    FString Label;
    FCityWeaponStats Base;
};
struct FCityChoice
{
    FName Id;
    FString Label, Description;
};
struct FCityItemCatalog
{
    static const TArray<FCityKind>& Kinds();
    static const TArray<FCityAffix>& Affixes();
    static const TArray<FCityChoice>& Skills();
    static const TArray<FCityChoice>& Ultimates();
    static const TArray<FCityChoice>& Nodes();
    static const FCityKind* Kind(FName Id);
    static const FCityAffix* Affix(FName Id);
    static TArray<FName> Pool(FName Kind, int32 Rarity);
    static FName Prerequisite(FName Id);
    static FCityItem Generate(FName Kind, FRandomStream& Rng);
    static FCityWeaponStats Stats(const FCityItem& Item);
    static float Amount(const FCityItem& Item, FName Id);
    static FString Describe(const FCityItem& Item);
};
