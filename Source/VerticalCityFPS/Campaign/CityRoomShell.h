#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityRoomShell.generated.h"

UCLASS()
class VERTICALCITYFPS_API ACityRoomShell : public AActor
{
    GENERATED_BODY()
public:
    ACityRoomShell();
    void Build(int32 Layout, int32 Depth, int32 Boss, int32 Encounter=0, bool bThirdUpper=true);
    bool IsUpperGate(int32 Slot) const { return Slot == 1 || (Slot == 2 && bExtraUpper); }
    int32 GateCount() const { return Barriers.Num(); }
    FVector GateBase(int32 Slot) const;
    int32 CrossedGate(const FVector& From, const FVector& To) const;
    void RevealGate(int32 Slot, const FString& Title, const FString& Encounter, const FString& Reward, const FString& Detail);
    void OpenExtraction();
    void AmmoDrop(FVector At);
    void CollectSupplies(class AShooterCharacter* P,bool Interact);
    void WarnReinforcements(bool Active);
    static FVector Start() { return FVector(-1150,0,100); }
    static float GateY(bool Upper) { return Upper ? -720 : 720; }
    static constexpr float ThresholdX = 1380;
protected:
    void ClearGeneratedComponents();
private:
    bool bExtraUpper = true;
    void SetGateOpen(int32 Slot);
    UPROPERTY() TObjectPtr<UStaticMesh> Box;
    UPROPERTY() TObjectPtr<UMaterialInterface> StructuralMaterial;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Ammo;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Medkit;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Warnings;
    UPROPERTY() TObjectPtr<UStaticMesh> FabCargo;
    UPROPERTY() TObjectPtr<UStaticMesh> FabPillar;
    UPROPERTY() TObjectPtr<UStaticMesh> FabGenerator;
    UPROPERTY() TObjectPtr<UStaticMesh> FloorTile;
    UPROPERTY() TObjectPtr<UStaticMesh> CeilingTile;
    UPROPERTY() TObjectPtr<UStaticMesh> WallTile;
    UPROPERTY() TObjectPtr<UStaticMesh> DoorLeaf;
    UPROPERTY() TObjectPtr<UStaticMesh> SupplyCase;
    UPROPERTY() TObjectPtr<UStaticMesh> AmmoDrum;
    UPROPERTY() TObjectPtr<UStaticMesh> Lamp;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> GateLeaves;
    UStaticMeshComponent* Supply(UStaticMesh* Model,FVector Center,FVector Size,const TCHAR* Mark,FColor Color);
    UStaticMeshComponent* ArtPart(UStaticMesh* Model,FVector Center,FVector Size,FRotator Facing);
    UPROPERTY() TObjectPtr<UStaticMesh> Cargo;
    UPROPERTY() TObjectPtr<UStaticMesh> WallPanel;
    UPROPERTY() TObjectPtr<UStaticMesh> Pipe;
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Barriers;
    TMap<UStaticMesh*,class UInstancedStaticMeshComponent*> ArtBatches;
    TMap<FLinearColor,class UInstancedStaticMeshComponent*> TrimBatches;
    UInstancedStaticMeshComponent* CreateBatch(UStaticMesh* Mesh);
    void Trim(FVector Center,FVector Size,FLinearColor Color);
    void Dress(UStaticMesh* Mesh,FVector Center,FVector Size,FRotator Orientation=FRotator::ZeroRotator);
    UStaticMeshComponent* Block(FVector At, FVector Size, FLinearColor Color, bool Solid=true, FRotator Rot=FRotator::ZeroRotator);
    void Label(FVector At, const FString& Text, FColor Color, float Height);
    void Light(FVector At, FLinearColor Hue, float Intensity, float Radius);
};