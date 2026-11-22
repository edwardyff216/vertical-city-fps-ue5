#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Abilities/CityAbilityCatalog.h"
#include "CityAbilities.generated.h"

class AShooterCharacter;
class AShooterNPC;
class ACityEnemyBolt;
class UCityProgression;
class UCitySkillVisuals;
class UCityCastView;
struct FMinimalViewInfo;

struct FCitySkillFlight
{
    FName Id;
    FVector Origin, Point, Direction, Side;
    FVector VisualOffset=FVector::ZeroVector;
    TWeakObjectPtr<AShooterNPC> Target;
    TMap<TWeakObjectPtr<AShooterNPC>, float> NextHits;
    TArray<FVector> Path;
    float Speed = 0, Remaining = 0, Age = 0, Delay = 0, Spin = 1;
    int32 Bounces = 0, NextPoint = 1;
};

struct FCitySkillField
{
    FName Id;
    FVector Point, Direction;
    TWeakObjectPtr<AShooterNPC> Follow;
    TMap<TWeakObjectPtr<AShooterNPC>, float> NextHits;
    float Life = 0, Duration = 0, Radius = 0, Pulse = 0, Health = 0;
    TWeakObjectPtr<AActor> CollisionWall;
    int32 Stored = 0;
    bool bTriggered = false;
};

UCLASS(ClassGroup=(City), meta=(BlueprintSpawnableComponent))
class VERTICALCITYFPS_API UCityAbilities : public UActorComponent
{
    GENERATED_BODY()
public:
    UCityAbilities();
    UFUNCTION(BlueprintCallable) void CastPrimary();
    UFUNCTION(BlueprintCallable) void CastSecondary();
    UFUNCTION(BlueprintCallable) void CastUltimate();
    UFUNCTION(BlueprintCallable) bool TryCast(FName Id);
    UFUNCTION(BlueprintPure) float GetCooldown(FName Id) const;
    UFUNCTION(BlueprintPure) float GetEnergy() const { return Energy; }
    UFUNCTION(BlueprintPure) FName GetCastPhase() const { return Phase; }
    UFUNCTION(BlueprintPure) FName GetCastingId() const { return Ultimate; }
    UFUNCTION(BlueprintPure) int32 GetLiveProjectiles() const { return Flights.Num(); }
    UFUNCTION(BlueprintPure) int32 GetLiveFields() const { return Fields.Num(); }
    UFUNCTION(BlueprintPure) int32 GetBladesFired() const { return BladesFired; }
    UFUNCTION(BlueprintCallable) void ClearRoomEffects();

