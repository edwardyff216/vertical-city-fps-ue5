#include "CityArsenalProbe.h"
#include "ShooterCharacter.h"
#include "CityLocomotion.h"
#include "S07RiflePresentation.h"
#include "ShooterWeapon.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Progression/CityProfileIO.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Engine/SkeletalMesh.h"

ACityArsenalProbe::ACityArsenalProbe() { PrimaryActorTick.bCanEverTick=true; }
#if !UE_BUILD_SHIPPING
void ACityArsenalProbe::Check(bool Valid,const FString& Name)
{
    UE_LOG(LogTemp,Display,TEXT("CITY_ARSENAL %s %s"),Valid?TEXT("PASS"):TEXT("FAIL"),*Name);
    if(!Valid) ++Fails;
}
void ACityArsenalProbe::Next(int32 S) { Stage=S;Clock=0; }
void ACityArsenalProbe::Box(FVector At,FVector Extent,FRotator Rotation)
{
    AActor* A=GetWorld()->SpawnActor<AActor>();
    auto* Solid=NewObject<UBoxComponent>(A);
    A->SetRootComponent(Solid);
    Solid->SetWorldLocationAndRotation(At,Rotation);Solid->SetBoxExtent(Extent);
    Solid->SetCollisionProfileName(TEXT("BlockAll"));
    Solid->RegisterComponent();
}
void ACityArsenalProbe::Initialize()
{
    P=CastChecked<AShooterCharacter>(GetOwner());
    P->Tags.AddUnique(TEXT("CityArsenalFixture"));
    Meta=P->GetGameInstance()->GetSubsystem<UCityProgression>();
    FCityProfile Profile;
    FRandomStream Random(823);
    Profile.Equipped[0]=FCityItemCatalog::Generate(TEXT("rifle"),Random);
    Profile.Equipped[1]=FCityItemCatalog::Generate(TEXT("sword"),Random);
    for(const auto& Kind:FCityItemCatalog::Kinds())
    {
        if(Kind.Id==TEXT("sword")) continue;
        auto Item=FCityItemCatalog::Generate(Kind.Id,Random);
        Item.Affixes.Reset();
        if (FParse::Param(FCommandLine::Get(), TEXT("CityTrailVerify")))
        {
            static const FName Effects[] = {"cinder_needle", "frost_tip", "volt_pin", "blast_cap", "void_hook", "arcane_quill"};
            Item.Rarity = 5;
            if (Items.Num() < UE_ARRAY_COUNT(Effects)) Item.Affixes.Add(Effects[Items.Num()]);
            if (Kind.Id == "pulse") Item.Affixes.Append({FName("volt_pin"), FName("arcane_quill")});
        }
        Profile.Bag.Add(Item);Items.Add(Item.Id);
    }
    FString File=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),TEXT("Arsenal-")+FGuid::NewGuid().ToString()+TEXT(".json")),Error;
    Check(CityProfileIO::Write(File,Profile,Error) && Meta->OpenProfile(File),TEXT("isolated equipment profile"));
    if(FParse::Param(FCommandLine::Get(),TEXT("CityWhiteBody")))
    {
        Check(Meta->SetCharacter(TEXT("white_01")) && GetNameSafe(P->ResolveCityBody())==TEXT("SK_White"),TEXT("white character loaded for weapon and cast cameras"));
    }
    Box(Origin-FVector(0,0,50),FVector(4000,2400,50));
    P->SetActorLocation(Origin+FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
    P->GetCharacterMovement()->StopMovementImmediately();
    Enemy=GetWorld()->SpawnActor<ACityCampaignEnemy>(Origin+FVector(670,0,96),FRotator::ZeroRotator);
    if(!Enemy) { Check(false,TEXT("spawn target"));FPlatformMisc::RequestExitWithStatus(false,8);return; }
    Enemy->Configure(ECityFoe::Melee);Enemy->SetActorTickEnabled(false);
    Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);Enemy->CurrentHP=10000;
    Next(1);
}
#endif
void ACityArsenalProbe::Tick(float Dt)
{
    Super::Tick(Dt);
#if !UE_BUILD_SHIPPING
    Clock+=Dt;Elapsed+=Dt;
    if(Elapsed>95) {Check(false,TEXT("timeout"));FPlatformMisc::RequestExitWithStatus(false,8);return;}
    if(Stage==0){if(Clock>.5f) Initialize();return;}
    if(!P || !Enemy || P->IsDead()){Check(false,TEXT("live player"));FPlatformMisc::RequestExitWithStatus(false,8);return;}
    auto* Gun=P->GetCityWeapon();
    auto* Look=P->GetFirstPersonCameraComponent();
    auto* Motion=P->FindComponentByClass<UCityLocomotion>();
    auto* Rig=P->FindComponentByClass<US07RiflePresentation>();
    if(Stage<=4) P->GetController()->SetControlRotation((Enemy->GetActorLocation()-Look->GetComponentLocation()).Rotation());
    if(Stage==1 && Clock>.45f)
    {
        if(Index==Items.Num()) { P->EquipKatana();Next(5);return; }
        const bool Swapped=Meta->Equip(Items[Index],0);
        P->EquipRifle();
        Gun=P->GetCityWeapon();
        const auto Stats=Meta->GetStats(0);
        Check(Swapped && Gun && Gun->GetCityKind()==Meta->GetWeapon(0).Kind && Gun->GetMagazineSize()==Stats.Magazine,TEXT("equipped ")+Meta->GetWeapon(0).Kind.ToString());
        if(!Gun){Next(20);return;}
        const FName Kind=Gun->GetCityKind();
        const FString Prefix=TEXT("/Game/VerticalCity/Weapons/FieldModels/SM_")+Kind.ToString();
        TInlineComponentArray<UStaticMeshComponent*> Parts(P);
        auto HasModel=[&Parts,&Prefix](FName PartName,const TCHAR* Suffix,bool bExpected)
        {
            for (auto* Part:Parts)
                if (Part->GetFName()==PartName)
                {
                    const UStaticMesh* Mesh=Part->GetStaticMesh();
                    if (!bExpected) return !Mesh && !Part->IsVisible();
                    return Part->IsVisible() && Mesh && Mesh->GetOutermost()->GetName()==Prefix+Suffix;
                }
            return false;
        };
        Check(HasModel(TEXT("S07Receiver"),TEXT(""),true)
            && HasModel(TEXT("S07Magazine"),TEXT("_Mag"),Kind!="shotgun")
            && HasModel(TEXT("S07ActionPart"),TEXT("_Action"),Kind=="shotgun" || Kind=="hand_cannon" || Kind=="sidearm"),
            TEXT("field model and moving parts ")+Kind.ToString());
        Next(101);
    }
    else if(Stage==101 && Gun && Clock>.24f)
    {
        Check(Rig && (Rig->ObservedHandActions&1),TEXT("first-person draw sampled ")+Gun->GetCityKind().ToString());
        Initial=Gun->GetBulletCount();Before=Enemy->CurrentHP;
        P->DoStartFiring();P->DoStopFiring();Next(2);
    }
    else if(Stage==2 && Clock>.7f)
    {
        const int Shots=Gun->GetCityKind()==TEXT("pulse")?3:1;
        Check(Gun->GetBulletCount()==Initial-Shots && Enemy->CurrentHP<Before,TEXT("direct damage and ammo ")+Gun->GetCityKind().ToString());
        P->DoReload();
        Check(Gun->IsReloading(),TEXT("reload began ")+Gun->GetCityKind().ToString());
        Next(3);
    }
    else if(Stage==3 && Gun->IsReloading() && Rig->GetReloadContactWeight()>.995f)
    {
        float Distance=0;
        if(Rig->SampleMagazineContact(Distance))
        {
            ++ContactFrames;
            PeakContactError=FMath::Max(PeakContactError,Distance);
        }
    }
    else if(Stage==3 && !Gun->IsReloading() && Clock>.12f)
    {
        Check(Gun->GetBulletCount()==Initial && Gun->GetCityReserve()<Meta->GetStats(0).Reserve,TEXT("reload conservation ")+Gun->GetCityKind().ToString());
        Check(Rig && (Rig->ObservedHandActions&6)==6,TEXT("first-person shot and reload sampled ")+Gun->GetCityKind().ToString());
        UE_LOG(LogTemp,Display,TEXT("CITY_GRIP kind=%s left=%.2fcm right=%.2fcm"),*Gun->GetCityKind().ToString(),Rig->GetLeftGripErrorCm(),Rig->GetRightGripErrorCm());
        if(Gun->GetCityKind()!=TEXT("shotgun"))
            Check(ContactFrames>0 && PeakContactError<4.f,FString::Printf(TEXT("left palm to moving magazine %s frames=%d max_cm=%.3f"),*Gun->GetCityKind().ToString(),ContactFrames,PeakContactError));
        ContactFrames=0; PeakContactError=0;
        ++Index;Next(1);
    }
    else if(Stage==5 && Clock>.5f)
    {
        P->SetActorLocation(Origin+FVector(0,0,96),false,nullptr,ETeleportType::TeleportPhysics);
        Enemy->SetActorLocation(Origin+FVector(260,0,96));
        P->GetController()->SetControlRotation(FRotator::ZeroRotator);P->SetActorRotation(FRotator::ZeroRotator);
        Next(6);
    }
    else if(Stage==6 && Clock>.25f)
    {
        Before=Enemy->CurrentHP;StartX=P->GetActorLocation().X;
        P->DoSpecial();ObservedDash=P->IsKatanaDashing();Next(7);
    }
    else if(Stage==7 && !P->IsKatanaDashing())
    {
        Travel=P->GetActorLocation().X-StartX;
        Check(ObservedDash && FMath::Abs(Travel-600)<5 && Enemy->CurrentHP==Before,TEXT("600cm dash through actor, no transit damage"));
        Next(8);
    }
    else if(Stage==8 && (Enemy->CurrentHP<Before || Clock>1.3f))
    {
        WaitAfterDash=Clock;
        Check(Enemy->CurrentHP<Before && FMath::Abs(Clock-1.f)<=Dt*2+.01f,FString::Printf(TEXT("delayed cut %.3fs"),Clock));
        Enemy->SetActorLocation(Origin+FVector(0,1700,100));
        P->EquipRifle();P->DoMove(0,1);Next(9);
    }
    else if(Stage==9)
    {
        P->DoMove(0,1);
        if(Clock>.65f){Check(FMath::Abs(P->GetVelocity().Size2D()-700)<15,TEXT("walk speed 700"));Motion->SprintPressed();Next(10);}
    }
    else if(Stage==10)
    {
        P->DoMove(0,1);
        if(Clock>.65f){Check(FMath::Abs(P->GetVelocity().Size2D()-1050)<20,TEXT("sprint speed 1050"));Motion->StartSlide();Check(P->IsCitySliding(),TEXT("slide accepted"));Next(11);}
    }
    else if(Stage==11 && Clock>.15f)
    {
        Check(P->bIsCrouched && P->GetVelocity().Size2D()>900,TEXT("moving low slide"));
        Motion->SprintReleased();Next(12);
    }
    else if(Stage==12 && Clock>1.f)
    {
        Check(!P->bIsCrouched && !P->IsCitySliding(),TEXT("slide restores standing capsule"));
        auto* Weapon=P->GetCityWeapon();
        Weapon->StartFiring();P->CancelCityInput();
        Initial=Weapon->GetBulletCount();Next(13);
    }
    else if(Stage==13 && Clock>.8f)
    {
        Check(Gun->GetBulletCount()==Initial,TEXT("modal cancel has no deferred shot"));
        Box(Origin+FVector(1350,750,200),FVector(1050,130,60));
        P->SetActorLocation(Origin+FVector(-80,750,98),false,nullptr,ETeleportType::TeleportPhysics);
        P->GetCharacterMovement()->StopMovementImmediately();
        Next(14);
    }
    else if(Stage==14)
    {
        P->DoMove(0,1);
        if(Clock>.38f){ Motion->StartSlide(); Next(15); }
    }
    else if(Stage==15 && Clock>.81f)
    {
        Check(P->bIsCrouched && P->GetActorLocation().X>Origin.X+350,TEXT("low ceiling prevents standing after slide"));
        Next(16);
    }
    else if(Stage==16)
    {
        P->DoMove(1,0);
        if(!P->bIsCrouched || Clock>1.5f)
        {
            Check(!P->bIsCrouched && P->GetActorLocation().Y>Origin.Y+865,TEXT("clearance restores capsule outside overhang"));
            Box(Origin+FVector(850,-800,91),FVector(415,190,18),FRotator(15,0,0));
            P->SetActorLocation(Origin+FVector(230,-800,98),false,nullptr,ETeleportType::TeleportPhysics);
            P->GetCharacterMovement()->StopMovementImmediately();Next(17);
        }
    }
    else if(Stage==17)
    {
        P->DoMove(0,1);
        if(P->GetActorLocation().X>Origin.X+1000 || Clock>2.5f)
        {
            Check(P->GetActorLocation().Z>Origin.Z+200 && P->GetCharacterMovement()->IsMovingOnGround(),TEXT("ascend ramp under normal movement"));
            P->EquipKatana();P->SetActorLocation(Origin+FVector(0,-1450,96),false,nullptr,ETeleportType::TeleportPhysics);
            P->GetCharacterMovement()->StopMovementImmediately();
            Box(Origin+FVector(260,-1450,180),FVector(20,220,180));
            Enemy->SetActorLocation(Origin+FVector(390,-1450,96));
            Before=Enemy->CurrentHP;Next(18);
        }
    }
    else if(Stage==18 && Clock>.4f)
    {
        StartX=P->GetActorLocation().X;P->DoSpecial();Next(19);
    }
    else if(Stage==19 && Clock>1.5f)
    {
        float Blocked=P->GetActorLocation().X-StartX;
        Check(Blocked>100 && Blocked<245 && Enemy->CurrentHP==Before,TEXT("dash sweep stops at wall, no damage behind it"));
        Next(21);
    }
    else if(Stage==21 && Clock>1.6f) { P->DoStartFiring(); Next(22); }
    else if(Stage==22 && Clock>.405f) {
        StartX=P->GetActorLocation().X;
        P->DoSpecial();P->EquipRifle();P->EquipKatana(); Next(23);
    }
    else if(Stage==23 && Clock>.5f) {
        Check(FMath::Abs(P->GetActorLocation().X-StartX)<2 && !P->IsKatanaDashing(),TEXT("switch cancels queued dash"));
        P->DoStartFiring(); Next(24);
    }
    else if(Stage==24 && Clock>.405f) { P->DoSpecial();P->CancelCityInput();Next(25); }
    else if(Stage==25 && Clock>.5f) {
        Check(FMath::Abs(P->GetActorLocation().X-StartX)<2 && !P->IsKatanaDashing(),TEXT("modal cancels queued dash"));
        const bool Granted=Meta->BeginRun() && Meta->ChooseAbility(TEXT("test-slide"),TEXT("slide_refund")) && Meta->ChooseAbility(TEXT("test-swap"),TEXT("swap_haste"));
        Check(Granted,TEXT("run passives granted"));
        P->EquipRifle();
        Check(FMath::IsNearlyEqual(P->SwapSpeedScale(),1.f+FMath::Loge(1.12f)),TEXT("switch applies haste"));
        P->SetActorLocation(Origin+FVector(0,0,96),false,nullptr,ETeleportType::TeleportPhysics);
        Enemy->SetActorLocation(Origin+FVector(460,0,96));
        P->GetController()->SetControlRotation((Enemy->GetActorLocation()-Look->GetComponentLocation()).Rotation());
        Next(26);
    }
    else if(Stage==26 && Clock>2.2f)
    {
        Check(FMath::IsNearlyEqual(P->SwapSpeedScale(),1.f),TEXT("switch haste expires"));
        Check(FMath::IsNearlyEqual(P->GetCharacterMovement()->GetGravityZ(),-2200.f,1.f),TEXT("Godot-equivalent 22 m/s2 gravity"));
        // Use the same rifle as a fresh campaign. Keep reserve constant throughout refund checks.
        P->TriggerCityAction(TEXT("slide"));
        Initial=Gun->GetBulletCount(); Before=Enemy->CurrentHP; ContactFrames=Gun->GetCityReserve();
        P->GetController()->SetControlRotation((Enemy->GetActorLocation()-Look->GetComponentLocation()).Rotation());
        P->DoStartFiring();P->DoStopFiring(); Next(27);
    }
    else if(Stage==27 && Clock>.6f)
    {
        Check(Enemy->CurrentHP<Before && Gun->GetBulletCount()==Initial && Gun->GetCityReserve()==ContactFrames,TEXT("slide first hit refunds without consuming reserve"));
        P->TriggerCityAction(TEXT("slide")); P->GetController()->SetControlRotation(FRotator(60,90,0));Next(28);
    }
    else if(Stage==28 && Clock>.15f)
    {
        P->bRiposte=true;
        Initial=Gun->GetBulletCount(); P->DoStartFiring();P->DoStopFiring(); Next(29);
    }
    else if(Stage==29 && Clock>.6f)
    {
        Check(Gun->GetBulletCount()==Initial-1 && P->ConsumeSlideRefund(true)==0,TEXT("slide miss consumes refund opportunity"));
        Check(!P->bRiposte,TEXT("next attack spends riposte even on a miss"));
        P->SetActorLocation(Origin+FVector(0,-1450,230),false,nullptr,ETeleportType::TeleportPhysics);
        P->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        P->GetCharacterMovement()->StopMovementImmediately();
        P->DoJumpStart(); P->DoJumpEnd();
        Check(!Motion->bUsedWallJump,TEXT("open air preserves wall jump charge"));
        Next(30);
    }
    else if(Stage==30)
    {
        P->SetActorLocation(Origin+FVector(200,-1450,230),false,nullptr,ETeleportType::TeleportPhysics);
        P->DoJumpStart(); P->DoJumpEnd(); Next(31);
    }
    else if(Stage==31 && Clock>.09f)
    {
        Check(Motion->bUsedWallJump && P->GetVelocity().X < -400 && P->GetVelocity().Z > 0 &&
            P->GetActorLocation().Z > Origin.Z+250,TEXT("jump input kicks away from vertical wall and gains height"));
        P->SetActorLocation(Origin+FVector(200,-1450,230),false,nullptr,ETeleportType::TeleportPhysics);
        Check(!Motion->TryWallJump(),TEXT("one wall jump per airtime"));
        Next(32);
    }
    else if(Stage==32 && (P->GetCharacterMovement()->IsMovingOnGround() || Clock>2.f))
    {
        Check(P->GetCharacterMovement()->IsMovingOnGround() && !Motion->bUsedWallJump,TEXT("landing restores wall jump"));
        Next(20);
    }
    else if(Stage==20)
    {
        UE_LOG(LogTemp,Display,TEXT("CITY_ARSENAL_VERIFY %s guns=%d fails=%d dash_cm=%.1f delay=%.3f"),Fails?TEXT("FAIL"):TEXT("PASS"),Items.Num(),Fails,Travel,WaitAfterDash);
        SetActorTickEnabled(false);FPlatformMisc::RequestExitWithStatus(false,Fails?8:0);
    }
#endif
}