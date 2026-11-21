#include "MigrationPreview.h"
#include "MigrationState.h"
#include "MigrationScene.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneView.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"

bool UMigrationDragPreview::Begin(int32 Entry)
{
    Clear();
    if (!State->Entries.IsValidIndex(Entry)) return false;
    auto* Composite = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UnityMigration/M_PreviewComposite.M_PreviewComposite"));
    if (!Composite) return false;
    // GameMode inherits AInfo's hidden-in-game flag. Preview primitives must
    // belong to a visible actor, not the GameMode, or the renderer culls them.
    AActor* PreviewActor = GetWorld()->SpawnActor<AActor>();
    if (!PreviewActor) return false;
    DragPreviewActor = PreviewActor;
    ForegroundTarget = NewObject<UTextureRenderTarget2D>(this);
    ForegroundTarget->ClearColor = FLinearColor(0, 0, 0, 1);
    ForegroundTarget->InitCustomFormat(1, 1, PF_FloatRGBA, false);
    ForegroundMaterial = UMaterialInstanceDynamic::Create(Composite, this);
    ForegroundMaterial->SetTextureParameterValue(TEXT("Preview"), ForegroundTarget);
    Capture = NewObject<USceneCaptureComponent2D>(PreviewActor);
    PreviewActor->SetRootComponent(Capture);
    Capture->RegisterComponent();
    Capture->TextureTarget = ForegroundTarget;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->ShowFlags.SetAtmosphere(false);
    Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetVolumetricFog(false);
    Capture->ShowFlags.SetTemporalAA(false);
    Capture->ShowFlags.SetBloom(false);
    // Independent colored copies leave the destination geometry and door poses intact.
    for (const FString& Id : State->Entries[Entry].Renderers)
    {
        auto* Source = Scene->Renderers.FindRef(Id);
        const auto* Slots = Scene->SavedSlots.Find(Id);
        if (!Source || !Slots) continue;
        auto* Copy = NewObject<UProceduralMeshComponent>(PreviewActor);
        Copy->RegisterComponent();
        Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Copy->SetCastShadow(false);
        Copy->SetVisibleInSceneCaptureOnly(true);
        Capture->ShowOnlyComponent(Copy);
        for (int32 Section = 0; Section < Source->GetNumSections(); ++Section)
            if (auto* Mesh = Source->GetProcMeshSection(Section)) Copy->SetProcMeshSection(Section, *Mesh);
        for (int32 M = 0; M < Slots->Num(); ++M) Copy->SetMaterial(M, (*Slots)[M]);
        Copy->SetWorldTransform(Source->GetComponentTransform());
        DragPreview.Add(Copy);
    }
    return true;
}

void UMigrationDragPreview::Clear()
{
    if (Capture) { Capture->TextureTarget = nullptr; Capture->DestroyComponent(); }
    Capture = nullptr;
    ForegroundMaterial = nullptr;
    ForegroundTarget = nullptr;
    for (auto Copy : DragPreview) if (Copy.IsValid()) Copy->DestroyComponent();
    DragPreview.Reset();
    if (DragPreviewActor.IsValid()) DragPreviewActor->Destroy();
    DragPreviewActor.Reset();
}

void UMigrationDragPreview::MovePreview(FVector2D Screen)
{
    if (!State->Entries.IsValidIndex(State->Selected)) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    int32 W = 0, H = 0;
    PC->GetViewportSize(W, H);
    FVector Origin, Direction;
    if (!PC->DeprojectScreenPositionToWorld(Screen.X * W, Screen.Y * H, Origin, Direction)) return;
    FVector Camera; FRotator Rotation;
    PC->GetPlayerViewPoint(Camera, Rotation);
    const FVector Center = State->Entries[State->Selected].Bounds.GetCenter();
    const double Depth = FMath::Max(100.0, FVector::DotProduct(Center - Camera, Rotation.Vector()));
    const double Denom = FVector::DotProduct(Direction, Rotation.Vector());
    if (Denom <= 0) return;
    const FVector Position = Camera + Direction * (Depth / Denom);
    const double Scale = Screen.X >= 0.81 ? FMath::Min(1.0, Depth * 0.08 / FMath::Max(1.0, State->Entries[State->Selected].Bounds.GetExtent().Size())) : 1.0;
    for (auto Copy : DragPreview)
        if (Copy.IsValid())
            Copy->SetWorldTransform(FTransform(FQuat::Identity, Position - Center * Scale, FVector(Scale)));
}

void UMigrationDragPreview::DrawForeground(AHUD* HUD)
{
    if (!Capture || DragPreview.IsEmpty()) return;
    auto* PC = GetWorld()->GetFirstPlayerController();
    auto* Player = PC ? PC->GetLocalPlayer() : nullptr;
    if (!Player || !Player->ViewportClient) return;
    FSceneViewProjectionData Projection;
    if (!Player->GetProjectionData(Player->ViewportClient->Viewport, Projection)) return;
    const FIntRect Rect = Projection.GetConstrainedViewRect();
    if (Rect.Width() <= 0 || Rect.Height() <= 0) return;
    if (ForegroundTarget->SizeX != Rect.Width() || ForegroundTarget->SizeY != Rect.Height())
        ForegroundTarget->ResizeTarget(Rect.Width(), Rect.Height());
    FVector Camera; FRotator Rotation;
    PC->GetPlayerViewPoint(Camera, Rotation);
    Capture->SetWorldLocationAndRotation(Camera, Rotation);
    Capture->bUseCustomProjectionMatrix = true;
    Capture->CustomProjectionMatrix = Projection.ProjectionMatrix;
    // Separate depth buffer: opaque sections still occlude each other, never the scene.
    Capture->CaptureScene();
    HUD->DrawMaterial(ForegroundMaterial, Rect.Min.X, Rect.Min.Y, Rect.Width(), Rect.Height(), 0, 0, 1, 1);
}

void UMigrationDragPreview::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Clear();
    Super::EndPlay(EndPlayReason);
}
