#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CitySkillVisuals.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UBillboardComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UTexture2D;

struct FCitySkillFlash
{
    FName Id;
    FVector A, B;
    float Age, Life, Width;
    uint8 Kind;
    bool bCast;
};

// Rendered geometry and imported sprite strips also render in packaged builds.
UCLASS()
class UCitySkillVisuals : public UActorComponent
{
    GENERATED_BODY()
public:
    UCitySkillVisuals();
    void BeginFrame(float Dt, FVector CastPoint);
    void EndFrame();
    void Reset();
    void Sprite(FName Id, bool bCast, FVector At, float Width);
    void Flash(FName Id, FVector From, FVector To, float Width, float Life=.15f);
    void Burst(FName Id, FVector At, float Radius, float Life=.30f);
    void Rod(FName Id, FVector From, FVector To, float Width);
    void Ring(FName Id, FVector At, float Radius, float Width, int32 Sides=24);
    void Shape(FName Id, int32 Mesh, FVector At, FQuat Rotation, FVector Dimensions);
    void Blade(FName Id, FVector At, FVector Forward, float Angle);
    void LoadAtlas();
    void Combat(FName Sheet, FVector At, float Width);
    static void Emit(AActor* Viewer, FName Sheet, FVector At, float Width);
    void FrostField(FVector At, float Radius, float Time);
    void SinkField(FVector At, float Radius, int32 Stored, float Time);
    void FrostLanding(FVector At);
    bool MuzzleFlash(UStaticMeshComponent* Gun, FVector At, FRotator Direction, FName Kind, bool bFirstPerson);
    void StartSink(FVector At);
    bool FireTrail(FName School, const FVector& Start, const FVector& End);
    UNiagaraComponent* BladeTrail(UStaticMeshComponent* Blade, bool bFirstPerson, float Life);
    void SwingWave(FVector Eye, FVector Forward, float Reach, TConstArrayView<FVector> Contacts);
    void PrismHit(FVector At, FRotator Heading, float Delay, float SizeScale=1.f);
    void BossInterrupt(FVector At);
    void DashSlash(FVector At, FRotator Direction);
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    UPROPERTY(Transient) TArray<TObjectPtr<UBillboardComponent>> Sprites;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMesh>> Meshes;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<UMaterialInterface>> Inks;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseInk;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Atlas;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> CombatAtlas;
    UPROPERTY() TObjectPtr<UNiagaraSystem> SinkSystem;
    UPROPERTY() TObjectPtr<UNiagaraSystem> SlashSystem;
    UPROPERTY() TObjectPtr<UNiagaraSystem> LightMuzzle;
    UPROPERTY() TObjectPtr<UNiagaraSystem> HeavyMuzzle;
    UPROPERTY() TObjectPtr<UNiagaraSystem> FrostLandingSystem;
    UPROPERTY() TObjectPtr<UNiagaraSystem> ArcHitSystem;
    UPROPERTY() TMap<FName, TObjectPtr<UNiagaraSystem>> ShotTrails;
    UPROPERTY() TObjectPtr<UMaterialInterface> PrismHitMaterial;
    struct FPrismHit { TWeakObjectPtr<UStaticMeshComponent> Card; float Age; };
    TArray<FPrismHit> PrismHits;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> IdleHitCards;
    UStaticMeshComponent* CreateStrokeCard(UMaterialInterface* Material);
    void AdvancePrismHits(float Delta);
    UPROPERTY() TObjectPtr<UStaticMesh> StrokePlane;
    UPROPERTY() TObjectPtr<UMaterialInterface> StrokeMaterial;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Stroke;
    UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> StrokeInk;
    FVector StrokeStart, StrokeEnd;
    float StrokeTime=-1.f, StrokeLife=0.f, StrokeWidth=0.f;
    void StartStroke(FVector Start,FVector End,FRotator Heading,float Width,float Life);
    void AdvanceStroke(float Delta);
    UPROPERTY() TObjectPtr<UNiagaraSystem> InterruptSystem;
    struct FMovingTrail
    {
        TWeakObjectPtr<UNiagaraComponent> Component;
        FVector Start, End;
        float Time, Duration;
    };
    TArray<FMovingTrail> MovingTrails;
    void AdvanceTrails(float Delta);
    struct FActiveParticle { TWeakObjectPtr<UNiagaraComponent> Component; float Life; };
    TArray<FActiveParticle> Particles;
    UNiagaraComponent* Particle(UNiagaraSystem* System, FVector At, FRotator Rotation, float Scale, float Life, FName TrailSchool = NAME_None);
    TArray<FCitySkillFlash> Flashes;
    int32 Used = 0, UsedSprites = 0;
    void DrawSprite(const FCitySkillFlash& Cue, FVector At);
};

