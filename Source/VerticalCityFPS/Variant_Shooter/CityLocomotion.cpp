#include "CityLocomotion.h"
#include "Abilities/CityAbilities.h"
#include "ShooterCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

namespace
{
	constexpr float WalkSpeed = 700.f;
	constexpr float SprintSpeed = 1050.f;
	constexpr float SlideSpeed = 1400.f;
	constexpr float SlideSeconds = 0.7f;
	constexpr float SlideCooldown = 1.f;
	constexpr float CrouchEyeDrop = 69.f;
}

UCityLocomotion::UCityLocomotion()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCityLocomotion::BeginPlay()
{
	Super::BeginPlay();
	Character = CastChecked<AShooterCharacter>(GetOwner());
	Movement = Character->GetCharacterMovement();
	Movement->GravityScale=2200.f/FMath::Max(1.f,FMath::Abs(GetWorld()->GetGravityZ()));
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	Movement->SetCrouchedHalfHeight(50.f);
	Movement->MaxWalkSpeed = WalkSpeed;
	StandingMeshOffset = Character->GetFirstPersonMesh()->GetRelativeLocation();
}

void UCityLocomotion::BindControls(UEnhancedInputComponent* Input)
{
	const auto* PC = Cast<APlayerController>(CastChecked<AShooterCharacter>(GetOwner())->GetController());
	auto* Subsystem = PC ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (!Subsystem) return;

	if (BoundSubsystem.IsValid()) BoundSubsystem->RemoveMappingContext(Controls);
	Controls = NewObject<UInputMappingContext>(this);
	auto* Sprint = NewObject<UInputAction>(Controls);
	auto* Slide = NewObject<UInputAction>(Controls);
    AShooterCharacter* Shooter = CastChecked<AShooterCharacter>(GetOwner());
    auto* Skills=Shooter->GetCityAbilities();
    auto bind=[&](FKey Key,void (UCityAbilities::*Method)()){
        UInputAction* Action=NewObject<UInputAction>(Controls);
        Controls->MapKey(Action,Key);
        Input->BindAction(Action,ETriggerEvent::Started,Skills,Method);
    };
    bind(EKeys::C,&UCityAbilities::CastPrimary);
    bind(EKeys::X,&UCityAbilities::CastSecondary);
    bind(EKeys::G,&UCityAbilities::CastUltimate);
    auto* Swap=NewObject<UInputAction>(Controls);
    auto* View=NewObject<UInputAction>(Controls);
    Controls->MapKey(View,EKeys::V);
    Input->BindAction(View,ETriggerEvent::Started,Shooter,&AShooterCharacter::ToggleCityView);
    auto* Parry=NewObject<UInputAction>(Controls);
    Controls->MapKey(Swap,EKeys::Q);
    Controls->MapKey(Parry,EKeys::F);
    Input->BindAction(Swap,ETriggerEvent::Started,Shooter,&AShooterCharacter::DoSwitchWeapon);
    Input->BindAction(Parry,ETriggerEvent::Started,Shooter,&AShooterCharacter::CityParry);
    auto* SwordSlot = NewObject<UInputAction>(Controls);
    auto* RifleSlot = NewObject<UInputAction>(Controls);
    auto* Special = NewObject<UInputAction>(Controls);
    Controls->MapKey(SwordSlot, EKeys::Two);
    Controls->MapKey(RifleSlot, EKeys::One);
    Controls->MapKey(Special, EKeys::RightMouseButton);
    Input->BindAction(SwordSlot, ETriggerEvent::Started, Shooter, &AShooterCharacter::EquipKatana);
    Input->BindAction(RifleSlot, ETriggerEvent::Started, Shooter, &AShooterCharacter::EquipRifle);
    Input->BindAction(Special, ETriggerEvent::Started, Shooter, &AShooterCharacter::DoSpecial);
    Input->BindAction(Special,ETriggerEvent::Completed,Shooter,&AShooterCharacter::EndCityAim);
    Input->BindAction(Special,ETriggerEvent::Canceled,Shooter,&AShooterCharacter::EndCityAim);
	auto* Reload = NewObject<UInputAction>(Controls);
	Controls->MapKey(Reload, EKeys::R);
	Input->BindAction(Reload, ETriggerEvent::Started, CastChecked<AShooterCharacter>(GetOwner()), &AShooterCharacter::DoReload);
	Controls->MapKey(Sprint, EKeys::LeftShift);
	Controls->MapKey(Slide, EKeys::LeftControl);
	Input->BindAction(Sprint, ETriggerEvent::Started, this, &ThisClass::SprintPressed);
	Input->BindAction(Sprint, ETriggerEvent::Completed, this, &ThisClass::SprintReleased);
	Input->BindAction(Sprint, ETriggerEvent::Canceled, this, &ThisClass::SprintReleased);
	Input->BindAction(Slide, ETriggerEvent::Started, this, &ThisClass::StartSlide);
	Subsystem->AddMappingContext(Controls, 1);
	BoundSubsystem = Subsystem;
}

void UCityLocomotion::EndPlay(const EEndPlayReason::Type Reason)
{
	if (BoundSubsystem.IsValid()) BoundSubsystem->RemoveMappingContext(Controls);
	Super::EndPlay(Reason);
}

void UCityLocomotion::SprintPressed()
{
	bSprintHeld = true;
	if (Movement && !bSliding) Movement->MaxWalkSpeed = SprintSpeed;
}

