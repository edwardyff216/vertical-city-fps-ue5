// Copyright Epic Games, Inc. All Rights Reserved.


#include "ShooterWeapon.h"
#include "Campaign/CityCampaignEnemy.h"
#include "ShooterCharacter.h"
#include "S07RiflePresentation.h"
#include "ShooterNPC.h"
#include "Abilities/CityAbilities.h"
#include "Progression/CityProgression.h"
#include "CityWeaponEffects.h"
#include "Abilities/CitySkillVisuals.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "ShooterProjectile.h"
#include "ShooterWeaponHolder.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

AShooterWeapon::AShooterWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the first person mesh
	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("First Person Mesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);

	FirstPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	FirstPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonMesh->bOnlyOwnerSee = true;

	// create the third person mesh
	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Third Person Mesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ThirdPersonMesh->SetCollisionProfileName(FName("NoCollision"));
	ThirdPersonMesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::WorldSpaceRepresentation);
	ThirdPersonMesh->bOwnerNoSee = true;
}

void AShooterWeapon::BeginPlay()
{
	Super::BeginPlay();

	// subscribe to the owner's destroyed delegate
	GetOwner()->OnDestroyed.AddDynamic(this, &AShooterWeapon::OnOwnerDestroyed);

	// cast the weapon owner
	WeaponOwner = Cast<IShooterWeaponHolder>(GetOwner());
	PawnOwner = Cast<APawn>(GetOwner());

	// fill the first ammo clip
	CurrentBullets = MagazineSize;

	// attach the meshes to the owner
	WeaponOwner->AttachWeaponMeshes(this);
}

void AShooterWeapon::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	CancelReload();
    GetWorldTimerManager().ClearTimer(BurstTimer);
	GetWorldTimerManager().ClearTimer(RefireTimer);
	Super::EndPlay(EndPlayReason);
}

void AShooterWeapon::OnOwnerDestroyed(AActor* DestroyedActor)
{
	// ensure this weapon is destroyed when the owner is destroyed
	Destroy();
}

void AShooterWeapon::ActivateWeapon(const FName& OwnerTag)
{
	// save the owner tag for perception noise detection
	NoiseOwnerTag = OwnerTag;

	// unhide this weapon
	SetActorHiddenInGame(false);

	// notify the owner
	WeaponOwner->OnWeaponActivated(this);
}

void AShooterWeapon::DeactivateWeapon()
{
	// ensure we're no longer firing this weapon while deactivated
	StopFiring();
	CancelReload();

	BurstRemaining=0;
    GetWorldTimerManager().ClearTimer(BurstTimer);
	// hide the weapon
	SetActorHiddenInGame(true);

	// notify the owner
	WeaponOwner->OnWeaponDeactivated(this);
}

bool AShooterWeapon::StartReload()
{
	if (!bManualReloadEnabled || IsHidden() || IsReloading() || CurrentBullets >= MagazineSize || (!ItemId.IsEmpty() && Reserve<=0))
	{
		return false;
	}

	StopFiring();
    BurstRemaining=0;
    GetWorldTimerManager().ClearTimer(BurstTimer);
    if (!ItemId.IsEmpty()) {
        auto* Shooter=CastChecked<AShooterCharacter>(PawnOwner);
        UCityWeaponEffects::Sound(this,TEXT("reload"));
        ReloadDuration=BaseReloadSeconds*(1.f-Shooter->CityAffix(CurrentBullets==0 ? TEXT("empty_reload") : TEXT("tactical_reload")));
    }
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AShooterWeapon::FinishReload, ReloadDuration, false);
	return true;
}

void AShooterWeapon::CancelReload()
{
	GetWorldTimerManager().ClearTimer(ReloadTimer);
}

bool AShooterWeapon::IsReloading() const
{
	return GetWorldTimerManager().IsTimerActive(ReloadTimer);
}

float AShooterWeapon::GetReloadProgress() const
{
	return FMath::Clamp(GetWorldTimerManager().GetTimerElapsed(ReloadTimer) / ReloadDuration, 0.0f, 1.0f);
}

void AShooterWeapon::FinishReload()
{
	CancelReload();
    if (!ItemId.IsEmpty())
    {
        RefillCityMagazine(CityKind==TEXT("scout")?2:MAX_int32);
        CastChecked<AShooterCharacter>(PawnOwner)->TriggerCityAction(TEXT("reload"));
        if (CityKind==TEXT("scout") && Reserve>0 && CurrentBullets<MagazineSize) StartReload();
        return;
    }
	CurrentBullets = MagazineSize;
	WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
}

