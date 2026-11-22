#include "CityWeaponEffects.h"
#include "Abilities/CitySkillVisuals.h"
#include "Sound/SoundWave.h"
#include "Camera/CameraComponent.h"
#include "Abilities/CityAbilities.h"
#include "Campaign/CityCampaignEnemy.h"
#include "Progression/CityProgression.h"
#include "ShooterCharacter.h"
#include "ShooterWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UCityWeaponEffects::UCityWeaponEffects() { PrimaryComponentTick.bCanEverTick=true; }
AShooterCharacter* UCityWeaponEffects::Player() const { return CastChecked<AShooterCharacter>(GetOwner()); }
void UCityWeaponEffects::Beam(UWorld* W,FVector Start,FVector End,FLinearColor Color,float Radius,float Life)
{
    const FVector Line=End-Start;
    if (!W || Line.IsNearlyZero()) return;
    // The cube and its tinted material are engine assets available in cooked games.
    static TWeakObjectPtr<UStaticMesh> Shape;
    static TWeakObjectPtr<UMaterialInterface> TintBase;
    if (!TintBase.IsValid()) TintBase=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!Shape.IsValid()) Shape=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Shape.IsValid()) return;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Trace=W->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),(Start+End)*.5,Line.Rotation(),Params);
    if (!Trace) return;
    auto* Mesh=Trace->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(Shape.Get());
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Trace->SetActorScale3D(FVector(Line.Size()/100,Radius/100,Radius/100));
    if (TintBase.IsValid())
        if (auto* Material=Mesh->CreateDynamicMaterialInstance(0,TintBase.Get())) Material->SetVectorParameterValue(TEXT("Color"),Color);
    Trace->SetLifeSpan(Life);
}
void UCityWeaponEffects::ResetRoom()
{
    Ready.Reset(); Contacts.Reset(); Status.Reset(); Pending.Reset(); PreHit.Reset();
}

float UCityWeaponEffects::MovementScale(const AActor* Enemy) const
{
    auto* NPC=Cast<AShooterNPC>(const_cast<AActor*>(Enemy));
    const auto* S=NPC?Status.Find(NPC):nullptr;
    return S && S->Frost>0 ? .65f : 1.f;
}

