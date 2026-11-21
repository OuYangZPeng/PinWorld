#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MigrationTypes.h"
#include "MigrationState.generated.h"



UCLASS()
class PINWORLD_API UMigrationPlacementState : public UObject
{
    GENERATED_BODY()
public:
    TArray<int32> VisibleEntries() const;
    int32 PlacedCount() const;
    int32 ClosedDoor(int32 Entry) const;
    bool ToggleDoor(int32 Index);
    bool CanBeginDrag(int32 Entry) const;
    bool CanPlace(int32 Entry, FVector2D Screen, const FBox2D& Target) const;
    bool RendererPlaced(const FString& Id) const;
    virtual UWorld* GetWorld() const override;
    TArray<FMigrationEntry> Entries;
    TArray<FMigrationDoor> Doors;
    bool Menu = true, Paused = false, Help = false, IsFoxy = false;
    bool IsHallOfPrayer = false;
    bool IsTaiheHall = false;
    bool IsTiananmen = false;
    bool IsArchitecture() const { return IsHallOfPrayer || IsTaiheHall || IsTiananmen; }
    int32 Selected = INDEX_NONE, Scroll = 0;
    FString Status;
    TMap<FString, int32> EntryByMember;
    TMap<FString, int32> EntryById;
};
