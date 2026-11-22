#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Progression/CityItemCatalog.h"
#include "CityProgression.generated.h"

class SWidget;
class APlayerController;

struct FCityProfile
{
    FCityItem Equipped[2];
    TArray<FCityItem> Bag;
    int64 Parts = 0, Respecs = 0;
    int32 Points = 2, Page = 0;
    TArray<FName> Nodes[2];
    FName Skills[2] = {TEXT("echo_pin"), TEXT("cinch_post")};
    FName Ultimate = TEXT("barrage"), Character = TEXT("s07"), Result;
    bool bActive = false;
    TMap<FString,TArray<FString>> Rewards;
};

UCLASS()
class VERTICALCITYFPS_API UCityProgression : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    bool OpenProfile(const FString& AbsoluteFile);
    bool OpenDefault();
    bool ImportLegacy(const FString& Source);
    bool SwitchArchive();
    bool BeginRun();
    bool FinishRun(bool bWon);
    bool ChooseAbility(FString Ticket, FName Ability);
    int32 AbilityLevel(FName Ability) const { return LabProfile.IsSet()?0:Abilities.FindRef(Ability); }
    bool HasNode(FName Id) const { return GetProfile().Nodes[GetProfile().Page].Contains(Id); }
    FName GetActiveSkill(int32 Slot) const { return Slot >= 0 && Slot < 2 ? GetProfile().Skills[Slot] : NAME_None; }
    FName GetUltimate() const { return LabProfile.IsSet() || SessionUltimate.IsNone() ? GetProfile().Ultimate : SessionUltimate; }
    bool SelectSessionUltimate(FName Id);
    FCityItem GetWeapon(int32 Slot) const {return Slot>=0 && Slot<2 ? GetProfile().Equipped[Slot] : FCityItem{};}
    FCityWeaponStats GetStats(int32 Slot) const;
    bool SwitchPage(int32 Page);
    bool Unlock(FName Id);
    bool Respec();
    bool SetActiveSkill(int32 Slot, FName Id);
    bool SetUltimate(FName Id);
    bool SetCharacter(FName Id);
    bool Equip(const FString& Id, int32 Slot);
    bool ClaimBasicWeapons();
    bool Salvage(const TArray<FString>& Ids);
    bool Manufacture(FName Kind);
    bool Reroll(const FString& Id, int32 Index, FName Expected);
    FString PreviewReward(int32 Seed, FName Kind, bool bNamesOnly=false) const;
    bool GrantReward(const FString& Ticket, int32 Seed, FName Kind);
    bool PresentReward(APlayerController* PC, FString Ticket, int32 Seed, FName Kind, FSimpleDelegate OnDismissed);
    void DismissReward();
    void ShowPreparation(APlayerController* PC, FSimpleDelegate OnReady);
    void Depart();
    void EnterAffixLab();
    void UseLabProfile(const FCityProfile& Loadout) { LabProfile=Loadout; }
    void LeaveLabProfile() { LabProfile.Reset(); }
    const FCityProfile& GetProfile() const {return LabProfile.IsSet()?LabProfile.GetValue():State;}
    const FString& GetError() const {return Error;}
    bool IsReady() const {return bLoaded;}
    const FCityItem* FindItem(const FString& Id) const;
    FSimpleMulticastDelegate OnEquipmentChanged;
private:
    friend class FCityProcessTest;
    friend class FCityLabWorkflow;
    bool IsPrep() const {return bLoaded && !State.bActive;}
    bool Commit(const FCityProfile& Next);
    void ClosePanel();
    void RenderPanel();
    void AttachPanel(APlayerController* PC);
    static FString DefaultPath();
    FString File, Error, ActiveTicket, Tab, Selected, Confirmation, LegacySource;
    TSet<FString> SalvageSelection;
    FCityProfile State;
    TOptional<FCityProfile> LabProfile;
    FName SessionUltimate;
    bool bLoaded = false;
    TMap<FName,int32> Abilities;
    TSet<FString> AbilityTickets;
    TWeakObjectPtr<APlayerController> OwnerPC;
    TSharedPtr<SWidget> Panel;
    TUniquePtr<FSystemWideCriticalSection> SlotLock;
    FSimpleDelegate OnContinue;
    uint64 PanelGeneration = 0;
    bool bAcquiredPause = false;
};
