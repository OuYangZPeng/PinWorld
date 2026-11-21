#include "MigrationController.h"
#include "MigrationSession.h"
#include "MigrationState.h"
#include "MigrationScene.h"
#include "MigrationPreview.h"
#include "MigrationEnvironment.h"
#include "ProceduralMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"

namespace { FVector UnityToUE(const FVector& V) { return FVector(V.Z, V.X, V.Y); } }
bool AMigrationPlayerController::TargetRect(int32 Entry, FBox2D& Rect) const
{
    if (!State->Entries.IsValidIndex(Entry)) return false;
    FBox Bounds(ForceInit);
    for (const FString& Id : State->Entries[Entry].Renderers)
        if (const auto* R = Scene->Renderers.FindRef(Id))
        {
            FTransform Pose = FTransform::Identity;
            for (const auto& Door : State->Doors)
                if (const auto* Motion = Door.Motions.Find(Door.Motion))
                {
                    int32 Frame = 0;
                    while (Frame + 1 < Motion->Frames.Num() && Motion->Frames[Frame].Time < Door.Time) ++Frame;
                    if (Motion->Frames.IsValidIndex(Frame))
                        if (const auto* Delta = Motion->Frames[Frame].Renderers.Find(Id)) Pose = *Delta;
                }
            Bounds += R->CalcBounds(Pose).GetBox();
        }
    return ProjectBounds(Bounds.IsValid ? Bounds : State->Entries[Entry].Bounds, Rect);
}

bool AMigrationPlayerController::ProjectBounds(const FBox& B, FBox2D& Rect) const
{
    Rect = FBox2D(ForceInit);
    if (!B.IsValid) return false;
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return false;
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return false;
    for (int32 I = 0; I < 8; ++I)
    {
        FVector2D Pixel;
        const FVector Corner((I & 1) ? B.Max.X : B.Min.X, (I & 2) ? B.Max.Y : B.Min.Y, (I & 4) ? B.Max.Z : B.Min.Z);
        // Reject near-plane crossing bounds rather than inventing a target.
        if (!PC->ProjectWorldLocationToScreen(Corner, Pixel)) return false;
        Rect += FVector2D(Pixel.X / Width, Pixel.Y / Height);
    }
    Rect.Min.X = FMath::Max(0.0, Rect.Min.X); Rect.Min.Y = FMath::Max(0.0, Rect.Min.Y);
    Rect.Max.X = FMath::Min(1.0, Rect.Max.X); Rect.Max.Y = FMath::Min(1.0, Rect.Max.Y);
    return Rect.Max.X > Rect.Min.X && Rect.Max.Y > Rect.Min.Y;
}

bool AMigrationPlayerController::TargetScreen(int32 Entry, FVector2D& Screen) const
{
    FBox2D Rect(ForceInit);
    if (!TargetRect(Entry, Rect)) return false;
    Rect.Max.X = FMath::Min(Rect.Max.X, 0.80);
    Rect.Min.Y = FMath::Max(Rect.Min.Y, 0.10);
    if (Rect.Max.X <= Rect.Min.X || Rect.Max.Y <= Rect.Min.Y) return false;
    Screen = Rect.GetCenter();
    return true;
}

void AMigrationPlayerController::PointerDown(FVector2D Screen)
{
    if (State->Menu)
    {
        if (Screen.X >= 0.3 && Screen.X <= 0.7)
        {
            if (Screen.Y >= 0.27 && Screen.Y <= 0.37) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Tiananmen"));
            else if (Screen.Y >= 0.4 && Screen.Y <= 0.52) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Room"));
            else if (Screen.Y >= 0.56 && Screen.Y <= 0.68) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Foxy"));
            else if (Screen.Y >= 0.72 && Screen.Y <= 0.84) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=HallOfPrayer"));
            else if (Screen.Y >= 0.88 && Screen.Y <= 0.98) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=TaiheHall"));
        }
        return;
    }
    if (!State->Menu && Screen.Y <= 0.09 && Screen.X >= 0.81) { Session->ReturnToMenu(); return; }
    if (State->Menu || State->Help || State->Selected != INDEX_NONE || Screen.Y <= 0.09) return;
    const TArray<int32> Queue = State->VisibleEntries();
    State->Scroll = FMath::Clamp(State->Scroll, 0, FMath::Max(0, Queue.Num() - 3));
    if (Screen.X >= 0.81)
    {
        for (int32 Slot = 0; Slot < 3; ++Slot)
            if (Queue.IsValidIndex(State->Scroll + Slot) && SlotRect(Slot).IsInside(Screen))
            { Session->BeginDrag(Queue[State->Scroll + Slot]); Preview->MovePreview(Screen); return; }
        return;
    }
    int32 Best = INDEX_NONE;
    double Area = DBL_MAX;
    for (int32 I = 0; I < State->Doors.Num(); ++I)
    {
        FBox2D Rect(ForceInit);
        if (ProjectBounds(Scene->DoorBounds(I), Rect) && Rect.IsInside(Screen) && Rect.GetArea() < Area)
        { Best = I; Area = Rect.GetArea(); }
    }
    if (Best != INDEX_NONE) Session->ToggleDoor(Best);
}

void AMigrationPlayerController::ApplyCamera()
{
    if (!OrbitView.IsValid()) return;
    OrbitView->SetActorRotation(OrbitRotation);
    OrbitView->SetActorLocation(OrbitCenter - OrbitRotation.Vector() * OrbitDistance);
    if (auto* PC = GetWorld()->GetFirstPlayerController()) PC->PlayerCameraManager->UpdateCamera(0);
}

