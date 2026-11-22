#include "Abilities/CityAbilities.h"
#include "Abilities/CitySkillVisuals.h"
#include "Campaign/CityEnemyBolt.h"
#include "ShooterCharacter.h"
#include "ShooterNPC.h"
#include "ShooterWeapon.h"
#include "CityKatana.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

bool UCityAbilities::Small(FName Id)
{
    FHitResult H;
    const FVector Origin=Eye(), Dir=Aim();
    auto* Gun=Player->GetCityWeapon();
    const bool bGun=Gun && !Gun->IsA<ACityKatana>();
    if (Id=="echo_pin") Launch(Id,Origin,Dir,6000,3000);
    else if (Id=="cinch_post") Launch(Id,Origin,(Dir*1800+FVector::UpVector*300).GetSafeNormal(),1800,1800);
    else if (Id=="frost_floor" || Id=="smoke_veil" || Id=="bullet_sink")
    {
        const bool bSink=Id=="bullet_sink";
        if (bSink)
        {
            Fields.RemoveAll([Id](const FCitySkillField& F){return F.Id==Id;});
            Flights.RemoveAll([Id](const FCitySkillFlight& F){return F.Id==Id;});
        }
        const FVector Velocity=Dir*(bSink?1700:1300)+FVector::UpVector*(bSink?200:420);
        Launch(Id,Origin,Velocity.GetSafeNormal(),Velocity.Size(),bSink?1200:Id=="smoke_veil"?1400:1600);
    }
    else if (Id=="ricochet_disc")
    {
        if (Flights.ContainsByPredicate([Id](const FCitySkillFlight& F){return F.Id==Id;})) return false;
        Launch(Id,Origin,Dir.GetSafeNormal2D(),2200,3600);
    }
    else if (Id=="seeker_swarm") LaunchSwarm();
    else if (Id=="barrier_wall") return PlaceBarrier();
    else if (Id=="shrapnel_mine")
    {
        const FVector At=EndPoint(1400,H);
        if (!H.bBlockingHit) return false;
        AddField(Id,At+H.ImpactNormal*8,250,15.5f,H.ImpactNormal,Cast<AShooterNPC>(H.GetActor()));
        int32 Count=0;
        for (int32 I=Fields.Num()-1; I>=0; --I)
            if (Fields[I].Id==Id && ++Count>2) Fields[I].bTriggered=true;
    }
    else if (Id=="arc_lance")
    {
        FVector From=Origin+Dir*65-Right()*24;
        FVector At=EndPoint(1800,H);
        auto* Target=Cast<AShooterNPC>(H.GetActor());
        if (!Target)
        {
            Art->Flash(Id,From,At,5);
            if (H.bBlockingHit) Area(Id,At+H.ImpactNormal*9,300,15);
            return true;
        }
        TSet<AShooterNPC*> Visited;
        for (int32 I=0; Target && I<3; ++I)
        {
            At=Target->GetActorLocation();
            Visited.Add(Target);
            const FVector Bend=(From+At)*.5+Right()*25+FVector::UpVector*24;
            Art->Flash(Id,From,Bend,6);
            Art->Flash(Id,Bend,At,6);
            CastArt(Id,false,At,115);
            ArcMarks.Add(Target,Clock+1.5f);
            Hit(Target,I==0?60:30,I==0?.35f:.2f);
            Target=nullptr;
            for (auto* Next : Near(At,600)) if (!Visited.Contains(Next)) { Target=Next; break; }
            From=At;
        }
    }
    else if (Id=="patch_kit")
    {
        if (Player->GetCityHealthFraction()<.3f) Cooldowns[Id]=14;
        HealLeft=1.5f; bHealClean=true;
        Player->HealCity(15);
    }
    else if (Id=="overclock_round")
    {
        if (!bGun) return false;
        if (!Gun->GetBulletCount()) Gun->RefillCityMagazine(Gun->GetMagazineSize());
        Rounds=6; SetBuff(Id,8);
    }
    else if (Id=="finisher_lens")
    {
        LensExtended=0; SetBuff(Id,4);
        EndPoint(9000,H);
        auto* N=Cast<AShooterNPC>(H.GetActor());
        if (Wounded(N)) Hit(N,25);
    }
    else if (Id=="magnet_reload")
    {
        if (!bGun) { bPendingReload=true; return true; }
        const bool bEmpty=Gun->GetBulletCount()==0;
        Gun->RefillCityMagazine(Gun->GetMagazineSize());
        SetBuff(Id,6);
        if (bEmpty)
        {
            Area(Id,Origin,300,30,.4f);
            for (auto* N : Near(Origin,300))
                if (!Boss(N)) N->LaunchCharacter((N->GetActorLocation()-Origin).GetSafeNormal2D()*900+FVector::UpVector*180,true,true);
            for (TActorIterator<ACityEnemyBolt> It(GetWorld()); It; ++It)
                if (!It->IsFriendly() && FVector::DistSquared(It->GetActorLocation(),Origin)<90000) It->Destroy();
        }
    }
    else if (Id=="deflect_plate")
    {
        SetBuff(Id,1.5f); SetBuff("plate_lock",1.75f); DeflectRefund=0;
        if (Gun) Gun->StopFiring();
    }
    else if (Id=="grapple_hook")
    {
        const FVector At=EndPoint(2500,H);
        if (!H.bBlockingHit) { Refund(Id,6.3f); return true; }
        HookEnemy=Cast<AShooterNPC>(H.GetActor());
        const float HalfHeight=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (HookEnemy.IsValid())
            MotionTarget=HookEnemy->GetActorLocation()-Dir.GetSafeNormal2D()*200;
        else
            MotionTarget=At+H.ImpactNormal*85-(Origin-Player->GetActorLocation());
        FVector Ground;
        if (Floor(MotionTarget,Ground)) MotionTarget.Z=FMath::Max(MotionTarget.Z,Ground.Z+HalfHeight);
        Motion=Id; MotionLeft=1;
    }
    else if (Id=="phase_step")
    {
        const bool bAir=Player->GetCharacterMovement()->IsFalling();
        if (bAir && bAirPhase) return false;
        bAirPhase=bAir;
        FVector Move=Player->GetLastMovementInputVector().GetSafeNormal2D();
        if (Move.IsNearlyZero()) Move=Dir.GetSafeNormal2D();
        MotionDirection=Move; Motion=Id; MotionLeft=600;
        AddField("afterimage",Player->GetActorLocation(),85,1);
        SetBuff("immune",.37f);
    }
    else if (Id=="vault_jet")
    {
        bJet=true; Player->LaunchCharacter(FVector(0,0,1100),false,true);
    }
    else if (Id=="slide_surge")
    {
        Motion=Id; MotionLeft=1.2f; MotionDirection=Dir.GetSafeNormal2D(); SurgeHits.Reset();
        Player->Crouch();
        if (bGun && Gun->GetBulletCount()<Gun->GetMagazineSize()) Gun->StartReload();
    }
    else
    {
        const float Range=Id=="turret_drone"?1200:1500;
        FVector At=EndPoint(Range,H);
        if (H.bBlockingHit) At+=H.ImpactNormal*22;
        if (Id=="turret_drone")
        {
            if (!H.bBlockingHit) return false;
            AddField(Id,At,1800,10);
        }
        else
        {
            if (!Floor(At,At)) return false;
            if (Id=="shock_fence") AddField(Id,At,300,7,Dir.GetSafeNormal2D());
            else if (Id=="decoy_holo") AddField(Id,At+FVector::UpVector*90,400,6);
            else return false;
        }
    }
    return true;
}

