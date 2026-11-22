#if WITH_DEV_AUTOMATION_TESTS
#include "Progression/CityProgression.h"
#include "Progression/CityProfileIO.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/GameInstance.h"

namespace
{
struct FSlotFixture
{
    FString Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/CityProgression")/FGuid::NewGuid().ToString());
    FString File=Dir/TEXT("profile.json");
    UGameInstance* Instance=NewObject<UGameInstance>();
    UCityProgression* NewReader(){return NewObject<UCityProgression>(Instance);}
    FSlotFixture(){IFileManager::Get().MakeDirectory(*Dir,true);}
    ~FSlotFixture(){IFileManager::Get().DeleteDirectory(*Dir,false,true);}
};

FCityItem Item(FName Kind,int32 Tier,TArray<FName> Affixes={})
{
    FCityItem W;
    W.Id=FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
    W.Kind=Kind; W.Rarity=Tier; W.Affixes=MoveTemp(Affixes); return W;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityEconomyTest,"VerticalCity.Progression.WorkshopAndSettlement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCityEconomyTest::RunTest(const FString&)
{
    FSlotFixture F;
    auto* P=F.NewReader();
    FCityProfile Seed;
    Seed.Equipped[0]=Item(TEXT("rifle"),3,{TEXT("damage"),TEXT("rapid")});
    Seed.Equipped[1]=Item(TEXT("sword"),1);
    Seed.Bag={Item(TEXT("rifle"),5),Item(TEXT("shotgun"),3)};
    FString Error;
    if (!TestTrue(TEXT("Write real fixture and reopen"), CityProfileIO::Write(F.File,Seed,Error) && P->OpenProfile(F.File))) return false;
    TArray<FString> Ids={Seed.Bag[0].Id,Seed.Bag[1].Id};
    TestFalse(TEXT("Protected hand blocks entire batch"),P->Salvage({Ids[0],Seed.Equipped[0].Id}));
    TestEqual(TEXT("Failed batch retains both"),P->GetProfile().Bag.Num(),2);
    auto& FS=IFileManager::Get();
    const FString Block=F.File+TEXT(".tmp");
    FS.MakeDirectory(*Block,false);
    TestFalse(TEXT("Failed write rolls back batch"),P->Salvage(Ids));
    TestEqual(TEXT("Balance unchanged"),P->GetProfile().Parts,int64(0));
    FS.DeleteDirectory(*Block,false);
    TestTrue(TEXT("Retry batch"),P->Salvage(Ids));
    TestFalse(TEXT("Replaying scrap cannot pay twice"),P->Salvage(Ids));
    TestEqual(TEXT("Exact tier sum"),P->GetProfile().Parts,int64(8));
    TestTrue(TEXT("Forge exact template"),P->Manufacture(TEXT("hand_cannon")));
    TestTrue(TEXT("Reroll one selected affix"),P->Reroll(Seed.Equipped[0].Id,0,TEXT("damage")));
    TestFalse(TEXT("Stale reroll cannot spend again"),P->Reroll(Seed.Equipped[0].Id,0,TEXT("damage")));
    TestEqual(TEXT("Second affix locked"),P->GetWeapon(0).Affixes[1],FName(TEXT("rapid")));
    TestEqual(TEXT("Forge/reroll consume exactly 8"),P->GetProfile().Parts,int64(0));
    if (!TestEqual(TEXT("Single crafted item"),P->GetProfile().Bag.Num(),1)) return false;
    const FString Crafted=P->GetProfile().Bag[0].Id;
    TestTrue(TEXT("Equip preserves displaced weapon"),P->Equip(Crafted,1));
    TestTrue(TEXT("Ready to depart"),P->BeginRun());
    TestFalse(TEXT("Forge locked in combat"),P->Manufacture(TEXT("sidearm")));
    const FString Preview=P->PreviewReward(1314,TEXT("smg"));
    TestTrue(TEXT("Grant committed pair"),P->GrantReward(TEXT("room4"),1314,TEXT("smg")));
    TestTrue(TEXT("Retry same receipt"),P->GrantReward(TEXT("room4"),1314,TEXT("smg")));
    const auto& Bag=P->GetProfile().Bag;
    if (!TestEqual(TEXT("Only two promised items"),Bag.Num(),3)) return false;
    TestEqual(TEXT("Reward matches public preview"),FCityItemCatalog::Describe(Bag[1])+TEXT("\n\n")+FCityItemCatalog::Describe(Bag[2]),Preview);
    TestTrue(TEXT("Victory transaction"),P->FinishRun(true));
    TestFalse(TEXT("Duplicate victory rejected"),P->FinishRun(true));
    auto* Read=F.NewReader();
    if (!TestTrue(TEXT("Relaunch"),Read->OpenProfile(F.File))) return false;
    TestEqual(TEXT("Victory gives one point"),Read->GetProfile().Points,3);
    TestEqual(TEXT("Equipped item level +1"),Read->GetWeapon(1).Level,int64(2));
    TestTrue(TEXT("Second expedition"),Read->BeginRun());
    TestTrue(TEXT("Death transaction"),Read->FinishRun(false));
    TestFalse(TEXT("Death not replayed"),Read->FinishRun(false));
    TestTrue(TEXT("Both hands lost"),Read->GetWeapon(0).Id.IsEmpty() && Read->GetWeapon(1).Id.IsEmpty());
    TestFalse(TEXT("Inventory covers replacement; no free extras"),Read->ClaimBasicWeapons());
    TestTrue(TEXT("Use backpack for next life"),Read->Equip(Read->GetProfile().Bag[0].Id,0) && Read->Equip(Read->GetProfile().Bag[0].Id,1));
    TestTrue(TEXT("Second consecutive death"),Read->BeginRun() && Read->FinishRun(false));
    TestTrue(TEXT("Exactly one missing item issued"),Read->ClaimBasicWeapons());
    TestTrue(TEXT("One remaining asset must come from inventory"),Read->GetWeapon(1).Id.IsEmpty() && Read->GetProfile().Bag.Num()==1);
    TestFalse(TEXT("Cannot farm basic kit"),Read->ClaimBasicWeapons());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityProfileTest,"VerticalCity.Progression.RecoveryAndTwoPages",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCityProfileTest::RunTest(const FString&)
{
    FSlotFixture F;
    auto* P=F.NewReader();
    const FString Legacy=F.Dir/TEXT("source.json");
    const FString Source=TEXT("{\"version\":1,\"equipped\":[{\"id\":\"11111111111111111111111111111111\",\"template\":\"rifle\",\"frame\":\"automatic\",\"level\":2,\"rarity\":1,\"damage_roll\":10000,\"affixes\":[]},{}],\"bag\":[],\"active\":false,\"last_result\":\"\",\"active_skill_ids\":[12,\"arc_lance\"],\"skill_tree\":{\"points\":2,\"nodes\":[\"reload_drill\"],\"respecs\":0}}");
    if (!TestTrue(TEXT("Open native then import read-only legacy"),FFileHelper::SaveStringToFile(Source,*Legacy) && P->OpenProfile(F.File) && P->ImportLegacy(Legacy))) return false;
    FString Actual;
    FFileHelper::LoadFileToString(Actual,*Legacy);
    TestEqual(TEXT("Original source never modified"),Actual,Source);
    TestEqual(TEXT("Gun-only legacy build inferred"),P->GetProfile().Page,1);
    TestEqual(TEXT("Malformed slot repaired without shifting valid binding"),P->GetActiveSkill(0),FName(TEXT("echo_pin")));
    TestEqual(TEXT("Valid slot retained"),P->GetActiveSkill(1),FName(TEXT("arc_lance")));
    TestTrue(TEXT("Level 2 rifle delivers 8% bonus"),FMath::IsNearlyEqual(P->GetStats(0).Damage,19.f*1.08f));
    TestTrue(TEXT("Gun page applies 10% reload reduction"),FMath::IsNearlyEqual(P->GetStats(0).Reload,1.65f*.9f));
    TestTrue(TEXT("Switch freely and buy survival branch"),P->SwitchPage(0) && P->Unlock(TEXT("endurance")) && P->Unlock(TEXT("parry_aid")));
    TestTrue(TEXT("Gun page retains independent build"),P->SwitchPage(1) && P->HasNode(TEXT("reload_drill")) && !P->HasNode(TEXT("endurance")));
    TestTrue(TEXT("Prepare skill loadout"),P->SetActiveSkill(0,TEXT("patch_kit")) && P->SetUltimate(TEXT("hammer")));
    TestTrue(TEXT("Equip shortage restored and run starts"),P->ClaimBasicWeapons() && P->BeginRun());
    TestFalse(TEXT("Combat blocks page switch"),P->SwitchPage(0));
    FString Import=F.File+TEXT(".imported.json");
    auto* Read=F.NewReader();
    if (!TestTrue(TEXT("Interrupted run recovered"),Read->OpenProfile(Import))) return false;
    TestEqual(TEXT("Interruption settled"),Read->GetProfile().Result,FName(TEXT("interrupted")));
    TestTrue(TEXT("Permanent skill choices retained"),Read->GetUltimate()==TEXT("hammer") && Read->GetActiveSkill(0)==TEXT("patch_kit"));
    TestTrue(TEXT("Both hands lost on interruption"),Read->GetWeapon(0).Id.IsEmpty() && Read->GetWeapon(1).Id.IsEmpty());
    TestTrue(TEXT("Survival page persisted"),Read->SwitchPage(0) && Read->HasNode(TEXT("parry_aid")));
    TestTrue(TEXT("Restore basic kit and store rollback point"),Read->ClaimBasicWeapons() && Read->SwitchPage(1));
    TestTrue(TEXT("Corrupt file"),FFileHelper::SaveStringToFile(TEXT("{}"),*Import));
    TestTrue(TEXT("Restore verified backup"),Read->OpenProfile(Import) && Read->GetProfile().Page==0 && Read->HasNode(TEXT("parry_aid")));
    TestTrue(TEXT("Native profile intact"),Read->OpenProfile(F.File) && Read->GetWeapon(0).Level==1 && Read->GetProfile().Bag.Num()==2);
    const FString Duplicate=Source.Replace(TEXT("},{}]"),TEXT("},{\"id\":\"11111111111111111111111111111111\",\"template\":\"rifle\",\"frame\":\"automatic\",\"level\":1,\"rarity\":1,\"damage_roll\":10000,\"affixes\":[]}]"));
    FFileHelper::SaveStringToFile(Duplicate,*Legacy);
    TestFalse(TEXT("Duplicate identity rejects profile with no good backup"),Read->OpenProfile(Legacy));
    FFileHelper::LoadFileToString(Actual,*Legacy);
    TestEqual(TEXT("Rejected profile preserved verbatim"),Actual,Duplicate);
    return true;
}
#endif

