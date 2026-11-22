#pragma once

#include "Blueprint/UserWidget.h"
#include "CityHitMarker.generated.h"

class USoundBase;

UCLASS()
class UCityHitMarker : public UUserWidget
{
    GENERATED_BODY()

public:
    UCityHitMarker(const FObjectInitializer& Initializer);
    void ShowPass();
    void Confirm(bool bKill, bool bKatana=false);
    void Clear();

    UFUNCTION(BlueprintPure, Category="Combat")
    bool IsKillMarker() const { return bLethal; }

protected:
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
        FSlateWindowElementList& List, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY() TObjectPtr<USoundBase> BodySound;
    UPROPERTY() TObjectPtr<USoundBase> KillSound;
    UPROPERTY() TObjectPtr<USoundBase> BladeSound;
    bool bPass=false, bPendingBladeSound=false;
    FTimerHandle HideTimer;
    FTimerHandle SoundTimer;
    double Started = 0.0;
    float Duration = 0.f;
    bool bLethal = false;
    bool bPendingKillSound = false;
    void PlayPendingSound();
};
