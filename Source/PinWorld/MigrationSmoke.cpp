#include "MigrationSmoke.h"
#include "MigrationSession.h"
#include "MigrationController.h"
#include "MigrationState.h"
#include "MigrationScene.h"
#include "MigrationPreview.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomMigration, Log, All);

namespace { int32 MenuSmokeStage = 0; }
void UMigrationSmoke::RunInteractionSmoke()
{
    bSmoke = true;
    bool OK = true;
    int32 WhiteSlots = 0;
    for (const auto& Pair : Scene->Renderers)
        for (int32 S = 0; S < Pair.Value->GetNumMaterials(); ++S)
        {
            OK &= Pair.Value->GetMaterial(S) == Scene->WhiteMaterial;
            ++WhiteSlots;
        }
    int32 VW = 0, VH = 0;
    Controller->GetViewportSize(VW, VH);
    if (State->IsHallOfPrayer)
        for (int32 E = 6; E <= 8; ++E)
        {
            FBox2D Rect(ForceInit);
            FVector2D Center;
            Controller->TargetRect(E, Rect);
            Controller->ProjectWorldLocationToScreen(State->Entries[E].Bounds.GetCenter(), Center);
            UE_LOG(LogRoomMigration, Display, TEXT("HALL_TARGET entry=%d viewport=%dx%d rectPx=%s..%s centerPx=%s bounds=%s"), E, VW, VH,
                *(Rect.Min * FVector2D(VW, VH)).ToString(), *(Rect.Max * FVector2D(VW, VH)).ToString(), *Center.ToString(), *State->Entries[E].Bounds.ToString());
        }
    FSceneViewProjectionData Projection;
    auto* Player = Controller->GetLocalPlayer();
    const bool FullViewport = Player && Player->ViewportClient
        && Player->GetProjectionData(Player->ViewportClient->Viewport, Projection)
        && Projection.GetConstrainedViewRect() == FIntRect(0, 0, VW, VH);
    OK &= FullViewport;
    UE_LOG(LogRoomMigration, Display, TEXT("VIEWPORT_COORDINATES %s viewport=%dx%d"), FullViewport ? TEXT("PASS") : TEXT("FAIL"), VW, VH);
    const FTransform InitialCamera = Controller->OrbitView->GetActorTransform();
    Controller->OrbitCamera(FVector2D(0.12, 0.03));
    OK &= !InitialCamera.Equals(Controller->OrbitView->GetActorTransform());
    const float BeforeZoom = Controller->OrbitDistance;
    Controller->PointerWheel(FVector2D(0.4, 0.5), -1);
    OK &= Controller->OrbitDistance < BeforeZoom;
    Controller->ResetCamera();
    OK &= InitialCamera.Equals(Controller->OrbitView->GetActorTransform(), 0.01);
    Controller->PointerWheel(FVector2D(0.9, 0.5), 1);
    OK &= State->Scroll == FMath::Min(1, FMath::Max(0, State->VisibleEntries().Num() - 3));
    Controller->PointerWheel(FVector2D(0.9, 0.5), -1);
    OK &= State->Scroll == 0;
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_INPUT whiteSlots=%d orbitZoomResetPaging=%d"), WhiteSlots, OK);
    if (State->IsHallOfPrayer)
    {
        bool EquivalentPlaced = false;
        for (int32 A = 0; A < State->Entries.Num() && !EquivalentPlaced; ++A)
            for (int32 B = 0; B < State->Entries.Num() && !EquivalentPlaced; ++B)
            {
                if (A == B || State->Entries[A].EquivalenceGroup.IsEmpty()
                    || State->Entries[A].EquivalenceGroup != State->Entries[B].EquivalenceGroup) continue;
                FBox2D RA(ForceInit), RB(ForceInit);
                if (!Controller->TargetRect(A, RA) || !Controller->TargetRect(B, RB)) continue;
                for (int32 Y = 1; Y < 10 && !EquivalentPlaced; ++Y)
                    for (int32 X = 1; X < 10 && !EquivalentPlaced; ++X)
                    {
                        const FVector2D P = RB.Min + RB.GetSize() * FVector2D(X / 10.0, Y / 10.0);
                        if (RA.IsInside(P) || P.X >= .8 || P.Y <= .1) continue;
                        if (Session->BeginDrag(A))
                        {
                            EquivalentPlaced = Session->EndDrag(P) && !State->Entries[A].Placed && State->PlacedCount() == 1;
                            for (auto& Entry : State->Entries) Entry.Placed = false;
                            Scene->UpdateAppearance();
                        }
                    }
            }
        OK &= EquivalentPlaced;
        UE_LOG(LogRoomMigration, Display, TEXT("HALL_EQUIVALENT_STAIRS %s"), EquivalentPlaced ? TEXT("PASS") : TEXT("FAIL"));
    }
    int32 Placed = 0, Blocked = 0, Cancelled = 0, Missed = 0, Clicked = 0;
    for (int32 D = 0; D < State->Doors.Num(); ++D)
    {
        const int32* OwnerEntry = State->EntryById.Find(State->Doors[D].Owner);
        if (OwnerEntry && !State->Entries[*OwnerEntry].Placed && Session->ToggleDoor(D)) OK = false;
    }
    for (int32 Pass = 0; Pass < State->Entries.Num() + 1 && State->PlacedCount() < State->Entries.Num(); ++Pass)
    {
        bool Progress = false;
        for (int32 E = 0; E < State->Entries.Num(); ++E)
        {
            if (State->Entries[E].Placed || !State->VisibleEntries().Contains(E)) continue;
            FVector2D Target;
            if (!Controller->TargetScreen(E, Target)) { UE_LOG(LogRoomMigration, Error, TEXT("ROOM_TEST no-target entry=%d name=%s bounds=%s"), E, *State->Entries[E].Name, *State->Entries[E].Bounds.ToString()); OK = false; continue; }
            const TArray<int32> Queue = State->VisibleEntries();
            State->Scroll = FMath::Clamp(Queue.IndexOfByKey(E), 0, FMath::Max(0, Queue.Num() - 3));
            const int32 Slot = Queue.IndexOfByKey(E) - State->Scroll;
            Controller->PointerDown(AMigrationPlayerController::SlotRect(Slot).GetCenter());
            if (State->Selected != E) { OK = false; continue; }
            ++Clicked;
            TMap<FString, FTransform> BeforeDrag;
            for (const FString& Id : State->Entries[E].Renderers)
                if (auto* R = Scene->Renderers.FindRef(Id)) BeforeDrag.Add(Id, R->GetComponentTransform());
            Preview->MovePreview(Target + FVector2D(0.07, 0));
            OK &= Preview->DragPreview.Num() == BeforeDrag.Num();
            for (const auto& Pair : BeforeDrag)
            {
                auto* R = Scene->Renderers.FindRef(Pair.Key);
                OK &= R->GetComponentTransform().Equals(Pair.Value);
                for (int32 S = 0; S < R->GetNumMaterials(); ++S) OK &= R->GetMaterial(S) == Scene->WhiteMaterial;
            }
            for (auto Copy : Preview->DragPreview)
            {
                OK &= Copy.IsValid();
                if (Copy.IsValid())
                {
                    OK &= Copy->IsRegistered() && Copy->IsVisible() && !Copy->bHiddenInGame;
                    OK &= Copy->GetOwner() && !Copy->GetOwner()->IsHidden();
                    OK &= Copy->GetOwner() == Preview->DragPreviewActor.Get();
                    OK &= Copy->bVisibleInSceneCaptureOnly;
                    OK &= Preview->Capture && Preview->Capture->PrimitiveRenderMode == ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
                    OK &= Preview->Capture && Preview->Capture->ShowOnlyComponents.Contains(Copy.Get());
                    OK &= Preview->Capture && Preview->Capture->CaptureSource == ESceneCaptureSource::SCS_SceneColorHDR;
                    OK &= Preview->ForegroundMaterial && Preview->ForegroundMaterial->GetBlendMode() == BLEND_Translucent;
                    for (int32 S = 0; S < Copy->GetNumMaterials(); ++S) OK &= Copy->GetMaterial(S) != Scene->WhiteMaterial;
                }
            }
            if (State->IsHallOfPrayer && E >= 6 && E <= 8)
            {
                // Aim at actual mesh center, not the test helper's rectangle center.
                FVector2D Pixel;
                Controller->ProjectWorldLocationToScreen(State->Entries[E].Bounds.GetCenter(), Pixel);
                Target = Pixel / FVector2D(VW, VH);
                Preview->MovePreview(Target);
                bool Aligned = FullViewport;
                for (auto Copy : Preview->DragPreview)
                    if (Copy.IsValid())
                    {
                        FVector2D PreviewPixel;
                        Controller->ProjectWorldLocationToScreen(Copy->CalcBounds(Copy->GetComponentTransform()).Origin, PreviewPixel);
                        // Compare corresponding source geometry: manifest bounds
                        // can be conservative and need not equal the mesh bounds.
                        FVector2D SourcePixel;
                        const FString& Id = State->Entries[E].Renderers[0];
                        const auto* Source = Scene->Renderers.FindRef(Id);
                        Controller->ProjectWorldLocationToScreen(Source->CalcBounds(Source->GetComponentTransform()).Origin, SourcePixel);
                        // Deprojection quantizes input to integer pixels.
                        Aligned &= FMath::Abs(PreviewPixel.X - SourcePixel.X) <= 1.01
                            && FMath::Abs(PreviewPixel.Y - SourcePixel.Y) <= 1.01;
                        UE_LOG(LogRoomMigration, Display, TEXT("HALL_PREVIEW entry=%d source=%s preview=%s"), E, *SourcePixel.ToString(), *PreviewPixel.ToString());
                    }
                // Old Canvas-origin shift made a visually aligned drag miss all
                // three targets at 900x470: 32px horizontal (or vertical in tall views).
                FBox2D Raw(ForceInit);
                Controller->TargetRect(E, Raw);
                const bool OldShiftMiss = !State->CanPlace(E, Target - FVector2D(32.0 / VW, 0), Raw);
                OK &= Aligned;
                UE_LOG(LogRoomMigration, Display, TEXT("HALL_VISUAL_DRAG entry=%d aligned=%d old32pxShiftMiss=%d pixel=%s"), E, Aligned, OldShiftMiss, *Pixel.ToString());
            }
            Session->CancelDrag();
            OK &= Preview->DragPreview.IsEmpty() && !Preview->DragPreviewActor.IsValid();
            if (State->Selected != INDEX_NONE || State->Entries[E].Placed) OK = false; else ++Cancelled;
            for (const auto& Pair : BeforeDrag)
                OK &= Scene->Renderers[Pair.Key]->GetComponentTransform().Equals(Pair.Value);
            if (!Session->BeginDrag(E) || Session->EndDrag(FVector2D(-1, -1))) OK = false; else ++Missed;
            int32 Gate = State->ClosedDoor(E);
            if (Gate != INDEX_NONE)
            {
                if (!Session->BeginDrag(E) || Session->EndDrag(Target) || State->Entries[E].Placed) OK = false; else ++Blocked;
                while ((Gate = State->ClosedDoor(E)) != INDEX_NONE)
                {
                    FBox2D DoorRect(ForceInit);
                    bool Opened = false;
                    if (Controller->ProjectBounds(Scene->DoorBounds(Gate), DoorRect))
                        for (int32 Y = 1; Y < 10 && !Opened; ++Y)
                            for (int32 X = 1; X < 10 && !Opened; ++X)
                            {
                                const FVector2D Point = DoorRect.Min + DoorRect.GetSize() * FVector2D(X / 10.0, Y / 10.0);
                                Controller->PointerDown(Point);
                                Opened = State->Doors[Gate].Open;
                            }
                    if (!Opened) break;
                }
                if (State->ClosedDoor(E) != INDEX_NONE) continue;
            }
            if (!Session->BeginDrag(E) || !Session->EndDrag(Target)) { OK = false; continue; }
            ++Placed; Progress = true;
            if (Session->BeginDrag(E) || Session->EndDrag(Target)) OK = false;
        }
        if (!Progress) break;
    }
    for (int32 D = 0; D < State->Doors.Num(); ++D)
    {
        auto& Door = State->Doors[D];
        for (const TCHAR* Command : {TEXT("firstOpen"), TEXT("close"), TEXT("open")})
        {
            if (!Door.CanClose && FString(Command) == TEXT("close")) continue;
            Door.Motion = Command; Door.Time = 0;
            Scene->UpdateDoors(1000);
            const auto* Motion = Door.Motions.Find(Door.Motion);
            bool Match = Motion && !Motion->Frames.IsEmpty();
            double MaxTranslation = 0;
            if (Match) for (const auto& Pair : Motion->Frames.Last().Renderers)
                if (auto* R = Scene->Renderers.FindRef(Pair.Key))
                {
                    Match &= R->GetComponentTransform().Equals(Pair.Value, 0.001);
                    MaxTranslation = FMath::Max(MaxTranslation, Pair.Value.GetTranslation().Size());
                }
            if (Match)
            {
                // Exercise every exported timestamp, not only the final open/closed pose.
                for (const auto& Frame : Motion->Frames)
                {
                    Door.Time = Frame.Time;
                    Scene->UpdateDoors(0);
                    for (const auto& Pair : Frame.Renderers)
                        if (auto* R = Scene->Renderers.FindRef(Pair.Key)) Match &= R->GetComponentTransform().Equals(Pair.Value, 0.001);
                }
            }
            OK &= Match;
            UE_LOG(LogRoomMigration, Display, TEXT("ROOM_DOOR_MOTION index=%d command=%s frames=%d endpointMatch=%d maxTranslationCm=%.6f"), D, Command, Motion ? Motion->Frames.Num() : 0, Match, MaxTranslation);
        }
    }
    OK &= State->PlacedCount() == State->Entries.Num();
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_SMOKE %s level=%s placed=%d/%d panelClicks=%d cancelled=%d wrongDrops=%d blocked=%d camera=%d"), OK ? TEXT("PASS") : TEXT("FAIL"), State->IsTiananmen ? TEXT("Tiananmen") : State->IsTaiheHall ? TEXT("TaiheHall") : State->IsHallOfPrayer ? TEXT("HallOfPrayer") : State->IsFoxy ? TEXT("Foxy") : TEXT("Room"), Placed, State->Entries.Num(), Clicked, Cancelled, Missed, Blocked, Controller->bCameraReady);
    FPlatformMisc::RequestExitWithStatus(false, OK ? 0 : 1);
}

