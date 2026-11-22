#include "Campaign/CityEnemyBolt.h"
#include "CityGeometry.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"
#include "Abilities/CityAbilities.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ACityEnemyBolt::ACityEnemyBolt()
{
    PrimaryActorTick.bCanEverTick = true;
    Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pulse"));
    RootComponent = Model;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Model->SetStaticMesh(Mesh.Object);
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InitialLifeSpan = 4;
}
void ACityEnemyBolt::Launch(FVector Heading, float Speed, float Damage)
{
    Velocity = Heading.GetSafeNormal() * Speed;
    Strength = Damage;
    SetActorRotation(Heading.Rotation());
    SetActorScale3D(FVector(.54,.17,.17));
    if (auto* Mat=CityGeometry::TintedMaterial(FLinearColor(6,.34f,.03f))) Model->SetMaterial(0,Mat);
}
void ACityEnemyBolt::ReturnToPlayer(APawn* NewInstigator,FVector Heading,float Speed,float Damage,float Range)
{
    bFriendly=true;
    SetOwner(NewInstigator);
    SetInstigator(NewInstigator);
    Velocity=Heading.GetSafeNormal()*Speed;
    Strength=Damage;
    TravelRemaining=Range;
    SetActorRotation(Velocity.Rotation());
    SetActorScale3D(FVector(.56,.13,.13));
    SetLifeSpan(4.f);
    if (auto* Mat=CityGeometry::TintedMaterial(FLinearColor(1.9f,.65f,4.f))) Model->SetMaterial(0,Mat);
}
void ACityEnemyBolt::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector Start=GetActorLocation();
    APawn* Player=UGameplayStatics::GetPlayerPawn(this,0);
    UCityAbilities* Skills=Player?Player->FindComponentByClass<UCityAbilities>():nullptr;
    if (!bFriendly && Skills) Velocity=Skills->BendBolt(Start,Velocity,DeltaSeconds);
    const FVector Step=bFriendly?(Velocity*DeltaSeconds).GetClampedToMaxSize(TravelRemaining):Velocity*DeltaSeconds;
    const FVector End=Start+Step;
    SetActorRotation(Velocity.Rotation());
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CityBolt), false, GetOwner());
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
    const bool Blocked=GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(10), Params);
    if (!bFriendly && Skills && Skills->InterceptBolt(*this,Start,Blocked?Hit.Location:End)) return;
    if (Blocked)
    {
        if (bFriendly)
        {
            if (auto* Enemy=Cast<AShooterNPC>(Hit.GetActor()); Enemy && Enemy->CurrentHP>0)
                UGameplayStatics::ApplyDamage(Enemy,Strength,GetInstigatorController(),this,nullptr);
        }
        else if (Hit.GetActor()==Player)
            UGameplayStatics::ApplyDamage(Player,Strength,GetInstigatorController(),this,nullptr);
        Destroy();
    }
    else
    {
        SetActorLocation(End);
        if (bFriendly && (TravelRemaining-=Step.Size())<=KINDA_SMALL_NUMBER) Destroy();
    }
}