void AShooterWeapon::StartFiring()
{
    if (!ItemId.IsEmpty()) {
        if (BurstRemaining>0) return;
        if (IsReloading() && CurrentBullets>0 && CityKind==TEXT("scout")) CancelReload();
        if (!CurrentBullets) { StartReload(); return; }
    }
	if (bManualReloadEnabled && (IsReloading() || CurrentBullets <= 0))
	{
		return;
	}

	// raise the firing flag
	bIsFiring = true;

	// check how much time has passed since we last shot
	// this may be under the refire rate if the weapon shoots slow enough and the player is spamming the trigger
	const float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - TimeOfLastShot;

	if (TimeSinceLastShot >= CityInterval())
	{
		// fire the weapon right away
		Fire();

	} else {

		// if we're full auto, schedule the next shot
		if (bFullAuto)
		{
			GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, FMath::Max(.001f, CityInterval()-TimeSinceLastShot), false);
		}

	}
}

void AShooterWeapon::StopFiring()
{
	// lower the firing flag
	bIsFiring = false;

	// clear the refire timer
	GetWorld()->GetTimerManager().ClearTimer(RefireTimer);
}

void AShooterWeapon::Fire()
{
	// ensure the player still wants to fire. They may have let go of the trigger
	if (!bIsFiring || (bManualReloadEnabled && (IsReloading() || CurrentBullets <= 0)))
	{
		return;
	}
	
	// fire a projectile at the target
	if (ItemId.IsEmpty()) FireProjectile(WeaponOwner->GetWeaponTargetLocation());
    else
    {
        BurstRemaining=CityKind==TEXT("pulse") ? FMath::Min(3,CurrentBullets) : 1;
        CityRound();
    }

	// update the time of our last shot
	TimeOfLastShot = GetWorld()->GetTimeSeconds();

	// make noise so the AI perception system can hear us
	MakeNoise(ShotLoudness, PawnOwner, PawnOwner->GetActorLocation(), ShotNoiseRange, NoiseOwnerTag);

	// are we full auto?
	if (bFullAuto)
	{
		// schedule the next shot
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::Fire, CityInterval(), false);
	} else {

		// for semi-auto weapons, schedule the cooldown notification
		GetWorld()->GetTimerManager().SetTimer(RefireTimer, this, &AShooterWeapon::FireCooldownExpired, CityInterval(), false);

	}
}

void AShooterWeapon::FireCooldownExpired()
{
	// notify the owner
	WeaponOwner->OnSemiWeaponRefire();
}

void AShooterWeapon::FireProjectile(const FVector& TargetLocation)
{
	// get the projectile transform
	FTransform ProjectileTransform = CalculateProjectileSpawnTransform(TargetLocation);
	
	// spawn the projectile
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = PawnOwner;

	AShooterProjectile* Projectile = GetWorld()->SpawnActor<AShooterProjectile>(ProjectileClass, ProjectileTransform, SpawnParams);

	// set the noise tag on the projectile
	if (Projectile)
	{
		Projectile->SetNoiseTag(NoiseOwnerTag);
	}

	// play the firing montage
	WeaponOwner->PlayFiringMontage(FiringMontage);

	// add recoil
	WeaponOwner->AddWeaponRecoil(FiringRecoil);

	// consume bullets
	--CurrentBullets;

	// Template weapons automatically refill. The player rifle waits for a completed reload.
	if (!bManualReloadEnabled && CurrentBullets <= 0)
	{
		CurrentBullets = MagazineSize;
	}

	// update the weapon HUD
	WeaponOwner->UpdateWeaponHUD(CurrentBullets, MagazineSize);
}

