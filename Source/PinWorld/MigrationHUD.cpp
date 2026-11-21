#include "MigrationHUD.h"
#include "MigrationController.h"
#include "MigrationSession.h"
#include "MigrationState.h"
#include "MigrationScene.h"
#include "MigrationPreview.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/CommandLine.h"

void AMigrationHUD::UpdateThumbnails()
{
    const auto Queue = State->VisibleEntries();
    TArray<int32> Wanted;
    for (int32 S = 0; S < 3; ++S) Wanted.Add(Queue.IsValidIndex(State->Scroll + S) ? Queue[State->Scroll + S] : INDEX_NONE);
    if (Wanted == ThumbnailEntries) return;
    for (auto Actor : ThumbnailActors) if (Actor.IsValid()) Actor->Destroy();
    ThumbnailActors.Reset();
    ThumbnailEntries = Wanted;
    while (ThumbnailTargets.Num() < 3)
    {
        auto* Target = NewObject<UTextureRenderTarget2D>(this);
        Target->ClearColor = FLinearColor(0.15f, 0.23f, 0.28f, 1);
        Target->InitAutoFormat(256, 256);
        ThumbnailTargets.Add(Target);
    }
    for (int32 S = 0; S < 3; ++S)
    {
        if (!State->Entries.IsValidIndex(Wanted[S])) continue;
        const auto& Entry = State->Entries[Wanted[S]];
        if (!Entry.Bounds.IsValid) continue;
        auto* Actor = GetWorld()->SpawnActor<AActor>();
        ThumbnailActors.Add(Actor);
        auto* Capture = NewObject<USceneCaptureComponent2D>(Actor);
        Actor->SetRootComponent(Capture);
        Capture->RegisterComponent();
        Capture->TextureTarget = ThumbnailTargets[S];
        Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        // Match the working foreground capture: scene color avoids the
        // final-color/manual-exposure path producing black thumbnail targets.
        Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
        // HUD creates these components after the world's render update. Keep
        // capturing so the registered meshes/lights are included on later frames.
        Capture->bCaptureEveryFrame = true;
        Capture->bCaptureOnMovement = false;
        Capture->FOVAngle = 35;
        Capture->ShowFlags.SetAtmosphere(false);
        Capture->ShowFlags.SetFog(false);
        Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
        Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
        const FVector Center(0, 0, -100000 - S * 10000);
        const FRotator View(-25, 145, 0);
        const double Radius = FMath::Max(1.0, Entry.Bounds.GetExtent().Size());
        Capture->SetWorldLocationAndRotation(Center - View.Vector() * (Radius / FMath::Sin(FMath::DegreesToRadians(17.5)) * 1.15), View);
        if (State->IsArchitecture())
        {
            auto* Fill = NewObject<UPointLightComponent>(Actor);
            Fill->RegisterComponent();
            Fill->SetUseInverseSquaredFalloff(false);
            Fill->SetIntensity(8.0f);
            Fill->SetAttenuationRadius(Radius * 12.0);
            Fill->SetCastShadows(false);
            Fill->SetWorldLocation(Center - View.Vector() * Radius * 2.5);
        }
        for (const auto& Id : Entry.Renderers)
        {
            auto* Source = Scene->Renderers.FindRef(Id);
            const auto* Slots = Scene->SavedSlots.Find(Id);
            if (!Source || !Slots) continue;
            auto* Copy = NewObject<UProceduralMeshComponent>(Actor);
            Copy->RegisterComponent();
            Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Copy->SetVisibleInSceneCaptureOnly(true);
            Copy->SetCastShadow(false);
            for (int32 Section = 0; Section < Source->GetNumSections(); ++Section)
                if (auto* Mesh = Source->GetProcMeshSection(Section)) Copy->SetProcMeshSection(Section, *Mesh);
            for (int32 M = 0; M < Slots->Num(); ++M) Copy->SetMaterial(M, (*Slots)[M]);
            Copy->SetWorldLocation(Center - Entry.Bounds.GetCenter());
            Capture->ShowOnlyComponent(Copy);
        }
        Capture->CaptureScene();
    }
}

