#include "Campaign/CityCampaignEnemy.h"
#include "Campaign/CityGeometry.h"
#include "Abilities/CityAbilities.h"
#include "Abilities/CitySkillVisuals.h"
#include "ShooterCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

void ACityCampaignEnemy::ConfigureCues()
{
    if (Kind==ECityFoe::Breaker)
    {
        FloorWarning=CityGeometry::Part(this,CuePlane,FVector::ZeroVector,FVector(2*StompRadius,2*StompRadius,100),FLinearColor::White);
        FloorWarning->SetAbsolute(true,true,true);
        FloorWarning->SetVisibility(false);
        FloorWarning->SetCastShadow(false);
        WarningInk=FloorWarning->CreateDynamicMaterialInstance(0,FloorMaterial);
        return;
    }
    if (Kind==ECityFoe::Support || Kind==ECityFoe::Spire) return;
    const float Width=GetCapsuleComponent()->GetScaledCapsuleRadius()*2.55f;
    const float Height=GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2.12f;
    ShieldShell=CityGeometry::Part(this,CueBall,FVector::ZeroVector,FVector(Width,Width,Height),FLinearColor::White);
    ShieldShell->SetVisibility(false);
    ShieldShell->SetCastShadow(false);
    ShellInk=ShieldShell->CreateDynamicMaterialInstance(0,ShellMaterial);
    ShieldPatch=CityGeometry::Part(this,CueBall,FVector::ZeroVector,FVector(90,90,22),FLinearColor::White);
    ShieldPatch->SetCastShadow(false);
    ShieldPatch->SetVisibility(false);
    if (auto* Ink=ShieldPatch->CreateDynamicMaterialInstance(0,DomeMaterial))
    {
        Ink->SetVectorParameterValue(TEXT("Main Color"),FLinearColor(.035f,1,.65f));
        Ink->SetVectorParameterValue(TEXT("Pattern Color"),FLinearColor(.13f,1,.82f));
        Ink->SetVectorParameterValue(TEXT("Edge Color"),FLinearColor(.35f,1,1));
    }
}

void ACityCampaignEnemy::UpdateShield(ACityCampaignEnemy* Provider,APawn* Viewer,float Dt)
{
    if (Provider)
        CityGeometry::Segment(Tether,GetActorLocation()+FVector::UpVector*55,Provider->GetActorLocation()+FVector::UpVector*55,6);
    else
    {
        Tether->SetVisibility(false);
        if (bLinked)
        {
            UCitySkillVisuals::Emit(Viewer,TEXT("frost_break"),GetActorLocation()+FVector::UpVector*30,IsBoss()?245.f:165.f);
            ShieldFlashLeft=0;
            BreakLeft=.38f;
        }
    }
    bLinked=Provider!=nullptr;
    ShieldFlashLeft=FMath::Max(0.f,ShieldFlashLeft-Dt);
    if (!ShieldPatch) return;
    ShieldPatch->SetVisibility(bLinked && ShieldFlashLeft>0);
    BreakLeft=bLinked?0.f:FMath::Max(0.f,BreakLeft-Dt);
    ShieldShell->SetVisibility(bLinked || BreakLeft>0);
    if (!bLinked && BreakLeft<=0) return;
    const float Fracture=bLinked?0.f:1.f-BreakLeft/.38f;
    const float Diameter=GetCapsuleComponent()->GetScaledCapsuleRadius()*2.55f;
    const float Height=GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2.12f;
    ShieldShell->SetRelativeScale3D(FVector(Diameter,Diameter,Height)*(1.f+Fracture*.26f)/100.f);
    if (ShellInk) ShellInk->SetScalarParameterValue(TEXT("Progress"),bLinked?ShieldFlashLeft/.22f:-Fracture);

}

