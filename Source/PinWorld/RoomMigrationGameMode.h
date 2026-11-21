#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RoomMigrationGameMode.generated.h"

UCLASS()
class PINWORLD_API ARoomMigrationGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ARoomMigrationGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<class UMigrationSession> Session;
};
