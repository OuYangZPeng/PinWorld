#include "MigrationState.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoomMigration, Log, All);

TArray<int32> UMigrationPlacementState::VisibleEntries() const
{
    TArray<int32> Result;
    for (int32 I = 0; I < Entries.Num(); ++I)
    {
        if (Entries[I].Placed) continue;
        if (IsFoxy && Result.Num() > 0 && Entries[0].Placed == false) break;
        Result.Add(I);
    }
    return Result;
}

int32 UMigrationPlacementState::PlacedCount() const
{
    int32 Count = 0;
    for (const FMigrationEntry& E : Entries) if (E.Placed) ++Count;
    return Count;
}

int32 UMigrationPlacementState::ClosedDoor(int32 Entry) const
{
    if (!Entries.IsValidIndex(Entry)) return INDEX_NONE;
    for (int32 D = 0; D < Doors.Num(); ++D)
        if (!Doors[D].Open)
            for (const FString& Member : Entries[Entry].Members)
                if (Doors[D].Inside.Contains(Member)) return D;
    return INDEX_NONE;
}

bool UMigrationPlacementState::RendererPlaced(const FString& Id) const
{
    bool Owned = false;
    for (const auto& Entry : Entries)
        if (Entry.Renderers.Contains(Id))
        {
            Owned = true;
            if (!Entry.Placed) return false;
        }
    return Owned;
}

UWorld* UMigrationPlacementState::GetWorld() const
{
    return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

bool UMigrationPlacementState::ToggleDoor(int32 Index)
{
    if (Menu || Selected != INDEX_NONE || !Doors.IsValidIndex(Index)) return false;
    const int32* OwnerEntry = EntryById.Find(Doors[Index].Owner);
    if (!OwnerEntry) OwnerEntry = EntryByMember.Find(Doors[Index].Owner);
    if (OwnerEntry && !Entries[*OwnerEntry].Placed) { Status = TEXT("Place furniture before opening"); return false; }
    if (Doors[Index].Open && !Doors[Index].CanClose) return false;
    // Source transitions are non-interruptible. Do not snap to another sampled start pose.
    if (Doors[Index].Motion != TEXT("closed"))
        if (const auto* Running = Doors[Index].Motions.Find(Doors[Index].Motion))
            if (!Running->Frames.IsEmpty() && Doors[Index].Time < Running->Frames.Last().Time) return false;
    Doors[Index].Open = !Doors[Index].Open;
    UE_LOG(LogRoomMigration, Display, TEXT("ROOM_DOOR index=%d open=%d"), Index, Doors[Index].Open ? 1 : 0);
    auto& Door = Doors[Index];
    Door.Motion = Door.Open ? (Door.EverOpened ? TEXT("open") : TEXT("firstOpen")) : TEXT("close");
    Door.EverOpened |= Door.Open;
    Door.Time = 0;
    Status = FString::Printf(TEXT("Door %d %s (source controller motion)"), Index + 1, Door.Open ? TEXT("OPEN") : TEXT("CLOSED"));
    return true;
}

bool UMigrationPlacementState::CanBeginDrag(int32 Entry) const
{
    return !Menu && Selected == INDEX_NONE && Entries.IsValidIndex(Entry) && !Entries[Entry].Placed
        && (!IsFoxy || Entries[0].Placed || Entry == 0);
}

bool UMigrationPlacementState::CanPlace(int32 Entry, FVector2D Screen, const FBox2D& Target) const
{
    return !Menu && Entries.IsValidIndex(Entry) && !Entries[Entry].Placed
        && Screen.X < 0.81 && Screen.Y > 0.09 && ClosedDoor(Entry) == INDEX_NONE && Target.IsInside(Screen);
}