void AMigrationHUD::DrawPanel(AHUD* HUD)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    int32 W = 0, H = 0; PC->GetViewportSize(W, H);
    HUD->DrawRect(FLinearColor(0.025f, 0.035f, 0.05f, 0.95f), 0, 0, W, H * 0.09f);
    HUD->DrawText(State->Help ? TEXT("Paused - Esc to resume") : FString::Printf(TEXT("PinWorld | %d / %d placed"), State->PlacedCount(), State->Entries.Num()), FColor::White, W * 0.02f, H * 0.025f);
    if (!State->Menu) HUD->DrawText(TEXT("Back to menu"), FColor::White, W * 0.83f, H * 0.025f);
    if (State->Menu)
    {
        HUD->DrawRect(FLinearColor(0.02f, 0.03f, 0.04f, 1), 0, 0, W, H);
        HUD->DrawText(TEXT("PINWORLD"), FColor::White, W * 0.3f, H * 0.17f, nullptr, 2.0f);
        HUD->DrawRect(FLinearColor(0.4f, 0.08f, 0.04f, 1), W * 0.3f, H * 0.27f, W * 0.4f, H * 0.10f);
        HUD->DrawText(TEXT("Tiananmen [5]"), FColor::White, W * 0.34f, H * 0.30f);
        HUD->DrawRect(FLinearColor(0.15f, 0.3f, 0.35f, 1), W * 0.3f, H * 0.4f, W * 0.4f, H * 0.12f);
        HUD->DrawRect(FLinearColor(0.3f, 0.2f, 0.15f, 1), W * 0.3f, H * 0.56f, W * 0.4f, H * 0.12f);
        HUD->DrawText(TEXT("Room"), FColor::White, W * 0.34f, H * 0.44f);
        HUD->DrawText(TEXT("Foxy"), FColor::White, W * 0.34f, H * 0.60f);
        HUD->DrawRect(FLinearColor(0.12f, 0.22f, 0.36f, 1), W * 0.3f, H * 0.72f, W * 0.4f, H * 0.12f);
        HUD->DrawText(TEXT("Hall of Prayer"), FColor::White, W * 0.34f, H * 0.76f);
        HUD->DrawRect(FLinearColor(0.36f, 0.18f, 0.06f, 1), W * 0.3f, H * 0.88f, W * 0.4f, H * 0.10f);
        HUD->DrawText(TEXT("Taihe Hall [4]"), FColor::White, W * 0.34f, H * 0.91f);
        return;
    }
    HUD->DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, 0.95f), W * 0.81f, H * 0.09f, W * 0.19f, H);
    const auto DrawFit = [HUD](const FString& Text, FLinearColor Color, float X, float Y, float Width, float MaxScale)
    {
        float TextWidth = 0, TextHeight = 0;
        HUD->GetTextSize(Text, TextWidth, TextHeight, GEngine->GetSmallFont(), 1.0f);
        HUD->DrawText(Text, Color, X, Y, GEngine->GetSmallFont(), FMath::Min(MaxScale, Width / FMath::Max(1.0f, TextWidth)));
    };
    UpdateThumbnails();
    DrawFit(TEXT("RMB: orbit | Wheel: zoom | R: reset"), FColor::White, W * 0.3f, H * 0.025f, W * 0.49f, 1.0f);
    DrawFit(TEXT("Drag slots / Wheel: scroll"), FColor::White, W * 0.82f, H * 0.11f, W * 0.17f, 1.0f);
    const TArray<int32> Queue = State->VisibleEntries();
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        const FBox2D R = AMigrationPlayerController::SlotRect(Slot);
        HUD->DrawRect(FLinearColor(0.15f, 0.23f, 0.28f, 1), R.Min.X * W, R.Min.Y * H, R.GetSize().X * W, R.GetSize().Y * H);
        if (Queue.IsValidIndex(State->Scroll + Slot))
        {
            const int32 E = Queue[State->Scroll + Slot];
            if (ThumbnailTargets.IsValidIndex(Slot))
            {
                const float Size = FMath::Min(float(R.GetSize().X * W - 16), float(R.GetSize().Y * H - 42));
                HUD->DrawTexture(ThumbnailTargets[Slot], R.GetCenter().X * W - Size * 0.5f, R.Min.Y * H + 30, Size, Size, 0, 0, 1, 1, FLinearColor::White, BLEND_Opaque);
            }
            DrawFit(FString::Printf(TEXT("%d. %s"), E + 1, *State->Entries[E].Name), State->Selected == E ? FColor::Yellow : FColor::White, R.Min.X * W + 8, R.Min.Y * H + 12, R.GetSize().X * W - 16, 1.0f);
        }
    }
    for (int32 D = 0; FParse::Param(FCommandLine::Get(), TEXT("MigrationDebug")) && D < State->Doors.Num(); ++D)
    {
        FBox2D R(ForceInit);
        if (Controller->ProjectBounds(Scene->DoorBounds(D), R)) HUD->DrawText(FString::Printf(TEXT("D%d %s [source motion]"), D + 1, State->Doors[D].Open ? TEXT("OPEN") : TEXT("CLOSED")), FColor::Cyan, R.GetCenter().X * W, R.GetCenter().Y * H);
    }
}

void AMigrationHUD::ResetThumbnails()
{
    for (auto Actor : ThumbnailActors) if (Actor.IsValid()) Actor->Destroy();
    ThumbnailActors.Reset();
    ThumbnailTargets.Reset();
    ThumbnailEntries.Reset();
}

void AMigrationHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* PC = Cast<AMigrationPlayerController>(GetOwningPlayerController());
    if (!PC || !PC->Session) return;
    State = PC->Session->State;
    Scene = PC->Session->Scene;
    Controller = PC;
    DrawPanel(this);
    PC->Session->Preview->DrawForeground(this);
}

void AMigrationHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetThumbnails();
    Super::EndPlay(EndPlayReason);
}
