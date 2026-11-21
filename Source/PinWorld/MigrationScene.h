#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MigrationTypes.h"
#include "MigrationScene.generated.h"

class UMigrationPlacementState;

UCLASS()
class PINWORLD_API UMigrationScene : public UObject
{
    GENERATED_BODY()
public:
    bool LoadLevel(bool Foxy);
    void UpdateDoors(float DeltaSeconds);
    void UpdateAppearance();
    FBox DoorBounds(int32 Index) const;
    virtual UWorld* GetWorld() const override;
    UPROPERTY() TObjectPtr<UMigrationPlacementState> State;
    TMap<FString, TArray<UMaterialInterface*>> SavedSlots;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> WhiteMaterial;
    TMap<FString, class UProceduralMeshComponent*> Renderers;
    TMap<FString, FTransform> OriginalTransforms;
    TArray<TWeakObjectPtr<AActor>> Spawned;
    UPROPERTY() TMap<FString, TObjectPtr<class UMaterialInstanceDynamic>> OriginalMaterials;
    UPROPERTY() TArray<TObjectPtr<UObject>> Resources;
    FBox RoomBounds = FBox(ForceInit);
};
