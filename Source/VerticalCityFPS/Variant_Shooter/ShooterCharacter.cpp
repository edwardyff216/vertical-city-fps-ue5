// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "CityArsenalProbe.h"
#include "Campaign/CityCampaignEnemy.h"
#include "EngineUtils.h"
#include "Progression/CityProgression.h"
#include "Progression/CityItemCatalog.h"
#include "Abilities/CityAbilities.h"
#include "CityWeaponEffects.h"
#include "ShooterNPC.h"
#include "CityLocomotion.h"
#include "S07RiflePresentation.h"
#include "ShooterWeapon.h"
#include "CityKatana.h"
#include "EnhancedInputComponent.h"
#include "Components/InputComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterGameMode.h"

AShooterCharacter::AShooterCharacter()
{
    CityAbilities = CreateDefaultSubobject<UCityAbilities>(TEXT("CityAbilities"));
    CityEffects = CreateDefaultSubobject<UCityWeaponEffects>(TEXT("CityWeaponEffects"));
	CityLocomotion = CreateDefaultSubobject<UCityLocomotion>(TEXT("CityLocomotion"));
	S07Presentation = CreateDefaultSubobject<US07RiflePresentation>(TEXT("S07RiflePresentation"));
	StartingWeapon = TSoftClassPtr<AShooterWeapon>(FSoftObjectPath(TEXT("/Game/Variant_Shooter/Blueprints/Pickups/Weapons/BP_ShooterWeapon_Rifle.BP_ShooterWeapon_Rifle_C")));

	// create the noise emitter component
	PawnNoiseEmitter = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("Pawn Noise Emitter"));

	// configure movement
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);
}

void AShooterCharacter::CalcCamera(float Delta, FMinimalViewInfo& OutResult)
{
    Super::CalcCamera(Delta, OutResult);
    int32 Width=0,Height=0;
    if(auto* PC=Cast<APlayerController>(GetController())) PC->GetViewportSize(Width,Height);
    const float Aspect=Width>0 && Height>0 ? float(Width)/Height : OutResult.AspectRatio;
    const float VerticalDegrees=FMath::Lerp(84.f,62.f,S07Presentation->GetAimBlend());
    OutResult.AspectRatioAxisConstraint=AspectRatio_MaintainXFOV;
    OutResult.FOV=FMath::RadiansToDegrees(2.f*FMath::Atan(FMath::Tan(FMath::DegreesToRadians(VerticalDegrees*.5f))*Aspect));
    if (auto* Sword = Cast<ACityKatana>(CurrentWeapon)) Sword->FrameDashView(Delta, OutResult);
    else if (CityAbilities->GetCastingId().IsNone()) S07Presentation->FrameShoulder(Delta,OutResult);
    CityAbilities->FrameCamera(Delta,OutResult);
}

void AShooterCharacter::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("CityArsenalVerify")))
    {
        FActorSpawnParameters Options;Options.Owner=this;
        GetWorld()->SpawnActor<ACityArsenalProbe>(FVector::ZeroVector,FRotator::ZeroRotator,Options);
    }
#endif

	// reset HP to max
    BaseCityHP = MaxHP;
    CurrentHP = MaxHP;
    if (auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>())
        Meta->OnEquipmentChanged.AddUObject(this,&ThisClass::RefreshCityLoadout);

	// Defer until the pawn, weapon components and HUD have finished BeginPlay.
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (!IsDead() && !CurrentWeapon)
		{
			if (HasCampaignEquipment()) RefreshCityLoadout();
            else if (UClass* Rifle = StartingWeapon.LoadSynchronous()) AddWeaponClass(Rifle);
		}
	}));

	// update the HUD
	OnDamaged.Broadcast(1.0f);
}

void AShooterCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// base class handles move, aim and jump inputs
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		CityLocomotion->BindControls(EnhancedInputComponent);

		// Firing
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AShooterCharacter::DoStartFiring);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AShooterCharacter::DoStopFiring);

		// Switch weapon
		EnhancedInputComponent->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &AShooterCharacter::DoSwitchWeapon);
	}

}

float AShooterCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// ignore if already dead
	if (CurrentHP <= 0.0f)
	{
		return 0.0f;
	}

	// Reduce HP
    Damage=CityAbilities->FilterIncomingDamage(Damage,DamageCauser);
    Damage*=1.f-CityAffix(TEXT("armor"));
    Damage*=1.f-CityAffix(Cast<ACityCampaignEnemy>(DamageCauser) ? TEXT("melee_guard") : TEXT("ranged_guard"));
    if (GetCityHealthFraction()<=.35f) Damage*=1.f-CityAffix(TEXT("last_stand"));
    if (Damage<=0) return 0;
    CurrentHP -= Damage;
    if (CurrentHP<=0 && !UsedLastChance && HasCampaignEquipment() && GetGameInstance()->GetSubsystem<UCityProgression>()->HasNode(TEXT("last_chance")))
    {
        CurrentHP=1;
        UsedLastChance=true;
    }

	// Have we depleted HP?
	if (CurrentHP <= 0.0f)
	{
		Die();
	}

	// update the HUD
	OnDamaged.Broadcast(FMath::Max(0.0f, CurrentHP / MaxHP));

	return Damage;
}

void AShooterCharacter::DoAim(float Yaw, float Pitch)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoAim(Yaw, Pitch);
	}
}

void AShooterCharacter::DoMove(float Right, float Forward)
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		if (!IsKatanaDashing() && !CityLocomotion->IsSliding()) Super::DoMove(Right, Forward);
	}
}

void AShooterCharacter::Landed(const FHitResult& Hit)
{
    CityLocomotion->Landed();
    Super::Landed(Hit);
}

void AShooterCharacter::DoJumpStart()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		if (!IsKatanaDashing()) {
            GetCharacterMovement()->JumpZVelocity=750.f*(1.f+CityAffix(TEXT("jump")));
            if (CityAbilities->JumpPressed() || CityLocomotion->TryWallJump()) return;
            Super::DoJumpStart();
        }
	}
}

void AShooterCharacter::DoJumpEnd()
{
	// only route inputs if the character is not dead
	if (!IsDead())
	{
		Super::DoJumpEnd();
	}
}

void AShooterCharacter::DoStartFiring()
{
	// fire the current weapon
	if (CurrentWeapon && !IsDead() && GetWorld()->GetTimeSeconds()>=SwapFireReady)
	{
        if (!CityAbilities->HandlePrimaryAttack()) CurrentWeapon->StartFiring();
	}
}

void AShooterCharacter::DoStopFiring()
{
	// stop firing the current weapon
	if (CurrentWeapon && !IsDead())
	{
		CurrentWeapon->StopFiring();
	}
}

bool AShooterCharacter::IsCitySliding() const { return CityLocomotion->IsSliding(); }
bool AShooterCharacter::IsKatanaDashing() const
{
    const auto* Blade = Cast<ACityKatana>(CurrentWeapon);
    return Blade && Blade->IsDashing();
}

void AShooterCharacter::EquipWeaponOfClass(TSubclassOf<AShooterWeapon> WeaponClass)
{
    if (IsDead() || !WeaponClass) return;
    if (CurrentWeapon && CurrentWeapon->IsA(WeaponClass)) return;
    AShooterWeapon* Owned = FindWeaponOfType(WeaponClass);
    if (!Owned) { AddWeaponClass(WeaponClass); return; }
    if (CurrentWeapon) CurrentWeapon->DeactivateWeapon();
    CurrentWeapon = Owned;
    Owned->ActivateWeapon(PlayerTag);
}

