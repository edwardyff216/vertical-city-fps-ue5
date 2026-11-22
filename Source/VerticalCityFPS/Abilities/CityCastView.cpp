#include "Abilities/CityCastView.h"
#include "Abilities/CitySkillVisuals.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Camera/CameraTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UCityCastView::Setup(AShooterCharacter* Character)
{
    Player=Character;
    auto* Mesh=Player->ResolveCityBody();
    if (!Mesh || !FFileHelper::LoadFileToArray(Data,*(FPaths::ProjectContentDir()/TEXT("VerticalCity/Abilities/S07Casts.bin"))) || Data.Num()<8) return;
    if (FMemory::Memcmp(Data.GetData(),"VCSP",4)!=0) return;
    int32 Pos=4;
    auto UInt16=[&](){ const int32 N=Data[Pos]+256*Data[Pos+1]; Pos+=2; return N; };
    auto Name=[&](){
        FString S;
        const int32 Len=Data[Pos++];
        for (int32 I=0; I<Len; ++I) S.AppendChar(TCHAR(Data[Pos++]));
        return FName(S);
    };
    SourceBones=UInt16();
    const int32 NumClips=UInt16();
    if (SourceBones<1 || SourceBones>256 || NumClips<1 || NumClips>100) return;
    const auto& Ref=Mesh->GetRefSkeleton();
    BoneMap.Init(INDEX_NONE,Ref.GetNum());
    for (int32 I=0; I<SourceBones; ++I)
    {
        if (!Data.IsValidIndex(Pos) || Pos+1+Data[Pos]>Data.Num()) return;
        const FName Bone=Name();
        int32 Dst=Ref.FindBoneIndex(Bone);
        if (Dst==INDEX_NONE && Bone=="Head") Dst=Ref.FindBoneIndex(TEXT("Head1"));
        if (Dst!=INDEX_NONE) BoneMap[Dst]=I;
    }
    FrameBytes=12+SourceBones*8;
    for (int32 I=0; I<NumClips; ++I)
    {
        if (!Data.IsValidIndex(Pos) || Pos+3+Data[Pos]>Data.Num()) return;
        const FName Clip=Name();
        const int32 Frames=UInt16();
        if (Frames<2 || Frames>181 || Pos+Frames*FrameBytes>Data.Num()) return;
        Clips.Add(Clip,{Pos,Frames}); Pos+=Frames*FrameBytes;
    }
    Hips=Ref.FindBoneIndex(TEXT("Hips"));
    RefGlobal.SetNum(Ref.GetNum()); Animated.SetNum(Ref.GetNum());
    for (int32 I=0; I<Ref.GetNum(); ++I)
    {
        const int32 P=Ref.GetParentIndex(I);
        const FQuat Local=Ref.GetRefBonePose()[I].GetRotation();
        RefGlobal[I]=P<0?Local:RefGlobal[P]*Local;
    }
    if(!Body)
    {
        Body=NewObject<UPoseableMeshComponent>(Player,TEXT("CityCastS07"));
        Player->AddInstanceComponent(Body); Body->SetupAttachment(Player->GetCapsuleComponent());
    }
    Body->SetSkinnedAssetAndUpdate(Mesh); Body->SetOnlyOwnerSee(true);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision); Body->SetGenerateOverlapEvents(false);
    Body->SetCanEverAffectNavigation(false); Body->SetVisibility(false);
    Body->SetRelativeLocationAndRotation(FVector(0,0,-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),FRotator(0,-90,0));
    if(!Body->IsRegistered()) Body->RegisterComponent();
    Body->SetComponentTickEnabled(false);
}
FVector UCityCastView::Palm(bool bRight) const
{
    if (!Body) return Player->GetActorLocation();
    const FName Bone=bRight?TEXT("RightHandIndex1"):TEXT("LeftHandIndex1");
    return Body->GetSocketLocation(Bone);
}
void UCityCastView::Pose(FName Id,FName Clip,float Time,UCitySkillVisuals* Art)
{
    if(Player && (!Body || Body->GetSkinnedAsset()!=Player->ResolveCityBody())) Setup(Player);
    if (!Body) return;
    auto* Track=Clips.Find(Clip);
    if (!Track) return;
    const float Sample=FMath::Clamp(Time,0.f,1.f)*(Track->Frames-1);
    const int32 Index=FMath::FloorToInt(Sample), Other=FMath::Min(Index+1,Track->Frames-1);
    const uint8* A=Data.GetData()+Track->Offset+FrameBytes*Index;
    const uint8* B=Data.GetData()+Track->Offset+FrameBytes*Other;
    const float Alpha=Sample-Index;
    auto ReadQuat=[](const uint8* P){
        int16 Q[4]; FMemory::Memcpy(Q,P,8);
        return FQuat(Q[0]/32767.,Q[1]/32767.,Q[2]/32767.,Q[3]/32767.).GetNormalized();
    };
    auto ReadOffset=[](const uint8* P){ float V[3]; FMemory::Memcpy(V,P,12); return FVector(V[0],V[1],V[2]); };
    const auto& Ref=Body->GetSkinnedAsset()->GetRefSkeleton();
    auto& Local=Body->BoneSpaceTransforms;
    if (Local.Num()!=Ref.GetNum()) return;
    for (int32 I=0; I<Local.Num(); ++I)
    {
        Local[I]=Ref.GetRefBonePose()[I];
        const int32 Parent=Ref.GetParentIndex(I), Slot=BoneMap[I];
        const FQuat Basis=Parent>=0?Animated[Parent]:FQuat::Identity;
        if (Slot==INDEX_NONE) Animated[I]=Basis*Local[I].GetRotation();
        else
        {
            const int32 Offset=12+Slot*8;
            Animated[I]=(FQuat::Slerp(ReadQuat(A+Offset),ReadQuat(B+Offset),Alpha)*RefGlobal[I]).GetNormalized();
            Local[I].SetRotation((Basis.Inverse()*Animated[I]).GetNormalized());
        }
        if (I==Hips) Local[I].AddToTranslation(Basis.UnrotateVector(FMath::Lerp(ReadOffset(A),ReadOffset(B),Alpha)));
    }
    Body->RefreshBoneTransforms();
    if (bFramed) Props(Id,Clip,Art);
}
void UCityCastView::Props(FName Id,FName Clip,UCitySkillVisuals* Art)
{
    const FVector R=Palm(true), L=Palm(false), C=(R+L)*.5;
    const FVector F=Player->GetControlRotation().Vector(), Side=Player->GetActorRightVector(), Up=FVector::UpVector;
    if (Id=="barrage")
    {
        if (Clip=="barrage_windup")
            for (int32 I=-4; I<=4; ++I) Art->Blade(Id,C+Side*I*13+F*FMath::Abs(I)*4,Up,I*.19f);
    }
    else if (Id=="frenzy" || Id=="storm" || Id=="well")
    {
        Art->Rod(Id,R,R+F*51+Up*36,10);
        if (Id=="frenzy") Art->Rod(Id,L,L+F*55+Up*37,10);
    }
    else if (Id=="hammer")
    {
        Art->Rod(Id,R,R+Up*53,7);
        Art->Shape(Id,0,R+Up*48,F.ToOrientationQuat(),FVector(51,22,24));
    }
    else if (Id=="anchor")
    {
        Art->Rod(Id,L-Up*58+F*13,L,7); Art->Rod(Id,L,L+Up*58+F*13,7);
        Art->Rod(Id,L+Up*58+F*13,R,2); Art->Rod(Id,R,L-Up*58+F*13,2);
    }
    else if (Id=="nova") Art->Shape(Id,1,C+F*15,FQuat::Identity,FVector(36));
    else for (const FVector& P:{R,L}) Art->Shape(Id,Id=="ice"?2:1,P,F.ToOrientationQuat(),FVector(Id=="ice"?29:18));
}
void UCityCastView::SetFraming(bool bShow)
{
    if (bShow==bFramed || !Body) return;
    bFramed=bShow; Body->SetVisibility(bShow);
    if (bShow)
    {
        auto HidePart=[this](UPrimitiveComponent* Part){
            if (!Part || Part->bHiddenInGame || !Part->IsVisible()) return;
            Hidden.Add(Part); Part->SetHiddenInGame(true);
        };
        HidePart(Player->GetFirstPersonMesh());
        TInlineComponentArray<UPrimitiveComponent*> Parts(Player);
        for (auto* Part:Parts)
            if (Part->GetFName()=="S07Body" || Part->GetFName()=="S07Receiver" || Part->GetFName()=="S07Magazine" || Part->GetFName()=="S07ActionPart") HidePart(Part);
        if (auto* Gun=Player->GetCityWeapon())
        {
            TInlineComponentArray<UPrimitiveComponent*> WeaponParts(Gun);
            for (auto* Part:WeaponParts) HidePart(Part);
        }
    }
    else
    {
        for (auto Part:Hidden) if (Part.IsValid()) Part->SetHiddenInGame(false);
        Hidden.Reset();
    }
}
void UCityCastView::Frame(float Dt,float Blend,FMinimalViewInfo& View,FVector AimPoint)
{
    if (!Body || Blend<=0) { Hide(); return; }
    FRotator BoomDir(FMath::ClampAngle(View.Rotation.Pitch,-32,32),View.Rotation.Yaw,0);
    FVector Offset=BoomDir.RotateVector(FVector(-318,72,32));
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CastCamera),false,Player);
    if (auto* Weapon=Player->GetCityWeapon()) Query.AddIgnoredActor(Weapon);
    const bool bBlocked=GetWorld()->SweepSingleByChannel(Hit,View.Location,View.Location+Offset,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(18),Query);
    const float Clearance=bBlocked?FMath::Max(0.f,Hit.Distance-5):float(Offset.Size());
    Boom=FMath::Min(Clearance,Boom+Dt*1350);
    if (Clearance<90) { SetFraming(false); return; }
    View.Location+=Offset.GetSafeNormal()*Boom*Blend;
    View.Rotation=FQuat::Slerp(View.Rotation.Quaternion(),(AimPoint-View.Location).ToOrientationQuat(),Blend).Rotator();
    SetFraming(Boom*Blend>65);
}
void UCityCastView::Hide() { SetFraming(false); Boom=360; }

