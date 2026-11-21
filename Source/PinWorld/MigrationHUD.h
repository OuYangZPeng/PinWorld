#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MigrationTypes.h"
#include "MigrationHUD.generated.h"

class AMigrationPlayerController;
class UMigrationScene;
class UMigrationPlacementState;

UCLASS()
class PINWORLD_API AMigrationHUD : public AHUD
{
    GENERATED_BODY()
public:
    void UpdateThumbnails();
    void DrawPanel(AHUD* HUD);
    void ResetThumbnails();
    virtual void DrawHUD() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UPROPERTY() TObjectPtr<AMigrationPlayerController> Controller;
    UPROPERTY() TObjectPtr<UMigrationScene> Scene;
    UPROPERTY() TObjectPtr<UMigrationPlacementState> State;
    UPROPERTY() TArray<TObjectPtr<class UTextureRenderTarget2D>> ThumbnailTargets;
    TArray<TWeakObjectPtr<AActor>> ThumbnailActors;
    TArray<int32> ThumbnailEntries;
};