void UMigrationSmoke::Tick()
{
    if (!Session->bLoadSucceeded)
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("RoomMigrationSmoke")) && !bSmoke)
        {
            bSmoke = true;
            UE_LOG(LogRoomMigration, Error, TEXT("ROOM_SMOKE FAIL: level load failed"));
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
        return;
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("MenuMigrationSmoke")))
    {
        ++SmokeFrames;
        const bool ExpectedMenu = MenuSmokeStage % 2 == 0;
        const int32 ExpectedEntries = MenuSmokeStage == 1 ? 162 : MenuSmokeStage == 3 ? 28 : MenuSmokeStage == 5 ? 25 : MenuSmokeStage == 7 ? 33 : 31;
        const bool Valid = Session->bLoadSucceeded && State->Menu == ExpectedMenu
            && (ExpectedMenu ? GetWorld()->GetMapName().Contains(TEXT("PinWorldBoot"))
                : Controller->bCameraReady && State->IsFoxy == (MenuSmokeStage == 3)
                    && State->IsHallOfPrayer == (MenuSmokeStage == 5)
                    && State->IsTaiheHall == (MenuSmokeStage == 7)
                    && State->IsTiananmen == (MenuSmokeStage == 9) && State->Entries.Num() == ExpectedEntries);
        if (!Valid)
        {
            UE_LOG(LogRoomMigration, Error, TEXT("MENU_SMOKE FAIL stage=%d map=%s"), MenuSmokeStage, *GetWorld()->GetMapName());
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        if (SmokeFrames == 180)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("MenuFlow%d.png"), MenuSmokeStage), true, false);
            UE_LOG(LogRoomMigration, Display, TEXT("MENU_SMOKE stage=%d map=%s menu=%d entries=%d"), MenuSmokeStage, *GetWorld()->GetMapName(), State->Menu, State->Entries.Num());
        }
        if (SmokeFrames == 200)
        {
            const int32 Stage = MenuSmokeStage++;
            if (Stage == 10)
            {
                UE_LOG(LogRoomMigration, Display, TEXT("MENU_SMOKE PASS Boot->Room->Boot->Foxy->Boot->HallOfPrayer->Boot->TaiheHall->Boot->Tiananmen->Boot publicPointerClicks=10"));
                FPlatformMisc::RequestExitWithStatus(false, 0);
            }
            else Controller->PointerDown(Stage == 0 ? FVector2D(0.5, 0.46) : Stage == 2 ? FVector2D(0.5, 0.62)
                : Stage == 4 ? FVector2D(0.5, 0.78) : Stage == 6 ? FVector2D(0.5, 0.93) : Stage == 8 ? FVector2D(0.5, 0.32) : FVector2D(0.9, 0.045));
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("RoomMigrationSmoke")) && !bSmoke)
    {
        ++SmokeFrames;
        const auto Shot = [this](const TCHAR* Stage)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("%s_%s.png"), State->IsTiananmen ? TEXT("Tiananmen") : State->IsTaiheHall ? TEXT("TaiheHall") : State->IsHallOfPrayer ? TEXT("HallOfPrayer") : State->IsFoxy ? TEXT("Foxy") : TEXT("Room"), Stage), true, false);
        };
        if (SmokeFrames == 300) Shot(TEXT("Initial"));
        if (SmokeFrames == 320) Controller->OrbitCamera(FVector2D(0.14, 0.02));
        if (SmokeFrames == 340) Shot(TEXT("Orbit"));
        if (SmokeFrames == 350) { Controller->ResetCamera(); Controller->PointerDown(AMigrationPlayerController::SlotRect(0).GetCenter()); Preview->MovePreview(FVector2D(0.55, 0.45)); }
        if (SmokeFrames >= 350 && SmokeFrames <= 379) Preview->MovePreview(FVector2D(0.55, 0.45));
        if (SmokeFrames == 370) Shot(TEXT("Drag"));
        if (SmokeFrames == 375 && Preview->ForegroundTarget)
        {
            TArray<FFloat16Color> Pixels;
            const bool Read = Preview->ForegroundTarget->GameThread_GetRenderTargetResource()->ReadFloat16Pixels(Pixels);
            int32 Covered = 0, Transparent = 0;
            for (const FFloat16Color& Pixel : Pixels)
            {
                if (Pixel.A.GetFloat() < 0.1f) ++Covered;
                if (Pixel.A.GetFloat() > 0.99f) ++Transparent;
            }
            const bool Valid = Read && Covered > 20 && Transparent > Pixels.Num() / 4;
            UE_LOG(LogRoomMigration, Display, TEXT("PREVIEW_PIXELS %s covered=%d transparent=%d"), Valid ? TEXT("PASS") : TEXT("FAIL"), Covered, Transparent);
            if (!Valid) { FPlatformMisc::RequestExitWithStatus(false, 1); return; }
        }
        if (SmokeFrames == 380) Session->CancelDrag();
        if (SmokeFrames == 400) Shot(TEXT("Cancel"));
        if (SmokeFrames == 410)
        {
            FVector2D Target;
            if (Controller->TargetScreen(0, Target)) { Controller->PointerDown(AMigrationPlayerController::SlotRect(0).GetCenter()); Preview->MovePreview(Target); Session->EndDrag(Target); }
        }
        if (SmokeFrames == 430) Shot(TEXT("Placed"));
        if (SmokeFrames == 440) { State->Entries[0].Placed = false; Scene->UpdateAppearance(); }
        if (State->IsArchitecture() && SmokeFrames == 442) { for (auto& Entry : State->Entries) Entry.Placed = true; Scene->UpdateAppearance(); }
        if (State->IsArchitecture() && SmokeFrames == 450) Shot(TEXT("Complete"));
        if (State->IsArchitecture() && SmokeFrames == 455) { for (auto& Entry : State->Entries) Entry.Placed = false; Scene->UpdateAppearance(); }
        if (State->IsHallOfPrayer && SmokeFrames == 456)
        {
            for (int32 E = 0; E < 6; ++E) State->Entries[E].Placed = true;
            State->Scroll = 0;
            Scene->UpdateAppearance();
        }
        if (State->IsHallOfPrayer && SmokeFrames == 470) Shot(TEXT("SixPlaced"));
        if (State->IsHallOfPrayer && SmokeFrames >= 480 && SmokeFrames <= 540)
        {
            const int32 E = 6 + (SmokeFrames - 480) / 20;
            const int32 Phase = (SmokeFrames - 480) % 20;
            if (E <= 8)
            {
                FVector2D Pixel;
                Controller->ProjectWorldLocationToScreen(State->Entries[E].Bounds.GetCenter(), Pixel);
                int32 W, H; Controller->GetViewportSize(W, H);
                const FVector2D Point = Pixel / FVector2D(W, H);
                if (Phase == 0) Controller->PointerDown(AMigrationPlayerController::SlotRect(0).GetCenter());
                if (Phase < 15) Preview->MovePreview(Point);
                if (Phase == 10) Shot(*FString::Printf(TEXT("VisualDrag%d"), E + 1));
                if (Phase == 15 && !Session->EndDrag(Point))
                {
                    UE_LOG(LogRoomMigration, Error, TEXT("HALL_VISUAL_SEQUENCE FAIL entry=%d"), E);
                    FPlatformMisc::RequestExitWithStatus(false, 1);
                }
            }
        }
        if (State->IsHallOfPrayer && SmokeFrames == 545) Shot(TEXT("NinePlaced"));
        if (State->IsHallOfPrayer && SmokeFrames == 550)
        {
            for (auto& Entry : State->Entries) Entry.Placed = false;
            Scene->UpdateAppearance();
        }
        if (SmokeFrames >= (State->IsHallOfPrayer ? 550 : 460)) RunInteractionSmoke();
    }
}

UWorld* UMigrationSmoke::GetWorld() const
{
    return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}
