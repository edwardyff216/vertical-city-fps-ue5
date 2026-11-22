#pragma once

#include "Components/ActorComponent.h"
#include "KatanaDashView.generated.h"

class AShooterCharacter;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAnimSequence;
struct FMinimalViewInfo;

UCLASS()
class UKatanaDashView : public UActorComponent
{
    GENERATED_BODY()
public:
    UKatanaDashView();
    void Begin(AShooterCharacter* Shooter, UStaticMeshComponent* FirstPersonBlade);
    UStaticMeshComponent* GetDashBlade() const { return bActive ? Sword.Get() : nullptr; }
    void Recover();
    void Reset();
    void Frame(float Delta, FMinimalViewInfo& View);
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<AShooterCharacter> Player;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Sword;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstBlade;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<USkeletalMesh> BodyAsset;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UStaticMesh> SwordAsset;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimSequence> Draw;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimSequence> Travel;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimSequence> Recovery;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Playing;

    bool Prepare();
    void Play(UAnimSequence* Clip, float Rate);
    void SetFraming(bool ShowBody);
    bool bActive = false;
    double StartTime = 0;
    double RecoveryStart = -1;
    float ArmLimit = 0;
};
