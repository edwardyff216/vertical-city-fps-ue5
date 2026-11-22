#include "CityKatana.h"
#include "CityAftercutDamage.h"
#include "NiagaraComponent.h"
#include "Curves/CurveVector.h"
#include "Progression/CityProgression.h"
#include "KatanaDashView.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"
#include "ShooterPlayerController.h"
#include "CityWeaponEffects.h"
#include "Abilities/CitySkillVisuals.h"
#include "Abilities/CityAbilities.h"
#include "CityLocomotion.h"
#include "Camera/CameraComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float Reach = 310.f, SlashInterval = .52f, Damage = 58.f;
    constexpr float DashLength = 600.f, DashSpeed = 2800.f, Cooldown = 2.5f;
    // Source frames in A_S07_TripleCut. Each strike uses the equipped blade's attack interval.
    struct FStroke { float First, Contact, Last; };
    constexpr FStroke Strokes[] = {{12,23,31}, {31,38,50}, {50,59,85}};
    constexpr float ChainGrace = .35f;
    constexpr float MarkRadius = 105.f, CutDelay = 1.f, BufferWindow = .16f;

    FVector SeamPoint(const AShooterNPC& Enemy, const FVector& View)
    {
        const FVector Chest=Enemy.GetActorLocation()+FVector::UpVector*23.f;
        return Chest+(View-Chest).GetSafeNormal()*(Enemy.GetCapsuleComponent()->GetScaledCapsuleRadius()+3.f);
    }
}

