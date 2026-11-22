#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Progression/CityProgression.h"
#include "CityAffixLabMode.generated.h"

class ACityCampaignEnemy;
class AShooterCharacter;
class IInputProcessor;
class SScrollBox;

UCLASS()
class VERTICALCITYFPS_API ACityAffixLabMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACityAffixLabMode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    bool HandleLabKey(FKey Key);
private:
    friend class FCityLabWorkflow;
    void BuildRange();
    void RenderEditor();
    void RefreshChoices();
    void ToggleAffix(FName Id);
    void SetWeapon(FName Kind);
    void PickPreset(int32 Index);
    void ToggleEditor();
    void StartTrial(bool ApplyDraft);
    void ReturnToPreparation();
    void RemovePanel();
    void ShowTrialHUD();
    FString TrialStatus() const;
    APlayerController* PC() const;
    AShooterCharacter* Player() const;
    UCityProgression* Progress() const;
    FCityProfile Draft, Active;
    int32 Slot=0, ActiveSlot=0;
    bool bInitialized=false, bEditor=true, bTrial=false, bLive=false, bActiveLive=false;
    FString Search;
    FName School;
    TSharedPtr<SWidget> Panel;
    TSharedPtr<SScrollBox> Choices;
    TSharedPtr<IInputProcessor> Input;
    UPROPERTY() TArray<TObjectPtr<ACityCampaignEnemy>> Targets;
    float TotalTargetHP=0;
};
