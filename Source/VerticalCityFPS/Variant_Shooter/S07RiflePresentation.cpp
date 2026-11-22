#include "S07RiflePresentation.h"
#include "Progression/CityArsenalRow.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Camera/CameraComponent.h"
#include "CityLocomotion.h"
#include "Abilities/CitySkillVisuals.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Engine/CurveTable.h"

namespace
{
template<typename T>
T* LoadS07(const TCHAR* Name)
{
    return LoadObject<T>(nullptr, *FString::Printf(TEXT("/Game/VerticalCity/S07/%s.%s"), Name, Name));
}

template<typename T>
T* MakeViewMesh(AShooterCharacter* Character, FName Name)
{
    T* Part = NewObject<T>(Character, Name);
    Character->AddInstanceComponent(Part);
    Part->SetupAttachment(Character->GetRootComponent());
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->SetOnlyOwnerSee(true);
    Part->SetCastShadow(false);
    Part->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    Part->RegisterComponent();
    return Part;
}

void SampleClip(UAnimSequence* Clip, double Time, UPoseableMeshComponent* Body, TArray<FTransform>& Locals)
{
    FMemMark Mark(FMemStack::Get());
    FBoneContainer& Bones = Body->RequiredBones;
    FCompactPose Pose;
    Pose.SetBoneContainer(&Bones);
    FBlendedCurve Curve;
    Curve.InitFrom(Bones);
    UE::Anim::FStackAttributeContainer Attributes;
    FAnimationPoseData Sample(Pose, Curve, Attributes);
    Clip->GetAnimationPose(Sample, FAnimExtractContext(Time));
    Locals.SetNumUninitialized(Body->BoneSpaceTransforms.Num());
    for (FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
    {
        Locals[Bones.MakeMeshPoseIndex(Bone).GetInt()] = Pose[Bone];
    }
}

void BuildComponentPose(const FReferenceSkeleton& Skeleton, const TArray<FTransform>& Locals, TArray<FTransform>& Globals)
{
    Globals.SetNumUninitialized(Locals.Num());
    for (int32 Bone = 0; Bone < Locals.Num(); ++Bone)
    {
        const int32 Parent = Skeleton.GetParentIndex(Bone);
        Globals[Bone] = Parent == INDEX_NONE ? Locals[Bone] : Locals[Bone] * Globals[Parent];
    }
}

void RotateBone(const FReferenceSkeleton& Skeleton, TArray<FTransform>& Locals, TArray<FTransform>& Globals,
    int32 Bone, const FQuat& Rotation)
{
    const int32 Parent = Skeleton.GetParentIndex(Bone);
    Locals[Bone].SetRotation((Globals[Parent].GetRotation().Inverse() * Rotation).GetNormalized());
    Globals[Bone] = Locals[Bone] * Globals[Parent];
}

float Smooth(float From, float To, float Value)
{
    return FMath::SmoothStep(From, To, Value);
}


enum class EHandAction : uint8 { Equip, Fire, Reload };
struct FViewHandSample
{
    FVector Wrist = FVector::ZeroVector;
    FVector Support = FVector::ZeroVector;
    float Pitch = 0, Trigger = 0, Aperture = 0;
};

FViewHandSample ReadHandAction(const UCurveTable* Table, bool bPistol, EHandAction Action, float Phase)
{
    if (!Table) return {};
    // Cache row names, not curve pointers. A table can be reimported while PIE is running.
    struct FRows
    {
        FName Names[6][9];
        FRows()
        {
            int32 Index=0;
            for (const TCHAR* Family : {TEXT("Rifle"), TEXT("Pistol")})
                for (const TCHAR* Event : {TEXT("Equip"), TEXT("Fire"), TEXT("Reload")})
                {
                    int32 Channel=0;
                    for (const TCHAR* Suffix : {TEXT("Forward"), TEXT("Right"), TEXT("Up"), TEXT("Pitch"),
                        TEXT("SupportForward"), TEXT("SupportRight"), TEXT("SupportUp"), TEXT("Trigger"), TEXT("Aperture")})
                        Names[Index][Channel++]=FName(FString(Family)+Event+Suffix);
                    ++Index;
                }
        }
    };
    static const FRows Rows;
    const int32 Index=(bPistol ? 3 : 0)+static_cast<int32>(Action);
    float Value[9]{};
    for (int32 I=0; I<9; ++I)
        if (const auto* Curve=Table->FindCurve(Rows.Names[Index][I], TEXT("FirstPersonHands")))
            Value[I]=Curve->Eval(Phase);
    FViewHandSample Sample;
    Sample.Wrist=FVector(Value[0],Value[1],Value[2]);
    Sample.Support=FVector(Value[4],Value[5],Value[6]);
    Sample.Pitch=Value[3];
    Sample.Trigger=Value[7];
    Sample.Aperture=Value[8];
    return Sample;
}

// Anatomical palm contact points measured on the template rifle receiver in cm.
const FVector TriggerGrip(-0.8, 2.1, -0.5);
const FVector ForeGrip(25.0, -1.65, 2.7);
const FVector MagazineGrip(14.5, -1.2, -5.7);
const FName FingerNames[] = { TEXT("Thumb"), TEXT("Index"), TEXT("Middle"), TEXT("Ring"), TEXT("Pinky") };
}

US07RiflePresentation::US07RiflePresentation()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool US07RiflePresentation::CreateView(AShooterCharacter* OwnerCharacter)
{
    auto* Mesh = OwnerCharacter->ResolveCityBody();
    auto* RifleReceiver = LoadS07<UStaticMesh>(TEXT("SM_S07_RifleReceiver"));
    auto* RifleMagazine = LoadS07<UStaticMesh>(TEXT("SM_S07_RifleMagazine"));
    HandActions = LoadObject<UCurveTable>(nullptr, TEXT("/Game/VerticalCity/Weapons/CT_FirstPersonHands.CT_FirstPersonHands"));
    Idle = LoadS07<UAnimSequence>(TEXT("A_S07_RifleIdle"));
    Walk = LoadS07<UAnimSequence>(TEXT("A_S07_Walk"));
    Run = LoadS07<UAnimSequence>(TEXT("A_S07_Run"));
    Sprint = LoadS07<UAnimSequence>(TEXT("A_S07_Sprint"));
    Slide = LoadS07<UAnimSequence>(TEXT("A_S07_Slide"));
    Jump = LoadS07<UAnimSequence>(TEXT("A_S07_Jump"));
    Reload = LoadS07<UAnimSequence>(TEXT("A_S07_RifleReload"));
    if (!Mesh || !Idle || !Walk || !Run || !Slide || !Reload || !RifleReceiver || !RifleMagazine) return false;

    const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
    BuildComponentPose(Skeleton, Skeleton.GetRefBonePose(), Globals);
    const auto Find = [&Skeleton](FName Name) { return Skeleton.FindBoneIndex(Name); };
    Head = Find(TEXT("Head"));
    if (Head == INDEX_NONE) Head = Find(TEXT("Head1"));
    Spine = Find(TEXT("Spine"));
    Hips = Find(TEXT("Hips"));
    const int32 LeftToe = Find(TEXT("LeftToeBase"));
    if (Head < 0 || Spine < 0 || Hips < 0 || LeftToe < 0) return false;

    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FString Prefix = Side == 0 ? TEXT("Right") : TEXT("Left");
        FArm& Arm = Arms[Side];
        Arm.Upper = Find(FName(Prefix + TEXT("Arm")));
        Arm.Elbow = Find(FName(Prefix + TEXT("ForeArm")));
        Arm.Hand = Find(FName(Prefix + TEXT("Hand")));
        if (Arm.Upper < 0 || Arm.Elbow < 0 || Arm.Hand < 0) return false;
        for (int32 Digit = 0; Digit < 5; ++Digit)
        {
            for (int32 Link = 0; Link < 4; ++Link)
            {
                Arm.Fingers[Digit][Link] = Find(FName(FString::Printf(TEXT("%sHand%s%d"),
                    *Prefix, *FingerNames[Digit].ToString(), Link + 1)));
            }
        }
        if (Arm.Fingers[1][0] < 0 || Arm.Fingers[4][0] < 0) return false;
        const FTransform& Hand = Globals[Arm.Hand];
        const FVector Index = Hand.InverseTransformPosition(Globals[Arm.Fingers[1][0]].GetLocation());
        const FVector Pinky = Hand.InverseTransformPosition(Globals[Arm.Fingers[4][0]].GetLocation());
        Arm.PalmOffset = (Index + Pinky) * 0.5;
        Arm.PalmFrame = FRotationMatrix::MakeFromXZ(Arm.PalmOffset, Index - Pinky).ToQuat();
    }

