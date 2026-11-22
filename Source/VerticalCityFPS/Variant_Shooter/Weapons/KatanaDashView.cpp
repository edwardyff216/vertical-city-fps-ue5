#include "KatanaDashView.h"
#include "ShooterCharacter.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float DrawSeconds = .095f;
    constexpr float PullbackSeconds = .36f;
    constexpr float ReturnSeconds = .27f;
    constexpr float MinimumClearance = 95.f;
}

UKatanaDashView::UKatanaDashView()
{
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/VerticalCity/S07/SK_S07"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Blade(TEXT("/Game/VerticalCity/Weapons/Katana/SM_S07WorldKatana"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Windup(TEXT("/Game/VerticalCity/S07/A_S07_KatanaDraw"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Rush(TEXT("/Game/VerticalCity/S07/A_S07_KatanaTravel"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> Finish(TEXT("/Game/VerticalCity/S07/A_S07_KatanaRecover"));
    BodyAsset=Mesh.Object; SwordAsset=Blade.Object;
    Draw=Windup.Object; Travel=Rush.Object; Recovery=Finish.Object;
}

bool UKatanaDashView::Prepare()
{
    BodyAsset=Player->ResolveCityBody();
    if(Body) { if(Body->GetSkeletalMeshAsset()!=BodyAsset) Body->SetSkeletalMeshAsset(BodyAsset); return BodyAsset!=nullptr; }
    if (!BodyAsset || !SwordAsset || !Draw || !Travel || !Recovery) return false;
    const FReferenceSkeleton& Ref = BodyAsset->GetRefSkeleton();
    const int32 Index = Ref.FindBoneIndex(TEXT("RightHandIndex1"));
    const int32 Pinky = Ref.FindBoneIndex(TEXT("RightHandPinky1"));
    const FName Hand(TEXT("RightHand"));
    const int32 Wrist = Ref.FindBoneIndex(Hand);
    if (Index<0 || Pinky<0 || Wrist<0 || Ref.GetParentIndex(Index)!=Wrist || Ref.GetParentIndex(Pinky)!=Wrist)
        return false;
    Body=NewObject<USkeletalMeshComponent>(GetOwner(),TEXT("DashS07Body"));
    Body->SetupAttachment(Player->GetCapsuleComponent());
    Body->SetSkeletalMeshAsset(BodyAsset);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetGenerateOverlapEvents(false);
    Body->SetCanEverAffectNavigation(false);
    Body->SetOnlyOwnerSee(true);
    Body->SetVisibility(false);
    Body->RegisterComponent();

    Sword=NewObject<UStaticMeshComponent>(GetOwner(),TEXT("DashS07Blade"));
    Sword->SetupAttachment(Body,Hand);
    Sword->SetStaticMesh(SwordAsset);
    Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sword->SetGenerateOverlapEvents(false);
    Sword->SetCanEverAffectNavigation(false);
    Sword->SetOnlyOwnerSee(true);
    Sword->SetVisibility(false);
    // Grip mid-point sits in the palm. The blade's imported tip points along local -Y.
    const auto& Pose=Ref.GetRefBonePose();
    const FVector IndexBase=Pose[Index].GetLocation(), PinkyBase=Pose[Pinky].GetLocation();
    const FVector TowardTip=(IndexBase-PinkyBase).GetSafeNormal();
    const FVector Palm=(IndexBase+PinkyBase)*.5f;
    Sword->SetRelativeLocationAndRotation(Palm+TowardTip*14.f,
        FRotationMatrix::MakeFromYZ(-TowardTip,Palm.GetSafeNormal()).ToQuat());
    Sword->SetRelativeScale3D(FVector(.78f));
    Sword->RegisterComponent();
    return true;
}

void UKatanaDashView::Play(UAnimSequence* Clip, float Rate)
{
    if (Playing==Clip) return;
    Playing=Clip;
    Body->PlayAnimation(Clip,Clip==Travel);
    Body->SetPlayRate(Rate);
}