bool AShooterCharacter::HasCampaignEquipment() const
{
#if !UE_BUILD_SHIPPING
    if (Tags.Contains(TEXT("CityArsenalFixture"))) return true;
#endif
    return GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_Campaign"));
}
void AShooterCharacter::HealCity(float Amount)
{
    if (!IsDead() && Amount>0)
    {
        CurrentHP=FMath::Min(MaxHP,CurrentHP+Amount);
        OnDamaged.Broadcast(GetCityHealthFraction());
    }
}
void AShooterCharacter::AwardCityAmmo(float Fraction)
{
    for (AShooterWeapon* Weapon : OwnedWeapons)
        if (IsValid(Weapon)) Weapon->GiveReserve(Fraction);
}
float AShooterCharacter::CityAffix(FName Id) const
{
    if (!HasCampaignEquipment()) return 0;
    auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>();
    return FCityItemCatalog::Amount(Meta->GetWeapon(CitySlot),Id);
}
void AShooterCharacter::TriggerCityAction(FName Action)
{
    if (!HasCampaignEquipment()) return;
    auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>();
    if (Action==TEXT("parry") && Meta->HasNode(TEXT("parry_aid"))) HealCity(8);
    if (Action==TEXT("slide") && CurrentWeapon)
    {
        bRefundReady=Meta->AbilityLevel(TEXT("slide_refund"))>0;
        if(Meta->HasNode(TEXT("slide_reload"))) CurrentWeapon->RefillCityMagazine(MAX_int32);
    }
    CityEffects->Action(Action);
}
int32 AShooterCharacter::ConsumeSlideRefund(bool HitEnemy)
{
    if(!bRefundReady) return 0;
    bRefundReady=false;
    return HitEnemy ? GetGameInstance()->GetSubsystem<UCityProgression>()->AbilityLevel(TEXT("slide_refund")) : 0;
}
float AShooterCharacter::SwapSpeedScale() const
{
    if(GetWorld()->GetTimeSeconds()>=SwapHasteUntil) return 1.f;
    const int32 Level=GetGameInstance()->GetSubsystem<UCityProgression>()->AbilityLevel(TEXT("swap_haste"));
    return 1.f+FMath::Loge(1.f+Level*.12f);
}
float AShooterCharacter::ConsumeRiposte()
{
    if (!bRiposte) return 1.f;
    bRiposte=false;
    return 1.f+.35f*GetGameInstance()->GetSubsystem<UCityProgression>()->AbilityLevel(TEXT("riposte"));
}
float AShooterCharacter::HitCityTarget(AShooterNPC* Target,float BaseDamage,bool Melee,const FHitResult* Hit,float Impact,const FCityItem* ArmedItem,AShooterWeapon* SourceWeapon,TSubclassOf<UDamageType> DamageType)
{
    if (!Target || Target->CurrentHP<=0 || Target->GetCombatTeam()==GetCombatTeam()) return 0;
    const float Before=Target->CurrentHP;
    auto* Progress=GetGameInstance()->GetSubsystem<UCityProgression>();
    const FCityItem Item=ArmedItem ? *ArmedItem : HasCampaignEquipment()?Progress->GetWeapon(CitySlot):FCityItem{};
    AShooterWeapon* Causer=SourceWeapon ? SourceWeapon : GetCityWeapon();
    float Damage=CityAbilities->WeaponDamage(Target,BaseDamage,Melee,ArmedItem!=nullptr);
    Damage*=CityEffects->ContactMultiplier(Target,Melee,Hit,Item);
    if (Hit) UGameplayStatics::ApplyPointDamage(Target,Damage,(Hit->TraceEnd-Hit->TraceStart).GetSafeNormal(),*Hit,GetController(),Causer,DamageType);
    else UGameplayStatics::ApplyDamage(Target,Damage,GetController(),Causer,DamageType);
    const float Actual=FMath::Clamp(Before-Target->CurrentHP,0.f,Before);
    if (Actual>0)
    {
        const bool Killed=Target->CurrentHP<=0;
        CityAbilities->OnWeaponHit(Target,Actual,Melee,Killed);
        CityEffects->Contact(Target,Actual,Melee,Killed,Impact,Item);
        if (Killed && Causer) Causer->GiveRounds(FMath::RoundToInt(FCityItemCatalog::Amount(Item,TEXT("bounty"))));
        if (Killed && HasCampaignEquipment() && CurrentWeapon && GetGameInstance()->GetSubsystem<UCityProgression>()->HasNode(TEXT("kill_feed")))
            CurrentWeapon->RefillCityMagazine(1);
    }
    return Actual;
}
void AShooterCharacter::SelectCitySlot(int32 Slot)
{
    if (IsDead()) return;
    auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>();
    const FCityItem Item=Meta->GetWeapon(Slot);
    if (Item.Id.IsEmpty()) return;
    const auto Stats=Meta->GetStats(Slot);
    const bool bChangedSlot = Slot!=CitySlot;
    if(bChangedSlot && CurrentWeapon)
    {
        CancelCityInput();
        SwapFireReady=GetWorld()->GetTimeSeconds()+.15;
        SwapHasteUntil=GetWorld()->GetTimeSeconds()+2.;
    }
    CitySlot=Slot;
    EquipWeaponOfClass(Item.Kind==TEXT("sword") ? ACityKatana::StaticClass() : StartingWeapon.LoadSynchronous());
    if (CurrentWeapon && !CurrentWeapon->IsA<ACityKatana>())
    {
        CurrentWeapon->ConfigureCity(Item,Stats);
        S07Presentation->SetArsenal(Item.Kind);
        if (bChangedSlot) S07Presentation->BeginHandDraw();
    }
}
USkeletalMesh* AShooterCharacter::ResolveCityBody()
{
    const auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>();
    const FName Id=Meta->GetProfile().Character;
    if(!SelectedBody || SelectedBodyId!=Id)
    {
        const TCHAR* Path=Id==TEXT("white_01") ? TEXT("/Game/VerticalCity/White/SK_White.SK_White") : TEXT("/Game/VerticalCity/S07/SK_S07.SK_S07");
        SelectedBody=LoadObject<USkeletalMesh>(nullptr,Path);
        SelectedBodyId=Id;
    }
    return SelectedBody.Get();
}
void AShooterCharacter::RefreshCityLoadout()
{
    if (!HasCampaignEquipment() || IsDead()) return;
    auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>();
    const float NewMax=BaseCityHP+(Meta->HasNode(TEXT("endurance"))?15.f:0.f)+12.f*Meta->AbilityLevel(TEXT("vitality"));
    CurrentHP=FMath::Clamp(CurrentHP+FMath::Max(0.f,NewMax-MaxHP),0.f,NewMax);
    MaxHP=NewMax;
    OnDamaged.Broadcast(GetCityHealthFraction());
    SelectCitySlot(Meta->GetWeapon(CitySlot).Id.IsEmpty() ? 1-CitySlot : CitySlot);
}
void AShooterCharacter::EquipKatana() { if (HasCampaignEquipment()) SelectCitySlot(1); else EquipWeaponOfClass(ACityKatana::StaticClass()); }
void AShooterCharacter::EquipRifle() { if (HasCampaignEquipment()) SelectCitySlot(0); else EquipWeaponOfClass(StartingWeapon.LoadSynchronous()); }

