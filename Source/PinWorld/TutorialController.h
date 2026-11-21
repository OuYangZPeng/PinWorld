#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TutorialController.generated.h"

class UTutorialState;
UCLASS()
class PINWORLD_API ATutorialController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void PlayerTick(float DeltaSeconds) override;
    void HandleClick(const FVector2D& P);
    void HandleRelease(const FVector2D& P);
    UPROPERTY() TObjectPtr<UTutorialState> State;
    bool bDragging = false;
private:
    bool bGlobePressed = false;
    bool bRotating = false;
    FVector2D PressPosition = FVector2D::ZeroVector;
    FVector2D LastPosition = FVector2D::ZeroVector;
};
