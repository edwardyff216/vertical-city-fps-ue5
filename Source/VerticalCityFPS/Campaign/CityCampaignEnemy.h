#pragma once
#include "CoreMinimal.h"
#include "Variant_Shooter/AI/ShooterNPC.h"
#include "CityCampaignEnemy.generated.h"

UENUM()
enum class ECityFoe : uint8 { Melee, Blink, Shooter, Support, Spire, Breaker, Relay };

UCLASS()
class VERTICALCITYFPS_API ACityCampaignEnemy : public AShooterNPC
{
    GENERATED_BODY()
public:
    ACityCampaignEnemy();
    void Configure(ECityFoe InKind);
    UFUNCTION(BlueprintCallable) void StunFor(float Seconds);
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* InstigatorController, AActor* Source) override;
    ECityFoe GetKind() const { return Kind; }
    bool IsBoss() const { return Kind >= ECityFoe::Spire; }
    bool IsStunned() const { return Stunned > 0; }
    bool IsExposed() const { return Recovery>0; }
    bool IsWindingUp() const { return Windup > 0; }
    float MaxHealth() const { return InitialHP; }
    FString Status() const;
protected:
    virtual void BeginPlay() override;
private:
    friend class FCityAffixCombatTest;
    ECityFoe Kind = ECityFoe::Melee;
    float InitialHP = 40, Cooldown = 1.3f, Windup = 0, Recovery = 0, Charge = 0, Stunned = 0;
    FVector Heading = FVector::ForwardVector;
    UPROPERTY() TObjectPtr<USkeletalMesh> HeavyBody;
    UPROPERTY() TObjectPtr<USkeletalMesh> LightBody;
    UPROPERTY() TObjectPtr<class UAnimSequence> ArmedIdle;
    UPROPERTY() TObjectPtr<UAnimSequence> UnarmedIdle;
    UPROPERTY() TObjectPtr<UAnimSequence> UnarmedJog;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> SentryRifle;
    UPROPERTY() TObjectPtr<UStaticMesh> Box;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Optic;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Tether;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Telegraphs;
    ACityCampaignEnemy* SupportingAlly() const;
    FVector LastVisibleTarget=FVector::ZeroVector;
    bool bLastVisibleTarget=false, bMeleeWindup=false;
    float BlinkCooldown=4.f;
    bool TryBlinkBehind(const class AShooterCharacter& Target);
    void Fire();
    void FinishCharge(float RecoverySeconds=1.45f);
    void HideTelegraphs();
    void ConfigureCues();
    void UpdateShield(ACityCampaignEnemy* Provider, APawn* Viewer, float Dt);
    void FlashShield(const FDamageEvent& Event, const AActor* Source);
    bool BeginStomp();
    void LandStomp(AShooterCharacter& Player);
    void CancelStomp();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShieldPatch;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FloorWarning;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> WarningInk;
    UPROPERTY() TObjectPtr<class UMaterialInterface> DomeMaterial;
    UPROPERTY() TObjectPtr<class UNiagaraSystem> Shockwave;
    UPROPERTY() TObjectPtr<UMaterialInterface> FloorMaterial;
    UPROPERTY() TObjectPtr<UStaticMesh> CuePlane;
    UPROPERTY() TObjectPtr<UStaticMesh> CueBall;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShieldShell;
    UPROPERTY() TObjectPtr<UMaterialInterface> ShellMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShellInk;
    float BreakLeft=0, AftershockLeft=0;
    float ShieldFlashLeft = 0;
    bool bLinked = false;
    bool bStomping = false;
    FVector StompCenter = FVector::ZeroVector;
    static constexpr float StompRadius = 325.f;
    static constexpr float StompWindup = 1.f;
    bool CanSee(const AActor* Other) const;
    void Chase(FVector Target, float Speed, float DeltaSeconds);
};