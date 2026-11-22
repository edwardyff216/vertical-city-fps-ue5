#include "Progression/CityAffixLabMode.h"
#include "ShooterCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Slate/DeferredCleanupSlateBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSpacer.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

namespace
{
const FLinearColor LabAccent(1.f,.84f,.52f);
const FLinearColor PrintInk(.035f,.022f,.015f);
TSharedRef<SWidget> LabSheet(const TCHAR* Asset,TSharedRef<SWidget> Content,float Headroom)
{
    auto* Texture=LoadObject<UTexture2D>(nullptr,Asset);
    auto Base=SNew(SOverlay);
    if(Texture)
    {
        auto Brush=FDeferredCleanupSlateBrush::CreateBrush(Texture);
        Base->AddSlot()[SNew(SImage).Image_Lambda([Brush](){return Brush->GetSlateBrush();})];
    }
    Base->AddSlot()[SNew(SVerticalBox)
        +SVerticalBox::Slot().FillHeight(Headroom)[SNew(SSpacer)]
        +SVerticalBox::Slot().FillHeight(1.f-Headroom).Padding(FMargin(30,0,30,24))[Content]];
    return Base;
}
TSharedRef<STextBlock> RangeLabel(const FString& S,int Size=15,FLinearColor Color=FLinearColor::White)
{
    return SNew(STextBlock).Text(FText::FromString(S)).AutoWrapText(S.Len()>30)
        .Font(FCoreStyle::GetDefaultFontStyle(Size>=18?TEXT("Bold"):TEXT("Regular"),Size)).ColorAndOpacity(Color);
}
TSharedRef<SButton> RangeButton(const FString& S,TFunction<void()> Fn,bool Enabled=true)
{
    static const FButtonStyle Print=FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(.056f,.031f,.021f)))
        .SetHovered(FSlateColorBrush(FLinearColor(.24f,.073f,.031f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.43f,.037f,.024f)))
        .SetDisabled(FSlateColorBrush(FLinearColor(.26f,.17f,.095f)));
    return SNew(SButton).ButtonStyle(&Print).IsEnabled(Enabled).ContentPadding(FMargin(9,5))
        .OnClicked_Lambda([Fn=MoveTemp(Fn)](){Fn(); return FReply::Handled();})[RangeLabel(S,16)];
}
}
void ACityAffixLabMode::SetWeapon(FName Kind)
{
    auto& W=Draft.Equipped[Slot];
    if(!FCityItemCatalog::Kind(Kind)) return;
    W.Kind=Kind;
    const auto Pool=FCityItemCatalog::Pool(Kind,5);
    W.Affixes.RemoveAll([&Pool](FName Id){return !Pool.Contains(Id);});
}
void ACityAffixLabMode::ToggleAffix(FName Id)
{
    auto& W=Draft.Equipped[Slot];
    if(!W.Affixes.Remove(Id) && W.Affixes.Num()<4 && FCityItemCatalog::Pool(W.Kind,5).Contains(Id)) W.Affixes.Add(Id);
}
void ACityAffixLabMode::PickPreset(int32 Index)
{
    if(Index<0 || Index>2) return;
    auto& Rifle=Draft.Equipped[0]; auto& Sword=Draft.Equipped[1];
    Rifle.Kind=TEXT("rifle"); Sword.Kind=TEXT("sword");
    Rifle.Affixes.Reset(); Sword.Affixes.Reset();
    Draft.Skills[0]=TEXT("echo_pin"); Draft.Skills[1]=TEXT("cinch_post"); Draft.Ultimate=TEXT("barrage");
    Slot=Index==0?1:0;
    if(Index==0) Sword.Affixes={TEXT("quick_blade"),TEXT("reach"),TEXT("parry_range"),TEXT("frost_counter")};
    if(Index==1) Rifle.Affixes={TEXT("rapid"),TEXT("arcane_quill"),TEXT("arcane_triad"),TEXT("arcane_rupture")};
    if(Index==2)
    {
        Rifle.Affixes={TEXT("frost_tip"),TEXT("volt_conduct"),TEXT("volt_link"),TEXT("frost_break")};
        Sword.Affixes={TEXT("frost_counter"),TEXT("volt_conduct")};
        Draft.Skills[0]=TEXT("frost_floor"); Draft.Skills[1]=TEXT("arc_lance");
    }
    School=NAME_None; Search.Reset();
}
void ACityAffixLabMode::RenderEditor()
{
    if(!PC() || !GetWorld()->GetGameViewport()) return;
    const float PriorScroll=Choices?Choices->GetScrollOffset():0.f;
    RemovePanel(); bEditor=true;
    if(Player()) Player()->CancelCityInput();
    PC()->FlushPressedKeys();
    UGameplayStatics::SetGamePaused(this,true); PC()->bShowMouseCursor=true; PC()->SetInputMode(FInputModeUIOnly());
    auto Build=SNew(SVerticalBox);
    auto Library=SNew(SVerticalBox);
    auto AddLabel=[](const TSharedRef<SVerticalBox>& Box,const FString& S)
    { Box->AddSlot().AutoHeight().Padding(0,9,0,3)[RangeLabel(S,20,LabAccent)]; };
    AddLabel(Build,TEXT("配装预设"));
    auto Presets=SNew(SWrapBox).UseAllottedSize(true);
    const TCHAR* Names[]={TEXT("武士刀 · 近战防反"),TEXT("步枪 · 法印连击"),TEXT("冰控 · 电弧 · 飞刃齐射")};
    for(int I=0;I<3;++I) Presets->AddSlot().Padding(0,0,4,4)[RangeButton(Names[I],[this,I](){PickPreset(I);RenderEditor();})];
    Build->AddSlot().AutoHeight()[Presets];
    AddLabel(Build,TEXT("测试武器"));
    auto Hands=SNew(SWrapBox).UseAllottedSize(true);
    for(int I=0;I<2;++I)
    {
        const auto* Kind=FCityItemCatalog::Kind(Draft.Equipped[I].Kind);
        const FString Caption=(I?FString(TEXT("副手  ")):FString(TEXT("主手  ")))+Kind->Label;
        Hands->AddSlot().Padding(0,0,4,4)[RangeButton(Caption,[this,I](){Slot=I;School=NAME_None;RenderEditor();},I!=Slot)];
    }
    Build->AddSlot().AutoHeight()[Hands];
    auto Weapons=SNew(SWrapBox).UseAllottedSize(true);
    for(const auto& K:FCityItemCatalog::Kinds())
        Weapons->AddSlot().Padding(0,0,4,4)[RangeButton(K.Label,[this,Id=K.Id](){SetWeapon(Id);RenderEditor();},Draft.Equipped[Slot].Kind!=K.Id)];
    Build->AddSlot().AutoHeight()[Weapons];
    const auto& W=Draft.Equipped[Slot];
    const auto Stats=FCityItemCatalog::Stats(W);
    Build->AddSlot().AutoHeight().Padding(0,4)[RangeLabel(FString::Printf(TEXT("Lv.1   T5    伤害 %.1f × %d    间隔 %.2f 秒"),Stats.Damage,Stats.Pellets,Stats.Interval),14)];
    AddLabel(Build,FString::Printf(TEXT("已装词条  %d / 4"),W.Affixes.Num()));
    for(int I=0;I<4;++I)
    {
        if(!W.Affixes.IsValidIndex(I)) { Build->AddSlot().AutoHeight().Padding(0,3)[RangeLabel(TEXT("—  空槽"),13)]; continue; }
        const auto* A=FCityItemCatalog::Affix(W.Affixes[I]);
        Build->AddSlot().AutoHeight().Padding(0,1)[RangeButton(FString::Printf(TEXT("%d    %s    × 卸下"),I+1,*A->Label),[this,Id=A->Id](){ToggleAffix(Id);RenderEditor();})];
    }
    AddLabel(Build,TEXT("训练设置"));
    Build->AddSlot().AutoHeight()[RangeButton(bLive?TEXT("活动目标 · 切换为固定靶"):TEXT("固定靶 · 切换为活动目标"),[this](){bLive=!bLive;RenderEditor();})];
    FString Skills;
    for(FName Id:{Draft.Skills[0],Draft.Skills[1],Draft.Ultimate})
        for(const auto& C:(Id==Draft.Ultimate?FCityItemCatalog::Ultimates():FCityItemCatalog::Skills())) if(C.Id==Id) Skills+=C.Label+TEXT("  ");
    Build->AddSlot().AutoHeight().Padding(0,8)[RangeLabel(Skills,13)];
    Library->AddSlot().AutoHeight().Padding(0,0,0,6)[RangeLabel(TEXT("词条库"),24,PrintInk)];
    Library->AddSlot().AutoHeight().Padding(0,2,0,5)[SNew(SEditableTextBox).Text(FText::FromString(Search))
        .HintText(FText::FromString(TEXT("搜索名称、触发条件或效果"))).OnTextChanged_Lambda([this](const FText& T){Search=T.ToString();RefreshChoices();})];
    auto Groups=SNew(SWrapBox).UseAllottedSize(true);
    TArray<FName> Categories{NAME_None};
    for(FName Id:FCityItemCatalog::Pool(W.Kind,5)) Categories.AddUnique(FName(*FCityItemCatalog::Affix(Id)->Category));
    if(!Categories.Contains(School)) School=NAME_None;
    for(FName C:Categories)
        Groups->AddSlot().Padding(0,0,4,3)[RangeButton(C.IsNone()?TEXT("全部"):C.ToString(),[this,C](){School=C;RenderEditor();},C!=School)];
    Library->AddSlot().AutoHeight()[Groups];
    Library->AddSlot().FillHeight(1)[SAssignNew(Choices,SScrollBox)];
    auto Contents=SNew(SVerticalBox);
    Contents->AddSlot().FillHeight(1)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(.495f)[LabSheet(TEXT("/Game/VerticalCity/Lab/T_LabPoster.T_LabPoster"),SNew(SScrollBox)+SScrollBox::Slot()[Build],.225f)]
        +SHorizontalBox::Slot().FillWidth(.505f)[LabSheet(TEXT("/Game/VerticalCity/Lab/T_LabPlacard.T_LabPlacard"),Library,.102f)]];
    auto Footer=SNew(SWrapBox).UseAllottedSize(true);
    Footer->AddSlot().Padding(0,0,8,0)[RangeButton(TEXT("应用配装并试射"),[this](){StartTrial(true);})];
    if(bTrial) Footer->AddSlot().Padding(0,0,8,0)[RangeButton(TEXT("取消编辑，继续试射"),[this](){ToggleEditor();})];
    Footer->AddSlot()[RangeButton(TEXT("返回战备室"),[this](){ReturnToPreparation();})];
    Contents->AddSlot().AutoHeight().Padding(30,10)[Footer];
    Panel=SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.025f,.019f,.014f,1))[Contents];
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(Panel.ToSharedRef(),820);
    RefreshChoices(); Choices->SetScrollOffset(PriorScroll);
}
void ACityAffixLabMode::RefreshChoices()
{
    if(!Choices) return;
    Choices->ClearChildren(); Choices->ScrollToStart();
    const auto& W=Draft.Equipped[Slot];
    const FString Term=Search.TrimStartAndEnd();
    int32 Matches=0;
    for(FName Id:FCityItemCatalog::Pool(W.Kind,5))
    {
        const auto* A=FCityItemCatalog::Affix(Id);
        if(!School.IsNone() && FName(*A->Category)!=School) continue;
        if(!Term.IsEmpty() && !(A->Label+A->Category+A->Description+Id.ToString()).Contains(Term)) continue;
        const bool Selected=W.Affixes.Contains(Id); ++Matches;
        const FString Caption=FString::Printf(TEXT("%s   T%d   %s   ·   %s\n%s"),Selected?TEXT("✓"):TEXT("+"),A->Tier,*A->Label,*A->Category,*A->Description);
        Choices->AddSlot().Padding(0,2)[RangeButton(Caption,[this,Id](){ToggleAffix(Id);RenderEditor();},Selected || W.Affixes.Num()<4)];
    }
    if(!Matches) Choices->AddSlot().Padding(0,12)[RangeLabel(TEXT("没有匹配的词条"))];
}
