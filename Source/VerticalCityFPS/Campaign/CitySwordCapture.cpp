#include "Campaign/CityCampaignMode.h"

#if !UE_BUILD_SHIPPING
#include "ShooterCharacter.h"
#include "CityKatana.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "AudioMixerBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Capture input, the offline audio mixer and images at the same fixed 30 Hz.
bool ACityCampaignMode::CaptureSword()
{
    if (FParse::Param(FCommandLine::Get(), TEXT("CityTripleCutTest"))) return CaptureTripleCut();
    auto* P = Player();
    if (!P || SwordCaptureTick > 167) return false;
    const int32 Frame = SwordCaptureTick++;
    if (Frame == 0)
    {
        for (ACityCampaignEnemy* Enemy : Enemies)
            if (IsValid(Enemy) && Enemy->GetKind() == ECityFoe::Melee) { SwordCaptureTarget=Enemy; break; }
        if (!SwordCaptureTarget.IsValid()) { EndBot(false,TEXT("no sword capture opponent")); return false; }
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this,6.f);
        P->EquipKatana();
        if (!Cast<ACityKatana>(P->GetCityWeapon())) { EndBot(false,TEXT("Sword capture requires a katana in slot two")); return false; }
        P->SetActorLocation(SwordCaptureTarget->GetActorLocation()-FVector(210,0,0),false,nullptr,ETeleportType::TeleportPhysics);
        P->SetActorRotation(FRotator::ZeroRotator);
        PC()->SetControlRotation(FRotator::ZeroRotator);
    }
    if (Frame == 11)
    {
        ComboTargetHP=SwordCaptureTarget->CurrentHP;
        P->DoStartFiring();
        P->DoStopFiring();
    }
    if (Frame == 30)
    {
        if (SwordCaptureTarget->CurrentHP>=ComboTargetHP) { EndBot(false,TEXT("normal slash did not hit")); return false; }
    }
    if (Frame == 34 || Frame == 116)
    {
        P->DoSpecial();
        if (!P->IsKatanaDashing()) { EndBot(false,TEXT("capture dash rejected")); return false; }
    }
    if (Frame >= 41 && Frame < 66) P->DoMove(0,1);
    if (Frame == 167)
    {
        UAudioMixerBlueprintLibrary::StopRecordingOutputToAnyPath(this,EAudioRecordingExportType::WavFile,TEXT("katana-mix"),SwordCaptureFolder);
        P->EquipRifle();
        return false;
    }
    const FString Name=FString::Printf(TEXT("frame-%03d.png"),Frame);
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(SwordCaptureFolder,Name),true,false);
    return true;
}
// Exercise the campaign input path with one stationary target so each HP change can be timed.
bool ACityCampaignMode::CaptureTripleCut()
{
    auto* P = Player();
    const int32 Frame = SwordCaptureTick++;
    if (Frame == 0)
    {
        for (ACityCampaignEnemy* E : Enemies)
        {
            if (IsValid(E) && E->GetKind()==ECityFoe::Melee) { SwordCaptureTarget=E; break; }
        }
        if (!SwordCaptureTarget.IsValid()) { EndBot(false,TEXT("triple cut target missing")); return true; }
        auto* E = SwordCaptureTarget.Get();
        P->EquipKatana();
        P->SetActorLocation(E->GetActorLocation()-FVector(155,0,0),false,nullptr,ETeleportType::TeleportPhysics);
        P->SetActorRotation(FRotator::ZeroRotator);
        PC()->SetControlRotation(FRotator::ZeroRotator);
        E->StunFor(10.f);
        ComboTargetHP = E->CurrentHP = 2000.f;
    }
    auto* Blade=Cast<ACityKatana>(P->GetCityWeapon());
    auto* E= SwordCaptureTarget.Get();
    if (!E) { EndBot(false,TEXT("lost triple cut target")); return true; }
    // Hold for two complete cycles. Release during the last strike, then tap once after a pause.
    if (Frame==18 || Frame==128 || Frame==161 || Frame==193 || Frame==230 || Frame==243 || Frame==280) P->DoStartFiring();
    if (Frame==101 || Frame==129 || Frame==231 || Frame==244 || Frame==281) P->DoStopFiring();
    if (Frame==214) P->DoReload();
    if (Frame==293) P->DoSpecial();
    if (Frame==162) P->EquipRifle();
    if (Frame==178) P->EquipKatana();
    if (Frame==194) { SetModal(true); SetModal(false); }
    if (E->CurrentHP < ComboTargetHP)
    {
        const int32 Expected[] = {29,41,56,77,89,104,139,240,253,290};
        if (ComboHitsObserved >= UE_ARRAY_COUNT(Expected) ||
            FMath::Abs(Frame-Expected[ComboHitsObserved])>3)
        { EndBot(false,TEXT("extra or mistimed primary cut")); return true; }
        ++ComboHitsObserved;
        ComboTargetHP = E->CurrentHP;
        UE_LOG(LogTemp,Display,TEXT("CITY_FP_CUT hit=%d frame=%d hp=%.1f"),ComboHitsObserved,Frame,ComboTargetHP);
    }
    if (Blade && Blade->IsPrimaryCutActive() &&
        FVector::Dist(PC()->PlayerCameraManager->GetCameraLocation(),P->GetFirstPersonCameraComponent()->GetComponentLocation())>24.f)
    { EndBot(false,TEXT("primary cut left first person")); return true; }
    if (Frame==299)
    {
        EndBot(ComboHitsObserved==10 && Blade && !Blade->IsPrimaryCutActive() && Blade->IsDashing(),
               TEXT("first-person hold twice, release, tap, swap, menu, buffered tap and buffered dash"));
        return true;
    }
    if (Frame<=150)
    {
        const FString Name=FString::Printf(TEXT("combo-%03d.png"),Frame);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(SwordCaptureFolder,Name),true,false);
    }
    return true;
}

