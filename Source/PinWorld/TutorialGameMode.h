#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TutorialGameMode.generated.h"

UCLASS()
class PINWORLD_API ATutorialGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ATutorialGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<class UTutorialState> State;
    UPROPERTY() TObjectPtr<class ATutorialScene> Scene;
};
