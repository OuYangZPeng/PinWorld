#include "TutorialGameMode.h"
#include "TutorialController.h"
#include "TutorialHUD.h"
#include "TutorialState.h"
#include "TutorialScene.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ATutorialGameMode::ATutorialGameMode()
{
    PlayerControllerClass = ATutorialController::StaticClass();
    HUDClass = ATutorialHUD::StaticClass();
    DefaultPawnClass = nullptr;
    PrimaryActorTick.bCanEverTick = true;
}
void ATutorialGameMode::StartPlay()
{
    Super::StartPlay();
    State = NewObject<UTutorialState>(this);
    State->Placed.Init(false, 7);
    Scene = GetWorld()->SpawnActor<ATutorialScene>(ATutorialScene::StaticClass(), FVector(0, 0, 0), FRotator::ZeroRotator);
    if (Scene) Scene->SetState(State);
    if (auto* PC = Cast<ATutorialController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->State = State;
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
        if (auto* HUD = Cast<ATutorialHUD>(PC->GetHUD())) HUD->State = State;
    }
}
void ATutorialGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!State) return;
    State->SplashTime += DeltaSeconds;
    if (State->Splash && State->SplashTime > 3.0f) State->Splash = false;
    if (FParse::Param(FCommandLine::Get(), TEXT("TutorialEntrySmoke")))
    {
        static int32 Step = -2;
        static bool CloudsCaptured=false;
        if (!CloudsCaptured && State->SplashTime>3.3f)
        {
            CloudsCaptured=true;
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialClouds.png"),false,false);
        }
        static float Next = 1.2f;
        if (State->SplashTime > Next)
        {
            auto* PC = Cast<ATutorialController>(GetWorld()->GetFirstPlayerController());
            if (Step == -2) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialSplash.png"),false,false);
            else if (Step == -1) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialReady.png"),false,false);
            else if (Step < 14)
            {
                int32 I=Step/2;
                if (Step%2==0)
                {
                    PC->HandleClick(FVector2D(.85f,.14f+I*.115f));
                    PC->SetMouseLocation(780,380);
                    if (I==2) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialDrag.png"),false,false);
                }
                else
                {
                    FVector2D P; Scene->TargetScreen(I,P);
                    int32 W,H; PC->GetViewportSize(W,H);
                    const bool Wrong=Scene->TryDrop(I,FVector2D(5,5),FVector2D(W,H));
                    const bool Mismatch=Scene->TryDrop((I+1)%7,P,FVector2D(W,H));
                    UE_LOG(LogTemp,Display,TEXT("TUTORIAL_TARGET_MATCH continent=%d mismatchRejected=%d"),I,!Mismatch);
                    PC->HandleRelease(P);
                    UE_LOG(LogTemp,Display,TEXT("TUTORIAL_SMOKE continent=%d placed=%d outsideRejected=%d"),I,State->Placed[I],!Wrong);
                }
            }
            else if (Step==14) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialComplete.png"),false,false);
            else if (Step==15) PC->HandleClick(FVector2D(.85f,.14f+2*.115f));
            else if (Step==16) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialDive.png"),false,false);
            ++Step; Next=State->SplashTime+(Step==-1?5.f:1.2f);
        }
    }
    if (State->bTransitioning)
    {
        State->TransitionTime += DeltaSeconds;
        if (State->TransitionTime >= 3.0f)
            UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/PinWorldGameplay"), true, TEXT("Level=Room"));
    }
}
