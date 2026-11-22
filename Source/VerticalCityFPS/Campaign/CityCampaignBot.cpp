#include "Campaign/CityCampaignMode.h"
#include "CityRoomShell.h"
#include "Progression/CityProgression.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Abilities/CityAbilities.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "AudioMixerBlueprintLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if !UE_BUILD_SHIPPING
namespace
{
// Fast routes exercise the damage windows independently from the three filmed combat runs.
bool CheckBossMechanic(ACityCampaignEnemy* Boss,ACityCampaignEnemy* Support,AShooterCharacter* Player)
{
    const FVector BeforePosition=Player->GetActorLocation();
    const FVector Focus=Boss->GetActorLocation();
    Player->SetActorLocation(Focus-FVector(500,0,0),false,nullptr,ETeleportType::TeleportPhysics);
    auto Damage=[&](float Base,bool Head)
    {
        const float Before=Boss->CurrentHP;
        if(Head)
        {
            FHitResult Hit;
            Hit.ImpactPoint=Focus+FVector(0,0,78);
            UGameplayStatics::ApplyPointDamage(Boss,Base,FVector::ForwardVector,Hit,Player->GetController(),Player->GetCityWeapon(),nullptr);
        }
        else UGameplayStatics::ApplyDamage(Boss,Base,Player->GetController(),Player,nullptr);
        return Before-Boss->CurrentHP;
    };
    bool Passed=false;
    if(Boss->GetKind()==ECityFoe::Spire)
    {
        const float Closed=Damage(10,true);
        Boss->Tick(2);
        const bool Locked=Boss->IsWindingUp();
        const float Interrupt=Damage(10,true), Exposed=Damage(10,true);
        Passed=Locked && !Boss->IsWindingUp() && Boss->IsExposed() && FMath::IsNearlyEqual(Closed,10.f)
            && FMath::IsNearlyEqual(Interrupt,15.f) && FMath::IsNearlyEqual(Exposed,15.f);
        UE_LOG(LogTemp,Display,TEXT("City mechanic spire locked=%d closed=%.1f interrupt=%.1f exposed=%.1f result=%d"),Locked,Closed,Interrupt,Exposed,Passed);
    }
    else if(Boss->GetKind()==ECityFoe::Breaker)
    {
        Boss->Tick(2);
        const bool Locked=Boss->IsWindingUp();
        Boss->StunFor(1.4f);
        const float Vulnerable=Damage(20,false);
        Passed=Locked && !Boss->IsWindingUp() && Boss->IsExposed() && FMath::IsNearlyEqual(Vulnerable,30.f);
        const bool ChargeInterrupted=Passed;
        const float InitialHealth=Player->GetCityHealthFraction();
        auto Place=[&](float Distance)
        {
            const float Height=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            Player->SetActorLocation(Focus+FVector(-Distance,0,Height),false,nullptr,ETeleportType::TeleportPhysics);
        };
        Place(220);
        Boss->Tick(3);
        const bool Warned=Boss->IsWindingUp() && Boss->Status().Contains(TEXT("震地"));
        Boss->Tick(.85f);
        const bool SafeBefore=Player->GetCityHealthFraction()==InitialHealth;
        Boss->Tick(.16f);
        const float AfterHit=Player->GetCityHealthFraction();
        Boss->Tick(.1f);
        const bool HitOnce=AfterHit<InitialHealth && Player->GetCityHealthFraction()==AfterHit;
        Boss->Tick(3);
        const bool WarnedAgain=Boss->IsWindingUp();
        Place(420);
        Boss->Tick(1.01f);
        const bool Escaped=Player->GetCityHealthFraction()==AfterHit;
        Place(220);
        Boss->Tick(3);
        const bool CanInterrupt=Boss->IsWindingUp();
        Boss->StunFor(1.4f);
        Boss->Tick(1.01f);
        const bool Cancelled=CanInterrupt && !Boss->IsWindingUp() && Player->GetCityHealthFraction()==AfterHit;
        Passed=ChargeInterrupted && Warned && SafeBefore && HitOnce && WarnedAgain && Escaped && Cancelled;
        UE_LOG(LogTemp,Display,TEXT("City stomp checks warn=%d safeBefore=%d once=%d evade=%d cancel=%d"),Warned,SafeBefore,HitOnce,WarnedAgain&&Escaped,Cancelled);
        UE_LOG(LogTemp,Display,TEXT("City mechanic breaker locked=%d interrupted=%d recovery_damage=%.1f result=%d"),Locked,!Boss->IsWindingUp(),Vulnerable,Passed);
    }
    else if(Support)
    {
        const float Guarded=Damage(20,false);
        Support->StunFor(1);
        const float Disrupted=Damage(20,false);
        UGameplayStatics::ApplyDamage(Support,99999,Player->GetController(),Player,nullptr);
        const float Unlinked=Damage(20,false);
        Passed=FMath::IsNearlyEqual(Guarded,13.f) && FMath::IsNearlyEqual(Disrupted,20.f) && FMath::IsNearlyEqual(Unlinked,20.f);
        UE_LOG(LogTemp,Display,TEXT("City mechanic relay shield=%.1f stunned=%.1f dead=%.1f result=%d"),Guarded,Disrupted,Unlinked,Passed);
    }
    Player->SetActorLocation(BeforePosition,false,nullptr,ETeleportType::TeleportPhysics);
    return Passed;
}
}
#endif
void ACityCampaignMode::WalkTo(AShooterCharacter* Pawn,FVector Point)
{
    const FVector Direction=(Point-Pawn->GetActorLocation()).GetSafeNormal2D();
    Pawn->DoMove(FVector::DotProduct(Direction,Pawn->GetActorRightVector()),FVector::DotProduct(Direction,Pawn->GetActorForwardVector()));
}
void ACityCampaignMode::EndBot(bool Success,const TCHAR* Reason)
{
    if(bBotFinished) return;
    bBotFinished=true;
    UE_LOG(LogTemp,Display,TEXT("CITY_ROUTE_TEST %s route=%s debugDamage=%d depth=%d reason=%s"),
        Success?TEXT("PASS"):TEXT("FAIL"),*TestRoute,bFast,Depth,Reason);
    FPlatformMisc::RequestExitWithStatus(false,Success?0:9);
}
void ACityCampaignMode::StepBot(float Delta)
{
#if !UE_BUILD_SHIPPING
    if(bBotFinished) return;
    if (!SwordCaptureFolder.IsEmpty() && Depth==0 && (Phase==ECityPhase::Combat || MovingCutStart!=INDEX_NONE) &&
        FParse::Param(FCommandLine::Get(),TEXT("CityMovingSwordCapture")))
    {
        CaptureMovingSword();
        return;
    }
    // GameMode continues ticking with a paused world. Modal decisions must run even when world delta is zero.
    BotTime+=Delta; BotClock+=Delta; BotStateTime+=Delta;
    if(BotTime>420 || BotStateTime>105) { UE_LOG(LogTemp,Warning,TEXT("City bot stalled phase=%d depth=%d botStage=%d pos=%s enemies=%d"),int32(Phase),Depth,BotStage,Player()?*Player()->GetActorLocation().ToString():TEXT("none"),Enemies.Num()); EndBot(false,TEXT("stage timeout")); return; }
    const double Age=FPlatformTime::Seconds()-PhaseStarted;
    if(bRecording && (Phase==ECityPhase::Weapon || HasCampaignOverlay()) && Age<1.8) return;
    if(Phase==ECityPhase::Result)
    {
        if(AudioStarted>0)
        {
            if(AudioStopped<=0)
            {
                AudioStopped=FPlatformTime::Seconds();
                UAudioMixerBlueprintLibrary::StopRecordingOutputToAnyPath(this,EAudioRecordingExportType::WavFile,TEXT("game-mix"),AudioFolder);
                UE_LOG(LogTemp,Display,TEXT("City master mix stopped; end sync marker"));
            }
            const double Since=FPlatformTime::Seconds()-AudioStopped;
            if(Since<.65) return;
            if(IFileManager::Get().FileSize(*(AudioFolder / TEXT("game-mix.wav")))<=44)
            {
                if(Since>12) EndBot(false,TEXT("game master mix export failed"));
                return;
            }
        }
        EndBot(bWon && Route==TestRoute && Depth==4,bWon?TEXT("physical extraction"):TEXT("player died")); return;
    }
    if(Phase==ECityPhase::Retry) { EndBot(false,TEXT("persisting campaign state failed")); return; }
    if(Phase==ECityPhase::Preparation)
    {
        if(!AudioFolder.IsEmpty())
        {
            if(AudioStarted<=0)
            {
                if(Age>40) { EndBot(false,TEXT("capture handshake timed out")); return; }
                if(Age<2.5 || !FPaths::FileExists(AudioFolder / TEXT("capture.ready"))) return;
                AudioStarted=FPlatformTime::Seconds();
                UAudioMixerBlueprintLibrary::StartRecordingOutput(this,110.f);
                UE_LOG(LogTemp,Display,TEXT("City master mix started; start sync marker"));
            }
            if(FPlatformTime::Seconds()-AudioStarted<2.5) return;
        }
        if(bRecording && Age>5.0) Progress()->Depart();
        return;
    }
    if(Phase==ECityPhase::Weapon) { Progress()->DismissReward(); return; }
    if(Phase==ECityPhase::Ability) { Select(0); return; }
    auto* P=Player();
    if(!P || P->IsDead()) return;
    const FVector At=P->GetActorLocation();
    if(Phase==ECityPhase::Gate || Phase==ECityPhase::Extract)
    {
        // Optional render probe. Take a first-person view across the exit wall before walking a route.
        if (Depth == 0 && Phase == ECityPhase::Gate && BotDoor == INDEX_NONE)
        {
            FString Shot;
            if (FParse::Value(FCommandLine::Get(),TEXT("CityDoorShot="),Shot))
            {
                if (BotStage == 0)
                {
                    P->SetActorLocation(FVector(430,0,100),false,nullptr,ETeleportType::TeleportPhysics);
                    PC()->SetControlRotation(FRotator(15,0,0));
                    BotStage = 1;
                }
                if (Age < 2) return;
                if (BotStage == 1) { FScreenshotRequest::RequestScreenshot(Shot,true,false); BotStage = 2; return; }
                BotStage = 0;
            }
        }
        const bool Extract = Phase == ECityPhase::Extract;
        if (!Extract && BotDoor == INDEX_NONE)
        {
            if (Offers.Num()!=3 || HasCampaignOverlay() || GetWorld()->IsPaused())
            { EndBot(false,TEXT("three non-modal doors expected")); return; }
            const bool Upper = TestRoute[FMath::Min(Depth,2)] == TEXT('U');
            for (int32 I=0; I<Offers.Num(); ++I)
                if (Offers[I].Upper == Upper) { BotDoor=I; if (FParse::Param(FCommandLine::Get(),TEXT("CityRouteFirstDoor"))) break; }
            if (BotDoor == INDEX_NONE) { EndBot(false,TEXT("missing route door")); return; }
        }
        const int32 Slot = Extract ? 1 : BotDoor;
        const FVector Base = Shell->GateBase(Slot);
        const float Lane = ACityRoomShell::GateY(Shell->IsUpperGate(Slot));
        FVector Goal;
        if (BotStage == 0)
        {
            Goal=FVector(-815,At.Y,At.Z);
            if (At.X < -755) ++BotStage;
        }
        else if (BotStage == 1)
        {
            Goal=FVector(-815,Lane,At.Z);
            if (FMath::Abs(At.Y-Lane)<35) ++BotStage;
        }
        else if (BotStage == 2)
        {
            Goal=FVector(1100,Lane,At.Z);
            if (At.X>1040) ++BotStage;
        }
        else if (BotStage == 3)
        {
            Goal=FVector(1120,Base.Y,At.Z);
            if (FMath::Abs(At.Y-Base.Y)<40) ++BotStage;
        }
        else Goal=FVector(1500,Base.Y,At.Z);
        PC()->SetControlRotation((Goal-At).GetSafeNormal2D().Rotation());
        WalkTo(P,Goal);
        return;
    }
    if(Phase!=ECityPhase::Combat) return;
    if (Depth == 0 && !SwordCaptureFolder.IsEmpty() && CaptureSword()) return;
    if (bFast && Depth==4 && !EnemyCueFolder.IsEmpty())
    {
        CaptureEnemyCue();
        return;
    }
    if(bFast)
    {
        if(Depth==4)
        {
            ACityCampaignEnemy* Boss=nullptr;
            ACityCampaignEnemy* Support=nullptr;
            for(ACityCampaignEnemy* E : Enemies) if(IsValid(E))
            {
                if(E->IsBoss()) Boss=E;
                if(E->GetKind()==ECityFoe::Support) Support=E;
            }
            if(Boss && !CheckBossMechanic(Boss,Support,P)) { EndBot(false,TEXT("independent boss mechanic")); return; }
        }
        TArray<TObjectPtr<ACityCampaignEnemy>> Targets=Enemies;
        for(ACityCampaignEnemy* E : Targets) if(IsValid(E) && E->CurrentHP>0)
            UGameplayStatics::ApplyDamage(E,99999,PC(),P,nullptr);
        return;
    }
    const FVector Eye=P->GetFirstPersonCameraComponent()->GetComponentLocation();
    ACityCampaignEnemy* Best=nullptr;
    FVector Aim=FVector::ZeroVector;
    double BestScore=TNumericLimits<double>::Max();
    for(ACityCampaignEnemy* E : Enemies)
    {
        if(!IsValid(E) || E->CurrentHP<=0) continue;
        if(Current.Boss!=INDEX_NONE && BotStateTime<3.8f && (E->IsBoss() || E->GetKind()==ECityFoe::Support)) continue;
        FVector Point=E->GetActorLocation()+FVector(0,0,E->GetKind()==ECityFoe::Spire?72:34);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CityBotAim),false,P);
        FHitResult Hit;
        if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Point,ECC_Visibility,Query) && Hit.GetActor()!=E) continue;
        double Score=FVector::Dist(Eye,Point);
        if(E->GetKind()==ECityFoe::Support) Score-=3200;
        if(Score<BestScore) { Best=E; BestScore=Score; Aim=Point; }
    }
    if(!Best)
    {
        if(bFireHeld) { P->DoStopFiring(); bFireHeld=false; }
        if(Enemies.IsEmpty()) return;
        const FVector Goal=At.X<-650 && At.Y>-640 ? FVector(-900,-720,0) :
            (At.Y> -620 && At.X<100 ? FVector(-500,-720,0) : FVector(1120,-700,0));
        WalkTo(P,Goal);
        return;
    }
    PC()->SetControlRotation((Aim-Eye).Rotation());
    if(bBuildShowcase && BotStateTime>.3f)
    {
        UCityAbilities* Kit=P->GetCityAbilities();
        if(ShowcaseCasts==0)
        {
            Kit->CastPrimary();
            ++ShowcaseCasts;
            UE_LOG(LogTemp,Display,TEXT("City loadout primary=%s"),*Progress()->GetActiveSkill(0).ToString());
        }
        else if(ShowcaseCasts==1 && BotStateTime>1.6f)
        {
            Kit->CastSecondary();
            ++ShowcaseCasts;
            UE_LOG(LogTemp,Display,TEXT("City loadout secondary=%s"),*Progress()->GetActiveSkill(1).ToString());
        }
        else if(ShowcaseCasts==2 && BotStateTime>2.4f)
        {
            if((Depth==0 || Depth==4) && Kit->GetCastingId().IsNone())
            {
                Kit->CastUltimate();
                UE_LOG(LogTemp,Display,TEXT("City loadout ultimate=%s depth=%d"),*Kit->GetCastingId().ToString(),Depth);
            }
            ++ShowcaseCasts;
        }
        if(TestRoute==TEXT("UUU") && Depth==0 && ShowcaseCasts==1 && Best->GetKind()==ECityFoe::Melee)
        {
            if(P->GetCitySlot()==0) P->EquipKatana();
            if(FVector::Dist2D(At,Aim)<1250) P->DoSpecial();
        }
        else if(P->GetCitySlot()==1 && !P->IsKatanaDashing()) P->EquipRifle();
    }
    // Refire and reload by invoking the same pawn methods as the input mappings.
    if(BotClock>=.14f)
    {
        BotClock=0;
        P->DoStopFiring();
        bool Empty=false;
        for(TActorIterator<AShooterWeapon> It(GetWorld()); It; ++It)
            if(It->GetOwner()==P && !It->IsHidden() && It->GetMagazineSize()>0 && It->GetBulletCount()==0) { Empty=true; break; }
        if(Empty) P->DoReload(); else { P->DoStartFiring(); bFireHeld=true; }
    }
    const float Lane=FMath::Sin(BotTime*1.4f)*395;
    WalkTo(P,FVector(-850,Lane,0));
#endif
}