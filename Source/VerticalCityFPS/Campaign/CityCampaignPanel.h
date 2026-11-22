#pragma once
#include "Widgets/SCompoundWidget.h"

class ACityCampaignMode;
class SVerticalBox;
class SCityCampaignPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCityCampaignPanel) {}
        SLATE_ARGUMENT(ACityCampaignMode*,Mode)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Refresh();
private:
    TWeakObjectPtr<ACityCampaignMode> Mode;
    TSharedPtr<SVerticalBox> Options;
};