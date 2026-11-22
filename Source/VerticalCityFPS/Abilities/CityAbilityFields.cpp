#include "Abilities/CityAbilities.h"
#include "Abilities/CitySkillVisuals.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"
#include "Campaign/CityEnemyBolt.h"
#include "Campaign/CityRoomShell.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
double SphereContact(FVector From,FVector To,FVector Center,double Radius)
{
    const FVector Travel=To-From, Offset=From-Center;
    const double C=Offset.SizeSquared()-Radius*Radius;
    if (C<=0) return 0;
    const double A=Travel.SizeSquared(), B=Offset.Dot(Travel);
    const double Root=B*B-A*C;
    if (A<=SMALL_NUMBER || B>=0 || Root<0) return 2;
    return (-B-FMath::Sqrt(Root))/A;
}
double WallContact(const FCitySkillField& Wall,FVector From,FVector To)
{
    if (Wall.Id!="barrier_wall" || Wall.bTriggered || Wall.Duration-Wall.Life<.25f) return 2;
    const FVector Side=FVector::UpVector.Cross(Wall.Direction);
    const FVector Middle=Wall.Point+FVector::UpVector*100;
    const FVector Start=From-Middle, Delta=To-From;
    const double A[]={Start.Dot(Wall.Direction),Start.Dot(Side),Start.Z};
    const double D[]={Delta.Dot(Wall.Direction),Delta.Dot(Side),Delta.Z};
    const double Half[]={10.,Wall.Radius,100.};
    double First=0, Last=1;
    for (int Axis=0; Axis<3; ++Axis)
    {
        if (FMath::IsNearlyZero(D[Axis]))
        {
            if (FMath::Abs(A[Axis])>Half[Axis]) return 2;
            continue;
        }
        const double T0=(-Half[Axis]-A[Axis])/D[Axis], T1=(Half[Axis]-A[Axis])/D[Axis];
        First=FMath::Max(First,FMath::Min(T0,T1));
        Last=FMath::Min(Last,FMath::Max(T0,T1));
        if (First>Last) return 2;
    }
    return First;
}
}
bool UCityAbilities::PlaceBarrier()
{
    FHitResult H;
    FVector At=Eye()+Aim()*1000;
    if (Trace(Eye(),At,H,true)) At=H.ImpactPoint+H.ImpactNormal*15;
    if (!Trace(At+FVector::UpVector*10,At-FVector::UpVector*1000,H,true) || H.ImpactNormal.Z<.65f) return false;
    At=H.ImpactPoint;
    const FVector Normal=FRotator(0,Player->GetControlRotation().Yaw,0).Vector(), Side=FVector::UpVector.Cross(Normal);
    auto Extent=[&](FVector Axis)
    {
        float Width=150;
        for (float Height:{25.f,110.f,190.f})
        {
            const FVector Start=At+FVector::UpVector*Height;
            if (Trace(Start,Start+Axis*Width,H,true)) Width=FMath::Max(0.f,H.Distance-15);
        }
        for (TActorIterator<ACityRoomShell> Shell(GetWorld()); Shell; ++Shell)
            for (int32 Slot=0; Slot<Shell->GateCount(); ++Slot)
            {
                const FVector Gate=Shell->GetActorTransform().TransformPosition(Shell->GateBase(Slot) + FVector(-30,0,0));
                for (float Step=0; Step<=Width; Step+=10)
                    if (FVector::DistSquared(At+Axis*Step,Gate)<22500)
                    { Width=FMath::Max(0.f,Step-10); break; }
            }
        return Width;
    };
    const float Left=Extent(-Side), RightWidth=Extent(Side);
    if (Left+RightWidth<100) return false;
    AddField("barrier_wall",At+Side*((RightWidth-Left)*.5f),(Left+RightWidth)*.5f,8,Normal);
    return true;
}
void UCityAbilities::RemoveWall(FCitySkillField& F)
{
    if (AActor* Wall=F.CollisionWall.Get())
    {
        Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Wall,false);
        Wall->Destroy();
    }
}
void UCityAbilities::TrackReturnedBolt(ACityEnemyBolt* Bolt)
{
    ReturnedBolts.RemoveAll([](const TWeakObjectPtr<ACityEnemyBolt>& Old){return !Old.IsValid();});
    ReturnedBolts.Add(Bolt);
}
void UCityAbilities::AddField(FName Id,FVector Point,float Radius,float Duration,FVector Direction,AShooterNPC* Follow)
{
    if (Id!="ice" && Id!="shrapnel_mine")
        for (int32 I=Fields.Num()-1; I>=0; --I) if (Fields[I].Id==Id)
        { RemoveWall(Fields[I]); Fields.RemoveAtSwap(I); }
    auto& F=Fields.AddDefaulted_GetRef();
    F.Id=Id; F.Point=Point; F.Direction=Direction; F.Radius=Radius;
    F.Duration=F.Life=Duration; F.Follow=Follow;
    F.Health=Id=="barrier_wall"?400:Id=="turret_drone"?150:200;
    if (Follow) F.Direction=Point-Follow->GetActorLocation();
    if (Id=="bullet_sink") Art->StartSink(Point);
    if (Id=="frost_floor") Art->FrostLanding(Point);
    if (Id=="barrier_wall")
    {
        AActor* Wall=GetWorld()->SpawnActor<AActor>();
        if (Wall)
        {
            auto* Box=NewObject<UBoxComponent>(Wall);
            Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(10,Radius,100));
            Box->SetCollisionObjectType(ECC_GameTraceChannel2);
            Box->SetCollisionResponseToAllChannels(ECR_Ignore);
            Box->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
            Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Box->RegisterComponent();
            Wall->SetActorLocationAndRotation(Point+FVector::UpVector*100,Direction.Rotation());
            Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Wall,true);
            F.CollisionWall=Wall;
        }
    }
    CastArt(Id,false,Point+FVector::UpVector*15,110);
}
bool UCityAbilities::InField(FName Id,FVector Point) const
{
    for (const auto& F:Fields)
        if (F.Id==Id && F.Life>0 && FVector::DistSquared(F.Point,Point)<=FMath::Square(F.Radius) && Visible(Point,F.Point)) return true;
    return false;
}
float UCityAbilities::EnemySpeedScale(const AActor* N) const
{
    if (!N) return 1;
    const FVector At=N->GetActorLocation();
    return InField("frost_floor",At) ? (Boss(Cast<AShooterNPC>(N))?.8f:.5f) : 1.f;
}
bool UCityAbilities::GetEnemyAimPoint(AActor* Enemy,FVector Actual,FVector& Out) const
{
    Out=Actual;
    if (!Enemy) return true;
    const FVector From=Enemy->GetActorLocation();
    const bool bBoss=Boss(Cast<AShooterNPC>(Enemy));
    bool bHidden=false;
    for (const auto& F:Fields)
        if (F.Id=="smoke_veil" && !bBoss && !IsInsideSmoke(Enemy) &&
            SphereContact(From+FVector::UpVector*28,Actual,F.Point,F.Radius)<=1) bHidden=true;
    for (const auto& F:Fields)
    {
        if (F.Id!="decoy_holo" && F.Id!="afterimage") continue;
        if (bBoss && !bHidden && Visible(From,Actual)) continue;
        if (FVector::DistSquared(From,F.Point)>2250000 || !Visible(From,F.Point)) continue;
        if (F.Id=="decoy_holo" && (FVector::DistSquared(From,Actual)<90000 || (LastHit.Get()==Enemy && Buff("last_hit")>0))) continue;
        Out=F.Point;
        return true;
    }
    return !bHidden;
}
bool UCityAbilities::IsInsideSmoke(const AActor* Enemy) const
{
    if (!Enemy) return false;
    for (const auto& F:Fields)
        if (F.Id=="smoke_veil" && FVector::DistSquared(Enemy->GetActorLocation(),F.Point)<=FMath::Square(F.Radius)) return true;
    return false;
}
float UCityAbilities::SmokeSpread(const AActor* Enemy,FVector Direction) const
{
    if (!Enemy) return 0;
    if (!Boss(Cast<AShooterNPC>(Enemy))) return IsInsideSmoke(Enemy)?6.f:0.f;
    const FVector From=Enemy->GetActorLocation()+FVector::UpVector*28;
    for (const auto& F:Fields)
        if (F.Id=="smoke_veil" && SphereContact(From,From+Direction*2400,F.Point,F.Radius)<=1) return 3.f;
    return 0;
}
FVector UCityAbilities::BarrierWaypoint(FVector From,FVector To) const
{
    for (const auto& F:Fields) if (WallContact(F,From,To)<=1)
    {
        const FVector Side=FVector::UpVector.Cross(F.Direction), Local=From-F.Point;
        return F.Point+Side*((Local.Dot(Side)<0?-1:1)*(F.Radius+85))+
            F.Direction*((Local.Dot(F.Direction)<0?-1:1)*75)+FVector::UpVector*90;
    }
    return To;
}
bool UCityAbilities::StrikeBarrier(FVector From,FVector To,float Damage)
{
    for (auto& F:Fields) if (WallContact(F,From,To)<=1)
    {
        if (Damage>0)
        {
            F.Health-=Damage;
            Art->Flash(F.Id,From,FMath::Lerp(From,To,WallContact(F,From,To)),5);
            if (F.Health<=0) { F.bTriggered=true; RemoveWall(F); }
        }
        return true;
    }
    return false;
}
FVector UCityAbilities::BendBolt(FVector Point,FVector Velocity,float Delta) const
{
    for (const auto& F:Fields)
        if (F.Id=="bullet_sink" && F.Stored<24 && FVector::DistSquared(Point,F.Point)<FMath::Square(F.Radius) && Visible(Point,F.Point))
            return Velocity+(F.Point-Point).GetSafeNormal()*(4000.f*Delta);
    return Velocity;
}
bool UCityAbilities::InterceptBolt(ACityEnemyBolt& Bolt,FVector From,FVector To)
{
    if (!Player || Player->IsDead() || Bolt.IsFriendly()) return false;
    const FVector Forward=Aim(), Up=Forward.Cross(Right()), Center=Eye()+Forward*110-Up*15;
    const double Start=(From-Center).Dot(Forward), End=(To-Center).Dot(Forward);
    double First=2;
    bool Plate=false;
    if (Buff("deflect_plate")>0 && Start>=0 && End<=0 && Start>End)
    {
        const double T=Start/(Start-End);
        const FVector Offset=FMath::Lerp(From,To,T)-Center;
        if (FMath::Abs(Offset.Dot(Right()))<=100 && FMath::Abs(Offset.Dot(Up))<=90) { First=T; Plate=true; }
    }
    FCitySkillField* Contact=nullptr;
    for (auto& F:Fields)
    {
        if (F.bTriggered || F.Life<=0) continue;
        double T=2;
        if (F.Id=="barrier_wall") T=WallContact(F,From,To);
        else if (F.Id=="bullet_sink" && F.Stored<24) T=SphereContact(From,To,F.Point,50);
        else if (F.Id=="decoy_holo" || F.Id=="afterimage" || F.Id=="turret_drone")
            T=SphereContact(From,To,F.Point,F.Id=="turret_drone"?50:75);
        if (T<=1 && T<First) { First=T; Contact=&F; Plate=false; }
    }
    if (First>1) return false;
    const FVector Impact=FMath::Lerp(From,To,First);
    if (Plate)
    {
        auto* Enemy=Cast<AShooterNPC>(Bolt.GetOwner());
        FVector Heading=IsValid(Enemy) && Enemy->CurrentHP>0?(Enemy->GetActorLocation()-Impact).GetSafeNormal():
            Bolt.FlightVelocity().MirrorByVector(Forward).GetSafeNormal();
        Bolt.SetActorLocation(Impact+Heading*3);
        Bolt.ReturnToPlayer(Player,Heading,Bolt.FlightVelocity().Size()*2,Bolt.DamageAmount()*2.5f,MAX_flt);
        TrackReturnedBolt(&Bolt);
        const float Discount=FMath::Min(.5f,4.f-DeflectRefund);
        Refund("deflect_plate",Discount); DeflectRefund+=Discount;
        CastArt("deflect_plate",false,Impact,80);
        Player->TriggerCityAction(TEXT("parry"));
    }
    else
    {
        if (Contact->Id=="bullet_sink") { ++Contact->Stored; Art->Flash(Contact->Id,From,Contact->Point,4); }
        else if (Contact->Id!="afterimage")
        {
            Contact->Health-=Bolt.DamageAmount();
            if (Contact->Health<=0) { Contact->bTriggered=true; RemoveWall(*Contact); }
        }
        CastArt(Contact->Id,false,Impact,65);
        Bolt.Destroy();
    }
    return true;
}
bool UCityAbilities::OnGunRay(const FVector& From,const FVector& To,FVector* VisualEnd)
{
    int32 Field=INDEX_NONE,Disc=INDEX_NONE;
    float Nearest=MAX_flt;
    FVector Intercept = To;
    auto Consider=[&](const FVector& Point,float Radius)
    {
        float Distance=FVector::Dist(From,Point);
        if (Distance>=Nearest || FMath::PointDistToSegment(Point,From,To)>Radius) return false;
        Nearest=Distance;
        if (VisualEnd) Intercept = FMath::ClosestPointOnSegment(Point, From, To);
        return true;
    };
    for (int32 I=0; I<Fields.Num(); ++I)
    {
        const auto& F=Fields[I];
        if ((F.Id=="shrapnel_mine" || F.Id=="decoy_holo") && Consider(F.Point,F.Id=="decoy_holo"?65:42)) Field=I;
        else if (F.Id=="shock_fence")
        {
            const FVector Side=FVector::UpVector.Cross(F.Direction)*F.Radius;
            if (Consider(F.Point+Side+FVector::UpVector*80,55) || Consider(F.Point-Side+FVector::UpVector*80,55)) Field=I;
        }
    }
    for (int32 I=0; I<Flights.Num(); ++I)
        if (Flights[I].Id=="ricochet_disc" && Consider(Flights[I].Point,45)) Disc=I;
    if (VisualEnd) *VisualEnd = Intercept;
    if (Disc!=INDEX_NONE)
    {
        const FVector At=Flights[Disc].Point; Flights.RemoveAtSwap(Disc); Cooldowns.Add("ricochet_disc",8);
        Area("ricochet_disc",At,350,80); CastArt("ricochet_disc",false,At,170); return true;
    }
    if (Field==INDEX_NONE) return false;
    auto& F=Fields[Field];
    if (F.Id=="shock_fence")
    {
        if (Buff("fence_gun")<=0)
        {
            const FVector Side=FVector::UpVector.Cross(F.Direction)*F.Radius;
            const FVector Center=F.Point+FVector::UpVector*80;
            for (auto* N:Near(Center,F.Radius+130))
                if (FMath::PointDistToSegment(N->GetActorLocation(),Center-Side,Center+Side)<=120) Hit(N,25);
            SetBuff("fence_gun",.5f);
        }
    }
    else F.bTriggered=true;
    return true;
}
void UCityAbilities::PulseField(FCitySkillField& F,float Dt)
{
    if (F.Id=="well" && FVector::DistSquared(F.Point,Player->GetActorLocation())<=FMath::Square(F.Radius) && Visible(F.Point,Eye()))
    { Player->HealCity(Dt*10); SetBuff("well_boost",.2f); }
    if (F.Pulse>0) { F.Pulse-=Dt; return; }
    F.Pulse=.19f;
    const FVector Origin=F.Point+FVector::UpVector*12;
    if (F.Id=="anchor" || F.Id=="ice")
    {
        for (auto* N:Near(Origin,F.Radius))
        {
            if (Boss(N)) continue;
            Hit(N,0,F.Id=="anchor"?.36f:.3f);
            if (F.Id=="anchor") Art->Flash(F.Id,Origin,N->GetActorLocation(),4,.18f);
        }
    }
    else if (F.Id=="turret_drone" && F.Duration-F.Life>.4f)
    {
        auto Targets=Near(Origin,F.Radius);
        if (Targets.IsEmpty()) return;
        AShooterNPC* Target=Buff("last_hit")>0 && Targets.Contains(LastHit.Get())?LastHit.Get():Targets[0];
        Art->Flash(F.Id,Origin,Target->GetActorLocation(),4,.11f);
        Hit(Target,Boss(Target)?7.2f:9.f);
    }
    else if (F.Id=="shrapnel_mine" && F.Duration-F.Life>=.5f)
    {
        if (F.Follow.IsValid()) F.bTriggered=F.Duration-F.Life>=.6f;
        else for (auto* N:Near(Origin,F.Radius)) if (!Boss(N)) { F.bTriggered=true; break; }
    }
    else if (F.Id=="shock_fence")
    {
        const FVector Side=FVector::UpVector.Cross(F.Direction)*F.Radius;
        const FVector Center=F.Point+FVector::UpVector*85;
        for (auto* N:Near(Center,F.Radius+120))
        {
            if (FMath::PointDistToSegment(N->GetActorLocation(),Center-Side,Center+Side)>95 || F.NextHits.FindRef(N)>Clock) continue;
            Hit(N,40,.8f); F.NextHits.Add(N,Clock+1.5f); Art->Flash(F.Id,Center,N->GetActorLocation(),6);
        }
    }
}
void UCityAbilities::EndField(FCitySkillField& F)
{
    RemoveWall(F);
    const FVector At=F.Point+FVector::UpVector*9;
    if (F.Id=="decoy_holo") Area(F.Id,At,400,50);
    else if (F.Id=="shrapnel_mine" && F.bTriggered)
    {
        Art->Burst(F.Id,At,500); CastArt(F.Id,false,At,230);
        for (auto* N:Near(At,500))
            Hit(N,FMath::Lerp(95.f,35.f,float(FVector::Dist(At,N->GetActorLocation())/500))*(Boss(N)?.7f:1.f),.18f);
        FRandomStream Random(FMath::RoundToInt(Clock*1000));
        for (int32 I=0; I<12; ++I)
        {
            FVector Dir=Random.VRand();
            if (Dir.Dot(F.Direction)<0) Dir=-Dir;
            FHitResult H;
            if (Trace(At,At+Dir*900,H)) Hit(Cast<AShooterNPC>(H.GetActor()),12);
            Art->Flash(F.Id,At,At+Dir*135,3,.12f);
        }
    }
    else if (F.Id=="cinch_post")
    {
        Art->Burst(F.Id,At,F.Radius,.5f);
        for (auto* N:Near(At,F.Radius))
        {
            if (Boss(N)) continue;
            Hit(N,0,.6f);
            const FVector Move=(F.Point-N->GetActorLocation()).GetSafeNormal2D();
            const float Distance=FMath::Min(400.f,float(FVector::Dist2D(F.Point,N->GetActorLocation())));
            for (float Moved=0; Moved<Distance; Moved+=40)
            {
                const FVector Step=Move*FMath::Min(40.f,Distance-Moved);
                FHitResult Ground,Block; const FVector Foot=N->GetActorLocation()+Step-FVector::UpVector*55;
                if (!Trace(Foot+FVector::UpVector*40,Foot-FVector::UpVector*90,Ground,true) || Ground.ImpactNormal.Z<.65) break;
                N->AddActorWorldOffset(Step,true,&Block);
                if (Block.bBlockingHit) break;
            }
            CastArt(F.Id,false,N->GetActorLocation(),100);
        }
    }
    else if (F.Id=="bullet_sink")
    {
        TArray<AShooterNPC*> Targets=Near(F.Point,400);
        if (Targets.IsEmpty())
        {
            Targets=Near(F.Point,2000);
            if (Targets.Num()>1) Targets.SetNum(1);
        }
        FActorSpawnParameters Spawn;
        Spawn.Owner=Player; Spawn.Instigator=Player;
        Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        for (int32 I=0; I<F.Stored; ++I)
        {
            const FVector Heading=Targets.IsEmpty()?FRotator(0,360.f*I/F.Stored,0).Vector():
                (Targets[I%Targets.Num()]->GetActorLocation()-F.Point).GetSafeNormal();
            if (auto* Fragment=GetWorld()->SpawnActor<ACityEnemyBolt>(F.Point,Heading.Rotation(),Spawn))
            {
                Fragment->ReturnToPlayer(Player,Heading,2400,15,1400);
                TrackReturnedBolt(Fragment);
            }
        }
        CastArt(F.Id,false,F.Point,150);
        if (F.Stored>=5) Area(F.Id,F.Point,300,40);
    }
    else if (F.Id=="turret_drone" && F.Health<=0) Refund(F.Id,4);
}
void UCityAbilities::RenderField(const FCitySkillField& F) const
{
    const FVector At=F.Point, Up=FVector::UpVector, Side=Up.Cross(F.Direction)*F.Radius;
    if (F.Id=="shock_fence" || F.Id=="barrier_wall")
    {
        const float Height=F.Id=="shock_fence"?220:200*FMath::Clamp((F.Duration-F.Life)/.25f,.01f,1.f);
        Art->Rod(F.Id,At-Side,At-Side+Up*Height,12); Art->Rod(F.Id,At+Side,At+Side+Up*Height,12);
        for (int32 I=1; I<=4; ++I) Art->Rod(F.Id,At-Side+Up*Height*I/4,At+Side+Up*Height*I/4,F.Id=="shock_fence"?4:12);
    }
    else if (F.Id=="decoy_holo" || F.Id=="afterimage")
    {
        const FName Ink=F.Id=="afterimage"?FName("phase_step"):F.Id;
        Art->Shape(Ink,1,At+Up*64,FQuat::Identity,FVector(39));
        Art->Shape(Ink,3,At+Up*8,FQuat::Identity,FVector(42,52,63));
        for (float Sign:{-1.f,1.f}) Art->Rod(Ink,At+Right()*Sign*13-Up*18,At+Right()*Sign*22-Up*79,13);
    }
    else if (F.Id=="shrapnel_mine") { Art->Shape(F.Id,1,At,FQuat::Identity,FVector(48,48,21)); }
    else if (F.Id=="turret_drone")
    {
        Art->Shape(F.Id,3,At+Up*15,FQuat::Identity,FVector(65,65,28));
        Art->Shape(F.Id,1,At+Up*39,FQuat::Identity,FVector(39));
    }
    else if (F.Id=="bullet_sink") Art->SinkField(At,F.Radius,F.Stored,Clock);
    else if (F.Id=="frost_floor") Art->FrostField(At,F.Radius,Clock);
    else if (F.Id=="cinch_post") { Art->Rod(F.Id,At,At+Up*100,13); Art->Ring(F.Id,At,85,6,12); }
    else
    {
        Art->Ring(F.Id,At,F.Radius,5,F.Id=="ice"?12:24);
        if (F.Id=="ice") Art->Shape(F.Id,2,At+Up*65,FRotator(14,0,0).Quaternion(),FVector(65,55,140));
        else if (F.Id=="anchor") Art->Shape(F.Id,2,At+Up*45,FQuat::Identity,FVector(44,44,85));
        else if (F.Id=="well") { Art->Rod(F.Id,At,At+Up*120,11); Art->Rod(F.Id,At+Up*90-Right()*32,At+Up*90+Right()*32,9); }
        else if (F.Id=="smoke_veil")
            for (int32 I=0; I<12; ++I)
            {
                const float A=I*2.f*PI/12+Clock*.19f;
                const FVector P=At+FVector(FMath::Cos(A)*F.Radius*.70,FMath::Sin(A)*F.Radius*.70,FMath::Sin(A*3)*65);
                Art->Shape(F.Id,1,P,FQuat::Identity,FVector(135,130,110));
            }
    }
}
void UCityAbilities::TickFields(float Delta)
{
    for (int32 I=Fields.Num()-1; I>=0; --I)
    {
        auto& F=Fields[I];
        if (F.Follow.IsValid()) F.Point=F.Follow->GetActorLocation()+F.Direction;
        PulseField(F,FMath::Min(Delta,F.Life));
        F.Life-=Delta;
        if (F.CollisionWall.IsValid() && F.Duration-F.Life>=.25f)
        {
            auto* Box=CastChecked<UBoxComponent>(F.CollisionWall->GetRootComponent());
            if (Box->GetCollisionEnabled()==ECollisionEnabled::NoCollision) Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        }
        if (F.bTriggered || F.Life<=0) { EndField(F); Fields.RemoveAtSwap(I); }
        else RenderField(F);
    }
}

