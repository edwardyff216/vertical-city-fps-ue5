#include "Campaign/CityCampaignMode.h"
#include "CityRoomShell.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "CityCampaignPanel.h"
#include "CityEnemyBolt.h"
#include "Progression/CityProgression.h"
#include "ShooterCharacter.h"
#include "CityWeaponEffects.h"
#include "ShooterPlayerController.h"
#include "Abilities/CityAbilities.h"
#include "Abilities/CityAbilityCatalog.h"
#include "Engine/GameInstance.h"
#include "ShooterProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "PipelineStateCache.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
class FCityMenuInput final : public IInputProcessor
{
public:
    explicit FCityMenuInput(ACityCampaignMode* InMode) : Mode(InMode) {}
    virtual void Tick(float,FSlateApplication&,TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&,const FKeyEvent& Key) override
    {
        if(!Mode.IsValid() || Key.IsRepeat()) return false;
        if (Key.GetKey()==EKeys::H) return Mode->ToggleUltimateMenu();
        if (Key.GetKey()!=EKeys::Escape) return false;
        if (Mode->IsUltimateMenuOpen()) return Mode->ToggleUltimateMenu();
        Mode->TogglePause(); return true;
    }
    virtual const TCHAR* GetDebugName() const override { return TEXT("VerticalCityCampaignMenu"); }
private:
    TWeakObjectPtr<ACityCampaignMode> Mode;
};
const TCHAR* LayoutNames[]={TEXT("错层仓库"),TEXT("白区转运仓"),TEXT("横桥检修间"),TEXT("坡台装卸间"),TEXT("管线走廊"),
    TEXT("四柱中庭"),TEXT("折返工坊"),TEXT("双列货场"),TEXT("中继机房"),TEXT("机组大厅")};
