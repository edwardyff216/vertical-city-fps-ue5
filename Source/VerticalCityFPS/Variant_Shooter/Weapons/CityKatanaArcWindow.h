#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CityKatanaArcWindow.generated.h"

UCLASS(meta=(DisplayName="Katana curved edge"))
class UCityKatanaArcWindow : public UAnimNotifyState
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Arc") TObjectPtr<UStaticMesh> Plane;
    UPROPERTY(EditAnywhere, Category="Arc") TObjectPtr<UMaterialInterface> Edge;
    UPROPERTY(EditAnywhere, Category="Arc") FVector Center=FVector(0,12,42);
    UPROPERTY(EditAnywhere, Category="Arc") FVector2D Size=FVector2D(220,180);
    UPROPERTY(EditAnywhere, Category="Arc") float Tilt=0;
    virtual void NotifyBegin(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Clip,float Duration,const FAnimNotifyEventReference& Event) override;
    virtual void NotifyTick(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Clip,float Dt,const FAnimNotifyEventReference& Event) override;
    virtual void NotifyEnd(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Clip,const FAnimNotifyEventReference& Event) override;
private:
    UStaticMeshComponent* FindArc(USkeletalMeshComponent* Mesh) const;
};