void UKatanaDashView::Begin(AShooterCharacter* Shooter,UStaticMeshComponent* FirstPersonBlade)
{
    if (!Shooter || !Shooter->IsLocallyControlled()) return;
    Player=Shooter;
    if (!Prepare()) return;
    FirstBlade=FirstPersonBlade;
    bActive=true;
    StartTime=GetWorld()->GetTimeSeconds();
    RecoveryStart=-1;
    ArmLimit=350.f;
    Body->SetRelativeLocationAndRotation(FVector(0,0,-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),FRotator(0,-90,0));
    Body->SetAbsolute(false,true,false);
    Body->SetWorldRotation(FRotator(0,Player->GetActorRotation().Yaw-90.f,0));
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Play(Draw,Draw->GetPlayLength()/DrawSeconds);
}

void UKatanaDashView::Recover()
{
    if (!bActive) return;
    RecoveryStart=GetWorld()->GetTimeSeconds();
    Play(Recovery,1.f);
}

void UKatanaDashView::SetFraming(bool ShowBody)
{
    Body->SetVisibility(ShowBody);
    Sword->SetVisibility(ShowBody);
    if (FirstBlade) FirstBlade->SetVisibility(!ShowBody,true);
}

void UKatanaDashView::Reset()
{
    if (!bActive) return;
    SetFraming(false);
    Body->Stop();
    Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Playing=nullptr;
    bActive=false;
    FirstBlade=nullptr;
}

void UKatanaDashView::EndPlay(const EEndPlayReason::Type Reason)
{
    Reset();
    Super::EndPlay(Reason);
}

void UKatanaDashView::Frame(float Delta,FMinimalViewInfo& View)
{
    if (!bActive) return;
    const double Now = GetWorld()->GetTimeSeconds();
    const float Age = Now-StartTime;
    float Blend=FMath::SmoothStep(0.f,PullbackSeconds,Age);
    if (RecoveryStart>=0)
    {
        const float Left=Recovery->GetPlayLength()-(Now-RecoveryStart);
        if (Left<=0.f) { Reset(); return; }
        Blend*=FMath::SmoothStep(0.f,ReturnSeconds,Left);
    }
    else if (Age>=DrawSeconds) Play(Travel,1.f);

    const FVector Eye=View.Location;
    // Use actual eye aim for combat. Only this render viewpoint is displaced.
    const FRotator BoomRotation(FMath::ClampAngle(View.Rotation.Pitch,-32.f,32.f),View.Rotation.Yaw,0.f);
    const FVector Desired=BoomRotation.RotateVector(FVector(-305,85,36));
    const FVector Dir=Desired.GetSafeNormal();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(KatanaCamera),false,Player);
    Params.AddIgnoredActor(GetOwner());
    FHitResult Hit;
    FCollisionObjectQueryParams Obstacles;
    Obstacles.AddObjectTypesToQuery(ECC_WorldStatic);
    Obstacles.AddObjectTypesToQuery(ECC_WorldDynamic);
    Obstacles.AddObjectTypesToQuery(ECC_PhysicsBody);
    const bool Blocked=GetWorld()->SweepSingleByObjectType(Hit,Eye,Eye+Desired,FQuat::Identity,
        Obstacles,FCollisionShape::MakeSphere(18.f),Params);
    const float Safe=Blocked ? FMath::Max(0.f,static_cast<float>(Hit.Distance)-4.f) : Desired.Size();
    ArmLimit=FMath::Min(Safe,ArmLimit+FMath::Max(0.f,Delta)*1350.f);
    const float Offset=ArmLimit*Blend;
    if (Safe<MinimumClearance || Offset<1.f) { SetFraming(false); return; }

    View.Location=Eye+Dir*Offset;
    const FVector Focus=Eye+BoomRotation.RotateVector(FVector(235,0,-52));
    const FRotator WideAim=(Focus-View.Location).Rotation();
    View.Rotation=FQuat::Slerp(View.Rotation.Quaternion(),WideAim.Quaternion(),Blend).Rotator();
    SetFraming(Offset>60.f);
}
