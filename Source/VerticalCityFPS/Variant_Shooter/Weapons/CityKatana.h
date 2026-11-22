#pragma once

#include "ShooterWeapon.h"
#include "Progression/CityItemCatalog.h"
#include "CityKatana.generated.h"

enum class EVisibilityBasedAnimTickOption : uint8;
class UKatanaDashView;
class UNiagaraComponent;
struct FMinimalViewInfo;
class AShooterCharacter;
class AShooterNPC;
class UBillboardComponent;
class UStaticMeshComponent;
class UTexture2D;
class UCurveVector;

UCLASS()
class VERTICALCITYFPS_API ACityKatana : public AShooterWeapon
{
    GENERATED_BODY()
    friend class FCityAffixCombatTest;
public:
    ACityKatana();
    virtual void ActivateWeapon(const FName& Tag) override;
    virtual void DeactivateWeapon() override;
    virtual void StartFiring() override;
    void RequestDash();
    virtual void StopFiring() override;
    void CancelAttackInput();
    bool IsPrimaryCutActive() const { return StrokeStart >= 0.; }
    void FrameDashView(float Delta, FMinimalViewInfo& View);
    UFUNCTION(BlueprintPure, Category="Katana")
    bool IsDashing() const { return bTravel; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void Tick(float Delta) override;
private:
    UPROPERTY() TObjectPtr<AShooterCharacter> Player;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Blade;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UKatanaDashView> DashView;
    UPROPERTY() TObjectPtr<UTexture2D> CutTexture;
    struct FDashMark
    {
        TWeakObjectPtr<UBillboardComponent> Seam;
        double Due;
    };
    TMap<TWeakObjectPtr<AShooterNPC>, FDashMark> Marks;
    TWeakObjectPtr<UNiagaraComponent> ActiveTrail;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GripHand;
    UPROPERTY() TObjectPtr<UStaticMesh> S07Hand;
    UPROPERTY() TObjectPtr<UStaticMesh> WhiteHand;
    void StopBladeTrail();
    bool bTravel = false;
    bool bBuffered = false;
    bool bEquipped = false;
    bool bPreviousMeshVisible = true;
    EVisibilityBasedAnimTickOption PreviousPoseTick{};
    UPROPERTY() TObjectPtr<UCurveVector> GripPath;
    UPROPERTY() TObjectPtr<UCurveVector> EdgePath;
    double StrokeStart = -1.;
    int32 StrokeIndex = INDEX_NONE;
    float StrokeDuration = 0.f;
    bool bHeld = false;
    bool bQueued = false;
    bool bContactPending = false;
    bool bLoopEntry = false;
    FTransform EntryPose;
    void BeginStroke(double Now);
    void AdvanceStroke(double Now);
    void ApplySlash();
    double NextDash = 0;
    double NextSlash = 0;
    double SwingStart = -10;
    double CutAt = -1;
    FCityItem ArmedItem;
    float ArmedDamage=0, ArmedImpact=0;
    float Remaining = 0;
    FVector Heading = FVector::ZeroVector;
    void StartDash();
    void StepDash(float Delta);
    void EndTravel(bool bArm);
    void ClearMarks();
    void StrikeMarks(double Now);
    void TraceMarks(const FVector& From, const FVector& To);
    bool CanDash() const;
    bool CanDashGround() const;
    bool CanReach(const FVector& Origin, AShooterNPC* Target) const;
    void PoseBlade(double Now);
};
