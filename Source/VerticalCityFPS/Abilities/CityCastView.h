#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CityCastView.generated.h"

class AShooterCharacter;
class UPoseableMeshComponent;
class UPrimitiveComponent;
class UCitySkillVisuals;
struct FMinimalViewInfo;

struct FCityPoseClip { int32 Offset=0, Frames=0; };

UCLASS()
class UCityCastView : public UActorComponent
{
    GENERATED_BODY()
public:
    void Setup(AShooterCharacter* Character);
    void Pose(FName Id,FName Clip,float Time,UCitySkillVisuals* Art);
    void Frame(float Delta,float Blend,FMinimalViewInfo& View,FVector AimPoint);
    void Hide();
    FVector Palm(bool bRight) const;
private:
    UPROPERTY(Transient) TObjectPtr<AShooterCharacter> Player;
    UPROPERTY(Transient) TObjectPtr<UPoseableMeshComponent> Body;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden;
    TArray<uint8> Data;
    TArray<int32> BoneMap;
    TArray<FQuat> RefGlobal, Animated;
    TMap<FName,FCityPoseClip> Clips;
    int32 SourceBones=0, FrameBytes=0, Hips=INDEX_NONE;
    float Boom=360;
    bool bFramed=false;
    void SetFraming(bool bShow);
    void Props(FName Id,FName Clip,UCitySkillVisuals* Art);
};