void AMigrationPlayerController::ResetCamera()
{
    OrbitRotation = DefaultOrbitRotation;
    OrbitDistance = DefaultOrbitDistance;
    ApplyCamera();
}

void AMigrationPlayerController::OrbitCamera(FVector2D Delta)
{
    if (State->Menu || State->Help || State->Selected != INDEX_NONE) return;
    OrbitRotation.Yaw += Delta.X * 240;
    OrbitRotation.Pitch = FMath::Clamp(OrbitRotation.Pitch + float(Delta.Y * 160), -80.f, -5.f);
    ApplyCamera();
}

void AMigrationPlayerController::PointerWheel(FVector2D Screen, int32 Steps)
{
    if (State->Menu || State->Help || State->Selected != INDEX_NONE) return;
    if (Screen.X >= 0.81)
        State->Scroll = FMath::Clamp(State->Scroll + Steps, 0, FMath::Max(0, State->VisibleEntries().Num() - 3));
    else
    {
        OrbitDistance = FMath::Clamp(OrbitDistance * FMath::Pow(1.12f, float(Steps)), DefaultOrbitDistance * 0.35f, DefaultOrbitDistance * 2.f);
        ApplyCamera();
    }
}

FBox2D AMigrationPlayerController::SlotRect(int32 Slot)
{
    return FBox2D(FVector2D(0.83, 0.16 + Slot * 0.23), FVector2D(0.98, 0.37 + Slot * 0.23));
}

void AMigrationPlayerController::InitializeCamera(bool Foxy)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    bCameraReady = false;
    if (PC && Scene->RoomBounds.IsValid)
    {
        ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>();
        if (Camera)
        {
            // Serialized LevelFocusCamera world poses (Unity metres, ZXY conversion).
            const FVector Center = State->IsArchitecture() ? FVector(0, 0, 1400) : UnityToUE(Foxy ? FVector(-2.6664658, 2.224944, -0.24036479) : FVector(0.097156644, 2.58, 1.8069513)) * 100;
            Camera->SetActorLocation(State->IsArchitecture() ? FVector(8500, -13500, 8500) : UnityToUE(Foxy ? FVector(-6.632327, 3.8910794, 10.981376) : FVector(-11.749631, 10.389368, 24.211273)) * 100);
            Camera->SetActorRotation((Center - Camera->GetActorLocation()).Rotation());
            Camera->GetCameraComponent()->SetFieldOfView(60.0f);
            // Input, HUD and foreground capture use full viewport pixels. A constrained
            // camera adds a Canvas origin (32px at 900x470), shifting the HUD/preview
            // twice while hit testing remains in viewport space. Fill the viewport.
            Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
            // Soft view-side fill substitutes the source ambient environment in
            // these empty migration worlds; keep textured DefaultLit shading.
            FMigrationEnvironment::CreateViewFill(GetWorld(), Camera->GetActorRotation(), Foxy, Scene->Spawned);
            PC->SetViewTarget(Camera);
            OrbitView = Camera;
            OrbitCenter = Center;
            DefaultOrbitRotation = Camera->GetActorRotation();
            DefaultOrbitDistance = FVector::Distance(Center, Camera->GetActorLocation());
            ResetCamera();
            Scene->Spawned.Add(Camera);
            bCameraReady = true;
        }
    }
}

void AMigrationPlayerController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);
    if (!Session || !Session->bLoadSucceeded) return;
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (PC)
    {
        PC->bShowMouseCursor = true;
        if (PC->WasInputKeyJustPressed(EKeys::One)) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Room"));
        if (PC->WasInputKeyJustPressed(EKeys::Two)) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Foxy"));
        if (PC->WasInputKeyJustPressed(EKeys::Three)) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=HallOfPrayer"));
        if (PC->WasInputKeyJustPressed(EKeys::Five)) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Tiananmen"));
        if (PC->WasInputKeyJustPressed(EKeys::Four)) UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=TaiheHall"));
        if (State->Menu && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
        {
            float X, Y; int32 W, H;
            PC->GetViewportSize(W, H);
            if (W > 0 && H > 0 && PC->GetMousePosition(X, Y)) PointerDown(FVector2D(X / W, Y / H));
        }
        if (PC->WasInputKeyJustPressed(EKeys::Escape))
        {
            if (State->Selected != INDEX_NONE) Session->CancelDrag();
            else if (!State->Menu) State->Help = !State->Help;
        }
        if (!State->Menu)
        {
            FVector2D Mouse;
            if (PC->GetMousePosition(Mouse.X, Mouse.Y))
            {
                int32 Width = 1, Height = 1;
                PC->GetViewportSize(Width, Height);
                const FVector2D Normalized(Mouse.X / Width, Mouse.Y / Height);
                if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton)) PointerDown(Normalized);
                if (State->Selected != INDEX_NONE && !FParse::Param(FCommandLine::Get(), TEXT("RoomMigrationSmoke"))) Preview->MovePreview(Normalized);
                if (PC->WasInputKeyJustReleased(EKeys::LeftMouseButton) && State->Selected != INDEX_NONE) Session->EndDrag(Normalized);
                if (!State->Help && State->Selected == INDEX_NONE)
                {
                    if (PC->WasInputKeyJustPressed(EKeys::R)) ResetCamera();
                    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) PointerWheel(Normalized, 1);
                    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) PointerWheel(Normalized, -1);
                    if (PC->IsInputKeyDown(EKeys::RightMouseButton))
                    {
                        if (bOrbitDragging) OrbitCamera(Normalized - LastPointer);
                        bOrbitDragging = true;
                    }
                    else bOrbitDragging = false;
                }
                else bOrbitDragging = false;
                LastPointer = Normalized;
            }
        }
    }
}
