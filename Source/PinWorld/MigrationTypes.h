#pragma once

#include "CoreMinimal.h"

struct FMigrationEntry
{
    FString Id, Name;
    FString EquivalenceGroup;
    TArray<FString> Members, Renderers;
    FBox Bounds = FBox(ForceInit);
    bool Placed = false;
};
struct FMigrationDoorFrame
{
    float Time = 0;
    TMap<FString, FTransform> Renderers;
};
struct FMigrationDoorMotion
{
    TArray<FMigrationDoorFrame> Frames;
};
struct FMigrationDoor
{
    FString Owner;
    TArray<FString> Inside, Renderers;
    TMap<FString, FMigrationDoorMotion> Motions;
    FString Motion = TEXT("closed");
    float Time = 0;
    bool EverOpened = false;
    bool Open = false, CanClose = true;
};