void ACityCampaignEnemy::FlashShield(const FDamageEvent& Event,const AActor* Source)
{
    if (!ShieldPatch) return;
    const FVector Center=GetActorLocation();
    const bool bPoint=Event.IsOfType(FPointDamageEvent::ClassID);
    const auto* Hit=bPoint?&static_cast<const FPointDamageEvent&>(Event).HitInfo:nullptr;
    const FVector Contact=Hit?FVector(Hit->ImpactPoint):(Source?Source->GetActorLocation():Center+GetActorForwardVector());
    FVector Out=(Contact-Center).GetSafeNormal2D();
    if (Out.IsNearlyZero()) Out=GetActorForwardVector();
    const float Height=GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Z=Hit?FMath::Clamp(float(Contact.Z-Center.Z),-Height*.8f,Height*.8f):Height*.28f;
    const FVector At=Center+Out*(GetCapsuleComponent()->GetScaledCapsuleRadius()+7)+FVector::UpVector*Z;
    if (ShellInk)
    {
        const FVector Direction=(At-Center).GetSafeNormal();
        ShellInk->SetVectorParameterValue(TEXT("ImpactDir"),FLinearColor(Direction.X,Direction.Y,Direction.Z));
        ShellInk->SetScalarParameterValue(TEXT("Progress"),1);
    }
    ShieldPatch->SetWorldLocationAndRotation(At,FRotationMatrix::MakeFromZ(Out).Rotator());
    ShieldPatch->SetWorldScale3D(FVector(.95,.85,.20)*(IsBoss()?1.45f:1.f));
    ShieldPatch->SetVisibility(true);
    ShieldFlashLeft=.22f;
    bLinked=true;
}

bool ACityCampaignEnemy::BeginStomp()
{
    const FVector From=GetActorLocation();
    FHitResult Hit;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityStompFloor),false,this);
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,From,From-FVector::UpVector*(GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+35),Objects,Query)
        || Hit.ImpactNormal.Z<.88f) return false;
    StompCenter=Hit.ImpactPoint;
    bStomping=true;
    Windup=StompWindup;
    GetCharacterMovement()->StopMovementImmediately();
    FloorWarning->SetWorldLocation(StompCenter+FVector::UpVector*4);
    FloorWarning->SetWorldScale3D(FVector(2*StompRadius/100.f,2*StompRadius/100.f,1));
    FloorWarning->SetWorldRotation(FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator());
    if (WarningInk) WarningInk->SetScalarParameterValue(TEXT("Progress"),0);
    FloorWarning->SetVisibility(true);
    UE_LOG(LogTemp,Verbose,TEXT("City breaker stomp warning center=%s"),*StompCenter.ToString());
    return true;
}

void ACityCampaignEnemy::CancelStomp()
{
    if (!bStomping) return;
    bStomping=false;
    FloorWarning->SetVisibility(false);
    GetMesh()->SetRelativeLocation(FVector(0,0,-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
}

void ACityCampaignEnemy::LandStomp(AShooterCharacter& Player)
{
    const FVector Impact=StompCenter+FVector::UpVector*8;
    const FVector Target=Player.GetActorLocation();
    const double FeetZ=Target.Z-Player.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const bool bInDisk=FVector::DistSquared2D(Target,StompCenter)<=FMath::Square(StompRadius) && FMath::Abs(FeetZ-StompCenter.Z)<=95;
    FCollisionObjectQueryParams Walls;
    Walls.AddObjectTypesToQuery(ECC_WorldStatic);
    Walls.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CityStompCover),false,this);
    Query.AddIgnoredActor(&Player);
    auto* Skills=Player.FindComponentByClass<UCityAbilities>();
    const FVector Start=Impact+FVector::UpVector*36;
    const bool bHit=bInDisk && !GetWorld()->LineTraceTestByObjectType(Start,Target,Walls,Query)
        && !(Skills && Skills->StrikeBarrier(Start,Target,14));
    if (Shockwave)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,Shockwave,Impact,FRotator::ZeroRotator,FVector(.8f));
    if (bHit) UGameplayStatics::ApplyDamage(&Player,24.5f,nullptr,this,nullptr);
    UE_LOG(LogTemp,Display,TEXT("City breaker stomp landed hit=%d range=%.1f"),bHit,FVector::Dist2D(Target,StompCenter));
    FinishCharge();
    AftershockLeft=.38f;
    if (WarningInk) WarningInk->SetScalarParameterValue(TEXT("Progress"),-.01f);
    FloorWarning->SetVisibility(true);
}
