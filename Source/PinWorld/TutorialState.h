#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TutorialState.generated.h"

UCLASS()
class PINWORLD_API UTutorialState : public UObject
{
    GENERATED_BODY()
public:
    bool Splash = true;
    bool Completed = false;
    int32 Selected = INDEX_NONE;
    TArray<bool> Placed;
    int32 SelectedContinent = INDEX_NONE;
    float SplashTime = 0.0f;
    float TransitionTime = 0.0f;
    bool bTransitioning = false;
    bool AllPlaced() const { return Placed.Num() == 7 && !Placed.Contains(false); }
};
