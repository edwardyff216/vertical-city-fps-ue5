#pragma once
#include "CoreMinimal.h"
#include "Campaign/CityRoomShell.h"
#include "CityCampaignPreview.generated.h"

UENUM()
enum class ECityPreviewLayout : uint8
{
    Offset UMETA(DisplayName="错层仓库"), WhiteDepot UMETA(DisplayName="白区转运仓"),
    Crosswalk UMETA(DisplayName="横桥检修间"), LoadingDock UMETA(DisplayName="坡台装卸间"),
    Spine UMETA(DisplayName="管线走廊"), Court UMETA(DisplayName="四柱中庭"),
    Switchback UMETA(DisplayName="折返工坊"), Lanes UMETA(DisplayName="双列货场"),
    Relay UMETA(DisplayName="中继机房"), Engine UMETA(DisplayName="机组大厅")
};
UENUM()
enum class ECityPreviewBoss : uint8
{
    None UMETA(DisplayName="普通房"), Spire UMETA(DisplayName="镜塔哨卫"),
    Breaker UMETA(DisplayName="炉心破阵者"), Relay UMETA(DisplayName="中继织盾者")
};

/** Editor view of the campaign room builder. The map stores only the two preview selections. */
UCLASS()
class VERTICALCITYFPS_API ACityCampaignPreview : public ACityRoomShell
{
    GENERATED_BODY()
public:
    ACityCampaignPreview();
    virtual bool IsEditorOnly() const override { return true; }
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere,Category="Campaign Preview") ECityPreviewLayout PreviewLayout=ECityPreviewLayout::Offset;
    UPROPERTY(EditAnywhere,Category="Campaign Preview") ECityPreviewBoss PreviewBoss=ECityPreviewBoss::None;
#endif
#if WITH_EDITOR
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
private:
    bool bBuilt=false;
    void Rebuild();
#endif
protected:
    virtual void BeginPlay() override;
};