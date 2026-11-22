#include "Campaign/CityRoomShell.h"
#include "Components/WidgetComponent.h"
#include "Misc/App.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "CityGeometry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ShooterCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/ConstructorHelpers.h"

ACityRoomShell::ACityRoomShell()
{
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Structure"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    Box=Mesh.Object;
    // The engine cube defaults to WorldGridMaterial. Keep the parameterized material in cooked builds.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> StructuralBase(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    StructuralMaterial=StructuralBase.Object;
    static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Crate(TEXT("/Game/VerticalCity/Warehouse/SM_CargoA.SM_CargoA"));
    static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Wall(TEXT("/Game/VerticalCity/Warehouse/SM_WallSample.SM_WallSample"));
    static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Pipes(TEXT("/Game/VerticalCity/Warehouse/SM_PipeSample.SM_PipeSample"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StoreCrate(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Props/SM_Crate_01_A.SM_Crate_01_A"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StorePillar(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Constructions/SM_Pillar_01.SM_Pillar_01"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> StoreGenerator(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Props/SM_Generator_01_A.SM_Generator_01_A"));
    FabCargo=StoreCrate.Object; FabPillar=StorePillar.Object; FabGenerator=StoreGenerator.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FloorTileAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Constructions/SM_Floor_01_A"));
    FloorTile=FloorTileAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CeilingTileAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Constructions/SM_Ceiling_01_A"));
    CeilingTile=CeilingTileAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> WallTileAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Constructions/SM_Wall_01"));
    WallTile=WallTileAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DoorLeafAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Constructions/SM_Door_01_B"));
    DoorLeaf=DoorLeafAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SupplyCaseAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Props/SM_Crate_02_A"));
    SupplyCase=SupplyCaseAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> AmmoDrumAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Props/SM_Barrel"));
    AmmoDrum=AmmoDrumAsset.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> LampAsset(TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Models/Props/SM_Lamp_01_A"));
    Lamp=LampAsset.Object;
    Cargo=Crate.Get(); WallPanel=Wall.Get(); Pipe=Pipes.Get();
}
UStaticMeshComponent* ACityRoomShell::Block(FVector At,FVector Size,FLinearColor Color,bool Solid,FRotator Rot)
{
    return CityGeometry::Part(this,Box,At,Size,Color,Solid,Rot);
}
void ACityRoomShell::Dress(UStaticMesh* Mesh,FVector Center,FVector Size,FRotator Orientation)
{
    if(!Mesh) return;
    const FBoxSphereBounds Bounds=Mesh->GetBounds();
    const FVector Extent=Bounds.BoxExtent.ComponentMax(FVector(.1));
    const FVector Scale=Size/(Extent*2);
    auto*& Batch=ArtBatches.FindOrAdd(Mesh);
    if(!Batch) Batch=CreateBatch(Mesh);
    Batch->AddInstance(FTransform(Orientation,Center-Orientation.RotateVector(Bounds.Origin*Scale),Scale));
}
// Individually movable art for doors and removable supplies. Size is mesh-local.
UStaticMeshComponent* ACityRoomShell::ArtPart(UStaticMesh* Model,FVector Center,FVector Size,FRotator Facing)
{
    const auto Bounds=Model->GetBounds();
    const FVector Scale=Size/(2*Bounds.BoxExtent.ComponentMax(FVector(.1)));
    auto* Part=NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Part);
    Part->SetupAttachment(RootComponent);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetStaticMesh(Model);
    Part->SetRelativeTransform(FTransform(Facing,Center-Facing.RotateVector(Bounds.Origin*Scale),Scale));
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->RegisterComponent();
    return Part;
}
UStaticMeshComponent* ACityRoomShell::Supply(UStaticMesh* Model,FVector Center,FVector Size,const TCHAR* Mark,FColor Color)
{
    auto* Part=ArtPart(Model,Center,Size,FRotator::ZeroRotator);
    auto* Icon=NewObject<UTextRenderComponent>(this);
    AddInstanceComponent(Icon);
    Icon->SetupAttachment(Part);
    Icon->SetAbsolute(false,true,true);
    Icon->SetWorldRotation(FRotator(0,180,0));
    Icon->SetHorizontalAlignment(EHTA_Center);
    Icon->SetText(FText::FromString(Mark));
    Icon->SetTextRenderColor(Color);
    Icon->SetWorldSize(31);
    Icon->RegisterComponent();
    Icon->SetWorldLocation(GetActorTransform().TransformPosition(Center+FVector(0,0,Size.Z*.5+6)));
    return Part;
}
static void RemoveSupply(UStaticMeshComponent* Part)
{
    TArray<USceneComponent*> Children;
    Part->GetChildrenComponents(false,Children);
    for (auto* Child:Children) Child->DestroyComponent();
    Part->DestroyComponent();
}
UInstancedStaticMeshComponent* ACityRoomShell::CreateBatch(UStaticMesh* Mesh)
{
    auto* Batch=NewObject<UInstancedStaticMeshComponent>(this);
    AddInstanceComponent(Batch);
    Batch->SetupAttachment(RootComponent);
    Batch->SetStaticMesh(Mesh);
    Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Batch->SetGenerateOverlapEvents(false);
    Batch->SetCanEverAffectNavigation(false);
    Batch->RegisterComponent();
    return Batch;
}
void ACityRoomShell::Trim(FVector Center,FVector Size,FLinearColor Color)
{
    auto*& Batch=TrimBatches.FindOrAdd(Color);
    if(!Batch)
    {
        Batch=CreateBatch(Box);
        Batch->SetMaterial(0,CityGeometry::TintedMaterial(Color));
    }
    Batch->AddInstance(FTransform(FQuat::Identity,Center,Size/100.f));
}
void ACityRoomShell::Label(FVector At,const FString& Text,FColor Color,float Height)
{
    auto* Label=NewObject<UTextRenderComponent>(this);
    AddInstanceComponent(Label);
    Label->SetupAttachment(RootComponent);
    Label->SetRelativeLocationAndRotation(At,FRotator(0,180,0));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetTextRenderColor(Color);
    Label->SetText(FText::FromString(Text));
    Label->SetWorldSize(Height);
    Label->RegisterComponent();
}
void ACityRoomShell::Light(FVector At,FLinearColor Hue,float Intensity,float Radius)
{
    auto* L=NewObject<UPointLightComponent>(this);
    AddInstanceComponent(L);
    L->SetupAttachment(RootComponent);
    L->SetRelativeLocation(At);
    L->SetMobility(EComponentMobility::Movable);
    L->SetIntensityUnits(ELightUnits::Lumens);
    L->SetLightColor(Hue);
    L->SetIntensity(Intensity);
    L->SetAttenuationRadius(Radius);
    L->SetCastShadows(false);
    L->RegisterComponent();
}
void ACityRoomShell::Build(int32 Layout,int32 Depth,int32 Boss,int32 Encounter,bool bThirdUpper)
{
    bExtraUpper = bThirdUpper;
    AmmoDrop(FVector(-650,390,42));
    Medkit=Supply(SupplyCase,FVector(-655,-470,38),FVector(66,40,42),TEXT("+"),FColor(75,255,125));
    const FLinearColor Iron(.075,.095,.125),Concrete(.30,.35,.39),Steel(.17,.23,.27),Stripe(.92,.52,.08);
    const FLinearColor Theme[]={{.53,.31,.16},{.65,.72,.70},{.18,.48,.56},{.64,.35,.15},{.20,.45,.44},
        {.38,.36,.55},{.54,.40,.20},{.35,.44,.24},{.23,.33,.40},{.54,.24,.31}};
    const FLinearColor Accent=Theme[FMath::Clamp(Layout,0,9)];
    Block(FVector(0,0,-30),FVector(3200,2350,60),Concrete)->SetVisibility(false);
    Block(FVector(-1580,0,335),FVector(50,2350,670),Steel);
    Block(FVector(1570,0,335),FVector(50,2350,670),Iron);
    Block(FVector(0,0,680),FVector(3200,2350,30),Iron);
    for (int X=0; X<8; ++X)
        for (int Y=0; Y<12; ++Y)
        {
            const FVector Center(-1400+400*X,(Y-5.5)*195,0);
            Dress(FloorTile,Center-FVector(0,0,2),FVector(400,195,7));
            Dress(CeilingTile,Center+FVector(0,0,660),FVector(400,195,14));
        }
    // Panels face into the room. Keep the walkable shell behind the surface seams.
    for (int Side : {-1,1})
        for (int Y=0;Y<6;++Y)
            for (int Tier=0;Tier<2;++Tier)
                Dress(WallTile,FVector(Side*1533,(Y-2.5)*387,167+334*Tier),FVector(387,24,334),FRotator(0,Side*90,0));
    for (int Side : {-1,1})
    {
        Block(FVector(0,Side*1160,335),FVector(3200,50,670),Accent);
        for (int X=-1390;X<=1400;X+=460)
        {
            Block(FVector(X,Side*1118,320),FVector(32,45,645),Steel);
            for (int Tier=0;Tier<2;++Tier)
                Dress(WallTile,FVector(X+168,Side*1126,167+334*Tier),FVector(460,19,334),FRotator(0,Side<0?0:180,0));
            if(X<1250) Dress(Pipe,FVector(X+168,Side*1086,480),FVector(454,30,62));
            Trim(FVector(X+180,Side*1119,290),FVector(280,6,15),Stripe);
        }
        Trim(FVector(0,Side*1100,489),FVector(3080,32,28),Iron);
        for(int X : {-1020,0,920})
        {
            Dress(Lamp,FVector(X,Side*605,604),FVector(241,63,20));
            Trim(FVector(X,Side*605,595),FVector(220,28,5),FLinearColor(2.4,2.1,1.7));
        }
    }
    for (int X=-1380;X<1500;X+=460)
        Trim(FVector(X,0,640),FVector(35,2290,47),Steel);
    // The rear mezzanine and inclined slab share the same walkable upper surface.
    const int32 DeckTiles = bExtraUpper ? 5 : 3;
    const float DeckWidth = DeckTiles * 260.f;
    Block(FVector(1240,-1115.f + DeckWidth*.5f,260),FVector(610,DeckWidth,40),Steel);
    const float Angle=FMath::RadiansToDegrees(FMath::Atan2(280.f,840.f));
    Block(FVector(526,-720,122),FVector(FMath::Sqrt(840.f*840.f+280.f*280.f),390,38),Concrete,true,FRotator(Angle,0,0));
    for (int I=0;I<4;++I)
    {
        const float T=(I+.5f)/4;
        Dress(FloorTile,FVector(106+840*T,-720,280*T-2),FVector(222,388,7),FRotator(Angle,0,0));
    }
    for (int X : {1088,1392})
        for(int Y=0;Y<DeckTiles;++Y)
            Dress(FloorTile,FVector(X,-985+260*Y,278),FVector(304,260,7));
    for (int X=-900;X<1250;X+=140)
        for (int Sign : {-1,1})
        {
            if (Sign<0 && X>60) continue;
            Trim(FVector(X,Sign*720,2),FVector(69,9,2),Sign<0?FLinearColor(.16,.80,.74):Stripe);
        }
    for (int X : {970,1450})
        for (int Y : {-990,-1220 + DeckTiles*260}) Block(FVector(X,Y,125),FVector(44,44,250),Steel);
    // Cover placements preserve the two side approach lanes and the middle firing lane.
    auto Crate=[&](float X,float Y,float W,float H)
    {
        auto* Collider=Block(FVector(X,Y,H/2),FVector(118,W,H),Accent);
        Collider->SetVisibility(false);
        const int Across=FMath::Max(1,FMath::RoundToInt(W/120.f));
        const int Tiers=FMath::Max(1,FMath::RoundToInt(H/84.f));
        for (int Col=0;Col<Across;++Col)
            for (int Level=0;Level<Tiers;++Level)
            {
                const FVector Size(118,W/Across,H/Tiers);
                Dress(FabCargo,FVector(X,Y-W*.5+(Col+.5)*Size.Y,(Level+.5)*Size.Z),Size);
            }
    };
    auto Column=[&](FVector Center,FVector Size)
    {
        Block(Center,Size,Steel)->SetVisibility(false);
        Dress(FabPillar,Center,Size);
    };
    switch(Layout)
    {
        case 2:
            for (int Side : {-1,1}) Column(FVector(155,Side*390,150),FVector(45,51,300));
            break;
        case 3: Crate(70,360,310,164); Crate(460,-310,160,120); break;
        case 5:
            for (int X : {-165,550}) for (int Y : {-340,345})
                Column(FVector(X,Y,220),FVector(110,105,440));
            break;
        case 4: case 8: Crate(-135,-270,164,245); Crate(485,310,140,290); break;
        case 6: case 7: Crate(-120,340,295,144); Crate(450,-290,270,137); break;
        default: Crate(-70,-310,180,188); Crate(440,352,170,145); break;
    }
    for (int I=0; I<(Boss==INDEX_NONE ? 3 : 2); ++I)
    {
        const bool Upper = IsUpperGate(I);
        const FVector Base = GateBase(I);
        const float Y = Base.Y, Z = Base.Z;
        const FLinearColor Trim=Upper?FLinearColor(.09,.88,.74):Stripe;
        for(int Sign : {-1,1}) Block(FVector(1320,Y+Sign*151,Z+127),FVector(58,32,254),Trim);
        Block(FVector(1320,Y,Z+252),FVector(58,334,31),Trim);
        Barriers.Add(Block(FVector(1350,Y,Z+111),FVector(24,273,222),Steel));
        Barriers.Last()->SetVisibility(false);
        for (int Side : {-1,1})
            GateLeaves.Add(ArtPart(DoorLeaf,FVector(1346,Y+Side*68.25,Z+111),FVector(136.5,24,222),FRotator(0,Side*90,0)));

    }
    // Keep native proportions. The static collision proxy matches the visible silhouette at the perimeter.
    auto SetPiece=[&](UStaticMesh* Mesh,FVector Foot,float Scale)
    {
        if(!Mesh) return;
        const FVector Size=Mesh->GetBounds().BoxExtent*(2.f*Scale);
        const FVector Center=Foot+FVector(0,0,Size.Z*.5);
        auto* Proxy=Block(Center,Size,Iron);
        Proxy->SetVisibility(false);
        Dress(Mesh,Center,Size);
    };
    for(int Side : {-1,1})
        SetPiece(FabPillar,FVector(1280,Side*1008,Side<0?280:0),.85f);
    if(Layout==0 || Layout==1 || Layout==3 || Layout==7)
        SetPiece(FabCargo,FVector(-585,987,0),1.35f);
    if(Encounter==2 || Boss==2) SetPiece(FabGenerator,FVector(280,990,0),.92f);
    const TCHAR* Names[]={TEXT("OFFSET DEPOT"),TEXT("WHITE DEPOT"),TEXT("SKYBRIDGE"),TEXT("LOADING DOCK"),
        TEXT("PIPE GALLERY"),TEXT("RELAY COURT"),TEXT("SWITCHBACK"),TEXT("FREIGHT LANES"),TEXT("RELAY STATION"),TEXT("ENGINE HALL")};
    Label(FVector(1530,0,555),FString::Printf(TEXT("%02d   /   %s"),Depth+1,Names[Layout]),FColor(218,225,220),54);
    FLinearColor Key=Boss==0 ? FLinearColor(.50,.84,1) : Boss==1 ? FLinearColor(1,.48,.22) :
        Boss==2 ? FLinearColor(.39,1,.70) : FLinearColor(1,.84,.68);
    Light(FVector(-850,0,495),FLinearColor(.83,.90,1),130000,2050);
    Light(FVector(560,-470,535),Key,125000,1750);
    Light(FVector(900,640,450),FLinearColor(1,.76,.55),85000,1350);
}
FVector ACityRoomShell::GateBase(int32 Slot) const
{
    const bool Upper = IsUpperGate(Slot);
    const float Y = Slot == 2 ? (Upper ? -240.f : 365.f) : GateY(Upper);
    return FVector(ThresholdX, Y, Upper ? 280.f : 0.f);
}

int32 ACityRoomShell::CrossedGate(const FVector& From, const FVector& To) const
{
    if (From.X >= ThresholdX || To.X < ThresholdX) return INDEX_NONE;
    const FVector Crossing = FMath::Lerp(From, To, (ThresholdX - From.X) / (To.X - From.X));
    for (int32 Slot = 0; Slot < Barriers.Num(); ++Slot)
    {
        if (Barriers[Slot]->GetCollisionEnabled() != ECollisionEnabled::NoCollision) continue;
        const FVector Local = Crossing - GateBase(Slot);
        if (FMath::Abs(Local.Y) < 136.f && Local.Z > 0.f && Local.Z < 222.f) return Slot;
    }
    return INDEX_NONE;
}

void ACityRoomShell::SetGateOpen(int32 Slot)
{
    GateLeaves[Slot*2]->SetVisibility(false);
    GateLeaves[Slot*2+1]->SetVisibility(false);
    Barriers[Slot]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACityRoomShell::RevealGate(int32 Slot, const FString& Title, const FString& Encounter, const FString& Reward, const FString& Detail)
{
    SetGateOpen(Slot);
    if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized()) return;
    auto* Sign = NewObject<UWidgetComponent>(this);
    AddInstanceComponent(Sign);
    Sign->SetupAttachment(RootComponent);
    Sign->SetWidgetSpace(EWidgetSpace::World);
    Sign->SetDrawSize(FVector2D(660, 222));
    Sign->SetTintColorAndOpacity(FLinearColor(16.f,16.f,16.f,1.f));
    Sign->SetManuallyRedraw(true);
    Sign->SetRelativeLocationAndRotation(GateBase(Slot) + FVector(-108,0,312), FRotator(0,180,0));
    Sign->SetRelativeScale3D(FVector(.50f));
    Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sign->SetGenerateOverlapEvents(false);
    Sign->SetCanEverAffectNavigation(false);
    Sign->SetCastShadow(false);
    Sign->RegisterComponent();
    const FLinearColor Accent = IsUpperGate(Slot) ? FLinearColor(.23f,.98f,.86f) : FLinearColor(1.f,.71f,.26f);
    auto Line = [](const FString& Text, int32 Size, FLinearColor Color, bool Bold)
    {
        return SNew(STextBlock).Text(FText::FromString(Text)).ColorAndOpacity(Color)
            .Font(FCoreStyle::GetDefaultFontStyle(Bold ? "Bold" : "Regular", Size));
    };
    Sign->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.002f,.004f,.007f)).Padding(FMargin(14,5))
        [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Line(Title,34,Accent,true)]
        +SVerticalBox::Slot().AutoHeight()[Line(Encounter,31,FLinearColor::White,true)]
        +SVerticalBox::Slot().AutoHeight()[Line(Reward,29,FLinearColor::White,true)]
        +SVerticalBox::Slot().AutoHeight()[Line(Detail,24,FLinearColor(.74f,.85f,.88f),false)]]);
    Sign->RequestRenderUpdate();
}