    // All seven baked animation clips face mesh-local +Y.
    MeshYaw = -90.f;
    // Normalize the baked import only when necessary; the first-person eye is about 166 cm from the soles.
    const double EyeHeight = Globals[Head].GetLocation().Z - Globals[LeftToe].GetLocation().Z;
    if (EyeHeight <= UE_SMALL_NUMBER) return false;
    BodyScale = FMath::Clamp(157.0 / EyeHeight, 0.01, 100.0);
    FloorOffset = 3.2f - Globals[LeftToe].GetLocation().Z * BodyScale;
    Character = OwnerCharacter;
    Locomotion = Character->FindComponentByClass<UCityLocomotion>();
    if(!Body) Body = MakeViewMesh<UPoseableMeshComponent>(Character, TEXT("S07Body"));
    Body->SetSkinnedAssetAndUpdate(Mesh);
    Body->SetComponentTickEnabled(false);
    for (int32 Slot = 0; Slot < Mesh->GetMaterials().Num(); ++Slot)
    {
        const FString Name = Mesh->GetMaterials()[Slot].MaterialSlotName.ToString().ToLower();
        if (Name.Contains(TEXT("head")) || Name.Contains(TEXT("hair")))
        {
            for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD) Body->ShowMaterialSection(Slot, Slot, bShoulder, LOD);
        }
    }
    BaseLocals.Reset(); BlendFrom.Reset(); BaseClip=nullptr;
    if(Receiver && Magazine) return true;
    Receiver = MakeViewMesh<UStaticMeshComponent>(Character, TEXT("S07Receiver"));
    Magazine = MakeViewMesh<UStaticMeshComponent>(Character, TEXT("S07Magazine"));
    ActionPart = MakeViewMesh<UStaticMeshComponent>(Character, TEXT("S07ActionPart"));
    Receiver->SetStaticMesh(RifleReceiver);
    Magazine->SetStaticMesh(RifleMagazine);
    if (auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weapons/Rifle/Materials/M_Rifle.M_Rifle")))
    {
        Receiver->SetMaterial(0, Material);
        Magazine->SetMaterial(0, Material);
    }
    // Both exports keep the original grip pivot. The magazine sits straight ahead of it.
    const FVector MagCenter = RifleMagazine->GetBoundingBox().GetCenter();
    GunMeshYaw = -MagCenter.GetSafeNormal2D().Rotation().Yaw;
    return true;
}