void AShooterCharacter::DoSpecial()
{
    if (auto* Blade = Cast<ACityKatana>(CurrentWeapon)) Blade->RequestDash();
    else S07Presentation->SetAiming(true);
}

void AShooterCharacter::CityParry()
{
    if (IsDead() || !HasCampaignEquipment() || !CityAbilities->GetCastingId().IsNone()) return;
    ACityCampaignEnemy* Best=nullptr;
    double Distance=370.f*(1.f+CityAffix(TEXT("parry_range")));
    for(TActorIterator<ACityCampaignEnemy> It(GetWorld());It;++It)
    {
        auto* E=*It;
        const double D=GetDistanceTo(E);
        if (!E->IsWindingUp() || E->CurrentHP<=0 || D>=Distance) continue;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(CityParryLOS),false,this);
        Q.AddIgnoredActor(E);
        if (GetWorld()->LineTraceTestByChannel(GetActorLocation(),E->GetActorLocation(),ECC_Visibility,Q)) continue;
        Best=E;Distance=D;
    }
    if (Best)
    {
        Best->StunFor(1.4f);
        auto* Meta=GetGameInstance()->GetSubsystem<UCityProgression>();
        bRiposte=Meta->AbilityLevel(TEXT("riposte"))>0;
        const int32 Counter=Meta->AbilityLevel(TEXT("counter"));
        if (Counter>0) UGameplayStatics::ApplyDamage(Best,Counter*25.f,GetController(),this,nullptr);
        TriggerCityAction(TEXT("parry"));
        UCityWeaponEffects::Sound(this,TEXT("parry"));
        UCityWeaponEffects::Beam(GetWorld(),GetActorLocation(),Best->GetActorLocation(),FLinearColor(.4f,1,1),6,.16f);
    }
}
void AShooterCharacter::EndCityAim() { S07Presentation->SetAiming(false); }

