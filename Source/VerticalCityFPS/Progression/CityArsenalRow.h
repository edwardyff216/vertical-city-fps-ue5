#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "CityArsenalRow.generated.h"

/** Source units for combat are metres and seconds. View-model offsets are UE centimetres. */
USTRUCT(BlueprintType)
struct FCityArsenalRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float Damage=19;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float Impact=6;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float Interval=.115f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 Pellets=1;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float Spread=.012f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float RangeMetres=70;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 Magazine=30;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) int32 Reserve=240;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float Reload=1.65f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) float ViewScale=.36f;
    UPROPERTY(EditAnywhere,BlueprintReadOnly) FVector HipGrip=FVector(49,22,-21);
    static const FCityArsenalRow* Find(FName Kind);
};