    bool HandlePrimaryAttack();
    bool SelectSessionUltimate(FName Id);
    void NotifyKill(FName Kind);
    void AddWeaponEnergy(float Amount) { Energy=FMath::Clamp(Energy+Amount,0.f,100.f); }
    void OnGunShot();
    float WeaponDamage(AShooterNPC* Enemy, float Damage, bool bMelee, bool bAttackBonusApplied=false);
    float ConsumeWeaponBuffs(bool bMelee);
    void OnWeaponHit(AShooterNPC* Enemy, float Damage, bool bMelee, bool bLethal);
    float ModifyFireInterval(float Base) const;
    float ModifySpread(float Base) const;
    float FilterIncomingDamage(float Damage, AActor* Source);
    bool GetEnemyAimPoint(AActor* Enemy, FVector ActualTarget, FVector& OutAimPoint) const;
    float EnemySpeedScale(const AActor* Enemy) const;
    FVector BendBolt(FVector Point, FVector Velocity, float Delta) const;
    bool InterceptBolt(ACityEnemyBolt& Bolt, FVector From, FVector To);
    float SmokeSpread(const AActor* Enemy, FVector Direction) const;
    bool IsInsideSmoke(const AActor* Enemy) const;
    FVector BarrierWaypoint(FVector From, FVector To) const;
    bool StrikeBarrier(FVector From, FVector To, float Damage);
    bool OnGunRay(const FVector& From, const FVector& To, FVector* VisualEnd = nullptr);
    bool HasOverclockPenetration() const { return bOverclockShot; }
    bool JumpPressed();
    bool AllowsWallJump() const { return Motion.IsNone() && Ultimate.IsNone() && !bGliding && !bJet; }
    void SlidePressed();
    void FrameCamera(float Delta, FMinimalViewInfo& View);
    float GetSlideScale() const;
    static TArray<FVector> BarragePath(FVector Origin, FVector Forward, FVector Side, float Depth, float Lane);
protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
private:
    UPROPERTY(Transient) TObjectPtr<AShooterCharacter> Player;
    UPROPERTY(Transient) TObjectPtr<UCitySkillVisuals> Art;
    UPROPERTY(Transient) TObjectPtr<UCityProgression> Progress;
    UPROPERTY(Transient) TObjectPtr<UCityCastView> CastView;
    void PoseUltimate();
    void RunAbilityVerification();
#if !UE_BUILD_SHIPPING
    void TickSkillCapture();
    FString CaptureKind, CaptureDir;
    int32 CaptureFrame=-45;
    bool bCapturePrepared=false;
    TWeakObjectPtr<AShooterNPC> CaptureTarget;
#endif
    float CastStarted=0;
    FVector Eye() const;
    FVector Aim() const;
    FVector Right() const;
    bool Trace(FVector From, FVector To, FHitResult& Hit, bool bWorldOnly=false, const AActor* Ignore=nullptr, float Radius=0) const;
    FVector EndPoint(float Range, FHitResult& Hit) const;
    bool Floor(FVector Start, FVector& Out) const;
    bool Visible(FVector From, FVector To) const;
    TArray<AShooterNPC*> Near(FVector Point, float Radius, const AActor* Skip=nullptr) const;
    bool Boss(const AShooterNPC* Enemy) const;
    bool Wounded(const AShooterNPC* Enemy) const;
    void Hit(AShooterNPC* Enemy, float Damage, float Stun=0);
    void Area(FName Id, FVector At, float Radius, float Damage, float Stun=0);
    void Refund(FName Id, float Amount);
    void CastSlot(int32 Index);
    bool Small(FName Id);
    bool StartUltimate(FName Id);
    void TickUltimate(float Delta);
    void ReleaseUltimate();
    void FinishUltimate();
    void StartDash(float Distance);
    void TickPersonal(float Delta);
    void Launch(FName Id, FVector Point, FVector Direction, float Speed, float Range);
    void LaunchSwarm();
    void LaunchBarrage();
    void TickFlights(float Delta);
    bool MoveFlight(FCitySkillFlight& Shot, float Dt);
    void Land(FCitySkillFlight& Shot, const FHitResult& Hit);
    void AddField(FName Id, FVector Point, float Radius, float Duration, FVector Direction=FVector::ZeroVector, AShooterNPC* Follow=nullptr);
    bool PlaceBarrier();
    void RemoveWall(FCitySkillField& Field);
    void TrackReturnedBolt(ACityEnemyBolt* Bolt);
    TArray<TWeakObjectPtr<ACityEnemyBolt>> ReturnedBolts;
    void TickFields(float Delta);
    void PulseField(FCitySkillField& Field, float Dt);
    void EndField(FCitySkillField& Field);
    void RenderField(const FCitySkillField& Field) const;
    void CastArt(FName Id, bool bCast, FVector At, float Width);
    bool InField(FName Id, FVector Point) const;
    float Buff(FName Id) const;
    void SetBuff(FName Id, float Seconds);

    TMap<FName,float> Cooldowns, Buffs;
    TArray<FCitySkillFlight> Flights;
    TArray<FCitySkillField> Fields;
    TSet<TWeakObjectPtr<AShooterNPC>> SurgeHits;
    TMap<TWeakObjectPtr<AShooterNPC>,float> ArcMarks;
    TWeakObjectPtr<AShooterNPC> Echo, LastHit, HookEnemy;
    FName Phase, Ultimate, Motion;
    FVector MotionDirection = FVector::ForwardVector, MotionTarget = FVector::ZeroVector;
    float Clock = 0, Energy = 100, UltimateCooldown = 0, PhaseTime = 0, StanceTime = 0, AttackReady = 0;
    float MotionLeft = 0, Shield = 0, HealLeft = 0, LensExtended = 0, GravityBeforeGlide = 1, AirControlBeforeGlide = 0;
    float DeflectRefund = 0;
    int32 Charges = 0, Combo = 0, Rounds = 0, BladesFired = 0;
    bool bHealClean = false, bAirPhase = false, bJet = false, bGliding = false, bPendingReload = false;
    bool bOverclockShot = false, bSurgeBonus = false, bSurgeShot = false;
};