void ACityRoomShell::OpenExtraction()
{
    RevealGate(1, TEXT("↑ 上行"), TEXT("撤离出口"), TEXT("穿门完成本次行动"), FString());
}
void ACityRoomShell::AmmoDrop(FVector At)
{
    Ammo.Add(Supply(AmmoDrum,At,FVector(44,39,52),TEXT("III"),FColor(70,220,255)));
}
void ACityRoomShell::CollectSupplies(AShooterCharacter* P,bool Interact)
{
    if(!P || P->IsDead()) return;
    const FVector At=P->GetActorLocation();
    for(int I=Ammo.Num()-1;I>=0;--I)
    {
        if(FVector::DistSquared(Ammo[I]->Bounds.Origin,At)>200*200) continue;
        P->AwardCityAmmo(.35f);
        RemoveSupply(Ammo[I]); Ammo.RemoveAtSwap(I);
    }
    if(Interact && IsValid(Medkit) && FVector::DistSquared(Medkit->Bounds.Origin,At)<280*280)
    {
        P->HealCity(30);
        RemoveSupply(Medkit);
        Medkit=nullptr;
    }
}
void ACityRoomShell::WarnReinforcements(bool Active)
{
    for(UStaticMeshComponent* M : Warnings) if(IsValid(M)) M->DestroyComponent();
    Warnings.Reset();
    if(!Active) return;
    for(const FVector At : {FVector(-380,-480,4),FVector(-390,465,4)})
    {
        for(int I=0;I<12;++I)
        {
            const float Yaw=I*30.f;
            const FVector Radial=FRotator(0,Yaw,0).Vector();
            Warnings.Add(Block(At+Radial*143,FVector(12,48,4),FLinearColor(2.3,.60,.06),false,FRotator(0,Yaw,0)));
        }
    }
}

void ACityRoomShell::ClearGeneratedComponents()
{
    TArray<UActorComponent*> Generated;
    for(UActorComponent* Part : GetInstanceComponents()) if(IsValid(Part)) Generated.Add(Part);
    for(UActorComponent* Part : Generated) Part->DestroyComponent();
    ArtBatches.Reset(); TrimBatches.Reset();
    GateLeaves.Reset(); Barriers.Reset(); Ammo.Reset(); Warnings.Reset(); Medkit=nullptr;
}
