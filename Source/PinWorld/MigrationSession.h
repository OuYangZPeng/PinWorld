#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MigrationTypes.h"
#include "MigrationSession.generated.h"

class AMigrationPlayerController;
class UMigrationDragPreview;
class UMigrationScene;
class UMigrationSmoke;
class UMigrationPlacementState;

UCLASS()
class PINWORLD_API UMigrationSession : public UObject
{
    GENERATED_BODY()
public:
    void ReturnToMenu();
    bool BeginDrag(int32 Entry);
    bool EndDrag(FVector2D Screen);
    void CancelDrag();
    bool ToggleDoor(int32 Index);
    bool LoadLevel(bool Foxy);
    void Tick(float DeltaSeconds);
    virtual UWorld* GetWorld() const override;
    void Initialize(AMigrationPlayerController* InController);
    UPROPERTY() TObjectPtr<AMigrationPlayerController> Controller;
    UPROPERTY() TObjectPtr<UMigrationDragPreview> Preview;
    UPROPERTY() TObjectPtr<UMigrationScene> Scene;
    UPROPERTY() TObjectPtr<UMigrationSmoke> Smoke;
    UPROPERTY() TObjectPtr<UMigrationPlacementState> State;
    bool bLoadSucceeded = false;
};
