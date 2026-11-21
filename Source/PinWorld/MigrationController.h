#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MigrationTypes.h"
#include "MigrationController.generated.h"

class UMigrationDragPreview;
class UMigrationScene;
class UMigrationSession;
class UMigrationPlacementState;

UCLASS()
class PINWORLD_API AMigrationPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    bool TargetRect(int32 Entry, FBox2D& Rect) const;
    bool ProjectBounds(const FBox& B, FBox2D& Rect) const;
    bool TargetScreen(int32 Entry, FVector2D& Screen) const;
    void PointerDown(FVector2D Screen);
    void ApplyCamera();
    void ResetCamera();
    void OrbitCamera(FVector2D Delta);
    void PointerWheel(FVector2D Screen, int32 Steps);
    static FBox2D SlotRect(int32 Slot);
    void InitializeCamera(bool Foxy);
    virtual void PlayerTick(float DeltaSeconds) override;
    UPROPERTY() TObjectPtr<UMigrationDragPreview> Preview;
    UPROPERTY() TObjectPtr<UMigrationScene> Scene;
    UPROPERTY() TObjectPtr<UMigrationSession> Session;
    UPROPERTY() TObjectPtr<UMigrationPlacementState> State;
    TWeakObjectPtr<class ACameraActor> OrbitView;
    FVector OrbitCenter = FVector::ZeroVector;
    FRotator OrbitRotation, DefaultOrbitRotation;
    float OrbitDistance = 1000, DefaultOrbitDistance = 1000;
    FVector2D LastPointer = FVector2D::ZeroVector;
    bool bOrbitDragging = false;
    bool bCameraReady = false;
};
