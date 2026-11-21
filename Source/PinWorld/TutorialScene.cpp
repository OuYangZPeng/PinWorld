#include "TutorialScene.h"
#include "TutorialState.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Algo/Reverse.h"

namespace
{
constexpr float EarthRadius = 250.f;
constexpr float LandRadius = 253.f;
const TCHAR* ContinentPaths[] = {
    TEXT("/Game/Earth/Meshes/Continents/SM_Asia.SM_Asia"),
    TEXT("/Game/Earth/Meshes/Continents/SM_Europe.SM_Europe"),
    TEXT("/Game/Earth/Meshes/Continents/SM_Africa.SM_Africa"),
    TEXT("/Game/Earth/Meshes/Continents/SM_NorthAmerica.SM_NorthAmerica"),
    TEXT("/Game/Earth/Meshes/Continents/SM_SouthAmerica.SM_SouthAmerica"),
    TEXT("/Game/Earth/Meshes/Continents/SM_Oceania.SM_Oceania"),
    TEXT("/Game/Earth/Meshes/Continents/SM_Antarctica.SM_Antarctica")
};

// UE is left-handed (X forward, Y right, Z up). East must map to -Y so that, seen from outside, east is on the right.
FVector Globe(float Lon, float Lat, float Radius = LandRadius)
{
    const float A = FMath::DegreesToRadians(Lon), B = FMath::DegreesToRadians(Lat);
    return Radius * FVector(FMath::Cos(B)*FMath::Cos(A), -FMath::Cos(B)*FMath::Sin(A), FMath::Sin(B));
}
bool Inside(FVector2D P, const TArray<FVector2D>& Poly)
{
    bool Result = false;
    for (int32 I=0, J=Poly.Num()-1; I<Poly.Num(); J=I++)
        if ((Poly[I].Y>P.Y)!=(Poly[J].Y>P.Y) && P.X < (Poly[J].X-Poly[I].X)*(P.Y-Poly[I].Y)/(Poly[J].Y-Poly[I].Y)+Poly[I].X) Result=!Result;
    return Result;
}
bool InTriangle(FVector2D P, FVector2D A, FVector2D B, FVector2D C)
{
    const double D1=FVector2D::CrossProduct(B-A,P-A), D2=FVector2D::CrossProduct(C-B,P-B), D3=FVector2D::CrossProduct(A-C,P-C);
    return D1>=0 && D2>=0 && D3>=0;
}
// Ear clipping in lon/lat space; output triangles are counter-clockwise in lon/lat.
TArray<FVector2D> Triangulate(const TArray<FVector2D>& P)
{
    TArray<int32> Idx; for (int32 I=0; I<P.Num(); ++I) Idx.Add(I);
    double Area=0; for (int32 I=0; I<P.Num(); ++I) Area+=FVector2D::CrossProduct(P[I],P[(I+1)%P.Num()]);
    if (Area<0) Algo::Reverse(Idx);
    TArray<FVector2D> Out;
    while (Idx.Num()>3)
    {
        bool Clipped=false;
        for (int32 I=0; I<Idx.Num() && !Clipped; ++I)
        {
            const int32 N=Idx.Num(), A=Idx[(I+N-1)%N], B=Idx[I], C=Idx[(I+1)%N];
            const double Turn=FVector2D::CrossProduct(P[B]-P[A],P[C]-P[B]);
            if (FMath::IsNearlyZero(Turn)) { Idx.RemoveAt(I); Clipped=true; break; }
            if (Turn<0) continue;
            bool Empty=true;
            for (int32 K : Idx) if (K!=A && K!=B && K!=C && P[K]!=P[A] && P[K]!=P[B] && P[K]!=P[C] && InTriangle(P[K],P[A],P[B],P[C])) { Empty=false; break; }
            if (!Empty) continue;
            Out.Append({P[A],P[B],P[C]}); Idx.RemoveAt(I); Clipped=true;
        }
        if (!Clipped) break;
    }
    if (Idx.Num()==3) Out.Append({P[Idx[0]],P[Idx[1]],P[Idx[2]]});
    return Out;
}
// Split longest edge until every edge is short enough to follow the sphere curvature.
void Refine(FVector2D A, FVector2D B, FVector2D C, TArray<FVector2D>& Out, int32 Depth=0)
{
    const float AB=FVector2D::Distance(A,B), BC=FVector2D::Distance(B,C), CA=FVector2D::Distance(C,A);
    const float M=FMath::Max3(AB,BC,CA);
    if (M<=2.f || Depth>20) { Out.Append({A,B,C}); return; }
    if (M==AB) { const FVector2D D=(A+B)*.5f; Refine(A,D,C,Out,Depth+1); Refine(D,B,C,Out,Depth+1); }
    else if (M==BC) { const FVector2D D=(B+C)*.5f; Refine(A,B,D,Out,Depth+1); Refine(A,D,C,Out,Depth+1); }
    else { const FVector2D D=(C+A)*.5f; Refine(A,B,D,Out,Depth+1); Refine(D,B,C,Out,Depth+1); }
}
// Front face winding used by the existing procedural meshes in this project.
void AddFace(const TArray<FVector>& V, TArray<int32>& T, int32 A, int32 B, int32 C)
{
    const FVector Out=(V[A]+V[B]+V[C]);
    if (FVector::DotProduct(FVector::CrossProduct(V[B]-V[A],V[C]-V[A]),Out)<0) T.Append({A,B,C}); else T.Append({A,C,B});
}
template<typename F> void ForEachEdgePoint(const TArray<FVector2D>& Poly, F&& Fn)
{
    for (int32 J=0; J<Poly.Num(); ++J)
    {
        const FVector2D A=Poly[J], B=Poly[(J+1)%Poly.Num()];
        if ((A.Y<=-89.9f && B.Y<=-89.9f) || (FMath::Abs(A.X)>=179.9f && A.X==B.X)) continue;
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(A,B)/2.f));
        for (int32 S=0; S<Steps; ++S) Fn(FMath::Lerp(A,B,float(S)/Steps),FMath::Lerp(A,B,float(S+1)/Steps));
    }
}
}
const TArray<FVector2D>& ATutorialScene::Outline(int32 Index)
{
    // Coarse geographic coastlines, longitude/latitude degrees. Not proxy spheres.
    static const TArray<TArray<FVector2D>> Shapes = {
        {{30,40},{40,55},{60,70},{100,77},{150,70},{178,60},{160,50},{140,45},{130,30},{120,20},{110,0},{100,5},{90,22},{78,8},{68,25},{50,12},{35,30}},
        {{-10,36},{-10,44},{0,50},{-5,58},{10,60},{20,71},{32,70},{40,55},{30,40},{20,35},{12,42},{3,40}},
        {{-17,35},{10,37},{32,31},{43,12},{51,11},{42,-5},{35,-20},{20,-35},{12,-25},{9,-5},{-10,5},{-17,15}},
        {{-168,70},{-140,70},{-125,60},{-100,75},{-65,60},{-55,48},{-80,25},{-87,15},{-100,20},{-115,30},{-125,48},{-150,60},{-168,60}},
        {{-81,12},{-65,10},{-50,0},{-35,-7},{-42,-23},{-55,-35},{-68,-55},{-75,-45},{-80,-15}},
        {{113,-22},{122,-14},{135,-12},{143,-10},{153,-25},{150,-38},{135,-39},{115,-33}},
        {{-180,-70},{-140,-73},{-100,-72},{-65,-63},{-45,-75},{0,-70},{45,-68},{90,-67},{140,-70},{180,-70},{180,-90},{-180,-90}}
    };
    return Shapes[FMath::Clamp(Index,0,6)];
}
FVector2D ATutorialScene::CenterLL(int32 Index)
{
    static const FVector2D C[]={{95,42},{16,53},{18,0},{-110,48},{-60,-20},{135,-25},{0,-82}};
    return C[FMath::Clamp(Index,0,6)];
}
ATutorialScene::ATutorialScene()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Earth=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Earth"));
    Earth->SetupAttachment(RootComponent);
    Earth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> EarthAsset(TEXT("/Game/Earth/Meshes/SM_Earth.SM_Earth"));
    if (EarthAsset.Succeeded()) Earth->SetStaticMesh(EarthAsset.Object);
    Earth->SetRelativeScale3D(FVector(5.0f));
}
void ATutorialScene::BeginPlay() { Super::BeginPlay(); BuildScene(); }
void ATutorialScene::SetState(UTutorialState* InState) { State=InState; }
void ATutorialScene::BuildScene()
{
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Earth/Materials/M_Earth_Realistic.M_Earth_Realistic"));
    if (!Base) Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UnityMigration/M_Migration.M_Migration"));
    EarthMaterial=Base;
    Earth->SetMaterial(0,Base);
    auto* White=UMaterialInstanceDynamic::Create(Base,this);
    White->SetVectorParameterValue(TEXT("Color"),FLinearColor(.85f,.9f,.94f));
    CloudMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    for (int32 I=0; I<7; ++I)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(RootComponent); Mesh->RegisterComponent();
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        auto* Asset=LoadObject<UStaticMesh>(nullptr,ContinentPaths[I]);
        if (Asset) Mesh->SetStaticMesh(Asset);
        Mesh->SetRelativeScale3D(FVector(5.0f));
        auto* Mat=UMaterialInstanceDynamic::Create(Base,this);
        Mesh->SetMaterial(0,Mat);
        Continents.Add(Mesh); Materials.Add(Mat);
        const FVector2D Center=CenterLL(I);
        Centers.Add(Globe(Center.X,Center.Y));
    }
    for (int32 I=0; I<24; ++I)
    {
        auto* Cloud=NewObject<UStaticMeshComponent>(this); Cloud->SetupAttachment(RootComponent); Cloud->RegisterComponent();
        Cloud->SetStaticMesh(CloudMesh); Cloud->SetMaterial(0,White);
        Cloud->SetRelativeLocation(Globe(I*137.5f,-65+(I%9)*16,285));
        Cloud->SetRelativeScale3D(FVector(.75f,.5f,.22f)); Cloud->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Clouds.Add(Cloud);
    }
    auto* Light=GetWorld()->SpawnActor<ADirectionalLight>(FVector(500,-300,500),FRotator(-30,150,0));
    Light->SetActorRotation(FRotator(-30,150,0));
    auto* Fill=GetWorld()->SpawnActor<APointLight>(FVector(600,200,200),FRotator::ZeroRotator);
    Fill->PointLightComponent->SetIntensity(1500000); Fill->PointLightComponent->SetAttenuationRadius(2000);
    Camera=GetWorld()->SpawnActor<ACameraActor>(FVector(1250,-150,0),FRotator(0,180,0));
    auto* C=Camera->GetCameraComponent(); C->SetFieldOfView(48);
    C->PostProcessSettings.bOverride_AutoExposureMethod=true; C->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    C->PostProcessSettings.bOverride_AutoExposureBias=true; C->PostProcessSettings.AutoExposureBias=9;
    C->PostProcessSettings.bOverride_DepthOfFieldFocalDistance=true;
    C->PostProcessSettings.bOverride_DepthOfFieldFstop=true;
    C->PostProcessSettings.DepthOfFieldFocalDistance=1250; C->PostProcessSettings.DepthOfFieldFstop=32;
    GetWorld()->GetFirstPlayerController()->SetViewTarget(Camera);
    FocusContinent(2);
}
void ATutorialScene::FocusContinent(int32 Index)
{
    if (Index<0 || Index>=7) return;
    const FVector2D C=CenterLL(Index);
    ViewLon=C.X; ViewLat=C.Y;
    TargetRotation=FRotator(ViewLat,-ViewLon,0).Quaternion().Inverse();
}
void ATutorialScene::RotateBy(const FVector2D& Delta)
{
    const float Scale=90.f/FMath::Max(GlobeScreenRadius(),50.f);
    ViewLon=FMath::UnwindDegrees(ViewLon-Delta.X*Scale);
    ViewLat=FMath::Clamp(ViewLat+Delta.Y*Scale,-85.f,85.f);
    TargetRotation=FRotator(ViewLat,-ViewLon,0).Quaternion().Inverse();
}
void ATutorialScene::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!State) return;
    if (!State->Splash) CloudTime+=Dt;
    SetActorRotation(FQuat::Slerp(GetActorQuat(),TargetRotation,1-FMath::Exp(-Dt*10)));
    for (int32 I=0; I<Clouds.Num(); ++I)
    {
        Clouds[I]->SetRelativeLocation(Globe(I*137.5f,-65+(I%9)*16,285+FMath::Square(CloudTime)*110));
        Clouds[I]->SetVisibility(CloudTime<3.5f);
    }
    static const FLinearColor Colors[]={ { .95f,.55f,.15f },{.55f,.3f,.85f},{.2f,.7f,.3f},{.2f,.55f,.9f},{.85f,.3f,.4f},{.1f,.8f,.75f},{.65f,.75f,.95f} };
    for (int32 I=0; I<7; ++I)
    {
        const FLinearColor Color=State->Placed[I]?Colors[I]:(State->Selected==I?FLinearColor(.9f,.75f,.25f):FLinearColor(.48f,.53f,.58f));
        Continents[I]->SetMaterial(0,State->Placed[I]?EarthMaterial:Materials[I]);
        if (Materials[I]) Materials[I]->SetVectorParameterValue(TEXT("Color"),Color);
    }
    if (State->bTransitioning)
    {
        FocusContinent(State->SelectedContinent);
        const float A=FMath::SmoothStep(0.f,1.f,FMath::Clamp(State->TransitionTime/2.8f,0.f,1.f));
        Camera->SetActorLocation(FVector(FMath::Lerp(1250.f,268.f,A),FMath::Lerp(-150.f,0.f,A),0));
        auto& PP=Camera->GetCameraComponent()->PostProcessSettings;
        PP.DepthOfFieldFstop=FMath::Lerp(32.f,1.f,A);
        PP.DepthOfFieldFocalDistance=FMath::Lerp(1250.f,1400.f,A);
    }
}
float ATutorialScene::GlobeScreenRadius() const
{
    auto* PC=GetWorld()->GetFirstPlayerController();
    FVector2D A,B;
    if (!PC || !Camera || !PC->ProjectWorldLocationToScreen(GetActorLocation(),A) || !PC->ProjectWorldLocationToScreen(GetActorLocation()+Camera->GetActorUpVector()*LandRadius,B)) return 0;
    return FVector2D::Distance(A,B);
}
bool ATutorialScene::TargetScreen(int32 I,FVector2D& P) const
{
    return Centers.IsValidIndex(I) && GetWorld()->GetFirstPlayerController()->ProjectWorldLocationToScreen(GetActorTransform().TransformPosition(Centers[I]),P);
}
void ATutorialScene::TargetSegments(int32 Index,TArray<FVector2D>& Out) const
{
    Out.Reset();
    auto* PC=GetWorld()->GetFirstPlayerController();
    if (!PC || !Camera || Index<0 || Index>=7) return;
    const FVector Eye=Camera->GetActorLocation(), O=GetActorLocation();
    const FTransform X=GetActorTransform();
    ForEachEdgePoint(Outline(Index),[&](FVector2D P0,FVector2D P1)
    {
        const FVector U=X.TransformPosition(Globe(P0.X,P0.Y)), V=X.TransformPosition(Globe(P1.X,P1.Y));
        if (((U-O)|(Eye-U))<=0 || ((V-O)|(Eye-V))<=0) return;
        FVector2D SU,SV;
        if (PC->ProjectWorldLocationToScreen(U,SU) && PC->ProjectWorldLocationToScreen(V,SV)) { Out.Add(SU); Out.Add(SV); }
    });
}
void ATutorialScene::GhostSegments(int32 Index,TArray<FVector2D>& Out) const
{
    Out.Reset();
    auto* PC=GetWorld()->GetFirstPlayerController();
    if (!PC || !Camera || Index<0 || Index>=7) return;
    const FVector2D C=CenterLL(Index);
    const FTransform X(FRotator(C.Y,-C.X,0).Quaternion().Inverse(),GetActorLocation());
    const FVector Eye=Camera->GetActorLocation(), O=GetActorLocation();
    FVector2D Anchor;
    if (!PC->ProjectWorldLocationToScreen(X.TransformPosition(Globe(C.X,C.Y)),Anchor)) return;
    ForEachEdgePoint(Outline(Index),[&](FVector2D P0,FVector2D P1)
    {
        const FVector U=X.TransformPosition(Globe(P0.X,P0.Y)), V=X.TransformPosition(Globe(P1.X,P1.Y));
        if (((U-O)|(Eye-U))<=0 || ((V-O)|(Eye-V))<=0) return;
        FVector2D SU,SV;
        if (PC->ProjectWorldLocationToScreen(U,SU) && PC->ProjectWorldLocationToScreen(V,SV)) { Out.Add(SU-Anchor); Out.Add(SV-Anchor); }
    });
}
int32 ATutorialScene::PickContinent(FVector2D Screen) const
{
    FVector O,D; if (!GetWorld()->GetFirstPlayerController()->DeprojectScreenPositionToWorld(Screen.X,Screen.Y,O,D)) return INDEX_NONE;
    const float B=FVector::DotProduct(O,D), Disc=B*B-O.SizeSquared()+LandRadius*LandRadius;
    if (Disc<0) return INDEX_NONE;
    FVector P=GetActorQuat().UnrotateVector(O+D*(-B-FMath::Sqrt(Disc))).GetSafeNormal();
    FVector2D LL(FMath::RadiansToDegrees(FMath::Atan2(-P.Y,P.X)),FMath::RadiansToDegrees(FMath::Asin(P.Z)));
    for (int32 I=0; I<7; ++I) if (Inside(LL,Outline(I))) return I;
    return INDEX_NONE;
}
bool ATutorialScene::TryDrop(int32 I,const FVector2D& P,const FVector2D& Size)
{
    if (!State || !State->Placed.IsValidIndex(I) || State->Placed[I] || State->Splash || Size.X<=0) return false;
    // The ghost is anchored at the continent center, so a drop near the target center counts even if the cursor sits on a coastline gap.
    FVector2D Target;
    const bool NearCenter=TargetScreen(I,Target) && FVector2D::Distance(Target,P)<GlobeScreenRadius()*.12f;
    if (!NearCenter && PickContinent(P)!=I) return false;
    State->Placed[I]=true; State->Selected=INDEX_NONE; State->Completed=State->AllPlaced();
    UE_LOG(LogTemp,Display,TEXT("TUTORIAL_DROP PASS continent=%d completed=%d"),I,State->Completed);
    return true;
}
