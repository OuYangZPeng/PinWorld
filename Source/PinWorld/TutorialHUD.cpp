#include "TutorialHUD.h"
#include "TutorialController.h"
#include "TutorialScene.h"
#include "TutorialState.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

void ATutorialHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!State) if (auto* PC=Cast<ATutorialController>(GetOwningPlayerController())) State=PC->State;
    if (!State) return;
    int32 W,H; GetOwningPlayerController()->GetViewportSize(W,H);
    if (W<=0 || H<=0) return;
    if (State->Splash)
    {
        if (!SplashTexture)
        {
            SplashTexture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Resource/Login/iupiup.iupiup"));
            if (!SplashTexture) SplashTexture=FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir()/TEXT("Resource/Login/iupiup.png"));
            UE_LOG(LogTemp,Display,TEXT("TUTORIAL_PNG loaded=%d"),SplashTexture!=nullptr);
        }
        DrawRect(FLinearColor::Black,0,0,W,H);
        if (SplashTexture)
        {
            const float S=FMath::Min(float(W)/SplashTexture->GetSizeX(),float(H)/SplashTexture->GetSizeY());
            const float TW=S*SplashTexture->GetSizeX(), TH=S*SplashTexture->GetSizeY();
            DrawTexture(SplashTexture,(W-TW)/2,(H-TH)/2,TW,TH,0,0,1,1,FLinearColor::White,BLEND_Opaque);
        }
        return;
    }
    if (State->bTransitioning)
    {
        DrawRect(FLinearColor(0,0,0,FMath::Clamp((State->TransitionTime-2.4f)/.6f,0.f,1.f)),0,0,W,H);
        return;
    }
    DrawText(TEXT("PINWORLD | RESTORE THE EARTH"),FColor::White,30,25,GEngine->GetSmallFont(),1.4f);
    DrawText(State->Completed?TEXT("Choose a restored continent to dive into Room"):TEXT("Drag a continent onto its gold geographic target"),FColor::White,30,60);
    DrawRect(FLinearColor(.025f,.04f,.07f,.92f),W*.76f,0,W*.24f,H);
    const TCHAR* Names[]={TEXT("Asia"),TEXT("Europe"),TEXT("Africa"),TEXT("North America"),TEXT("South America"),TEXT("Oceania"),TEXT("Antarctica")};
    auto DrawShape=[&](int32 I,FVector2D Center,float Scale,FLinearColor Color)
    {
        const auto& Poly=ATutorialScene::Outline(I);
        FBox2D B(ForceInit); for (auto P:Poly) B+=P;
        const FVector2D C=B.GetCenter(); const float S=Scale/FMath::Max(B.GetSize().X,B.GetSize().Y);
        for (int32 J=0; J<Poly.Num(); ++J)
        {
            FVector2D A=(Poly[J]-C)*S, D=(Poly[(J+1)%Poly.Num()]-C)*S;
            DrawLine(Center.X+A.X,Center.Y-A.Y,Center.X+D.X,Center.Y-D.Y,Color,2);
        }
    };
    for (int32 I=0; I<7; ++I)
    {
        const float Y=H*(.1f+I*.115f);
        DrawRect(State->Placed[I]?FLinearColor(.08f,.27f,.16f):FLinearColor(.1f,.14f,.2f),W*.78f,Y,W*.2f,H*.095f);
        DrawShape(I,FVector2D(W*.815f,Y+H*.045f),W*.055f,FLinearColor::White);
        DrawText(Names[I],State->Placed[I]?FColor::Green:FColor::White,W*.855f,Y+H*.035f);
    }
    if (State->Selected!=INDEX_NONE)
    {
        float X,Y; GetOwningPlayerController()->GetMousePosition(X,Y);
        for (TActorIterator<ATutorialScene> It(GetWorld()); It; ++It)
        {
            TArray<FVector2D> Seg;
            It->TargetSegments(State->Selected,Seg);
            for (int32 J=0; J+1<Seg.Num(); J+=2) DrawLine(Seg[J].X,Seg[J].Y,Seg[J+1].X,Seg[J+1].Y,FLinearColor(1,.85f,.2f,.6f),1.5f);
            FVector2D P; if (It->TargetScreen(State->Selected,P)) { DrawLine(P.X-12,P.Y,P.X+12,P.Y,FLinearColor::Yellow,2); DrawLine(P.X,P.Y-12,P.X,P.Y+12,FLinearColor::Yellow,2); }
            // Ghost uses the same spherical projection and scale as the globe, so it lines up when dropped on target.
            It->GhostSegments(State->Selected,Seg);
            for (int32 J=0; J+1<Seg.Num(); J+=2) DrawLine(X+Seg[J].X,Y+Seg[J].Y,X+Seg[J+1].X,Y+Seg[J+1].Y,FLinearColor(1,.8f,.2f),2.5f);
        }
    }
}
