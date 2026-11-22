#include "Abilities/CityAbilities.h"
#include "Abilities/CitySkillVisuals.h"
#include "Abilities/CityCastView.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Campaign/CityEnemyBolt.h"
#include "Campaign/CityCampaignMode.h"
#include "Progression/CityProgression.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

UCityAbilities::UCityAbilities()
{
    PrimaryComponentTick.bCanEverTick = true;
}
void UCityAbilities::BeginPlay()
{
    Super::BeginPlay();
    Player = Cast<AShooterCharacter>(GetOwner());
    if (!Player) { SetComponentTickEnabled(false); return; }
    Progress = GetWorld()->GetGameInstance()->GetSubsystem<UCityProgression>();
    Art = NewObject<UCitySkillVisuals>(Player);
    Art->RegisterComponent();
    Art->LoadAtlas();
    CastView=NewObject<UCityCastView>(Player);
    CastView->RegisterComponent();
    CastView->Setup(Player);
#if !UE_BUILD_SHIPPING
    if (FParse::Value(FCommandLine::Get(),TEXT("CitySkillCapture="),CaptureKind)
        && FParse::Value(FCommandLine::Get(),TEXT("CityCaptureDir="),CaptureDir))
        PrimaryComponentTick.bTickEvenWhenPaused=true;
    if (FParse::Param(FCommandLine::Get(),TEXT("CityAbilityVerify")))
    {
        FTimerHandle Timer;
        GetWorld()->GetTimerManager().SetTimer(Timer,this,&UCityAbilities::RunAbilityVerification,1.f,false);
    }
#endif
}
FVector UCityAbilities::Eye() const
{
    if (const UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>()) return Camera->GetComponentLocation();
    return Player->GetPawnViewLocation();
}
FVector UCityAbilities::Aim() const { return Player->GetControlRotation().Vector(); }
FVector UCityAbilities::Right() const { return FRotationMatrix(Player->GetControlRotation()).GetScaledAxis(EAxis::Y); }
bool UCityAbilities::Trace(FVector From,FVector To,FHitResult& Result,bool bWorldOnly,const AActor* Ignore,float Radius) const
{
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    if (!bWorldOnly) Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CityAbility),false,Player);
    if (Ignore) Params.AddIgnoredActor(Ignore);
    if (Player->GetCityWeapon()) Params.AddIgnoredActor(Player->GetCityWeapon());
    if (Radius > 0)
        return GetWorld()->SweepSingleByObjectType(Result,From,To,FQuat::Identity,Objects,FCollisionShape::MakeSphere(Radius),Params);
    return GetWorld()->LineTraceSingleByObjectType(Result,From,To,Objects,Params);
}
FVector UCityAbilities::EndPoint(float Range,FHitResult& Result) const
{
    FVector End = Eye()+Aim()*Range;
    return Trace(Eye(),End,Result) ? Result.ImpactPoint : End;
}
bool UCityAbilities::Floor(FVector Start,FVector& Out) const
{
    FHitResult H;
    if (!Trace(Start+FVector::UpVector*65,Start-FVector::UpVector*2200,H,true) || H.ImpactNormal.Z<.55) return false;
    Out = H.ImpactPoint+FVector::UpVector*6;
    return true;
}
bool UCityAbilities::Visible(FVector From,FVector To) const { FHitResult H; return !Trace(From,To,H,true); }
TArray<AShooterNPC*> UCityAbilities::Near(FVector Point,float Radius,const AActor* Skip) const
{
    TArray<AShooterNPC*> Found;
    for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
    {
        if (*It==Skip || It->CurrentHP<=0 || It->GetCombatTeam()==Player->GetCombatTeam()) continue;
        const FVector Center = It->GetActorLocation();
        if (FVector::DistSquared(Center,Point)<=FMath::Square(Radius) && Visible(Point,Center)) Found.Add(*It);
    }
    Found.Sort([Point](const AShooterNPC& A,const AShooterNPC& B){
        return FVector::DistSquared(A.GetActorLocation(),Point)<FVector::DistSquared(B.GetActorLocation(),Point);
    });
    return Found;
}
bool UCityAbilities::Boss(const AShooterNPC* N) const
{
    auto* Enemy = Cast<ACityCampaignEnemy>(N);
    return Enemy && Enemy->IsBoss();
}
bool UCityAbilities::Wounded(const AShooterNPC* N) const
{
    if (!N || N->CurrentHP<=0) return false;
    const auto* Enemy = Cast<ACityCampaignEnemy>(N);
    return N->CurrentHP <= (Enemy ? Enemy->MaxHealth() : 100.f) * (Boss(N) ? .20f : .35f);
}
void UCityAbilities::Hit(AShooterNPC* N,float Damage,float Stun)
{
    if (!N || N->CurrentHP<=0 || N->GetCombatTeam()==Player->GetCombatTeam()) return;
    if (Damage>0) UGameplayStatics::ApplyDamage(N,Damage,Player->GetController(),Player,nullptr);
    if (N->CurrentHP>0 && Stun>0 && !Boss(N))
        if (auto* Enemy=Cast<ACityCampaignEnemy>(N)) Enemy->StunFor(Stun);
}
void UCityAbilities::Area(FName Id,FVector At,float Radius,float Damage,float Stun)
{
    for (auto* N : Near(At,Radius)) Hit(N,Damage,Stun);
    Art->Burst(Id,At,Radius);
}
void UCityAbilities::CastArt(FName Id,bool bCast,FVector At,float Width)
{
    Art->Sprite(Id,bCast,At,Width);
}
float UCityAbilities::GetCooldown(FName Id) const
{
    auto* Def=FCityAbilityCatalog::Find(Id);
    return Def && Def->bUltimate ? UltimateCooldown : Cooldowns.FindRef(Id);
}
void UCityAbilities::Refund(FName Id,float Amount) { Cooldowns.Add(Id,FMath::Max(0.f,Cooldowns.FindRef(Id)-Amount)); }
float UCityAbilities::Buff(FName Id) const { return Buffs.FindRef(Id)-Clock; }
void UCityAbilities::SetBuff(FName Id,float Seconds) { Buffs.Add(Id,Clock+Seconds); }
void UCityAbilities::CastSlot(int32 Index)
{
    if (Progress) TryCast(Progress->GetActiveSkill(Index));
}
void UCityAbilities::CastPrimary() { CastSlot(0); }
void UCityAbilities::CastSecondary() { CastSlot(1); }
void UCityAbilities::CastUltimate() { if (Progress) TryCast(Progress->GetUltimate()); }
bool UCityAbilities::SelectSessionUltimate(FName Id)
{
    if (!Progress || !Progress->SelectSessionUltimate(Id)) return false;
    if (Motion=="ultimate_dash")
    {
        Motion=NAME_None; MotionLeft=0;
        Player->GetCharacterMovement()->StopMovementImmediately();
    }
    Phase=Ultimate=NAME_None;
    Charges=Combo=0; PhaseTime=StanceTime=0;
    if (CastView) CastView->Hide();
    return true;
}
void UCityAbilities::NotifyKill(FName Kind) { Energy = FMath::Min(100.f,Energy+(Kind=="blink" ? 30.f : 20.f)); }
bool UCityAbilities::TryCast(FName Id)
{
    const auto* Def=FCityAbilityCatalog::Find(Id);
    if (!Def || !Player || !Art || Player->IsDead() || GetWorld()->IsPaused()) return false;
    if (const auto* Campaign=GetWorld()->GetAuthGameMode<ACityCampaignMode>())
    {
        const ECityPhase RoomPhase=Campaign->GetPhase();
        const bool Rest=RoomPhase==ECityPhase::Gate || RoomPhase==ECityPhase::Extract;
        if (RoomPhase!=ECityPhase::Combat && !(Def->bUltimate && Rest)) return false;
    }
    if (Def->bUltimate) return StartUltimate(Id);
    if (GetCooldown(Id)>0 || !Phase.IsNone()) return false;
    Cooldowns.Add(Id,Def->Cooldown);
    if (!Small(Id)) { Cooldowns.Remove(Id); return false; }
    CastArt(Id,true,Eye(),68);
    return true;
}
void UCityAbilities::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    if (!Player || !Art) return;
#if !UE_BUILD_SHIPPING
    if (!CaptureKind.IsEmpty() && !CaptureDir.IsEmpty()) TickSkillCapture();
