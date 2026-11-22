#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace CityGeometry
{
inline UMaterialInstanceDynamic* TintedMaterial(FLinearColor Tint)
{
    static TWeakObjectPtr<UMaterialInterface> Base;
    static TMap<FLinearColor,TWeakObjectPtr<UMaterialInstanceDynamic>> Swatches;
    if (const auto* Existing=Swatches.Find(Tint); Existing && Existing->IsValid()) return Existing->Get();
    if (!Base.IsValid()) Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Base.IsValid()) return nullptr;
    auto* Material=UMaterialInstanceDynamic::Create(Base.Get(),GetTransientPackage());
    Material->SetVectorParameterValue(TEXT("Color"),Tint);
    Swatches.Add(Tint,Material);
    return Material;
}
inline UStaticMeshComponent* Part(AActor* Actor, UStaticMesh* Mesh, FVector At, FVector Size,
    FLinearColor Tint, bool Solid = false, FRotator Rotation = FRotator::ZeroRotator)
{
    auto* Part = NewObject<UStaticMeshComponent>(Actor);
    Actor->AddInstanceComponent(Part);
    Part->SetupAttachment(Actor->GetRootComponent());
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetStaticMesh(Mesh);
    Part->SetRelativeLocationAndRotation(At, Rotation);
    Part->SetRelativeScale3D(Size / 100.f);
    Part->SetCollisionProfileName(Solid ? TEXT("BlockAll") : TEXT("NoCollision"));
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    if (auto* Mat=TintedMaterial(Tint)) Part->SetMaterial(0,Mat);
    Part->RegisterComponent();
    return Part;
}
inline void Segment(UStaticMeshComponent* Mesh, FVector From, FVector To, float Width)
{
    const FVector Line = To - From;
    Mesh->SetWorldLocationAndRotation((From + To) * .5, Line.Rotation());
    Mesh->SetWorldScale3D(FVector(Line.Size(), Width, Width) / 100.f);
    Mesh->SetVisibility(true);
}
}