// CHELLES 3D - F11 (ou Alt + Entree) = plein ecran <-> fenetre, partout (jeu exporte ET Play dans l'editeur).
//
// Pourquoi : F11 "ne faisait rien". Dans l'editeur (Play), Unreal garde F11 pour son mode immersif et le jeu
// ne la voit jamais ; quand un menu est ouvert, l'ancien code de RaccourcisF ne la lisait pas non plus.
// Ici on lit la touche AVANT tout le monde (pre-processeur Slate, comme les raccourcis de Windows) :
//  - jeu exporte : reglage du jeu (GameUserSettings) fenetre <-> plein ecran fenetre, garde au prochain lancement ;
//  - editeur (Play) : la fenetre du jeu passe en plein ecran (avec le mode immersif d'Unreal, le jeu prend
//    tout l'ecran) ; elle redevient normale toute seule quand on arrete Play.
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GenericPlatform/ICursor.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/SWindow.h"

namespace ChellesPleinEcran
{
	class FProcesseurPleinEcran : public IInputProcessor
	{
	public:
		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
			// Editeur : Play arrete alors que la fenetre etait en plein ecran -> on la remet comme avant
			if (FenetreEditeur.IsValid() && (!GEngine || !GEngine->GameViewport))
			{
				if (TSharedPtr<SWindow> F = FenetreEditeur.Pin())
				{
					F->SetWindowMode(EWindowMode::Windowed);
				}
				FenetreEditeur.Reset();
			}
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Evenement) override
		{
			if (Evenement.IsRepeat())
			{
				return false;
			}
			const FKey K = Evenement.GetKey();
			const bool bF11 = (K == EKeys::F11) && !Evenement.IsControlDown() && !Evenement.IsAltDown();
			const bool bAltEntree = (K == EKeys::Enter) && Evenement.IsAltDown();
			if (!bF11 && !bAltEntree)
			{
				return false;
			}
			if (!GEngine || !GEngine->GameViewport)
			{
				return false; // pas de partie en cours (editeur sans Play) : on laisse Unreal faire
			}
			Basculer();
			// Jeu exporte : on garde la touche (sinon Unreal basculerait une 2e fois et rien ne changerait).
			// Editeur : on la laisse passer pour que le mode immersif agrandisse aussi la vue du jeu.
			return !GIsEditor;
		}

	private:
		void Basculer()
		{
			bool bPleinEcran = false;
			if (!GIsEditor)
			{
				UGameUserSettings* Reglages = GEngine->GetGameUserSettings();
				if (!Reglages)
				{
					return;
				}
				bPleinEcran = Reglages->GetFullscreenMode() == EWindowMode::Windowed;
				Reglages->SetFullscreenMode(bPleinEcran ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
				Reglages->ApplyResolutionSettings(false);
				Reglages->SaveSettings();
			}
			else
			{
				TSharedPtr<SWindow> Fenetre = GEngine->GameViewport->GetWindow();
				if (!Fenetre.IsValid())
				{
					return;
				}
				bPleinEcran = Fenetre->GetWindowMode() == EWindowMode::Windowed;
				Fenetre->SetWindowMode(bPleinEcran ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
				FenetreEditeur = bPleinEcran ? TWeakPtr<SWindow>(Fenetre) : TWeakPtr<SWindow>();
			}
			GEngine->AddOnScreenDebugMessage(int32(0xC4E11F), 2.f, FColor(160, 205, 255),
				bPleinEcran ? TEXT("PLEIN ECRAN  (F11 = revenir en fenetre)") : TEXT("FENETRE  (F11 = plein ecran)"));
			UE_LOG(LogTemp, Log, TEXT("[Raccourcis] Plein ecran : %s."), bPleinEcran ? TEXT("oui") : TEXT("non (fenetre)"));
		}

		TWeakPtr<SWindow> FenetreEditeur;
	};

	TSharedPtr<FProcesseurPleinEcran> GProcesseur;

	void Demarrer()
	{
		if (!GProcesseur.IsValid() && FSlateApplication::IsInitialized())
		{
			GProcesseur = MakeShared<FProcesseurPleinEcran>();
			FSlateApplication::Get().RegisterInputPreProcessor(GProcesseur);
		}
	}

	void Arreter()
	{
		if (GProcesseur.IsValid() && FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(GProcesseur);
		}
		GProcesseur.Reset();
	}
}