const TCHAR* Encounters[]={TEXT("高台射手压制"),TEXT("近身围攻 · 两波"),TEXT("支援者护阵")};
const TCHAR* BossNames[]={TEXT("镜塔哨卫"),TEXT("炉心破阵者"),TEXT("中继织盾者")};
const TCHAR* BossIDs[]={TEXT("spire"),TEXT("breaker"),TEXT("relay")};
constexpr int32 Arenas[]={2,3,5};
const FName Weapons[]={TEXT("rifle"),TEXT("shotgun"),TEXT("sword")};
const FName Abilities[]={TEXT("reload"),TEXT("vitality"),TEXT("slide_refund"),TEXT("swap_haste"),TEXT("counter"),TEXT("riposte")};
const TCHAR* AbilityNames[]={TEXT("快速装填"),TEXT("强化躯体"),TEXT("滑铲回收"),TEXT("换枪疾行"),TEXT("防反反击"),TEXT("蓄势反击")};
FString AbilityName(FName Id)
{
    for (int I=0;I<6;++I) if (Abilities[I]==Id) return AbilityNames[I];
    return Id.ToString();
}
int32 BossFor(const FString& History) { return History==TEXT("UUU") ? 0 : History==TEXT("DDD") ? 1 : 2; }
FString Forecast(const FString& Prefix)
{
    if (Prefix.Contains(TEXT("U")) && Prefix.Contains(TEXT("D"))) return BossNames[2];
    const int32 Pure=Prefix.StartsWith(TEXT("U")) ? 0 : 1;
    return Prefix.Len()==3 ? FString(BossNames[Pure]) : FString(BossNames[Pure])+TEXT(" / ")+BossNames[2];
}
template<typename T> void Shuffle(TArray<T>& Items,FRandomStream& Stream)
{
    for(int32 I=Items.Num()-1;I>0;--I) Items.Swap(I,Stream.RandRange(0,I));
}
}
ACityCampaignMode::ACityCampaignMode()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    static ConstructorHelpers::FClassFinder<AShooterCharacter> PawnClass(TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"));
    static ConstructorHelpers::FClassFinder<AShooterPlayerController> ControlClass(TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterPlayerController"));
    DefaultPawnClass=PawnClass.Class;
    PlayerControllerClass=ControlClass.Class;
}
APlayerController* ACityCampaignMode::PC() const { return GetWorld()->GetFirstPlayerController(); }
AShooterCharacter* ACityCampaignMode::Player() const { return PC()?Cast<AShooterCharacter>(PC()->GetPawn()):nullptr; }
UCityProgression* ACityCampaignMode::Progress() const { return GetGameInstance()->GetSubsystem<UCityProgression>(); }
void ACityCampaignMode::BeginPlay()
{
    Super::BeginPlay();
    int32 Seed=FMath::Rand();
    FParse::Value(FCommandLine::Get(),TEXT("CitySeed="),Seed);
    Rng.Initialize(Seed);
    Current.Seed=Rng.RandRange(1,100000000);
    Current.Layout=Rng.RandRange(0,1);
    Shell=GetWorld()->SpawnActor<ACityRoomShell>();
    check(Shell);
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("CityMovingSwordCapture"))) Current.Encounter=1;
#endif
    Shell->Build(Current.Layout,0,INDEX_NONE,Current.Encounter,(Current.Seed & 256) != 0);
    SpawnWave();
    for(ACityCampaignEnemy* Enemy : Enemies) Enemy->SetActorTickEnabled(false);
    RoomPreparedAt=FPlatformTime::Seconds();
    PreparedFrame=GFrameCounter;
    if (auto* Viewport=GetWorld()->GetGameViewport())
    {
        SAssignNew(Panel,SCityCampaignPanel).Mode(this);
        Viewport->AddViewportWidgetContent(Panel.ToSharedRef(),800);
        if(FSlateApplication::IsInitialized())
        {
            MenuInput=MakeShared<FCityMenuInput>(this);
            FSlateApplication::Get().RegisterInputPreProcessor(MenuInput,0);
        }
    }
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("CityPerf")) && GEngine)
    {
        GEngine->bSmoothFrameRate=false;
        GEngine->bUseFixedFrameRate=false;
        GEngine->SetMaxFPS(0);
        if(auto* Sync=IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))) Sync->Set(0,ECVF_SetByCode);
        UE_LOG(LogTemp,Display,TEXT("City performance capture uncapped fixedRate=0 smoothRate=0 vsync=0"));
    }
    FParse::Value(FCommandLine::Get(),TEXT("CityRouteTest="),TestRoute);
    TestRoute=TestRoute.ToUpper();
    FParse::Value(FCommandLine::Get(),TEXT("CitySwordCapture="),SwordCaptureFolder);
    FParse::Value(FCommandLine::Get(),TEXT("CityEnemyCueCapture="),EnemyCueFolder);
    bBuildShowcase=FParse::Param(FCommandLine::Get(),TEXT("CityBuildShowcase"));
    bRecording=FParse::Param(FCommandLine::Get(),TEXT("CityRouteRecord"));
    if(bRecording && !TestRoute.IsEmpty()) FParse::Value(FCommandLine::Get(),TEXT("CityAudioDir="),AudioFolder);
    bFast=FParse::Param(FCommandLine::Get(),TEXT("CityRouteFast"));
    if (!TestRoute.IsEmpty() && TestRoute.Len()!=3) EndBot(false,TEXT("route needs three directions"));
    for(TCHAR Step : TestRoute) if(Step!=TEXT('U') && Step!=TEXT('D')) EndBot(false,TEXT("unknown route direction"));
