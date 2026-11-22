#include "Campaign/CityCampaignEnemy.h"
#include "Campaign/CityEnemyBolt.h"
#include "Campaign/CityCampaignMode.h"
#include "CityGeometry.h"
#include "NiagaraSystem.h"
#include "CityKatana.h"
#include "CityAftercutDamage.h"
#include "ShooterWeapon.h"
#include "CityWeaponEffects.h"
#include "Abilities/CitySkillVisuals.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Abilities/CityAbilities.h"
#include "ShooterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ACityCampaignEnemy::ACityCampaignEnemy()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessAI = EAutoPossessAI::Disabled;
    GetFirstPersonCameraComponent()->SetupAttachment(GetCapsuleComponent());
    GetFirstPersonCameraComponent()->SetAutoActivate(false);
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Box = CubeAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Floor(TEXT("/Game/VerticalCity/VFX/M_BreakerFloor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dome(TEXT("/Game/StylizedDomesShader/Materials/StylizedDomeShader/MI/MI_StylizedDome_03"));
    CuePlane=Plane.Object; CueBall=Ball.Object;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Wave(TEXT("/Game/Vefects/Easy_Shockwaves_VFX/VFX/Shockwaves/Particles/Color_Variation/VFX_Shockwave_01_Orange_1s"));
    Shockwave=Wave.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shell(TEXT("/Game/VerticalCity/VFX/M_SupportShell"));
    ShellMaterial=Shell.Object;
    FloorMaterial=Floor.Object; DomeMaterial=Dome.Object;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Quinn(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Rifle(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Jog(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Aim(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"));
    HeavyBody=Manny.Object; LightBody=Quinn.Object;
    UnarmedIdle=Idle.Object; UnarmedJog=Jog.Object; ArmedIdle=Aim.Object;
    SentryRifle=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SentryRifle"));
    SentryRifle->SetupAttachment(GetMesh());
    SentryRifle->SetSkeletalMesh(Rifle.Object);
    SentryRifle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SentryRifle->SetGenerateOverlapEvents(false);
    SentryRifle->SetCanEverAffectNavigation(false);
}
void ACityCampaignEnemy::BeginPlay()
{
    // Room sentries own their attacks. The template NPC spawns a StateTree weapon in BeginPlay.
    AVerticalCityFPSCharacter::BeginPlay();
}
void ACityCampaignEnemy::Configure(ECityFoe InKind)
{
    Kind = InKind;
    const bool Boss = IsBoss();
    constexpr float HP[] = {100,125,90,85,1050,1050,1050};
    InitialHP = CurrentHP = HP[int32(Kind)];
    const float Factor = Boss ? 1.45f : 1.f;
    GetCapsuleComponent()->SetCapsuleSize(46*Factor, 90*Factor);
    FLinearColor Color(.65,.12,.055);
    if (Kind == ECityFoe::Support || Kind == ECityFoe::Relay) Color = FLinearColor(.04,.84,.46);
    if (Kind == ECityFoe::Spire || Kind == ECityFoe::Shooter) Color = FLinearColor(.10,.50,.9);
    if (Kind == ECityFoe::Blink) Color = FLinearColor(.66,.15,.76);
    const bool bLight=Kind==ECityFoe::Blink || Kind==ECityFoe::Support || Kind==ECityFoe::Relay;
    const bool bArmed=Kind==ECityFoe::Shooter || Kind==ECityFoe::Spire || Kind==ECityFoe::Relay;
    auto* Body=GetMesh();
    Body->SetSkeletalMesh(bLight?LightBody:HeavyBody);
    Body->SetRelativeLocationAndRotation(FVector(0,0,-90*Factor),FRotator(0,-90,0));
    Body->SetRelativeScale3D(FVector(Factor));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetGenerateOverlapEvents(false);
    Body->SetOwnerNoSee(false);
    Body->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::None);
    for (int32 Slot=0;Slot<Body->GetNumMaterials();++Slot)
    {
        if (auto* Finish=Body->CreateDynamicMaterialInstance(Slot))
        {
            Finish->SetVectorParameterValue(TEXT("Paint Tint"),Color);
            Finish->SetVectorParameterValue(TEXT("LogoTint"),Color*2.f);
        }
    }
    if (bArmed) SentryRifle->AttachToComponent(Body,FAttachmentTransformRules::SnapToTargetIncludingScale,ThirdPersonWeaponSocket);
    Body->PlayAnimation(bArmed?ArmedIdle:UnarmedIdle,true);
    SentryRifle->SetVisibility(bArmed);
    auto Plate = [&](FVector At, FVector Size, FLinearColor Tint)
    { return CityGeometry::Part(this, Box, At*Factor, Size*Factor, Tint); };
    Optic = Plate(FVector(26,0,71), FVector(17,29,23), FLinearColor(3,1,.14));
    Optic->SetVisibility(false);
    Tether = Plate(FVector::ZeroVector, FVector(1), FLinearColor(.05,3,.9));
    Tether->SetVisibility(false);
    ConfigureCues();
    for (int i=0; i<(Kind == ECityFoe::Relay ? 5 : 1); ++i)
    {
        auto* Line = Plate(FVector::ZeroVector, FVector(1), FLinearColor(3,.19,.025));
        Line->SetVisibility(false);
        Line->SetCastShadow(false);
        Telegraphs.Add(Line);
    }
}
bool ACityCampaignEnemy::CanSee(const AActor* Other) const
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CityEnemySight), false, this);
    const bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation()+FVector(0,0,26),
        Other->GetActorLocation()+FVector(0,0,28), ECC_Visibility, Params);
    return !Blocked || Hit.GetActor() == Other;
}
ACityCampaignEnemy* ACityCampaignEnemy::SupportingAlly() const
{
    if (Kind==ECityFoe::Support || (IsBoss() && Kind!=ECityFoe::Relay)) return nullptr;
    FCollisionObjectQueryParams Cover;
    Cover.AddObjectTypesToQuery(ECC_WorldStatic); Cover.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityShieldLink),false,this);
    for (TActorIterator<ACityCampaignEnemy> It(GetWorld()); It; ++It)
    {
        if (It->Kind!=ECityFoe::Support || It->CurrentHP<=0 || It->Stunned>0) continue;
        if (Kind==ECityFoe::Relay) return *It;
        const FVector Start=GetActorLocation(), End=It->GetActorLocation();
        if (FVector::DistSquared(Start,End)<=1200*1200 && !GetWorld()->LineTraceTestByObjectType(Start,End,Cover,Query))
            return *It;
    }
    return nullptr;
}
void ACityCampaignEnemy::HideTelegraphs()
{
    for (UStaticMeshComponent* Line : Telegraphs) Line->SetVisibility(false);
}
void ACityCampaignEnemy::FinishCharge(float RecoverySeconds)
{
    CancelStomp();
    Windup = Charge = 0;
    Recovery = RecoverySeconds;
    Cooldown = RecoverySeconds + .75f;
    HideTelegraphs();
}
void ACityCampaignEnemy::StunFor(float Seconds)
{
    if ((Kind == ECityFoe::Spire || Kind == ECityFoe::Breaker) && (Windup>0 || Charge>0))
        if (auto* Viewer = UGameplayStatics::GetPlayerPawn(this,0))
            if (auto* Art = Viewer->FindComponentByClass<UCitySkillVisuals>())
                Art->BossInterrupt(GetActorLocation()+FVector::UpVector*56.f);
    Stunned = FMath::Max(Stunned, Seconds);
    if (Kind == ECityFoe::Breaker && (Windup>0 || Charge>0)) FinishCharge();
    Windup = 0;
    Cooldown = FMath::Max(Cooldown, .5f);
    HideTelegraphs();
}
float ACityCampaignEnemy::TakeDamage(float Amount, const FDamageEvent& Event, AController* ControllerArg, AActor* Source)
{
    if (CurrentHP<=0 || Amount<=0) return 0;
    if (SupportingAlly())
    {
        Amount *= .65f;
        FlashShield(Event,ControllerArg && ControllerArg->GetPawn() ? ControllerArg->GetPawn() : Source);
    }
    if (Kind == ECityFoe::Breaker && Recovery>0) Amount *= 1.5f;
    if (Kind == ECityFoe::Spire && Event.IsOfType(FPointDamageEvent::ClassID))
    {
        const auto* Gun=Cast<AShooterWeapon>(Source);
        const FName GunKind=Gun ? Gun->GetCityKind() : NAME_None;
        const FVector Contact=static_cast<const FPointDamageEvent&>(Event).HitInfo.ImpactPoint;
        const bool Precise=GunKind==TEXT("rifle") || GunKind==TEXT("scout") || GunKind==TEXT("pulse");
        if(Precise && Contact.Z>GetActorLocation().Z+45)
        {
            if(Windup>0) { StunFor(.15f); Recovery=1.35f; }
            if(Recovery>0) Amount*=1.5f;
        }
    }
    if (Source && Source->IsA<ACityKatana>()) StunFor(.35f);
    const float Before=CurrentHP;
    const float Applied = Super::TakeDamage(Amount,Event,ControllerArg,Source);
    if (CurrentHP<Before && ControllerArg && Event.DamageTypeClass!=UCityAftercutDamage::StaticClass())
    {
        APawn* Viewer=ControllerArg->GetPawn();
        const bool bPoint=Event.IsOfType(FPointDamageEvent::ClassID);
        FVector At=bPoint ? FVector(static_cast<const FPointDamageEvent&>(Event).HitInfo.ImpactPoint) : GetActorLocation();
        if (Viewer)
        {
            At+=(Viewer->GetPawnViewLocation()-At).GetSafeNormal()*(bPoint?9.f:42.f);
            UCitySkillVisuals::Emit(Viewer,CurrentHP<=0?FName("kill_burst"):FName("hit_spark"),At,CurrentHP<=0?178.f:74.f);
        }
    }
    if (CurrentHP<=0)
    {
        if (auto* Mode=GetWorld()->GetAuthGameMode<ACityCampaignMode>()) Mode->DropAmmo(GetActorLocation());
        if (APawn* Killer = ControllerArg ? ControllerArg->GetPawn() : nullptr)
            if (auto* Skills = Killer->FindComponentByClass<UCityAbilities>())
            {
                const FName Types[]={TEXT("melee"),TEXT("blink"),TEXT("shooter"),TEXT("support")};
                Skills->NotifyKill(IsBoss()?FName(TEXT("boss")):Types[int32(Kind)]);
            }
        SetActorHiddenInGame(true);
        SetActorEnableCollision(false);
        SetActorTickEnabled(false);
        Destroy();
    }
    return Applied;
}
FString ACityCampaignEnemy::Status() const
{
    if (Stunned>0) return TEXT("已打断");
    switch (Kind)
    {
        case ECityFoe::Spire: return Recovery>0 ? TEXT("镜核暴露 · 瞄准头部") : Windup>0 ? TEXT("重弹锁定 · 横移") : TEXT("镜核闭合");
        case ECityFoe::Breaker: return bStomping ? TEXT("震地蓄力 · 撤出橙圈或打断") : Recovery>0 ? TEXT("失衡 · 伤害增加") : Windup>0 || Charge>0 ? TEXT("直线冲锋 · 侧闪或 F 防反") : TEXT("逼近中");
        case ECityFoe::Relay: return SupportingAlly() ? TEXT("支援供盾 · 优先击破青色支援者") : TEXT("护盾离线 · 穿过扇射空隙");
        default: return FString();
    }
}
void ACityCampaignEnemy::Fire()
{
    FActorSpawnParameters Args;
    Args.Owner=this; Args.Instigator=this;
    Args.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector From = GetActorLocation()+FVector(0,0,28);
    APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    UCityAbilities* Skills=Player?Player->FindComponentByClass<UCityAbilities>():nullptr;
    const float Spread=Skills?Skills->SmokeSpread(this,Heading):0;
    for (int i=0; i<Telegraphs.Num(); ++i)
    {
        FVector Dir = Heading.RotateAngleAxis((i-(Telegraphs.Num()-1)*.5f)*13.75f,FVector::UpVector);
        if (Spread>0)
        {
            FRotator Shot=Dir.Rotation();
            Shot.Yaw+=FMath::FRandRange(-Spread,Spread); Shot.Pitch+=FMath::FRandRange(-Spread,Spread);
            Dir=Shot.Vector();
        }
        if (auto* Bolt=GetWorld()->SpawnActor<ACityEnemyBolt>(From+Dir*64,Dir.Rotation(),Args))
            Bolt->Launch(Dir, Kind==ECityFoe::Spire ? 1500 : 900, IsBoss() ? 19 : 11);
    }
    if (Kind==ECityFoe::Spire) Recovery=1.35f;
    Cooldown=Kind==ECityFoe::Spire ? 2.6f : IsBoss() ? 2.15f : 2.3f;
    if (Skills && !IsBoss() && Skills->IsInsideSmoke(this)) Cooldown*=1.3f;
    if(IsBoss()) UE_LOG(LogTemp,Display,TEXT("City boss fired kind=%s lanes=%d"),Kind==ECityFoe::Spire?TEXT("spire"):TEXT("relay"),Telegraphs.Num());
    UE_LOG(LogTemp,Verbose,TEXT("City attack fired kind=%d lanes=%d"), int32(Kind),Telegraphs.Num());
}
void ACityCampaignEnemy::Chase(FVector Target, float Speed, float DeltaSeconds)
{
    const FVector Origin=GetActorLocation();
    if (auto* P=UGameplayStatics::GetPlayerPawn(this,0))
        if (auto* Skills=P->FindComponentByClass<UCityAbilities>()) Target=Skills->BarrierWaypoint(Origin,Target);
    FVector Walk=(Target-Origin).GetSafeNormal2D();
    if (FMath::Abs(Target.Z-Origin.Z)>170 && Target.Z>Origin.Z)
    {
        const FVector Waypoint(Origin.Y < -460 && Origin.X > 120 ? 1060 : 115, -720, 0);
        Walk=(Waypoint-Origin).GetSafeNormal2D();
    }
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityChaseSteering),false,this);
    if (GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Walk*115,ECC_Visibility,Query) &&
        FMath::Abs(Hit.Normal.Z)<.6f && !Cast<AShooterCharacter>(Hit.GetActor()))
    {
        FVector Around=FVector::CrossProduct(FVector::UpVector,Hit.Normal).GetSafeNormal();
        if (FVector::DotProduct(Around,Walk)<0) Around*=-1;
        Walk=(Walk*.25+Around).GetSafeNormal();
    }
    if(!IsBoss())
        if(const auto* P=UGameplayStatics::GetPlayerPawn(this,0))
            if(auto* Effects=P->FindComponentByClass<UCityWeaponEffects>()) Speed*=Effects->MovementScale(this);
    GetCharacterMovement()->MaxWalkSpeed=Speed;
    AddMovementInput(Walk,1,true);
}
bool ACityCampaignEnemy::TryBlinkBehind(const AShooterCharacter& Target)
{
    const FVector Behind=Target.GetActorLocation()-Target.GetActorForwardVector().GetSafeNormal2D()*280;
    const auto* Capsule=GetCapsuleComponent();
    const float Height=Capsule->GetScaledCapsuleHalfHeight(), Radius=Capsule->GetScaledCapsuleRadius();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityBlink),false,this);
    FCollisionObjectQueryParams FloorObjects;
    FloorObjects.AddObjectTypesToQuery(ECC_WorldStatic); FloorObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FHitResult Floor;
    if (!GetWorld()->LineTraceSingleByObjectType(Floor,Behind+FVector::UpVector*90,Behind-FVector::UpVector*500,FloorObjects,Query)
        || Floor.bStartPenetrating || Floor.ImpactNormal.Z<.65f) return false;
    const FVector Destination=Floor.ImpactPoint+FVector::UpVector*(Height+3);
    if (GetWorld()->SweepTestByChannel(Destination+FVector::UpVector*3,Destination,FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius,Height),Query)) return false;
    const FVector Previous=GetActorLocation();
    SetActorLocation(Destination,false,nullptr,ETeleportType::TeleportPhysics);
    GetCharacterMovement()->StopMovementImmediately();
    if (auto* Visuals=Target.FindComponentByClass<UCitySkillVisuals>())
    {
        Visuals->Sprite(TEXT("phase_step"),false,Previous,84);
        Visuals->Sprite(TEXT("phase_step"),false,Destination,84);
    }
    BlinkCooldown=6; Cooldown=.45f;
    UE_LOG(LogTemp,Verbose,TEXT("City blink landed behind target distance=%.1f"),FVector::Dist2D(Target.GetActorLocation(),Destination));
    return true;
}
void ACityCampaignEnemy::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Player = Cast<AShooterCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if (!Player || Player->IsDead() || CurrentHP<=0 || !Optic)
    {
        if (bStomping) FinishCharge();
        return;
    }
    if (Kind==ECityFoe::Melee || Kind==ECityFoe::Blink || Kind==ECityFoe::Breaker || Kind==ECityFoe::Support)
    {
        UAnimSequence* Pose=GetVelocity().SizeSquared2D()>1600 && Stunned<=0 ? UnarmedJog : UnarmedIdle;
        if (auto* Node=GetMesh()->GetSingleNodeInstance(); !Node || Node->GetCurrentAsset()!=Pose) GetMesh()->PlayAnimation(Pose,true);
    }
    const FVector From=GetActorLocation();
    FVector To=Player->GetActorLocation();
    auto* Skills=Player->FindComponentByClass<UCityAbilities>();
    const bool VisibleTarget=!Skills || Skills->GetEnemyAimPoint(this,Player->GetActorLocation(),To);
    const FVector Toward=To-From;
    UpdateShield(SupportingAlly(),Player,Dt);
    if (AftershockLeft>0)
    {
        AftershockLeft=FMath::Max(0.f,AftershockLeft-Dt);
        if (WarningInk) WarningInk->SetScalarParameterValue(TEXT("Progress"),-(1.f-AftershockLeft/.38f));
        FloorWarning->SetVisibility(AftershockLeft>0);
    }
    Optic->SetVisibility(Kind==ECityFoe::Spire && Recovery>0);
    Stunned=FMath::Max(0.f,Stunned-Dt); Recovery=FMath::Max(0.f,Recovery-Dt); Cooldown-=Dt;
    BlinkCooldown-=Dt;
    if (Tags.Contains(TEXT("CityStationaryTarget"))) return;
    if (!VisibleTarget)
    {
        CancelStomp(); Windup=0; HideTelegraphs();
        if (Stunned<=0 && bLastVisibleTarget && FVector::DistSquared2D(From,LastVisibleTarget)>10000)
            Chase(LastVisibleTarget,310,Dt);
        return;
    }
    LastVisibleTarget=To; bLastVisibleTarget=true;
    if (Stunned>0 || Recovery>0) return;
    if (Charge>0)
    {
        FHitResult Hit;
        const FVector ChargeStep=Heading*(1600.f*FMath::Min(Dt,Charge));
        if (Skills && Skills->StrikeBarrier(From,From+ChargeStep+Heading*65,14)) { FinishCharge(); return; }
        AddActorWorldOffset(ChargeStep,true,&Hit);
        Charge-=Dt;
        if (Hit.GetActor()==Player || (FVector::DistSquared(GetActorLocation(),To)<160*160 && CanSee(Player)))
        {
            UGameplayStatics::ApplyDamage(Player,24.5f,nullptr,this,nullptr);
            FinishCharge(.65f);
        }
        else if (Hit.bBlockingHit || Charge<=0) FinishCharge();
        return;
    }
    if (Windup>0)
    {
        Windup-=Dt;
        if (bStomping)
        {
            const float Progress=FMath::Clamp(1.f-Windup/StompWindup,0.f,1.f);
            if (WarningInk) WarningInk->SetScalarParameterValue(TEXT("Progress"),Progress);
            const float Lift=FMath::Min(Progress/.7f,(1.f-Progress)/.16f);
            GetMesh()->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()+48*FMath::Clamp(Lift,0.f,1.f)));
        }
        if (Windup>0) return;
        if (bStomping) { LandStomp(*Player); return; }
        HideTelegraphs();
        if (Kind==ECityFoe::Breaker) { Charge=.48f; UE_LOG(LogTemp,Display,TEXT("City boss charged kind=breaker")); }
        else if (bMeleeWindup)
        {
            const bool HitWall=Skills && Skills->StrikeBarrier(From,From+(To-From).GetClampedToMaxSize(260),14);
            if (!HitWall && FVector::DistSquared(From,To)<360*360 && CanSee(Player))
                    UGameplayStatics::ApplyDamage(Player,14,nullptr,this,nullptr);
            Cooldown=1.7f;
        }
        else Fire();
        return;
    }
    SetActorRotation(FRotator(0,Toward.Rotation().Yaw,0));
    const bool MeleeFoe=Kind==ECityFoe::Melee || Kind==ECityFoe::Blink;
    const bool Breaker=Kind==ECityFoe::Breaker;
    const bool AtWall=(MeleeFoe || Breaker) && Skills && Skills->StrikeBarrier(From,From+Toward.GetClampedToMaxSize(180),0);
    if (Kind==ECityFoe::Blink && BlinkCooldown<=0 && Toward.Size()>400 && TryBlinkBehind(*Player)) return;
    if ((MeleeFoe || Breaker) && !AtWall && (Toward.Size()>(Breaker?820:310) || !CanSee(Player)))
    {
        Chase(To,310*(Skills?Skills->EnemySpeedScale(this):1.f),Dt);
        return;
    }
    if (Cooldown>0 || !CanSee(Player)) return;
    if (Breaker && !AtWall && Toward.Size2D()<=StompRadius && FMath::Abs(Toward.Z)<145 && BeginStomp()) return;
    bMeleeWindup=!IsBoss() && (AtWall || Toward.Size()<310);
    if (Kind==ECityFoe::Support && !bMeleeWindup) return;
    const bool Close=bMeleeWindup || Breaker;
    Heading=Close ? Toward.GetSafeNormal2D() : (To-(From+FVector(0,0,28))).GetSafeNormal();
    Windup=Close ? (Kind==ECityFoe::Breaker ? .85f : .52f) : (Kind==ECityFoe::Spire ? 1.05f : IsBoss() ? .95f : .65f);
    const FVector Start=From+FVector(0,0,Close ? -65 : 28);
    for (int i=0; i<Telegraphs.Num(); ++i)
    {
        const FVector Dir=Heading.RotateAngleAxis((i-(Telegraphs.Num()-1)*.5f)*13.75f,FVector::UpVector);
        CityGeometry::Segment(Telegraphs[i],Start,Start+Dir*(Close ? (IsBoss()?768:230) : FMath::Min(3200.,Toward.Size()+160)),Close ? 18 : 5);
    }
}