void US07RiflePresentation::Equip(AShooterWeapon* Rifle)
{
    auto* Shooter = Cast<AShooterCharacter>(GetOwner());
    if (!IsValid(Rifle) || !Shooter || !Shooter->IsLocallyControlled()) return;
    if (!Body && !CreateView(Shooter)) return;
    Weapon = Rifle;
    BeginHandDraw();
    auto* Template = Character->GetFirstPersonMesh();
    bTemplateVisible = Template->IsVisible();
    bWeaponVisible = Rifle->GetFirstPersonMesh()->IsVisible();
    auto* Camera = Character->GetFirstPersonCameraComponent();
    SavedViewmodelFOV = Camera->FirstPersonFieldOfView;
    Camera->FirstPersonFieldOfView = 96.f;
    SavedTickOption = Template->VisibilityBasedAnimTickOption;
    Template->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Template->SetVisibility(false);
    Rifle->GetFirstPersonMesh()->SetVisibility(false);
    Body->SetVisibility(true);
    Receiver->SetVisibility(true);
    Magazine->SetVisibility(Magazine->GetStaticMesh()!=nullptr);
    ActionPart->SetVisibility(ActionPart->GetStaticMesh()!=nullptr);
    UpdatePose(0.f);
    SetComponentTickEnabled(true);
}

void US07RiflePresentation::Unequip(AShooterWeapon* Rifle)
{
    if (!Weapon || Rifle != Weapon || !Body) return;
    if(bShoulder) ToggleShoulder();
    SetComponentTickEnabled(false);
    Body->SetVisibility(false);
    Receiver->SetVisibility(false);
    Magazine->SetVisibility(false);
    ActionPart->SetVisibility(false);
    Character->GetFirstPersonMesh()->SetVisibility(bTemplateVisible);
    Character->GetFirstPersonMesh()->VisibilityBasedAnimTickOption = SavedTickOption;
    Character->GetFirstPersonCameraComponent()->FirstPersonFieldOfView = SavedViewmodelFOV;
    Rifle->GetFirstPersonMesh()->SetVisibility(bWeaponVisible);
    Weapon = nullptr;
    Kick = ContactWeight = 0.f;
}

void US07RiflePresentation::NotifyShot()
{
    if (Weapon) { Kick = 1.f; FirePhase=0.f; DrawPhase=1.f; }
}

void US07RiflePresentation::BeginHandDraw()
{
    DrawPhase=0.f;
    FirePhase=1.f;
#if !UE_BUILD_SHIPPING
    ObservedHandActions=0;
#endif
}

void US07RiflePresentation::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Delta, Type, Tick);
    if (IsValid(Weapon) && Body) UpdatePose(Delta);
}

