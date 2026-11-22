#include "Abilities/CitySkillVisuals.h"
#include "TimerManager.h"
#include "Abilities/CityAbilityCatalog.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "ImageUtils.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FCitySkillSheet* CombatSheet(FName Id)
{
    static const FCitySkillSheet Entries[] = {
#include "Abilities/CityCombatAtlas.inl"
    };
    static const TMap<FName,const FCitySkillSheet*> Sheets = []()
    {
        TMap<FName,const FCitySkillSheet*> Map;
        for (const auto& Entry : Entries) Map.Add(Entry.Id, &Entry);
        return Map;
    }();
    const auto* Found = Sheets.Find(Id);
    return Found ? *Found : nullptr;
}
}
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UCitySkillVisuals::UCitySkillVisuals()
{
    PrimaryComponentTick.bCanEverTick = false;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Sink(TEXT("/Game/AbsorbtionFX/FX/NS_Absorption_Purple.NS_Absorption_Purple"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SlashAsset(TEXT("/Game/VerticalCityArt/PrismShardTrail/NS_PrismShardTrail.NS_PrismShardTrail"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> LightMuzzleAsset(TEXT("/Game/NW_MuzzleFX/Particle_FX/FXS_NS_MuzzleFlash_00.FXS_NS_MuzzleFlash_00"));
    LightMuzzle = LightMuzzleAsset.Object;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> HeavyMuzzleAsset(TEXT("/Game/NW_MuzzleFX/Particle_FX/FXS_NS_MuzzleFlash_03.FXS_NS_MuzzleFlash_03"));
    HeavyMuzzle = HeavyMuzzleAsset.Object;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FrostLandingSystemAsset(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Area1.NS_Free_Magic_Area1"));
    FrostLandingSystem = FrostLandingSystemAsset.Object;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ArcHitSystemAsset(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Free_Magic_Hit1.NS_Free_Magic_Hit1"));
    ArcHitSystem = ArcHitSystemAsset.Object;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FireTrail(TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Fire.NS_ArrowTrail_Fire"));
    ShotTrails.Add(TEXT("fire"), FireTrail.Object);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> FrostTrail(TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Ice.NS_ArrowTrail_Ice"));
    ShotTrails.Add(TEXT("frost"), FrostTrail.Object);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> VoltTrail(TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Magic_2.NS_ArrowTrail_Magic_2"));
    ShotTrails.Add(TEXT("volt"), VoltTrail.Object);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BlastTrail(TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Fire_02.NS_ArrowTrail_Fire_02"));
    ShotTrails.Add(TEXT("blast"), BlastTrail.Object);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> VoidTrail(TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Magic.NS_ArrowTrail_Magic"));
    ShotTrails.Add(TEXT("void"), VoidTrail.Object);
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ArcaneTrail(TEXT("/Game/ArrowTrail/FX/NS_ArrowTrail_Holy.NS_ArrowTrail_Holy"));
    ShotTrails.Add(TEXT("arcane"), ArcaneTrail.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Chips(TEXT("/Game/VerticalCity/VFX/M_KatanaPrismHit"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Interrupt(TEXT("/Game/SlashTrail_SoftTofu/Niagara/Cyberpunk/NS_Hit_CyberPunk_Once"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Edge(TEXT("/Game/VerticalCity/VFX/M_KatanaPrismWave"));
    StrokePlane=Plane.Object;
    StrokeMaterial=Edge.Object;
    PrismHitMaterial=Chips.Object;
    InterruptSystem = Interrupt.Object;
    SinkSystem=Sink.Object;
    SlashSystem=SlashAsset.Object;
}

void UCitySkillVisuals::LoadAtlas()
{
    // Prepare the two short-lived effects before combat so the first swing can render on a cold launch.
    if (FApp::CanEverRender() && StrokePlane && StrokeMaterial && PrismHitMaterial && !Stroke)
    {
        Stroke=CreateStrokeCard(StrokeMaterial);
        StrokeInk=CastChecked<UMaterialInstanceDynamic>(Stroke->GetMaterial(0));
        // A dash can resolve multiple contacts in one frame.
        for (int32 Index=0; Index<2; ++Index) IdleHitCards.Add(CreateStrokeCard(PrismHitMaterial));
    }
    Meshes = {
        LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")),
        LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere")),
        LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone")),
        LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"))
    };
    BaseInk = LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    TArray<uint8> Png;
    if (FFileHelper::LoadFileToArray(Png, *(FPaths::ProjectContentDir() / TEXT("VerticalCity/Abilities/SkillAtlas.png"))))
    {
        Atlas = FImageUtils::ImportBufferAsTexture2D(Png);
        if (Atlas)
        {
            Atlas->Filter = TF_Nearest;
            Atlas->UpdateResource();
        }
    }
    Png.Reset();
    if (FFileHelper::LoadFileToArray(Png, *(FPaths::ProjectContentDir()/TEXT("VerticalCity/Abilities/CombatAtlas.png"))))
    {
        CombatAtlas=FImageUtils::ImportBufferAsTexture2D(Png);
        if (CombatAtlas) { CombatAtlas->Filter=TF_Nearest; CombatAtlas->UpdateResource(); }
    }
    ensureMsgf(CombatAtlas,TEXT("Missing or unreadable CombatAtlas.png"));
    ensureMsgf(Atlas, TEXT("Missing or unreadable VerticalCity/Abilities/SkillAtlas.png"));
}

void UCitySkillVisuals::Shape(FName Id, int32 Mesh, FVector At, FQuat Rotation, FVector Dimensions)
{
    if (!Meshes.IsValidIndex(Mesh) || !Meshes[Mesh]) return;
    if (!Parts.IsValidIndex(Used))
    {
        auto* Part = NewObject<UStaticMeshComponent>(GetOwner());
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetCastShadow(false);
        Part->RegisterComponent();
        Parts.Add(Part);
    }
    auto* Part = Parts[Used++].Get();
    Part->SetStaticMesh(Meshes[Mesh]);
    if (!Inks.Contains(Id) && BaseInk)
    {
        const auto* Def = FCityAbilityCatalog::Find(Id);
        auto* Ink = UMaterialInstanceDynamic::Create(BaseInk,this);
        FLinearColor Color=Def ? FLinearColor(Def->Color)*2.2f : FLinearColor(2.f,1.9f,1.5f);
        if (Id=="frost_floor") Color=FLinearColor(.024f,.42f,.92f);
        Ink->SetVectorParameterValue(TEXT("Color"),Color);
        Inks.Add(Id,Ink);
    }
    if (auto* Ink = Inks.Find(Id)) Part->SetMaterial(0,*Ink);
    Part->SetWorldTransform(FTransform(Rotation,At,Dimensions/100.0));
    Part->SetVisibility(true);
}

void UCitySkillVisuals::Rod(FName Id, FVector From, FVector To, float Width)
{
    const FVector Span = To-From;
    if (Span.IsNearlyZero()) return;
    Shape(Id,0,(From+To)*.5,Span.ToOrientationQuat(),FVector(Span.Size(), Width, Width));
}

void UCitySkillVisuals::Ring(FName Id, FVector At, float Radius, float Width, int32 Sides)
{
    FVector Prev = At + FVector(Radius,0,0);
    for (int32 I=1; I<=Sides; ++I)
    {
        const float Angle = 2.f*PI*I/Sides;
        FVector Next = At + FVector(FMath::Cos(Angle)*Radius, FMath::Sin(Angle)*Radius,0);
        Rod(Id,Prev,Next,Width);
        Prev = Next;
    }
}

void UCitySkillVisuals::Blade(FName Id, FVector At, FVector Forward, float Angle)
{
    // Bank the exposed blade face toward the eye, then counter-rotate the two wings.
    const FQuat Plane = Forward.ToOrientationQuat() * FQuat(FVector::RightVector,.74f) * FQuat(FVector::UpVector,Angle);
    const FVector Axis = Plane.GetAxisX();
    const FVector Point = At + Axis*18;
    Shape(Id,2,Point,FRotationMatrix::MakeFromZX(Axis,Plane.GetAxisY()).ToQuat(),FVector(22,9,72));
    Rod(NAME_None,At-Axis*17,At-Axis*37,8);
    Rod(Id,At-Axis*14-Plane.GetAxisY()*13,At-Axis*14+Plane.GetAxisY()*13,5);
}

void UCitySkillVisuals::Sprite(FName Id, bool bCast, FVector At, float Width)
{
    if (!bCast && Id == "arc_lance" && Particle(ArcHitSystem, At, FRotator::ZeroRotator, .42f, .45f)) return;
    const auto* Sheet = FCityAbilityCatalog::Sheet(Id,bCast);
    if (Sheet) Flashes.Add({Id,At,At,0,Sheet->Frames/Sheet->Fps,Width,0,bCast});
}
void UCitySkillVisuals::Flash(FName Id, FVector A, FVector B, float Width, float Life)
{
    Flashes.Add({Id,A,B,0,Life,Width,1,false});
}
void UCitySkillVisuals::Burst(FName Id, FVector At, float Radius, float Life)
{
    Flashes.Add({Id,At,At,0,Life,Radius,2,false});
    static const TMap<FName,FName> ImpactSheets = {
        {"nova","nova_burst"},{"hammer","hammer_burst"},{"charge","charge_burst"},{"storm","storm_burst"}
    };
    if (const auto* Sheet=ImpactSheets.Find(Id)) Combat(*Sheet,At,Radius*.70f);
}
void UCitySkillVisuals::DrawSprite(const FCitySkillFlash& Cue, FVector At)
{
    const bool bCombat = Cue.Kind==3;
    const auto* S = bCombat ? CombatSheet(Cue.Id) : FCityAbilityCatalog::Sheet(Cue.Id,Cue.bCast);
    UTexture2D* Source = bCombat ? CombatAtlas.Get() : Atlas.Get();
    if (!Source || !S) return;
    if (!Sprites.IsValidIndex(UsedSprites))
    {
        auto* Sprite = NewObject<UBillboardComponent>(GetOwner());
        Sprite->bIsScreenSizeScaled = false;
        Sprite->SetHiddenInGame(false);
        Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Sprite->SetCanEverAffectNavigation(false);
        Sprite->SetOpacityMaskRefVal(.16f);
        Sprite->RegisterComponent();
        Sprites.Add(Sprite);
    }
    auto* Sprite = Sprites[UsedSprites++].Get();
    const int32 Frame = FMath::Min(S->Frames-1,FMath::FloorToInt(Cue.Age*S->Fps));
    Sprite->SetSpriteAndUV(Source,S->X+Frame*S->Width,S->Width,S->Y,S->Height);
    Sprite->SetWorldLocation(At);
    Sprite->SetWorldScale3D(FVector(2.f*Cue.Width/S->Width));
    Sprite->SetVisibility(true);
}

void UCitySkillVisuals::BeginFrame(float Dt, FVector CastPoint)
{
    AdvanceTrails(Dt);
    AdvanceStroke(Dt);
    AdvancePrismHits(Dt);
    for (int32 Index=Particles.Num()-1; Index>=0; --Index)
    {
        auto& Active=Particles[Index];
        Active.Life-=Dt;
        if (!Active.Component.IsValid() || Active.Life<=0)
        {
            if (Active.Component.IsValid()) Active.Component->DestroyComponent();
            Particles.RemoveAtSwap(Index);
        }
    }
    Used=0;
    UsedSprites=0;
    for (int32 I=Flashes.Num()-1; I>=0; --I)
    {
        auto& Cue = Flashes[I];
        if (Cue.Age >= Cue.Life) { Flashes.RemoveAtSwap(I); continue; }
        const float T = Cue.Age/Cue.Life;
        if (Cue.Kind==0 || Cue.Kind==3) DrawSprite(Cue,Cue.bCast ? CastPoint : Cue.A);
        else if (Cue.Kind==1) Rod(Cue.Id,Cue.A,Cue.B,Cue.Width*(1.f-T*.7f));
        else Ring(Cue.Id,Cue.A,Cue.Width*(.2f+.8f*T),FMath::Max(3.f,8.f*(1-T)),18);
        Cue.Age += Dt;
    }
}
void UCitySkillVisuals::Emit(AActor* Viewer,FName Sheet,FVector At,float Width)
{
    if (auto* Visuals=Viewer ? Viewer->FindComponentByClass<UCitySkillVisuals>() : nullptr)
        Visuals->Combat(Sheet,At,Width);
}
void UCitySkillVisuals::Combat(FName Sheet,FVector At,float Width)
{
    if (const auto* S=CombatSheet(Sheet))
    {
        Flashes.Add({Sheet,At,At,0,S->Frames/S->Fps,Width,3,false});
        UE_LOG(LogTemp,Verbose,TEXT("CITY_VFX_SHEET %s world=%.3f"),*Sheet.ToString(),GetWorld()->GetTimeSeconds());
    }
}
UNiagaraComponent* UCitySkillVisuals::Particle(UNiagaraSystem* System,FVector At,FRotator Rotation,float Scale,float Life,FName TrailSchool)
{
    if (!System || !GetWorld() || !FApp::CanEverRender()) return nullptr;
    if (auto* Component=UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,System,At,Rotation,FVector(Scale),true,false))
    {
        if (System == InterruptSystem)
        {
            Component->SetVariableLinearColor(TEXT("User.Color"), FLinearColor(3.f,.56f,.085f));
            Component->SetVariableFloat(TEXT("User.Lifetime_Flare"), .11f);
        }
        else if (System == FrostLandingSystem)
        {
            Component->SetVariableFloat(TEXT("User.Scale_All"), .35f);
        }
        else if (System == ArcHitSystem)
        {
            Component->SetVariableLinearColor(TEXT("User.Color_Flash"), FLinearColor(.25f, .76f, 1.8f));
            Component->SetVariableLinearColor(TEXT("User.Color_Sparks1"), FLinearColor(.18f, .8f, 2.f));
            Component->SetVariableLinearColor(TEXT("User.Color_Spiral1"), FLinearColor(.35f, .82f, 1.5f));
            Component->SetVariableLinearColor(TEXT("User.Color_Smoke"), FLinearColor(.13f, .27f, .38f));
        }
        if (!TrailSchool.IsNone())
        {
            // Scale the author's width and glow multipliers for short, head-on FPS shots.
            const auto& Defaults = System->GetExposedParameters();
            for (FName Name : {FName(TEXT("User.Trail Size")), FName(TEXT("User.Emissive"))})
            {
                const FNiagaraVariableBase Parameter(FNiagaraTypeDefinition::GetFloatDef(), Name);
                if (Defaults.IndexOf(Parameter) != INDEX_NONE)
                {
                    const float Factor = Name == "User.Emissive" ? 1.45f : 2.1f;
                    Component->SetVariableFloat(Name, Defaults.GetParameterValue<float>(Parameter) * Factor);
                }
            }
            if (TrailSchool == "arcane")
                Component->SetVariableLinearColor(TEXT("User.Color"), FLinearColor(1.25f,.78f,.24f));
            else if (TrailSchool == "volt" || TrailSchool == "void")
            {
                const bool bVolt = TrailSchool == "volt";
                Component->SetVariableLinearColor(TEXT("User.ColorA"), bVolt ? FLinearColor(.08f,.54f,1.3f) : FLinearColor(.23f,.025f,.6f));
                Component->SetVariableLinearColor(TEXT("User.ColorB"), bVolt ? FLinearColor(.48f,1.1f,1.6f) : FLinearColor(.72f,.1f,1.2f));
            }
        }
        Component->Activate(true);
        Particles.Add({Component,Life});
        UE_LOG(LogTemp,Verbose,TEXT("CITY_VFX Niagara=%s"),*System->GetName());
        return Component;
    }
    return nullptr;
}

bool UCitySkillVisuals::MuzzleFlash(UStaticMeshComponent* Gun, FVector At, FRotator Direction, FName Kind, bool bFirstPerson)
{
    const bool bHeavy = Kind == "shotgun" || Kind == "hand_cannon";
    const float Scale = bHeavy ? .38f : Kind == "smg" || Kind == "sidearm" ? .20f : .27f;
    auto* Flash = Particle(bHeavy ? HeavyMuzzle : LightMuzzle, At, Direction, Scale, .09f);
    if (!Flash) return false;
    Flash->AttachToComponent(Gun, FAttachmentTransformRules::KeepWorldTransform);
    Flash->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
    return true;
}

void UCitySkillVisuals::FrostLanding(FVector At)
{
    Particle(FrostLandingSystem, At + FVector::UpVector * 4.f, FRotator::ZeroRotator, .85f, .75f);
}

bool UCitySkillVisuals::FireTrail(FName School, const FVector& Start, const FVector& End)
{
    const auto* System = ShotTrails.Find(School);
    const FVector Delta = End - Start;
    if (!System || Delta.IsNearlyZero()) return false;
    const float Duration = FMath::Clamp(float(Delta.Size()) / 16000.f, .15f, .25f);
    if (auto* Trail = Particle(*System, Start, Delta.Rotation(), .72f, Duration + .20f, School))
    {
        MovingTrails.Add({Trail, Start, End, -1.f, Duration});
        UE_LOG(LogTemp, Verbose, TEXT("CITY_AFFIX_TRAIL %s %s"), *School.ToString(), *(*System)->GetName());
        return true;
    }
    return false;
}

void UCitySkillVisuals::AdvanceTrails(float Delta)
{
    for (int32 Index = MovingTrails.Num() - 1; Index >= 0; --Index)
    {
        auto& Trail = MovingTrails[Index];
        if (auto* Component = Trail.Component.Get())
        {
            // Give Niagara one simulation tick at the muzzle before moving the emitter.
            if (Trail.Time < 0.f) { Trail.Time = 0.f; continue; }
            Trail.Time = FMath::Min(Trail.Time + Delta, Trail.Duration);
            Component->SetWorldLocation(FMath::Lerp(Trail.Start, Trail.End, Trail.Time / Trail.Duration));
            if (Trail.Time < Trail.Duration) continue;
            Component->Deactivate();
        }
        MovingTrails.RemoveAtSwap(Index);
    }
}

UNiagaraComponent* UCitySkillVisuals::BladeTrail(UStaticMeshComponent* Blade, bool bFirstPerson, float Life)
{
    if (!SlashSystem || !Blade || !Blade->GetStaticMesh() || !FApp::CanEverRender()) return nullptr;
    const FBox Bounds = Blade->GetStaticMesh()->GetBoundingBox();
    FVector Edge = Bounds.GetCenter();
    Edge.Y = Bounds.Min.Y * .64f; // Measured from the guard along the cutting edge.
    auto* Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(SlashSystem,Blade,NAME_None,Edge,
        FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false);
    if (Trail)
    {
        Trail->SetWorldScale3D(FVector(bFirstPerson ? .36f : .65f));
        Trail->SetFirstPersonPrimitiveType(bFirstPerson ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::None);
        Trail->AddTickPrerequisiteComponent(Blade);
        Trail->AddTickPrerequisiteActor(Blade->GetOwner());
        Trail->Activate(true);
        FTimerHandle Timer;
        GetWorld()->GetTimerManager().SetTimer(Timer,FTimerDelegate::CreateUObject(Trail,&UNiagaraComponent::Deactivate),Life,false);
        Particles.Add({Trail,Life+1.f});
        UE_LOG(LogTemp,Verbose,TEXT("CITY_KATANA_TRAIL system=%s blade=%s fp=%d duration=%.3f"),*SlashSystem->GetName(),*Blade->GetName(),bFirstPerson,Life);
    }
    return Trail;
}

void UCitySkillVisuals::SwingWave(FVector Eye, FVector Forward, float Reach, TConstArrayView<FVector> Contacts)
{
    const FVector Aim=Forward.GetSafeNormal();
    FVector End=Eye+Aim*FMath::Min(Reach,240.f);
    if (Contacts.IsEmpty())
    {
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit,Eye,End,ECC_Visibility,
                FCollisionQueryParams(SCENE_QUERY_STAT(CitySwordWave),false,GetOwner())))
            End=Eye+Aim*FMath::Max(0.f,Hit.Distance-5.f);
    }
    else
    {
        // Keep a cleave on the target nearest the reticle. Every enemy actually damaged gets its own chips.
        End=Contacts[0];
        float Best=-2.f;
        for (const FVector& Point : Contacts)
        {
            const float Alignment=FVector::DotProduct(Aim,(Point-Eye).GetSafeNormal());
            if (Alignment>Best) { Best=Alignment; End=Point; }
        }
    }
    const float Clear=FVector::Distance(Eye,End);
    const bool ShowWave=Clear>=46.f;
    FRotator Tilt=Aim.Rotation();
    Tilt.Roll=-24.f;
    if (ShowWave)
    {
        const FVector Start=Eye+Aim*FMath::Min(85.f,Clear*.52f)-FVector::UpVector*9.f;
        // Keep the angular width bounded when a pursuing enemy reaches the camera.
        StartStroke(Start,End,Tilt,FMath::Clamp(Clear*.64f,24.f,116.f),.19f);
    }
    for (const FVector& Point : Contacts) PrismHit(Point,(Point-Eye).Rotation(),ShowWave?.12f:0.f);
    UE_LOG(LogTemp,Verbose,TEXT("CITY_PRISM_WAVE world=%.4f contacts=%d flight=%d"),GetWorld()->GetTimeSeconds(),Contacts.Num(),ShowWave);
}

UStaticMeshComponent* UCitySkillVisuals::CreateStrokeCard(UMaterialInterface* Material)
{
    auto* Card=NewObject<UStaticMeshComponent>(GetOwner());
    Card->SetMobility(EComponentMobility::Movable);
    Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Card->SetGenerateOverlapEvents(false);
    Card->SetCanEverAffectNavigation(false);
    Card->SetCastShadow(false);
    Card->SetStaticMesh(StrokePlane);
    Card->SetMaterial(0,UMaterialInstanceDynamic::Create(Material,Card));
    Card->SetVisibility(false);
    Card->RegisterComponent();
    Card->PrecachePSOs();
    return Card;
}

void UCitySkillVisuals::PrismHit(FVector At, FRotator Heading, float Delay, float SizeScale)
{
    if (!StrokePlane || !PrismHitMaterial || !FApp::CanEverRender()) return;
    UStaticMeshComponent* Card=IdleHitCards.IsEmpty() ? CreateStrokeCard(PrismHitMaterial) : IdleHitCards.Pop().Get();
    const FRotationMatrix View(Heading);
    Card->SetWorldLocationAndRotation(At,FRotationMatrix::MakeFromXY(View.GetUnitAxis(EAxis::Y),View.GetUnitAxis(EAxis::Z)).ToQuat());
    Card->SetWorldScale3D(FVector(.94f*SizeScale,.76f*SizeScale,1.f));
    PrismHits.Add({Card,-Delay});
}

void UCitySkillVisuals::AdvancePrismHits(float Delta)
{
    constexpr float Life=.23f;
    for (int32 I=PrismHits.Num()-1; I>=0; --I)
    {
        auto& Hit=PrismHits[I];
        auto* Card=Hit.Card.Get();
        Hit.Age+=Delta;
        if (!Card || Hit.Age>=Life)
        {
            if (Card)
            {
                Card->SetVisibility(false);
                IdleHitCards.Add(Card);
            }
            PrismHits.RemoveAtSwap(I);
            continue;
        }
        if (Hit.Age<0.f) continue;
        if (!Card->IsVisible())
            UE_LOG(LogTemp,Verbose,TEXT("CITY_PRISM_HIT world=%.4f position=%s"),GetWorld()->GetTimeSeconds(),*Card->GetComponentLocation().ToCompactString());
        Card->SetVisibility(true);
        CastChecked<UMaterialInstanceDynamic>(Card->GetMaterial(0))->SetScalarParameterValue(TEXT("StrokePhase"),Hit.Age/Life);
    }
}

void UCitySkillVisuals::StartStroke(FVector Start,FVector End,FRotator Heading,float Width,float Life)
{
    if (!StrokePlane || !StrokeMaterial || !FApp::CanEverRender()) return;
    if (!Stroke)
    {
        Stroke=CreateStrokeCard(StrokeMaterial);
        StrokeInk=CastChecked<UMaterialInstanceDynamic>(Stroke->GetMaterial(0));
    }
    StrokeStart=Start; StrokeEnd=End; StrokeTime=0.f; StrokeLife=Life; StrokeWidth=Width;
    const FRotationMatrix View(Heading);
    Stroke->SetWorldRotation(FRotationMatrix::MakeFromXY(View.GetUnitAxis(EAxis::Y),View.GetUnitAxis(EAxis::Z)).ToQuat());
    Stroke->SetVisibility(true);
    AdvanceStroke(0.f);
}
void UCitySkillVisuals::AdvanceStroke(float Delta)
{
    if (StrokeTime<0.f || !Stroke) return;
    StrokeTime+=Delta;
    const float T=FMath::Clamp(StrokeTime/StrokeLife,0.f,1.f);
    if (T>=1.f) { Stroke->SetVisibility(false); StrokeTime=-1.f; return; }
    const float Width=StrokeWidth*FMath::Lerp(.65f,1.15f,T);
    Stroke->SetWorldLocation(FMath::Lerp(StrokeStart,StrokeEnd,1.f-FMath::Square(1.f-T)));
    Stroke->SetWorldScale3D(FVector(Width/100.f,Width*.42f/100.f,1.f));
    StrokeInk->SetScalarParameterValue(TEXT("StrokePhase"),T);
}

void UCitySkillVisuals::BossInterrupt(FVector At)
{
    Particle(InterruptSystem,At,FRotator::ZeroRotator,.72f,.46f);
}

void UCitySkillVisuals::DashSlash(FVector At, FRotator Direction)
{
    const FVector Aim=Direction.Vector();
    FHitResult Hit;
    FVector End=At + Aim*165.f;
    FCollisionObjectQueryParams Solid;
    Solid.AddObjectTypesToQuery(ECC_WorldStatic);
    if (GetWorld()->LineTraceSingleByObjectType(Hit,At,End,Solid,FCollisionQueryParams(SCENE_QUERY_STAT(CityDashWave),false,GetOwner())))
        End=Hit.ImpactPoint-Aim*5.f;
    Direction.Roll=27.f;
    StartStroke(At,End,Direction,225.f,.32f);
}

void UCitySkillVisuals::StartSink(FVector At)
{
    Particle(SinkSystem,At,FRotator::ZeroRotator,.82f,4.f);
}
void UCitySkillVisuals::SinkField(FVector At,float Radius,int32 Stored,float Time)
{
    const FName Id("bullet_sink");
    const float Core=42.f+Stored*.9f;
    Ring(Id,At-FVector::UpVector*112.f,Radius,4.f,24);
    Shape(Id,1,At,FQuat::Identity,FVector(Core));
    // Traveling crests move inward in world space, keeping the absorption readable on Low.
    for (int32 Arm=0; Arm<4; ++Arm)
    {
        const float T=FMath::Frac(Time*1.55f+.25f*Arm);
        const float A=Time*1.4f+Arm*PI*.5f;
        const FVector Dir(FMath::Cos(A),FMath::Sin(A),.22f*FMath::Sin(A*2));
        const float R=FMath::Lerp(Radius*.48f,Core*.36f,T);
        Rod(Id,At+Dir*R,At+Dir*(R+27.f),5.f);
    }
}
void UCitySkillVisuals::FrostField(FVector At,float Radius,float Time)
{
    const FName Id("frost_floor");
    const FVector Origin=At+FVector::UpVector*4;
    Ring(Id,Origin,Radius,9.f,24);
    // Raised forked ice crystals keep the perimeter legible when seen nearly edge-on.
    for (int32 I=0; I<8; ++I)
    {
        const FRotator Turn(0,I*45.f,0);
        const FVector Dir=Turn.Vector(), Tangent=Turn.RotateVector(FVector::RightVector);
        const FVector Root=Origin+Dir*(Radius*.84f);
        const float Height=36.f+9.f*FMath::Sin(Time*2.5f+I);
        const FVector Tip=Root+FVector::UpVector*Height+Dir*16;
        Rod(Id,Root,Tip,10);
        Rod(NAME_None,Tip,Tip+FVector::UpVector*9+Dir*3,5);
        Rod(Id,Root+FVector::UpVector*12,Root+Tangent*18+FVector::UpVector*(Height*.8f),5);
        Rod(Id,Root,Root-Dir*Radius*.23f+Tangent*16,5);
    }
}
void UCitySkillVisuals::EndPlay(EEndPlayReason::Type Reason)
{
    Reset();
    Super::EndPlay(Reason);
}
void UCitySkillVisuals::EndFrame()
{
    for (int32 I=Used; I<Parts.Num(); ++I) Parts[I]->SetVisibility(false);
    for (int32 I=UsedSprites; I<Sprites.Num(); ++I) Sprites[I]->SetVisibility(false);
}
void UCitySkillVisuals::Reset()
{
    for (auto& Active : Particles) if (Active.Component.IsValid()) Active.Component->DestroyComponent();
    Particles.Reset();
    MovingTrails.Reset();
    for (auto& Hit : PrismHits) if (auto* Card=Hit.Card.Get()) Card->DestroyComponent();
    PrismHits.Reset();
    for (const auto& Card : IdleHitCards) if (IsValid(Card)) Card->DestroyComponent();
    IdleHitCards.Reset();
    StrokeTime=-1.f;
    if (Stroke) Stroke->SetVisibility(false);
    Flashes.Reset();
    Used=UsedSprites=0;
    EndFrame();
}

