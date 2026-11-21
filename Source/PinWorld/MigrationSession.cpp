#include "MigrationSession.h"
#include "MigrationController.h"
#include "MigrationPreview.h"
#include "MigrationScene.h"
#include "MigrationSmoke.h"
#include "MigrationState.h"
#include "MigrationHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomMigration, Log, All);

void UMigrationSession::ReturnToMenu()
{
    CancelDrag();
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldBoot"), true, TEXT("Menu=1"));
}

bool UMigrationSession::BeginDrag(int32 Entry)
{
    if (!State->CanBeginDrag(Entry)) return false;
    if (!Preview->Begin(Entry)) return false;
    State->Selected = Entry;
    State->Paused = true;
    Scene->UpdateAppearance();
    return true;
}

bool UMigrationSession::EndDrag(FVector2D Screen)
{
    if (!State->Entries.IsValidIndex(State->Selected)) return false;
    const int32 Dragged = State->Selected;
    FBox2D Rect(ForceInit);
    int32 TargetEntry = Dragged;
    bool bValidTarget = Controller->TargetRect(Dragged, Rect) && State->CanPlace(Dragged, Screen, Rect);
    // Opt-in only: existing manifests have no equivalence group, preserving rules.
    const FString& Group = State->Entries[Dragged].EquivalenceGroup;
    if (!bValidTarget && !Group.IsEmpty())
        for (int32 I = 0; I < State->Entries.Num(); ++I)
            if (I != Dragged && State->Entries[I].EquivalenceGroup == Group
                && Controller->TargetRect(I, Rect) && State->CanPlace(I, Screen, Rect))
            { TargetEntry = I; bValidTarget = true; break; }
    if (!bValidTarget)
    {
        UE_LOG(LogRoomMigration, Display, TEXT("ROOM_DROP result=FAIL entry=%d reason=outside-target"), Dragged);
        CancelDrag();
        return false;
    }
    CancelDrag();
    State->Entries[TargetEntry].Placed = true;
    State->Paused = false;
    State->Selected = INDEX_NONE;
    Scene->UpdateAppearance();
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_DROP result=PASS entry=%d name=%s"), Dragged, *State->Entries[Dragged].Name);
    return true;
}

void UMigrationSession::CancelDrag()
{
    Preview->Clear();
    State->Selected = INDEX_NONE;
    State->Paused = false;
    Scene->UpdateAppearance();
}

bool UMigrationSession::ToggleDoor(int32 Index)
{
    if (!State->ToggleDoor(Index)) return false;
    Scene->UpdateDoors(0);
    return true;
}

bool UMigrationSession::LoadLevel(bool Foxy)
{
    CancelDrag();
    if (auto* HUD = Cast<AMigrationHUD>(Controller->GetHUD())) HUD->ResetThumbnails();
    Controller->bOrbitDragging = false;
    State->IsHallOfPrayer = !Foxy && (GetWorld()->URL.GetOption(TEXT("Level="), TEXT("")) == FString(TEXT("HallOfPrayer"))
        || FParse::Param(FCommandLine::Get(), TEXT("HallOfPrayer")));
    State->IsTaiheHall = !Foxy && (GetWorld()->URL.GetOption(TEXT("Level="), TEXT("")) == FString(TEXT("TaiheHall"))
        || FParse::Param(FCommandLine::Get(), TEXT("TaiheHall")));
    State->IsTiananmen = !Foxy && (GetWorld()->URL.GetOption(TEXT("Level="), TEXT("")) == FString(TEXT("Tiananmen"))
        || FParse::Param(FCommandLine::Get(), TEXT("Tiananmen")));
    bLoadSucceeded = Scene->LoadLevel(Foxy);
    Controller->InitializeCamera(Foxy);
    return bLoadSucceeded;
}

void UMigrationSession::Tick(float DeltaSeconds)
{
    if (!State->Menu && !State->Help) Scene->UpdateDoors(DeltaSeconds);
    Smoke->Tick();
}

UWorld* UMigrationSession::GetWorld() const
{
    return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

void UMigrationSession::Initialize(AMigrationPlayerController* InController)
{
    Controller = InController;
    Controller->Session = this;
    State = NewObject<UMigrationPlacementState>(this);
    Scene = NewObject<UMigrationScene>(this);
    Smoke = NewObject<UMigrationSmoke>(this);
    Preview = NewObject<UMigrationDragPreview>(Controller);
    Preview->RegisterComponent();
    Scene->State = State;
    Smoke->Controller = Controller;
    Smoke->Preview = Preview;
    Smoke->Scene = Scene;
    Smoke->Session = this;
    Smoke->State = State;
    Preview->Scene = Scene;
    Preview->State = State;
    Controller->Preview = Preview;
    Controller->Scene = Scene;
    Controller->Session = this;
    Controller->State = State;
    State->Status = TEXT("Menu");
    bLoadSucceeded = true;
}
