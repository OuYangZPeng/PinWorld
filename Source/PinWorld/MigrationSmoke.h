#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MigrationTypes.h"
#include "MigrationSmoke.generated.h"

class AMigrationPlayerController;
class UMigrationDragPreview;
class UMigrationScene;
class UMigrationSession;
class UMigrationPlacementState;

UCLASS()
class PINWORLD_API UMigrationSmoke : public UObject
{
    GENERATED_BODY()
public:
    void RunInteractionSmoke();
    void Tick();
    virtual UWorld* GetWorld() const override;
    UPROPERTY() TObjectPtr<AMigrationPlayerController> Controller;
    UPROPERTY() TObjectPtr<UMigrationDragPreview> Preview;
    UPROPERTY() TObjectPtr<UMigrationScene> Scene;
    UPROPERTY() TObjectPtr<UMigrationSession> Session;
    UPROPERTY() TObjectPtr<UMigrationPlacementState> State;
    bool bSmoke = false;
    int32 SmokeFrames = 0;
};