float UCityWeaponEffects::ContactMultiplier(AShooterNPC* Enemy,bool Melee,const FHitResult* Hit,const FCityItem& Item)
{
    PreHit.Reset();
    auto* P=Player();
    auto* Foe=Cast<ACityCampaignEnemy>(Enemy);
    auto& S=Status.FindOrAdd(Enemy);
    float M=1;
    const FName Kind=Item.Kind;
    auto Affix=[&Item](FName Id){ return FCityItemCatalog::Amount(Item,Id); };
    if (!Melee && Hit && Kind==TEXT("rifle") && Hit->ImpactPoint.Z >= Enemy->GetActorLocation().Z+48.f)
    {
        PreHit.Add(TEXT("weak"));
        // Spire's exposed optic applies its own 1.5x hit multiplier in TakeDamage.
        if (!Foe || Foe->GetKind()!=ECityFoe::Spire) M*=1.5f*(1.f+Affix(TEXT("weakpoint")));
        else if (Foe->IsWindingUp() || Foe->IsExposed()) M*=1.f+Affix(TEXT("weakpoint"));
    }
    if (Foe && Enemy->CurrentHP>=Foe->MaxHealth()) { PreHit.Add(TEXT("opening")); M*=1+Affix(TEXT("opener")); }
    if (S.Fire>0) PreHit.Add(TEXT("burning"));
    if (S.Frost>0) PreHit.Add(TEXT("chilled"));
    if (Foe && Foe->IsStunned()) PreHit.Add(TEXT("stunned"));
    if (P->IsCitySliding()) PreHit.Add(TEXT("slide_hit"));
    if (P->GetCharacterMovement()->IsFalling()) PreHit.Add(TEXT("air_hit"));
    if (FVector::DistSquared(P->GetActorLocation(),Enemy->GetActorLocation())<250000) PreHit.Add(TEXT("close"));
    const float Distance=P->GetDistanceTo(Enemy);
    if(Foe && Enemy->CurrentHP<=.30f*Foe->MaxHealth()) M*=1+Affix(TEXT("execute"));
    if(Foe && Foe->IsBoss()) M*=1+Affix(TEXT("boss_hunter"));
    if(Foe && Foe->IsStunned()) M*=1+Affix(TEXT("punish"));
    if(S.Fire>0) M*=1+Affix(TEXT("kindling"));
    if(P->GetCityHealthFraction()<=.35f) M*=1+Affix(TEXT("desperate"));
    if(!Melee && Distance<=500) M*=1+Affix(TEXT("point_blank"));
    if(!Melee && Distance>=1500) M*=1+Affix(TEXT("longshot"));
    if(P->GetCharacterMovement()->IsFalling()) M*=1+Affix(TEXT("air_strike"));
    else if(P->GetVelocity().Size2D()<50) M*=1+Affix(TEXT("steady"));
    if(P->IsCitySliding()) M*=1+Affix(TEXT("slide_strike"));
    return M;
}
void UCityWeaponEffects::Contact(AShooterNPC* Enemy,float Actual,bool Melee,bool Killed,float Impact,const FCityItem& Item)
{
    auto* P=Player();
    if (!P->HasCampaignEquipment()) return;
    auto Affix=[&Item](FName Id){ return FCityItemCatalog::Amount(Item,Id); };
    auto Events=PreHit;
    Events.Add(TEXT("hit"));
    if (++Contacts.FindOrAdd(Item.Id)%3==0) Events.Add(TEXT("third"));
    if (Killed)
    {
        Events.Add(TEXT("kill"));
        P->GetCityAbilities()->AddWeaponEnergy(Affix(TEXT("kill_energy")));
    }
    else if (auto* Foe=Cast<ACityCampaignEnemy>(Enemy))
    {
        if (Impact<0) Impact=FCityItemCatalog::Stats(Item).Impact;
        const bool BreakCharge=Foe->GetKind()==ECityFoe::Breaker && Foe->IsWindingUp() && Impact>=42.f;
        if (BreakCharge || (!Foe->IsWindingUp() && Impact >= (Foe->IsBoss()?126.f:42.f))) Foe->StunFor(.25f);
    }
    P->HealCity(Actual*Affix(TEXT("leech"))+(Killed?Affix(TEXT("kill_heal")):0));
    if (!Killed && Affix(TEXT("burn"))>0) Burn(Enemy);
    Trigger(MoveTemp(Events),Enemy->GetActorLocation(),Enemy,Item);
}
void UCityWeaponEffects::Burn(AShooterNPC* Enemy)
{
    auto& S=Status.FindOrAdd(Enemy);
    if(S.Fire<=0) S.BurnTick=.5f;
    S.Fire=2;
}
void UCityWeaponEffects::Action(FName Event)
{
    auto* P=Player();
    const auto Item=P->GetGameInstance()->GetSubsystem<UCityProgression>()->GetWeapon(P->GetCitySlot());
    Trigger({Event},P->GetActorLocation(),nullptr,Item);
}
void UCityWeaponEffects::Trigger(TSet<FName> Events,FVector Center,AShooterNPC* Primary,const FCityItem& Item)
{
    auto* P=Player();
    const double Time=GetWorld()->GetTimeSeconds();
    const FStatus* Previous=Primary?Status.Find(Primary):nullptr;
    const int32 OriginalSeals=Previous?Previous->Seals:0;
    int32 Spent=0;
    if (Primary && Events.Contains(TEXT("hit")))
    {
        auto& S=Status.FindOrAdd(Primary);
        for (FName Id:{FName(TEXT("arcane_codex")),FName(TEXT("arcane_rupture"))})
        {
            if (!Item.Affixes.Contains(Id)) continue;
            if (S.Seals>=(Id==TEXT("arcane_codex")?3:1) && Ready.FindRef(Item.Id+Id.ToString())<=Time)
            { Spent=S.Seals;S.Seals=0;Events.Add(FCityItemCatalog::Affix(Id)->Event);break; }
        }
        if (!Spent && (Item.Affixes.Contains(TEXT("arcane_codex")) || Item.Affixes.Contains(TEXT("arcane_rupture")))) S.Seals=FMath::Min(3,S.Seals+1);
    }
    const FVector Aim=P->GetControlRotation().Vector();
    for (FName Id:Item.Affixes)
    {
        const auto* E=FCityItemCatalog::Affix(Id);
        const FString Key=Item.Id+Id.ToString();
        if (!E || !Events.Contains(E->Event) || Ready.FindRef(Key)>Time) continue;
        Ready.Add(Key,Time+E->Cooldown);
        const float Bonus=Id==TEXT("arcane_rupture")?6.f*Spent:0.f;
        const int32 Gain=Id==TEXT("arcane_transfer")?FMath::Min(3,1+OriginalSeals):E->SealGain;
        if (E->Shape!=TEXT("echo")) Pulse(*E,Center,Aim,Primary,Bonus,Gain);
        if (E->Shape==TEXT("field") || E->Shape==TEXT("echo"))
            Pending.Add({E,Center,Aim,E->Shape==TEXT("echo")?.45f:.55f,Bonus,E->Shape==TEXT("field")?2:1});
    }
}
void UCityWeaponEffects::ShowAffix(FName Id,FVector Center,float Width)
{
    auto* P=Player();
    const auto* Camera=P->GetFirstPersonCameraComponent();
    const FVector Eye=Camera->GetComponentLocation();
    FVector Anchor=Center;
    if (FVector::DistSquared(Center,P->GetActorLocation())<400)
    {
        Anchor=Camera->GetComponentTransform().TransformPosition(FVector(175,32,-50));
        FHitResult H;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CityAffixAnchor),false,P);
        if (GetWorld()->LineTraceSingleByObjectType(H,Eye,Anchor,FCollisionObjectQueryParams(ECC_WorldStatic),Params))
            Anchor=H.ImpactPoint+H.ImpactNormal*5;
    }
    else Anchor+=((Eye-Center)*.15f).GetClampedToMaxSize(75);
    UCitySkillVisuals::Emit(P,Id,Anchor,FMath::Min(Width,FMath::Max(20.f,float(FVector::Dist(Eye,Anchor))*.38f)));
}
void UCityWeaponEffects::Pulse(const FCityAffix& E,FVector Center,FVector Aim,AShooterNPC* Primary,float Bonus,int32 SealGain)
{
    const bool Linked=E.Shape==TEXT("chain") || E.Shape==TEXT("fork");
    ShowAffix(E.Id,Center,Linked?165.f:E.Shape==TEXT("single")?155.f:FMath::Clamp(E.RadiusMetres*65.f,140.f,310.f));
    static const TMap<FName,FColor> Inks = {
        {TEXT("fire"),FColor(255,139,56)}, {TEXT("frost"),FColor(168,236,255)}, {TEXT("volt"),FColor(255,240,122)},
        {TEXT("blast"),FColor(255,216,148)}, {TEXT("void"),FColor(208,142,255)}, {TEXT("arcane"),FColor(255,230,171)}
    };
    const FLinearColor Color=FLinearColor::FromSRGBColor(Inks.FindChecked(E.School));
    TSet<AShooterNPC*> Seen;
    FVector Origin=Center;
    for (int I=0;I<(Linked?3:1);++I)
    {
        TArray<ACityCampaignEnemy*> Targets;
        for(TActorIterator<ACityCampaignEnemy> It(GetWorld());It;++It)
        {
            auto* Enemy=*It;
            if (Enemy->CurrentHP<=0 || Seen.Contains(Enemy)) continue;
            FVector Offset=Enemy->GetActorLocation()-Origin;
            if (E.Shape==TEXT("single") ? Enemy!=Primary : Offset.Size()>E.RadiusMetres*100) continue;
            if(E.Shape==TEXT("cone") && !Offset.IsNearlyZero() && FVector::DotProduct(Aim,Offset.GetSafeNormal())<.574f) continue;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(CityEffectLOS),false,Player());
            Params.AddIgnoredActor(Enemy);Params.AddIgnoredActor(Primary);
            if(GetWorld()->LineTraceTestByObjectType(Origin,Enemy->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Params)) continue;
            Targets.Add(Enemy);
        }
        Targets.Sort([Origin](const auto& A,const auto& B){return FVector::DistSquared(Origin,A.GetActorLocation())<FVector::DistSquared(Origin,B.GetActorLocation());});
        if (Linked && Targets.Num()>1) Targets.SetNum(1);
        for(auto* Enemy:Targets)
        {
            Seen.Add(Enemy);
            const auto At=Enemy->GetActorLocation();
            if (Linked && !Origin.Equals(At,1.f)) ShowAffix(E.Id,At,115.f);
            Beam(GetWorld(),Origin.Equals(At)?At+FVector(0,0,35):Origin,At,Color,4,.13f);
            auto& S=Status.FindOrAdd(Enemy);
            float Extra=Bonus;
            if(E.Id==TEXT("arcane_codex")) { Extra+=4*S.Seals;S.Seals=0; }
            S.Seals=FMath::Clamp(S.Seals+(SealGain>=0?SealGain:E.SealGain),0,3);
            UGameplayStatics::ApplyDamage(Enemy,E.Damage+Extra,Player()->GetController(),Player(),nullptr);
            if(E.School==TEXT("fire")) Burn(Enemy);
            if(!Enemy->IsBoss() && Enemy->CurrentHP>0)
            {
                if(E.School==TEXT("frost")) { S.Frost=1.2f;if(E.Shape==TEXT("freeze")) Enemy->StunFor(1.f); }
                if(E.School==TEXT("volt")) Enemy->StunFor(.2f);
                if(E.School==TEXT("blast") || E.School==TEXT("void"))
                    {
                    FVector Offset=At-Origin; Offset.Z=0;
                    if (E.School==TEXT("blast") && Offset.IsNearlyZero()) Offset=At-Player()->GetActorLocation();
                    const FVector Step=E.School==TEXT("void") ? -Offset.GetClampedToMaxSize(120.f) : Offset.GetSafeNormal2D()*90.f;
                    Enemy->AddActorWorldOffset(Step,true);
                }
            }
            if(E.Shape==TEXT("chain")) Origin=At;
        }
        if(Targets.IsEmpty()) break;
    }
}
void UCityWeaponEffects::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Tick,Function);
    auto* P=Player();
    if (P->IsDead() || !P->HasCampaignEquipment()) return;
    for (int32 I=Pending.Num()-1;I>=0;--I)
    {
        auto& Job=Pending[I]; Job.Remaining-=Delta;
        while (Job.Waves>0 && Job.Remaining<=0.f)
        {
            Pulse(*Job.Effect,Job.Center,Job.Aim,nullptr,Job.Bonus);
            --Job.Waves; Job.Remaining+=.55f;
        }
        if (!Job.Waves) Pending.RemoveAt(I);
    }
    float Fraction=P->GetCityHealthFraction();
    if(Fraction<LastHP) RegenWait=4.f*(1.f-P->CityAffix(TEXT("regen_delay")));
    const float RecoveryTime=FMath::Max(0.f,Delta-RegenWait);
    RegenWait=FMath::Max(0.f,RegenWait-Delta);
    const float Cap=.15f+P->CityAffix(TEXT("regen_cap"));
    if (RecoveryTime>0 && Fraction<Cap)
    {
        const float MaxHP=GetWorld()->GetGameInstance()->GetSubsystem<UCityProgression>()->HasNode(TEXT("endurance")) ? 115.f : 100.f;
        const float Healing=RecoveryTime*5.f*(1.f+P->CityAffix(TEXT("regen_rate")));
        const float TotalHP=P->GetCityMaxHealth();
        P->HealCity(FMath::Min(Healing,(Cap-Fraction)*TotalHP));
    }
    LastHP=P->GetCityHealthFraction();
    for (auto I=Status.CreateIterator();I;++I)
    {
        auto* Enemy=I.Key().Get();
        if(!Enemy || Enemy->CurrentHP<=0) { I.RemoveCurrent();continue; }
        auto& S=I.Value();
        S.Frost=FMath::Max(0.f,S.Frost-Delta);
        if(S.Fire>0) { S.BurnTick-=FMath::Min(Delta,S.Fire);S.Fire=FMath::Max(0.f,S.Fire-Delta);
            while(S.BurnTick<=0 && Enemy->CurrentHP>0){ S.BurnTick+=.5f;UGameplayStatics::ApplyDamage(Enemy,3,P->GetController(),P,nullptr); }
        }
    }
}
void UCityWeaponEffects::Sound(UObject* Context,FName Name)
{
    if (!Context || !Context->GetWorld() || !Context->GetWorld()->AllowAudioPlayback()) return;
    static TMap<FName,TWeakObjectPtr<USoundWave>> Clips;
    auto& Weak=Clips.FindOrAdd(Name);
    if (!Weak.IsValid()) Weak=LoadObject<USoundWave>(nullptr,*(TEXT("/Game/VerticalCity/Audio/")+Name.ToString()));
    if (Weak.IsValid()) UGameplayStatics::PlaySound2D(Context,Weak.Get());
}





