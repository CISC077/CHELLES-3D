// CHELLES 3D - Touches des plugins CHELLES modifiables par chaque joueur (25/11/2026, agent Terrain)
#include "TouchesChelles.h"

#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SViewport.h"

namespace ChellesTouches
{
	static const TCHAR* Section = TEXT("ChellesTouches");

	struct FLigneTouche
	{
		const TCHAR* Id;
		const TCHAR* Libelle;
		FKey Origine;
	};

	static const TArray<FLigneTouche>& Lignes()
	{
		static TArray<FLigneTouche> L;
		if (L.Num() == 0)
		{
			L.Add({ TEXT("Pouvoir"), TEXT("POUVOIR"), EKeys::P });
			L.Add({ TEXT("ChangerPouvoir"), TEXT("CHANGER DE POUVOIR"), EKeys::Y });
			L.Add({ TEXT("Danse"), TEXT("DANSER"), EKeys::J });
			L.Add({ TEXT("Armements"), TEXT("INVENTAIRE ARMEMENTS"), EKeys::I });
			L.Add({ TEXT("RoueArmes"), TEXT("ROUE DES ARMES"), EKeys::Tab });
			L.Add({ TEXT("Utiliser"), TEXT("UTILISER / OUVRIR / MONTER"), EKeys::E });
			L.Add({ TEXT("Vehicule"), TEXT("VOLER / PRENDRE VÉHICULE"), EKeys::F });
			L.Add({ TEXT("Apparence"), TEXT("CHANGER D'APPARENCE"), EKeys::K });
			L.Add({ TEXT("TirSpecial"), TEXT("CHANGER DE TIR SPÉCIAL"), EKeys::O });
			L.Add({ TEXT("Flash"), TEXT("FLASH ÉLECTRIQUE"), EKeys::X });
			L.Add({ TEXT("Arsenal"), TEXT("ARME LOURDE SUIVANTE"), EKeys::Three });
			L.Add({ TEXT("Frappe"), TEXT("FRAPPE AÉRIENNE"), EKeys::Four });
			L.Add({ TEXT("Chat"), TEXT("CHAT GLOBAL"), EKeys::B });
			L.Add({ TEXT("Missions"), TEXT("MISSIONS"), EKeys::F10 }); // 26/11 : N = couper la musique
			L.Add({ TEXT("Klaxon"), TEXT("KLAXON"), EKeys::H });
			L.Add({ TEXT("Sirene"), TEXT("SIRÈNE (POLICE)"), EKeys::L });
			L.Add({ TEXT("Motoneige"), TEXT("MOTONEIGE"), EKeys::M });
			L.Add({ TEXT("Drone"), TEXT("DRONE"), EKeys::End });
			L.Add({ TEXT("Photo"), TEXT("MODE PHOTO"), EKeys::F12 }); // 26/11 : plugin ChellesPhoto
			L.Add({ TEXT("Console"), TEXT("CONSOLE"), FKey(FName(TEXT("²"))) });
		}
		return L;
	}

	/** Touches choisies, relues du fichier au premier besoin. */
	static TArray<FKey>& Choisies()
	{
		static TArray<FKey> C;
		static bool bLu = false;
		if (!bLu)
		{
			bLu = true;
			for (const FLigneTouche& L : Lignes())
			{
				FString Nom;
				if (GConfig)
				{
					GConfig->GetString(Section, L.Id, Nom, GGameUserSettingsIni);
				}
				const FKey K = Nom.IsEmpty() ? FKey() : FKey(FName(*Nom));
				C.Add(K.IsValid() ? K : L.Origine);
			}
		}
		return C;
	}

	int32 Nombre() { return Lignes().Num(); }
	FString Libelle(int32 i) { return Lignes().IsValidIndex(i) ? FString(Lignes()[i].Libelle) : FString(); }
	FKey Origine(int32 i) { return Lignes().IsValidIndex(i) ? Lignes()[i].Origine : FKey(); }
	FKey Actuelle(int32 i) { return Choisies().IsValidIndex(i) ? Choisies()[i] : FKey(); }