void AShooterCharacter::DoReload()
{
	if (CurrentWeapon && !IsDead())
	{
        if (!CurrentWeapon->IsA<ACityKatana>()) CurrentWeapon->StartReload();
	}
}

void AShooterCharacter::DoSwitchWeapon()
{
	if (HasCampaignEquipment()) { SelectCitySlot(1-CitySlot); return; }
	// ensure we have at least two weapons two switch between
	if (OwnedWeapons.Num() > 1 && !IsDead())
	{
		// deactivate the old weapon
		CurrentWeapon->DeactivateWeapon();

		// find the index of the current weapon in the owned list
		int32 WeaponIndex = OwnedWeapons.Find(CurrentWeapon);

		// is this the last weapon?
		if (WeaponIndex == OwnedWeapons.Num() - 1)
		{
			// loop back to the beginning of the array
			WeaponIndex = 0;
		}
		else {
			// select the next weapon index
			++WeaponIndex;
		}

		// set the new weapon as current
		CurrentWeapon = OwnedWeapons[WeaponIndex];

		// activate the new weapon
		CurrentWeapon->ActivateWeapon(PlayerTag);
	}
}

void AShooterCharacter::AttachWeaponMeshes(AShooterWeapon* Weapon)
{
	const FAttachmentTransformRules AttachmentRule(EAttachmentRule::SnapToTarget, false);

	// attach the weapon actor
	Weapon->AttachToActor(this, AttachmentRule);

	// attach the weapon meshes
	Weapon->GetFirstPersonMesh()->AttachToComponent(GetFirstPersonMesh(), AttachmentRule, FirstPersonWeaponSocket);
	Weapon->GetThirdPersonMesh()->AttachToComponent(GetMesh(), AttachmentRule, ThirdPersonWeaponSocket);
	
}

void AShooterCharacter::PlayFiringMontage(UAnimMontage* Montage)
{
	S07Presentation->NotifyShot();
}

void AShooterCharacter::AddWeaponRecoil(float Recoil)
{
	// apply the recoil as pitch input
	AddControllerPitchInput(Recoil);
}

void AShooterCharacter::UpdateWeaponHUD(int32 CurrentAmmo, int32 MagazineSize)
{
	OnBulletCountUpdated.Broadcast(MagazineSize, CurrentAmmo);
}

bool AShooterCharacter::TryGetWeaponMuzzleLocation(FVector& Out) const
{
	return S07Presentation->TryGetMuzzleLocation(Out);
}

FVector AShooterCharacter::GetWeaponTargetLocation()
{
	// trace ahead from the camera viewpoint
	FHitResult OutHit;

	const FVector Start = GetFirstPersonCameraComponent()->GetComponentLocation();
	const FVector End = Start + (GetFirstPersonCameraComponent()->GetForwardVector() * MaxAimDistance);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

	// return either the impact point or the trace end
	return OutHit.bBlockingHit ? OutHit.ImpactPoint : OutHit.TraceEnd;
}

