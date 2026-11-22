#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityEnemyBolt.generated.h"

UCLASS()
class VERTICALCITYFPS_API ACityEnemyBolt : public AActor
{
    GENERATED_BODY()
public:
    ACityEnemyBolt();
    void Launch(FVector Heading, float Speed, float Damage);
    void ReturnToPlayer(APawn* NewInstigator, FVector Heading, float Speed, float Damage, float Range);
    bool IsFriendly() const { return bFriendly; }
    FVector FlightVelocity() const { return Velocity; }
    float DamageAmount() const { return Strength; }
    virtual void Tick(float DeltaSeconds) override;
private:
    FVector Velocity = FVector::ZeroVector;
    float Strength = 0;
    bool bFriendly = false;
    float TravelRemaining = 0;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Model;
};