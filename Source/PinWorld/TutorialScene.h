#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialScene.generated.h"

UCLASS()
class PINWORLD_API ATutorialScene : public AActor
{
    GENERATED_BODY()
public:
    ATutorialScene();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void SetState(class UTutorialState* InState);
    bool TryDrop(int32 Continent, const FVector2D& ScreenPosition, const FVector2D& ViewportSize);
    void FocusContinent(int32 Index);
    void RotateBy(const FVector2D& ScreenDelta);
    bool TargetScreen(int32 Index, FVector2D& Screen) const;
    int32 PickContinent(FVector2D Screen) const;
    // Screen-space line pairs of the continent outline as it appears on the globe right now (front side only).
    void TargetSegments(int32 Index, TArray<FVector2D>& Out) const;
    // Screen-space line pairs, relative to the continent center, as the continent looks when focused on the globe.
    void GhostSegments(int32 Index, TArray<FVector2D>& Out) const;
    static const TArray<FVector2D>& Outline(int32 Index);
    static FVector2D CenterLL(int32 Index);
private:
    UPROPERTY() TObjectPtr<class UTutorialState> State;
    UPROPERTY() TObjectPtr<class UStaticMeshComponent> Earth;
    UPROPERTY() TObjectPtr<class UMaterialInterface> EarthMaterial;
    UPROPERTY() TObjectPtr<class UStaticMesh> CloudMesh;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Clouds;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Continents;
    UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    TArray<FVector> Centers;
    FQuat TargetRotation = FQuat::Identity;
    float ViewLon = 0;
    float ViewLat = 0;
    float CloudTime = 0;
    void BuildScene();
    float GlobeScreenRadius() const;
};
