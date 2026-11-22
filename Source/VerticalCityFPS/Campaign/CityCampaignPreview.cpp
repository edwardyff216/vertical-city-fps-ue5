#include "CityCampaignPreview.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"

ACityCampaignPreview::ACityCampaignPreview()
{
    bIsEditorOnlyActor=true;
#if WITH_EDITOR
    PrimaryActorTick.bCanEverTick=true;
#endif
}
void ACityCampaignPreview::BeginPlay()
{
    Super::BeginPlay();
    // PIE can duplicate an already-open editor map without running the cook filter.
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    Destroy();
}
#if WITH_EDITOR
void ACityCampaignPreview::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Rebuild();
}
void ACityCampaignPreview::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bBuilt) Rebuild();
}
void ACityCampaignPreview::Rebuild()
{
    if(!GetWorld() || GetWorld()->WorldType!=EWorldType::Editor || IsRunningCookCommandlet()) return;
    ClearGeneratedComponents();
    const int32 Boss=int32(PreviewBoss)-1;
    Build(int32(PreviewLayout),Boss==INDEX_NONE?0:4,Boss);
    for(UActorComponent* Component : GetInstanceComponents())
    {
        if(!IsValid(Component)) continue;
        Component->SetFlags(RF_Transient|RF_DuplicateTransient|RF_TextExportTransient);
        Component->bIsEditorOnly=true;
    }
    bBuilt=true;
}
#endif