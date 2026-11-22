#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Progression/CityProgression.h"
#include "Progression/CityProfileIO.h"
#include "CityWeaponEffects.h"
#include "CityLocomotion.h"
#include "CityKatana.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Abilities/CityAbilities.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCityAffixCombatTest,"VerticalCity.Weapons.AffixCombat",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
void FCityAffixCombatTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    if (FParse::Param(FCommandLine::Get(),TEXT("CityAffixVerify")))
    {
        Names.Add(TEXT("AllInstances")); Commands.Add(TEXT("AllInstances"));
    }
}

bool FCityAffixCombatTest::RunTest(const FString&)
{
    FString File;
    if (!TestTrue(TEXT("Explicit isolated slot"),FParse::Value(FCommandLine::Get(),TEXT("CityProfile="),File))) return false;
    UWorld* W=nullptr;
    for (const auto& C:GEngine->GetWorldContexts())
        if(C.World() && C.World()->IsGameWorld() && C.World()->GetMapName().Contains(TEXT("L_Campaign"))) {W=C.World(); break;}
    if (!TestNotNull(TEXT("Loaded campaign map"),W)) return false;
    auto* PC=W->GetFirstPlayerController();
    if (!TestNotNull(TEXT("Player controller"),PC)) return false;
    // The campaign boots into a paused loadout screen. Exercise combat in an unpaused world.
    const bool WasPaused=W->IsPaused();
    if (WasPaused && !TestTrue(TEXT("Combat fixture unpaused"),UGameplayStatics::SetGamePaused(W,false))) return false;
    ON_SCOPE_EXIT { if (WasPaused) UGameplayStatics::SetGamePaused(W,true); };
    const FVector Origin(64000,48000,24000);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Blueprint=LoadClass<AShooterCharacter>(nullptr,TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter.BP_ShooterCharacter_C"));
    auto* P=W->SpawnActor<AShooterCharacter>(Blueprint,Origin,FRotator::ZeroRotator,Spawn);
    if (!TestNotNull(TEXT("Playable shooter pawn"),P)) return false;
    PC->Possess(P); P->SetTeam(0);
    auto* Progress=W->GetGameInstance()->GetSubsystem<UCityProgression>();
    auto* FX=P->CityEffects.Get(); auto* Move=P->GetCharacterMovement(); auto* Loco=P->CityLocomotion.Get();
    FRandomStream Rng(321);
    FCityProfile Kit;
    Kit.Equipped[0]=FCityItemCatalog::Generate(TEXT("rifle"),Rng); Kit.Equipped[1]=FCityItemCatalog::Generate(TEXT("sword"),Rng);
    TMap<FName,FString> Instances;
    for (const auto& E:FCityItemCatalog::Affixes())
    {
        FCityItem I;
        I.Kind=E.Family==TEXT("sword")?TEXT("sword"):E.Family==TEXT("shotgun") || E.Id==TEXT("impact")?TEXT("shotgun"):TEXT("rifle");
        I.Id=FGuid::NewGuid().ToString(EGuidFormats::Digits); I.Rarity=E.Tier; I.Affixes={E.Id};
        Kit.Bag.Add(I);Instances.Add(E.Id,I.Id);
    }
    FString Err;
    if (!TestTrue(TEXT("Valid full inventory on disk"),CityProfileIO::Write(File,Kit,Err) && Progress->OpenProfile(File))) return false;
    auto Enemy=[&](FVector At,ECityFoe Type)
    {
        auto* N=W->SpawnActor<ACityCampaignEnemy>(ACityCampaignEnemy::StaticClass(),At,FRotator::ZeroRotator,Spawn);
        N->Configure(Type);N->SetActorTickEnabled(false);N->GetCharacterMovement()->SetComponentTickEnabled(false);return N;
    };
    auto* Wall=W->SpawnActor<AActor>(AActor::StaticClass(),Origin,FRotator::ZeroRotator,Spawn);
    auto* Blocker=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Blocker);Blocker->SetBoxExtent(FVector(400,10,140));
    Blocker->SetCollisionProfileName(TEXT("BlockAll"));Blocker->SetCollisionObjectType(ECC_WorldStatic);
    Blocker->RegisterComponent();Wall->SetActorLocation(Origin+FVector(160,-110,0));
    auto* Occluded=Enemy(Origin+FVector(180,-210,0),ECityFoe::Melee);
    auto* Distant=Enemy(Origin+FVector(3500,0,0),ECityFoe::Melee);
    Occluded->CurrentHP=Distant->CurrentHP=1000;
    int32 Performance=0,Effects=0;
    for (const auto& E:FCityItemCatalog::Affixes())
    {
        const FName Id=E.Id;
        if (!TestTrue(TEXT("Equip and start: ")+Id.ToString(),Progress->Equip(Instances.FindChecked(Id),0) && Progress->BeginRun())) return false;
        P->SelectCitySlot(0); FX->ResetRoom();
        P->SetActorLocationAndRotation(Origin,FRotator::ZeroRotator);
        PC->SetControlRotation(FRotator::ZeroRotator);
        Loco->bSliding=false; Loco->bSprintHeld=false; Loco->NextSlide=0;
        P->CurrentHP=P->MaxHP; FX->RegenWait=0; FX->LastHP=1;
        Move->SetMovementMode(MOVE_Walking); Move->StopMovementImmediately();
        auto* Gun=P->GetCityWeapon(); auto* Blade=Cast<ACityKatana>(Gun);
        const auto* Eye=P->GetFirstPersonCameraComponent();
        const FVector Sight=Eye->GetForwardVector(), EyeAt=Eye->GetComponentLocation();
        auto* A=Enemy(Origin+FVector(125,0,0),Id==TEXT("boss_hunter")?ECityFoe::Breaker:ECityFoe::Melee);
        auto* B=Enemy(Origin+FVector(248,12,0),ECityFoe::Melee);
        A->CurrentHP=Id=="opener" || E.Event=="opening"?A->MaxHealth():1000;B->CurrentHP=1000;
        FHitResult Head;
        Head.ImpactPoint=A->GetActorLocation()+FVector(0,0,60);
        Head.TraceStart=EyeAt;Head.TraceEnd=Head.ImpactPoint;
        auto Hit=[&](float Amount=1,bool Weak=false)
        { return P->HitCityTarget(A,Amount,Blade!=nullptr,Weak?&Head:nullptr); };
        auto DamagePlayer=[&](float Amount,AActor* Source)
        { const float Before=P->CurrentHP;UGameplayStatics::ApplyDamage(P,Amount,nullptr,Source,nullptr);return Before-P->CurrentHP; };
        auto AdvanceFX=[&](float Dt){FX->TickComponent(Dt,LEVELTICK_All,nullptr);};
        auto Shoot=[&](int32 Rounds)
        {
            A->SetActorLocation(EyeAt+Sight*650);
            B->SetActorLocation(EyeAt+Sight*1000);
            Gun->CurrentBullets=Rounds; Gun->Spread=0;Gun->BurstRemaining=1;Gun->CancelReload();Gun->CityRound();
        };
        auto Near=[](float X,float Y){return FMath::IsNearlyEqual(X,Y,.045f);};
        bool Ok=false;
        FString What;
        if (!E.Event.IsNone())
        {
            const FName Event=E.Event;
            const bool Action=Event==TEXT("reload") || Event==TEXT("slide") || Event==TEXT("parry");
            if (Event==TEXT("chilled")) FX->Status.FindOrAdd(A).Frost=1;
            if (Event==TEXT("burning")) {auto& S=FX->Status.FindOrAdd(A); S.Fire=.01f;S.BurnTick=.5f;}
            if (Event==TEXT("stunned")) A->StunFor(.3f);
            if (Event==TEXT("slide_hit")) Loco->bSliding=true;
            if (Event==TEXT("air_hit")) Move->SetMovementMode(MOVE_Falling);
            if (Event==TEXT("sealed")) Hit();
            if (Event==TEXT("full_seal")) {Hit();Hit();Hit();}
            if (Event==TEXT("third")) {Hit();Hit();}
            if (Event==TEXT("kill")) {A->CurrentHP=1;if (Id==TEXT("arcane_transfer")) FX->Status.FindOrAdd(A).Seals=2;}
            const bool Single=E.Shape==TEXT("single");
            ACityCampaignEnemy* Measure=Single?A:B;
            if (Event==TEXT("weak"))
            {
                // Place the rifle ray inside the upper capsule. CityRound computes the hit location.
                A->SetActorLocation(EyeAt+Sight*600-FVector(0,0,60));
                B->SetActorLocation(A->GetActorLocation()+FVector(125,12,0));
            }
            const float Before=Measure->CurrentHP;
            const FVector Where=Measure->GetActorLocation();
            if (Action) P->TriggerCityAction(Event);
            else if(Event==TEXT("weak")) {Gun->Spread=0;Gun->BurstRemaining=1;Gun->CityRound();}
            else Hit(1);
            const float BaseDirect=Single && !Action?(Event==TEXT("weak")?19.f*1.5f:1.f):0.f;
            const float Immediate=Before-Measure->CurrentHP-BaseDirect;
            if (E.Shape==TEXT("echo"))
            {
                AdvanceFX(.44f);
                const bool Delayed=Near(Before-Measure->CurrentHP-BaseDirect,0);
                AdvanceFX(.02f);
                Ok=Delayed && Before-Measure->CurrentHP>=E.Damage-.03f;
            }
            else
            {
                Ok=Immediate>=E.Damage-.03f;
                if (E.Shape==TEXT("field"))
                {
                    float AfterFirst=Measure->CurrentHP;
                    // Return the displaced fixture target to its original cell between separate waves.
                    Measure->SetActorLocation(Where);
                    AdvanceFX(.56f);const float AfterSecond=Measure->CurrentHP;
                    Measure->SetActorLocation(Where);
                    AdvanceFX(.55f);
                    Ok &= AfterFirst-AfterSecond>=E.Damage-.03f && AfterSecond-Measure->CurrentHP>=E.Damage-.03f;
                }
            }
            auto S=FX->Status.FindRef(Measure);
            if (E.School==TEXT("fire")) Ok &= S.Fire>0;
            if (E.School==TEXT("frost")) Ok &= FX->MovementScale(Measure)<.66f && (E.Shape!=TEXT("freeze") || Measure->IsStunned());
            if (E.School==TEXT("volt")) Ok &= Measure->IsStunned();
            if (E.School==TEXT("arcane") && E.SealGain>0) Ok &= S.Seals >= (Id==TEXT("arcane_transfer")?3:E.SealGain);
            if (E.School==TEXT("void") || E.School==TEXT("blast")) Ok &= FVector::DistSquared(Where,Measure->GetActorLocation())>1;
            const bool Cooled=FX->Ready.FindRef(Instances[Id]+Id.ToString())>W->GetTimeSeconds();
            Ok &= Cooled && Near(Occluded->CurrentHP,1000) && Near(Distant->CurrentHP,1000);
            What=FString::Printf(TEXT("%s/%s hp %.1f->%.1f proc=%.1f cd=%d"),*Event.ToString(),*E.Shape.ToString(),Before,Measure->CurrentHP,Immediate,int32(Cooled));
            if (Ok) ++Effects;
        }
        else
        {
            const auto Stats=Progress->GetStats(0);
            if (Id=="damage") Ok=Near(Hit(Gun->GunDamage),19*1.12f);
            else if (Id=="impact") {P->HitCityTarget(A,10,false,nullptr,Stats.Impact*4);Ok=Near(Stats.Impact,13.5f) && A->IsStunned();}
            else if (Id=="rapid") Ok=FMath::IsNearlyEqual(Gun->CityInterval(),.115f/1.12f,.0001f);
            else if (Id=="long_barrel") Ok=Near(Gun->Range,8400);
            else if (Id=="precision") Ok=FMath::IsNearlyEqual(Gun->Spread,.0096f,.00001f);
            else if (Id=="buckshot") Ok=Gun->Pellets==10;
            else if (Id=="quick_blade" || Id=="reach" || Id=="wide_sweep")
            {
                const FVector Dir=Id=="wide_sweep"?Sight.RotateAngleAxis(62,FVector::UpVector):Sight;
                A->SetActorLocation(EyeAt+Dir*(Id=="reach"?340:260));
                B->SetActorLocation(Origin+FVector(0,1200,0)); Blade->CancelAttackInput(); Blade->NextSlash=0;
                Blade->StartFiring(); Blade->StopFiring();
                Blade->AdvanceStroke(W->GetTimeSeconds()+Blade->StrokeDuration*.7f);
                Ok=A->CurrentHP<1000 && (Id!="quick_blade" || Near(Blade->NextSlash-W->GetTimeSeconds(),.52f/1.14f));
                Blade->CancelAttackInput();
            }
            else if (Id=="weakpoint") Ok=Near(Hit(10,true),18.75f);
            else if (Id=="burn") {Hit();float Prev=A->CurrentHP;AdvanceFX(2.001f);Ok=Near(Prev-A->CurrentHP,12);}
            else if (Id=="opener" || Id=="execute" || Id=="boss_hunter" || Id=="desperate" || Id=="steady" ||
                     Id=="point_blank" || Id=="longshot" || Id=="punish" || Id=="kindling" || Id=="air_strike" || Id=="slide_strike")
            {
                if(Id=="execute") A->CurrentHP=25;
                if(Id=="desperate") P->CurrentHP=20;
                if(Id=="longshot") A->SetActorLocation(Origin+FVector(1600,0,0));
                if(Id=="punish") A->StunFor(1);
                if(Id=="kindling") FX->Burn(A);
                if(Id=="air_strike") Move->SetMovementMode(MOVE_Falling);
                if(Id=="slide_strike") Loco->bSliding=true;
                Ok=Near(Hit(10),10*(1+E.Value));
            }
            else if (Id=="magazine") Ok=Gun->GetMagazineSize()==38 && Gun->GetBulletCount()==38;
            else if (Id=="reserve") Ok=Gun->GetCityReserve()==312;
            else if (Id=="reload" || Id=="empty_reload" || Id=="tactical_reload")
            {
                Gun->CurrentBullets=Id=="empty_reload"?0:5; Gun->CancelReload();
                Ok=Gun->StartReload() && Near(W->GetTimerManager().GetTimerRate(Gun->ReloadTimer),1.65f*(1-E.Value));
                Gun->CancelReload();
            }
            else if (Id=="scavenger"){Gun->Reserve=0;Gun->GiveReserve(.35f);Ok=Gun->Reserve==110;}
            else if (Id=="bounty"){Gun->Reserve=0;A->CurrentHP=1;Hit(2);Ok=Gun->Reserve==3;}
            else if (Id=="first_round" || Id=="last_round" || Id=="pierce")
            {
                Shoot(Id=="last_round"?1:Gun->MagazineSize);
                Ok=Id=="pierce"?Near(1000-B->CurrentHP,19*.6f):Near(1000-A->CurrentHP,19*(1+E.Value));
            }
            else if (Id=="lightweight" || Id=="sprint")
            {if(Id=="sprint")Loco->SprintPressed(); Loco->TickComponent(.016f,LEVELTICK_All,nullptr); Ok=Near(Move->MaxWalkSpeed,(Id=="sprint"?1050:700)*(1+E.Value));}
            else if (Id=="slide_speed" || Id=="slide_time" || Id=="slide_cd")
            {
                P->AddMovementInput(FVector::ForwardVector,1,true);Loco->StartSlide();
                Ok=Loco->bSliding && (Id=="slide_speed"?Near(Move->Velocity.Size2D(),1610):Id=="slide_time"?Near(Loco->SlideEnd-W->GetTimeSeconds(),.84f):Near(Loco->NextSlide-W->GetTimeSeconds(),.8f));
                if(Loco->bSliding)Loco->FinishSlide();
            }
            else if (Id=="jump"){P->DoJumpStart();Ok=Near(Move->JumpZVelocity,825);P->DoJumpEnd();}
            else if (Id=="dash_cd"){Blade->NextSlash=0;Blade->NextDash=0;Blade->RequestDash();Ok=Blade->IsDashing() && Near(Blade->NextDash-W->GetTimeSeconds(),1.875f);Blade->EndTravel(false);}
            else if (Id=="parry_range"){A->SetActorLocation(Origin+FVector(408,0,0));B->SetActorLocation(Origin+FVector(0,1200,0));A->Windup=1;P->CityParry();Ok=A->IsStunned();}
            else if (Id=="armor" || Id=="melee_guard" || Id=="ranged_guard" || Id=="last_stand")
            {if(Id=="last_stand")P->CurrentHP=P->MaxHP*.3f;Ok=Near(DamagePlayer(10,Id=="ranged_guard"?nullptr:A),10*(1-E.Value));}
            else if (Id=="regen_rate" || Id=="regen_delay" || Id=="regen_cap")
            {
                P->CurrentHP=P->MaxHP*(Id=="regen_cap"?.2f:.1f); const float Before=P->CurrentHP;
                FX->LastHP=Id=="regen_delay"?1:P->GetCityHealthFraction();
                AdvanceFX(Id=="regen_delay"?3.01f:.1f);
                Ok=Id=="regen_rate"?Near(P->CurrentHP-Before,.65f):P->CurrentHP>Before;
            }
            else if (Id=="leech" || Id=="kill_heal")
            {P->CurrentHP=40;A->CurrentHP=Id=="leech"?3:1;Hit(100);Ok=Near(P->CurrentHP-40,Id=="leech"?.15f:5.f);}
            else if (Id=="kill_energy")
            {
                P->GetCityAbilities()->AddWeaponEnergy(-100);
                A->CurrentHP=1;Hit(2);
                Ok=Near(P->GetCityAbilities()->GetEnergy(),28);
            }
            if (Ok) ++Performance;
            What=FString::Printf(TEXT("hp=%.2f enemy=%.2f secondary=%.2f energy=%.1f speed=%.1f jump=%.1f damage=%.3f impact=%.3f range=%.1f spread=%.4f interval=%.4f pellets=%d clip=%d/%d reserve=%d reload=%.3f"),P->CurrentHP,A->CurrentHP,B->CurrentHP,P->GetCityAbilities()->GetEnergy(),Move->MaxWalkSpeed,Move->JumpZVelocity,Stats.Damage,Stats.Impact,Stats.RangeMetres,Stats.Spread,Stats.Interval,Stats.Pellets,Gun->CurrentBullets,Gun->MagazineSize,Gun->Reserve,Gun->ReloadDuration);
        }
        AddInfo(FString::Printf(TEXT("CITY_AFFIX_CASE %s %s %s"),*Id.ToString(),Ok?TEXT("PASS"):TEXT("FAIL"),*What));
        TestTrue(Id.ToString()+TEXT(" actual effect: ")+What,Ok);
        if(IsValid(A)) A->Destroy(); if(IsValid(B)) B->Destroy();
        Loco->bSliding=false;FX->ResetRoom();
        if (!TestTrue(TEXT("Settle ")+Id.ToString(),Progress->FinishRun(true))) return false;
    }
    // The original aftercut snapshots its blade at the end of travel, even if the player swaps guns before rupture.
    FCityProfile CutKit=Kit;
    CutKit.Equipped[0].Affixes.Reset();
    CutKit.Equipped[1].Affixes={FName("cinder_needle")};
    CutKit.Equipped[1].DamageRoll=13500;
    Progress->UseLabProfile(CutKit);
    P->SetActorLocationAndRotation(Origin,FRotator::ZeroRotator);
    PC->SetControlRotation(FRotator::ZeroRotator);
    P->EquipKatana();
    auto* CutBlade=Cast<ACityKatana>(P->GetCityWeapon());
    bool CachedCut=false;
    if (CutBlade)
    {
        FX->ResetRoom();
        auto* Target=Enemy(Origin+FVector(300,0,0),ECityFoe::Shooter);
        Target->CurrentHP=1000;
        Move->SetMovementMode(MOVE_Walking);
        const float Expected=Progress->GetStats(1).Damage+4;
        CutBlade->StartDash(); CutBlade->StepDash(.12f); CutBlade->EndTravel(true);
        const bool Armed=CutBlade->Marks.Contains(Target) && FMath::IsNearlyEqual(CutBlade->CutAt-W->GetTimeSeconds(),1.,.001);
        P->EquipRifle();
        CutBlade->Tick(0);
        const bool Deferred=Target->CurrentHP==1000 && P->GetCityWeapon()!=CutBlade;
        CutBlade->StrikeMarks(CutBlade->CutAt-.01);
        const bool StillDeferred=Target->CurrentHP==1000;
        CutBlade->StrikeMarks(CutBlade->CutAt);
        const float Delta=1000-Target->CurrentHP;
        CachedCut=Armed && Deferred && StillDeferred && FMath::IsNearlyEqual(Delta,Expected,.05f) && Target->IsStunned();
        AddInfo(FString::Printf(TEXT("CITY_AFTERCUT_SWAP marked=%d deferred=%d damage=%.3f expected=%.3f"),Armed,Deferred,Delta,Expected));
        Target->Destroy();
    }
    TestTrue(TEXT("Armed slash after switching retains blade damage, affix and source"),CachedCut);
    Progress->LeaveLabProfile();
    AddInfo(FString::Printf(TEXT("CITY_AFFIX_TOTAL numeric=%d/50 effects=%d/60"),Performance,Effects));
    return Performance==50 && Effects==60 && CachedCut;
}
#endif

