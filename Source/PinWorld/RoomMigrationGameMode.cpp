#include "RoomMigrationGameMode.h"
#include "MigrationSession.h"
#include "MigrationController.h"
#include "MigrationHUD.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ARoomMigrationGameMode::ARoomMigrationGameMode()
{
    HUDClass=AMigrationHUD::StaticClass();
    PlayerControllerClass=AMigrationPlayerController::StaticClass();
    PrimaryActorTick.bCanEverTick=true;
}
void ARoomMigrationGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
    Super::InitGame(MapName,Options,ErrorMessage);
}
void ARoomMigrationGameMode::StartPlay()
{
    Super::StartPlay();
    const FString MapName=GetWorld()->GetMapName();
    if (MapName.Contains(TEXT("PinWorldBoot")) && UGameplayStatics::ParseOption(OptionsString,TEXT("Menu"))!=TEXT("1")
        && !FParse::Param(FCommandLine::Get(),TEXT("RoomMigrationSmoke")) && !FParse::Param(FCommandLine::Get(),TEXT("MenuMigrationSmoke")))
    {
        UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/PinWorldTutorial"));
        return;
    }
    auto* PC=Cast<AMigrationPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC) return;
    Session=NewObject<UMigrationSession>(this); Session->Initialize(PC);
    PC->bShowMouseCursor=true;
    PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
    if (FParse::Param(FCommandLine::Get(),TEXT("RoomMigrationSmoke"))) Session->LoadLevel(FParse::Param(FCommandLine::Get(),TEXT("Foxy")));
    else if (MapName.Contains(TEXT("PinWorldGameplay")))
    {
        const bool Loaded=Session->LoadLevel(UGameplayStatics::ParseOption(OptionsString,TEXT("Level"))==TEXT("Foxy"));
        UE_LOG(LogTemp,Display,TEXT("TUTORIAL_ROOM_ARRIVAL loaded=%d"),Loaded);
    }
}
void ARoomMigrationGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    if (Session) Session->Tick(Dt);
    if (Session && GetWorld()->GetMapName().Contains(TEXT("PinWorldGameplay")) && FParse::Param(FCommandLine::Get(),TEXT("TutorialEntrySmoke")))
    {
        static float Time=0; Time+=Dt;
        static bool Captured=false;
        if (Time>3 && !Captured) { Captured=true; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/TutorialRoom.png"),false,false); }
        if (Time>5) GetWorld()->GetFirstPlayerController()->ConsoleCommand(TEXT("quit"));
    }
}
