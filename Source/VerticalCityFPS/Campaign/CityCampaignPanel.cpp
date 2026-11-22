#include "CityCampaignPanel.h"
#include "CityCampaignMode.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Styling/CoreStyle.h"

namespace
{
FSlateFontInfo Font(int32 Size,bool Bold=false) { return FCoreStyle::GetDefaultFontStyle(Bold?"Bold":"Regular",Size); }
}
void SCityCampaignPanel::Construct(const FArguments& Args)
{
    Mode=Args._Mode;
    SetVisibility(EVisibility::SelfHitTestInvisible);
    ChildSlot
    [
        SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
        +SOverlay::Slot()
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.015,.022,.03,.88))
            .Visibility_Lambda([this]{return Mode.IsValid() && Mode->IsUserPaused()?EVisibility::Visible:EVisibility::Collapsed;})]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
        [SNew(SBox).WidthOverride(40).HeightOverride(24)
            .Visibility_Lambda([this]{return Mode.IsValid() && Mode->SyncMarker()!=0?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor_Lambda([this]{return FSlateColor(Mode.IsValid() && Mode->SyncMarker()==2 ? FLinearColor::Green : FLinearColor(1,0,1));})]]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(30,26)
        [
            SNew(SBorder).Visibility_Lambda([this] {
                if(!Mode.IsValid() || Mode->IsUserPaused()) return EVisibility::Collapsed;
                const ECityPhase Phase=Mode->GetPhase();
                return Phase==ECityPhase::Combat || Phase==ECityPhase::Gate || Phase==ECityPhase::Extract ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.022,.04,.06,.86))
            .Padding(FMargin(14,7))
            [SNew(STextBlock).Font(Font(14)).ColorAndOpacity(FLinearColor(.86,.95,.96))
                .Text_Lambda([this]{ return Mode.IsValid() ? FText::FromString(Mode->StatusText()) : FText(); })]
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(660)
            .Visibility_Lambda([this]{ return Mode.IsValid() && Mode->HasCampaignOverlay() ? EVisibility::Visible:EVisibility::Collapsed; })
            [
                SNew(SBorder).Padding(27).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.017,.027,.043,.97))
                [
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,13)
                    [SNew(STextBlock).Font(Font(24,true)).AutoWrapText(true).ColorAndOpacity(FLinearColor(.73,.96,.91))
                        .Text_Lambda([this]{return Mode.IsValid()?FText::FromString(Mode->PanelTitle()):FText();})]
                    +SVerticalBox::Slot().AutoHeight()
                    [SNew(SBox).MaxDesiredHeight(480)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Options,SVerticalBox)]]]
                ]
            ]
        ]
    ];
    Refresh();
}
void SCityCampaignPanel::Refresh()
{
    if (!Options || !Mode.IsValid()) return;
    Options->ClearChildren();
    const TArray<FString> Rows=Mode->Actions();
    for(int I=0;I<Rows.Num();++I)
    {
        Options->AddSlot().AutoHeight().Padding(0,4)
        [
            SNew(SButton).ContentPadding(FMargin(14,9))
            .ButtonColorAndOpacity(FLinearColor(.11,.19,.23))
            .OnClicked_Lambda([this,I]{ if(Mode.IsValid()) Mode->Select(I); return FReply::Handled(); })
            [SNew(STextBlock).Font(Font(14)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
                .Text(FText::FromString(Rows[I]))]
        ];
    }
}