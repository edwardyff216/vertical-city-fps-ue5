#pragma once

#include "CoreMinimal.h"

struct FCityAbilityDef
{
    FName Id;
    FString Label;
    FString Description;
    float Cooldown;
    FColor Color;
    bool bUltimate;
    float Windup;
    float Duration;
};

struct FCitySkillSheet
{
    FName Id;
    bool bCast;
    int32 X, Y, Width, Height, Frames;
    float Fps;
};

struct VERTICALCITYFPS_API FCityAbilityCatalog
{
    static const TArray<FCityAbilityDef>& All();
    static const FCityAbilityDef* Find(FName Id);
    static const FCitySkillSheet* Sheet(FName Id, bool bCast);
};

