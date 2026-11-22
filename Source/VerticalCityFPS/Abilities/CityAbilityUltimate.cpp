#include "Abilities/CityAbilities.h"
#include "Abilities/CitySkillVisuals.h"
#include "Abilities/CityCastView.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"
#include "ShooterWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UCityAbilities::StartUltimate(FName Id)
{
    if (!Phase.IsNone())
    {
        if (Id!=Ultimate) return false;
        if (Phase=="ready" && Id=="storm" && Charges>0) { --Charges; StartDash(800); return true; }
        if (Phase=="active" && (Id=="beam" || Id=="hammer" || Id=="frenzy")) { FinishUltimate(); return true; }
        return false;
    }
    if (UltimateCooldown>0 || Energy<100 || Player->IsKatanaDashing() || Player->GetCharacterMovement()->IsFalling()) return false;
    const auto* Def=FCityAbilityCatalog::Find(Id);
    if (auto* Weapon=Player->GetCityWeapon()) { Weapon->StopFiring(); Weapon->CancelReload(); }
    Energy=0; Ultimate=Id; Phase="windup"; PhaseTime=Def->Windup;
    CastStarted=Clock;
    UltimateCooldown=Def->Cooldown; StanceTime=Def->Duration; AttackReady=Clock; Combo=0;
    Charges=Id=="storm"?3:Id=="hammer"?4:0;
    return true;
}
void UCityAbilities::FinishUltimate()
{
    Phase="recovery"; PhaseTime=.42f;
    if (Motion=="ultimate_dash") Motion=NAME_None;
}

void UCityAbilities::PoseUltimate()
{
    if (!CastView || Phase.IsNone()) return;
    const auto* Def=FCityAbilityCatalog::Find(Ultimate);
    FName Stage=Phase;
    float T=1.f-StanceTime/FMath::Max(Def->Duration,.01f);
    if (Phase=="windup") T=1.f-PhaseTime/Def->Windup;
    else if (Phase=="recovery") T=1.f-PhaseTime/.42f;
    else if (Phase=="travel") { Stage="active"; T=1.f-MotionLeft/(Ultimate=="storm"?800:1800); }
    else if (Ultimate=="barrage") Stage="release";
    else if (Phase=="active" && Ultimate=="hammer" && AttackReady>Clock)
    { Stage="throw"; T=1.f-(AttackReady-Clock)/.6f; }
    else if (Phase=="active" && Ultimate=="frenzy" && Combo>0 && AttackReady>Clock)
    { Stage=FName(FString::Printf(TEXT("slash%d"),Combo)); T=1.f-(AttackReady-Clock)/.38f; }
    const FName Clip(Ultimate.ToString()+TEXT("_")+Stage.ToString());
    CastView->Pose(Ultimate,Clip,T,Art);
}