void UCityAbilities::TickPersonal(float Dt)
{
    auto* Move=Player->GetCharacterMovement();
    if (Move->IsMovingOnGround()) bAirPhase=false;
    if (HealLeft>0)
    {
        const float Part=FMath::Min(Dt,HealLeft);
        Player->HealCity(20.f/1.5f*Part);
        HealLeft-=Part;
        if (HealLeft<=0 && bHealClean) { Shield=15; SetBuff("shield",5); }
    }
    if (bJet && !bGliding && Move->Velocity.Z<=0 && Move->IsFalling())
    {
        bJet=false; bGliding=true;
        GravityBeforeGlide=Move->GravityScale; AirControlBeforeGlide=Move->AirControl;
        Move->GravityScale=GravityBeforeGlide*.25f; Move->AirControl=1; SetBuff("glide",1.5f);
    }
    if (bGliding && (Buff("glide")<=0 || Move->IsMovingOnGround()))
    {
        bGliding=false; Move->GravityScale=GravityBeforeGlide; Move->AirControl=AirControlBeforeGlide;
    }
    if (Buff("dive")>0 && Move->IsMovingOnGround()) { SetBuff("dive",0); Area("vault_jet",Player->GetActorLocation(),250,30); }
    if (bPendingReload)
    {
        auto* Gun=Player->GetCityWeapon();
        if (Gun && !Gun->IsA<ACityKatana>()) { Gun->RefillCityMagazine(Gun->GetMagazineSize()); SetBuff("magnet_reload",6); bPendingReload=false; }
    }
    const FVector Here=Player->GetActorLocation();
    if (Motion=="grapple_hook")
    {
        const FVector Span=MotionTarget-Here;
        FHitResult H;
        Player->AddActorWorldOffset(Span.GetClampedToMaxSize(2800.f*Dt),true,&H);
        Art->Rod(Motion,Eye()-Right()*30,MotionTarget,4);
        MotionLeft-=Dt;
        if (MotionLeft<=0 || H.bBlockingHit || Span.Size()<100)
        {
            if (HookEnemy.IsValid() && FVector::Dist(HookEnemy->GetActorLocation(),Here)<350)
            { Hit(HookEnemy.Get(),0,.6f); SetBuff("hook_blade",2); }
            Motion=NAME_None;
        }
    }
    else if (Motion=="phase_step")
    {
        const float Step=FMath::Min(MotionLeft,5000.f*Dt);
        FHitResult H;
        if (Trace(Here,Here+MotionDirection*(Step+40),H,true,nullptr,30))
        { Player->AddActorWorldOffset(MotionDirection*FMath::Max(0.f,H.Distance-40.f),true); Motion=NAME_None; }
        else { Player->AddActorWorldOffset(MotionDirection*Step,false); MotionLeft-=Step; if (MotionLeft<=0) Motion=NAME_None; }
    }
    else if (Motion=="slide_surge")
    {
        MotionLeft-=Dt;
        FHitResult H;
        Player->AddActorWorldOffset(MotionDirection*1960*Dt,true,&H);
        for (auto* N : Near(Here,180))
        {
            if (SurgeHits.Contains(N)) continue;
            SurgeHits.Add(N); Hit(N,20,.5f);
            if (!Boss(N)) N->LaunchCharacter(MotionDirection*900+FVector::UpVector*200,true,true);
        }
        if (MotionLeft<=0 || H.bBlockingHit)
        {
            bSurgeBonus=SurgeHits.Num()>0; Motion=NAME_None; Player->UnCrouch();
            if (auto* Gun=Player->GetCityWeapon(); Gun && Gun->IsReloading()) Gun->RefillCityMagazine(Gun->GetMagazineSize());
        }
    }
    if (Buff("deflect_plate")>0)
    {
        const FVector CameraUp=Aim().Cross(Right());
        const FVector Center=Eye()+Aim()*110-CameraUp*15;
        const FVector R=Right()*100, U=CameraUp*90;
        Art->Rod("deflect_plate",Center-R-U,Center-R+U,7);
        Art->Rod("deflect_plate",Center+R-U,Center+R+U,7);
        Art->Rod("deflect_plate",Center-R+U,Center+R+U,7);
        Art->Rod("deflect_plate",Center-R-U,Center+R-U,7);
    }
    if (Echo.IsValid() && Buff("echo_pin")>0)
    {
        const FVector At=Echo->GetActorLocation()+FVector::UpVector*105;
        Art->Shape("echo_pin",2,At,FRotator(180,0,0).Quaternion(),FVector(32,32,45));
    }
    if (Buff("finisher_lens")>0)
        for (TActorIterator<AShooterNPC> It(GetWorld()); It; ++It)
            if (Wounded(*It)) Art->Ring("finisher_lens",It->GetActorLocation()+FVector::UpVector*90,28,4,8);
}
bool UCityAbilities::JumpPressed()
{
    if (Motion != "grapple_hook") return false;
    Motion = NAME_None;
    Player->LaunchCharacter((MotionTarget-Player->GetActorLocation()).GetSafeNormal()*1680,true,true);
    return true;
}
void UCityAbilities::SlidePressed()
{
    if (bGliding) { SetBuff("glide",0); SetBuff("dive",3); Player->LaunchCharacter(FVector(0,0,-1800),false,true); }
}
float UCityAbilities::GetSlideScale() const { return Player && InField("frost_floor",Player->GetActorLocation()) ? 1.25f : 1.f; }
float UCityAbilities::ModifyFireInterval(float Base) const { return Base*(Buff("magnet_reload")>0?.75f:1.f); }
float UCityAbilities::ModifySpread(float Base) const { return bGliding?0:Base; }
void UCityAbilities::OnGunShot()
{
    bOverclockShot=Buff("overclock_round")>0 && Rounds>0;
    if (bOverclockShot) --Rounds;
    bSurgeShot=bSurgeBonus; bSurgeBonus=false;
}
float UCityAbilities::ConsumeWeaponBuffs(bool bMelee)
{
    float Bonus=Buff("well_boost")>0 ? 1.3f : 1.f;
    if (bMelee && Buff("hook_blade")>0) { Bonus*=1.5f; SetBuff("hook_blade",0); }
    return Bonus;
}
float UCityAbilities::WeaponDamage(AShooterNPC* N,float Damage,bool bMelee,bool bAttackBonusApplied)
{
    if (!N) return Damage;
    if (!bMelee && bOverclockShot) Damage*=1.6f;
    if (!bMelee && bSurgeShot) Damage*=1.5f;
    if (!bMelee && ArcMarks.FindRef(N)>Clock) Damage+=4;
    if (!bAttackBonusApplied) Damage*=ConsumeWeaponBuffs(bMelee);
    if (Buff("finisher_lens")>0 && Wounded(N)) { Damage*=Boss(N)?1.4f:2.f; SetBuff("lens_hit",.02f); }
    return Damage;
}
void UCityAbilities::OnWeaponHit(AShooterNPC* N,float Damage,bool bMelee,bool bLethal)
{
    if (!N || Damage<=0) return;
    LastHit=N; SetBuff("last_hit",1);
    if (bLethal && !bMelee && bOverclockShot) Refund("overclock_round",.6f);
    if (bLethal && Buff("lens_hit")>0 && !Boss(N))
    {
        SetBuff("lens_hit",0); Refund("finisher_lens",2);
        if (auto* Gun=Player->GetCityWeapon()) Gun->RefillCityMagazine(FMath::CeilToInt(Gun->GetMagazineSize()*.2f));
        const float Extra=FMath::Min(.5f,2.f-LensExtended);
        LensExtended+=Extra; SetBuff("finisher_lens",Buff("finisher_lens")+Extra);
    }
    if (Echo.Get()!=N || Buff("echo_pin")<=0) return;
    const float Fraction=(bMelee?0.f:.4f)+(bLethal?1.f:0.f);
    if (bLethal) Echo.Reset();
    if (Fraction<=0) return;
    const FVector At=N->GetActorLocation();
    int32 Count=0;
    for (auto* Other : Near(At,400,N))
    {
        Art->Flash("echo_pin",At,Other->GetActorLocation(),5);
        CastArt("echo_pin",false,Other->GetActorLocation(),100); Hit(Other,Damage*Fraction);
        if (++Count==3) break;
    }
}
float UCityAbilities::FilterIncomingDamage(float Damage,AActor* Source)
{
    if (Damage<=0 || Buff("immune")>0) return 0;
    auto* Attacker=Cast<AShooterNPC>(Source);
    const FVector Out=Attacker?(Attacker->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D():FVector::ZeroVector;
    if (Buff("deflect_plate")>0 && Attacker && Player->GetActorForwardVector().GetSafeNormal2D().Dot(Out)>=.5f)
    {
        if (!Boss(Attacker))
        {
            Hit(Attacker,0,.8f);
            Attacker->AddActorWorldOffset(Out*150,true);
        }
        CastArt("deflect_plate",false,Attacker->GetActorLocation(),120);
        Player->TriggerCityAction(TEXT("parry"));
        return 0;
    }
    if (Buff("shield")>0)
    {
        const float Saved=FMath::Min(Shield,Damage);
        Shield-=Saved; Damage-=Saved;
    }
    if (Damage>0) bHealClean=false;
    return Damage;
}