#endif
}
void ACityCampaignMode::EndPlay(const EEndPlayReason::Type Reason)
{
    if(MenuInput && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(MenuInput);
    MenuInput.Reset();
    if(Panel.IsValid() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Panel.ToSharedRef());
    Panel.Reset();
    Super::EndPlay(Reason);
}
void ACityCampaignMode::Preparation()
{
    SetPhase(ECityPhase::Preparation);
    Progress()->ShowPreparation(PC(),FSimpleDelegate::CreateUObject(this,&ACityCampaignMode::StartRun));
}
bool ACityCampaignMode::RequestPreparedRoom()
{
    const double Now=FPlatformTime::Seconds();
    if(DepartureRequestedAt==0) DepartureRequestedAt=Now;
    // Give the first visible room a chance to submit its render resources before accepting input.
    // Long-running background shader jobs must never keep a ready player in preparation indefinitely.
    return (GFrameCounter>=PreparedFrame+5 && PipelineStateCache::GetNumActivePipelinePrecompileTasks()==0)
        || Now-DepartureRequestedAt>2.;
}
void ACityCampaignMode::StartRun()
{
    RunID=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Route.Reset(); Depth=0; Notice.Reset(); bWon=false; bPendingFinish=false;
    const double Requested=FPlatformTime::Seconds();
    UE_LOG(LogTemp,Display,TEXT("City campaign started seed=%d routeTest=%s debugDamage=%d"),Rng.GetInitialSeed(),*TestRoute,bFast);
    Enter(Current,true);
    UE_LOG(LogTemp,Display,TEXT("City departure prepared_ms=%.2f handler_ms=%.2f request_to_combat_ms=%.2f"),(Requested-RoomPreparedAt)*1000.,(FPlatformTime::Seconds()-Requested)*1000.,(FPlatformTime::Seconds()-DepartureRequestedAt)*1000.);
}
void ACityCampaignMode::SetModal(bool Enable)
{
    if (Enable==bModal || !PC()) return;
    bModal=Enable;
    if (auto* P=Player()) P->CancelCityInput();
    PC()->FlushPressedKeys();
    bFireHeld=false;
    PC()->bShowMouseCursor=Enable;
    if (Enable)
    {
        PC()->SetInputMode(FInputModeUIOnly());
        UGameplayStatics::SetGamePaused(this,true);
    }
    else
    {
        UGameplayStatics::SetGamePaused(this,false);
        PC()->SetInputMode(FInputModeGameOnly());
    }
}
int32 ACityCampaignMode::SyncMarker() const
{
    const double Now=FPlatformTime::Seconds();
    if(AudioStopped>0 && Now-AudioStopped<.5) return 2;
    if(AudioStarted>0 && Now-AudioStarted<.5) return 1;
    return 0;
}
bool ACityCampaignMode::HasCampaignOverlay() const
{
    return bUserPaused || bUltimateMenu || Phase==ECityPhase::Ability || Phase==ECityPhase::Result || Phase==ECityPhase::Retry;
}
bool ACityCampaignMode::ToggleUltimateMenu()
{
    if (!bUltimateMenu && (!bStarted || bUserPaused || !Player() || Player()->IsDead() || GetWorld()->IsPaused() ||
        (Phase!=ECityPhase::Combat && Phase!=ECityPhase::Gate && Phase!=ECityPhase::Extract))) return false;
    bUltimateMenu=!bUltimateMenu;
    SetModal(bUltimateMenu);
    if (Panel) Panel->Refresh();
    return true;
}
void ACityCampaignMode::TogglePause()
{
    if(!PC() || !bStarted || !TestRoute.IsEmpty()) return;
    bUserPaused=!bUserPaused;
    if(auto* P=Player()) P->CancelCityInput();
    PC()->FlushPressedKeys();
    if(bUserPaused)
    {
        bUserAcquiredPause=!GetWorld()->IsPaused() && UGameplayStatics::SetGamePaused(this,true);
        PC()->bShowMouseCursor=true;
        PC()->SetInputMode(FInputModeUIOnly());
    }
    else if(bUserAcquiredPause)
    {
        UGameplayStatics::SetGamePaused(this,false);
        PC()->bShowMouseCursor=false;
        PC()->SetInputMode(FInputModeGameOnly());
        bUserAcquiredPause=false;
    }
    if(Panel) Panel->Refresh();
}
void ACityCampaignMode::SetPhase(ECityPhase Next)
{
    Phase=Next;
    PhaseStarted=FPlatformTime::Seconds();
    SetModal(HasCampaignOverlay());
    if (Panel) Panel->Refresh();
    BotStateTime=0;
}
void ACityCampaignMode::CleanupRoom()
{
    if(auto* P=Player()) if(auto* Skills=P->FindComponentByClass<UCityAbilities>()) Skills->ClearRoomEffects();
    for(ACityCampaignEnemy* E : Enemies) if (IsValid(E)) E->Destroy();
    Enemies.Reset();
    for(TActorIterator<ACityEnemyBolt> It(GetWorld());It;++It) It->Destroy();
    for(TActorIterator<AShooterProjectile> It(GetWorld());It;++It) It->Destroy();
    if(IsValid(Shell)) { Shell->SetActorEnableCollision(false); Shell->Destroy(); }
}
void ACityCampaignMode::Enter(const FCityRoomOffer& Room,bool ReuseFirstRoom)
{
    if(!ReuseFirstRoom) CleanupRoom();
    ShowcaseCasts=0;
    Current=Room;
    Offers.Reset(); Wave=0; NextWave=-1; BotStage=0; BotDoor=INDEX_NONE;
    if(!ReuseFirstRoom)
    {
        Shell=GetWorld()->SpawnActor<ACityRoomShell>();
        Shell->Build(Current.Layout,Depth,Current.Boss,Current.Encounter,(Current.Seed & 256) != 0);
    }
    if(auto* P=Player())
    {
        P->GetCharacterMovement()->StopMovementImmediately();
        P->SetActorLocationAndRotation(ACityRoomShell::Start(),FRotator::ZeroRotator,false,nullptr,ETeleportType::TeleportPhysics);
        PC()->SetControlRotation(FRotator::ZeroRotator);
        P->RefreshCityLoadout();
        if(auto* Effects=P->FindComponentByClass<UCityWeaponEffects>()) Effects->ResetRoom();
    }
    PreviousPosition=ACityRoomShell::Start();
    SetPhase(ECityPhase::Combat);
    if(ReuseFirstRoom) { for(ACityCampaignEnemy* Enemy : Enemies) Enemy->SetActorTickEnabled(true); }
    else SpawnWave();
    UE_LOG(LogTemp,Display,TEXT("City room depth=%d layout=%d encounter=%d boss=%s route=%s ticket=%s"),
        Depth,Current.Layout,Current.Encounter, Current.Boss==INDEX_NONE?TEXT("none"):BossIDs[Current.Boss],*Route,*Ticket());
}
void ACityCampaignMode::Spawn(ECityFoe Kind,FVector Position)
{
    FActorSpawnParameters Args;
    Args.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if(auto* E=GetWorld()->SpawnActor<ACityCampaignEnemy>(Position,FRotator(0,180,0),Args))
    {
        E->Configure(Kind);
        Enemies.Add(E);
    }
}
void ACityCampaignMode::SpawnWave()
{
    const FVector Left(-380,-480,100),Right(-390,465,100),Center(-130,0,100);
    // Keep all high-ground encounters on the walkable deck on either exit layout.
    const float OffsetY = Shell->IsUpperGate(2) ? 0.f : -520.f;
    const FVector A(1090,-430+OffsetY,383),B(1200,-170+OffsetY,383),C(1070,72+OffsetY,383);
    if(Current.Boss!=INDEX_NONE)
    {
        if(Current.Boss==0) { Spawn(ECityFoe::Spire,B+FVector(0,0,40)); Spawn(ECityFoe::Shooter,A); Spawn(ECityFoe::Melee,Right); }
        else if(Current.Boss==1) { Spawn(ECityFoe::Breaker,FVector(380,40,140)); Spawn(ECityFoe::Melee,Right); Spawn(ECityFoe::Melee,Left); }
        else { Spawn(ECityFoe::Relay,B+FVector(0,0,40)); Spawn(ECityFoe::Support,A); Spawn(ECityFoe::Blink,Left); }
        return;
    }
    switch(Current.Encounter)
    {
        case 0: Spawn(ECityFoe::Melee,Right); Spawn(ECityFoe::Shooter,A); Spawn(ECityFoe::Shooter,B); Spawn(ECityFoe::Shooter,C); break;
        case 1:
            if(Wave==0) { Spawn(ECityFoe::Melee,Left); Spawn(ECityFoe::Melee,Right); Spawn(ECityFoe::Melee,Center); }
            else { Spawn(ECityFoe::Blink,Left); Spawn(ECityFoe::Melee,Right); }
            break;
        default: Spawn(ECityFoe::Melee,Right); Spawn(ECityFoe::Support,B); Spawn(ECityFoe::Shooter,A); Spawn(ECityFoe::Shooter,C); break;
    }
}
FString ACityCampaignMode::Ticket() const { return FString::Printf(TEXT("%s-%d-%d"),*RunID,Depth,Current.Seed); }
void ACityCampaignMode::DropAmmo(FVector Position)
{
    if(Phase==ECityPhase::Combat && IsValid(Shell)) Shell->AmmoDrop(Position-FVector(0,0,35));
}
void ACityCampaignMode::Cleared()
{
    if(Phase!=ECityPhase::Combat) return;
    if(auto* P=Player()) { P->DoStopFiring(); P->HealCity(18); }
    bFireHeld=false;
    for(TActorIterator<ACityEnemyBolt> It(GetWorld());It;++It) It->Destroy();
    if(auto* P=Player()) if(auto* Skills=P->FindComponentByClass<UCityAbilities>()) Skills->ClearRoomEffects();
    UE_LOG(LogTemp,Display,TEXT("City clear depth=%d waves=%d ticket=%s"),Depth,Wave+1,*Ticket());
    Award();
}
void ACityCampaignMode::Award()
{
    bPendingFinish=false;
    if(Current.Reward==0)
    {
        if(Depth==4)
        {
            if(Progress()->GrantReward(Ticket(),Current.Seed,Current.Weapon))
            {
                UE_LOG(LogTemp,Display,TEXT("City boss reward auto-granted kind=%s ticket=%s"),*Current.Weapon.ToString(),*Ticket());
                AfterReward();
            }
            else { Notice=TEXT("终局奖励尚未保存"); SetPhase(ECityPhase::Retry); }
            return;
        }
        SetPhase(ECityPhase::Weapon);
        if(!Progress()->PresentReward(PC(),Ticket(),Current.Seed,Current.Weapon,FSimpleDelegate::CreateUObject(this,&ACityCampaignMode::AfterReward)))
        {
            Notice=TEXT("武器奖励尚未保存");
            SetPhase(ECityPhase::Retry);
        }
    }
    else if(Current.Reward==1) SetPhase(ECityPhase::Ability);
    else { if(auto* P=Player()) P->HealCity(30); AfterReward(); }
}
void ACityCampaignMode::AfterReward()
{
    if(auto* P=Player()) P->RefreshCityLoadout();
    if(Depth==4)
    {
        SetPhase(ECityPhase::Extract);
        Shell->OpenExtraction();
        BotStage=0;
        UE_LOG(LogTemp,Display,TEXT("City extract open boss=%s reward=%s"),BossIDs[Current.Boss],*Current.Weapon.ToString());
    }
    else { OpenRoomDoors(); SetPhase(ECityPhase::Gate); }
    if (auto* P = Player()) PreviousPosition = P->GetActorLocation();
}
void ACityCampaignMode::OpenRoomDoors()
{
    Offers.Reset();
    TArray<int32> Layouts,Enc{0,1,2},Rewards{0,1,2},Arms{0,1,2};
    for(int I=0;I<10;++I) if(I!=Current.Layout) Layouts.Add(I);
    Shuffle(Layouts,Rng); Shuffle(Enc,Rng); Shuffle(Rewards,Rng); Shuffle(Arms,Rng);
    for(int I=0;I<3;++I)
    {
        auto& O=Offers.AddDefaulted_GetRef();
        O.Upper=Shell->IsUpperGate(I); O.Encounter=Enc[I]; O.Reward=Rewards[I];
        O.Layout=Layouts[I]; O.Seed=Rng.RandRange(1,100000000);
        TArray<int32> Passives{0,1,2,3,4,5}; Shuffle(Passives,Rng);
        for(int J=0;J<3;++J) O.Abilities.Add(Abilities[Passives[J]]);
        if(Depth==3) { O.Weapon=Weapons[Arms[I]]; O.Boss=BossFor(Route); O.Layout=Arenas[O.Boss]; O.Reward=0; }
        const FString Title = FString(O.Upper ? TEXT("↑ 上行   ") : TEXT("↓ 下行   ")) + LayoutNames[O.Layout];
        const FString Fight = O.Boss == INDEX_NONE ? Encounters[O.Encounter] : BossNames[O.Boss];
        FString Reward;
        if (O.Reward == 0)
            Reward = TEXT("奖励   ") + Progress()->PreviewReward(O.Seed, O.Weapon, true);
        else if (O.Reward == 1)
            Reward = TEXT("奖励   能力三选一");
        else
            Reward = TEXT("奖励   生命 +30");
        const FString Detail = O.Boss == INDEX_NONE
            ? TEXT("可达首领   ") + Forecast(Route + (O.Upper ? TEXT("U") : TEXT("D")))
            : TEXT("终局武器奖励");
        Shell->RevealGate(I, Title, Fight, Reward, Detail);
        UE_LOG(LogTemp,Display,TEXT("City door offer depth=%d slot=%d upper=%d seed=%d layout=%d encounter=%d reward=%d"),Depth,I,O.Upper,O.Seed,O.Layout,O.Encounter,O.Reward);
    }
}
void ACityCampaignMode::Commit(int32 Slot)
{
    if(Phase!=ECityPhase::Gate || !Offers.IsValidIndex(Slot)) return;
#if !UE_BUILD_SHIPPING
    if (!TestRoute.IsEmpty() && Slot != BotDoor) { EndBot(false,TEXT("crossed a different offer")); return; }
#endif
    const FCityRoomOffer Room=Offers[Slot];
    if(Depth<3) Route+=Room.Upper?TEXT("U"):TEXT("D");
    UE_LOG(LogTemp,Display,TEXT("City gate crossed depth=%d slot=%d direction=%s seed=%d"),Depth,Slot,Room.Upper?TEXT("U"):TEXT("D"),Room.Seed);
    ++Depth; Enter(Room);
}
void ACityCampaignMode::Finish(bool Won)
{
    bWon=Won; bPendingFinish=true;
    if(!Progress()->FinishRun(Won))
    {
        Notice=TEXT("结算未写入存档");
        SetPhase(ECityPhase::Retry);
        return;
    }
    bPendingFinish=false;
    UE_LOG(LogTemp,Display,TEXT("City settlement victory=%d depth=%d route=%s"),Won,Depth,*Route);
    SetPhase(ECityPhase::Result);
}
FString ACityCampaignMode::StatusText() const
{
    FString Lead=FString::Printf(TEXT("%02d / 05    %s   ·   %s"),Depth+1,LayoutNames[Current.Layout],
        Current.Boss==INDEX_NONE?Encounters[Current.Encounter]:BossNames[Current.Boss]);
    if(Phase==ECityPhase::Combat)
    {
        if(auto* P=Player()) if(auto* Skills=P->FindComponentByClass<UCityAbilities>())
        {
            for(int Slot=0;Slot<2;++Slot)
            {
                const FName Id=Progress()->GetActiveSkill(Slot);
                if(const auto* Def=FCityAbilityCatalog::Find(Id))
                {
                    const float Wait=Skills->GetCooldown(Id);
                    Lead+=FString::Printf(TEXT("%s  %s  %s"),Slot==0?TEXT("\nC"):TEXT("    X"),*Def->Label,
                        Wait>0?*FString::Printf(TEXT("%.1fs"),Wait):TEXT("就绪"));
                }
            }
            if (Progress()->GetWeapon(P->GetCitySlot()).Kind == TEXT("sword"))
                Lead += TEXT("    左键连斩 · 按住循环");
            if(const auto* Def=FCityAbilityCatalog::Find(Progress()->GetUltimate()))
                Lead+=FString::Printf(TEXT("    G  %s  %.0f%%"),*Def->Label,Skills->GetEnergy());
        }
        if(NextWave>=0) return Lead+TEXT("\n增援将至 · 离开入口两侧的标记区域");
        if(Current.Boss!=INDEX_NONE)
            for(ACityCampaignEnemy* E : Enemies) if(IsValid(E) && E->IsBoss())
                return Lead+FString::Printf(TEXT("\n首领  %.0f / %.0f   %s"),E->CurrentHP,E->MaxHealth(),*E->Status());
        return Lead+FString::Printf(TEXT("\n第 %d 波   ·   剩余 %d"),Wave+1,Enemies.Num());
    }
    if(Phase==ECityPhase::Gate) return Lead+TEXT("\n查看门上战况与奖励，穿过一扇门继续");
    if(Phase==ECityPhase::Extract) return Lead+TEXT("\n撤离已开放 · 登上斜坡，穿过青色门");
    return Lead;
}
FString ACityCampaignMode::PanelTitle() const
{
    if(bUltimateMenu) return TEXT("大招选择  ·  G 施放，H / Esc 继续");
    if(bUserPaused) return TEXT("暂停");
    switch(Phase)
    {
        case ECityPhase::Ability: return TEXT("清房奖励 · 选择一项能力");
        case ECityPhase::Retry: return Notice;
        case ECityPhase::Result: return bWon?TEXT("已成功撤离"):TEXT("行动失败");
        default: return FString();
    }
}
TArray<FString> ACityCampaignMode::Actions() const
{
    TArray<FString> Rows;
    if(bUltimateMenu)
    {
        TArray<FString> Choices;
        for (const auto& D:FCityAbilityCatalog::All()) if (D.bUltimate)
            Choices.Add((Progress()->GetUltimate()==D.Id?FString(TEXT("✓  ")):FString())+D.Label+TEXT("   ")+D.Description);
        Choices.Add(TEXT("保留当前大招并继续"));
        return Choices;
    }
    if(bUserPaused) return {TEXT("继续游戏"),Phase==ECityPhase::Preparation || Phase==ECityPhase::Result ? TEXT("退出到桌面") : TEXT("结束本次行动并退出到桌面")};
    if(Phase==ECityPhase::Ability) for(FName Id : Current.Abilities) Rows.Add(AbilityName(Id));
    else if(Phase==ECityPhase::Retry) Rows.Add(TEXT("重试保存"));
    else if(Phase==ECityPhase::Result) Rows.Add(TEXT("返回准备区"));
    return Rows;
}
void ACityCampaignMode::Select(int32 Index)
{
    if (bUltimateMenu)
    {
        if (Index<0) return;
        int32 Row=0;
        for (const auto& D:FCityAbilityCatalog::All()) if (D.bUltimate)
        {
            if (Row++==Index)
            {
                if (Player()->GetCityAbilities()->SelectSessionUltimate(D.Id)) ToggleUltimateMenu();
                return;
            }
        }
        if (Index==Row) ToggleUltimateMenu();
        return;
    }
    if(bUserPaused)
    {
        if(Index==0) TogglePause();
        else if(Index==1)
        {
            if(Phase!=ECityPhase::Preparation && Phase!=ECityPhase::Result && !Progress()->FinishRun(false))
            {
                TogglePause(); bWon=false; bPendingFinish=true;
                Notice=TEXT("退出前结算未写入存档"); SetPhase(ECityPhase::Retry); return;
            }
            UKismetSystemLibrary::QuitGame(this,PC(),EQuitPreference::Quit,false);
        }
        return;
    }
    if(Phase==ECityPhase::Ability && Current.Abilities.IsValidIndex(Index))
    {
        if(Progress()->ChooseAbility(Ticket(),Current.Abilities[Index])) AfterReward();
    }
    else if(Phase==ECityPhase::Retry && Index==0)
    {
        if(bPendingFinish) Finish(bWon); else Award();
    }
    else if(Phase==ECityPhase::Result && Index==0)
    {
        SetModal(false);
        UGameplayStatics::OpenLevel(this,FName(*UGameplayStatics::GetCurrentLevelName(this,true)));
    }
}
void ACityCampaignMode::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!PC() || bBotFinished) return;
    if(!bStarted && Player())
    {
        bStarted=true;
        Preparation();
        if(!TestRoute.IsEmpty() && !bRecording) Progress()->Depart();
    }
    if(Phase==ECityPhase::Preparation && DepartureRequestedAt>0 && RequestPreparedRoom()) Progress()->Depart();
    if(!TestRoute.IsEmpty() && bStarted && !bUltimateMenu) StepBot(Dt);
    if(HasCampaignOverlay() || Phase==ECityPhase::Weapon || Phase==ECityPhase::Preparation || GetWorld()->IsPaused()) return;
    auto* P=Player();
    if(!P || P->IsDead()) { Finish(false); return; }
    const bool EDown=PC()->IsInputKeyDown(EKeys::E);
    const bool Interact=EDown && !bEWasDown; bEWasDown=EDown;
    if(IsValid(Shell)) Shell->CollectSupplies(P,Interact);
    if(Phase==ECityPhase::Combat)
    {
        Enemies.RemoveAll([](const auto& E){return !IsValid(E.Get()) || E->CurrentHP<=0;});
        if(Enemies.IsEmpty())
        {
            if(Current.Boss==INDEX_NONE && Current.Encounter==1 && Wave==0)
            {
                if(NextWave<0) { NextWave=1.25f; Shell->WarnReinforcements(true); }
                else NextWave=FMath::Max(0.f,NextWave-Dt);
                const FVector At=P->GetActorLocation();
                if(NextWave==0 && FVector::Dist2D(At,FVector(-380,-480,0))>145 && FVector::Dist2D(At,FVector(-390,465,0))>145)
                { ++Wave; NextWave=-1; Shell->WarnReinforcements(false); SpawnWave(); }
            }
            else Cleared();
        }
    }
    else if(Phase==ECityPhase::Gate || Phase==ECityPhase::Extract)
    {
        const int32 Slot = Shell->CrossedGate(PreviousPosition,P->GetActorLocation());
        if (Slot != INDEX_NONE)
        {
            if (Phase == ECityPhase::Extract) Finish(true);
            else Commit(Slot);
            return;
        }
    }
    PreviousPosition=P->GetActorLocation();
}