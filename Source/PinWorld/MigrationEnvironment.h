#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
struct FMigrationEnvironment
{
    static void Create(UWorld* World, TArray<TWeakObjectPtr<AActor>>& Spawned);
    static void CreateViewFill(UWorld* World, FRotator View, bool Foxy, TArray<TWeakObjectPtr<AActor>>& Spawned);
};