void UCityAbilities::FrameCamera(float Delta,FMinimalViewInfo& View)
{
    if (!CastView || !Player || Phase.IsNone()) { if (CastView) CastView->Hide(); return; }
    float Blend=FMath::SmoothStep(0.f,.14f,Clock-CastStarted);
    if (Phase=="recovery") Blend*=FMath::SmoothStep(0.f,.22f,PhaseTime);
    FHitResult H;
    FVector Focus=EndPoint(3000,H);
    CastView->Frame(Delta,Blend,View,Focus);
}
void UCityAbilities::StartDash(float Distance)
{
    Phase="travel"; Motion="ultimate_dash"; MotionDirection=Aim().GetSafeNormal2D(); MotionLeft=Distance;
}
void UCityAbilities::ReleaseUltimate()
{
    Phase="active";
    PhaseTime=StanceTime;
    if (Ultimate=="storm") { --Charges; StartDash(800); }
    else if (Ultimate=="charge") StartDash(1800);
    else if (Ultimate=="barrage")
    {
        // Spawn from the exact release pose even when the windup expires between rendered frames.
        if (CastView) CastView->Pose(Ultimate,TEXT("barrage_windup"),1.f,Art);
        LaunchBarrage();
    }
    else if (Ultimate=="anchor") Launch(Ultimate,Eye(),Aim(),3000,3000);
    else if (Ultimate=="nova") Launch(Ultimate,Eye(),Aim(),900,3000);
    else if (Ultimate=="well")
    {
        FVector Point=Player->GetActorLocation(); Floor(Point,Point); AddField(Ultimate,Point,400,8);
    }
    else if (Ultimate=="ice")
    {
        FVector F=Aim().GetSafeNormal2D(), R=Right().GetSafeNormal2D();
        for (int32 Lane=-1; Lane<=1; ++Lane)
            for (int32 Step=0; Step<3; ++Step)
            {
                FVector At=Player->GetActorLocation()+F*(200+330*Step)+R*Lane*330;
                if (!Visible(Eye(),At)) break;
                if (!Floor(At,At)) continue;
                AddField(Ultimate,At,160,5);
                Area(Ultimate,At+FVector::UpVector*55,160,45,1);
            }
    }
}
bool UCityAbilities::HandlePrimaryAttack()
{
    if (Buff("plate_lock")>0) return true;
    if (Phase.IsNone()) return false;
    if (Phase!="active" || AttackReady>Clock) return true;
    if (Ultimate=="hammer" && Charges>0)
    {
        Launch(Ultimate,Eye(),Aim(),2400,2800);
        AttackReady=Clock+.6f;
        if (--Charges==0) FinishUltimate();
    }
    else if (Ultimate=="frenzy")
    {
        Combo=Combo%3+1;
        const FVector Dir=Aim();
        for (auto* N : Near(Eye(),360))
            if (FVector::DotProduct(Dir,(N->GetActorLocation()-Eye()).GetSafeNormal())>(Combo==3?.15:.45))
                Hit(N,Combo==3?190:140,.18f);
        const float Side=Combo==2?-1.f:1.f;
        const FVector Center=Eye()+Dir*160;
        Art->Flash(Ultimate,Center+Right()*120*Side+FVector::UpVector*55,Center-Right()*125*Side-FVector::UpVector*40,12,.20f);
        AttackReady=Clock+.38f;
    }
    return true;
}
void UCityAbilities::TickUltimate(float Dt)
{
    if (Phase.IsNone()) return;
    PhaseTime-=Dt;
    if (Phase=="windup") { if (PhaseTime<=0) ReleaseUltimate(); return; }
    if (Phase=="recovery")
    {
        if (PhaseTime<=0) { Phase=NAME_None; Ultimate=NAME_None; }
        return;
    }
    StanceTime-=Dt;
    if (Phase=="travel")
    {
        const float Step=FMath::Min(MotionLeft,(Ultimate=="storm"?3600.f:3000.f)*Dt);
        FHitResult H;
        const FVector Before=Player->GetActorLocation();
        Player->AddActorWorldOffset(MotionDirection*Step,true,&H);
        MotionLeft-=Step;
        const FVector Now=Player->GetActorLocation();
        Art->Flash(Ultimate,Before+FVector::UpVector*10,Now,14,.12f);
        if (H.bBlockingHit || MotionLeft<=0 || Near(Now+MotionDirection*75,120).Num()>0)
        {
            Motion=NAME_None;
            Area(Ultimate,Now,Ultimate=="storm"?300:400,Ultimate=="storm"?160:260,.3f);
            if (Ultimate=="storm" && Charges>0 && StanceTime>0) Phase="ready";
            else FinishUltimate();
        }
    }
    else if (Phase=="active" && Ultimate=="beam")
    {
        FHitResult H;
        FVector End=EndPoint(3500,H);
        Hit(Cast<AShooterNPC>(H.GetActor()),180*Dt);
        const FVector Start=Eye()+Aim()*47+Right()*18-FVector::UpVector*16;
        Art->Rod(Ultimate,Start,End,10);
        Art->Shape(Ultimate,1,End,FQuat::Identity,FVector(24));
    }
    if (StanceTime<=0 && Phase!="recovery") FinishUltimate();
}
TArray<FVector> UCityAbilities::BarragePath(FVector Origin,FVector F,FVector Side,float Depth,float Lane)
{
    const float Width=FMath::Min(520.f,Depth*.48f)*FMath::Lerp(.57f,1.f,Lane);
    const float Spread=FMath::Min(200.f,Depth*.36f)*FMath::Sqrt(Lane);
    const FVector End=Origin+F*Depth+Side*Spread;
    const FVector B=Origin+Side*Width*1.45+F*Depth*.05;
    const FVector C=End+Side*Width*.95-F*Depth*.24;
    TArray<FVector> Points;
    constexpr int32 Samples=72;
    Points.Reserve(Samples+1);
    for (int32 I=0; I<=Samples; ++I)
    {
        const double T=double(I)/Samples, S=1.-T;
        Points.Add(S*S*S*Origin+3*S*S*T*B+3*S*T*T*C+T*T*T*End);
    }
    return Points;
}
void UCityAbilities::LaunchBarrage()
{
    const FVector Origin=Eye(), Forward=Aim();
    FHitResult H;
    EndPoint(3000,H);
    const float Depth=H.bBlockingHit ? FMath::Clamp(H.Distance,50.f,3000.f) : 1200.f;
    for (int32 I=0; I<9; ++I)
    {
        const bool bRight=I<5;
        const float Side=bRight?1.f:-1.f;
        const float Lane=bRight?(4-I)/4.f:(I-5)/3.f;
        Launch("barrage",Origin,Forward,2600,3000);
        auto& Shot=Flights.Last();
        Shot.Path=BarragePath(Origin,Forward,Right()*Side,Depth,Lane);
        Shot.Side=Right()*Side; Shot.Spin=Side;
        if (CastView)
        {
            const FVector Palm=CastView->Palm(bRight)+Right()*Side*Lane*42;
            if (Visible(Origin,Palm)) Shot.VisualOffset=Palm-Origin;
        }
        ++BladesFired;
    }
}

