#include "Progression/CityProgression.h"
#include "Engine/GameViewportClient.h"
#include "Campaign/CityCampaignMode.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "HAL/FileManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

namespace
{
TSharedRef<STextBlock> Label(const FString& Text,int32 Size=15,FLinearColor Color=FLinearColor::White)
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(FCoreStyle::GetDefaultFontStyle(Size>=19?"Bold":"Regular", Size)).ColorAndOpacity(Color).AutoWrapText(true);
}
const FLinearColor Accent(.42f,.91f,.81f);
const FLinearColor Muted(.64f,.72f,.76f);
FString TitleFor(FName Id,const TArray<FCityChoice>& Choices)
{
    const FCityChoice* C=Choices.FindByPredicate([Id](const FCityChoice& X){return X.Id==Id;});
    return C?C->Label:Id.ToString();
}
}

void UCityProgression::Deinitialize()
{
    ClosePanel();
    SlotLock.Reset();
    Super::Deinitialize();
}

void UCityProgression::AttachPanel(APlayerController* PC)
{
    ClosePanel();
    OwnerPC=PC;
    if (!PC) return;
    PC->FlushPressedKeys();
    bAcquiredPause=!UGameplayStatics::IsGamePaused(PC) && UGameplayStatics::SetGamePaused(PC,true);
    PC->bShowMouseCursor=true;
    PC->SetInputMode(FInputModeUIOnly());
    RenderPanel();
}

void UCityProgression::ClosePanel()
{
    if (APlayerController* PC=OwnerPC.Get())
    {
        if (PC->GetWorld() && PC->GetWorld()->GetGameViewport() && Panel.IsValid())
            PC->GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Panel.ToSharedRef());
        if (bAcquiredPause) UGameplayStatics::SetGamePaused(PC,false);
        PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor=false; PC->FlushPressedKeys();
    }
    Panel.Reset(); OwnerPC.Reset(); bAcquiredPause=false;
}

void UCityProgression::ShowPreparation(APlayerController* PC, FSimpleDelegate OnReady)
{
    if (!PC) return;
    OpenDefault(); Tab=TEXT("gear"); Selected.Reset(); SalvageSelection.Reset(); Confirmation.Reset(); ActiveTicket.Reset();
    OnContinue=MoveTemp(OnReady); AttachPanel(PC);
}

void UCityProgression::Depart()
{
    if (!ActiveTicket.IsEmpty() || !OwnerPC.IsValid()) return;
    if(auto* Mode=OwnerPC->GetWorld()->GetAuthGameMode<ACityCampaignMode>())
        if(!Mode->RequestPreparedRoom()) { Error=TEXT("正在准备首房…"); RenderPanel(); return; }
    if (!BeginRun())
    {
        if(auto* Mode=OwnerPC->GetWorld()->GetAuthGameMode<ACityCampaignMode>()) Mode->CancelDepartureRequest();
        RenderPanel(); return;
    }
    FSimpleDelegate Next=MoveTemp(OnContinue);
    ClosePanel(); Next.ExecuteIfBound();
}

bool UCityProgression::PresentReward(APlayerController* PC,FString Ticket,int32 Seed,FName Kind,FSimpleDelegate OnDismissed)
{
    if (!PC || !GrantReward(Ticket,Seed,Kind)) return false;
    ActiveTicket=Ticket; Selected.Reset(); Confirmation.Reset();
    OnContinue=MoveTemp(OnDismissed); AttachPanel(PC); return true;
}

void UCityProgression::DismissReward()
{
    if (ActiveTicket.IsEmpty()) return;
    ActiveTicket.Reset();
    FSimpleDelegate Next=MoveTemp(OnContinue);
    ClosePanel(); Next.ExecuteIfBound();
}

