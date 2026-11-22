#pragma once

#include "Components/ActorComponent.h"
#include "CityLocomotion.generated.h"

class AShooterCharacter;
class UCharacterMovementComponent;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;

UCLASS()
class UCityLocomotion : public UActorComponent
{
	GENERATED_BODY()
    friend class FCityAffixCombatTest;

public:
	UCityLocomotion();
	void BindControls(UEnhancedInputComponent* Input);
	bool TryWallJump();
	void Landed() { bUsedWallJump = false; }
	bool IsSliding() const { return bSliding; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    friend class ACityArsenalProbe;
	void SprintPressed();
	void SprintReleased();
	void StartSlide();
	void FinishSlide();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Controls;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundSubsystem;
	AShooterCharacter* Character = nullptr;
	UCharacterMovementComponent* Movement = nullptr;

	FVector StandingMeshOffset = FVector::ZeroVector;
	double SlideEnd = 0.0;
	double NextSlide = 0.0;
	float NormalFriction = 0.0f;
	float NormalBraking = 0.0f;
	float NormalBrakeFactor = 0.0f;
	float CameraDrop = 0.0f;
	bool bUsedWallJump = false;
	bool bSliding = false;
	bool bSprintHeld = false;
};
