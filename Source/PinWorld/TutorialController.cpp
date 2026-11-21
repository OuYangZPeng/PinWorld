#include "TutorialController.h"
#include "TutorialState.h"
#include "TutorialScene.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"

void ATutorialController::HandleClick(const FVector2D& P)
{
    if (!State || State->bTransitioning) return;
    if (State->Splash) { State->Splash=false; return; }
    int32 Index=INDEX_NONE;
    if (P.X>=.76f) Index=FMath::FloorToInt((P.Y-.1f)/.115f);
    for (TActorIterator<ATutorialScene> It(GetWorld()); It; ++It)
    {
        if (State->Completed && P.X<.76f)
        { int32 W,H; GetViewportSize(W,H); Index=It->PickContinent(P*FVector2D(W,H)); }
        if (Index<0 || Index>=7) return;
        It->FocusContinent(Index);
        if (State->Completed)
        { State->SelectedContinent=Index; State->bTransitioning=true; State->TransitionTime=0; }
        else if (!State->Placed[Index]) State->Selected=Index;
    }
}
void ATutorialController::HandleRelease(const FVector2D& P)
{
    if (!State || State->Selected==INDEX_NONE) return;
    int32 W,H; GetViewportSize(W,H);
    for (TActorIterator<ATutorialScene> It(GetWorld()); It; ++It) It->TryDrop(State->Selected,P,FVector2D(W,H));
    State->Selected=INDEX_NONE; bDragging=false;
}
void ATutorialController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);
    if (!State || State->bTransitioning) return;
    bShowMouseCursor=true;
    float X,Y; int32 W,H; GetViewportSize(W,H);
    if (!GetMousePosition(X,Y) || W<=0 || H<=0) return;
    const FVector2D Mouse(X,Y);
    const bool OnGlobeArea=X/W<.76f;

    if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        if (!State->Splash && OnGlobeArea && State->Selected==INDEX_NONE)
        { bGlobePressed=true; bRotating=false; PressPosition=LastPosition=Mouse; }
        else
        { HandleClick(FVector2D(X/W,Y/H)); bDragging=State->Selected!=INDEX_NONE; }
    }
    // Right mouse rotates at any time, including while carrying a continent.
    if (WasInputKeyJustPressed(EKeys::RightMouseButton) && !State->Splash)
    { bGlobePressed=true; bRotating=true; PressPosition=LastPosition=Mouse; }

    if (bGlobePressed && (IsInputKeyDown(EKeys::LeftMouseButton) || IsInputKeyDown(EKeys::RightMouseButton)))
    {
        if (!bRotating && FVector2D::Distance(Mouse,PressPosition)>4.f) bRotating=true;
        if (bRotating && Mouse!=LastPosition)
            for (TActorIterator<ATutorialScene> It(GetWorld()); It; ++It) It->RotateBy(Mouse-LastPosition);
        LastPosition=Mouse;
    }
    if (bGlobePressed && WasInputKeyJustReleased(EKeys::LeftMouseButton))
    {
        // A press on the globe without moving acts as a click (dive into a restored continent).
        if (!bRotating) HandleClick(FVector2D(X/W,Y/H));
        bGlobePressed=bRotating=false;
    }
    if (bGlobePressed && WasInputKeyJustReleased(EKeys::RightMouseButton)) bGlobePressed=bRotating=false;

    if (bDragging && WasInputKeyJustReleased(EKeys::LeftMouseButton)) HandleRelease(Mouse);
}
