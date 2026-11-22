#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Campaign/CityCampaignEnemy.h"
#include "CityCampaignMode.generated.h"

class ACityRoomShell;
class AShooterCharacter;
class UCityProgression;
class SCityCampaignPanel;
class IInputProcessor;

enum class ECityPhase : uint8 { Preparation, Combat, Weapon, Ability, Gate=5, Extract, Result, Retry };
struct FCityRoomOffer
{
    int32 Layout=0, Encounter=0, Seed=0, Reward=0, Boss=INDEX_NONE;
    bool Upper=false;
    FName Weapon=NAME_None;
    TArray<FName> Abilities;
};

UCLASS()
class VERTICALCITYFPS_API ACityCampaignMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACityCampaignMode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    FString StatusText() const;
    FString PanelTitle() const;
    TArray<FString> Actions() const;
    void Select(int32 Index);
    void DropAmmo(FVector Position);
    ECityPhase GetPhase() const { return Phase; }
    bool HasCampaignOverlay() const;
    int32 SyncMarker() const;
    bool IsUserPaused() const { return bUserPaused; }
    bool ToggleUltimateMenu();
    bool IsUltimateMenuOpen() const { return bUltimateMenu; }
    void TogglePause();
    bool RequestPreparedRoom();
    void CancelDepartureRequest() { DepartureRequestedAt=0; }
private:
#if !UE_BUILD_SHIPPING
    friend struct FCityAbilityPhaseFixture;
#endif
    void Preparation();
    void StartRun();
    void Enter(const FCityRoomOffer& Room, bool ReuseFirstRoom=false);
    void SpawnWave();
    void Spawn(ECityFoe Kind,FVector Position);
    void Cleared();
    void Award();
    void AfterReward();
    void OpenRoomDoors();
    void Commit(int32 Slot);
    void Finish(bool Won);
    void SetPhase(ECityPhase Next);
    void SetModal(bool Enable);
    void StepBot(float DeltaSeconds);
    void EndBot(bool Success,const TCHAR* Reason);
    void WalkTo(AShooterCharacter* Pawn, FVector Point);
    void CleanupRoom();
    FString Ticket() const;
    UCityProgression* Progress() const;
    AShooterCharacter* Player() const;
    APlayerController* PC() const;
    ECityPhase Phase=ECityPhase::Preparation;
    FCityRoomOffer Current;
    TArray<FCityRoomOffer> Offers;
    UPROPERTY() TObjectPtr<ACityRoomShell> Shell;
    UPROPERTY() TArray<TObjectPtr<ACityCampaignEnemy>> Enemies;
    TSharedPtr<SCityCampaignPanel> Panel;
    TSharedPtr<IInputProcessor> MenuInput;
    FRandomStream Rng;
    double RoomPreparedAt=0, DepartureRequestedAt=0;
    uint64 PreparedFrame=0;
    bool bBuildShowcase=false;
    int32 ShowcaseCasts=0;
    FString Route,RunID,TestRoute,Notice;
    int32 Depth=0,Wave=0,BotStage=0,BotDoor=INDEX_NONE;
    bool bUltimateMenu=false;
    bool bModal=false,bWon=false,bFast=false,bBotFinished=false,bStarted=false,bEWasDown=false,bFireHeld=false,bPendingFinish=false;
    FVector PreviousPosition = FVector::ZeroVector;
    float NextWave=-1,BotTime=0,BotClock=0,BotStateTime=0;
    bool bRecording=false,bUserPaused=false,bUserAcquiredPause=false;
    double PhaseStarted=0,AudioStarted=0,AudioStopped=0;
    FString AudioFolder;
#if !UE_BUILD_SHIPPING
    FString SwordCaptureFolder;
    FString EnemyCueFolder;
    int32 CueCaptureTick=0;
    void CaptureEnemyCue();
    int32 SwordCaptureTick = 0;
    TWeakObjectPtr<ACityCampaignEnemy> SwordCaptureTarget;
    bool CaptureSword();
    bool CaptureTripleCut();
    void CaptureMovingSword();
    int32 MovingCutStart=INDEX_NONE;
    bool bMovingDashSeen=false;
    float ComboTargetHP = 0.f;
    int32 ComboHitsObserved = 0;
#endif
};