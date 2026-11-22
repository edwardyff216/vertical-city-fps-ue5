#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CityFrameProbe.generated.h"

UCLASS()
class UCityFrameProbe : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float Delta) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCityFrameProbe,STATGROUP_Tickables); }
    virtual bool IsTickableWhenPaused() const override { return !File.IsEmpty(); }
    virtual bool IsTickable() const override { return Super::IsTickable() && !File.IsEmpty(); }
private:
    FString File,Rows;
    double Last=0, MemoryMiB=0, TextureMiB=0;
    bool bWrote=false;
    int32 Samples=0,Limit=0;
    void Flush();
};