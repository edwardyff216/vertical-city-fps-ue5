#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityArsenalProbe.generated.h"
class AShooterCharacter;
class ACityCampaignEnemy;
class UCityProgression;
UCLASS(NotBlueprintable)
class ACityArsenalProbe : public AActor
{
    GENERATED_BODY()
public:
    ACityArsenalProbe();
    virtual void Tick(float Dt) override;
private:
#if !UE_BUILD_SHIPPING
    void Check(bool Valid,const FString& Name);
    void Next(int32 Step);
    void Box(FVector Position,FVector Extent,FRotator Rotation=FRotator::ZeroRotator);
    void Initialize();
    UCityProgression* Meta=nullptr;
    AShooterCharacter* P=nullptr;
    ACityCampaignEnemy* Enemy=nullptr;
    int32 Stage=0,Index=0,Fails=0,Initial=0;
    float Before=0,Clock=0,Elapsed=0,StartX=0,Travel=0,WaitAfterDash=0;
    bool ObservedDash=false;
    float PeakContactError=0;
    int32 ContactFrames=0;
    FVector Origin=FVector(62000,60000,3000);
    TArray<FString> Items;
#endif
};