void UCityProgression::RenderPanel()
{
    auto* PC=OwnerPC.Get();
    if (!PC || !PC->GetWorld() || !PC->GetWorld()->GetGameViewport()) return;
    auto* View=PC->GetWorld()->GetGameViewport();
    if (Panel) View->RemoveViewportWidgetContent(Panel.ToSharedRef());
    const uint64 Gen=++PanelGeneration;
    const bool Reward=!ActiveTicket.IsEmpty();
    TSharedRef<SVerticalBox> Body=SNew(SVerticalBox);
    TSharedRef<SWrapBox> Tabs=SNew(SWrapBox).UseAllottedSize(true);
    auto button=[this,Gen](const FString& Caption,TFunction<void()> Fn,bool Enabled=true)
    {
        return SNew(SButton).IsEnabled(Enabled).ContentPadding(FMargin(12,6))
        .OnClicked_Lambda([Weak=TWeakObjectPtr<UCityProgression>(this),Gen,Fn=MoveTemp(Fn)]()
        {
            if (Weak.IsValid() && Weak->OwnerPC.IsValid() && Weak->PanelGeneration==Gen) Fn();
            return FReply::Handled();
        })[SNew(STextBlock).Text(FText::FromString(Caption)).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).AutoWrapText(Caption.Len()>30)];
    };
    auto header=[&](const FString& Text){Body->AddSlot().AutoHeight().Padding(0,15,0,6)[Label(Text,19,Accent)];};
    auto info=[&](const FString& Text){Body->AddSlot().AutoHeight().Padding(0,4)[Label(Text)];};
    auto refresh=[this](bool Success)
    {
        if (!Success && Error.IsEmpty()) Error=TEXT("操作条件未满足。");
        RenderPanel();
    };
    auto equipRow=[&](const FCityItem& W)
    {
        const FString Id=W.Id;
        info(FCityItemCatalog::Describe(W));
        TSharedRef<SWrapBox> Actions=SNew(SWrapBox).UseAllottedSize(true);
        for (int32 Slot=0;Slot<2;++Slot)
            Actions->AddSlot().Padding(0,0,6,0)[button(Slot?TEXT("装备到副手"):TEXT("装备到主手"),[this,Id,Slot,refresh](){refresh(Equip(Id,Slot));},State.Equipped[Slot].Id!=Id)];
        Body->AddSlot().AutoHeight()[Actions];
    };
    const auto importOld=[this,refresh]()
    {
        refresh(ImportLegacy(LegacySource));
    };
    if (LegacySource.IsEmpty()) LegacySource=FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"))/TEXT("VerticalCityFPS-Gameplay02/profile.json");
    auto archive=[&]()
    {
        const bool Imported=File.EndsWith(TEXT(".imported.json"));
        if (Imported || IFileManager::Get().FileExists(*(File+TEXT(".imported.json"))))
        {
            Body->AddSlot().AutoHeight()[button(Imported?TEXT("返回 UE 原档"):TEXT("切换到 Godot 导入槽"),[this,refresh](){refresh(SwitchArchive());})];
        }
        else
        {
            info(TEXT("旧档路径"));
            Body->AddSlot().AutoHeight()[SNew(SEditableTextBox).Text(FText::FromString(LegacySource)).OnTextChanged_Lambda([this](const FText& Value){LegacySource=Value.ToString();})];
            Body->AddSlot().AutoHeight()[button(TEXT("导入 Godot 旧档到独立槽"),importOld)];
        }
    };
    if (!Error.IsEmpty()) info(Error);
    if (!bLoaded)
    {
        Body->AddSlot().AutoHeight()[button(TEXT("重新读取"),[this](){OpenDefault();RenderPanel();})];
        archive();
    }
    else if (Reward)
    {
        header(TEXT("当前装备"));
        info(TEXT("主手    ")+FCityItemCatalog::Describe(State.Equipped[0]));
        info(TEXT("副手    ")+FCityItemCatalog::Describe(State.Equipped[1]));
        header(TEXT("本房武器 · 已收入背包"));
        for (const FString& Id:State.Rewards.FindChecked(ActiveTicket))
            if (const FCityItem* W=FindItem(Id)) equipRow(*W);
    }
    else
    {
        auto page=[this,&Tabs,&button](const FString& Code,const FString& Name)
        {
            Tabs->AddSlot().Padding(0,0,7,0)[button(Name,[this,Code](){Tab=Code;Confirmation.Reset();Selected.Reset();SalvageSelection.Reset();RenderPanel();},Tab!=Code)];
        };
        page(TEXT("gear"),TEXT("装备")); page(TEXT("work"),TEXT("工坊")); page(TEXT("tree"),TEXT("技能树")); page(TEXT("skills"),TEXT("作战技能"));
        Tabs->AddSlot().Padding(0,0,7,0)[button(TEXT("词条测试房"),[this](){EnterAffixLab();})];
        if (Tab==TEXT("gear"))
        {
            header(TEXT("出战角色"));
            auto Characters=SNew(SWrapBox).UseAllottedSize(true);
            for(FName Id:{FName(TEXT("s07")),FName(TEXT("white_01"))})
            {
                const bool Current=State.Character==Id;
                FString Name=Id==TEXT("s07")?TEXT("S-07"):TEXT("白角色");
                if(Current) Name+=TEXT("  ✓");
                Characters->AddSlot().Padding(0,0,6,0)[button(Name,[this,Id,refresh](){refresh(SetCharacter(Id));},!Current)];
            }
            Body->AddSlot().AutoHeight()[Characters];
            if (State.Result==TEXT("interrupted")) info(TEXT("上次出战中断，随身武器遗失。背包及零件已保留。"));
            header(TEXT("主手")); info(FCityItemCatalog::Describe(State.Equipped[0]));
            header(TEXT("副手")); info(FCityItemCatalog::Describe(State.Equipped[1]));
            if (!State.Equipped[0].Id.IsEmpty() && !State.Equipped[1].Id.IsEmpty())
                Body->AddSlot().AutoHeight()[button(TEXT("互换主副手"),[this,refresh](){refresh(Equip(State.Equipped[1].Id,0));})];
            const int32 Needed=int32(State.Equipped[0].Id.IsEmpty())+int32(State.Equipped[1].Id.IsEmpty())-State.Bag.Num();
            if (Needed>0) Body->AddSlot().AutoHeight()[button(TEXT("领取基础配发武器"),[this,refresh](){refresh(ClaimBasicWeapons());})];
            header(FString::Printf(TEXT("背包 · %d 件"),State.Bag.Num()));
            for (const FCityItem& W:State.Bag) equipRow(W);
            header(TEXT("档案"));
            if (File.EndsWith(TEXT(".imported.json"))) info(TEXT("当前使用独立的 Godot 导入槽"));
            archive();
        }
        else if (Tab==TEXT("tree"))
        {
            header(FString::Printf(TEXT("%s · 可用技能点 %d / %d"),State.Page?TEXT("枪械配置"):TEXT("生存配置"),State.Points-State.Nodes[State.Page].Num(),State.Points));
            Body->AddSlot().AutoHeight()[button(State.Page?TEXT("免费切换到生存配置"):TEXT("免费切换到枪械配置"),[this,refresh](){Confirmation.Reset();refresh(SwitchPage(1-State.Page));})];
            for (const auto& Node:FCityItemCatalog::Nodes())
            {
                const FName Id=Node.Id,Req=FCityItemCatalog::Prerequisite(Id);
                const bool Unlocked=HasNode(Id);
                FString Name=Node.Label+TEXT("  ·  ")+Node.Description;
                if (Unlocked) Name+=TEXT("（已解锁）");
                else if (!Req.IsNone()) Name+=TEXT("   前置：")+TitleFor(Req,FCityItemCatalog::Nodes());
                Body->AddSlot().AutoHeight().Padding(0,4)[button(Name,[this,Id,refresh](){refresh(Unlock(Id));},!Unlocked && State.Points>State.Nodes[State.Page].Num() && (Req.IsNone() || HasNode(Req)))];
            }
            int64 Cost=2+2*State.Respecs;
            Body->AddSlot().AutoHeight()[button(FString::Printf(TEXT("%s重置当前配置 · %lld 零件"),Confirmation==TEXT("respec")?TEXT("确认"):TEXT(""),Cost),[this,refresh]()
            {
                if (Confirmation!=TEXT("respec")){Confirmation=TEXT("respec");RenderPanel();return;}
                if (Respec()) Confirmation.Reset(); refresh(Confirmation.IsEmpty());
            },State.Parts>=Cost && !State.Nodes[State.Page].IsEmpty())];
        }
        else if (Tab==TEXT("skills"))
        {
            for (int32 Slot=0;Slot<3;++Slot)
            {
                const bool Ult=Slot==2;
                const auto& Choices=Ult?FCityItemCatalog::Ultimates():FCityItemCatalog::Skills();
                const FName Current=Ult?GetUltimate():GetActiveSkill(Slot);
                FString Key=Slot==0?TEXT("C"):Slot==1?TEXT("X"):TEXT("G");
                Body->AddSlot().AutoHeight().Padding(0,5)[button(Key+TEXT(" · ")+TitleFor(Current,Choices),[this,Key](){Selected=Selected==Key?FString():Key; RenderPanel();})];
                if (Selected!=Key) continue;
                for (const auto& Choice:Choices)
                {
                    const FName Id=Choice.Id;
                    Body->AddSlot().AutoHeight().Padding(15,2,0,2)[button(Choice.Label+TEXT("   ")+Choice.Description,[this,Id,Slot,refresh]()
                    {refresh(Slot==2?SetUltimate(Id):SetActiveSkill(Slot,Id));},Id!=Current && (Ult || Id!=GetActiveSkill(1-Slot)))];
                }
            }
        }
        else
        {
            header(TEXT("制造 · 每件 5 零件"));
            TSharedRef<SWrapBox> Make=SNew(SWrapBox).UseAllottedSize(true);
            for (const auto& K:FCityItemCatalog::Kinds())
            {
                const FName Kind=K.Id;
                Make->AddSlot().Padding(2)[button(K.Label,[this,Kind,refresh]()
                {
                    if (Manufacture(Kind)) {Tab=TEXT("gear");Selected=State.Bag.Last().Id;refresh(true);}
                    else refresh(false);
                },State.Parts>=5)];
            }
            Body->AddSlot().AutoHeight()[Make];
            header(TEXT("改造 · 替换一条词条"));
            auto forge=[&](const FCityItem& W)
            {
                if (W.Affixes.IsEmpty()) return;
                info(FCityItemCatalog::Kind(W.Kind)->Label+TEXT("   ")+W.Id.Right(6));
                for (int32 I=0; I<W.Affixes.Num(); ++I)
                {
                    const FString Id=W.Id; const FName A=W.Affixes[I];
                    Body->AddSlot().AutoHeight().Padding(8,2)[button(FString::Printf(TEXT("重掷 %s · %d 零件"),*FCityItemCatalog::Affix(A)->Label,W.Rarity),[this,Id,I,A,refresh](){refresh(Reroll(Id,I,A));},State.Parts>=W.Rarity)];
                }
            };
            for (const auto& W:State.Equipped) forge(W);
            for (const auto& W:State.Bag) forge(W);
            header(TEXT("拆解背包武器"));
            int32 Quote=0;
            for (const auto& W:State.Bag)
            {
                const FString Id=W.Id;
                const bool Chosen=SalvageSelection.Contains(Id);
                if (Chosen) Quote+=W.Rarity;
                Body->AddSlot().AutoHeight().Padding(0,2)[button(FString::Printf(TEXT("%s  %s   T%d   %s   +%d 零件"),Chosen?TEXT("✓"):TEXT("□"),*FCityItemCatalog::Kind(W.Kind)->Label,W.Rarity,*Id.Right(6),W.Rarity),[this,Id]()
                {
                    if (SalvageSelection.Contains(Id)) SalvageSelection.Remove(Id); else SalvageSelection.Add(Id);
                    Confirmation.Reset();RenderPanel();
                })];
            }
            Body->AddSlot().AutoHeight()[button(FString::Printf(TEXT("%s %d 件 · 共 %d 零件"),Confirmation==TEXT("salvage")?TEXT("确认销毁"):TEXT("拆解所选"),SalvageSelection.Num(),Quote),[this,refresh]()
            {
                if (Confirmation!=TEXT("salvage")){Confirmation=TEXT("salvage");RenderPanel();return;}
                const bool Ok=Salvage(SalvageSelection.Array());
                if (Ok) {SalvageSelection.Reset();Confirmation.Reset();}
                refresh(Ok);
            },Quote>0)];
        }
    }
    TSharedRef<SVerticalBox> Layout=SNew(SVerticalBox);
    Layout->AddSlot().AutoHeight()[Label(Reward?TEXT("武器奖励"):TEXT("出战准备"),27,Accent)];
    if (bLoaded) Layout->AddSlot().AutoHeight().Padding(0,4)[Label(FString::Printf(TEXT("零件 %lld    技能点 %d"),State.Parts,State.Points),14,Muted)];
    Layout->AddSlot().AutoHeight().Padding(0,5)[Tabs];
    Layout->AddSlot().FillHeight(1).Padding(0,4)[SNew(SScrollBox)+SScrollBox::Slot()[Body]];
    if (bLoaded) Layout->AddSlot().AutoHeight().Padding(0,7)[button(Reward?TEXT("收好奖励，继续"):TEXT("出战"),[this,Reward](){if(Reward)DismissReward();else Depart();},Reward || (!State.Equipped[0].Id.IsEmpty() && !State.Equipped[1].Id.IsEmpty()))];
    Panel=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.025f,.04f,.97f)).Padding(FMargin(34,20))[Layout];
    View->AddViewportWidgetContent(Panel.ToSharedRef(),600);
}


void UCityProgression::EnterAffixLab()
{
    APlayerController* PC=OwnerPC.Get();
    if (!PC || !IsPrep() || !ActiveTicket.IsEmpty()) return;
    ClosePanel();
    OnContinue.Unbind();
    UGameplayStatics::OpenLevel(PC,TEXT("/Game/VerticalCity/Maps/L_Campaign"),true,TEXT("game=/Script/VerticalCityFPS.CityAffixLabMode"));
}
