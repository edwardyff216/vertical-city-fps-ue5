#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Progression/CityAffixLabMode.h"
#include "Campaign/CityCampaignMode.h"
#include "Campaign/CityCampaignEnemy.h"
#include "ShooterCharacter.h"
#include "Abilities/CityAbilities.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Layout/Children.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
class FWaitCityLab final : public IAutomationLatentCommand
{
public:
    explicit FWaitCityLab(TFunction<bool()> InStep):Step(MoveTemp(InStep)) {}
    virtual bool Update() override {return Step();}
private:
    TFunction<bool()> Step;
};
UWorld* RangeWorld()
{
    for(const auto& C:GEngine->GetWorldContexts())
        if(C.World() && C.World()->IsGameWorld() && C.World()->GetMapName().Contains(TEXT("L_Campaign"))) return C.World();
    return nullptr;
}
TSharedPtr<SButton> FindRangeButton(const TSharedRef<SWidget>& W,const FString& Caption)
{
    if(W->GetType()==TEXT("SButton") && W->IsEnabled())
    {
        TArray<TSharedRef<SWidget>> Pending{W};
        while(!Pending.IsEmpty())
        {
            auto Child=Pending.Pop();
            if(Child->GetType()==TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Child)->GetText().ToString()==Caption)
                return StaticCastSharedRef<SButton>(W);
            for(int I=0;I<Child->GetChildren()->Num();++I) Pending.Add(Child->GetChildren()->GetChildAt(I));
        }
    }
    for(int I=0;I<W->GetChildren()->Num();++I)
        if(auto Found=FindRangeButton(W->GetChildren()->GetChildAt(I),Caption)) return Found;
    return {};
}
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCityLabWorkflow,"VerticalCity.Progression.AffixLab",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
void FCityLabWorkflow::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    if(FParse::Param(FCommandLine::Get(),TEXT("CityLabVerify"))) {Names.Add(TEXT("CampaignRoundTrip")); Commands.Add(TEXT("CampaignRoundTrip"));}
}
bool FCityLabWorkflow::RunTest(const FString&)
{
    FString File;
    if(!TestTrue(TEXT("Explicit isolated test profile"),FParse::Value(FCommandLine::Get(),TEXT("CityProfile="),File))) return false;
    UWorld* W=RangeWorld();
    if(!TestTrue(TEXT("Normal campaign entry"),W && W->GetAuthGameMode<ACityCampaignMode>()!=nullptr)) return false;
    auto* P=W->GetGameInstance()->GetSubsystem<UCityProgression>();
    if(!TestTrue(TEXT("Preparation panel owns the profile"),P->Panel.IsValid() && P->IsReady())) return false;
    TArray<uint8> Before;
    if(!TestTrue(TEXT("Snapshot persistent save bytes"),FFileHelper::LoadFileToArray(Before,*File))) return false;
    auto Button=FindRangeButton(P->Panel.ToSharedRef(),TEXT("词条测试房"));
    if(!TestTrue(TEXT("Accessible preparation button"),Button.IsValid())) return false;
    Button->SimulateClick();
    const double Deadline=FPlatformTime::Seconds()+90;
    FString CaptureDir;
    FParse::Value(FCommandLine::Get(),TEXT("CityLabEvidence="),CaptureDir);
    auto ReadyForCapture=[this,Deadline,CaptureDir](FString Name)
    {
        if(CaptureDir.IsEmpty()) return;
        const FString Path=CaptureDir/(Name+TEXT(".png"));
        auto StableSince=MakeShared<double>(0.0);
        auto Requested=MakeShared<bool>(false);
        ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this,Deadline,Path,StableSince,Requested](){
            UWorld* World=RangeWorld();
            auto* Lab=World?World->GetAuthGameMode<ACityAffixLabMode>():nullptr;
            if(!Lab || !Lab->bInitialized)
            {
                if(FPlatformTime::Seconds()<Deadline) return false;
                AddError(TEXT("Cannot capture lab viewport")); return true;
            }
            if(!*Requested)
            {
                if(*StableSince==0.0) *StableSince=FPlatformTime::Seconds();
                if(FPlatformTime::Seconds()<*StableSince+1.25) return false;
                FScreenshotRequest::RequestScreenshot(Path,true,false);
                *Requested=true; return false;
            }
            if(FPaths::FileExists(Path)) return true;
            if(FPlatformTime::Seconds()<Deadline) return false;
            AddError(TEXT("Missing requested capture ")+Path);return true;
        }));
    };
    ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this,Deadline](){
        UWorld* World=RangeWorld();
        auto* M=World?World->GetAuthGameMode<ACityAffixLabMode>():nullptr;
        if(!M || !M->bInitialized || !M->Panel)
        {
            if(FPlatformTime::Seconds()<Deadline) return false;
            AddError(TEXT("Lab travel timed out"));return true;
        }
        TSet<FName> Union;
        for(const auto& K:FCityItemCatalog::Kinds())
        {
            M->SetWeapon(K.Id); M->RefreshChoices();
            auto Pool=FCityItemCatalog::Pool(K.Id,5);
            AddInfo(FString::Printf(TEXT("CITY_LAB_POOL kind=%s rows=%d"),*K.Id.ToString(),M->Choices->NumSlots()));
            TestEqual(K.Label+TEXT(" compatible rows"),M->Choices->NumSlots(),Pool.Num());
            for(FName Id:Pool) Union.Add(Id);
        }
        TestEqual(TEXT("All authored affixes available"),Union.Num(),110);
        M->SetWeapon(TEXT("rifle")); M->School=TEXT("余烬"); M->Search=TEXT("冷却"); M->RefreshChoices();
        TestTrue(TEXT("Family and description filters combine"),M->Choices->NumSlots()>1 && M->Choices->NumSlots()<=10);
        for(FName Id:{FName("damage"),FName("weakpoint"),FName("rapid"),FName("burn"),FName("armor")}) M->ToggleAffix(Id);
        TestEqual(TEXT("Four distinct sockets maximum"),M->Draft.Equipped[0].Affixes.Num(),4);
        M->SetWeapon(TEXT("shotgun"));
        TestFalse(TEXT("Incompatible affix removed on weapon swap"),M->Draft.Equipped[0].Affixes.Contains(TEXT("weakpoint")));
        M->PickPreset(1); M->RenderEditor();
        return true;
    }));
    ReadyForCapture(TEXT("loadout"));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this](){
        UWorld* World=RangeWorld();
        auto* M=World?World->GetAuthGameMode<ACityAffixLabMode>():nullptr;
        if(!M || !M->Panel) {AddError(TEXT("Lab editor unavailable"));return true;}
        auto Apply=FindRangeButton(M->Panel.ToSharedRef(),TEXT("应用配装并试射"));
        if(!TestTrue(TEXT("Apply from editor"),Apply.IsValid())) return true;
        Apply->SimulateClick();
        if(!TestTrue(TEXT("Four targets in live trial"),M->Targets.Num()==4 && M->Player() && !M->bEditor)) return true;
        auto* Player=M->Player(); auto* A=M->Targets[0].Get(); auto* Adj=M->Targets[1].Get();
        const float H=A->CurrentHP, Neighbor=Adj->CurrentHP;
        Player->HitCityTarget(A,1,false);
        TestEqual(TEXT("First hit inscribes two seals without consuming same-hit inscriptions"),H-A->CurrentHP,5.f);
        Player->HitCityTarget(A,1,false);
        TestEqual(TEXT("Second hit spends prior two seals, quill cooldown suppresses repeat"),H-A->CurrentHP,25.f);
        TestEqual(TEXT("Rupture area reaches neighbor"),Neighbor-Adj->CurrentHP,19.f);
        Player->HitCityTarget(A,1,false);
        TestEqual(TEXT("Third-shot triad reaches neighbor once"),Neighbor-Adj->CurrentHP,26.f);
        AddInfo(FString::Printf(TEXT("CITY_LAB_RUNES third_hit_target_damage=%.2f adjacent_damage=%.2f"),H-A->CurrentHP,Neighbor-Adj->CurrentHP));
        M->HandleLabKey(EKeys::Tab); M->PickPreset(0); M->HandleLabKey(EKeys::Escape);
        TestTrue(TEXT("Escape discards uncommitted preset"),M->Progress()->GetWeapon(0).Affixes.Contains(TEXT("arcane_quill")) && !M->bEditor);
        M->HandleLabKey(EKeys::T);
        TestTrue(TEXT("Reset restores target HP and clears pending effects"),M->Targets.Num()==4 && M->Targets[0]->CurrentHP==M->Targets[0]->MaxHealth());
        // Both direct and affix damage pass through the same live support shield.
        auto* Shielded=M->Targets[2].Get();
        auto* Support=World->SpawnActor<ACityCampaignEnemy>(Shielded->GetActorLocation()+FVector(0,230,0),FRotator::ZeroRotator);
        if(!TestNotNull(TEXT("Support fixture"),Support)) return true;
        Support->Configure(ECityFoe::Support);
        const float BeforeShield=Shielded->CurrentHP;
        M->Player()->HitCityTarget(Shielded,10.f,false);
        TestTrue(TEXT("Support shield reduces direct hit and quill exactly once"),FMath::IsNearlyEqual(BeforeShield-Shielded->CurrentHP,14.f*.65f,.01f));
        Support->StunFor(1.f);
        const float AfterShield=Shielded->CurrentHP;
        M->Player()->HitCityTarget(Shielded,10.f,false);
        TestTrue(TEXT("Interrupted shield restores full direct and two-seal rupture damage"),FMath::IsNearlyEqual(AfterShield-Shielded->CurrentHP,29.f,.01f));
        AddInfo(FString::Printf(TEXT("CITY_LAB_SHIELD protected=%.2f interrupted=%.2f"),BeforeShield-AfterShield,AfterShield-Shielded->CurrentHP));
        Support->Destroy();
        M->HandleLabKey(EKeys::Tab); M->PickPreset(0); M->StartTrial(true);
        TestTrue(TEXT("Sword preset selects edited off hand"),M->Player()->GetCitySlot()==1);
        return true;
    }));
    ReadyForCapture(TEXT("firing-range"));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this,Deadline,WaitUntil=0.0]() mutable {
        UWorld* World=RangeWorld();
        auto* M=World?World->GetAuthGameMode<ACityAffixLabMode>():nullptr;
        if(!M || !M->Player() || M->Targets.Num()!=4) {AddError(TEXT("Sword trial unavailable"));return true;}
        if(WaitUntil==0) WaitUntil=World->GetTimeSeconds()+.6;
        if(World->GetTimeSeconds()<WaitUntil && FPlatformTime::Seconds()<Deadline) return false;
        auto* BladeTarget=M->Targets[0].Get();
        const FVector At=BladeTarget->GetActorLocation();
        M->Player()->SetActorLocation(At-FVector(200,0,0)); M->PC()->SetControlRotation(FRotator::ZeroRotator);
        M->Player()->DoStartFiring(); M->Player()->DoStopFiring();
        AddInfo(FString::Printf(TEXT("CITY_LAB_BLADE damage=%.2f"),BladeTarget->MaxHealth()-BladeTarget->CurrentHP));
        TestTrue(TEXT("Equipped katana strikes real target"),BladeTarget->CurrentHP<BladeTarget->MaxHealth());
        M->HandleLabKey(EKeys::Tab); M->PickPreset(2); M->StartTrial(true);
        auto* A=M->Targets[0].Get(); auto* Adj=M->Targets[1].Get();
        M->Player()->HitCityTarget(A,1,false);
        TestEqual(TEXT("Cold application does not satisfy same-hit chilled requirement"),Adj->CurrentHP,Adj->MaxHealth());
        M->Player()->HitCityTarget(A,1,false);
        TestEqual(TEXT("Pre-existing chill triggers both area effects"),Adj->MaxHealth()-Adj->CurrentHP,26.f);
        TestTrue(TEXT("Cold and volt cooldowns block immediate repeat"),M->Player()->HitCityTarget(A,1,false)>0 && FMath::IsNearlyEqual(Adj->MaxHealth()-Adj->CurrentHP,35.f));
        AddInfo(FString::Printf(TEXT("CITY_LAB_CHILL third_hit_target_damage=%.2f adjacent_damage=%.2f"),A->MaxHealth()-A->CurrentHP,Adj->MaxHealth()-Adj->CurrentHP));
        TestTrue(TEXT("Control skills and ultimate are playable loadout"),M->Progress()->GetActiveSkill(0)==TEXT("frost_floor") && M->Progress()->GetActiveSkill(1)==TEXT("arc_lance") && M->Progress()->GetUltimate()==TEXT("barrage"));
        for(ACityCampaignEnemy* Target:M->Targets) if(IsValid(Target)) UGameplayStatics::ApplyDamage(Target,9000,M->PC(),M->Player(),nullptr);
        TestTrue(TEXT("Clearing range retains active trial"),M->bTrial && !M->bEditor && M->TrialStatus().Contains(TEXT("0 / 4")));
        M->HandleLabKey(EKeys::Tab); M->bLive=true; M->StartTrial(true);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this,Deadline](){
        UWorld* World=RangeWorld();
        auto* M=World?World->GetAuthGameMode<ACityAffixLabMode>():nullptr;
        if(!M || !M->bTrial || M->Targets.IsEmpty()) return true;
        auto* E=M->Targets[0].Get();
        if(IsValid(E) && E->GetActorLocation().X>=-309 && FPlatformTime::Seconds()<Deadline) return false;
        TestTrue(TEXT("Live enemy advances from its firing lane"),IsValid(E) && E->GetActorLocation().X < -309);
        UGameplayStatics::ApplyDamage(M->Player(),100000,nullptr,E,nullptr);
        return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this,Deadline](){
        UWorld* World=RangeWorld();
        auto* M=World?World->GetAuthGameMode<ACityAffixLabMode>():nullptr;
        if(!M) return true;
        if(!M->bEditor && FPlatformTime::Seconds()<Deadline) return false;
        TestTrue(TEXT("Death reopens editor with build intact"),M->bEditor && M->Draft.Equipped[0].Affixes.Contains(TEXT("frost_tip")));
        M->ReturnToPreparation(); return true;
    }));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitCityLab([this,Deadline,File,Before](){
        UWorld* World=RangeWorld();
        if(!World || !World->GetAuthGameMode<ACityCampaignMode>() || !World->GetGameInstance()->GetSubsystem<UCityProgression>()->Panel.IsValid())
        {
            if(FPlatformTime::Seconds()<Deadline) return false;
            AddError(TEXT("Return to campaign preparation timed out"));return true;
        }
        TArray<uint8> After;
        TestTrue(TEXT("Entire production profile is byte-identical after trial, death and return"),FFileHelper::LoadFileToArray(After,*File) && After==Before);
        AddInfo(FString::Printf(TEXT("CITY_LAB_PROFILE start_bytes=%d return_bytes=%d identical=%d"),Before.Num(),After.Num(),Before==After));
        return true;
    }));
    return true;
}
#endif
