#include "Abilities/CityAbilities.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/CityCastView.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Campaign/CityCampaignMode.h"
#include "Framework/Application/SlateApplication.h"
#include "Campaign/CityEnemyBolt.h"
#include "Progression/CityProgression.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/World.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "HAL/PlatformMisc.h"

struct FCityAbilityPhaseFixture
{
    static void Enter(ACityCampaignMode& Mode,ECityPhase Phase) { Mode.SetPhase(Phase); }
};

// Run Lvl_Shooter -game -CityAbilityVerify. This fixture lives in the standalone
// process, creates real actors/colliders and exits with its assertion status.
void UCityAbilities::RunAbilityVerification()
{
    if (!Player || !Player->IsLocallyControlled()) return;
    int32 Failures=0, Cases=0;
    auto Check=[&](FName Id,const TCHAR* Assertion,bool Ok)
    {
        UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_CHECK %s %s %s"),*Id.ToString(),Assertion,Ok?TEXT("PASS"):TEXT("FAIL"));
        if (!Ok) ++Failures;
    };
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Box=[&](FVector At,FVector Extent){
        auto* A=GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),At,FRotator::ZeroRotator,Spawn);
        auto* B=NewObject<UBoxComponent>(A);
        A->SetRootComponent(B); B->SetBoxExtent(Extent); B->SetCollisionProfileName(TEXT("BlockAll"));
        B->SetCollisionObjectType(ECC_WorldStatic); B->RegisterComponent(); A->SetActorLocation(At);
        return A;
    };
    const FVector Origin(36000,36000,20000);
    Box(Origin-FVector(0,0,30),FVector(4500,4500,30));
    Box(Origin+FVector(1450,0,150),FVector(25,1000,160));
    auto Victim=[&](FVector At){
        auto* N=GetWorld()->SpawnActor<ACityCampaignEnemy>(ACityCampaignEnemy::StaticClass(),At,FRotator::ZeroRotator,Spawn);
        N->Configure(ECityFoe::Melee); N->SetActorTickEnabled(false); N->GetCharacterMovement()->SetComponentTickEnabled(false);
        N->CurrentHP=30000; return N;
    };
    auto* A=Victim(Origin+FVector(650,0,92)); auto* B=Victim(Origin+FVector(650,210,92));
    Player->EquipRifle();
    auto Advance=[this](float Seconds){
        const int32 Steps=FMath::CeilToInt(Seconds*60);
        for (int32 I=0; I<Steps; ++I) TickComponent(1.f/60,LEVELTICK_All,nullptr);
    };
    auto Place=[&](float Distance){
        A->SetActorLocation(Origin+FVector(Distance,0,92),false,nullptr,ETeleportType::TeleportPhysics);
        B->SetActorLocation(Origin+FVector(Distance,210,92),false,nullptr,ETeleportType::TeleportPhysics);
        Player->SetActorLocation(Origin+FVector(0,0,Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2),false,nullptr,ETeleportType::TeleportPhysics);
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Player->SetActorRotation(FRotator::ZeroRotator);
        Player->GetController()->SetControlRotation((A->GetActorLocation()-Eye()).Rotation());
    };
    auto FireBolt=[&](FVector At,FVector Dir,float Speed=900.f,float Damage=11.f){
        FActorSpawnParameters Args=Spawn;
        Args.Owner=A; Args.Instigator=A;
        auto* Bolt=GetWorld()->SpawnActor<ACityEnemyBolt>(At,Dir.Rotation(),Args);
        Bolt->Launch(Dir,Speed,Damage);
        return Bolt;
    };
    auto Fly=[](ACityEnemyBolt* Bolt,float Seconds){
        for (int32 I=0; I<FMath::CeilToInt(Seconds*120); ++I)
            if (IsValid(Bolt)) Bolt->Tick(1.f/120); else break;
    };
    auto TapKey=[&](FKey Key){
        auto* PC=Cast<APlayerController>(Player->GetController());
        auto* Input=Player->FindComponentByClass<UEnhancedInputComponent>();
        if (PC && PC->PlayerInput && Input)
        {
            for (EInputEvent Event:{IE_Pressed,IE_Released})
            {
                PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,
                    Event==IE_Pressed?1.f:0.f,false,FPlatformTime::Cycles64()));
                PC->PlayerInput->ProcessInputStack({Input},1.f/60,false);
            }
        }
    };
    auto PressSlot=[&](FName Id,int32 Slot){
        FCityProfile Loadout=Progress->GetProfile();
        Loadout.Skills[Slot]=Id;
        Progress->UseLabProfile(Loadout);
        TapKey(Slot==0?EKeys::C:EKeys::X);
        Progress->LeaveLabProfile();
        return GetCooldown(Id)>0;
    };
    for (const auto& D:FCityAbilityCatalog::All())
    {
        const FName Id=D.Id;
        ClearRoomEffects(); Cooldowns.Reset(); UltimateCooldown=0; Energy=100;
        A->CurrentHP=B->CurrentHP=30000;
        Place(Id=="frenzy"||Id=="slide_surge"?240:650);
        const FVector Start=Player->GetActorLocation();
        const float Before=A->CurrentHP;
        FHitResult FixtureHit;
        const FVector Endpoint=EndPoint(1400,FixtureHit);
        UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_FIXTURE %s eye=%s aim=%s target=%s contact=%s at=%s"),*Id.ToString(),*Eye().ToCompactString(),*Aim().ToCompactString(),*A->GetActorLocation().ToCompactString(),*GetNameSafe(FixtureHit.GetActor()),*Endpoint.ToCompactString());
        const bool C=Id=="deflect_plate" || Id=="bullet_sink";
        const bool X=Id=="barrier_wall" || Id=="smoke_veil";
        const bool Ok=(C || X)?PressSlot(Id,C?0:1):TryCast(Id);
        if (C || X) Check(Id,C?TEXT("equipped_C_pressed"):TEXT("equipped_X_pressed"),Ok);
        Check(Id,TEXT("cast"),Ok); if (!Ok) continue;
        ++Cases;
        if (D.bUltimate)
        {
            Check(Id,TEXT("shared_resource"),Energy==0 && UltimateCooldown>0);
            Advance(.22f);
            FMinimalViewInfo View; View.Location=Eye(); View.Rotation=Player->GetControlRotation();
            FrameCamera(.22f,View);
            Check(Id,TEXT("shoulder_camera"),FVector::Dist(View.Location,Eye())>80);
            Advance(D.Windup-.20f);
            if (Id=="barrage")
            {
                int32 Left=0, RightCount=0;
                bool BothConverge=true;
                for (const auto& F:Flights) if (F.Id==Id)
                {
                    if (F.Spin<0) ++Left; else ++RightCount;
                    const FVector Side=Right();
                    const float Middle=FMath::Abs((F.Path[F.Path.Num()/2]-F.Origin).Dot(Side));
                    const float End=FMath::Abs((F.Path.Last()-F.Origin).Dot(Side));
                    BothConverge &= Middle>End;
                }
                Check(Id,TEXT("nine_opposed_curves"),Left==4 && RightCount==5 && BothConverge);
            }
            if (Id=="hammer"||Id=="frenzy") Check(Id,TEXT("alternate_primary"),HandlePrimaryAttack());
            if (Id=="well"||Id=="ice") Check(Id,TEXT("field_deployment"),Fields.Num()==(Id=="ice"?9:1));
            if (Id=="well") Check(Id,TEXT("damage_aura"),WeaponDamage(A,100,false)>125);
            Advance(Id=="storm"||Id=="charge"?.68f:1.2f);
            if (Id=="storm"||Id=="charge") Check(Id,TEXT("swept_dash"),FVector::Dist(Start,Player->GetActorLocation())>150);
            if (Id=="storm") Check(Id,TEXT("repeat_dash"),TryCast(Id) && Charges==1);
            if (Id=="anchor") Check(Id,TEXT("anchor_control"),Fields.Num()==1 && A->IsStunned());
            else if (Id!="well") Check(Id,TEXT("target_damage"),A->CurrentHP<Before);
            if (Id=="beam"||Id=="hammer"||Id=="frenzy") Check(Id,TEXT("cancel_stance"),TryCast(Id));
            Advance(7);
            View.Location=Eye(); FrameCamera(.1f,View);
            Check(Id,TEXT("first_person_restored"),Phase.IsNone() && FVector::Dist(View.Location,Eye())<1);
        }
        else
        {
            Check(Id,TEXT("cooldown_guard"),GetCooldown(Id)>0 && !TryCast(Id));
            if (Id=="frost_floor" || Id=="smoke_veil" || Id=="bullet_sink")
            {
                Check(Id,TEXT("ballistic_canister"),Flights.Num()==1 && Fields.IsEmpty());
                Advance(1.4f);
            }
            if (Id=="echo_pin") { Advance(.2f); float Prev=B->CurrentHP; OnWeaponHit(A,50,false,false); Check(Id,TEXT("echo_chain"),B->CurrentHP<Prev); }
            else if (Id=="arc_lance") Check(Id,TEXT("chain_hits"),A->CurrentHP<Before && B->CurrentHP<Before);
            else if (Id=="ricochet_disc") { Advance(.10f); FVector P=Flights[0].Point; Check(Id,TEXT("disc_detonation"),OnGunRay(Eye(),P) && Flights.IsEmpty()); }
            else if (Id=="seeker_swarm") { Check(Id,TEXT("six_missiles"),Flights.Num()==6); Advance(1); Check(Id,TEXT("homing_damage"),A->CurrentHP<Before); }
            else if (Id=="shrapnel_mine") { Advance(.8f); Check(Id,TEXT("contact_detonation"),A->CurrentHP<Before); }
            else if (Id=="cinch_post") { const FVector P=A->GetActorLocation(); Advance(2); Check(Id,TEXT("pull_or_stun"),FVector::Dist(P,A->GetActorLocation())>10 || A->IsStunned()); }
            else if (Id=="overclock_round") { OnGunShot(); Check(Id,TEXT("round_modifier"),HasOverclockPenetration() && WeaponDamage(A,100,false)>155); }
            else if (Id=="magnet_reload") Check(Id,TEXT("magnetic_rate"),ModifyFireInterval(1)<.8);
            else if (Id=="finisher_lens") { A->CurrentHP=30; Check(Id,TEXT("execute_threshold"),WeaponDamage(A,100,false)>=200); }
            else if (Id=="patch_kit") { Advance(1.6f); Check(Id,TEXT("clean_shield"),FilterIncomingDamage(20,A)==5); }
            else if (Id=="deflect_plate")
            {
                auto* Bolt=FireBolt(Eye()+Aim()*230,-Aim());
                Fly(Bolt,.15f);
                Check(Id,TEXT("original_bolt_changes_team"),IsValid(Bolt) && Bolt->IsFriendly() && Bolt->GetOwner()==Player);
                Check(Id,TEXT("reflected_speed_damage"),FMath::IsNearlyEqual(Bolt->FlightVelocity().Size(),1800.,1.) && Bolt->DamageAmount()==27.5f);
                Check(Id,TEXT("return_travels_before_hit"),A->CurrentHP==Before);
                Fly(Bolt,1);
                Check(Id,TEXT("deflect"),!IsValid(Bolt) && FMath::IsNearlyEqual(A->CurrentHP,Before-27.5f));
                const float InitialCD=GetCooldown(Id)+.5f;
                for (int32 I=0; I<9; ++I)
                {
                    auto* Extra=FireBolt(Eye()+Aim()*160,-Aim());
                    Fly(Extra,.08f); Extra->Destroy();
                }
                Check(Id,TEXT("refund_capped_at_four_seconds"),FMath::IsNearlyEqual(InitialCD-GetCooldown(Id),4.f));
                auto* Miss=FireBolt(Eye()+Aim()*190+Right()*160,-Aim());
                Fly(Miss,.15f);
                Check(Id,TEXT("outside_plate_passes"),IsValid(Miss) && !Miss->IsFriendly());
                Miss->Destroy();
                const FVector NearFront=Player->GetActorLocation()+Player->GetActorForwardVector()*160;
                A->SetActorLocation(NearFront);
                Check(Id,TEXT("front_melee_guard_knockback"),FilterIncomingDamage(14,A)==0 && A->IsStunned() && FVector::Dist(A->GetActorLocation(),NearFront)>100);
                A->SetActorLocation(Player->GetActorLocation()-Player->GetActorForwardVector()*160);
                Check(Id,TEXT("rear_melee_passes"),FilterIncomingDamage(14,A)==14);
                Advance(1.51f);
                auto* Late=FireBolt(Eye()+Aim()*190,-Aim());
                Fly(Late,.12f);
                Check(Id,TEXT("guard_expires_before_fire_lock"),!Late->IsFriendly() && HandlePrimaryAttack());
                Late->Destroy();
            }
            else if (Id=="phase_step") { Advance(.14f); Check(Id,TEXT("phase_move_and_immunity"),FVector::Dist(Start,Player->GetActorLocation())>450 && FilterIncomingDamage(10,A)==0); }
            else if (Id=="slide_surge"||Id=="grapple_hook") { Advance(.4f); Check(Id,TEXT("displacement"),FVector::Dist(Start,Player->GetActorLocation())>80); }
            else if (Id=="vault_jet") Check(Id,TEXT("jet_impulse"),bJet && Player->GetCharacterMovement()->PendingLaunchVelocity.Z>=1090);
            else if (!Fields.IsEmpty())
            {
                const auto& F=Fields.Last();
                if (Id=="bullet_sink")
                {
                    const FVector Center=F.Point, Side=FVector::RightVector;
                    A->SetActorLocation(Center+FVector(-230,0,0));
                    B->SetActorLocation(Center+FVector(0,-230,0));
                    auto* Curving=FireBolt(Center+Side*240,FVector::ForwardVector,200);
                    Fly(Curving,.06f);
                    Check(Id,TEXT("real_bolt_curves_without_capture"),IsValid(Curving) && Curving->FlightVelocity().Y< -160 && F.Stored==0);
                    Curving->Destroy();
                    auto* Cover=Box(Center+Side*120,FVector(50,8,80));
                    auto* Hidden=FireBolt(Center+Side*240,FVector::ForwardVector,200);
                    Fly(Hidden,.05f);
                    Check(Id,TEXT("solid_cover_blocks_attraction"),IsValid(Hidden) && FMath::Abs(Hidden->FlightVelocity().Y)<.1);
                    Hidden->Destroy(); Cover->Destroy();
                    auto* First=FireBolt(Center+Side*105,-Side);
                    Fly(First,.12f);
                    Check(Id,TEXT("absorbs_bolt"),!IsValid(First) && F.Stored==1);
                    for (int32 I=1; I<24; ++I) Fly(FireBolt(Center+Side*105,-Side),.12f);
                    auto* Overflow=FireBolt(Center+Side*105,-Side);
                    Fly(Overflow,.13f);
                    Check(Id,TEXT("capacity_24_then_bypass"),F.Stored==24 && IsValid(Overflow) && FMath::IsNearlyEqual(Overflow->FlightVelocity().Size(),900.,.1));
                    Overflow->Destroy();
                    const float BeforeA=A->CurrentHP, BeforeB=B->CurrentHP;
                    Advance(F.Life+.03f);
                    Check(Id,TEXT("expiry_releases_24_live_fragments"),Fields.IsEmpty() && ReturnedBolts.Num()==24);
                    Check(Id,TEXT("five_round_area_damage"),A->CurrentHP==BeforeA-40 && B->CurrentHP==BeforeB-40);
                    auto* FragmentCover=Box(Center+FVector(-115,0,0),FVector(10,65,80));
                    for (const auto& Fragment:ReturnedBolts) if (Fragment.IsValid()) Fly(Fragment.Get(),.3f);
                    Check(Id,TEXT("released_rounds_respect_new_cover"),A->CurrentHP==BeforeA-40);
                    Check(Id,TEXT("nearby_targets_share_15_damage_rounds"),B->CurrentHP==BeforeB-40-12*15);
                    FragmentCover->Destroy();
                }
                if (Id=="barrier_wall")
                {
                    const FVector Mid=F.Point+FVector::UpVector*95, Out=F.Direction;
                    auto* Early=FireBolt(Mid+Out*100,-Out);
                    Fly(Early,.16f);
                    Check(Id,TEXT("construction_passes_bolt"),IsValid(Early) && F.Health==400);
                    Early->Destroy(); Advance(.26f);
                    auto* EnemyShot=FireBolt(Mid+Out*100,-Out);
                    Fly(EnemyShot,.18f);
                    Check(Id,TEXT("one_way_wall"),!IsValid(EnemyShot) && F.Health==389);
                    const FVector Target=Mid+Out*260;
                    A->SetActorLocation(Target);
                    const float OldHP=A->CurrentHP;
                    auto* Friendly=FireBolt(Mid-Out*100,Out);
                    Friendly->ReturnToPlayer(Player,Out,1000,15,1400);
                    Fly(Friendly,.45f);
                    Check(Id,TEXT("player_projectile_passes"),A->CurrentHP==OldHP-15 && F.Health==389);
                    A->SetActorLocation(Mid+Out*100);
                    FHitResult Sweep;
                    A->AddActorWorldOffset(-Out*200,true,&Sweep);
                    Check(Id,TEXT("real_capsule_blocked"),Sweep.GetActor()==F.CollisionWall.Get());
                    Check(Id,TEXT("steering_around_edge"),!BarrierWaypoint(Mid+Out*200,Mid-Out*200).Equals(Mid-Out*200));
                    auto* Fatal=FireBolt(Mid+Out*100,-Out,900,400);
                    Fly(Fatal,.2f); Advance(.02f);
                    Check(Id,TEXT("health_destruction_removes_collision"),!IsValid(Fatal) && Fields.IsEmpty());
                }
                if (Id=="frost_floor")
                {
                    A->SetActorLocation(F.Point+FVector::UpVector*90);
                    Check(Id,TEXT("slow"),EnemySpeedScale(A)<1);
                }
                if (Id=="turret_drone"||Id=="shock_fence") { Advance(.8f); Check(Id,TEXT("deployable_damage"),A->CurrentHP<Before); }
                if (Id=="decoy_holo") { FVector P; Check(Id,TEXT("decoy_target"),GetEnemyAimPoint(A,Start,P) && FVector::Dist(P,F.Point)<1); }
                if (Id=="smoke_veil")
                {
                    const FVector P=F.Point;
                    A->SetActorLocation(P+FVector(800,0,0));
                    FVector Target; Check(Id,TEXT("sight_block"),!GetEnemyAimPoint(A,P-FVector(800,0,0),Target));
                    A->SetActorLocation(P+FVector(0,150,90));
                    Check(Id,TEXT("inside_keeps_target_six_degree_error"),GetEnemyAimPoint(A,Start,Target) && SmokeSpread(A,-FVector::ForwardVector)==6);
                    auto* Heavy=GetWorld()->SpawnActor<ACityCampaignEnemy>(P+FVector(0,800,90),FRotator::ZeroRotator,Spawn);
                    Heavy->Configure(ECityFoe::Spire);
                    Check(Id,TEXT("boss_tracks_with_three_degree_error"),GetEnemyAimPoint(Heavy,P,Target) && SmokeSpread(Heavy,-FVector::RightVector)==3);
                    Heavy->Destroy();
                    auto* Through=FireBolt(P+FVector(0,450,160),-FVector::RightVector,900);
                    Fly(Through,.2f);
                    Check(Id,TEXT("inflight_enemy_round_crosses_smoke"),IsValid(Through) && Through->GetActorLocation().Y<P.Y+280);
                    Through->Destroy();
                    // Exercise the campaign shooter's perception, telegraph and actual spawned bolt.
                    A->SetActorLocation(P+FVector(-700,650,92));
                    B->SetActorLocation(P+FVector(-700,860,92));
                    Player->SetActorLocation(P+FVector(0,-800,90));
                    auto* Gunner=GetWorld()->SpawnActor<ACityCampaignEnemy>(P+FVector(0,800,90),FRotator::ZeroRotator,Spawn);
                    Gunner->Configure(ECityFoe::Shooter);
                    Gunner->Tick(1.5f);
                    Check(Id,TEXT("outside_shooter_cannot_start_volley"),!Gunner->IsWindingUp());
                    Gunner->SetActorLocation(P+FVector(0,350,90));
                    Gunner->Tick(.02f);
                    Check(Id,TEXT("inside_shooter_starts_volley"),Gunner->IsWindingUp());
                    Gunner->Tick(.7f);
                    bool RealShot=false;
                    const FVector Muzzle=Gunner->GetActorLocation()+FVector::UpVector*28;
                    const FRotator Ideal=(Player->GetActorLocation()-Muzzle).Rotation();
                    for (TActorIterator<ACityEnemyBolt> It(GetWorld()); It; ++It) if (It->GetOwner()==Gunner)
                    {
                        const FRotator Error=(It->FlightVelocity().Rotation()-Ideal).GetNormalized();
                        RealShot=FMath::Abs(Error.Yaw)<=6.01f && FMath::Abs(Error.Pitch)<=6.01f && !Error.IsNearlyZero(.001f);
                        It->Destroy();
                    }
                    Check(Id,TEXT("inside_shooter_fires_real_scattered_round"),RealShot);
                    Gunner->Destroy();
                    Advance(F.Life+.03f);
                    Check(Id,TEXT("six_second_smoke_clears"),Fields.IsEmpty() && SmokeSpread(A,-FVector::ForwardVector)==0);
                }
            }
            else Check(Id,TEXT("missing_fixture_result"),false);
        }
    }
    ClearRoomEffects(); Place(650);
    const FVector Base=Player->GetActorLocation(), Facing=Player->GetActorForwardVector().GetSafeNormal2D();
    A->SetActorLocation(Base+FVector(780,-750,0)); B->SetActorLocation(Base+FVector(780,-550,0));
    int32 FoeChecks=0, FoeFailures=0;
    auto CheckFoe=[&](const TCHAR* Assertion,bool Ok)
    {
        ++FoeChecks;
        if (!Ok) { ++FoeFailures; ++Failures; }
        UE_LOG(LogTemp,Display,TEXT("CITY_FOE_CHECK %s %s"),Assertion,Ok?TEXT("PASS"):TEXT("FAIL"));
    };
    auto Foe=[&](ECityFoe Kind,FVector At)
    {
        auto* N=GetWorld()->SpawnActor<ACityCampaignEnemy>(At,FRotator::ZeroRotator,Spawn);
        N->Configure(Kind); return N;
    };
    auto* Support=Foe(ECityFoe::Support,Base+FVector(850,200,0));
    Support->Tick(2); Support->Tick(.8f);
    bool SupportFired=false;
    for (TActorIterator<ACityEnemyBolt> It(GetWorld()); It; ++It) SupportFired |= It->GetOwner()==Support;
    CheckFoe(TEXT("support_stays_out_of_ranged_volley"),!Support->IsWindingUp() && !SupportFired);
    const float GuardedHP=A->CurrentHP;
    UGameplayStatics::ApplyDamage(A,20,Player->GetController(),Player,nullptr);
    CheckFoe(TEXT("support_reduces_melee_ally_damage"),A->CurrentHP==GuardedHP-13);
    Support->StunFor(1.f);
    UGameplayStatics::ApplyDamage(A,20,Player->GetController(),Player,nullptr);
    CheckFoe(TEXT("support_stun_breaks_ordinary_link"),A->CurrentHP==GuardedHP-33);
    Support->SetActorLocation(Base+FVector(170,0,0));
    Support->Tick(1.1f);
    CheckFoe(TEXT("support_can_warn_in_melee_range"),Support->IsWindingUp());
    const float PlayerHP=Player->GetCityHealthFraction();
    Support->Tick(.6f);
    CheckFoe(TEXT("support_melee_deals_contact_damage"),Player->GetCityHealthFraction()<PlayerHP);
    Support->Destroy();
    const FVector BlinkStart=Base+Facing*660, Behind=Base-Facing*280;
    auto* Blink=Foe(ECityFoe::Blink,BlinkStart);
    Blink->Tick(3.5f);
    CheckFoe(TEXT("blink_first_cooldown_four_seconds"),FVector::Dist2D(Blink->GetActorLocation(),BlinkStart)<5);
    auto* Occupied=Box(Behind+FVector::UpVector*25,FVector(85,85,200));
    Blink->Tick(.6f);
    CheckFoe(TEXT("blink_rejects_blocked_landing"),FVector::Dist2D(Blink->GetActorLocation(),BlinkStart)<5);
    Occupied->Destroy();
    Blink->Tick(.02f);
    CheckFoe(TEXT("blink_lands_280cm_behind_player_on_floor"),FVector::Dist2D(Blink->GetActorLocation(),Behind)<2
        && FMath::Abs(Blink->GetActorLocation().Z-Origin.Z-Blink->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())<6);
    Blink->SetActorLocation(BlinkStart);
    Blink->Tick(5.8f);
    CheckFoe(TEXT("blink_respects_six_second_repeat_cooldown"),FVector::Dist2D(Blink->GetActorLocation(),BlinkStart)<5);
    Blink->Tick(.3f);
    CheckFoe(TEXT("blink_can_relocate_again_after_cooldown"),FVector::Dist2D(Blink->GetActorLocation(),Behind)<2);
    Blink->Destroy();
    UE_LOG(LogTemp,Display,TEXT("CITY_FOE_VERIFY checks=%d failures=%d"),FoeChecks,FoeFailures);
    if (auto* Mode=GetWorld()->GetAuthGameMode<ACityCampaignMode>())
    {
        ClearRoomEffects(); UltimateCooldown=0; Energy=100; Place(650);
        const FName Saved=Progress->GetProfile().Ultimate;
        auto MenuKey=[](FKey Key){
            const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
            FSlateApplication::Get().ProcessKeyDownEvent(Event);
            FSlateApplication::Get().ProcessKeyUpEvent(Event);
        };
        MenuKey(EKeys::H);
        Check("ultimate_menu",TEXT("H_opens_paused_ten_choices"),Mode->IsUltimateMenuOpen() && GetWorld()->IsPaused() && Mode->Actions().Num()==11);
        MenuKey(EKeys::Escape);
        Check("ultimate_menu",TEXT("escape_resumes_unmodified_choice"),!Mode->IsUltimateMenuOpen() && !GetWorld()->IsPaused() && Progress->GetUltimate()==Saved);
        TryCast("well"); Advance(.58f);
        const float SharedCD=UltimateCooldown, SharedEnergy=Energy;
        const int32 LiveFields=Fields.Num();
        MenuKey(EKeys::H);
        Mode->Select(7);
        Check("ultimate_menu",TEXT("selection_cancels_cast_keeps_deployed_effect"),Phase.IsNone() && LiveFields>0 && Fields.Num()==LiveFields && !GetWorld()->IsPaused());
        Check("ultimate_menu",TEXT("hammer_session_choice_preserves_saved_loadout"),Progress->GetUltimate()=="hammer" && Progress->GetProfile().Ultimate==Saved);
        TapKey(EKeys::G);
        Check("ultimate_menu",TEXT("G_keeps_shared_cooldown_and_energy"),Phase.IsNone() && SharedCD>0 && UltimateCooldown==SharedCD && Energy==SharedEnergy);
        Energy=100; UltimateCooldown=0; TapKey(EKeys::G);
        Check("ultimate_menu",TEXT("G_fires_selected_ultimate_when_ready"),Phase=="windup" && Ultimate=="hammer");
        MenuKey(EKeys::H); MenuKey(EKeys::H);
        Check("ultimate_menu",TEXT("H_toggles_menu_without_changing_cast"),!Mode->IsUltimateMenuOpen() && Ultimate=="hammer");
        for (ECityPhase Rest:{ECityPhase::Gate,ECityPhase::Extract})
        {
            FCityAbilityPhaseFixture::Enter(*Mode,Rest);
            ClearRoomEffects(); Energy=100; UltimateCooldown=0;
            MenuKey(EKeys::H); TapKey(EKeys::G);
            const bool PausedGuard=Mode->IsUltimateMenuOpen() && Phase.IsNone() && Energy==100;
            MenuKey(EKeys::H); TapKey(EKeys::G);
            Check("ultimate_menu",Rest==ECityPhase::Gate?TEXT("gate_G_only_in_nonmodal_rest"):TEXT("extract_G_only_in_nonmodal_rest"),
                PausedGuard && Ultimate=="hammer" && Phase=="windup" && Energy==0);
        }
        FCityAbilityPhaseFixture::Enter(*Mode,ECityPhase::Combat);
    }
    UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_VERIFY %s casts=%d failures=%d"),Failures==0&&Cases==32?TEXT("PASS"):TEXT("FAIL"),Cases,Failures);
    FPlatformMisc::RequestExitWithStatus(false,Failures==0&&Cases==32?0:1);
}
#endif

