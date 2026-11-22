#include "Progression/CityAffixLabMode.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Campaign/CityEnemyBolt.h"
#include "Campaign/CityGeometry.h"
#include "ShooterCharacter.h"
#include "ShooterPlayerController.h"
#include "ShooterProjectile.h"
#include "ShooterWeapon.h"
#include "Abilities/CityAbilities.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr double RangeHeight=4000;
const FVector Entry(-1100,0,RangeHeight+100);
class FCityRangeKeys final : public IInputProcessor
{
public:
    explicit FCityRangeKeys(ACityAffixLabMode* InMode) : Mode(InMode) {}
    virtual void Tick(float,FSlateApplication&,TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&,const FKeyEvent& E) override
    {
        return !E.IsRepeat() && Mode.IsValid() && Mode->HandleLabKey(E.GetKey());
    }
private:
    TWeakObjectPtr<ACityAffixLabMode> Mode;
};
}
ACityAffixLabMode::ACityAffixLabMode()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    static ConstructorHelpers::FClassFinder<AShooterCharacter> PawnClass(TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter"));
    static ConstructorHelpers::FClassFinder<AShooterPlayerController> ControlClass(TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterPlayerController"));
    DefaultPawnClass=PawnClass.Class;
    PlayerControllerClass=ControlClass.Class;
}
APlayerController* ACityAffixLabMode::PC() const { return GetWorld()->GetFirstPlayerController(); }
AShooterCharacter* ACityAffixLabMode::Player() const { return PC()?Cast<AShooterCharacter>(PC()->GetPawn()):nullptr; }
UCityProgression* ACityAffixLabMode::Progress() const { return GetGameInstance()->GetSubsystem<UCityProgression>(); }

void ACityAffixLabMode::BeginPlay()
{
    Super::BeginPlay();
    for(int I=0;I<2;++I)
    {
        auto& Item=Draft.Equipped[I];
        Item.Id=FString::Printf(TEXT("lab_%d"),I);
        Item.Kind=I==0?TEXT("rifle"):TEXT("sword");
        Item.Rarity=5;
    }
    Progress()->UseLabProfile(Draft);
    BuildRange();
    if(FSlateApplication::IsInitialized())
    {
        Input=MakeShared<FCityRangeKeys>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(Input,0);
    }
}
void ACityAffixLabMode::BuildRange()
{
    auto* Arena=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Arena);
    Arena->SetRootComponent(Root);
    Root->RegisterComponent();
    Arena->SetActorLocation(FVector(0,0,RangeHeight));
    auto* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    const FLinearColor Concrete(.12f,.16f,.19f), Wall(.16f,.22f,.25f), Paint(.15f,.68f,.65f);
    auto Block=[&](FVector At,FVector Size,FLinearColor Color,bool Solid=true)
    { CityGeometry::Part(Arena,Cube,At,Size,Color,Solid); };
    Block(FVector(0,0,-30),FVector(3200,3000,60),Concrete);
    Block(FVector(0,0,820),FVector(3200,3000,40),Wall);
    for(int S:{-1,1})
    {
        Block(FVector(S*1630,0,400),FVector(60,3060,800),Wall);
        Block(FVector(0,S*1530,400),FVector(3200,60,800),Wall);
        Block(FVector(0,S*1498,230),FVector(3200,3,13),Paint,false);
    }
    auto Label=[&](FVector At,const FString& Caption,float Size)
    {
        auto* T=NewObject<UTextRenderComponent>(Arena); Arena->AddInstanceComponent(T);
        T->SetupAttachment(Root); T->SetRelativeLocationAndRotation(At,FRotator(0,180,0));
        T->SetText(FText::FromString(Caption)); T->SetWorldSize(Size);
        T->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center); T->RegisterComponent();
    };
    Label(FVector(1596,0,415),TEXT("AFFIX LAB  /  LIVE RANGE"),78);
    for(int Distance=5;Distance<=25;Distance+=5)
    {
        const float X=Entry.X+Distance*100;
        Block(FVector(X,0,1),FVector(6,2650,2),Paint,false);
        for(int S:{-1,1})
        {
            Label(FVector(X,S*1280,64),FString::Printf(TEXT("%d m"),Distance),35);
            Block(FVector(X,S*1280,26),FVector(7,105,52),Wall,false);
        }
    }
    for(int X:{-1050,0,1050}) for(int Y:{-650,650})
    {
        auto* Light=NewObject<UPointLightComponent>(Arena); Arena->AddInstanceComponent(Light);
        Light->SetupAttachment(Root); Light->SetRelativeLocation(FVector(X,Y,545));
        Light->SetIntensity(85000); Light->SetAttenuationRadius(1700); Light->SetCastShadows(false); Light->RegisterComponent();
        Block(FVector(X,Y,795),FVector(220,40,9),FLinearColor(3,3.5f,3.7f),false);
    }
}
void ACityAffixLabMode::RemovePanel()
{
    if(Panel && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Panel.ToSharedRef());
    Panel.Reset(); Choices.Reset();
}
void ACityAffixLabMode::EndPlay(const EEndPlayReason::Type Reason)
{
    RemovePanel();
    if(Input && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Input);
    Input.Reset();
    if(auto* P=Progress()) P->LeaveLabProfile();
    Super::EndPlay(Reason);
}
void ACityAffixLabMode::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bInitialized && Player() && GetWorld()->GetGameViewport())
    {
        bInitialized=true;
        Player()->SetActorLocation(Entry);
        RenderEditor();
    }
    else if(!bEditor && bTrial && (!Player() || Player()->IsDead()))
    {
        bTrial=false;
        RenderEditor();
    }
}
bool ACityAffixLabMode::HandleLabKey(FKey Key)
{
    if(!bInitialized) return false;
    if(Key==EKeys::Escape || (Key==EKeys::Tab && !bEditor))
    { ToggleEditor(); return true; }
    if(Key==EKeys::T && !bEditor)
    { StartTrial(false); return true; }
    return false;
}
void ACityAffixLabMode::ToggleEditor()
{
    if(!bEditor)
    {
        Slot=Player()?Player()->GetCitySlot():ActiveSlot;
        Draft=Active; bLive=bActiveLive; RenderEditor();
    }
    else if(bTrial)
    {
        Draft=Active; bLive=bActiveLive;
        RemovePanel(); bEditor=false;
        UGameplayStatics::SetGamePaused(this,false);
        PC()->SetInputMode(FInputModeGameOnly()); PC()->bShowMouseCursor=false; PC()->FlushPressedKeys();
        ShowTrialHUD();
    }
}
void ACityAffixLabMode::StartTrial(bool ApplyDraft)
{
    if(!PC()) return;
    if(ApplyDraft) { Active=Draft; ActiveSlot=Slot; bActiveLive=bLive; }
    else if(Player()) ActiveSlot=Player()->GetCitySlot();
    RemovePanel();
    UGameplayStatics::SetGamePaused(this,false);
    bEditor=false; bTrial=true; Active.bActive=true;
    Progress()->UseLabProfile(Active);
    for(ACityCampaignEnemy* Target:Targets) if(IsValid(Target)) Target->Destroy();
    Targets.Reset();
    for(TActorIterator<ACityEnemyBolt> I(GetWorld()); I; ++I) I->Destroy();
    for(TActorIterator<AShooterProjectile> I(GetWorld()); I; ++I) I->Destroy();
    if(auto* Old=Player())
    {
        Old->CancelCityInput();
        if(auto* Skills=Old->GetCityAbilities()) Skills->ClearRoomEffects();
        Old->OnDestroyed.RemoveAll(PC());
        for(TActorIterator<AShooterWeapon> I(GetWorld()); I; ++I) if(I->GetOwner()==Old || I->GetInstigator()==Old) I->Destroy();
        PC()->UnPossess(); Old->Destroy();
    }
    RestartPlayerAtTransform(PC(),FTransform(FRotator::ZeroRotator,Entry));
    auto* NewPlayer=Player();
    if(!NewPlayer) { bTrial=false; RenderEditor(); return; }
    NewPlayer->RefreshCityLoadout();
    if(NewPlayer->GetCitySlot()!=ActiveSlot) NewPlayer->DoSwitchWeapon();
    PC()->SetControlRotation(FRotator::ZeroRotator);
    PC()->SetInputMode(FInputModeGameOnly()); PC()->bShowMouseCursor=false; PC()->FlushPressedKeys();
    const FVector At[]={FVector(-300,0,92),FVector(-150,-180,92),FVector(-150,180,92),FVector(1000,550,133)};
    const ECityFoe Types[]={ECityFoe::Melee,ECityFoe::Melee,ECityFoe::Shooter,ECityFoe::Breaker};
    TotalTargetHP=0;
    for(int I=0;I<4;++I)
    {
        auto* E=GetWorld()->SpawnActor<ACityCampaignEnemy>(At[I]+FVector(0,0,RangeHeight),FRotator(0,180,0));
        if(!E) continue;
        E->Configure(Types[I]); Targets.Add(E); TotalTargetHP+=E->MaxHealth();
        if(!bActiveLive) E->Tags.Add(TEXT("CityStationaryTarget"));
    }
    ShowTrialHUD();
}
void ACityAffixLabMode::ShowTrialHUD()
{
    Panel=SNew(SBorder).Visibility(EVisibility::HitTestInvisible).HAlign(HAlign_Center).VAlign(VAlign_Bottom)
        .BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0,0,0,38)
        [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(TrialStatus());})
        .Justification(ETextJustify::Center).ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor::Black)];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Panel.ToSharedRef(),820);
}
FString ACityAffixLabMode::TrialStatus() const
{
    int32 Alive=0; float Left=0;
    for(ACityCampaignEnemy* E:Targets) if(IsValid(E) && E->CurrentHP>0) {++Alive; Left+=E->CurrentHP;}
    return FString::Printf(TEXT("%s    剩余 %d / 4    累计伤害 %.0f\nTab / Esc 调整配装      T 重置本轮"),
        bActiveLive?TEXT("实战靶场"):TEXT("固定靶场"),Alive,TotalTargetHP-Left);
}
void ACityAffixLabMode::ReturnToPreparation()
{
    RemovePanel(); UGameplayStatics::SetGamePaused(this,false);
    UGameplayStatics::OpenLevel(this,TEXT("/Game/VerticalCity/Maps/L_Campaign"),true);
}
