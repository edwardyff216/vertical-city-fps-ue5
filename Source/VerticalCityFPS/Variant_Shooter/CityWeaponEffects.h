#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CityWeaponEffects.generated.h"

class AShooterNPC;
class AShooterCharacter;
struct FCityAffix;
struct FCityItem;
UCLASS()
class UCityWeaponEffects : public UActorComponent
{
    GENERATED_BODY()
public:
    UCityWeaponEffects();
    float ContactMultiplier(AShooterNPC* Enemy,bool Melee,const FHitResult* Hit,const FCityItem& Item);
    void Contact(AShooterNPC* Enemy,float Actual,bool Melee,bool Killed,float Impact,const FCityItem& Item);
    void Action(FName Event);
    float MovementScale(const AActor* Enemy) const;
    void ResetRoom();
    static void Sound(UObject* Context,FName Name);
    static void Beam(UWorld* World,FVector Start,FVector End,FLinearColor Color,float Radius,float Life);
protected:
    virtual void TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function) override;
private:
    friend class FCityAffixCombatTest;
    struct FPending {const FCityAffix* Effect; FVector Center,Aim; float Remaining,Bonus; int32 Waves;};
    TArray<FPending> Pending;
    struct FStatus { float Fire=0, BurnTick=0, Frost=0; int32 Seals=0; };
    TMap<TWeakObjectPtr<AShooterNPC>,FStatus> Status;
    TMap<FString,double> Ready;
    TMap<FString,int32> Contacts;
    TSet<FName> PreHit;
    float RegenWait=0, LastHP=1;
    AShooterCharacter* Player() const;
    void Trigger(TSet<FName> Events,FVector Center,AShooterNPC* Primary,const FCityItem& Item);
    void Pulse(const FCityAffix& Effect,FVector Center,FVector Aim,AShooterNPC* Primary,float Bonus=0,int32 SealGain=-1);
    void Burn(AShooterNPC* Target);
    void ShowAffix(FName Id,FVector Center,float Width);
};