void AShooterCharacter::AddWeaponClass(const TSubclassOf<AShooterWeapon>& WeaponClass)
{
	// do we already own this weapon?
	AShooterWeapon* OwnedWeapon = FindWeaponOfType(WeaponClass);

	if (!OwnedWeapon)
	{
		// spawn the new weapon
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

		AShooterWeapon* AddedWeapon = GetWorld()->SpawnActor<AShooterWeapon>(WeaponClass, GetActorTransform(), SpawnParams);

		if (AddedWeapon)
		{
			// add the weapon to the owned list
			OwnedWeapons.Add(AddedWeapon);

			// if we have an existing weapon, deactivate it
			if (CurrentWeapon)
			{
				CurrentWeapon->DeactivateWeapon();
			}

			// switch to the new weapon
			CurrentWeapon = AddedWeapon;
			CurrentWeapon->ActivateWeapon(PlayerTag);
		}
	}
}

void AShooterCharacter::OnWeaponActivated(AShooterWeapon* Weapon)
{
	// update the bullet counter
	OnBulletCountUpdated.Broadcast(Weapon->GetMagazineSize(), Weapon->GetBulletCount());

	// Preserve the camera anchor pose while the katana supplies its own first-person mesh.
	if (Weapon->IsA(ACityKatana::StaticClass())) return;
	// set the character mesh AnimInstances
	GetFirstPersonMesh()->SetAnimInstanceClass(Weapon->GetFirstPersonAnimInstanceClass());
	GetMesh()->SetAnimInstanceClass(Weapon->GetThirdPersonAnimInstanceClass());
	if (Weapon->IsA(StartingWeapon.Get()))
	{
		Weapon->EnableManualReload();
		S07Presentation->Equip(Weapon);
	}
}

void AShooterCharacter::OnWeaponDeactivated(AShooterWeapon* Weapon)
{
	EndCityAim();
	S07Presentation->Unequip(Weapon);
}

void AShooterCharacter::OnSemiWeaponRefire()
{
	// unused
}

AShooterWeapon* AShooterCharacter::FindWeaponOfType(TSubclassOf<AShooterWeapon> WeaponClass) const
{
	// check each owned weapon
	for (AShooterWeapon* Weapon : OwnedWeapons)
	{
		if (Weapon->IsA(WeaponClass))
		{
			return Weapon;
		}
	}

	// weapon not found
	return nullptr;

}

void AShooterCharacter::Die()
{
	// deactivate the weapon
	if (IsValid(CurrentWeapon))
	{
		CurrentWeapon->DeactivateWeapon();
	}

	// increment the team score
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->IncrementTeamScore(TeamByte);
	}

	// grant the death tag to the character
	Tags.Add(DeathTag);
		
	// stop character movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// disable controls
	DisableInput(nullptr);

	// reset the bullet counter UI
	OnBulletCountUpdated.Broadcast(0, 0);

	// call the BP handler
	BP_OnDeath();

	// destroy all owned weapons
	for (AShooterWeapon* Current : OwnedWeapons)
	{
		if (IsValid(Current))
		{
			Current->Destroy();
		}
	}

	// empty the array and current weapon
	OwnedWeapons.Empty();
	CurrentWeapon = nullptr;

	// schedule character respawn
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AShooterCharacter::OnRespawn, RespawnTime, false);
}

void AShooterCharacter::OnRespawn()
{
	// destroy the character to force the PC to respawn
	Destroy();
}

bool AShooterCharacter::IsDead() const
{
	// the character is dead if their current HP drops to zero
	return CurrentHP <= 0.0f;
}

void AShooterCharacter::SetTeam(uint8 Team)
{
	TeamByte = Team;
}

void AShooterCharacter::CancelCityInput()
{
    EndCityAim();
    if(CurrentWeapon) CurrentWeapon->CancelTrigger();
    if(auto* Blade=Cast<ACityKatana>(CurrentWeapon)) Blade->CancelAttackInput();
}
void AShooterCharacter::ToggleCityView()
{
    if (!IsDead() && CurrentWeapon && !CurrentWeapon->IsA<ACityKatana>() && CityAbilities->GetCastingId().IsNone())
        S07Presentation->ToggleShoulder();
}