#endif
    if (Player->IsDead()) { ClearRoomEffects(); return; }
    Clock += Delta;
    for (auto& Pair : Cooldowns) Pair.Value = FMath::Max(0.f,Pair.Value-Delta);
    UltimateCooldown=FMath::Max(0.f,UltimateCooldown-Delta);
    Art->BeginFrame(Delta,Eye()+Aim()*104-Right()*38-FVector::UpVector*23);
    TickPersonal(Delta);
    TickUltimate(Delta);
    PoseUltimate();
    TickFlights(Delta);
    TickFields(Delta);
    Art->EndFrame();
}
void UCityAbilities::ClearRoomEffects()
{
    if (bGliding && Player)
    {
        Player->GetCharacterMovement()->GravityScale=GravityBeforeGlide;
        Player->GetCharacterMovement()->AirControl=AirControlBeforeGlide;
    }
    for (auto& Field:Fields) RemoveWall(Field);
    for (auto& Bolt:ReturnedBolts) if (Bolt.IsValid()) Bolt->Destroy();
    ReturnedBolts.Reset();
    Flights.Reset(); Fields.Reset(); Buffs.Reset(); ArcMarks.Reset(); SurgeHits.Reset();
    Phase=Ultimate=Motion=NAME_None;
    Echo.Reset(); HookEnemy.Reset(); LastHit.Reset();
    bGliding=bJet=bPendingReload=bOverclockShot=bSurgeBonus=bSurgeShot=bAirPhase=false;
    Shield=HealLeft=MotionLeft=0;
    Rounds=Charges=0;
    if (Art) Art->Reset();
    if (CastView) CastView->Hide();
}