void US07RiflePresentation::UpdatePose(float Delta)
{
    const auto* Camera = Character->GetFirstPersonCameraComponent();
    const auto* Movement = Character->GetCharacterMovement();
    const auto& Skel = Body->GetSkinnedAsset()->GetRefSkeleton();
    const float Speed = Movement->Velocity.Size2D();
    const bool bSlide = Locomotion && Locomotion->IsSliding();
    const float ReloadTime = Weapon->GetReloadProgress();
    const float ReloadPose = Smooth(0.f, .18f, ReloadTime) * (1.f - Smooth(.75f, 1.f, ReloadTime));
    ContactWeight = Smooth(.04f, .25f, ReloadTime) * (1.f - Smooth(.78f, .97f, ReloadTime));
    const bool bPistol = ArsenalKind=="sidearm" || ArsenalKind=="hand_cannon";
    DrawPhase=FMath::Min(1.f,DrawPhase+Delta/.32f);
    FirePhase=FMath::Min(1.f,FirePhase+Delta/(bPistol?.20f:.16f));
    FViewHandSample HandMotion;
    if (!bShoulder && (Weapon->IsReloading() || FirePhase<1.f || DrawPhase<1.f))
    {
        const EHandAction Action=Weapon->IsReloading()?EHandAction::Reload:(FirePhase<1.f?EHandAction::Fire:EHandAction::Equip);
        const float T=Action==EHandAction::Reload?ReloadTime:(Action==EHandAction::Fire?FirePhase:DrawPhase);
        HandMotion=ReadHandAction(HandActions,bPistol,Action,T);
        const float Weight=Action==EHandAction::Equip?1.f:1.f-Smooth(.83f,1.f,T);
        // Restrict authored offsets to the viewmodel workspace. IK still solves actual grips afterwards.
        HandMotion.Wrist=(HandMotion.Wrist*(Action==EHandAction::Fire?.85f:.31f)).GetClampedToMaxSize(8.f)*Weight;
        HandMotion.Pitch=FMath::Clamp(HandMotion.Pitch*.30f,-8.f,8.f)*Weight;
        HandMotion.Support=(HandMotion.Support*.22f).GetClampedToMaxSize(3.f)*Weight;
        HandMotion.Aperture=FMath::Clamp(HandMotion.Aperture*.09f,-.45f,.45f)*Weight;
        HandMotion.Trigger=FMath::Clamp(HandMotion.Trigger*.015f,-.35f,.35f)*Weight;
#if !UE_BUILD_SHIPPING
        if (HandMotion.Wrist.SizeSquared()>.0001 || FMath::Abs(HandMotion.Pitch)>.01f)
            ObservedHandActions|=1u<<static_cast<uint8>(Action);
#endif
    }
    const float MagazineTravel = Smooth(.27f, .47f, ReloadTime) * (1.f - Smooth(.52f, .75f, ReloadTime));
    Kick = FMath::Max(0.f, Kick - 7.f * Delta);
    SlideWeight = FMath::FInterpConstantTo(SlideWeight, bSlide ? 1.f : 0.f, Delta, 7.f);

    UAnimSequence* Desired = Idle;
    if (bSlide || Character->bIsCrouched) Desired = Slide;
    else if (Movement->IsFalling() && Jump) Desired = Jump;
    else if (Speed > 790.f && Sprint) Desired = Sprint;
    else if (Speed > 325.f) Desired = Run;
    else if (Speed > 20.f) Desired = Walk;
    if (Desired != BaseClip)
    {
        BlendFrom = BaseLocals;
        BlendTime = BlendFrom.IsEmpty() ? 1.f : 0.f;
        BaseClip = Desired;
        ClipTime = 0.f;
    }
    const float ReferenceSpeed = BaseClip == Walk ? 240.f : (BaseClip == Sprint ? 1050.f : 700.f);
    const float Rate = (BaseClip == Walk || BaseClip == Run || BaseClip == Sprint) ?
        FMath::Clamp(Speed / ReferenceSpeed, .45f, 1.5f) : 1.f;
    ClipTime = FMath::Fmod(ClipTime + Delta * Rate, FMath::Max(.001f, BaseClip->GetPlayLength()));
    SampleClip(BaseClip, ClipTime, Body, BaseLocals);
    if (BlendTime < 1.f)
    {
        BlendTime = FMath::Min(1.f, BlendTime + Delta / .13f);
        for (int32 i = 0; i < BaseLocals.Num(); ++i)
        {
            FTransform Target = BaseLocals[i];
            BaseLocals[i].Blend(BlendFrom[i], Target, Smooth(0.f, 1.f, BlendTime));
        }
    }
    TArray<FTransform>& Local = Body->BoneSpaceTransforms;
    Local = BaseLocals;
    if (ReloadPose > 0.f)
    {
        SampleClip(Reload, ReloadTime * Reload->GetPlayLength(), Body, UpperLocals);
        for (int32 i = Spine; i < Local.Num(); ++i)
        {
            if (i == Spine || Skel.BoneIsChildOf(i, Spine)) Local[i].Blend(BaseLocals[i], UpperLocals[i], ReloadPose);
        }
    }
    // This component belongs only to the viewer; collapse head geometry even for unnamed material slots.
    if(!bShoulder) Local[Head].SetScale3D(FVector::ZeroVector);
    BuildComponentPose(Skel, Local, Globals);

    const FVector Eye = Camera->GetComponentLocation();
    const FRotator Aim = Character->GetControlRotation();
    const FRotator Yaw(0.f, Aim.Yaw, 0.f);
    const FQuat ActorFrame = Yaw.Quaternion();
    const FQuat BodyFrame = ActorFrame * FRotator(23.f * SlideWeight, 13.f * SlideWeight, 0.f).Quaternion()
        * FRotator(0.f, MeshYaw, 0.f).Quaternion();
    const FVector Shoulders = (Globals[Arms[0].Upper].GetLocation() + Globals[Arms[1].Upper].GetLocation()) * .5;
    const FVector ShoulderOffset = BodyFrame.RotateVector(Shoulders * BodyScale);
    FVector BodyOrigin = Eye + ActorFrame.RotateVector(FVector((ArsenalKind.IsNone()?-17.f:1.f) + 12.f * SlideWeight, 5.f + 7.f * SlideWeight, 0.f)) - ShoulderOffset;
    BodyOrigin.Z = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + FloorOffset;
    const FTransform BodyToWorld(BodyFrame, BodyOrigin, FVector(BodyScale));
    Body->SetWorldTransform(BodyToWorld);

    const float MotionWeight = Movement->IsMovingOnGround() && !bSlide ? FMath::Min(Speed / 700.f, 1.f) : 0.f;
    const float Phase = 2.f * PI * ClipTime / FMath::Max(.001f, BaseClip->GetPlayLength());
    const FVector Bob(0.f, FMath::Sin(Phase) * .8f * MotionWeight, FMath::Cos(2.f * Phase) * .5f * MotionWeight);
    FRotator ViewmodelAim = Aim;
    ViewmodelAim.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Aim.Pitch), -55.f, 75.f);
    FVector GunPosition = Eye + ViewmodelAim.RotateVector(FVector(HipGrip.X - 2.5f * Kick + ReloadPose,
        HipGrip.Y - 6.f * SlideWeight - 4.f * ReloadPose, HipGrip.Z + 3.f * SlideWeight + 3.f * ReloadPose) + Bob);
    ADS=FMath::FInterpTo(ADS,bAiming && !bSlide && !Weapon->IsReloading()?1.f:0.f,Delta,14.f);

    GunPosition=FMath::Lerp(GunPosition,Eye+ViewmodelAim.RotateVector(FVector(34,0,-SightHeight)),ADS);
    
    GunPosition+=ViewmodelAim.RotateVector(HandMotion.Wrist)*(1.f-.65f*ADS);
    const FQuat GunRotation = ViewmodelAim.Quaternion() * FRotator(2.f*(1.f-ADS) + 4.f * Kick + 2.f * ReloadPose + HandMotion.Pitch*(1.f-.65f*ADS),
        -8.f*(1.f-ADS) + 17.f * SlideWeight - 4.f * ReloadPose, -19.f * ReloadPose - 9.f * SlideWeight).Quaternion();
    const FQuat GunInBody = BodyFrame.Inverse() * GunRotation;
    FVector MagOffset(-1.f * MagazineTravel, -2.5f * MagazineTravel,
        -11.f * MagazineTravel + 1.f * Smooth(.72f, .76f, ReloadTime) * (1.f - Smooth(.76f, .82f, ReloadTime)));
    FQuat MovingPart=FQuat::Identity;
    FQuat HingeTurn=FQuat::Identity;
    FVector HingeTravel=FVector::ZeroVector;
    if(ArsenalKind==TEXT("hand_cannon"))
    {
        const float Open=Smooth(.08f,.28f,ReloadTime)*(1.f-Smooth(.74f,.94f,ReloadTime));
        // The revolver's authored swing rotates 91 degrees around the common crane pivot.
        const FVector Pivot=Receiver->GetSocketTransform(TEXT("Hinge"),RTS_Component).GetLocation();
        HingeTurn=FQuat(FVector::ForwardVector,FMath::DegreesToRadians(91.f)*Open);
        HingeTravel=Pivot-HingeTurn.RotateVector(Pivot);
        MagOffset=HingeTravel+HingeTurn.RotateVector(MagazinePoint)-MagazinePoint;
        MovingPart=HingeTurn*FQuat(FVector::ForwardVector,2.f*PI*Smooth(.32f,.5f,ReloadTime));
    }
    else if(ArsenalKind==TEXT("smg")) MagOffset=FVector(-2.f,-2.f,-10.f)*MagazineTravel;
    else if(ArsenalKind==TEXT("pulse")) MagOffset=FVector(-5.f,-2.f,-11.f)*MagazineTravel;
    else if(ArsenalKind==TEXT("scout")) MagOffset=FVector(-3.f,-2.f,-12.f)*MagazineTravel;
    else if(ArsenalKind==TEXT("sidearm")) MagOffset=FVector(-2.7f,-1.f,-10.f)*MagazineTravel;
    else if(ArsenalKind==TEXT("shotgun")) MagOffset=FVector(-7.f,-2,-2)*MagazineTravel;
    // Move the visible pump and the support palm together during the firing stroke.
    const bool bPump = ArsenalKind=="shotgun";
    const float Stroke = FMath::Sin(PI * FirePhase);
    const FVector ActionOffset=ArsenalKind=="hand_cannon" ? HingeTravel : FVector((bPump ? -5.f : -1.6f)*Stroke,0,0);
    const FVector Support = SupportPoint + HandMotion.Support*(1.f-ADS) + (bPump ? ActionOffset : FVector::ZeroVector);
    const FVector Contacts[2] = {TriggerGrip, FMath::Lerp(Support, MagazinePoint + MagOffset, ContactWeight)};
    const FVector WristDirections[2] = {FVector(.83, -.32, .44),
        FMath::Lerp(FVector(.81, .44, .39), FVector(.32, .86, .40), ContactWeight)};
    FQuat HandRotations[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        HandRotations[Side] = GunInBody * FRotationMatrix::MakeFromXZ(WristDirections[Side], FVector::UpVector).ToQuat()
            * Arms[Side].PalmFrame.Inverse();
    }
    // The slide and sprint animations shift the shoulders. Bring the entire gun into the
    // intersection of both arm reach spheres before solving elbows, preserving rigid contact.
    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        for (int32 Side = 0; Side < 2; ++Side)
        {
            const FArm& Arm = Arms[Side];
            const FVector Shoulder = Globals[Arm.Upper].GetLocation();
            const FVector Elbow = Globals[Arm.Elbow].GetLocation();
            const FVector Hand = Globals[Arm.Hand].GetLocation();
            const FVector Palm = BodyToWorld.InverseTransformPosition(GunPosition + GunRotation.RotateVector(Contacts[Side]));
            const FVector Reach = Palm - HandRotations[Side].RotateVector(Globals[Arm.Hand].GetScale3D() * Arm.PalmOffset) - Shoulder;
            const double Length = FVector::Dist(Elbow, Shoulder) + FVector::Dist(Hand, Elbow) - .05;
            GunPosition += BodyToWorld.TransformVector(Reach.GetClampedToMaxSize(Length) - Reach);
        }
    }

    const FQuat MeshRotation = GunRotation * FRotator(0.f, GunMeshYaw, 0.f).Quaternion();
    Receiver->SetWorldLocationAndRotation(GunPosition, MeshRotation);
    if (ActionPart->GetStaticMesh()) ActionPart->SetWorldLocationAndRotation(GunPosition+GunRotation.RotateVector(ActionOffset), MeshRotation*HingeTurn);
    Magazine->SetWorldLocationAndRotation(GunPosition + GunRotation.RotateVector(MagOffset+MagazinePoint-MovingPart.RotateVector(MagazinePoint)), MeshRotation*MovingPart);
    RightTarget = GunPosition + GunRotation.RotateVector(Contacts[0]);
    LeftTarget = GunPosition + GunRotation.RotateVector(Contacts[1]);

    for (int32 Side = 0; Side < 2; ++Side)
    {
        const FArm& Arm = Arms[Side];
        const FVector Shoulder = Globals[Arm.Upper].GetLocation();
        const FVector Elbow = Globals[Arm.Elbow].GetLocation();
        const FVector Hand = Globals[Arm.Hand].GetLocation();
        const FVector WantedPalm = BodyToWorld.InverseTransformPosition(Side == 0 ? RightTarget : LeftTarget);
        const FVector Wrist = WantedPalm - HandRotations[Side].RotateVector(Globals[Arm.Hand].GetScale3D() * Arm.PalmOffset);
        const FVector Along = (Wrist - Shoulder).GetSafeNormal();
        const double UpperLength = FVector::Dist(Elbow, Shoulder);
        const double LowerLength = FVector::Dist(Hand, Elbow);
        const double Length = FMath::Clamp(FVector::Dist(Wrist, Shoulder), FMath::Abs(UpperLength - LowerLength) + .01, UpperLength + LowerLength - .01);
        const double Projection = (UpperLength * UpperLength - LowerLength * LowerLength + Length * Length) / (2. * Length);
        const FVector Pole = BodyFrame.UnrotateVector(ActorFrame.RotateVector(FVector(-.15, Side == 0 ? .8 : -.8, -1.0)));
        const FVector Out = FVector::VectorPlaneProject(Pole, Along).GetSafeNormal();
        const FVector Bend = Along * Projection + Out * FMath::Sqrt(FMath::Max(0., UpperLength * UpperLength - Projection * Projection));
        auto Point = [&](int32 Bone, int32 Child, const FVector& Direction)
        {
            const FVector Original = Globals[Bone].TransformVector(Local[Child].GetLocation()).GetSafeNormal();
            const FQuat Turn = FQuat::FindBetweenNormals(Original, Direction.GetSafeNormal());
            RotateBone(Skel, Local, Globals, Bone, Turn * Globals[Bone].GetRotation());
        };
        Point(Arm.Upper, Arm.Elbow, Bend);
        Globals[Arm.Elbow] = Local[Arm.Elbow] * Globals[Arm.Upper];
        Point(Arm.Elbow, Arm.Hand, Wrist - Globals[Arm.Elbow].GetLocation());
        Globals[Arm.Hand] = Local[Arm.Hand] * Globals[Arm.Elbow];
        RotateBone(Skel, Local, Globals, Arm.Hand, HandRotations[Side]);

        // Wrap the trigger hand round the pistol grip and cradle the handguard with the support hand.
        for (int32 Digit = 0; Digit < 5; ++Digit)
        {
            for (int32 Link = 0; Link < 3; ++Link)
            {
                const int32 Bone = Arm.Fingers[Digit][Link], Child = Arm.Fingers[Digit][Link+1];
                if (Bone < 0 || Child < 0) break;
                Globals[Bone] = Local[Bone] * Globals[Skel.GetParentIndex(Bone)];
                const FVector TriggerCurl[] = { FVector(1., -.16, -.24), FVector(.05, -1., -.16), FVector(-.68, -.72, .10) };
                FVector Curl = Side == 0 ? TriggerCurl[Link] : FMath::Lerp(
                    FVector(.22 - .1 * Link, 1. - .21 * Link, -.22 + .60 * Link),
                    FVector(.72 - .78 * Link, 1., -.15), ContactWeight);
                if (Digit == 0) Curl = FVector(.85, Side == 0 ? -.55 : .55, .50);
                if (Digit == 1 && Side == 0) Curl = FVector(1. - .54 * Link, -.24 - .30 * Link - HandMotion.Trigger, .24);
                if (Digit > 0 && Side == 1) Curl.Z += HandMotion.Aperture;
                Point(Bone, Child, GunInBody.RotateVector(Curl));
            }
        }
        const FVector ActualPalm = BodyToWorld.TransformPosition(Globals[Arm.Hand].TransformPosition(Arm.PalmOffset));
        (Side == 0 ? RightGripError : LeftGripError) = FVector::Dist(ActualPalm, Side == 0 ? RightTarget : LeftTarget);
    }
    Body->RefreshBoneTransforms();
}