	int32 QuiUtilise(const FKey& K, int32 Sauf)
	{
		for (int32 i = 0; i < Nombre(); ++i)
		{
			if (i != Sauf && Actuelle(i) == K)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	static void Enregistrer(int32 i)
	{
		if (!GConfig || !Lignes().IsValidIndex(i))
		{
			return;
		}
		if (Actuelle(i) == Origine(i))
		{
			GConfig->RemoveKey(Section, Lignes()[i].Id, GGameUserSettingsIni);
		}
		else
		{
			GConfig->SetString(Section, Lignes()[i].Id, *Actuelle(i).GetFName().ToString(), GGameUserSettingsIni);
		}
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void Definir(int32 i, const FKey& K)
	{
		if (Choisies().IsValidIndex(i) && K.IsValid())
		{
			Choisies()[i] = K;
			Enregistrer(i);
		}
	}

	void ParDefaut(int32 i)
	{
		if (Choisies().IsValidIndex(i))
		{
			Choisies()[i] = Origine(i);
			Enregistrer(i);
		}
	}

	void ToutParDefaut()
	{
		for (int32 i = 0; i < Nombre(); ++i)
		{
			ParDefaut(i);
		}
	}

	// ------------------------------------------------ traducteur de touches

	/** Ligne dont la touche choisie est K (et deplacee), sinon INDEX_NONE. */
	static int32 LigneDeplaceeVers(const FKey& K)
	{
		for (int32 i = 0; i < Nombre(); ++i)
		{
			if (Actuelle(i) == K && Actuelle(i) != Origine(i))
			{
				return i;
			}
		}
		return INDEX_NONE;
	}

	/** Vrai si K est la touche d'origine d'une ligne deplacee ailleurs : elle ne doit plus rien faire. */
	static bool OrigineLiberee(const FKey& K)
	{
		for (int32 i = 0; i < Nombre(); ++i)
		{
			if (Origine(i) == K && Actuelle(i) != K)
			{
				return true;
			}
		}
		return false;
	}

	/** On ne traduit que pendant une partie, quand le joueur ne tape pas de texte et n'est pas dans un menu CHELLES. */
	static bool EnJeu(FSlateApplication& Slate)
	{
		UGameViewportClient* Ecran = GEngine ? GEngine->GameViewport : nullptr;
		if (!Ecran || !Ecran->GetWorld())
		{
			return false;
		}
		if (Ecran->ViewportConsole && Ecran->ViewportConsole->ConsoleActive())
		{
			return false;
		}
		const TSharedPtr<SViewport> Vue = Ecran->GetGameViewportWidget();
		bool bDansLeJeu = false;
		for (TSharedPtr<SWidget> W = Slate.GetUserFocusedWidget(0); W.IsValid(); W = W->GetParentWidget())
		{
			const FString Type = W->GetTypeAsString();
			if (Type.Contains(TEXT("EditableText")) || Type == TEXT("SMenuChelles"))
			{
				return false;
			}
			if (Vue.IsValid() && W == Vue)
			{
				bDansLeJeu = true;
			}
		}
		// Dans l'editeur : seulement dans la fenetre du Play (pas dans les panneaux de l'editeur)
		return bDansLeJeu || !GIsEditor;
	}

	class FTraducteur : public IInputProcessor
	{
	public:
		virtual void Tick(const float DeltaTime, FSlateApplication& Slate, TSharedRef<ICursor> Cursor) override
		{
			if (AEnvoyer.Num() == 0)
			{
				return;
			}
			TArray<FAEnvoyer> Liste = MoveTemp(AEnvoyer);
			AEnvoyer.Reset();
			bEnvoi = true;
			for (const FAEnvoyer& E : Liste)
			{
				const FKeyEvent Evenement(E.Touche, Slate.GetModifierKeys(), 0, E.bRepete, 0, 0);
				if (E.bAppui)
				{
					Slate.ProcessKeyDownEvent(Evenement);
				}
				else
				{
					Slate.ProcessKeyUpEvent(Evenement);
				}
			}
			bEnvoi = false;
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& Slate, const FKeyEvent& Evenement) override
		{
			return Appui(Slate, Evenement.GetKey(), Evenement.IsRepeat());
		}

		virtual bool HandleKeyUpEvent(FSlateApplication& Slate, const FKeyEvent& Evenement) override
		{
			return Relache(Evenement.GetKey());
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& Slate, const FPointerEvent& Evenement) override
		{
			const FKey K = Evenement.GetEffectingButton();
			return (K == EKeys::ThumbMouseButton || K == EKeys::ThumbMouseButton2) && Appui(Slate, K, false);
		}

		virtual bool HandleMouseButtonUpEvent(FSlateApplication& Slate, const FPointerEvent& Evenement) override
		{
			const FKey K = Evenement.GetEffectingButton();
			return (K == EKeys::ThumbMouseButton || K == EKeys::ThumbMouseButton2) && Relache(K);
		}

	private:
		struct FAEnvoyer
		{
			FKey Touche;
			bool bAppui;
			bool bRepete;
		};

		bool Appui(FSlateApplication& Slate, const FKey& K, bool bRepete)
		{
			if (bEnvoi || K.IsGamepadKey() || !EnJeu(Slate))
			{
				return false;
			}
			const int32 Ligne = LigneDeplaceeVers(K);
			if (Ligne != INDEX_NONE)
			{
				Enfoncees.Add(K, Origine(Ligne));
				AEnvoyer.Add({ Origine(Ligne), true, bRepete });
				return true; // la touche choisie fait l'action d'origine
			}
			if (OrigineLiberee(K))
			{
				Avalees.Add(K);
				return true; // l'ancienne touche ne fait plus rien
			}
			return false;
		}

		bool Relache(const FKey& K)
		{
			if (bEnvoi)
			{
				return false;
			}
			FKey Cible;
			if (Enfoncees.RemoveAndCopyValue(K, Cible))
			{
				AEnvoyer.Add({ Cible, false, false });
				return true;
			}
			return Avalees.Remove(K) > 0;
		}

		TArray<FAEnvoyer> AEnvoyer;
		TMap<FKey, FKey> Enfoncees;
		TSet<FKey> Avalees;
		bool bEnvoi = false;
	};

	static TSharedPtr<FTraducteur> GTraducteur;

	void Installer()
	{
		if (GTraducteur.IsValid() || !FSlateApplication::IsInitialized())
		{
			return;
		}
		GTraducteur = MakeShared<FTraducteur>();
		// En premier : il passe avant le menu ARMEMENTS et les autres pre-processeurs CHELLES
		FSlateApplication::Get().RegisterInputPreProcessor(GTraducteur, 0);
	}
}
