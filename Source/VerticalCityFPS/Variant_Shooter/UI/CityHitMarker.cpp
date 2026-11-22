#include "CityHitMarker.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UCityHitMarker::UCityHitMarker(const FObjectInitializer& Initializer) : Super(Initializer)
{
    static ConstructorHelpers::FObjectFinder<USoundBase> Body(TEXT("/Game/VerticalCity/Combat/Audio/SC_BodyHit.SC_BodyHit"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Kill(TEXT("/Game/VerticalCity/Combat/Audio/SC_Kill.SC_Kill"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Blade(TEXT("/Game/VerticalCity/Audio/sword_hit"));
    BladeSound=Blade.Object;
    BodySound = Body.Object;
    KillSound = Kill.Object;
    SetIsFocusable(false);
    ForceVolatile(true);
}

void UCityHitMarker::ShowPass()
{
    if (Duration>0 && !bPass) return;
    bPass=true;
    Started=GetWorld()->GetTimeSeconds();
    Duration=.21f;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    GetWorld()->GetTimerManager().SetTimer(HideTimer,this,&ThisClass::Clear,Duration);
    UE_LOG(LogTemp,Verbose,TEXT("CITY_KATANA_PASS t=%.3f"),Started);
}

void UCityHitMarker::Confirm(bool bKill, bool bKatana)
{
    auto& Timers = GetWorld()->GetTimerManager();
    // Keep the killing blow legible if another enemy is struck in the same burst.
    if (!bKill && bLethal && Timers.IsTimerActive(HideTimer)) return;
    bPass=false;
    bPendingBladeSound |= bKatana;
    if (bKatana) UE_LOG(LogTemp,Verbose,TEXT("CITY_KATANA_CONFIRMED t=%.3f kill=%d"),GetWorld()->GetTimeSeconds(),bKill);
    bLethal = bKill;
    Started = GetWorld()->GetTimeSeconds();
    Duration = bKill ? 0.255f : 0.155f;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    Timers.SetTimer(HideTimer, this, &ThisClass::Clear, Duration);
    bPendingKillSound |= bKill;
    if (!Timers.TimerExists(SoundTimer))
        SoundTimer = Timers.SetTimerForNextTick(this, &ThisClass::PlayPendingSound);
}

void UCityHitMarker::PlayPendingSound()
{
    UGameplayStatics::PlaySound2D(this, bPendingBladeSound ? BladeSound : bPendingKillSound ? KillSound : BodySound, 0.8f);
    bPendingKillSound = bPendingBladeSound = false;
}

void UCityHitMarker::Clear()
{
    if (auto* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HideTimer);
        World->GetTimerManager().ClearTimer(SoundTimer);
    }
    bPendingKillSound = bPendingBladeSound = bPass = bLethal = false;
    Duration = 0.f;
    SetVisibility(ESlateVisibility::Collapsed);
}

void UCityHitMarker::NativeDestruct()
{
    Clear();
    Super::NativeDestruct();
}

int32 UCityHitMarker::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull,
    FSlateWindowElementList& List, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
    const int32 Base = Super::NativePaint(Args, Geo, Cull, List, Layer, Style, bEnabled);
    if (!GetWorld() || Duration <= 0.f) return Base;
    const float Age = GetWorld()->GetTimeSeconds() - Started;
    const float Alpha = FMath::SmoothStep(0.f, bLethal ? .068f : .055f, Duration - Age);
    if (Alpha <= 0.f) return Base;
    const float Recoil = FMath::Square(1.f - FMath::Clamp(Age/.060f, 0.f, 1.f));
    const float Gap = (bLethal ? 8.f : 7.f) + 6.f * Recoil;
    const float Length = (bLethal ? 18.f : 12.f) + 3.f * Recoil;
    const float Width = bLethal ? 3.6f : 2.8f;
    FLinearColor Ink(bLethal ? FColor(255,82,78) : FColor(255,253,243));
    Ink.A = Alpha;
    const FVector2f Center = FVector2f(Geo.GetLocalSize()) * 0.5f;
    const auto Paint = Geo.ToPaintGeometry();
    if (bPass)
    {
        for (float Side : {-1.f,1.f})
        {
            const float X=Side*(19.f+5.f*Recoil);
            TArray<FVector2f> Points{Center+FVector2f(X+Side*3,-4),Center+FVector2f(X,0),Center+FVector2f(X+Side*3,4)};
            FSlateDrawElement::MakeLines(List,Base+1,Paint,Points,ESlateDrawEffect::None,FLinearColor(0,0,0,.8f*Alpha),true,3.2f);
            FSlateDrawElement::MakeLines(List,Base+2,Paint,Points,ESlateDrawEffect::None,FLinearColor(1.f,.48f,.83f,Alpha),true,1.6f);
        }
        return Base+2;
    }
    for (const FVector2f& Ray : {FVector2f(-.7071f,-.7071f), FVector2f(.7071f,-.7071f), FVector2f(-.7071f,.7071f), FVector2f(.7071f,.7071f)})
    {
        const FVector2f A = Center + Ray * Gap;
        const FVector2f B = A + Ray * Length;
        FSlateDrawElement::MakeLines(List, Base+1, Paint, {A-Ray, B+Ray}, ESlateDrawEffect::None,
            FLinearColor(.015f,.018f,.022f,Alpha*.94f), true, Width+2.6f);
        FSlateDrawElement::MakeLines(List, Base+2, Paint, {A,B}, ESlateDrawEffect::None, Ink, true, Width);
        if (bLethal)
            FSlateDrawElement::MakeLines(List, Base+3, Paint, {A,A+Ray*4.5f}, ESlateDrawEffect::None,
                FLinearColor(1.f,.84f,.75f,Alpha), true, Width);
    }
    return Base+3;
}
