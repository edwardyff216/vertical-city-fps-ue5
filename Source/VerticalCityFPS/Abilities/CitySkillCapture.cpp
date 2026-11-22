#include "Abilities/CityAbilities.h"

#if !UE_BUILD_SHIPPING
#include "Campaign/CityCampaignMode.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Campaign/CityEnemyBolt.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Progression/CityProgression.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Scalability.h"
#include "UnrealClient.h"

void UCityAbilities::TickSkillCapture()
{
    auto* Campaign=GetWorld()->GetAuthGameMode<ACityCampaignMode>();
    if (!Player->IsLocallyControlled() || !Progress || !Campaign) return;
    const bool Barrage=CaptureKind=="barrage";
    const bool Frost=CaptureKind=="frost-arc" || CaptureKind=="frost_arc";
    const bool Sink=CaptureKind=="sink";
    const bool Plate=CaptureKind=="plate-wall";
    const bool Trails = CaptureKind == "element-trails";
    const bool Gun=CaptureKind=="gun-affix";
    const bool WeaponModel=CaptureKind.StartsWith(TEXT("weapon-"));
    const bool Hands=CaptureKind=="hands";
    const bool Dash=CaptureKind=="katana";
    if (!bCapturePrepared)
    {
        if (!Progress->IsReady()) return;
        if (Campaign->GetPhase()!=ECityPhase::Preparation && Campaign->GetPhase()!=ECityPhase::Combat) return;
        // Only use an explicit test profile. Captures must not change a live player's equipment.
        FString PrivateProfile;
        if (!FParse::Value(FCommandLine::Get(),TEXT("CityProfile="),PrivateProfile) || PrivateProfile.IsEmpty())
        { UE_LOG(LogTemp,Error,TEXT("CITY_SKILL_CAPTURE needs explicit test profile")); FPlatformMisc::RequestExitWithStatus(false,2); return; }
        if (Campaign->GetPhase()==ECityPhase::Preparation)
        {
            Progress->ClaimBasicWeapons();
            Progress->SetActiveSkill(0,Sink?FName("bullet_sink"):Plate?FName("deflect_plate"):Gun?FName("finisher_lens"):FName("frost_floor"));
            Progress->SetActiveSkill(1,Plate?FName("barrier_wall"):FName("arc_lance"));
            Progress->SetUltimate(TEXT("barrage"));
            Progress->Depart();
        }
        if (Campaign->GetPhase()!=ECityPhase::Combat) return;
        IFileManager::Get().MakeDirectory(*CaptureDir,true);
        if (FParse::Param(FCommandLine::Get(),TEXT("CitySkillLow")))
        {
            Scalability::FQualityLevels Levels=Scalability::GetQualityLevels();
            Levels.SetFromSingleQualityLevel(0);
            Scalability::SetQualityLevels(Levels);
        }
        bCapturePrepared=true;
        if (Gun || Dash || Hands || WeaponModel || Trails)
        {
            FCityProfile Fixture=Progress->GetProfile();
            Fixture.Equipped[0].Id=TEXT("capture-rifle"); Fixture.Equipped[0].Kind="rifle";
            Fixture.Equipped[0].Affixes={FName("cinder_needle")};
            Fixture.Equipped[1].Id=TEXT("capture-sword"); Fixture.Equipped[1].Kind="sword";
            if (Hands)
            {
                Fixture.Equipped[1].Id=TEXT("capture-sidearm");
                Fixture.Equipped[1].Kind="sidearm";
                Fixture.Equipped[0].Affixes.Reset();
            }
            if (WeaponModel)
            {
                Fixture.Equipped[0].Kind=FName(*CaptureKind.RightChop(7));
                Fixture.Equipped[0].Affixes.Reset();
            }
            Fixture.Equipped[1].Affixes.Reset();
            Progress->UseLabProfile(Fixture);
        }
        if (Dash) Player->EquipKatana(); else Player->EquipRifle();
        UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_CAPTURE_START %s quality=%d"),*CaptureKind,Scalability::GetQualityLevels().ShadowQuality);
    }
    if (Campaign->GetPhase()!=ECityPhase::Combat)
    { UE_LOG(LogTemp,Error,TEXT("CITY_SKILL_CAPTURE interrupted phase")); FPlatformMisc::RequestExitWithStatus(false,3); return; }
    const int32 LastFrame=Trails?144:WeaponModel?96:Hands?223:Sink?181:Plate?150:111;
    const int32 Frame=CaptureFrame++;
    if (Frame==-44)
    {
        // Start the pursuer across the room so the first-person camera can see the full field before it closes.
        for (TActorIterator<ACityCampaignEnemy> It(GetWorld());It;++It)
            if (It->GetKind()==ECityFoe::Melee || It->GetKind()==ECityFoe::Blink)
                It->SetActorLocation(Player->GetActorLocation()+FVector(1980,-580,0));
    }
    const auto Targets=Near(Eye(),4800);
    // Reproducible positions for the two short-range interactions; the other room enemies remain active.
    if (Frame==-12)
    {
        FActorSpawnParameters Args; Args.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        FVector At=Player->GetActorLocation()+FVector::ForwardVector*(Dash?330:Gun?730:1080);
        if (auto* N=GetWorld()->SpawnActor<ACityCampaignEnemy>(At,Player->GetActorRotation(),Args))
        {
            N->Configure(ECityFoe::Shooter);
            CaptureTarget=N;
            if (Dash) N->StunFor(1.65f);
        }
    }
    AShooterNPC* N=CaptureTarget.IsValid() && CaptureTarget->CurrentHP>0 ? CaptureTarget.Get() : (Targets.IsEmpty()?nullptr:Targets[0]);
    if (N)
    {
        FVector Focus=N->GetActorLocation()+FVector::UpVector*16;
        const bool AimFloor=(Frost || Sink || (Plate && Frame>65)) && Frame>=14 && Frame<=(Plate?91:28);
        if (AimFloor) Focus.Z-=105;
        if (Frame<15 && !Dash && !Gun) Focus+=Right()*85;
        if (Sink && Frame>=52 && Frame<147)
            for (const auto& F : Fields) if (F.Id=="bullet_sink") Focus=F.Point;
        const FRotator Wanted=(Focus-Eye()).Rotation();
        const FRotator Current=Player->GetControlRotation();
        Player->GetController()->SetControlRotation(FMath::RInterpTo(Current,Wanted,1.f/30,12.f));
    }
    if (WeaponModel)
    {
        if (Frame==9) Player->DoSpecial();
        if (Frame==23) Player->DoStartFiring();
        if (Frame==26) Player->DoStopFiring();
        if (Frame==32) Player->EndCityAim();
        if (Frame==40) Player->DoReload();
    }
    if (Trails && Frame >= 0 && Frame < 144)
    {
        static const FName Affixes[] = {"cinder_needle", "frost_tip", "volt_pin", "blast_cap", "void_hook", "arcane_quill"};
        const int32 Beat = Frame % 24;
        if (Beat == 0)
        {
            FCityProfile Fixture = Progress->GetProfile();
            Fixture.Equipped[0].Rarity = 2;
            Fixture.Equipped[0].Affixes = {Affixes[Frame / 24]};
            Progress->UseLabProfile(Fixture);
            Player->RefreshCityLoadout();
        }
        if (Beat == 4 || Beat == 13) Player->DoStartFiring();
        if (Beat == 6 || Beat == 15) Player->DoStopFiring();
    }
    if (Hands)
    {
        if (Frame==0 || Frame==68 || Frame==85 || Frame==155) Player->DoStartFiring();
        if (Frame==4 || Frame==70 || Frame==88 || Frame==157) Player->DoStopFiring();
        if (Frame==12 || Frame==97) Player->DoReload();
        if (Frame==73) Player->EquipKatana(); // Selects the second fixture slot, holding the pistol.
        if (Frame==162) Player->EquipRifle();
        if (Frame==175 && !TryCast(TEXT("barrage")))
        { FPlatformMisc::RequestExitWithStatus(false,4);return; }
    }
    if (!Dash && ((Frame>=0 && Frame<16) || (Frame>=68 && Frame<86)))
        Player->AddMovementInput(Player->GetActorRightVector(),Frame<16?.62f:-.68f);
    if (!Trails && !WeaponModel && !Dash && !Hands && (Frame==4 || Frame==87 || (Gun && Frame==42))) Player->DoStartFiring();
    if (!Trails && !WeaponModel && !Dash && !Hands && (Frame==9 || Frame==94 || (Gun && Frame==60))) Player->DoStopFiring();
    if (Gun && Frame==32) Player->DoSpecial();
    if (Gun && Frame==70) Player->EndCityAim();
    if (Dash && Frame==17) Player->DoSpecial();
    if (Dash && Frame==87) Player->DoStartFiring();
    if (Dash && Frame==91) Player->DoStopFiring();
    if (!Trails && !WeaponModel && !Dash && !Hands && Frame==24)
    {
        const bool Cast=TryCast(Barrage?FName("barrage"):Progress->GetActiveSkill(0));
        UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_CAPTURE_CAST %s success=%d"),*CaptureKind,Cast);
        if (!Cast) { FPlatformMisc::RequestExitWithStatus(false,4); return; }
    }
    if ((Frost && Frame==62) || (Plate && Frame==86))
    {
        const bool Cast=TryCast(Progress->GetActiveSkill(1));
        UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_CAPTURE_CAST %s success=%d"),*Progress->GetActiveSkill(1).ToString(),Cast);
        if (!Cast) { FPlatformMisc::RequestExitWithStatus(false,4); return; }
    }
    // Fire the same hostile projectile actor the sentries use, on controlled approach angles.
    if (N && ((Sink && Frame>=49 && Frame<=94 && Frame%7==0) || (Plate && (Frame==35 || Frame==47 || Frame==101))))
    {
        FVector Origin=Eye()+Aim()*460, Destination=Eye();
        if (Sink)
        {
            for (const auto& F:Fields) if (F.Id=="bullet_sink")
            {
                const float Side=(Frame%2==0)?1.f:-1.f;
                Origin=F.Point+Right()*235*Side+FVector::UpVector*55-Aim()*85;
                Destination=F.Point+Right()*18*Side+FVector::UpVector*23;
            }
        }
        FActorSpawnParameters Args; Args.Owner=N; Args.Instigator=N;
        if (auto* Bolt=GetWorld()->SpawnActor<ACityEnemyBolt>(Origin,FRotator::ZeroRotator,Args))
            Bolt->Launch((Destination-Origin).GetSafeNormal(),Sink?320.f:600.f,5.f);
    }
    if (Frame>=0 && Frame%10==0)
    {
        int32 Stored=0;
        for (const auto& F:Fields) if (F.Id=="bullet_sink") Stored=F.Stored;
        UE_LOG(LogTemp,Display,TEXT("CITY_VFX_FRAME n=%d pos=%s yaw=%.1f hp=%.1f dash=%d stored=%d blades=%d"),Frame,
            *Player->GetActorLocation().ToCompactString(),Player->GetControlRotation().Yaw,N?N->CurrentHP:-1,Player->IsKatanaDashing(),Stored,BladesFired);
    }
    if (Frame<0) return;
    if (Frame>=LastFrame)
    {
        UE_LOG(LogTemp,Display,TEXT("CITY_SKILL_CAPTURE_DONE %s frames=%d"),*CaptureKind,LastFrame);
        CaptureKind.Reset();
        FPlatformMisc::RequestExitWithStatus(false,0);
        return;
    }
    // HUD and viewport are captured after gameplay and camera updates for this simulation tick.
    const FString Path=CaptureDir/FString::Printf(TEXT("frame%04d.png"),Frame);
    FScreenshotRequest::RequestScreenshot(Path,true,false,false,FIntRect(),true);
}
#endif

