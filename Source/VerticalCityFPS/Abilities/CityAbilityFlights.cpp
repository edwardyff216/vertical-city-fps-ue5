#include "Abilities/CityAbilities.h"
#include "Abilities/CitySkillVisuals.h"
#include "Campaign/CityEnemyBolt.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"
#include "EngineUtils.h"

void UCityAbilities::Launch(FName Id,FVector Point,FVector Direction,float Speed,float Range)
{
    auto& F=Flights.AddDefaulted_GetRef();
    F.Id=Id; F.Origin=F.Point=Point; F.Direction=Direction.GetSafeNormal(); F.Side=FVector::ZeroVector;
    F.Speed=Speed; F.Remaining=Range;
}
void UCityAbilities::LaunchSwarm()
{
    const FVector Origin=Eye(), Forward=Aim(), Side=Right();
    TArray<AShooterNPC*> Targets=Near(Origin,2000);
    auto AimDot=[Origin,Forward](const AShooterNPC& N){return Forward.Dot((N.GetActorLocation()-Origin).GetSafeNormal());};
    Targets.RemoveAll([&AimDot](AShooterNPC* N){return AimDot(*N)<FMath::Cos(FMath::DegreesToRadians(12.f));});
    Targets.Sort([&AimDot](const AShooterNPC& A,const AShooterNPC& B){return AimDot(A)>AimDot(B);});
    for (int32 I=0; I<6; ++I)
    {
        float Sign=(I%2)?1.f:-1.f;
        Launch("seeker_swarm",Origin,(Forward+Side*Sign*.58+FVector::UpVector*(.36+.08*I)).GetSafeNormal(),1400,5000);
        auto& F=Flights.Last(); F.Side=Forward; F.Delay=.04f*I;
        if (Targets.Num()) F.Target=Targets[I%Targets.Num()];
    }
}
bool UCityAbilities::MoveFlight(FCitySkillFlight& F,float Dt)
{
    const FName Id=F.Id;
    if (F.Delay>0) { F.Delay-=Dt; return true; }
    F.Age+=Dt;
    if (Id=="seeker_swarm" && F.Age>=.2f)
    {
        const FVector Wanted=F.Target.IsValid() && F.Target->CurrentHP>0 ? (F.Target->GetActorLocation()-F.Point).GetSafeNormal() : F.Side;
        F.Direction=FMath::VInterpNormalRotationTo(F.Direction,Wanted,Dt,360.f);
        F.Speed=2000;
    }
    const bool bToss=Id=="cinch_post" || Id=="frost_floor" || Id=="smoke_veil" || Id=="bullet_sink";
    if (bToss)
    {
        const bool Canister=Id=="smoke_veil" || Id=="frost_floor";
        const float Gravity=Canister?1600.f:2000.f;
        const FVector Previous=F.Direction*F.Speed;
        const FVector Velocity=Previous-FVector::UpVector*Gravity*Dt;
        F.Side=Velocity;
        const FVector StepVelocity=Canister?(Previous+Velocity)*.5f:Velocity;
        F.Speed=StepVelocity.Size(); F.Direction=StepVelocity.GetSafeNormal();
    }
    float Budget=FMath::Min(F.Speed*Dt,F.Remaining);
    if (Id=="bullet_sink")
    {
        const FVector End=F.Origin+(F.Point+F.Direction*F.Speed*Dt-F.Origin).GetClampedToMaxSize(1200);
        Budget=FVector::Dist(F.Point,End);
        F.Direction=(End-F.Point).GetSafeNormal();
    }
    const FVector TickStart=F.Point;
    while (Budget>KINDA_SMALL_NUMBER)
    {
        const bool bCurve=F.Path.IsValidIndex(F.NextPoint);
        const FVector Next=bCurve?F.Path[F.NextPoint]:F.Point+F.Direction*Budget;
        const FVector Span=Next-F.Point;
        const float Distance=Span.Size();
        if (Distance<=KINDA_SMALL_NUMBER) { if (bCurve) { ++F.NextPoint; continue; } break; }
        const float Step=FMath::Min(Budget,Distance);
        F.Direction=Span/Distance;
        const FVector End=F.Point+F.Direction*Step;
        FHitResult H;
        const bool bDisc=Id=="ricochet_disc";
        const bool bHit=Trace(F.Point,End,H,bDisc || bToss,nullptr,bDisc?20:0);
        const FVector Actual=bHit?H.ImpactPoint:End;
        if (bDisc)
        {
            for (auto* N : Near((F.Point+Actual)*.5,Step*.5+95))
            {
                if (F.NextHits.FindRef(N)>Clock || FMath::PointDistToSegment(N->GetActorLocation(),F.Point,Actual)>80) continue;
                Hit(N,45.f*FMath::Pow(1.1f,F.Bounces));
                CastArt(Id,false,N->GetActorLocation(),95); F.NextHits.Add(N,Clock+.4f);
            }
        }
        if (Id=="seeker_swarm")
            for (TActorIterator<ACityEnemyBolt> It(GetWorld()); It; ++It)
                if (!It->IsFriendly() && FMath::PointDistToSegment(It->GetActorLocation(),F.Point,Actual)<32)
                { It->Destroy(); CastArt(Id,false,Actual,70); return false; }
        F.Point=Actual;
        if (Id=="bullet_sink") F.Remaining=1200-FVector::Dist(F.Origin,F.Point);
        else F.Remaining-=Step;
        Budget-=Step;
        if (bHit)
        {
            if (bDisc && ++F.Bounces<=3)
            {
                F.Direction=F.Direction.MirrorByVector(H.ImpactNormal).GetSafeNormal();
                F.Point=H.Location+H.ImpactNormal*3;
                CastArt(Id,false,F.Point,85);
                break;
            }
            Land(F,H); return false;
        }
        if (bCurve && Step>=Distance-KINDA_SMALL_NUMBER) ++F.NextPoint;
    }
    if (bToss) { F.Speed=F.Side.Size(); F.Direction=F.Side.GetSafeNormal(); }
    if (F.Remaining<=KINDA_SMALL_NUMBER || (Id=="bullet_sink" && F.Age>=.8f) || (Id=="seeker_swarm" && F.Age>=2.5f) || (Id=="ricochet_disc" && F.Age>=3))
    {
        Land(F,FHitResult()); return false;
    }
    if (Id=="barrage")
    {
        const FVector FrameForward=F.Path.IsEmpty()?F.Direction:(F.Path.Last()-F.Origin).GetSafeNormal();
        const float HandBlend=1.f-FMath::SmoothStep(0.f,.08f,F.Age);
        Art->Blade(Id,F.Point+F.VisualOffset*HandBlend,FrameForward,F.Spin*F.Age*2*PI*2.75f);
        Art->Flash(Id,TickStart,F.Point,3,.07f);
    }
    else if (Id=="ricochet_disc")
    {
        Art->Shape(Id,3,F.Point,FRotator(10,F.Age*700,15).Quaternion(),FVector(76,76,9));
        Art->Rod(NAME_None,F.Point,F.Point+FRotator(0,F.Age*700,0).Vector()*35,7);
    }
    else if (Id=="hammer")
    {
        const FQuat Rot=F.Direction.ToOrientationQuat()*FQuat(FVector::RightVector,F.Age*17);
        Art->Rod(Id,F.Point-Rot.GetAxisZ()*42,F.Point+Rot.GetAxisZ()*19,8);
        Art->Shape(Id,0,F.Point+Rot.GetAxisZ()*23,Rot,FVector(54,28,28));
    }
    else if (Id=="anchor" || Id=="echo_pin")
    {
        Art->Rod(Id,F.Point-F.Direction*65,F.Point,6);
        Art->Shape(Id,2,F.Point,FRotationMatrix::MakeFromZ(F.Direction).ToQuat(),FVector(22,22,28));
    }
    else
    {
        Art->Shape(Id,1,F.Point,FQuat::Identity,FVector(Id=="nova"?66:Id=="cinch_post"?29:19));
        Art->Rod(Id,F.Point-F.Direction*(Id=="nova"?100:36),F.Point,Id=="nova"?12:5);
    }
    return true;
}
void UCityAbilities::Land(FCitySkillFlight& F,const FHitResult& H)
{
    const FName Id=F.Id;
    auto* N=Cast<AShooterNPC>(H.GetActor());
    FVector At=F.Point+(H.bBlockingHit?H.ImpactNormal*9:FVector::ZeroVector);
    if (Id=="echo_pin")
    {
        if (N && N->CurrentHP>0) { Echo=N; SetBuff(Id,4); CastArt(Id,false,At,150); }
        else Cooldowns.Add(Id,4.5f);
    }
    else if (Id=="cinch_post")
    {
        if (H.bBlockingHit || Floor(At,At)) AddField(Id,At,700,.7f);
    }
    else if (Id=="ricochet_disc") Cooldowns.Add(Id,8);
    else if (Id=="frost_floor") { if (Floor(At,At)) AddField(Id,At,500,6); }
    else if (Id=="smoke_veil") AddField(Id,F.Point,600,6);
    else if (Id=="bullet_sink")
    {
        FHitResult Ground;
        if (Trace(At+FVector::UpVector*5,At-FVector::UpVector*500,Ground,true) && Ground.ImpactNormal.Z>.6)
            At.Z=Ground.ImpactPoint.Z+120;
        AddField(Id,At,400,4);
    }
    else if (Id=="barrage") { Hit(N,110); Art->Burst(Id,At,55,.13f); }
    else if (Id=="seeker_swarm") { Hit(N,18); CastArt(Id,false,At,80); }
    else if (Id=="anchor") { Hit(N,70); AddField(Id,At,500,6); }
    else if (Id=="nova" || Id=="hammer")
    {
        const float Damage=Id=="nova"?260:130, Radius=Id=="nova"?450:220;
        Hit(N,Damage);
        for (auto* Other:Near(At,Radius,N)) Hit(Other,Damage);
        Art->Burst(Id,At,Radius);
    }
}
void UCityAbilities::TickFlights(float Delta)
{
    for (int32 I=Flights.Num()-1; I>=0; --I)
        if (!MoveFlight(Flights[I],Delta)) Flights.RemoveAtSwap(I);
}