FTransform AShooterWeapon::CalculateProjectileSpawnTransform(const FVector& TargetLocation) const
{
	// find the muzzle location
	FVector MuzzleLoc;
	if (!WeaponOwner->TryGetWeaponMuzzleLocation(MuzzleLoc))
	{
		MuzzleLoc = FirstPersonMesh->GetSocketLocation(MuzzleSocketName);
	}

	// calculate the spawn location ahead of the muzzle
	const FVector SpawnLoc = MuzzleLoc + ((TargetLocation - MuzzleLoc).GetSafeNormal() * MuzzleOffset);

	// find the aim rotation vector while applying some variance to the target 
	const FRotator AimRot = UKismetMathLibrary::FindLookAtRotation(SpawnLoc, TargetLocation + (UKismetMathLibrary::RandomUnitVector() * AimVariance));

	// return the built transform
	return FTransform(AimRot, SpawnLoc, FVector::OneVector);
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetFirstPersonAnimInstanceClass() const
{
	return FirstPersonAnimInstanceClass;
}

const TSubclassOf<UAnimInstance>& AShooterWeapon::GetThirdPersonAnimInstanceClass() const
{
	return ThirdPersonAnimInstanceClass;
}
void AShooterWeapon::ConfigureCity(const FCityItem& Item,const FCityWeaponStats& Stats)
{
    const bool Changed=Item.Id!=ItemId;
    if (Changed && !ItemId.IsEmpty())
    {
        StopFiring();
        CancelReload();
        GetWorldTimerManager().ClearTimer(BurstTimer);
        BurstRemaining=0;
        AmmoByItem.Add(ItemId,FIntPoint(CurrentBullets,Reserve));
    }
    ItemId=Item.Id;
    CityKind=Item.Kind;
    TArray<FName> NewSchools;
    for (FName Id : Item.Affixes)
    {
        const auto* Entry = FCityItemCatalog::Affix(Id);
        if (Entry && !Entry->School.IsNone()) NewSchools.AddUnique(Entry->School);
        else if (Id == "burn") NewSchools.AddUnique(TEXT("fire"));
    }
    if (Changed || TrailSchools != NewSchools)
    {
        TrailSchools = MoveTemp(NewSchools);
        NextTrailSchool = 0;
    }
    MagazineSize=Stats.Magazine;
    MaxReserve=Stats.Reserve;
    GunDamage=Stats.Damage;
    GunImpact=Stats.Impact;
    Spread=Stats.Spread;
    Range=Stats.RangeMetres*100.f;
    Pellets=Stats.Pellets;
    RefireRate=Stats.Interval;
    BaseReloadSeconds=Stats.Reload;
    bManualReloadEnabled=true;
    bFullAuto=CityKind==TEXT("rifle") || CityKind==TEXT("smg") || CityKind==TEXT("shotgun");
    if (Changed)
    {
        FIntPoint Ammo=AmmoByItem.Contains(ItemId)?AmmoByItem[ItemId]:FIntPoint(MagazineSize,MaxReserve);
        CurrentBullets=FMath::Min(Ammo.X,MagazineSize);
        Reserve=FMath::Min(Ammo.Y,MaxReserve);
    }
    WeaponOwner->UpdateWeaponHUD(CurrentBullets,MagazineSize);
}
void AShooterWeapon::RefillCityMagazine(int32 Rounds)
{
    if (ItemId.IsEmpty()) return;
    int32 Transfer=FMath::Max(0,FMath::Min3(Rounds,MagazineSize-CurrentBullets,Reserve));
    CurrentBullets+=Transfer;
    Reserve-=Transfer;
    WeaponOwner->UpdateWeaponHUD(CurrentBullets,MagazineSize);
}
void AShooterWeapon::GiveReserve(float Fraction)
{
    if (!ItemId.IsEmpty()) Reserve=FMath::Min(MaxReserve,Reserve+FMath::CeilToInt(MaxReserve*Fraction*(1.f+CastChecked<AShooterCharacter>(PawnOwner)->CityAffix(TEXT("scavenger")))));
}
float AShooterWeapon::CityInterval() const
{
    auto* P=Cast<AShooterCharacter>(PawnOwner);
    return (!ItemId.IsEmpty() && P)?FMath::Max(.02f,P->GetCityAbilities()->ModifyFireInterval(RefireRate)):RefireRate;
}
void AShooterWeapon::CityRound()
{
    auto* P=Cast<AShooterCharacter>(PawnOwner);
    if (!P || P->IsDead() || IsHidden() || IsReloading() || CurrentBullets<=0 || BurstRemaining<=0) { BurstRemaining=0; return; }
    --BurstRemaining;
    --CurrentBullets;
    auto* Skills=P->GetCityAbilities();
    Skills->OnGunShot();
    UCityWeaponEffects::Sound(this,CityKind);
    auto* Eye=P->GetFirstPersonCameraComponent();
    const FVector Start=Eye->GetComponentLocation();
    FVector Muzzle=Start;
    P->TryGetWeaponMuzzleLocation(Muzzle);
    if (FVector::DistSquared(Muzzle,Start)<100) Muzzle=Start+Eye->GetForwardVector()*72+Eye->GetRightVector()*18-FVector::UpVector*13;
    auto* Rig = P->FindComponentByClass<US07RiflePresentation>();
    if (!Rig || !Rig->PlayMuzzleFlash(CityKind, Eye->GetComponentRotation()))
        UCitySkillVisuals::Emit(P,CityKind=="shotgun" ? FName("muzzle_shotgun") : FName("muzzle_rifle"),Muzzle,CityKind=="shotgun"?40.f:27.f);
    const float Opening=CurrentBullets==MagazineSize-1 ? P->CityAffix(TEXT("first_round")) : 0.f;
    const float Closing=CurrentBullets==0 ? P->CityAffix(TEXT("last_round")) : 0.f;
    const float Mult=P->ConsumeRiposte()*(1.f+Opening)*(1.f+Closing);
    TMap<AShooterNPC*,float> Hits,Impacts;
    TMap<AShooterNPC*,FHitResult> Contacts;
    const FName TrailSchool = TrailSchools.IsEmpty() ? NAME_None : TrailSchools[NextTrailSchool];
    if (!TrailSchools.IsEmpty()) NextTrailSchool = (NextTrailSchool + 1) % TrailSchools.Num();
    auto* TrailArt = TrailSchool.IsNone() ? nullptr : P->FindComponentByClass<UCitySkillVisuals>();
    for (int32 Index=0;Index<Pellets;++Index)
    {
        FVector Dir=FMath::VRandCone(Eye->GetForwardVector(),Skills->ModifySpread(Spread));
        FVector End=Start+Dir*Range;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CityFire),false,P);
        Params.AddIgnoredActor(this);
        FHitResult Hit;
        bool Block=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params);
        FVector TrailEnd = Block ? Hit.ImpactPoint : End;
        if (!Skills->OnGunRay(Start, TrailEnd, &TrailEnd))
        {
            if (auto* Enemy=Cast<AShooterNPC>(Hit.GetActor()))
            {
                float Factor=1.f;
                if (CityKind==TEXT("shotgun")) Factor=FMath::GetMappedRangeValueClamped(FVector2D(800,Range),FVector2D(1,.4),Hit.Distance);
                Impacts.FindOrAdd(Enemy)+=GunImpact*Factor;
                Contacts.FindOrAdd(Enemy)=Hit;

                Hits.FindOrAdd(Enemy)+=GunDamage*Factor*Mult;
                if (Skills->HasOverclockPenetration() || P->CityAffix(TEXT("pierce"))>0)
                {
                    Params.AddIgnoredActor(Enemy);
                    FHitResult Second;
                    if (GetWorld()->LineTraceSingleByChannel(Second,Hit.ImpactPoint+Dir,End,ECC_Visibility,Params))
                        if(auto* Behind=Cast<AShooterNPC>(Second.GetActor())) { TrailEnd=Second.ImpactPoint; Hits.FindOrAdd(Behind)+=GunDamage*Mult*(Skills->HasOverclockPenetration()?1.f:P->CityAffix(TEXT("pierce"))); Contacts.FindOrAdd(Behind)=Second; Impacts.FindOrAdd(Behind)+=GunImpact*Factor; }
                }
            }
        }
        if (Index < 3 && !(TrailArt && TrailArt->FireTrail(TrailSchool, Muzzle, TrailEnd)))
        {
            const FVector Ray = (Block ? Hit.ImpactPoint : End) - Muzzle;
            const float Length = Ray.Size();
            const float Lead = FMath::Clamp(Length * .18f, 110.f, 400.f);
            if (Length > Lead)
            {
                const FVector Axis = Ray / Length;
                const FVector TraceStart = Muzzle + Axis * Lead;
                UCityWeaponEffects::Beam(GetWorld(), TraceStart, TraceStart + Axis * FMath::Min(Length - Lead, 180.f),
                    FLinearColor(1.f, .59f, .12f), .7f, .042f);
            }
        }
    }
    CurrentBullets=FMath::Min(MagazineSize,CurrentBullets+P->ConsumeSlideRefund(!Hits.IsEmpty()));
    for(const auto& Pair:Hits) P->HitCityTarget(Pair.Key,Pair.Value,false,Contacts.Find(Pair.Key),Impacts.FindRef(Pair.Key));
    WeaponOwner->PlayFiringMontage(FiringMontage);
    WeaponOwner->AddWeaponRecoil(FiringRecoil*(CityKind==TEXT("smg")?.45f:1.f));
    WeaponOwner->UpdateWeaponHUD(CurrentBullets,MagazineSize);
    TimeOfLastShot=GetWorld()->GetTimeSeconds();
    if (BurstRemaining>0)
        GetWorldTimerManager().SetTimer(BurstTimer,this,&AShooterWeapon::CityRound,CityInterval()*.22f,false);
}
void AShooterWeapon::CancelTrigger() {
    StopFiring();
    BurstRemaining=0;
    GetWorldTimerManager().ClearTimer(BurstTimer);
}