void UCityLocomotion::SprintReleased()
{
	bSprintHeld = false;
	if (Movement && !bSliding) Movement->MaxWalkSpeed = WalkSpeed;
}

bool UCityLocomotion::TryWallJump()
{
    if (!Movement || !Movement->IsFalling() || bUsedWallJump || bSliding ||
        !Character->GetCityAbilities()->AllowsWallJump()) return false;

    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    const FVector Center = Capsule->GetComponentLocation();
    const float Reach = Capsule->GetScaledCapsuleRadius() + 24.f;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityWallJump), false, Character);
    TArray<AActor*> Attached;
    Character->GetAttachedActors(Attached);
    Query.AddIgnoredActors(Attached);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

    FVector Heading = Movement->GetCurrentAcceleration().GetSafeNormal2D();
    if (Heading.IsNearlyZero()) Heading = Character->GetActorForwardVector().GetSafeNormal2D();
    FHitResult Closest;
    float BestDistance = Reach + 1.f;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const FVector End = Center + Heading.RotateAngleAxis(Index * 45.f, FVector::UpVector) * Reach;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByObjectType(Hit, Center, End, Objects, Query) &&
            Hit.GetComponent() && Hit.GetComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block &&
            !Hit.bStartPenetrating && FMath::Abs(Hit.ImpactNormal.Z) <= .25f && Hit.Distance < BestDistance)
        {
            BestDistance = Hit.Distance;
            Closest = Hit;
        }
    }
    if (!Closest.bBlockingHit) return false;
    const FVector Normal = Closest.ImpactNormal.GetSafeNormal2D();
    const FVector AlongWall = FVector::VectorPlaneProject(Movement->Velocity, Normal).GetClampedToMaxSize2D(500.f);
    FVector Kick = Normal * 660.f + AlongWall;
    Kick.Z = Movement->JumpZVelocity;
    bUsedWallJump = true;
    Character->LaunchCharacter(Kick, true, true);
    return true;
}

void UCityLocomotion::StartSlide()
{
	if (!Movement || Character->IsDead() || bSliding) return;
    Character->GetCityAbilities()->SlidePressed();
    if(!Movement->IsMovingOnGround()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextSlide) return;
	FVector Heading = Character->GetPendingMovementInputVector().GetSafeNormal2D();
	if (Heading.IsNearlyZero()) Heading = Movement->GetCurrentAcceleration().GetSafeNormal2D();
	if (Heading.IsNearlyZero()) return;


    const float Speed=SlideSpeed*(1.f+Character->CityAffix(TEXT("slide_speed")))*Character->GetCityAbilities()->GetSlideScale();
	Character->TriggerCityAction(TEXT("slide"));
	bSliding = true;
	SlideEnd = Now + SlideSeconds*(1.f+Character->CityAffix(TEXT("slide_time")));
	NextSlide = Now + SlideCooldown*(1.f-Character->CityAffix(TEXT("slide_cd")));
	NormalFriction = Movement->GroundFriction;
	NormalBraking = Movement->BrakingDecelerationWalking;
	NormalBrakeFactor = Movement->BrakingFrictionFactor;
	Movement->GroundFriction = 0.f;
	Movement->BrakingDecelerationWalking = 0.f;
	Movement->BrakingFrictionFactor = 0.f;
	Movement->MaxWalkSpeed = Speed;
	Movement->MaxWalkSpeedCrouched = Speed;
	Character->ConsumeMovementInputVector();
	Character->Crouch();
	Movement->Velocity = Heading * Speed;
}

void UCityLocomotion::FinishSlide()
{
	bSliding = false;
	Movement->GroundFriction = NormalFriction;
	Movement->BrakingDecelerationWalking = NormalBraking;
	Movement->BrakingFrictionFactor = NormalBrakeFactor;
	Movement->MaxWalkSpeed = bSprintHeld ? SprintSpeed : WalkSpeed;
	Movement->MaxWalkSpeedCrouched = WalkSpeed * 0.5f;
	Movement->Velocity = Movement->Velocity.GetClampedToMaxSize2D(Movement->MaxWalkSpeed);
	// CharacterMovement retries expansion on subsequent ticks if a ceiling blocks it.
	Character->UnCrouch();
}

void UCityLocomotion::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta, TickType, TickFunction);
	if (!Movement) return;
    if (!bSliding)
        Movement->MaxWalkSpeed=(bSprintHeld?SprintSpeed*(1.f+Character->CityAffix(TEXT("sprint"))):WalkSpeed)*(1.f+Character->CityAffix(TEXT("lightweight")))*Character->SwapSpeedScale();
	if (bSliding && (GetWorld()->GetTimeSeconds() >= SlideEnd || !Movement->IsMovingOnGround() ||
		Movement->Velocity.SizeSquared2D() < FMath::Square(WalkSpeed * 0.35f)))
	{
		FinishSlide();
	}
	// The template's camera and gun share the first-person mesh. Lower both together.
	const float Target = (bSliding || Character->bIsCrouched) ? CrouchEyeDrop : 0.f;
	if (CameraDrop != Target)
	{
		CameraDrop = FMath::FInterpConstantTo(CameraDrop, Target, Delta, 560.f);
		auto* Mesh = Character->GetFirstPersonMesh();
		const FVector LocalDrop = Mesh->GetAttachParent()->GetComponentTransform().InverseTransformVector(FVector(0, 0, -CameraDrop));
		Mesh->SetRelativeLocation(StandingMeshOffset + LocalDrop);
	}
}
