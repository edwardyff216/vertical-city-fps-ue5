#include "Campaign/CityCampaignMode.h"

#if !UE_BUILD_SHIPPING
#include "ShooterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ACityCampaignMode::CaptureEnemyCue()
{
    ACityCampaignEnemy* Boss=nullptr;
    ACityCampaignEnemy* Provider=nullptr;
    for (ACityCampaignEnemy* E : Enemies)
    {
        if (!IsValid(E)) continue;
        if (E->IsBoss()) Boss=E;
        if (E->GetKind()==ECityFoe::Support) Provider=E;
    }
    AShooterCharacter* P=Player();
    if (!Boss || !P) { EndBot(false,TEXT("cue subject missing")); return; }
    const bool bBreaker=Boss->GetKind()==ECityFoe::Breaker;
    const int32 Frame=CueCaptureTick++;
    const FVector Center=Boss->GetActorLocation();
    if (Frame==0)
    {
        const float Height=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector Offset=bBreaker?FVector(-285,0,Height):FVector(-485,-185,Height);
        P->SetActorLocation(Center+Offset,false,nullptr,ETeleportType::TeleportPhysics);
    }
    const FVector Eye=P->GetFirstPersonCameraComponent()->GetComponentLocation();
    const FVector Look=Center+(bBreaker?FVector(0,0,-52):FVector(0,-64,25));
    PC()->SetControlRotation((Look-Eye).Rotation());
    if (bBreaker && Frame>=46 && Frame<65) P->DoMove(0,-1);
    if (!bBreaker && (Frame==26 || Frame==44 || Frame==62))
    {
        FHitResult Contact;
        Contact.ImpactPoint=Center+(Eye-Center).GetSafeNormal2D()*64+FVector(0,(Frame==44?26:-18),40);
        UGameplayStatics::ApplyPointDamage(Boss,8,(Center-Eye).GetSafeNormal(),Contact,PC(),P,nullptr);
    }
    if (!bBreaker && Frame==78 && Provider)
        UGameplayStatics::ApplyDamage(Provider,9999,PC(),P,nullptr);
    if (Frame==110) { EndBot(true,TEXT("enemy cue capture")); return; }
    FScreenshotRequest::RequestScreenshot(FPaths::Combine(EnemyCueFolder,FString::Printf(TEXT("frame-%03d.png"),Frame)),true,false);
}
#endif
