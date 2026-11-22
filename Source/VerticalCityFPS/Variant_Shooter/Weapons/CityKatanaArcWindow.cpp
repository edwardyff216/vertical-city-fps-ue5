#include "CityKatanaArcWindow.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

UStaticMeshComponent* UCityKatanaArcWindow::FindArc(USkeletalMeshComponent* Mesh) const
{
    if (!Mesh) return nullptr;
    for (USceneComponent* Child:Mesh->GetAttachChildren())
        if (auto* Part=Cast<UStaticMeshComponent>(Child); Part && Part->ComponentHasTag(GetFName())) return Part;
    return nullptr;
}

void UCityKatanaArcWindow::NotifyBegin(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Clip,float Duration,const FAnimNotifyEventReference& Event)
{
    if (!Mesh || !Plane || !Edge || !Mesh->GetWorld()) return;
    if (auto* Previous=FindArc(Mesh)) Previous->DestroyComponent();
    auto* Part=NewObject<UStaticMeshComponent>(Mesh,NAME_None,RF_Transient);
    Part->SetupAttachment(Mesh);
    Part->SetStaticMesh(Plane);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->SetCastShadow(false);
    Part->ComponentTags.Add(GetFName());
    Part->SetRelativeLocation(Mesh->GetSocketTransform(TEXT("Hips"),RTS_Component).GetLocation()+Center);
    Part->SetRelativeRotation(FQuat(FVector::RightVector,FMath::DegreesToRadians(Tilt))*FQuat(FVector::ForwardVector,HALF_PI));
    Part->SetRelativeScale3D(FVector(Size.X*.01,Size.Y*.01,1));
    Part->SetMaterial(0,Edge);
    Part->RegisterComponentWithWorld(Mesh->GetWorld());
    if (auto* Ink=Part->CreateDynamicMaterialInstance(0))
    {
        Ink->SetScalarParameterValue(TEXT("Progress"),0);
        Part->SetCustomPrimitiveDataFloat(0,1.f/FMath::Max(Duration,.001f));
    }
}
void UCityKatanaArcWindow::NotifyTick(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Clip,float Dt,const FAnimNotifyEventReference& Event)
{
    if (auto* Part=FindArc(Mesh))
    {
        Part->SetRelativeLocation(Mesh->GetSocketTransform(TEXT("Hips"),RTS_Component).GetLocation()+Center);
        if (auto* Ink=Cast<UMaterialInstanceDynamic>(Part->GetMaterial(0)))
            Ink->SetScalarParameterValue(TEXT("Progress"),Ink->K2_GetScalarParameterValue(TEXT("Progress"))+Dt*Part->GetCustomPrimitiveData().Data[0]);
    }
}
void UCityKatanaArcWindow::NotifyEnd(USkeletalMeshComponent* Mesh,UAnimSequenceBase* Clip,const FAnimNotifyEventReference& Event)
{
    if (auto* Part=FindArc(Mesh)) Part->DestroyComponent();
}