ACityKatana::ACityKatana()
{
    PrimaryActorTick.bTickEvenWhenPaused = true;
    MagazineSize = 0;
    DashView = CreateDefaultSubobject<UKatanaDashView>(TEXT("KatanaDashView"));
    Blade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KatanaView"));
    Blade->SetupAttachment(GetRootComponent());
    Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Blade->SetOnlyOwnerSee(true);
    Blade->SetCastShadow(false);
    Blade->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sword(TEXT("/Game/VerticalCity/Weapons/Katana/SM_S07WorldKatana.SM_S07WorldKatana"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Cut(TEXT("/Game/VerticalCity/VFX/T_KatanaPrismSeam.T_KatanaPrismSeam"));
    Blade->SetStaticMesh(Sword.Object);
    GripHand=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KatanaGripHand"));
    GripHand->SetupAttachment(Blade);
    GripHand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GripHand->SetCastShadow(false);
    GripHand->SetOnlyOwnerSee(true);
    GripHand->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> S07(TEXT("/Game/VerticalCity/Weapons/Katana/SM_S07_KatanaHand"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> White(TEXT("/Game/VerticalCity/Weapons/Katana/SM_White_KatanaHand"));
    S07Hand=S07.Object; WhiteHand=White.Object;
    GripHand->SetStaticMesh(S07Hand);
    CutTexture = Cut.Object;
    static ConstructorHelpers::FObjectFinder<UCurveVector> Grip(TEXT("/Game/VerticalCity/S07/Combat/CV_Katana_FP_Grip"));
    static ConstructorHelpers::FObjectFinder<UCurveVector> Edge(TEXT("/Game/VerticalCity/S07/Combat/CV_Katana_FP_Edge"));
    GripPath = Grip.Object;
    EdgePath = Edge.Object;
}

void ACityKatana::BeginPlay()
{
    Super::BeginPlay();
    Player = CastChecked<AShooterCharacter>(GetOwner());
    Blade->AttachToComponent(Player->GetFirstPersonCameraComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    // Blade origin is the guard, local +Y runs down the hilt. Keep the hand at character scale.
    const FVector BladeScale(.49f,.71f,.45f);
    Blade->SetRelativeScale3D(BladeScale);
    GripHand->SetRelativeLocation(FVector(0,14,0));
    GripHand->SetRelativeScale3D(FVector(1.f)/BladeScale);
    Blade->SetVisibility(false,true);
}

void ACityKatana::ActivateWeapon(const FName& Tag)
{
    Super::ActivateWeapon(Tag);
    bEquipped = true;
    const auto* Progress=Player->GetGameInstance()->GetSubsystem<UCityProgression>();
    GripHand->SetStaticMesh(Progress->GetProfile().Character==TEXT("white_01") ? WhiteHand : S07Hand);
    auto* Mesh = Player->GetFirstPersonMesh();
    bPreviousMeshVisible = Mesh->IsVisible();
    PreviousPoseTick = Mesh->VisibilityBasedAnimTickOption;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->SetVisibility(false);
    Blade->SetRelativeLocationAndRotation(FVector(59,18,-5), FRotator(34,22,-12).Quaternion()*FRotator(0,90,0).Quaternion());
    Blade->SetVisibility(true,true);
    SetActorTickEnabled(true);
}

void ACityKatana::StopBladeTrail()
{
    if (auto* Trail = ActiveTrail.Get()) Trail->DestroyComponent();
    ActiveTrail.Reset();
}

void ACityKatana::DeactivateWeapon()
{
    StopBladeTrail();
    CancelAttackInput();
    DashView->Reset();
    EndTravel(false);
    bBuffered = false;
    if (bEquipped)
    {
        Player->GetFirstPersonMesh()->SetVisibility(bPreviousMeshVisible);
        Player->GetFirstPersonMesh()->VisibilityBasedAnimTickOption = PreviousPoseTick;
    }
    bEquipped = false;
    Blade->SetVisibility(false,true);
    Super::DeactivateWeapon();
    // A cut armed at the end of travel survives switching back to the rifle.
    SetActorTickEnabled(CutAt >= 0);
}

void ACityKatana::EndPlay(EEndPlayReason::Type Reason)
{
    CancelAttackInput();
    StopBladeTrail();
    ClearMarks();
    Super::EndPlay(Reason);
}

bool ACityKatana::CanReach(const FVector& Origin, AShooterNPC* Target) const
{
    if (Target->CurrentHP <= 0 || Target->GetCombatTeam() == Player->GetCombatTeam()) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(KatanaOcclusion), false, Player);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(Target);
    FCollisionObjectQueryParams Solid;
    Solid.AddObjectTypesToQuery(ECC_WorldStatic);
    Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
    Solid.AddObjectTypesToQuery(ECC_PhysicsBody);
    return !GetWorld()->LineTraceTestByObjectType(Origin, Target->GetActorLocation(), Solid, Params);
}

void ACityKatana::StartFiring()
{
    if (!bEquipped || bTravel || bBuffered || Player->IsDead() || GetWorld()->IsPaused() ||
        !Player->GetCityAbilities()->GetCastingId().IsNone()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    bHeld = true;
    if (!IsPrimaryCutActive() && Now >= NextSlash) BeginStroke(Now);
    else if (Now < NextSlash && NextSlash-Now <= BufferWindow) bQueued = true;
}

void ACityKatana::StopFiring()
{
    bHeld = false;
    Super::StopFiring();
}

void ACityKatana::CancelAttackInput()
{
    bHeld = bQueued = bBuffered = bContactPending = false;
    StrokeStart = -1.;
    StrokeIndex = INDEX_NONE;
    StopBladeTrail();
}

void ACityKatana::BeginStroke(double Now)
{
    bLoopEntry = StrokeIndex==2 && Now>=NextSlash && Now-NextSlash<=GetWorld()->GetDeltaSeconds()+KINDA_SMALL_NUMBER;
    StrokeIndex = (StrokeIndex == INDEX_NONE || Now > NextSlash + ChainGrace)
        ? 0 : (StrokeIndex+1) % UE_ARRAY_COUNT(Strokes);
    StrokeDuration = Player->HasCampaignEquipment()
        ? Player->GetGameInstance()->GetSubsystem<UCityProgression>()->GetStats(Player->GetCitySlot()).Interval
        : SlashInterval;
    StrokeDuration = FMath::Max(.06f,StrokeDuration);
    UE_LOG(LogTemp,Verbose,TEXT("CITY_KATANA_SWING index=%d world=%.4f"),StrokeIndex+1,Now);
    EntryPose = Blade->GetRelativeTransform();
    StrokeStart = Now;
    NextSlash = Now + StrokeDuration;
    bContactPending = true;
    bQueued = false;
    if (auto* Art=Player->FindComponentByClass<UCitySkillVisuals>())
        ActiveTrail=Art->BladeTrail(Blade, true, StrokeDuration*.52f);
}

void ACityKatana::AdvanceStroke(double Now)
{
    if (!IsPrimaryCutActive()) return;
    const FStroke& Stroke = Strokes[StrokeIndex];
    const double Alpha = (Now-StrokeStart)/StrokeDuration;
    if (bContactPending && Alpha >= (Stroke.Contact-Stroke.First)/(Stroke.Last-Stroke.First))
    {
        bContactPending = false;
        ApplySlash();
    }
    if (Alpha >= 1.) StrokeStart = -1.;
}

void ACityKatana::ApplySlash()
{
    const double Now = GetWorld()->GetTimeSeconds();
    const auto Stats=Player->GetGameInstance()->GetSubsystem<UCityProgression>()->GetStats(Player->GetCitySlot());
    const float SlashReach=Player->HasCampaignEquipment()?Stats.RangeMetres*100.f:Reach;
    const float AttackDamage=(Player->HasCampaignEquipment()?Stats.Damage:Damage)*Player->ConsumeRiposte();
    SwingStart = Now;

    UCityWeaponEffects::Sound(this,TEXT("sword"));
    const auto* Camera = Player->GetFirstPersonCameraComponent();
    const FVector Eye = Camera->GetComponentLocation();
    auto* Art=Player->FindComponentByClass<UCitySkillVisuals>();
    TArray<FVector,TInlineAllocator<4>> Contacts;
    for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
    {
        const FVector ToEnemy = It->GetActorLocation() - Eye;
        if (ToEnemy.SizeSquared() <= SlashReach*SlashReach &&
            FVector::DotProduct(Camera->GetForwardVector(), ToEnemy.GetSafeNormal()) > FMath::Cos(FMath::Acos(.55f)+FMath::DegreesToRadians(Player->CityAffix(TEXT("wide_sweep")))) && CanReach(Eye,*It))
        {
            // Capture the point before damage can kill or displace the enemy.
            const FVector Chest=It->GetActorLocation()+FVector::UpVector*23.f;
            const FVector Contact=Chest+(Eye-Chest).GetSafeNormal()*(It->GetCapsuleComponent()->GetScaledCapsuleRadius()+3.f);
            if (Player->HitCityTarget(*It,AttackDamage,true)>0.f) Contacts.Add(Contact);
        }
    }
    if (Art) Art->SwingWave(Eye,Camera->GetForwardVector(),SlashReach,Contacts);
}

bool ACityKatana::CanDash() const
{
    return CanDashGround() && Marks.IsEmpty() && GetWorld()->GetTimeSeconds() >= NextDash;
}

bool ACityKatana::CanDashGround() const
{
    return bEquipped && !Player->IsDead() && !bTravel &&
        Player->GetCharacterMovement()->IsMovingOnGround() && !Player->IsCitySliding();
}

void ACityKatana::RequestDash()
{
    if (!CanDash()) return;
    const double Recovery = NextSlash - GetWorld()->GetTimeSeconds();
    if (Recovery <= 0) StartDash();
    else if (Recovery <= BufferWindow) bBuffered = true;
}

void ACityKatana::StartDash()
{
    CancelAttackInput();
    bTravel = true;
    Heading = Player->GetActorForwardVector().GetSafeNormal2D();
    Remaining = DashLength;
    NextDash = GetWorld()->GetTimeSeconds() + Cooldown*(1.f-Player->CityAffix(TEXT("dash_cd")));
    Player->TriggerCityAction(TEXT("dash"));
    Player->ConsumeMovementInputVector();
    Player->GetCharacterMovement()->DisableMovement();
    DashView->Begin(Player,Blade);
    if (auto* Art=Player->FindComponentByClass<UCitySkillVisuals>())
    {
        StopBladeTrail();
        auto* ThirdPersonBlade = DashView->GetDashBlade();
        ActiveTrail = Art->BladeTrail(ThirdPersonBlade ? ThirdPersonBlade : Blade.Get(), !ThirdPersonBlade, DashLength / DashSpeed);
    }
}

void ACityKatana::TraceMarks(const FVector& From, const FVector& To)
{
    if (From.Equals(To)) return;
    for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
    {
        AShooterNPC* Enemy = *It;
        const FVector Center = Enemy->GetActorLocation();
        const FVector Nearest = FMath::ClosestPointOnSegment(Center, From, To);
        if (Marks.Contains(Enemy) || FVector::DistSquared(Nearest,Center)>MarkRadius*MarkRadius || !CanReach(Nearest,Enemy)) continue;
        UBillboardComponent* Seam=nullptr;
        if (CutTexture && FApp::CanEverRender())
        {
            Seam=NewObject<UBillboardComponent>(Enemy);
            Seam->SetupAttachment(Enemy->GetRootComponent());
            Seam->SetHiddenInGame(false);
            Seam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Seam->SetCastShadow(false);
            Seam->SetSprite(CutTexture);
            Seam->SetAbsolute(false,false,true);
            // Billboard renders a full width of half the source pixel width at unit scale.
            Seam->SetWorldScale3D(FVector(2.f*55.f/CutTexture->GetSizeX()));
            Seam->RegisterComponent();
        }
        if (Marks.IsEmpty())
            if (auto* PC=Cast<AShooterPlayerController>(Player->GetController())) PC->ShowKatanaPass();
        const double Now=GetWorld()->GetTimeSeconds();
        Marks.Add(Enemy,FDashMark{Seam,Now+CutDelay});
        UE_LOG(LogTemp,Verbose,TEXT("CITY_DASH_MARK target=%s pass=%.4f due=%.4f hp=%.1f"),*Enemy->GetName(),Now,Now+CutDelay,Enemy->CurrentHP);
    }
}

void ACityKatana::StepDash(float Delta)
{
    const FVector From = Player->GetActorLocation();
    UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
    // Ignore only enemy actors for this sweep. Walls and moving obstacles still block.
    TArray<AShooterNPC*, TInlineAllocator<16>> Ignored;
    for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
    {
        if (It->GetCombatTeam()!=Player->GetCombatTeam() && !Capsule->GetMoveIgnoreActors().Contains(*It))
        {
            Capsule->IgnoreActorWhenMoving(*It,true);
            Ignored.Add(*It);
        }
    }
    FHitResult Block;
    Player->AddActorWorldOffset(Heading*FMath::Min(Remaining,DashSpeed*Delta), true, &Block);
    for (AShooterNPC* Enemy : Ignored) Capsule->IgnoreActorWhenMoving(Enemy,false);
    const FVector To = Player->GetActorLocation();
    TraceMarks(From,To);
    Remaining -= FVector::Dist2D(From,To);
    if (Block.bBlockingHit || Remaining < .05f) EndTravel(true);
}

void ACityKatana::EndTravel(bool bArm)
{
    if (!bTravel) return;
    bTravel = false;
    if (!Player->IsDead())
    {
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Player->GetCharacterMovement()->StopMovementImmediately();
    }
    if (!bArm) { ClearMarks(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    SwingStart = Now;
    auto* Progress=Player->GetGameInstance()->GetSubsystem<UCityProgression>();
    ArmedItem=Player->HasCampaignEquipment()?Progress->GetWeapon(Player->GetCitySlot()):FCityItem{};
    const auto Stats=Progress->GetStats(Player->GetCitySlot());
    ArmedDamage=(Player->HasCampaignEquipment()?Stats.Damage:Damage)*Player->ConsumeRiposte()*Player->GetCityAbilities()->ConsumeWeaponBuffs(true);
    ArmedImpact=Player->HasCampaignEquipment()?Stats.Impact:0.f;
    NextSlash=Now+(Player->HasCampaignEquipment()?Stats.Interval:SlashInterval);
    CutAt=-1.;
    for (const auto& Pair : Marks)
        if (CutAt<0. || Pair.Value.Due<CutAt) CutAt=Pair.Value.Due;
    UE_LOG(LogTemp,Verbose,TEXT("CITY_VFX_DASH armed=%.3f due=%.3f marks=%d"),Now,CutAt,Marks.Num());
    DashView->Recover();
    if (auto* Art = Player->FindComponentByClass<UCitySkillVisuals>())
        Art->DashSlash(Player->GetActorLocation() + FVector::UpVector*30.f, Heading.Rotation());
}

void ACityKatana::StrikeMarks(double Now)
{
    auto* Art=Player->FindComponentByClass<UCitySkillVisuals>();
    auto* PC=Cast<APlayerController>(Player->GetController());
    FVector Eye=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
    FRotator Look;
    if (PC) PC->GetPlayerViewPoint(Eye,Look);
    CutAt=-1.;
    for (auto It=Marks.CreateIterator(); It; ++It)
    {
        const FDashMark Mark=It.Value();
        if (Mark.Due>Now+UE_KINDA_SMALL_NUMBER)
        {
            if (CutAt<0. || Mark.Due<CutAt) CutAt=Mark.Due;
            continue;
        }
        AShooterNPC* Enemy=It.Key().Get();
        It.RemoveCurrent();
        if (auto* Seam=Mark.Seam.Get()) Seam->DestroyComponent();
        if (!IsValid(Enemy) || Enemy->CurrentHP<=0.f || Enemy->GetCombatTeam()==Player->GetCombatTeam()) continue;

        const FVector Contact=SeamPoint(*Enemy,Eye);
        const FString TargetName=Enemy->GetName();
        const float Applied=Player->HitCityTarget(Enemy,ArmedDamage,true,nullptr,ArmedImpact,&ArmedItem,this,UCityAftercutDamage::StaticClass());
        if (Applied<=0.f) continue;
        if (Art) Art->PrismHit(Contact,(Contact-Eye).Rotation(),0.f,.95f);
        FVector2D Screen=FVector2D::ZeroVector;
        const bool InFront=PC && PC->ProjectWorldLocationToScreen(Contact,Screen,true);
        UE_LOG(LogTemp,Verbose,TEXT("CITY_DASH_RUPTURE target=%s time=%.4f damage=%.2f projected=%d screen=%.1f,%.1f"),
            *TargetName,Now,Applied,InFront,Screen.X,Screen.Y);
    }
}

void ACityKatana::FrameDashView(float Delta,FMinimalViewInfo& View)
{
    DashView->Frame(Delta,View);
}

void ACityKatana::ClearMarks()
{
    for (const auto& Pair : Marks)
        if (auto* Seam = Pair.Value.Seam.Get()) Seam->DestroyComponent();
    Marks.Reset();
    CutAt = -1;
}

void ACityKatana::PoseBlade(double Now)
{
    const FQuat GuardRotation = FRotator(bTravel ? 7.f : 34.f,22.f,-12.f).Quaternion()*FRotator(0,90,0).Quaternion();
    const FVector Guard(59,18,-5);
    if (IsPrimaryCutActive() && GripPath && EdgePath)
    {
        const FStroke& Stroke = Strokes[StrokeIndex];
        const float Alpha = FMath::Clamp(float((Now-StrokeStart)/StrokeDuration),0.f,1.f);
        const float Frame = FMath::Lerp(Stroke.First,Stroke.Last,Alpha);
        // Bridge the end of cut three to the moving start of cut one. Sample both
        // curves in source-frame space, with tangents taken from the adjoining motion.
        constexpr float TailStart=79.f, HeadEnd=16.f;
        constexpr float TailTime=(85.f-TailStart)/35.f, HeadTime=(HeadEnd-12.f)/19.f;
        const bool Tail=StrokeIndex==2 && Frame>=TailStart;
        const bool Head=bLoopEntry && StrokeIndex==0 && Frame<=HeadEnd;
        const float Phase=Tail ? (Frame-TailStart)/35.f : TailTime+(Frame-12.f)/19.f;
        const auto Sample=[&](const UCurveVector* Curve,float TangentLimit)
        {
            if (!Tail && !Head) return Curve->GetVectorValue(Frame);
            const FVector A=Curve->GetVectorValue(TailStart), B=Curve->GetVectorValue(HeadEnd);
            const FVector In=((A-Curve->GetVectorValue(TailStart-1))*(35.f*(TailTime+HeadTime))).GetClampedToMaxSize(TangentLimit);
            const FVector Out=((Curve->GetVectorValue(HeadEnd+1)-B)*(19.f*(TailTime+HeadTime))).GetClampedToMaxSize(TangentLimit);
            return FMath::CubicInterp(A,In,B,Out,FMath::Clamp(Phase/(TailTime+HeadTime),0.f,1.f));
        };
        const FVector Tip = Sample(EdgePath,1.65f).GetSafeNormal();
        const FQuat Orientation = FRotationMatrix::MakeFromYX(-Tip,FVector::RightVector).ToQuat();
        const float Enter = bLoopEntry ? 1.f : FMath::SmoothStep(0.f,.20f,Alpha);
        Blade->SetRelativeLocationAndRotation(FMath::Lerp(EntryPose.GetLocation(),Sample(GripPath,32.f),Enter),
            FQuat::Slerp(EntryPose.GetRotation(),Orientation,Enter));
        return;
    }
    const float Ease = 1.f-FMath::Exp(-GetWorld()->GetDeltaSeconds()*19.f);
    Blade->SetRelativeLocationAndRotation(FMath::Lerp(Blade->GetRelativeLocation(),Guard,Ease),
        FQuat::Slerp(Blade->GetRelativeRotation().Quaternion(),GuardRotation,Ease));
}

void ACityKatana::Tick(float Delta)
{
    Super::Tick(Delta);
    if (!Player || Player->IsDead()) { CancelAttackInput(); DashView->Reset(); ClearMarks(); return; }
    if (GetWorld()->IsPaused()) { CancelAttackInput(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (IsPrimaryCutActive() || bHeld || bQueued)
    {
        if (!Player->GetCityAbilities()->GetCastingId().IsNone()) CancelAttackInput();
        else
        {
            AdvanceStroke(Now);
            if (!bBuffered && !IsPrimaryCutActive() && (bHeld || bQueued) && Now >= NextSlash)
                BeginStroke(Now);
        }
    }
    if (bBuffered)
    {
        if (!CanDash()) bBuffered=false;
        else if (Now>=NextSlash) StartDash();
    }
    if (bTravel) StepDash(Delta);
    if (!Marks.IsEmpty())
    {
        FVector Eye=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
        FRotator Look;
        if (auto* PC=Cast<APlayerController>(Player->GetController())) PC->GetPlayerViewPoint(Eye,Look);
        for (const auto& Pair : Marks)
            if (auto* Enemy=Pair.Key.Get())
                if (auto* Seam=Pair.Value.Seam.Get()) Seam->SetWorldLocation(SeamPoint(*Enemy,Eye));
    }
    if (CutAt>=0 && Now+UE_KINDA_SMALL_NUMBER>=CutAt) StrikeMarks(Now);
    if (bEquipped) PoseBlade(Now);
    else if (CutAt<0) SetActorTickEnabled(false);
}