// Capture a real campaign encounter using the same movement, attack and dash inputs as the player.
void ACityCampaignMode::CaptureMovingSword()
{
    auto* P = Player();
    if (!P || P->IsDead()) { EndBot(false,TEXT("moving sword capture lost player")); return; }
    const int32 Frame = SwordCaptureTick++;
    if (Frame==0)
    {
        P->EquipKatana();
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this,10.f);
    }
    auto* Blade=Cast<ACityKatana>(P->GetCityWeapon());
    if (!Blade) { EndBot(false,TEXT("moving sword needs an equipped katana")); return; }
    const FVector At=P->GetActorLocation();
    const FVector Eye=P->GetFirstPersonCameraComponent()->GetComponentLocation();
    auto* Target=SwordCaptureTarget.Get();
    if (!IsValid(Target) || Target->CurrentHP<=0)
    {
        Target=nullptr;
        for (ACityCampaignEnemy* E : Enemies)
        {
            if (!IsValid(E) || E->CurrentHP<=0) continue;
            if (!Target || FVector::DistSquared2D(At,E->GetActorLocation()) < FVector::DistSquared2D(At,Target->GetActorLocation())) Target=E;
        }
        SwordCaptureTarget=Target;
    }
    const float Range=Target ? FVector::Dist2D(At,Target->GetActorLocation()) : 0.f;
    if (MovingCutStart==INDEX_NONE && Frame>=10 && Target && Range<245.f)
    {
        MovingCutStart=Frame;
        P->DoStartFiring();
        if (!Blade->IsPrimaryCutActive()) { EndBot(false,TEXT("primary input rejected")); return; }
    }
    const int32 Age=MovingCutStart==INDEX_NONE ? -1 : Frame-MovingCutStart;
    // Maintain the approach direction across the dash camera hand-off.
    if (Target && Age<96)
    {
        FRotator Desired=(Target->GetActorLocation()+FVector(0,0,40)-Eye).Rotation();
        Desired.Pitch=FMath::ClampAngle(Desired.Pitch,-12,12);
        PC()->SetControlRotation(FMath::RInterpConstantTo(PC()->GetControlRotation(),Desired,GetWorld()->GetDeltaSeconds(),100.f));
        // Kite the pack during cycle two, keeping a pursuer in dash range.
        const float WantedRange=Age>=42 ? 410.f : 160.f;
        const float Forward=Age<0 ? 1.f : FMath::Clamp((Range-WantedRange)/115.f,-1.f,.85f);
        const float Side=Age<0 ? .12f : .48f*FMath::Cos(Age*.054f);
        P->DoMove(Side,Forward);
    }
    else if (Age>=0 && Age<96) P->DoMove(.25f,.45f);
    // Optional capture input: turn back to see the two moving enemies after passing them.
    if (FParse::Param(FCommandLine::Get(),TEXT("CityLookBackAfterDash")) && bMovingDashSeen && !Blade->IsDashing())
    {
        FVector Focus=FVector::ZeroVector;
        int32 Count=0;
        for (ACityCampaignEnemy* Enemy : Enemies)
        {
            if (!IsValid(Enemy) || Enemy->CurrentHP<=0.f) continue;
            Focus+=Enemy->GetActorLocation()+FVector::UpVector*23.f;
            ++Count;
        }
        // Keep the reticle between the targets through the burst, even if one dies.
        if (Age>=115 && Age<125) P->DoMove(.72f,0.f);
        if (Count>=2)
        {
            const FRotator Want=(Focus/Count-Eye).Rotation();
            PC()->SetControlRotation(FMath::RInterpConstantTo(PC()->GetControlRotation(),Want,GetWorld()->GetDeltaSeconds(),360.f));
        }
    }
    if (Age==91) P->DoStopFiring();
    if (Age==92) P->DoSpecial();
    bMovingDashSeen |= Blade->IsDashing();
    const FVector Cam=PC()->PlayerCameraManager->GetCameraLocation();
    const FRotator Look=PC()->PlayerCameraManager->GetCameraRotation();
    const FVector Foe=Target ? Target->GetActorLocation() : FVector::ZeroVector;
    UE_LOG(LogTemp,Display,TEXT("MOVING_KATANA frame=%d world=%.4f age=%d player=%s eye=%s cam=%s yaw=%.2f pitch=%.2f dash=%d foe=%s pos=%s hp=%.1f"),
        Frame,GetWorld()->GetTimeSeconds(),Age,*At.ToCompactString(),*Eye.ToCompactString(),*Cam.ToCompactString(),
        Look.Yaw,Look.Pitch,Blade->IsDashing(),*GetNameSafe(Target),*Foe.ToCompactString(),Target?Target->CurrentHP:0.f);
    if (auto* Grip=Cast<USceneComponent>(Blade->GetDefaultSubobjectByName(TEXT("KatanaView"))))
        UE_LOG(LogTemp,Verbose,TEXT("KATANA_VIEW frame=%d grip=%s fov=%.2f"),Frame,*Grip->GetRelativeLocation().ToCompactString(),PC()->PlayerCameraManager->GetFOVAngle());
    if (Age==164)
        UAudioMixerBlueprintLibrary::StopRecordingOutputToAnyPath(this,EAudioRecordingExportType::WavFile,TEXT("game"),SwordCaptureFolder);
    if (Age>=165)
    {
        if (Age<230 && IFileManager::Get().FileSize(*(SwordCaptureFolder/TEXT("game.wav")))<=44) return;
        EndBot(bMovingDashSeen && FVector::Dist(Cam,Eye)<24.f,TEXT("moving sword take finished"));
        return;
    }
    if (Frame>360) { EndBot(false,TEXT("no melee engagement")); return; }
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(SwordCaptureFolder,FString::Printf(TEXT("live-%04d.png"),Frame)),true,false);
}

#endif
