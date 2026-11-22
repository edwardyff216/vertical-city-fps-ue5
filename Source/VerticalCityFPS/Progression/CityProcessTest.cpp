#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Progression/CityProgression.h"
#include "Progression/CityProfileIO.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "Layout/Children.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
FString CityProbeText(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetType()==TEXT("STextBlock"))
        return StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    FChildren* Children=Widget->GetChildren();
    for (int32 I=0;I<Children->Num();++I)
    {
        FString S=CityProbeText(Children->GetChildAt(I));
        if (!S.IsEmpty()) return S;
    }
    return {};
}
TSharedPtr<SButton> CityProbeButton(const TSharedRef<SWidget>& Widget,const FString& Label)
{
    if (!Widget->IsEnabled() || !Widget->GetVisibility().IsVisible()) return {};
    if (Widget->GetType()==TEXT("SButton") && CityProbeText(Widget).Contains(Label))
        return StaticCastSharedRef<SButton>(Widget);
    FChildren* Children=Widget->GetChildren();
    for (int32 I=0;I<Children->Num();++I)
        if (auto B=CityProbeButton(Children->GetChildAt(I),Label)) return B;
    return {};
}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCityProcessTest,"VerticalCity.Progression.StandaloneRestart",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
void FCityProcessTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    FString Step;
    if (FParse::Value(FCommandLine::Get(),TEXT("CityPersistenceProbe="),Step) && (Step==TEXT("Write") || Step==TEXT("Recover") || Step==TEXT("Clean")))
    {
        Names.Add(Step); Commands.Add(Step);
    }
}