bool US07RiflePresentation::SampleMagazineContact(float& ErrorCm) const
{
    if(!Body || !Magazine || !Magazine->GetStaticMesh() || Arms[1].Hand==INDEX_NONE) return false;
    const FVector Palm=Body->GetBoneTransform(Arms[1].Hand).TransformPosition(Arms[1].PalmOffset);
    const FVector Feed=Magazine->GetComponentTransform().TransformPosition(Magazine->GetStaticMesh()->GetBoundingBox().GetCenter());
    ErrorCm=FVector::Distance(Palm,Feed);
    return true;
}

bool US07RiflePresentation::TryGetMuzzleLocation(FVector& Out) const
{
    if (!IsValid(Weapon) || !Receiver || !IsComponentTickEnabled()) return false;
    const FQuat MeshToGrip = FRotator(0.f, GunMeshYaw, 0.f).Quaternion().Inverse();
    Out = Receiver->GetComponentTransform().TransformPosition(MeshToGrip.RotateVector(BarrelPoint));
    return true;
}
bool US07RiflePresentation::PlayMuzzleFlash(FName Kind, FRotator Direction)
{
    FVector Muzzle;
    if (!TryGetMuzzleLocation(Muzzle)) return false;
    auto* Art = Character->FindComponentByClass<UCitySkillVisuals>();
    return Art && Art->MuzzleFlash(Receiver, Muzzle, Direction, Kind, !bShoulder);
}
void US07RiflePresentation::SetArsenal(FName Kind)
{
    if(Body && Character && Body->GetSkinnedAsset()!=Character->ResolveCityBody()) CreateView(Character);
    if (!Receiver || !Magazine || ArsenalKind==Kind) return;
    const FString Path=TEXT("/Game/VerticalCity/Weapons/FieldModels/SM_")+Kind.ToString();
    UStaticMesh* Model=LoadObject<UStaticMesh>(nullptr,*Path);
    if (!Model) return;
    const UStaticMeshSocket* Muzzle=Model->FindSocket(TEXT("Muzzle"));
    const UStaticMeshSocket* Sight=Model->FindSocket(TEXT("Sight"));
    const UStaticMeshSocket* Support=Model->FindSocket(TEXT("Support"));
    if (!ensure(Muzzle && Sight && Support)) return;
    ArsenalKind=Kind;
    BeginHandDraw();
    UStaticMesh* Feed=Kind==TEXT("shotgun")?nullptr:LoadObject<UStaticMesh>(nullptr,*(Path+TEXT("_Mag")));
    Receiver->EmptyOverrideMaterials();
    Magazine->EmptyOverrideMaterials();
    Receiver->SetStaticMesh(Model);
    Magazine->SetStaticMesh(Feed);
    Magazine->SetVisibility(Feed!=nullptr);
    const auto* Row=FCityArsenalRow::Find(Kind);
    HipGrip=Row?Row->HipGrip:FVector(50,22,-21);
    GunMeshYaw=0;
    Receiver->SetWorldScale3D(FVector::OneVector);
    Magazine->SetWorldScale3D(FVector::OneVector);
    UStaticMesh* Action=(Kind=="sidearm" || Kind=="shotgun" || Kind=="hand_cannon") ? LoadObject<UStaticMesh>(nullptr,*(Path+TEXT("_Action"))) : nullptr;
    ActionPart->SetStaticMesh(Action);
    ActionPart->SetVisibility(Action!=nullptr);
    SupportPoint=Support->RelativeLocation;
    MagazinePoint=Feed?Feed->GetBoundingBox().GetCenter():SupportPoint;
    BarrelPoint=Muzzle->RelativeLocation;
    SightHeight=Sight->RelativeLocation.Z;
}

