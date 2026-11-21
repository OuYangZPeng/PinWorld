#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MigrationTypes.h"
#include "MigrationPreview.generated.h"

class UMigrationScene;
class UMigrationPlacementState;

UCLASS()
class PINWORLD_API UMigrationDragPreview : public UActorComponent
{
    GENERATED_BODY()
public:
    bool Begin(int32 Entry);
    void Clear();
    void MovePreview(FVector2D Screen);
    void DrawForeground(class AHUD* HUD);
    UPROPERTY() TObjectPtr<class USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> ForegroundTarget;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> ForegroundMaterial;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY() TObjectPtr<UMigrationScene> Scene;
    UPROPERTY() TObjectPtr<UMigrationPlacementState> State;
    TWeakObjectPtr<AActor> DragPreviewActor;
    TArray<TWeakObjectPtr<class UProceduralMeshComponent>> DragPreview;
};
