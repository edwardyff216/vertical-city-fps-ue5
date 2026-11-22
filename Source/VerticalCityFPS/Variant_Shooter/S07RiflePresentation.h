#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "S07RiflePresentation.generated.h"

enum class EVisibilityBasedAnimTickOption : uint8;
class AShooterCharacter;
class AShooterWeapon;
class UAnimSequence;
class UCurveTable;
class UCityLocomotion;
class UPoseableMeshComponent;
class UStaticMeshComponent;

/** First-person S07 body. The shooter weapon and its firing code stay authoritative. */
UCLASS()
class VERTICALCITYFPS_API US07RiflePresentation : public UActorComponent
{
    GENERATED_BODY()

public:
    US07RiflePresentation();
    void Equip(AShooterWeapon* Rifle);
    void Unequip(AShooterWeapon* Rifle);
    void NotifyShot();
    bool PlayMuzzleFlash(FName Kind, FRotator Direction);
    void BeginHandDraw();
#if !UE_BUILD_SHIPPING
    uint8 ObservedHandActions = 0;
#endif
    void SetArsenal(FName Kind);
    bool SampleMagazineContact(float& ErrorCm) const;
    float GetAimBlend() const { return Weapon && !bShoulder ? ADS : 0.f; }
    void ToggleShoulder();
    void FrameShoulder(float Delta,struct FMinimalViewInfo& View);
    void SetAiming(bool Value) { bAiming=Value; }
    bool TryGetMuzzleLocation(FVector& Out) const;

    UFUNCTION(BlueprintPure, Category = "S07")
    float GetRightGripErrorCm() const { return RightGripError; }
    UFUNCTION(BlueprintPure, Category = "S07")
    float GetLeftGripErrorCm() const { return LeftGripError; }
    UFUNCTION(BlueprintPure, Category = "S07")
    float GetReloadContactWeight() const { return ContactWeight; }

protected:
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    bool CreateView(AShooterCharacter* OwnerCharacter);
    void UpdatePose(float Delta);

    UPROPERTY(Transient) TObjectPtr<AShooterCharacter> Character;
    UPROPERTY(Transient) TObjectPtr<UCityLocomotion> Locomotion;
    UPROPERTY(Transient) TObjectPtr<AShooterWeapon> Weapon;
    UPROPERTY(Transient) TObjectPtr<UPoseableMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Receiver;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Magazine;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ActionPart;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Idle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Run;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Sprint;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Slide;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Reload;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Jump;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> BaseClip;

    struct FArm
    {
        int32 Upper = INDEX_NONE, Elbow = INDEX_NONE, Hand = INDEX_NONE;
        int32 Fingers[5][4] = {};
        FVector PalmOffset = FVector::ZeroVector;
        FQuat PalmFrame = FQuat::Identity;
    };
    FArm Arms[2];
    int32 Head = INDEX_NONE, Spine = INDEX_NONE, Hips = INDEX_NONE;
    EVisibilityBasedAnimTickOption SavedTickOption{};
    float GunMeshYaw = 0.f, MeshYaw = 0.f, BodyScale = 1.f, FloorOffset = 0.f;
    FName ArsenalKind;
    float SightHeight=10.88f;
    FVector HipGrip=FVector(50,22,-21);
    FVector SupportPoint=FVector(25,-1.65,2.7), MagazinePoint=FVector(14.5,-1.2,-5.7), BarrelPoint=FVector(50.88,0,9.92);
    bool bAiming=false, bShoulder=false;
    float ADS=0;
    float SavedViewmodelFOV = 70.f;
    UPROPERTY(Transient) TObjectPtr<UCurveTable> HandActions;
    float DrawPhase = 1.f, FirePhase = 1.f;
    float ClipTime = 0.f, BlendTime = 0.f, SlideWeight = 0.f, Kick = 0.f;
    float RightGripError = 0.f, LeftGripError = 0.f, ContactWeight = 0.f;
    bool bTemplateVisible = false, bWeaponVisible = false;
    FVector RightTarget = FVector::ZeroVector, LeftTarget = FVector::ZeroVector;
    TArray<FTransform> BaseLocals, UpperLocals, BlendFrom, Globals;
};