void US07RiflePresentation::ToggleShoulder()
{
    if(!Body || !Receiver || !Magazine) return;
    bShoulder=!bShoulder;
    auto Type=bShoulder?EFirstPersonPrimitiveType::None:EFirstPersonPrimitiveType::FirstPerson;
    Body->SetFirstPersonPrimitiveType(Type);
    Receiver->SetFirstPersonPrimitiveType(Type);
    Magazine->SetFirstPersonPrimitiveType(Type);
    ActionPart->SetFirstPersonPrimitiveType(Type);
    auto* Mesh=Body->GetSkinnedAsset();
    if(!Mesh) return;
    for(int Slot=0;Slot<Mesh->GetMaterials().Num();++Slot) {
        const FString Name=Mesh->GetMaterials()[Slot].MaterialSlotName.ToString().ToLower();
        if(Name.Contains(TEXT("head")) || Name.Contains(TEXT("hair")))
            for(int LOD=0;LOD<Mesh->GetLODNum();++LOD) Body->ShowMaterialSection(Slot,Slot,bShoulder,LOD);
    }
}
void US07RiflePresentation::FrameShoulder(float Delta,FMinimalViewInfo& View)
{
    if(!bShoulder || !IsValid(Weapon) || !Character) return;
    const FVector Eye=View.Location;
    const FVector End=Eye+View.Rotation.RotateVector(FVector(-220,62,24));
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CityShoulder),false,Character);
    Q.AddIgnoredActor(Weapon);
    FHitResult Hit;
    View.Location=GetWorld()->SweepSingleByChannel(Hit,Eye,End,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(12),Q)?Hit.Location:End;
    View.Rotation=((Eye+View.Rotation.Vector()*1900)-View.Location).Rotation();
}