bool FCityProcessTest::RunTest(const FString& Step)
{
    FString Native;
    if (!TestTrue(TEXT("Explicit isolated profile path"),FParse::Value(FCommandLine::Get(),TEXT("CityProfile="),Native))) return false;
    const FString Dir=FPaths::GetPath(Native), Source=Dir/TEXT("legacy.json"), Marker=Dir/TEXT("active.pid");
    UCityProgression* P=nullptr;
    for (const auto& C:GEngine->GetWorldContexts())
        if (UWorld* World=C.World(); World && World->IsGameWorld() && World->GetGameInstance())
        {P=World->GetGameInstance()->GetSubsystem<UCityProgression>();break;}
    if (!TestNotNull(TEXT("Live game instance progression"),P) || !TestTrue(TEXT("Open live save slot"),P->OpenDefault())) return false;
    if (Step==TEXT("Clean"))
    {
        if (!TestTrue(TEXT("Recovered profile survives a new launch"),P->GetProfile().Character==TEXT("white_01") && !P->GetProfile().bActive && P->GetProfile().Result==TEXT("defeat") && P->GetProfile().Parts==4 && P->HasNode(TEXT("endurance")))) return false;
        if (!TestTrue(TEXT("Record normal shutdown step"),FFileHelper::SaveStringToFile(TEXT("clean"),*(Dir/TEXT("clean.ok"))))) return false;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
        {
            FPlatformMisc::RequestExit(false,TEXT("CityVerificationNormalExit"));
            return false;
        }),2.f);
        return true;
    }
    if (Step==TEXT("Write"))
    {
        const auto& Fresh=P->GetProfile();
        if(!TestTrue(TEXT("Fresh native profile has original hands, bag, character and budget"),Fresh.Character==TEXT("s07") && Fresh.Parts==0 && Fresh.Points==2 && Fresh.Bag.Num()==2 && Fresh.Bag[0].Kind==TEXT("sword") && Fresh.Bag[1].Kind==TEXT("rifle") && Fresh.Equipped[0].Kind==TEXT("rifle") && Fresh.Equipped[1].Kind==TEXT("shotgun"))) return false;
        FCityProfile Old;
        if(!TestTrue(TEXT("Missing legacy character defaults to S-07"),CityProfileIO::Read(Source,Old) && Old.Character==TEXT("s07"))) return false;
        FString LegacyText;
        FFileHelper::LoadFileToString(LegacyText,*Source);
        const FString Unknown=Dir/TEXT("legacy-unknown-character.json");
        LegacyText.InsertAt(LegacyText.Find(TEXT("{"))+1,TEXT("\"character_id\":\"retired\","));
        if(!TestTrue(TEXT("Unknown offline legacy character falls back to S-07"),FFileHelper::SaveStringToFile(LegacyText,*Unknown) && CityProfileIO::Read(Unknown,Old) && Old.Character==TEXT("s07"))) return false;
        if (!TestTrue(TEXT("Import into separate slot"),P->ImportLegacy(Source))) return false;
        UWorld* PlayWorld=P->GetGameInstance()->GetWorld();
        auto* PC=PlayWorld?PlayWorld->GetFirstPlayerController():nullptr;
        if (!TestNotNull(TEXT("Live player controller"),PC)) return false;
        bool Departed=false,Dismissed=false;
        P->ShowPreparation(PC,FSimpleDelegate::CreateLambda([&Departed](){Departed=true;}));
        auto Click=[&](const FString& Caption)
        {
            if (!P->Panel.IsValid()) {AddError(TEXT("No panel for ")+Caption);return false;}
            const auto Btn=CityProbeButton(P->Panel.ToSharedRef(),Caption);
            if (!TestTrue(TEXT("Enabled Slate button: ")+Caption,Btn.IsValid())) return false;
            Btn->SimulateClick();
            return true;
        };
        const TCHAR* Steps[]={
            TEXT("白角色"),TEXT("S-07"),TEXT("白角色"),TEXT("工坊"),TEXT("333333"),TEXT("444444"),TEXT("拆解所选 2"),TEXT("确认销毁 2"),
            TEXT("重掷 强击"),TEXT("L-16 斥候步枪"),TEXT("技能树"),TEXT("体能训练"),
            TEXT("免费切换到枪械配置"),TEXT("装填训练"),TEXT("作战技能"),TEXT("C · 回声钉"),
            TEXT("速修"),TEXT("G · 飞刃齐射"),TEXT("霜脊震地"),TEXT("出战")
        };
        for (const TCHAR* S:Steps) if (!Click(S)) return false;
        if (!TestTrue(TEXT("UI deployed after committing run"),Departed && P->GetProfile().bActive)) return false;
        if (!TestTrue(TEXT("Deployed character is saved and locked"),P->GetProfile().Character==TEXT("white_01") && !P->SetCharacter(TEXT("s07")) && CityProfileIO::Read(Native+TEXT(".imported.json"),Old) && Old.Character==TEXT("white_01"))) return false;
        if (!TestTrue(TEXT("Real reward comparison panel"),P->PresentReward(PC,TEXT("declared-room-2"),909,TEXT("shotgun"),FSimpleDelegate::CreateLambda([&Dismissed](){Dismissed=true;})))) return false;
        if (!Click(TEXT("装备到主手")) || !Click(TEXT("收好奖励")) || !TestTrue(TEXT("Actual reward replaced main hand and dismissed overlay"),Dismissed && P->GetWeapon(0).Kind==TEXT("shotgun"))) return false;
        if (!TestEqual(TEXT("Four parts left"),P->GetProfile().Parts,int64(4))) return false;
        const FString Pid=FString::FromInt(FPlatformProcess::GetCurrentProcessId());
        return TestTrue(TEXT("Ready for external termination"),FFileHelper::SaveStringToFile(Pid,*Marker,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    }
    FString First;
    FFileHelper::LoadFileToString(First,*Marker);
    if (!TestTrue(TEXT("Second process has independent OS PID"),!First.IsEmpty() && FCString::Atoi(*First)!=int32(FPlatformProcess::GetCurrentProcessId()))) return false;
    if (!TestTrue(TEXT("Active interruption settled"),P->GetProfile().Character==TEXT("white_01") && P->GetProfile().Result==TEXT("interrupted") && !P->GetProfile().bActive && P->GetWeapon(0).Id.IsEmpty() && P->GetWeapon(1).Id.IsEmpty())) return false;
    if (!TestEqual(TEXT("Backpack survived including displaced primary and unselected reward"),P->GetProfile().Bag.Num(),3)) return false;
    const auto& B=P->GetProfile().Bag;
    if (!TestTrue(TEXT("Manufacture and reward identities preserved"),B[0].Kind==TEXT("scout") && B[1].Kind==TEXT("shotgun") && B[2].Id==TEXT("11111111111111111111111111111111") && B[2].Affixes.Num()==2 && B[2].Affixes[0]!=TEXT("damage") && B[2].Affixes[1]==TEXT("rapid"))) return false;
    if (!TestTrue(TEXT("Independent page, wallet, abilities survive"),P->GetProfile().Parts==4 && P->HasNode(TEXT("reload_drill")) && P->GetActiveSkill(0)==TEXT("patch_kit") && P->GetUltimate()==TEXT("ice") && P->SwitchPage(0) && P->HasNode(TEXT("endurance")))) return false;
    if (!TestTrue(TEXT("Re-equip from bag and die a second time"),P->Equip(P->GetProfile().Bag[0].Id,0) && P->Equip(P->GetProfile().Bag[0].Id,1) && P->BeginRun() && P->FinishRun(false))) return false;
    if (!TestTrue(TEXT("Reissue shortage only"),P->ClaimBasicWeapons() && !P->ClaimBasicWeapons() && P->GetProfile().Bag.Num()==1)) return false;
    if (!TestTrue(TEXT("Return to independent native slot"),P->SwitchArchive() && P->GetProfile().Character==TEXT("s07") && P->GetWeapon(0).Level==1 && P->GetProfile().Parts==0)) return false;
    const FString Imported=Native+TEXT(".imported.json");
    if (!TestTrue(TEXT("Damage only isolated imported primary"),FFileHelper::SaveStringToFile(TEXT("invalid"),*Imported))) return false;
    if (!TestTrue(TEXT("Fallback to valid previous imported backup"),P->SwitchArchive() && P->GetProfile().Result==TEXT("defeat") && P->GetProfile().Parts==4)) return false;
    return TestTrue(TEXT("Completed independent process recovery"),FFileHelper::SaveStringToFile(TEXT("passed"),*(Dir/TEXT("recovery.ok"))));
}
#endif

