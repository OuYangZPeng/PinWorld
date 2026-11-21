#include "MigrationEnvironment.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

void FMigrationEnvironment::Create(UWorld* World, TArray<TWeakObjectPtr<AActor>>& Spawned)
{
    // Runtime daytime atmosphere, shared by both migration rooms.
    if (AActor* Sky = World->SpawnActor<AActor>())
    {
        auto* Atmosphere = NewObject<USkyAtmosphereComponent>(Sky);
        Sky->SetRootComponent(Atmosphere);
        Atmosphere->RegisterComponent();
        // Fill the lower hemisphere too: the room camera looks down into an empty world.
        auto* Haze = NewObject<UExponentialHeightFogComponent>(Sky);
        Haze->SetFogInscatteringColor(FLinearColor(0.32f, 0.62f, 0.9f));
        Haze->SetFogDensity(0.02f);
        Haze->SetFogHeightFalloff(0.001f);
        Haze->SetStartDistance(10000.f);
        Haze->RegisterComponent();
        Spawned.Add(Sky);
    }
    for (int32 I = 0; I < 2; ++I)
    {
        ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>();
        if (Light)
        {
            Light->SetActorRotation(I == 0 ? FRotator(-45, -35, 0) : FRotator(-25, 145, 0));
            CastChecked<UDirectionalLightComponent>(Light->GetLightComponent())->ForwardShadingPriority = I == 0 ? 1 : 0;
            CastChecked<UDirectionalLightComponent>(Light->GetLightComponent())->bAtmosphereSunLight = I == 0;
            Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            Light->GetLightComponent()->MarkRenderStateDirty();
            Light->GetLightComponent()->SetIntensity(I == 0 ? 3.0f : 1.0f);
            Spawned.Add(Light);
        }
    }
}

void FMigrationEnvironment::CreateViewFill(UWorld* World, FRotator View, bool Foxy, TArray<TWeakObjectPtr<AActor>>& Spawned)
{
            ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>();
            if (Fill)
            {
                Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);
                Fill->SetActorRotation(View);
                Fill->GetLightComponent()->SetCastShadows(false);
                Fill->GetLightComponent()->SetIntensity(Foxy ? 5.0f : 2.0f);
                CastChecked<UDirectionalLightComponent>(Fill->GetLightComponent())->ForwardShadingPriority = 2;
                Fill->GetLightComponent()->MarkRenderStateDirty();
                Spawned.Add(Fill);
            }
}
