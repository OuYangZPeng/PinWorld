#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TutorialHUD.generated.h"

class UTutorialState;
UCLASS()
class PINWORLD_API ATutorialHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    UPROPERTY() TObjectPtr<UTutorialState> State;
    UPROPERTY() TObjectPtr<class UTexture2D> SplashTexture;
};
