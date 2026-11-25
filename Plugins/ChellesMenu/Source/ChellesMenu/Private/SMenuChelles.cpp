// CHELLES 3D - Dessin du menu (Slate, sans asset), style cyberpunk
#include "SMenuChelles.h"

#include "MenuChelles.h"
#include "GraphismesChelles.h"
#include "TouchesChelles.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/Package.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBorder.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HabillageChelles.h"

// ---------------------------------------------------------------- Sons

namespace
{
	TWeakObjectPtr<AMenuChelles> GMenuSons;

	void Son(ESonMenu Quel)
	{
		if (AMenuChelles* M = GMenuSons.Get())
		{
			M->JouerSon(Quel);
		}
	}
}

// ---------------------------------------------------------------- Couleurs et polices

namespace
{
	// Theme "lobby" bleu facon Fortnite (24/11 : le theme cyberpunk est abandonne).
	// Les anciens noms sont gardes pour ne rien casser : Cyan = bleu clair, Magenta = jaune d'accent,
	// Violet = bleu roi, Or = jaune, Fond = bleu nuit.
	const FLinearColor Cyan(FColor(150, 205, 255));
	const FLinearColor Magenta(FColor(255, 225, 15));
	const FLinearColor Violet(FColor(30, 75, 210));
	const FLinearColor Or(FColor(255, 225, 15));
	const FLinearColor Blanc(FColor(245, 248, 255));
	const FLinearColor Gris(FColor(110, 125, 170));
	const FLinearColor Fond(FColor(6, 20, 78, 235));
	const FLinearColor BleuNuit(FColor(4, 8, 40));

	FLinearColor Alpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
	}

	FSlateFontInfo Police(int32 Taille, const FLinearColor& Halo, const TCHAR* Graisse = TEXT("Black"))
	{
		FSlateFontInfo P = FCoreStyle::GetDefaultFontStyle(Graisse, Taille);
		// Contour bleu nuit (plus de halo neon)
		P.OutlineSettings.OutlineSize = FMath::Max(1, Taille / 28);
		P.OutlineSettings.OutlineColor = FLinearColor(0.01f, 0.02f, 0.12f, 0.85f);
		return P;
	}

	float Horloge()
	{
		double T = FPlatformTime::Seconds();
		T -= FMath::FloorToDouble(T / 3600.0) * 3600.0;
		return float(T);
	}

	void Ligne(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const FLinearColor& Couleur, float Epaisseur)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
	}

	/** Contour a coins coupes (haut-gauche et bas-droite), facon interface futuriste. */
	TArray<FVector2D> Chanfrein(const FVector2D& T, float Coin)
	{
		TArray<FVector2D> P;
		P.Add(FVector2D(Coin, 0.f));
		P.Add(FVector2D(T.X, 0.f));
		P.Add(FVector2D(T.X, T.Y - Coin));
		P.Add(FVector2D(T.X - Coin, T.Y));
		P.Add(FVector2D(0.f, T.Y));
		P.Add(FVector2D(0.f, Coin));
		P.Add(FVector2D(Coin, 0.f));
		return P;
	}
}

// ---------------------------------------------------------------- Reglages (OPTIONS)

namespace ChellesOptions
{
	static const TCHAR* Section = TEXT("ChellesMenu");

	static float LireFloat(const TCHAR* Cle, float ParDefaut)
	{
		float V = ParDefaut;
		if (GConfig)
		{
			GConfig->GetFloat(Section, Cle, V, GGameUserSettingsIni);
		}
		return V;
	}

	static void EcrireFloat(const TCHAR* Cle, float V)
	{
		if (GConfig)
		{
			GConfig->SetFloat(Section, Cle, V, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static bool LireBool(const TCHAR* Cle, bool ParDefaut)
	{
		bool V = ParDefaut;
		if (GConfig)
		{
			GConfig->GetBool(Section, Cle, V, GGameUserSettingsIni);
		}
		return V;
	}

	static void EcrireBool(const TCHAR* Cle, bool V)
	{
		if (GConfig)
		{
			GConfig->SetBool(Section, Cle, V, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static FString LireTexte(const TCHAR* Cle)
	{
		FString V;
		if (GConfig)
		{
			GConfig->GetString(Section, Cle, V, GGameUserSettingsIni);
		}
		return V;
	}

	static void EcrireTexte(const TCHAR* Cle, const FString& V)
	{
		if (GConfig)
		{
			GConfig->SetString(Section, Cle, *V, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static UGameUserSettings* Ecran()
	{
		return GEngine ? GEngine->GetGameUserSettings() : nullptr;
	}

	// --- Qualite CHELLES : BAS / MOYEN / HAUT (cle Qualite dans [ChellesMenu], -1 = jamais choisie)
	struct FPresetQualite
	{
		int32 Scalabilite;   // 0 Bas ... 3 Epique (qualite Unreal)
		float Resolution;    // % de la resolution de l'ecran (le jeu est agrandi, l'interface reste nette)
		float Vue;           // r.ViewDistanceScale : distance d'affichage des objets
		float Herbe;         // foliage.DensityScale / grass.DensityScale : densite de l'herbe et des plantes
		float Ombres;        // r.Shadow.DistanceScale : distance des ombres
		float DistanceLOD;   // r.StaticMeshLODDistanceScale : > 1 = versions simplifiees plus tot
		int32 BiaisLODPerso; // r.SkeletalMeshLODBias : 1 = personnages un cran plus simples
	};
	static const FPresetQualite Presets[3] = {
		{ 0, 70.f, 0.6f, 0.35f, 0.5f, 2.0f, 1 },   // BAS   : vieux PC / portable
		{ 1, 85.f, 0.8f, 0.65f, 0.75f, 1.3f, 0 },  // MOYEN
		{ 3, 100.f, 1.f, 1.f, 1.f, 1.f, 0 },       // HAUT  : comme avant
	};

	static void DefinirCVar(const TCHAR* Nom, float Valeur)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Nom))
		{
			V->Set(Valeur, ECVF_SetByGameSetting);
		}
	}

	static int32 QualiteChoisie()
	{
		float V = -1.f;
		if (GConfig)
		{
			GConfig->GetFloat(Section, TEXT("Qualite"), V, GGameUserSettingsIni);
		}
		return FMath::RoundToInt(V);
	}

	/** bEcran = vrai quand le joueur change le reglage (qualite Unreal + resolution sont alors enregistrees). */
	static void AppliquerQualite(int32 N, bool bEcran)
	{
		if (N < 0 || N > 2)
		{
			return;
		}
		const FPresetQualite& P = Presets[N];
		if (bEcran)
		{
			if (UGameUserSettings* R = Ecran())
			{
				R->SetOverallScalabilityLevel(P.Scalabilite);
				R->SetResolutionScaleValueEx(P.Resolution);
				R->ApplySettings(false);
			}
			if (GConfig)
			{
				GConfig->SetFloat(Section, TEXT("Qualite"), (float)N, GGameUserSettingsIni);
				GConfig->Flush(false, GGameUserSettingsIni);
			}
		}
		DefinirCVar(TEXT("r.ViewDistanceScale"), P.Vue);
		DefinirCVar(TEXT("foliage.DensityScale"), P.Herbe);
		DefinirCVar(TEXT("grass.DensityScale"), P.Herbe);
		DefinirCVar(TEXT("r.Shadow.DistanceScale"), P.Ombres);
		DefinirCVar(TEXT("r.StaticMeshLODDistanceScale"), P.DistanceLOD);
		DefinirCVar(TEXT("r.SkeletalMeshLODBias"), (float)P.BiaisLODPerso);
	}

	static TWeakObjectPtr<UWorld> GMonde;
	static TWeakObjectPtr<APlayerController> GJoueur;

	// --- Audio
	static float Volume() { return FMath::Clamp(LireFloat(TEXT("VolumeGeneral"), 1.f), 0.f, 1.f); }
	static bool SonCoupe() { return LireBool(TEXT("SonCoupe"), false); }

	/** Canaux du pack FPS_Game (SC_Music, SC_SFX, SC_Voice) : les sons ranges dans ces classes. */
	struct FCanal
	{
		const TCHAR* Cle;
		const TCHAR* Libelle;
		const TCHAR* Classe;
	};
	static const FCanal Canaux[] = {
		{ TEXT("VolumeMusique"), TEXT("MUSIQUE"), TEXT("SC_Music") },
		{ TEXT("VolumeEffets"), TEXT("EFFETS (SFX)"), TEXT("SC_SFX") },
		{ TEXT("VolumeVoix"), TEXT("VOIX"), TEXT("SC_Voice") },
		// Pas de classe de son : lu directement par le plugin ChellesAmbiance (sons de quartier, vent, nuit...)
		{ TEXT("VolumeAmbiance"), TEXT("AMBIANCE"), nullptr },
		// Pas de classe de son : lu directement par le plugin ChellesSonsVehicules (moteurs, klaxons)
		{ TEXT("VolumeVehicules"), TEXT("V\u00C9HICULES"), nullptr },
	};
	static USoundMix* GMix = nullptr;
	static TWeakObjectPtr<UWorld> GMondeMix;

	static float VolumeCanal(int32 i) { return FMath::Clamp(LireFloat(Canaux[i].Cle, 1.f), 0.f, 1.f); }

	static USoundClass* ClasseSon(const TCHAR* Nom)
	{
		const FString Chemin = FString::Printf(TEXT("/Game/FPS_Game/Blueprints/GameInstanceSettings/%s.%s"), Nom, Nom);
		return LoadObject<USoundClass>(nullptr, *Chemin);
	}

	static void AppliquerCanaux()
	{
		UWorld* Monde = GMonde.Get();
		if (!Monde)
		{
			return;
		}
		if (!GMix)
		{
			GMix = NewObject<USoundMix>(GetTransientPackage(), TEXT("ChellesMenuMix"));
			GMix->AddToRoot();
		}
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Canaux); ++i)
		{
			if (!Canaux[i].Classe)
			{
				continue;
			}
			if (USoundClass* Classe = ClasseSon(Canaux[i].Classe))
			{
				UGameplayStatics::SetSoundMixClassOverride(Monde, GMix, Classe, VolumeCanal(i), 1.f, 0.f, true);
			}
		}
		if (GMondeMix.Get() != Monde)
		{
			UGameplayStatics::PushSoundMixModifier(Monde, GMix);
			GMondeMix = Monde;
		}
	}

	static void AppliquerVolume()
	{
		const float V = SonCoupe() ? 0.f : Volume();
		FAudioDevice* Audio = GMonde.IsValid() ? GMonde->GetAudioDeviceRaw() : nullptr;
		if (!Audio && GEngine)
		{
			Audio = GEngine->GetMainAudioDeviceRaw();
		}
		if (Audio)
		{
			Audio->SetTransientMasterVolume(V);
		}
		AppliquerCanaux();
	}

	// --- Souris / manette
	static float Sensibilite() { return FMath::Clamp(LireFloat(TEXT("SensibiliteSouris"), 1.f), 0.1f, 5.f); }
	static bool InverserSouris() { return LireBool(TEXT("InverserSouris"), false); }
	static bool InverserManette() { return LireBool(TEXT("InverserManette"), false); }
	static bool Qwerty() { return LireBool(TEXT("ClavierQwerty"), false); }

	/** AZERTY : Z/Q pour avancer/aller a gauche.  QWERTY : W/A a la place (S et D ne bougent pas).
	 *  En QWERTY, W sert a avancer : les actions posees sur W (camera) passent sur Z, meme place sur le clavier. */
	static void Disposition(UPlayerInput* Entrees, bool bQwerty)
	{
		if (bQwerty)
		{
			TArray<FInputActionKeyMapping> ActionsW;
			for (const FInputActionKeyMapping& M : Entrees->ActionMappings)
			{
				if (M.Key == EKeys::W)
				{
					ActionsW.Add(M);
				}
			}
			for (const FInputActionKeyMapping& M : ActionsW)
			{
				Entrees->RemoveActionMapping(M);
				FInputActionKeyMapping Nouveau = M;
				Nouveau.Key = EKeys::Z;
				Entrees->AddActionMapping(Nouveau);
			}
			if (ActionsW.Num() > 0)
			{
				Entrees->ForceRebuildingKeyMaps(false);
			}
		}

		const FKey Avant = bQwerty ? EKeys::Z : EKeys::W;
		const FKey Gauche = bQwerty ? EKeys::Q : EKeys::A;
		const FKey NouvelAvant = bQwerty ? EKeys::W : EKeys::Z;
		const FKey NouvelleGauche = bQwerty ? EKeys::A : EKeys::Q;

		TArray<FInputAxisKeyMapping> AChanger;
		for (const FInputAxisKeyMapping& M : Entrees->AxisMappings)
		{
			if (M.Key == Avant || M.Key == Gauche)
			{
				AChanger.Add(M);
			}
		}
		if (AChanger.Num() == 0)
		{
			return;
		}
		for (const FInputAxisKeyMapping& M : AChanger)
		{
			Entrees->RemoveAxisMapping(M);
			FInputAxisKeyMapping Nouveau = M;
			Nouveau.Key = (M.Key == Avant) ? NouvelAvant : NouvelleGauche;
			Entrees->AddAxisMapping(Nouveau);
		}
		Entrees->ForceRebuildingKeyMaps(false);
	}

	/** Raccourcis clavier modifiables : une touche par defaut, et les actions/axes qui l'utilisent. */
	struct FToucheChelles
	{
		FString Id;
		FString Libelle;
		FKey Defaut;
		TArray<FName> Noms;
	};

	static const TArray<FToucheChelles>& ListeTouches()
	{
		static TArray<FToucheChelles> Liste;
		if (Liste.Num() == 0)
		{
			auto Ajouter = [](const TCHAR* Id, const TCHAR* Libelle, const FKey& Defaut, const TCHAR* Noms)
			{
				FToucheChelles T;
				T.Id = FString(TEXT("Touche_")) + Id;
				T.Libelle = Libelle;
				T.Defaut = Defaut;
				TArray<FString> Morceaux;
				FString(Noms).ParseIntoArray(Morceaux, TEXT(","), true);
				for (const FString& M : Morceaux)
				{
					T.Noms.Add(FName(*M));
				}
				Liste.Add(T);
			};
			Ajouter(TEXT("Avancer"), TEXT("AVANCER"), EKeys::Z, TEXT("MoveForward,Throttle"));
			Ajouter(TEXT("Reculer"), TEXT("RECULER"), EKeys::S, TEXT("MoveForward,Throttle"));
			Ajouter(TEXT("Gauche"), TEXT("ALLER \u00C0 GAUCHE"), EKeys::Q, TEXT("MoveRight,Steer,Rotate"));
			Ajouter(TEXT("Droite"), TEXT("ALLER \u00C0 DROITE"), EKeys::D, TEXT("MoveRight,Steer,Rotate"));
			Ajouter(TEXT("Sauter"), TEXT("SAUTER"), EKeys::SpaceBar, TEXT("Jump"));
			Ajouter(TEXT("Accroupir"), TEXT("S'ACCROUPIR"), EKeys::C, TEXT("Crouch"));
			Ajouter(TEXT("Sprint"), TEXT("SPRINTER"), EKeys::LeftShift, TEXT("Sprint"));
			Ajouter(TEXT("Tirer"), TEXT("TIRER"), EKeys::LeftMouseButton, TEXT("Fire"));
			Ajouter(TEXT("Viser"), TEXT("VISER"), EKeys::RightMouseButton, TEXT("ADS,Aim"));
			Ajouter(TEXT("Recharger"), TEXT("RECHARGER"), EKeys::R, TEXT("Reload"));
			// 25/11 : UTILISER (E) et INVENTAIRE (I) passent dans les touches CHELLES (TouchesChelles.cpp), qui couvrent aussi les plugins
			Ajouter(TEXT("Grenade"), TEXT("GRENADE"), EKeys::G, TEXT("GrenadeThrow"));
			Ajouter(TEXT("CorpsACorps"), TEXT("CORPS \u00C0 CORPS"), EKeys::V, TEXT("Melee"));
			Ajouter(TEXT("Camera"), TEXT("CAM\u00C9RA"), EKeys::W, TEXT("Camera,ChangeCamera"));
		}
		return Liste;
	}

	/** Touche choisie par le joueur (invalide = touche par defaut). */
	static FKey ToucheChoisie(const FToucheChelles& T)
	{
		const FString Nom = LireTexte(*T.Id);
		return Nom.IsEmpty() ? EKeys::Invalid : FKey(FName(*Nom));
	}

	static void AppliquerTouches(UPlayerInput* Entrees)
	{
		bool bChange = false;
		for (const FToucheChelles& T : ListeTouches())
		{
			const FKey Nouvelle = ToucheChoisie(T);
			if (!Nouvelle.IsValid() || Nouvelle == T.Defaut)
			{
				continue;
			}
			TArray<FInputActionKeyMapping> Actions;
			for (const FInputActionKeyMapping& M : Entrees->ActionMappings)
			{
				if (M.Key == T.Defaut && T.Noms.Contains(M.ActionName))
				{
					Actions.Add(M);
				}
			}
			for (const FInputActionKeyMapping& M : Actions)
			{
				Entrees->RemoveActionMapping(M);
				FInputActionKeyMapping Nouveau = M;
				Nouveau.Key = Nouvelle;
				Entrees->AddActionMapping(Nouveau);
				bChange = true;
			}
			TArray<FInputAxisKeyMapping> Axes;
			for (const FInputAxisKeyMapping& M : Entrees->AxisMappings)
			{
				if (M.Key == T.Defaut && T.Noms.Contains(M.AxisName))
				{
					Axes.Add(M);
				}
			}
			for (const FInputAxisKeyMapping& M : Axes)
			{
				Entrees->RemoveAxisMapping(M);
				FInputAxisKeyMapping Nouveau = M;
				Nouveau.Key = Nouvelle;
				Entrees->AddAxisMapping(Nouveau);
				bChange = true;
			}
		}
		if (bChange)
		{
			Entrees->ForceRebuildingKeyMaps(false);
		}
	}

	static void Inversion(UPlayerInput* Entrees, const FKey& Touche, bool bVoulu)
	{
		if (Entrees && Entrees->GetInvertAxisKey(Touche) != bVoulu)
		{
			Entrees->InvertAxisKey(Touche);
		}
	}

	void AppliquerAuJoueur(APlayerController* Joueur)
	{
		if (!Joueur)
		{
			return;
		}
		GJoueur = Joueur;
		if (UPlayerInput* Entrees = Joueur->PlayerInput)
		{
			AppliquerTouches(Entrees);
			Disposition(Entrees, Qwerty());
			// 1.0 = vitesse standard d'Unreal (0.07). Avant, 1.0 etait applique tel quel : souris 14 fois trop rapide.
			Entrees->SetMouseSensitivity(Sensibilite() * 0.07f);
			Inversion(Entrees, EKeys::MouseY, InverserSouris());
			Inversion(Entrees, EKeys::Gamepad_RightY, InverserManette());
		}
	}

	/** Apres un changement de touche : on repart des touches du projet puis on reapplique tout. */
	static void ReAppliquerTout()
	{
		APlayerController* Joueur = GJoueur.Get();
		if (Joueur && Joueur->PlayerInput)
		{
			Joueur->PlayerInput->ForceRebuildingKeyMaps(true);
			AppliquerAuJoueur(Joueur);
		}
	}

	void ReappliquerQualite()
	{
		AppliquerQualite(QualiteChoisie(), false);
	}

	void AppliquerAuDemarrage(UWorld* Monde)
	{
		GMonde = Monde;
		AppliquerVolume();
		AppliquerQualite(QualiteChoisie(), false);
		// 25/11/2026 : reglages fins de l'onglet AVANCES (apres le prereglage) + touches CHELLES modifiables
		ChellesGraphismes::AppliquerAuDemarrage(Monde);
		ChellesTouches::Installer();
	}

	bool ChatActif() { return LireBool(TEXT("Chat"), true); }
	bool AideActive() { return LireBool(TEXT("Aide"), true); }
	bool PvpActif() { return LireBool(TEXT("Pvp"), true); }
	float VolumeMusique() { return VolumeCanal(0); }
	float VolumeGeneral() { return Volume(); }
	bool MusiqueCoupee() { return LireBool(TEXT("MusiqueCoupee"), false); }
	void DefinirMusiqueCoupee(bool b) { EcrireBool(TEXT("MusiqueCoupee"), b); }
	FString Version() { return TEXT("v0.1.0"); }
	void DefinirVolumeGeneral(float V)
	{
		EcrireFloat(TEXT("VolumeGeneral"), FMath::Clamp(V, 0.f, 1.f));
		AppliquerVolume();
	}
	void DefinirVolumeCanal(int32 Canal, float V)
	{
		if (Canal >= 0 && Canal < (int32)UE_ARRAY_COUNT(Canaux))
		{
			EcrireFloat(Canaux[Canal].Cle, FMath::Clamp(V, 0.f, 1.f));
			AppliquerCanaux();
			if (AMenuChelles* M = GMenuSons.Get())
			{
				M->MajVolumeMusique();
			}
		}
	}
	float VolumeEffets() { return VolumeCanal(1); }
	bool Manette() { return LireBool(TEXT("Manette"), false); }
	int32 Difficulte() { return FMath::Clamp(FMath::RoundToInt(LireFloat(TEXT("Difficulte"), 1.f)), 0, 3); }

	static int32 Boucle(int32 Valeur, int32 Sens, int32 Nombre)
	{
		return ((Valeur + Sens) % Nombre + Nombre) % Nombre;
	}

	static FText Ligne(const TCHAR* Nom, const FString& Valeur)
	{
		return FText::FromString(FString::Printf(TEXT("%s   < %s >"), Nom, *Valeur));
	}

	// --- Affichage & graphisme
	static FText TexteQualite()
	{
		static const TCHAR* Noms[] = { TEXT("BAS"), TEXT("MOYEN"), TEXT("HAUT") };
		const int32 N = QualiteChoisie();
		return Ligne(TEXT("QUALIT\u00C9"), (N >= 0 && N <= 2) ? Noms[N] : TEXT("HAUT"));
	}

	static void ChangerQualite(int32 Sens)
	{
		const int32 N = QualiteChoisie();
		AppliquerQualite(Boucle(N < 0 ? 2 : N, Sens, 3), true);
		ChellesGraphismes::QualiteChangee(); // les reglages AVANCES repassent en AUTO
	}

	static const EWindowMode::Type ModesFenetre[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	static FText TexteAffichage()
	{
		UGameUserSettings* R = Ecran();
		const EWindowMode::Type M = R ? R->GetFullscreenMode() : EWindowMode::Windowed;
		const TCHAR* Nom = M == EWindowMode::Fullscreen ? TEXT("PLEIN \u00C9CRAN")
			: M == EWindowMode::WindowedFullscreen ? TEXT("PLEIN \u00C9CRAN FEN\u00CATR\u00C9") : TEXT("FEN\u00CATRE");
		return Ligne(TEXT("AFFICHAGE"), Nom);
	}

	static void ChangerAffichage(int32 Sens)
	{
		if (UGameUserSettings* R = Ecran())
		{
			int32 Index = 0;
			for (int32 i = 0; i < 3; ++i)
			{
				if (ModesFenetre[i] == R->GetFullscreenMode())
				{
					Index = i;
				}
			}
			R->SetFullscreenMode(ModesFenetre[Boucle(Index, Sens, 3)]);
			R->ApplySettings(false);
		}
	}

	static TArray<FIntPoint> Resolutions()
	{
		TArray<FIntPoint> Liste;
		const FIntPoint Toutes[] = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
		UGameUserSettings* R = Ecran();
		const FIntPoint Bureau = R ? R->GetDesktopResolution() : FIntPoint(1920, 1080);
		for (const FIntPoint& P : Toutes)
		{
			if (P.X <= Bureau.X && P.Y <= Bureau.Y)
			{
				Liste.Add(P);
			}
		}
		Liste.AddUnique(Bureau);
		Liste.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
		return Liste;
	}

	static FText TexteResolution()
	{
		UGameUserSettings* R = Ecran();
		const FIntPoint P = R ? R->GetScreenResolution() : FIntPoint::ZeroValue;
		return Ligne(TEXT("R\u00C9SOLUTION"), FString::Printf(TEXT("%d x %d"), P.X, P.Y));
	}

	static void ChangerResolution(int32 Sens)
	{
		UGameUserSettings* R = Ecran();
		const TArray<FIntPoint> Liste = Resolutions();
		if (!R || Liste.Num() == 0)
		{
			return;
		}
		int32 Index = Liste.IndexOfByKey(R->GetScreenResolution());
		Index = Index == INDEX_NONE ? Liste.Num() - 1 : Boucle(Index, Sens, Liste.Num());
		R->SetScreenResolution(Liste[Index]);
		R->ApplySettings(false);
	}

	static FText TexteVSync()
	{
		UGameUserSettings* R = Ecran();
		return Ligne(TEXT("SYNCHRO VERTICALE"), (R && R->IsVSyncEnabled()) ? TEXT("OUI") : TEXT("NON"));
	}

	static void ChangerVSync(int32)
	{
		if (UGameUserSettings* R = Ecran())
		{
			R->SetVSyncEnabled(!R->IsVSyncEnabled());
			R->ApplySettings(false);
		}
	}

	static const float LimitesFPS[] = { 30.f, 60.f, 120.f, 144.f, 0.f };

	static FText TexteFPS()
	{
		UGameUserSettings* R = Ecran();
		const float L = R ? R->GetFrameRateLimit() : 0.f;
		return Ligne(TEXT("IMAGES / SECONDE"), L <= 0.f ? FString(TEXT("ILLIMIT\u00C9")) : FString::Printf(TEXT("%d"), FMath::RoundToInt(L)));
	}

	static void ChangerFPS(int32 Sens)
	{
		if (UGameUserSettings* R = Ecran())
		{
			int32 Index = 4;
			for (int32 i = 0; i < 5; ++i)
			{
				if (FMath::IsNearlyEqual(LimitesFPS[i], R->GetFrameRateLimit()))
				{
					Index = i;
				}
			}
			R->SetFrameRateLimit(LimitesFPS[Boucle(Index, Sens, 5)]);
			R->ApplySettings(false);
		}
	}

	// --- Audio
	static FText TexteVolume()
	{
		return Ligne(TEXT("VOLUME G\u00C9N\u00C9RAL"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Volume() * 100.f)));
	}

	static void ChangerVolume(int32 Sens)
	{
		int32 Pas = FMath::RoundToInt(Volume() * 10.f);
		Pas = Boucle(Pas, Sens, 11);
		EcrireFloat(TEXT("VolumeGeneral"), Pas / 10.f);
		AppliquerVolume();
	}

	static FText TexteCanal(int32 i)
	{
		return Ligne(Canaux[i].Libelle, FString::Printf(TEXT("%d %%"), FMath::RoundToInt(VolumeCanal(i) * 100.f)));
	}

	static void ChangerCanal(int32 i, int32 Sens)
	{
		const int32 Pas = Boucle(FMath::RoundToInt(VolumeCanal(i) * 10.f), Sens, 11);
		EcrireFloat(Canaux[i].Cle, Pas / 10.f);
		AppliquerCanaux();
		if (AMenuChelles* M = GMenuSons.Get())
		{
			M->MajVolumeMusique();
		}
	}

	static FText TexteSon()
	{
		return Ligne(TEXT("SON"), SonCoupe() ? TEXT("COUP\u00C9") : TEXT("ACTIV\u00C9"));
	}

	static void ChangerSon(int32)
	{
		EcrireBool(TEXT("SonCoupe"), !SonCoupe());
		AppliquerVolume();
	}

	// --- Clavier / joystick
	static FText TexteSensibilite()
	{
		return Ligne(TEXT("SENSIBILIT\u00C9 SOURIS"), FString::Printf(TEXT("%.1f"), Sensibilite()));
	}

	static void ChangerSensibilite(int32 Sens)
	{
		int32 Pas = FMath::RoundToInt(Sensibilite() * 10.f) / 2; // pas de 0.2
		Pas = FMath::Clamp(Pas + Sens, 1, 15);
		EcrireFloat(TEXT("SensibiliteSouris"), Pas * 0.2f);
		AppliquerAuJoueur(GJoueur.Get());
	}

	static FText TexteInverserSouris()
	{
		return Ligne(TEXT("SOURIS HAUT/BAS INVERS\u00C9"), InverserSouris() ? TEXT("OUI") : TEXT("NON"));
	}

	static void ChangerInverserSouris(int32)
	{
		EcrireBool(TEXT("InverserSouris"), !InverserSouris());
		AppliquerAuJoueur(GJoueur.Get());
	}

	static FText TexteInverserManette()
	{
		return Ligne(TEXT("JOYSTICK HAUT/BAS INVERS\u00C9"), InverserManette() ? TEXT("OUI") : TEXT("NON"));
	}

	static void ChangerInverserManette(int32)
	{
		EcrireBool(TEXT("InverserManette"), !InverserManette());
		AppliquerAuJoueur(GJoueur.Get());
	}

	static FText TexteControle()
	{
		return Ligne(TEXT("JOUER AVEC"), Manette() ? TEXT("MANETTE") : TEXT("CLAVIER + SOURIS"));
	}

	static void ChangerControle(int32)
	{
		EcrireBool(TEXT("Manette"), !Manette());
	}

	static FText TexteClavier()
	{
		return Ligne(TEXT("CLAVIER"), Qwerty() ? TEXT("QWERTY (WASD)") : TEXT("AZERTY (ZQSD)"));
	}

	static void ChangerClavier(int32)
	{
		EcrireBool(TEXT("ClavierQwerty"), !Qwerty());
		ReAppliquerTout();
	}

	/** Message sous la liste des touches (touche deja prise...). */
	static FString GMessageTouches;

	/** Touche actuelle d'une ligne de la 1re liste (actions du kit FPS), avec ZQSD / WASD. */
	static FKey ToucheAncienne(const FToucheChelles& T)
	{
		FKey K = ToucheChoisie(T);
		if (!K.IsValid())
		{
			K = T.Defaut;
			if (Qwerty() && K == EKeys::Z) { K = EKeys::W; }
			else if (Qwerty() && K == EKeys::W) { K = EKeys::Z; }
			if (Qwerty() && K == EKeys::Q) { K = EKeys::A; }
		}
		return K;
	}

	/** Nom de la fonction qui utilise deja la touche K (vide = libre). i = ligne a ignorer (index global). */
	static FString ToucheDejaPrise(const FKey& K, int32 i)
	{
		static const FKey Reservees[] = { EKeys::Escape, EKeys::Enter, EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::MiddleMouseButton, EKeys::F11 };
		for (const FKey& R : Reservees)
		{
			if (K == R)
			{
				return TEXT("le jeu (touche r\u00E9serv\u00E9e)");
			}
		}
		const TArray<FToucheChelles>& Liste = ListeTouches();
		for (int32 j = 0; j < Liste.Num(); ++j)
		{
			if (j != i && ToucheAncienne(Liste[j]) == K)
			{
				return Liste[j].Libelle;
			}
		}
		const int32 Autre = ChellesTouches::QuiUtilise(K, i - Liste.Num());
		return Autre == INDEX_NONE ? FString() : ChellesTouches::Libelle(Autre);
	}

	static int32 NombreTouches()
	{
		return ListeTouches().Num() + ChellesTouches::Nombre();
	}

	static FText TexteTouche(int32 i, bool bEnAttente)
	{
		const TArray<FToucheChelles>& Liste = ListeTouches();
		if (i >= Liste.Num() && i < NombreTouches())
		{
			const int32 c = i - Liste.Num();
			const FString Nom = bEnAttente ? FString(TEXT("APPUIE SUR UNE TOUCHE...")) : ChellesTouches::Actuelle(c).GetDisplayName().ToString().ToUpper();
			return FText::FromString(FString::Printf(TEXT("%s   [ %s ]"), *ChellesTouches::Libelle(c), *Nom));
		}
		if (!Liste.IsValidIndex(i))
		{
			return FText::GetEmpty();
		}
		const FToucheChelles& T = Liste[i];
		FString Nom;
		if (bEnAttente)
		{
			Nom = TEXT("APPUIE SUR UNE TOUCHE...");
		}
		else
		{
			FKey K = ToucheChoisie(T);
			if (!K.IsValid())
			{
				K = T.Defaut;
				if (Qwerty() && K == EKeys::Z) { K = EKeys::W; }
				else if (Qwerty() && K == EKeys::W) { K = EKeys::Z; }
				if (Qwerty() && K == EKeys::Q) { K = EKeys::A; }
			}
			Nom = K.GetDisplayName().ToString().ToUpper();
		}
		return FText::FromString(FString::Printf(TEXT("%s   [ %s ]"), *T.Libelle, *Nom));
	}

	static void DefinirTouche(int32 i, const FKey& K)
	{
		const TArray<FToucheChelles>& Liste = ListeTouches();
		const FString Prise = ToucheDejaPrise(K, i);
		if (!Prise.IsEmpty())
		{
			GMessageTouches = FString::Printf(TEXT("%s est d\u00E9j\u00E0 utilis\u00E9e par : %s. Choisis une autre touche."), *K.GetDisplayName().ToString().ToUpper(), *Prise);
			return;
		}
		GMessageTouches.Empty();
		if (i >= Liste.Num() && i < NombreTouches())
		{
			ChellesTouches::Definir(i - Liste.Num(), K);
			return;
		}
		if (Liste.IsValidIndex(i))
		{
			EcrireTexte(*Liste[i].Id, K == Liste[i].Defaut ? FString() : K.GetFName().ToString());
			ReAppliquerTout();
		}
	}

	static void ToucheParDefaut(int32 i)
	{
		const TArray<FToucheChelles>& Liste = ListeTouches();
		GMessageTouches.Empty();
		if (i >= Liste.Num() && i < NombreTouches())
		{
			ChellesTouches::ParDefaut(i - Liste.Num());
			return;
		}
		if (Liste.IsValidIndex(i))
		{
			EcrireTexte(*Liste[i].Id, FString());
			ReAppliquerTout();
		}
	}

	static void ToutesLesTouchesParDefaut()
	{
		for (const FToucheChelles& T : ListeTouches())
		{
			EcrireTexte(*T.Id, FString());
		}
		ChellesTouches::ToutParDefaut();
		GMessageTouches.Empty();
		ReAppliquerTout();
	}

	// --- Chat et difficulte
	static FText TexteChat()
	{
		return FText::FromString(FString::Printf(TEXT("CHAT JOUEURS (MULTI) : %s"), ChatActif() ? TEXT("ON") : TEXT("OFF")));
	}

	static void ChangerChat()
	{
		EcrireBool(TEXT("Chat"), !ChatActif());
	}

	// --- Chat global (joueurs du jeu + visiteurs du site), lu par le plugin ChellesChat
	static bool ChatGlobalActif() { return LireBool(TEXT("ChatGlobal"), true); }

	static FText TexteChatGlobal()
	{
		return FText::FromString(FString::Printf(TEXT("CHAT GLOBAL SITE + JEU (B) : %s"), ChatGlobalActif() ? TEXT("ON") : TEXT("OFF")));
	}

	static void ChangerChatGlobal()
	{
		EcrireBool(TEXT("ChatGlobal"), !ChatGlobalActif());
	}

	static FText TexteAide()
	{
		return FText::FromString(FString::Printf(TEXT("AIDE TUTO & ASTUCES : %s"), AideActive() ? TEXT("ON") : TEXT("OFF")));
	}

	static void ChangerAide()
	{
		EcrireBool(TEXT("Aide"), !AideActive());
	}

	static FText TextePvp()
	{
		return FText::FromString(FString::Printf(TEXT("PVP (SI TU H\u00C9BERGES) : %s"), PvpActif() ? TEXT("ON") : TEXT("OFF")));
	}

	static void ChangerPvp()
	{
		EcrireBool(TEXT("Pvp"), !PvpActif());
	}

	static const TCHAR* NomsDifficulte[] = { TEXT("FACILE"), TEXT("NORMAL"), TEXT("DIFFICILE"), TEXT("EXTR\u00CAME") };

	static FText TexteDifficulte()
	{
		return Ligne(TEXT("DIFFICULT\u00C9"), NomsDifficulte[Difficulte()]);
	}

	static void ChangerDifficulte(int32 Sens)
	{
		EcrireFloat(TEXT("Difficulte"), float(Boucle(Difficulte(), Sens, 4)));
	}
}

// ---------------------------------------------------------------- Fond anime

void SDecorCyber::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SDecorCyber::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	// Decor "lobby" bleu (theme Fortnite, sans neon) : degrade de gauche a droite,
	// colonne sombre a gauche, bandes de lumiere horizontales qui glissent doucement.
	const FVector2D T = G.GetLocalSize();
	const float Temps = Horloge();
	const FSlateBrush* Pinceau = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	TArray<FSlateGradientStop> Degrade;
	Degrade.Add(FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0.012f, 0.03f, 0.16f, 1.f)));
	Degrade.Add(FSlateGradientStop(FVector2D(T.X * 0.55f, 0.f), FLinearColor(0.05f, 0.17f, 0.55f, 1.f)));
	Degrade.Add(FSlateGradientStop(FVector2D(T.X, 0.f), FLinearColor(0.02f, 0.07f, 0.28f, 1.f)));
	FSlateDrawElement::MakeGradient(Sortie, Couche, G.ToPaintGeometry(), Degrade, Orient_Vertical);

	// Colonne sombre a gauche + trait jaune (rappel des onglets en colonne)
	FSlateDrawElement::MakeBox(Sortie, Couche + 1, G.ToPaintGeometry(FVector2D::ZeroVector, FVector2D(26.f, T.Y)),
		Pinceau, ESlateDrawEffect::None, FLinearColor(0.f, 0.01f, 0.06f, 0.6f));
	FSlateDrawElement::MakeBox(Sortie, Couche + 1, G.ToPaintGeometry(FVector2D(26.f, 0.f), FVector2D(4.f, T.Y)),
		Pinceau, ESlateDrawEffect::None, Alpha(Or, 0.85f));

	// Bandes de lumiere horizontales
	for (int32 k = 0; k < 4; ++k)
	{
		const float Y = FMath::Fmod(T.Y * (0.15f + 0.23f * k) + Temps * (8.f + 3.f * k), T.Y + 120.f) - 60.f;
		const float H = 30.f + 22.f * (k % 2);
		FSlateDrawElement::MakeBox(Sortie, Couche + 1, G.ToPaintGeometry(FVector2D(30.f, Y), FVector2D(T.X, H)),
			Pinceau, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, 0.03f));
	}

	// Vignette en haut et en bas
	TArray<FSlateGradientStop> Haut;
	Haut.Add(FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0.f, 0.f, 0.06f, 0.45f)));
	Haut.Add(FSlateGradientStop(FVector2D(0.f, 140.f), FLinearColor(0.f, 0.f, 0.06f, 0.f)));
	FSlateDrawElement::MakeGradient(Sortie, Couche + 1, G.ToPaintGeometry(FVector2D::ZeroVector, FVector2D(T.X, 140.f)), Haut, Orient_Horizontal);
	TArray<FSlateGradientStop> Bas;
	Bas.Add(FSlateGradientStop(FVector2D(0.f, 0.f), FLinearColor(0.f, 0.f, 0.06f, 0.f)));
	Bas.Add(FSlateGradientStop(FVector2D(0.f, 160.f), FLinearColor(0.f, 0.f, 0.06f, 0.5f)));
	FSlateDrawElement::MakeGradient(Sortie, Couche + 1, G.ToPaintGeometry(FVector2D(0.f, T.Y - 160.f), FVector2D(T.X, 160.f)), Bas, Orient_Horizontal);

	// Aide et numero de version
	const float M = 46.f;
	const FSlateFontInfo Mini = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11);
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(M, T.Y - 40.f), FVector2D(600.f, 20.f)),
		FString(TEXT("\u00C9CHAP / B = RETOUR    A / ENTR\u00C9E = VALIDER    FL\u00C8CHES = CHOISIR")), Mini, ESlateDrawEffect::None, Alpha(Cyan, 0.75f));
	const FSlateFontInfo PoliceVersion = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(T.X - 150.f, T.Y - 42.f), FVector2D(120.f, 24.f)),
		ChellesOptions::Version(), PoliceVersion, ESlateDrawEffect::None, Alpha(Blanc, 0.85f));

	return Couche + 3;
}

// ---------------------------------------------------------------- Bouton

/** Coupe "NOM   < VALEUR >" (ou "NOM   [ TOUCHE ]") en deux morceaux. Valeur vide si la ligne n'a pas de valeur. */
static void CouperLigneMenu(const FString& Texte, FString& Nom, FString& Valeur)
{
	int32 Coupure = Texte.Find(TEXT("   <"));
	if (Coupure == INDEX_NONE)
	{
		Coupure = Texte.Find(TEXT("   ["));
	}
	if (Coupure == INDEX_NONE || Coupure == 0)
	{
		Nom = Texte;
		Valeur.Empty();
		return;
	}
	Nom = Texte.Left(Coupure);
	Valeur = Texte.Mid(Coupure + 3);
}

void SBoutonMenu::Construct(const FArguments& InArgs)
{
	Clic = InArgs._OnClic;
	ClicDroit = InArgs._OnClicDroit;
	bActif = InArgs._Actif;
	bReglage = InArgs._OnClicDroit.IsBound();
	bOnglet = InArgs._Onglet;
	SetCursor(EMouseCursor::Hand);

	const int32 Taille = InArgs._TaillePolice;
	auto CouleurTexte = [this]()
	{
		if (!bActif)
		{
			return FSlateColor(Gris);
		}
		// Plaque blanche sous la souris / onglet choisi : texte bleu nuit dessus, sinon texte blanc
		return FSlateColor((IsHovered() || bSelectionne || bOnglet) ? BleuNuit : Blanc);
	};

	// "NOM   < VALEUR >" ou "NOM   [ TOUCHE ]" : deux colonnes alignees a gauche
	// (le nom dans une colonne de largeur fixe, la valeur commence toujours au meme endroit).
	const TAttribute<FText> Texte = InArgs._Texte;
	FString Nom, Valeur;
	CouperLigneMenu(Texte.Get().ToString(), Nom, Valeur);
	if (!Valeur.IsEmpty())
	{
		ChildSlot
		.Padding(FMargin(Taille * 0.7f, Taille * 0.12f))
		.HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(InArgs._LargeurNom > 0.f ? InArgs._LargeurNom : Taille * 14.f)
				[
					SNew(STextBlock)
					.Text_Lambda([Texte]() { FString N, V; CouperLigneMenu(Texte.Get().ToString(), N, V); return FText::FromString(N); })
					.Font(Police(Taille, Magenta))
					.Justification(ETextJustify::Left)
					.ShadowOffset(FVector2D(0.f, 3.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
					.ColorAndOpacity_Lambda(CouleurTexte)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(Taille * 7.f) // largeur stable : le curseur de volume ne bouge pas quand la valeur change
				[
					SNew(STextBlock)
					.Text_Lambda([Texte]() { FString N, V; CouperLigneMenu(Texte.Get().ToString(), N, V); return FText::FromString(V); })
					.Font(Police(Taille, Magenta))
					.Justification(ETextJustify::Left)
					.ShadowOffset(FVector2D(0.f, 3.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
					.ColorAndOpacity_Lambda(CouleurTexte)
				]
			]
		];
		return;
	}

	ChildSlot
	.Padding(FMargin(Taille * 0.7f, Taille * 0.12f))
	.HAlign(HAlign_Left)
	[
		SNew(STextBlock)
		.Text(Texte)
		.Font(Police(Taille, Magenta))
		.Justification(ETextJustify::Left)
		.ShadowOffset(FVector2D(0.f, 3.f))
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
		.ColorAndOpacity_Lambda(CouleurTexte)
	];
}

FReply SBoutonMenu::OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	if (!bActif)
	{
		return FReply::Handled();
	}
	if (Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Cliquer(1);
		return FReply::Handled();
	}
	if (Evenement.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Cliquer(-1);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SBoutonMenu::OnMouseEnter(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	SCompoundWidget::OnMouseEnter(Geometrie, Evenement);
	if (bActif)
	{
		Son(ESonMenu::Survol);
	}
}

int32 SBoutonMenu::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	// Theme lobby : plaque blanche legerement penchee + barre jaune a gauche (plus de neon)
	const FSlateBrush* Pinceau = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FVector2D T = G.GetLocalSize();
	if ((bActif && (IsHovered() || bSelectionne)) || bOnglet)
	{
		const float Pulse = 0.85f + 0.15f * FMath::Sin(Horloge() * 5.f);
		const FSlateRenderTransform Penche(FShear2D(-0.12f, 0.f));
		const FGeometry GP = G.MakeChild(T, FSlateLayoutTransform(), Penche, FVector2D(0.5f, 0.5f));
		FSlateDrawElement::MakeBox(Sortie, Couche, GP.ToPaintGeometry(), Pinceau, ESlateDrawEffect::None,
			bOnglet && !(IsHovered() || bSelectionne) ? FLinearColor(0.93f, 0.95f, 1.f, 0.95f) : FLinearColor(1.f, 1.f, 1.f, Pulse));
		FSlateDrawElement::MakeBox(Sortie, Couche + 1, GP.ToPaintGeometry(FVector2D::ZeroVector, FVector2D(6.f, T.Y)), Pinceau, ESlateDrawEffect::None, Or);
	}
	else if (bActif)
	{
		// Fond discret pour que chaque bouton se voie comme une carte
		FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(), Pinceau, ESlateDrawEffect::None, FLinearColor(0.02f, 0.06f, 0.25f, 0.45f));
	}
	return SCompoundWidget::OnPaint(Args, G, Clip, Sortie, Couche + 3, Style, bParentEnabled);
}

void SBoutonMenu::Cliquer(int32 Sens)
{
	if (!bActif)
	{
		return;
	}
	Son(bReglage ? ESonMenu::Reglage : ESonMenu::Clic);
	if (Sens < 0 && ClicDroit.IsBound())
	{
		ClicDroit.Execute();
	}
	else
	{
		Clic.ExecuteIfBound();
	}
}

// ---------------------------------------------------------------- Menu

void SMenuChelles::Construct(const FArguments& InArgs)
{
	Menu = InArgs._Menu;
	GMenuSons = Menu;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBackgroundBlur)
			.BlurStrength(6.f)
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.ColorAndOpacity(Fond)
		]
		+ SOverlay::Slot()
		[
			SNew(SDecorCyber)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center) // le bloc du menu reste au milieu de l'ecran ; ses lignes sont alignees a gauche
		.VAlign(VAlign_Center)
		.Padding(FMargin(40.f, 80.f))
		[
			SAssignNew(Zone, SBox)
		]
		// Ecrans plein ecran (HABILLAGE facon lobby) : par-dessus le decor, sous le logo son
		+ SOverlay::Slot()
		[
			SAssignNew(ZonePleinEcran, SBox)
		]
		// Logo son en haut a droite : clic = couper / remettre la musique
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 30.f, 34.f, 0.f))
		[
			SNew(SIconeSon)
			.OnClic(FSimpleDelegate::CreateLambda([this]()
			{
				if (AMenuChelles* M = LeMenu())
				{
					M->BasculerMusique();
				}
			}))
		]
	];
	Afficher(EEcranMenu::Principal);
}

// ---------------------------------------------------------------- Logo son

void SIconeSon::Construct(const FArguments& InArgs)
{
	Clic = InArgs._OnClic;
	SetCursor(EMouseCursor::Hand);
}

FReply SIconeSon::OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	if (Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Son(ESonMenu::Clic);
		Clic.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

int32 SIconeSon::OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const bool bCoupe = ChellesOptions::MusiqueCoupee();
	const float Eclat = IsHovered() ? 1.f : 0.75f;
	const FLinearColor Couleur = Alpha(bCoupe ? Gris : Cyan, Eclat);

	// Cadre a coins coupes, comme les boutons
	Ligne(Sortie, Couche, Geometrie, Chanfrein(Geometrie.GetLocalSize(), 8.f), Alpha(Cyan, IsHovered() ? 0.9f : 0.35f), 1.5f);

	// Haut-parleur
	TArray<FVector2D> Corps = { FVector2D(10.f, 18.f), FVector2D(17.f, 18.f), FVector2D(25.f, 10.f), FVector2D(25.f, 36.f),
		FVector2D(17.f, 28.f), FVector2D(10.f, 28.f), FVector2D(10.f, 18.f) };
	Ligne(Sortie, Couche + 1, Geometrie, Corps, Couleur, 2.f);

	if (bCoupe)
	{
		// Barre magenta
		TArray<FVector2D> Barre = { FVector2D(8.f, 38.f), FVector2D(39.f, 8.f) };
		Ligne(Sortie, Couche + 2, Geometrie, Barre, Magenta, 3.f);
	}
	else
	{
		// Ondes sonores
		for (float Rayon : { 6.f, 11.f })
		{
			TArray<FVector2D> Onde;
			for (int32 i = -3; i <= 3; ++i)
			{
				const float Angle = FMath::DegreesToRadians(i * 15.f);
				Onde.Add(FVector2D(27.f + Rayon * FMath::Cos(Angle), 23.f + Rayon * FMath::Sin(Angle)));
			}
			Ligne(Sortie, Couche + 1, Geometrie, Onde, Couleur, 2.f);
		}
	}
	return Couche + 3;
}

AMenuChelles* SMenuChelles::LeMenu() const
{
	return Menu.Get();
}

void SMenuChelles::Tick(const FGeometry& Geometrie, const double Temps, const float Delta)
{
	SCompoundWidget::Tick(Geometrie, Temps, Delta);
	AMenuChelles* M = LeMenu();
	if (!M)
	{
		return;
	}
	// D'autres scripts du jeu peuvent cacher la souris au demarrage : on la garde visible.
	if (APlayerController* Joueur = M->GetPC())
	{
		Joueur->bShowMouseCursor = true;
	}
	M->EntretenirMusique();
	// ... et remettre les commandes au jeu : on reprend la main deux fois au debut.
	const float Avant = Age;
	Age += Delta;
	if ((Avant < 0.5f && Age >= 0.5f) || (Avant < 1.5f && Age >= 1.5f))
	{
		M->ModeInterface();
	}
}

FReply SMenuChelles::OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche)
{
	const FKey K = Touche.GetKey();
	if (ToucheEnAttente != INDEX_NONE)
	{
		if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right)
		{
			ToucheEnAttente = INDEX_NONE;
			Son(ESonMenu::Retour);
		}
		else if (!K.IsGamepadKey())
		{
			ChellesOptions::DefinirTouche(ToucheEnAttente, K);
			ToucheEnAttente = INDEX_NONE;
			Son(ESonMenu::Clic);
		}
		return FReply::Handled();
	}
	if (Ecran == EEcranMenu::Options && (K == EKeys::Gamepad_LeftShoulder || K == EKeys::Gamepad_RightShoulder || K == EKeys::Tab))
	{
		ChangerOnglet(K == EKeys::Gamepad_LeftShoulder ? -1 : 1);
		return FReply::Handled();
	}
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::Gamepad_Special_Left)
	{
		Retour();
		return FReply::Handled();
	}
	if (Ecran == EEcranMenu::Habillage)
	{
		// Lobby (disposition en colonnes) : haut / bas = choix suivant dans la colonne,
		// gauche / droite = colonne des personnages ou colonne de la tenue
		if (K == EKeys::Up || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up)
		{
			ChangerLobby(-1);
			return FReply::Handled();
		}
		if (K == EKeys::Down || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down)
		{
			ChangerLobby(1);
			return FReply::Handled();
		}
		if (K == EKeys::Left || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftStick_Left)
		{
			RubanLobby = 0;
			return FReply::Handled();
		}
		if (K == EKeys::Right || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_LeftStick_Right)
		{
			RubanLobby = 1;
			return FReply::Handled();
		}
		if (K == EKeys::Enter || K == EKeys::Gamepad_FaceButton_Bottom)
		{
			JouerDepuisLobby();
			return FReply::Handled();
		}
	}
	if (K == EKeys::Up || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up)
	{
		Deplacer(-1);
		return FReply::Handled();
	}
	if (K == EKeys::Down || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down)
	{
		Deplacer(1);
		return FReply::Handled();
	}
	TSharedPtr<SBoutonMenu> Choisi = Boutons.IsValidIndex(Selection) ? Boutons[Selection].Pin() : TSharedPtr<SBoutonMenu>();
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (Choisi.IsValid())
		{
			Choisi->Cliquer(1);
		}
		else
		{
			Deplacer(0);
		}
		return FReply::Handled();
	}
	const bool bGauche = (K == EKeys::Left || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftStick_Left);
	const bool bDroite = (K == EKeys::Right || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_LeftStick_Right);
	if ((bGauche || bDroite) && Choisi.IsValid() && Choisi->EstReglage())
	{
		Choisi->Cliquer(bDroite ? 1 : -1);
		return FReply::Handled();
	}
	if (bGauche || bDroite)
	{
		// Boutons en ligne (disposition inversee) : gauche / droite = bouton precedent / suivant
		Deplacer(bDroite ? 1 : -1);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SMenuChelles::OnPreviewMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	if (ToucheEnAttente != INDEX_NONE)
	{
		ChellesOptions::DefinirTouche(ToucheEnAttente, Evenement.GetEffectingButton());
		ToucheEnAttente = INDEX_NONE;
		Son(ESonMenu::Clic);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMenuChelles::ChangerOnglet(int32 Sens)
{
	const int32 N = ((int32(Onglet) + Sens) % 5 + 5) % 5;
	Onglet = EOngletOptions(N);
	Son(ESonMenu::Reglage);
	Afficher(EEcranMenu::Options);
}

void SMenuChelles::Deplacer(int32 Sens)
{
	bNavigation = true;
	if (Boutons.Num() == 0)
	{
		return;
	}
	if (Boutons.IsValidIndex(Selection))
	{
		if (TSharedPtr<SBoutonMenu> Ancien = Boutons[Selection].Pin())
		{
			Ancien->SetSelectionne(false);
		}
	}
	Selection = Boutons.IsValidIndex(Selection) ? ((Selection + Sens) % Boutons.Num() + Boutons.Num()) % Boutons.Num() : 0;
	if (TSharedPtr<SBoutonMenu> Nouveau = Boutons[Selection].Pin())
	{
		Nouveau->SetSelectionne(true);
	}
	if (Sens != 0)
	{
		Son(ESonMenu::Survol);
	}
}

TSharedRef<SBoutonMenu> SMenuChelles::Enregistrer(TSharedRef<SBoutonMenu> Nouveau)
{
	Boutons.Add(Nouveau);
	return Nouveau;
}

void SMenuChelles::Afficher(EEcranMenu NouvelEcran)
{
	if (NouvelEcran != EEcranMenu::Habillage)
	{
		RetirerApercu(); // le personnage d'apercu n'existe que sur l'ecran HABILLAGE
	}
	const bool bMemeEcran = (Ecran == NouvelEcran);
	const int32 AncienneSelection = Selection;
	Ecran = NouvelEcran;
	ToucheEnAttente = INDEX_NONE;
	Boutons.Reset();
	Selection = INDEX_NONE;
	// HABILLAGE (lobby) occupe tout l'ecran ; les autres ecrans restent dans le bloc du milieu
	const bool bPleinEcran = (Ecran == EEcranMenu::Habillage);
	if (ZonePleinEcran.IsValid())
	{
		ZonePleinEcran->SetContent(bPleinEcran ? Construire(Ecran) : SNullWidget::NullWidget);
	}
	if (Zone.IsValid())
	{
		Zone->SetContent(bPleinEcran ? SNullWidget::NullWidget : Construire(Ecran));
	}
	if (bNavigation || ChellesOptions::Manette())
	{
		// A la manette : un bouton est toujours en surbrillance
		Selection = (bMemeEcran && Boutons.IsValidIndex(AncienneSelection)) ? AncienneSelection - 1 : -1;
		Deplacer(1);
	}
	if (FSlateApplication::IsInitialized())
	{
		if (Ecran == EEcranMenu::AdresseIP && ChampIP.IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(ChampIP, EFocusCause::SetDirectly);
		}
		else if (Ecran == EEcranMenu::CodeVIP && ChampCodeVIP.IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(ChampCodeVIP, EFocusCause::SetDirectly);
		}
		else
		{
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		}
	}
}

void SMenuChelles::Rafraichir()
{
	if (Ecran == EEcranMenu::Serveurs || Ecran == EEcranMenu::Search)
	{
		Afficher(Ecran);
	}
}

void SMenuChelles::Retour()
{
	if (Ecran != EEcranMenu::Principal)
	{
		Son(ESonMenu::Retour);
	}
	switch (Ecran)
	{
	case EEcranMenu::Mode:
	case EEcranMenu::CodeVIP:
		Afficher(EEcranMenu::Habillage);
		break;
	case EEcranMenu::Habillage:
	case EEcranMenu::Options:
	case EEcranMenu::ConfirmerQuitter:
	case EEcranMenu::Credits:
		Afficher(EEcranMenu::Principal);
		break;
	case EEcranMenu::Touches:
		Afficher(EEcranMenu::Options);
		break;
	case EEcranMenu::Multi:
		Afficher(EEcranMenu::Mode);
		break;
	case EEcranMenu::Serveurs:
	case EEcranMenu::AdresseIP:
	case EEcranMenu::Search:
		Afficher(EEcranMenu::Multi);
		break;
	case EEcranMenu::Principal:
		// Menu PAUSE (Echap en jeu) : Echap encore une fois = reprendre la partie
		if (EnPause())
		{
			Son(ESonMenu::Retour);
			if (AMenuChelles* M = LeMenu())
			{
				M->Reprendre();
			}
		}
		break;
	default:
		break;
	}
}

bool SMenuChelles::EnPause() const
{
	const AMenuChelles* M = LeMenu();
	return M && M->EnPause();
}

// ---------------------------------------------------------------- Briques

TSharedRef<SWidget> SMenuChelles::Bouton(const FText& Texte, int32 Taille, TFunction<void()> Action, bool bActif)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, Taille * 0.12f))
		.HAlign(HAlign_Left)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte(Texte)
			.TaillePolice(Taille)
			.Actif(bActif)
			.OnClic(FSimpleDelegate::CreateLambda(MoveTemp(Action))))
		];
}

TSharedRef<SWidget> SMenuChelles::Reglage(TFunction<FText()> Texte, TFunction<void(int32)> Changer)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 4.f))
		.HAlign(HAlign_Left)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte_Lambda(MoveTemp(Texte))
			.TaillePolice(30)
			// Onglet CONTROLES : noms plus longs ("JOYSTICK HAUT/BAS INVERSE"), colonne plus large
			.LargeurNom((Ecran == EEcranMenu::Options && Onglet == EOngletOptions::Controles) ? 30.f * 19.f : 0.f)
			.OnClic(FSimpleDelegate::CreateLambda([Changer]() { Changer(1); }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([Changer]() { Changer(-1); })))
		];
}

TSharedRef<SWidget> SMenuChelles::Titre(const FText& Texte, const FText& SousTitre)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(Texte)
			.Font(Police(72, Cyan))
			.ColorAndOpacity(Blanc)
			.ShadowOffset(FVector2D(4.f, 4.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.1f, 0.6f))
			.RenderTransform(FSlateRenderTransform(FShear2D(-0.18f, 0.f)))
			.RenderTransformPivot(FVector2D(0.5f, 0.5f))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(FMargin(0.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Text(SousTitre)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
			.ColorAndOpacity(Alpha(Cyan, 0.9f))
		];
}

TSharedRef<SWidget> SMenuChelles::Petit(const FText& Texte)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 6.f))
		.HAlign(HAlign_Left)
		.MaxDesiredWidth(900.f)
		[
			SNew(STextBlock)
			.Text(Texte)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16))
			.ColorAndOpacity(Alpha(Blanc, 0.75f))
			.Justification(ETextJustify::Left)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SMenuChelles::TexteEtat()
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 14.f))
		.HAlign(HAlign_Left)
		.MaxDesiredWidth(900.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { AMenuChelles* M = LeMenu(); return M ? M->Etat() : FText::GetEmpty(); })
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
			.ColorAndOpacity(Magenta)
			.Justification(ETextJustify::Left)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SMenuChelles::BoutonRetour()
{
	return Bouton(FText::FromString(TEXT("< RETOUR")), 30, [this]() { Retour(); });
}

// ---------------------------------------------------------------- Ecrans

TSharedRef<SWidget> SMenuChelles::Construire(EEcranMenu E)
{
	switch (E)
	{
	case EEcranMenu::Habillage:     return EcranHabillage();
	case EEcranMenu::Mode:          return EcranMode();
	case EEcranMenu::Multi:         return EcranMulti();
	case EEcranMenu::Serveurs:      return EcranServeurs();
	case EEcranMenu::AdresseIP:     return EcranAdresseIP();
	case EEcranMenu::CodeVIP:       return EcranCodeVIP();
	case EEcranMenu::Search:        return EcranSearch();
	case EEcranMenu::Options:       return EcranOptions();
	case EEcranMenu::Touches:       return EcranTouches();
	case EEcranMenu::ConfirmerQuitter: return EcranQuitter();
	case EEcranMenu::Credits:       return EcranCredits();
	default:                        return EcranPrincipal();
	}
}

TSharedRef<SWidget> SMenuChelles::EcranPrincipal()
{
	// Disposition inversee (24/11) : les grands boutons sont en ligne, les liens de la communaute en colonne
	TSharedRef<SHorizontalBox> Ligne = SNew(SHorizontalBox);
	if (EnPause())
	{
		// Menu ouvert en jeu par Echap : REPRENDRE revient tout de suite a la partie
		Ligne->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Bouton(FText::FromString(TEXT("REPRENDRE")), 40, [this]() { if (AMenuChelles* M = LeMenu()) { M->Reprendre(); } }) ];
	}
	Ligne->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Bouton(FText::FromString(TEXT("JOUER")), 40, [this]() { Afficher(EEcranMenu::Habillage); }) ];
	// SANDBOX : mode bac a sable (plugin ChellesSandBox). Grise si le plugin est absent.
	Ligne->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Bouton(FText::FromString(TEXT("SANDBOX")), 40, [this]() { if (AMenuChelles* M = LeMenu()) { M->SandBox(); } },
		IConsoleManager::Get().FindConsoleObject(TEXT("Chelles.SandBox")) != nullptr) ];
	Ligne->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Bouton(FText::FromString(TEXT("OPTIONS")), 40, [this]() { Afficher(EEcranMenu::Options); }) ];
	Ligne->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Bouton(FText::FromString(TEXT("CR\u00C9DITS")), 26, [this]() { Afficher(EEcranMenu::Credits); }) ];
	Ligne->AddSlot().AutoWidth().VAlign(VAlign_Center)[ Bouton(FText::FromString(TEXT("QUITTER")), 40, [this]() { Afficher(EEcranMenu::ConfirmerQuitter); }) ];

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("CHELLES 3D")), FText::FromString(EnPause() ? TEXT("// PAUSE  -  ECHAP = REPRENDRE") : TEXT("// FPS  -  QUARTIER DE CHELLES"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ Ligne ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f)).HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("DISCORD")), 22, [this]() { OuvrirLien(TEXT("LienDiscord"), TEXT("Discord")); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("FACEBOOK")), 22, [this]() { OuvrirLien(TEXT("LienFacebook"), TEXT("Facebook")); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("YOUTUBE")), 22, [this]() { OuvrirLien(TEXT("LienYoutube"), TEXT("YouTube")); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("TIKTOK")), 22, [this]() { OuvrirLien(TEXT("LienTiktok"), TEXT("TikTok")); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("SITE")), 22, [this]() { OuvrirLien(TEXT("LienSite"), TEXT("le site")); }) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(30.f, 0.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return MessageLien; })
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.ColorAndOpacity(Alpha(Or, 0.9f))
				.Justification(ETextJustify::Left)
			]
		];
}

void SMenuChelles::OuvrirLien(const TCHAR* Cle, const TCHAR* Nom)
{
	// Liens de la communaute : a remplir ici, ou dans GameUserSettings.ini [ChellesMenu] (LienDiscord=, LienFacebook=, LienYoutube=, LienTiktok=, LienSite=)
	FString Url = ChellesOptions::LireTexte(Cle);
	if (Url.IsEmpty())
	{
		if (FCString::Strcmp(Cle, TEXT("LienDiscord")) == 0) { Url = TEXT("https://discord.gg/MtUru8vyg"); }
		else if (FCString::Strcmp(Cle, TEXT("LienFacebook")) == 0) { Url = TEXT("https://www.facebook.com/TIKT0K3D/"); }
		else if (FCString::Strcmp(Cle, TEXT("LienYoutube")) == 0) { Url = TEXT("https://www.youtube.com/@CHELLES3D-77"); }
		else if (FCString::Strcmp(Cle, TEXT("LienTiktok")) == 0) { Url = TEXT("https://www.tiktok.com/@meridja77"); }
		else if (FCString::Strcmp(Cle, TEXT("LienSite")) == 0) { Url = TEXT("http://meridja77.free.fr/"); }
	}
	if (Url.StartsWith(TEXT("http")))
	{
		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		MessageLien = FText::FromString(FString::Printf(TEXT("Ouverture de %s dans le navigateur..."), Nom));
	}
	else
	{
		MessageLien = FText::FromString(FString::Printf(TEXT("Lien %s : bient\u00F4t disponible"), Nom));
	}
}

TSharedRef<SWidget> SMenuChelles::EcranMode()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("MODE")), FText::FromString(TEXT("// CHOISIS TA FA\u00C7ON DE JOUER"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("SOLO")), 64, [this]() { if (AMenuChelles* M = LeMenu()) { M->Solo(); } }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("MULTI-PLAYER")), 64, [this]() { Afficher(EEcranMenu::Multi); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))[ BoutonRetour() ];
}

// ---------------------------------------------------------------- HABILLAGE (sexe, tenue, look)

/** Zone de l'apercu 3D : glisser a la souris pour faire tourner le personnage. */
class SZoneApercuChelles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZoneApercuChelles) : _SansCadre(false) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AApercuHabillage>, Acteur)
		SLATE_ARGUMENT(const FSlateBrush*, Image)
		/** Lobby (2026-11-24) : pas de cadre neon autour du personnage. */
		SLATE_ARGUMENT(bool, SansCadre)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Acteur = InArgs._Acteur;
		bSansCadre = InArgs._SansCadre;
		SetCursor(EMouseCursor::GrabHand);
		ChildSlot
		[
			SNew(SImage).Image(InArgs._Image)
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement) override
	{
		bTire = true;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseButtonUp(const FGeometry& Geometrie, const FPointerEvent& Evenement) override
	{
		bTire = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual FReply OnMouseMove(const FGeometry& Geometrie, const FPointerEvent& Evenement) override
	{
		if (bTire && Acteur.IsValid())
		{
			Acteur->Tourner(Evenement.GetCursorDelta().X * 0.6f);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const int32 Suite = SCompoundWidget::OnPaint(Args, G, Clip, Sortie, Couche, Style, bParentEnabled);
		if (bSansCadre)
		{
			return Suite;
		}
		const FVector2D T = G.GetLocalSize();
		const float Pulse = 0.6f + 0.3f * FMath::Sin(Horloge() * 2.f);
		const TArray<FVector2D> Contour = Chanfrein(T, 26.f);
		Ligne(Sortie, Suite + 1, G, Contour, Alpha(Cyan, 0.2f * Pulse), 8.f);
		Ligne(Sortie, Suite + 2, G, Contour, Alpha(Cyan, Pulse), 2.f);
		TArray<FVector2D> Sol;
		Sol.Add(FVector2D(T.X * 0.2f, T.Y * 0.93f));
		Sol.Add(FVector2D(T.X * 0.8f, T.Y * 0.93f));
		Ligne(Sortie, Suite + 2, G, Sol, Alpha(Magenta, 0.7f), 3.f);
		return Suite + 3;
	}

private:
	TWeakObjectPtr<AApercuHabillage> Acteur;
	bool bTire = false;
	bool bSansCadre = false;
};

SMenuChelles::~SMenuChelles()
{
	RetirerApercu();
}

void SMenuChelles::RetirerApercu()
{
	if (AApercuHabillage* A = ApercuActeur.Get())
	{
		UWorld* Monde = A->GetWorld();
		if (Monde && !Monde->bIsTearingDown && !A->IsPendingKill())
		{
			A->Destroy();
		}
	}
	ApercuActeur.Reset();
	if (PinceauApercu.IsValid())
	{
		PinceauApercu->SetResourceObject(nullptr);
	}
}

void SMenuChelles::MontrerApercu()
{
	AMenuChelles* M = LeMenu();
	UWorld* Monde = M ? M->GetWorld() : nullptr;
	if (!Monde)
	{
		return;
	}
	if (!ApercuActeur.IsValid())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Tres haut au-dessus de la carte : rien d'autre dans l'image
		ApercuActeur = Monde->SpawnActor<AApercuHabillage>(AApercuHabillage::StaticClass(), FTransform(FVector(0.f, 0.f, 150000.f)), Params);
	}
	AApercuHabillage* A = ApercuActeur.Get();
	if (!A)
	{
		return;
	}
	if (!PinceauApercu.IsValid())
	{
		PinceauApercu = MakeShared<FSlateBrush>();
		PinceauApercu->DrawAs = ESlateBrushDrawType::Image;
		PinceauApercu->ImageSize = FVector2D(512.f, 768.f);
	}
	PinceauApercu->SetResourceObject(A->Image());

	// Spider-Man : on montre le corps du joueur tel qu'il est dans la carte
	USkeletalMesh* MeshSpider = nullptr;
	UClass* AnimSpider = nullptr;
	APlayerController* PC = M->GetPC();
	if (ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr)
	{
		if (Perso->GetMesh())
		{
			MeshSpider = Perso->GetMesh()->SkeletalMesh;
			AnimSpider = Perso->GetMesh()->GetAnimClass();
		}
	}
	A->Montrer(TenueChoisie, MeshSpider, AnimSpider);
}

void SMenuChelles::ModifierTenue(TFunction<void(ChellesHabillage::FTenue&)> Changement)
{
	const int32 SexeAvant = TenueChoisie.Sexe;
	Changement(TenueChoisie);
	// LENA, SOFIA... : c'est le plugin ChellesWraith qui la pose sur le soldat (pas de lien direct entre plugins : on passe par GameUserSettings)
	const TCHAR* PersoAvant = ChellesHabillage::NomPersoWraith(SexeAvant);
	const TCHAR* PersoApres = ChellesHabillage::NomPersoWraith(TenueChoisie.Sexe);
	if (PersoAvant != PersoApres && GConfig)
	{
		GConfig->SetString(TEXT("ChellesWraith"), TEXT("Demande"), PersoApres ? PersoApres : TEXT("SOLDAT D'ORIGINE"), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	ChellesHabillage::Completer(TenueChoisie);
	ChellesHabillage::EnregistrerTenue(TenueChoisie);
	MontrerApercu();
	Afficher(EEcranMenu::Habillage);
}

TSharedRef<SWidget> SMenuChelles::ReglageHabillage(const FString& NomReglage, const FString& ValeurReglage, TFunction<void(int32)> Modifier)
{
	const FText Libelle = FText::FromString(FString::Printf(TEXT("%s   < %s >"), *NomReglage, *ValeurReglage));
	return SNew(SBox)
		.Padding(FMargin(0.f, 1.f))
		.HAlign(HAlign_Left)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte(Libelle)
			.TaillePolice(22)
			.OnClic(FSimpleDelegate::CreateLambda([Modifier]() { Modifier(1); }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([Modifier]() { Modifier(-1); })))
		];
}

// ---------------------------------------------------------------- HABILLAGE facon LOBBY (2026-11-24)
//
// Demande MERIDJA : ecran HABILLAGE refait comme un lobby de jeu de tir moderne :
//  - barre d'onglets en haut (JOUER, CASIER, ARMEMENTS, OPTIONS) ;
//  - le personnage en grand au centre, sur un decor bleu (on peut le faire tourner a la souris) ;
//  - son nom en tres gros a gauche ;
//  - en bas, les choix defilent en RUBAN HORIZONTAL de cartes : les personnages, puis (FEMININ / MASCULIN)
//    les morceaux de tenue (MODELE, HAUT, BAS, CHAUSSURES...).
// Le catalogue et l'enregistrement de la tenue ne changent pas (HabillageChelles).

namespace LobbyChelles
{
	const FLinearColor LobNuit(0.010f, 0.025f, 0.110f, 1.f);
	const FLinearColor LobBleu(0.030f, 0.130f, 0.380f, 1.f);
	const FLinearColor LobCyan(0.250f, 0.850f, 1.000f, 1.f);
	const FLinearColor LobSombre(0.020f, 0.030f, 0.140f, 1.f);
	const FLinearColor LobJaune(1.000f, 0.880f, 0.050f, 1.f);

	FSlateFontInfo LobPolice(int32 Taille, const TCHAR* Graisse = TEXT("Black"))
	{
		return FCoreStyle::GetDefaultFontStyle(Graisse, Taille);
	}

	FSlateRenderTransform LobPenche(float Force = 0.18f)
	{
		return FSlateRenderTransform(FShear2D(-Force, 0.f));
	}

	FLinearColor LobFois(const FLinearColor& C, float F)
	{
		return FLinearColor(FMath::Min(1.f, C.R * F), FMath::Min(1.f, C.G * F), FMath::Min(1.f, C.B * F), C.A);
	}

	/** Triangles colores (degrades, disques) : un sommet = une couleur. */
	void LobSommets(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const TArray<FLinearColor>& Couleurs, const TArray<SlateIndex>& Indices)
	{
		if (!FSlateApplication::IsInitialized() || Points.Num() < 3 || Couleurs.Num() == 0)
		{
			return;
		}
		const FSlateBrush* Pinceau = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FSlateResourceHandle Ressource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Pinceau);
		TArray<FSlateVertex> Liste;
		Liste.Reserve(Points.Num());
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			FSlateVertex V;
			FMemory::Memzero(&V, sizeof(FSlateVertex));
			V.TexCoords[2] = 1.f;
			V.TexCoords[3] = 1.f;
			V.Position = G.LocalToAbsolute(Points[i]);
			V.Color = Couleurs[FMath::Min(i, Couleurs.Num() - 1)].ToFColor(true);
			Liste.Add(V);
		}
		TArray<SlateIndex> Copie = Indices;
		FSlateDrawElement::MakeCustomVerts(Sortie, Couche, Ressource, Liste, Copie, nullptr, 0, 0);
	}

	void LobDegrade(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille,
		const FLinearColor& Haut, const FLinearColor& Bas, bool bHorizontal = false)
	{
		const TArray<FLinearColor> C = bHorizontal ? TArray<FLinearColor>{ Haut, Bas, Bas, Haut } : TArray<FLinearColor>{ Haut, Haut, Bas, Bas };
		LobSommets(Sortie, Couche, G, { Pos, Pos + FVector2D(Taille.X, 0.f), Pos + Taille, Pos + FVector2D(0.f, Taille.Y) }, C, { 0, 1, 2, 0, 2, 3 });
	}

	/** Halo ovale (centre colore, bord transparent). */
	void LobHalo(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre, const FVector2D& Rayon, const FLinearColor& C)
	{
		TArray<FVector2D> P;
		TArray<FLinearColor> Couleurs;
		TArray<SlateIndex> I;
		P.Add(Centre);
		Couleurs.Add(C);
		const int32 N = 40;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = k * 2.f * PI / N;
			P.Add(Centre + FVector2D(FMath::Cos(A) * Rayon.X, FMath::Sin(A) * Rayon.Y));
			Couleurs.Add(FLinearColor(C.R, C.G, C.B, 0.f));
		}
		for (int32 k = 0; k < N; ++k)
		{
			I.Add(0);
			I.Add(SlateIndex(1 + k));
			I.Add(SlateIndex(1 + (k + 1) % N));
		}
		LobSommets(Sortie, Couche, G, P, Couleurs, I);
	}

	void LobRect(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& C)
	{
		FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, C);
	}

	void LobCadre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& T, float E, const FLinearColor& C)
	{
		LobRect(Sortie, Couche, G, Pos, FVector2D(T.X, E), C);
		LobRect(Sortie, Couche, G, Pos + FVector2D(0.f, T.Y - E), FVector2D(T.X, E), C);
		LobRect(Sortie, Couche, G, Pos + FVector2D(0.f, E), FVector2D(E, T.Y - 2.f * E), C);
		LobRect(Sortie, Couche, G, Pos + FVector2D(T.X - E, E), FVector2D(E, T.Y - 2.f * E), C);
	}

	FVector2D LobMesure(const FString& Texte, const FSlateFontInfo& P)
	{
		return FSlateApplication::IsInitialized()
			? FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Texte, P)
			: FVector2D(10.f, 10.f);
	}

	void LobTexte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FString& Texte,
		const FSlateFontInfo& P, const FLinearColor& C, bool bOmbre = true)
	{
		const FVector2D T = LobMesure(Texte, P);
		if (bOmbre)
		{
			FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Pos + FVector2D(2.f, 2.f), T), Texte, P, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.05f, 0.7f * C.A));
		}
		FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Pos, T), Texte, P, ESlateDrawEffect::None, C);
	}
}

/** Fond du lobby : degrade bleu nuit, rayons de lumiere, halo au sol au centre. */
class SFondLobbyChelles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFondLobbyChelles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		using namespace LobbyChelles;
		const FVector2D L = G.GetLocalSize();
		// Degrade de gauche a droite (disposition en colonnes)
		LobDegrade(Sortie, Couche, G, FVector2D::ZeroVector, FVector2D(L.X * 0.5f, L.Y), LobNuit, LobBleu, true);
		LobDegrade(Sortie, Couche, G, FVector2D(L.X * 0.5f, 0.f), FVector2D(L.X * 0.5f, L.Y), LobBleu, FLinearColor(0.005f, 0.015f, 0.07f, 1.f), true);
		// bandes de lumiere horizontales qui partent du bord droit (comme un projecteur)
		const float T = Horloge();
		for (int32 k = 0; k < 3; ++k)
		{
			const float Y = L.Y * (0.22f + 0.2f * k) + 10.f * FMath::Sin(T * 0.4f + k);
			const float Haut = L.Y * 0.05f;
			const FLinearColor C(LobCyan.R, LobCyan.G, LobCyan.B, 0.06f);
			const FLinearColor Rien(LobCyan.R, LobCyan.G, LobCyan.B, 0.f);
			LobSommets(Sortie, Couche + 1, G,
				{ FVector2D(L.X, Y), FVector2D(L.X, Y + Haut), FVector2D(0.f, Y + Haut + L.X * 0.15f), FVector2D(0.f, Y + L.X * 0.15f) },
				{ C, C, Rien, Rien }, { 0, 1, 2, 0, 2, 3 });
		}
		// halo bleu derriere le personnage, halo cyan au sol
		LobHalo(Sortie, Couche + 2, G, FVector2D(L.X * 0.5f, L.Y * 0.42f), FVector2D(L.X * 0.28f, L.Y * 0.42f), FLinearColor(0.10f, 0.35f, 0.85f, 0.35f));
		LobHalo(Sortie, Couche + 2, G, FVector2D(L.X * 0.5f, L.Y * 0.79f), FVector2D(L.X * 0.18f, L.Y * 0.05f), FLinearColor(0.25f, 0.85f, 1.f, 0.35f));
		return Couche + 3;
	}
};

/** Bords fondus de l'apercu du personnage : le rectangle de l'image se perd dans le decor. */
class SBordsLobbyChelles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBordsLobbyChelles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetVisibility(EVisibility::HitTestInvisible); // laisse passer la souris (tourner le personnage)
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		using namespace LobbyChelles;
		const FVector2D L = G.GetLocalSize();
		const float Bord = L.X * 0.16f;
		const FLinearColor Plein(0.02f, 0.08f, 0.25f, 1.f);
		const FLinearColor Vide(0.02f, 0.08f, 0.25f, 0.f);
		LobDegrade(Sortie, Couche, G, FVector2D::ZeroVector, FVector2D(Bord, L.Y), Plein, Vide, true);
		LobDegrade(Sortie, Couche, G, FVector2D(L.X - Bord, 0.f), FVector2D(Bord, L.Y), Vide, Plein, true);
		LobDegrade(Sortie, Couche, G, FVector2D::ZeroVector, FVector2D(L.X, L.Y * 0.12f), FLinearColor(0.02f, 0.05f, 0.18f, 1.f), FLinearColor(0.02f, 0.05f, 0.18f, 0.f));
		LobDegrade(Sortie, Couche, G, FVector2D(0.f, L.Y * 0.82f), FVector2D(L.X, L.Y * 0.18f), FLinearColor(0.01f, 0.03f, 0.12f, 0.f), FLinearColor(0.01f, 0.03f, 0.12f, 1.f));
		return Couche + 1;
	}
};

/** Carte du ruban (personnage ou morceau de tenue) ou puce d'onglet, dessinee sans asset. */
class SCarteLobbyChelles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCarteLobbyChelles)
		: _Couleur(FLinearColor(0.1f, 0.3f, 0.8f))
		, _Taille(FVector2D(200.f, 116.f))
		, _Choisie(false)
		, _Puce(false)
		, _Point(false)
		, _TaillePolice(26)
		{}
		SLATE_ARGUMENT(FString, Titre)
		SLATE_ARGUMENT(FString, SousTitre)
		SLATE_ARGUMENT(FLinearColor, Couleur)
		SLATE_ARGUMENT(FVector2D, Taille)
		SLATE_ARGUMENT(bool, Choisie)
		/** Puce d'onglet (texte seul, fond blanc si choisie) au lieu d'une carte. */
		SLATE_ARGUMENT(bool, Puce)
		/** Petit point jaune devant le texte d'une puce (nouveaute). */
		SLATE_ARGUMENT(bool, Point)
		SLATE_ARGUMENT(int32, TaillePolice)
		SLATE_EVENT(FSimpleDelegate, OnClic)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Titre = InArgs._Titre;
		SousTitre = InArgs._SousTitre;
		Couleur = InArgs._Couleur;
		Taille = InArgs._Taille;
		bChoisie = InArgs._Choisie;
		bPuce = InArgs._Puce;
		bPoint = InArgs._Point;
		TaillePolice = InArgs._TaillePolice;
		Clic = InArgs._OnClic;
		SetCursor(EMouseCursor::Hand);
		SetClipping(EWidgetClipping::ClipToBounds);
		if (bPuce)
		{
			// la puce prend la largeur de son texte
			const FVector2D T = LobbyChelles::LobMesure(Titre, LobbyChelles::LobPolice(TaillePolice));
			Taille = FVector2D(T.X + 34.f + (bPoint ? 16.f : 0.f), FMath::Max(Taille.Y, T.Y + 14.f));
		}
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return Taille; }

	virtual void OnMouseEnter(const FGeometry& Geometrie, const FPointerEvent& Evenement) override
	{
		SLeafWidget::OnMouseEnter(Geometrie, Evenement);
		Son(ESonMenu::Survol);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement) override
	{
		if (Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			Son(ESonMenu::Clic);
			Clic.ExecuteIfBound();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		using namespace LobbyChelles;
		const FVector2D L = G.GetLocalSize();
		const bool bSurvol = IsHovered();
		const FSlateFontInfo P = LobPolice(TaillePolice);
		if (bPuce)
		{
			if (bChoisie)
			{
				LobRect(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(0.93f, 0.95f, 1.f));
			}
			else if (bSurvol)
			{
				LobRect(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(1.f, 1.f, 1.f, 0.14f));
			}
			const FVector2D T = LobMesure(Titre, P);
			float X = 17.f;
			if (bPoint && !bChoisie)
			{
				LobHalo(Sortie, Couche + 1, G, FVector2D(X + 4.f, L.Y * 0.5f), FVector2D(5.f, 5.f), LobJaune);
				LobRect(Sortie, Couche + 1, G, FVector2D(X + 1.f, L.Y * 0.5f - 3.f), FVector2D(6.f, 6.f), LobJaune);
				X += 16.f;
			}
			LobTexte(Sortie, Couche + 2, G, FVector2D(X, (L.Y - T.Y) * 0.5f), Titre, P, bChoisie ? LobSombre : FLinearColor(0.85f, 0.9f, 1.f), !bChoisie);
			return Couche + 4;
		}

		// Carte : cadre, degrade de sa couleur, reflet, titre en bas a gauche
		const float M = 3.f;
		LobRect(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(0.02f, 0.04f, 0.16f, 0.9f));
		LobDegrade(Sortie, Couche + 1, G, FVector2D(M, M), L - FVector2D(2.f * M, 2.f * M),
			LobFois(Couleur, bSurvol || bChoisie ? 1.5f : 1.2f), LobFois(Couleur, bSurvol || bChoisie ? 0.7f : 0.45f));
		LobSommets(Sortie, Couche + 2, G,
			{ FVector2D(M, M), FVector2D(L.X * 0.6f, M), FVector2D(M, L.Y * 0.7f) },
			{ FLinearColor(1.f, 1.f, 1.f, 0.18f), FLinearColor(1.f, 1.f, 1.f, 0.f), FLinearColor(1.f, 1.f, 1.f, 0.f) }, { 0, 1, 2 });
		// Initiale en grand, en filigrane a droite
		if (!Titre.IsEmpty())
		{
			const FString Initiale = Titre.Left(1);
			const FSlateFontInfo PoliceInitiale = LobPolice(FMath::RoundToInt(L.Y * 0.62f));
			const FVector2D TI = LobMesure(Initiale, PoliceInitiale);
			LobTexte(Sortie, Couche + 3, G, FVector2D(L.X - TI.X - 8.f, (L.Y - TI.Y) * 0.35f), Initiale, PoliceInitiale, FLinearColor(1.f, 1.f, 1.f, 0.16f), false);
		}
		// Titre : la police retrecit si le nom est long
		// (calcule une seule fois par largeur de carte, pas a chaque image)
		if (LargeurCalculee != L.X)
		{
			LargeurCalculee = L.X;
			TailleTitre = TaillePolice;
			while (TailleTitre > 11 && LobMesure(Titre, LobPolice(TailleTitre)).X > L.X - 20.f)
			{
				--TailleTitre;
			}
		}
		const FSlateFontInfo PT = LobPolice(TailleTitre);
		const FVector2D TT = LobMesure(Titre, PT);
		LobTexte(Sortie, Couche + 4, G, FVector2D(10.f, L.Y - TT.Y - 8.f), Titre, PT, FLinearColor::White);
		if (!SousTitre.IsEmpty())
		{
			const FSlateFontInfo PS = LobPolice(FMath::Max(10, TaillePolice / 2), TEXT("Bold"));
			const FVector2D TS = LobMesure(SousTitre, PS);
			LobTexte(Sortie, Couche + 4, G, FVector2D(10.f, L.Y - TT.Y - TS.Y - 8.f), SousTitre, PS, FLinearColor(0.85f, 0.92f, 1.f, 0.9f));
		}
		if (bChoisie)
		{
			const float Pulse = 0.75f + 0.25f * FMath::Sin(Horloge() * 4.f);
			LobCadre(Sortie, Couche + 6, G, FVector2D::ZeroVector, L, 4.f, FLinearColor(0.85f, 0.97f, 1.f, Pulse));
			LobCadre(Sortie, Couche + 6, G, FVector2D(4.f, 4.f), L - FVector2D(8.f, 8.f), 2.f, FLinearColor(LobCyan.R, LobCyan.G, LobCyan.B, 0.8f));
		}
		else if (bSurvol)
		{
			LobCadre(Sortie, Couche + 6, G, FVector2D::ZeroVector, L, 2.f, FLinearColor(1.f, 1.f, 1.f, 0.75f));
		}
		return Couche + 7;
	}

private:
	FString Titre;
	FString SousTitre;
	FLinearColor Couleur;
	FVector2D Taille;
	bool bChoisie = false;
	bool bPuce = false;
	bool bPoint = false;
	int32 TaillePolice = 26;
	FSimpleDelegate Clic;
	/** Taille de police du titre deja calculee pour cette largeur (evite de mesurer le texte a chaque image). */
	mutable float LargeurCalculee = -1.f;
	mutable int32 TailleTitre = 26;
};

namespace LobbyChelles
{
	/** Les personnages du premier ruban, dans l'ordre. */
	struct FPersoLobby
	{
		int32 Sexe;
		FString Nom;
		FString SousTitre;
		FLinearColor Couleur;
	};

	TArray<FPersoLobby> PersosLobby()
	{
		TArray<FPersoLobby> Liste;
		Liste.Add({ int32(ChellesHabillage::SpiderMan), TEXT("SPIDER-MALE"), TEXT("HÉROS"), FLinearColor(0.80f, 0.06f, 0.10f) });
		Liste.Add({ int32(ChellesHabillage::Pantera), TEXT("PANTERA"), TEXT("HÉROS"), FLinearColor(0.22f, 0.10f, 0.42f) });
		Liste.Add({ int32(ChellesHabillage::Feminin), TEXT("FÉMININ"), TEXT("CRÉE TON LOOK"), FLinearColor(0.85f, 0.22f, 0.55f) });
		Liste.Add({ int32(ChellesHabillage::Masculin), TEXT("MASCULIN"), TEXT("CRÉE TON LOOK"), FLinearColor(0.12f, 0.38f, 0.90f) });
		const FLinearColor Couleurs[] = { FLinearColor(0.05f, 0.65f, 0.80f), FLinearColor(0.90f, 0.45f, 0.05f), FLinearColor(0.20f, 0.65f, 0.15f), FLinearColor(0.75f, 0.60f, 0.05f) };
		int32 k = 0;
		for (const int32 Perso : ChellesHabillage::PersosWraith())
		{
			if (ChellesHabillage::EstPersoVIP(Perso))
			{
				// 25/11/2026 : personnages VIP (Plugins\ChellesMenu\VIP.txt), carte doree ; "CODE" tant que le code n'a pas ete tape
				Liste.Add({ Perso, ChellesHabillage::NomPersoWraith(Perso), ChellesHabillage::PersoDebloque(Perso) ? TEXT("VIP") : TEXT("VIP  -  CODE"), FLinearColor(0.85f, 0.62f, 0.08f) });
				continue;
			}
			if (ChellesHabillage::EstPersoCree(Perso))
			{
				Liste.Add({ Perso, ChellesHabillage::NomPersoWraith(Perso), TEXT("PERSONNAGE"), Couleurs[k++ % UE_ARRAY_COUNT(Couleurs)] });
			}
			else
			{
				// Heros de la touche K : carte de la couleur du heros (un peu eclaircie pour rester lisible)
				FLinearColor C = ChellesHabillage::CouleurPersoWraith(Perso);
				C = FLinearColor::LerpUsingHSV(C, FLinearColor(0.10f, 0.12f, 0.20f), C.GetLuminance() < 0.04f ? 0.6f : 0.f);
				C.R = FMath::Max(C.R, 0.06f); C.G = FMath::Max(C.G, 0.06f); C.B = FMath::Max(C.B, 0.08f);
				Liste.Add({ Perso, ChellesHabillage::NomPersoWraith(Perso), TEXT("HÉROS"), C });
			}
		}
		return Liste;
	}

	/** 24/11/2026 : les anciens modes des touches F1-F12, choisis maintenant dans le lobby. */
	struct FModeLobby
	{
		const TCHAR* Mode;      // nom du mode dans la roue (ChellesRoue)
		const TCHAR* Nom;       // nom affiche
		const TCHAR* SousTitre;
		FLinearColor Couleur;
		const TCHAR* Description;
	};

	const TArray<FModeLobby>& ModesLobby()
	{
		static const TArray<FModeLobby> Liste =
		{
			{ TEXT("Spider-Femelle"), TEXT("SPIDER-FEMELLE"), TEXT("HÉROS"), FLinearColor(0.85f, 0.10f, 0.35f), TEXT("L'héroïne araignée : elle vole et tire des mains comme Spider-Male.") },
			{ TEXT("FLASH ECLAIR"), TEXT("FLASH ECLAIR"), TEXT("HÉROS"), FLinearColor(0.90f, 0.75f, 0.05f), TEXT("Le héros de l'éclair : rapide et électrique.") },
			{ TEXT("FLAMME FIRE"), TEXT("FLAMME FIRE"), TEXT("HÉROS"), FLinearColor(0.95f, 0.30f, 0.03f), TEXT("Le héros de feu.") },
			{ TEXT("SuperPowers"), TEXT("SUPERPOWERS"), TEXT("HÉROS"), FLinearColor(0.35f, 0.15f, 0.85f), TEXT("Le héros aux super-pouvoirs du pack SuperPowers.") },
			{ TEXT("DYNAMIC COMBAT"), TEXT("DYNAMIC COMBAT"), TEXT("COMBATTANT"), FLinearColor(0.55f, 0.08f, 0.08f), TEXT("Combat au corps à corps : coups de poing, de pied et combos.") },
			{ TEXT("Motoneige"), TEXT("MOTONEIGE"), TEXT("VÉHICULE"), FLinearColor(0.15f, 0.55f, 0.85f), TEXT("Tu pars en motoneige dans la neige de Chelles.") },
			{ TEXT("VOITURE"), TEXT("VOITURE"), TEXT("VÉHICULE"), FLinearColor(0.20f, 0.60f, 0.25f), TEXT("Tu pars en voiture (H = klaxon).") },
			{ TEXT("HORSE"), TEXT("CHEVAL"), TEXT("MONTURE"), FLinearColor(0.50f, 0.30f, 0.12f), TEXT("Tu pars à cheval.") },
		};
		return Liste;
	}

	const FModeLobby* TrouverModeLobby(const FString& Mode)
	{
		for (const FModeLobby& M : ModesLobby())
		{
			if (Mode.Equals(M.Mode, ESearchCase::IgnoreCase))
			{
				return &M;
			}
		}
		return nullptr;
	}

	FString LobNormaliser(const FString& Brut)
	{
		FString R;
		for (TCHAR C : Brut.ToUpper())
		{
			switch (C)
			{
			case 0x00C9: case 0x00C8: case 0x00CA: case 0x00CB: case 0x00E9: case 0x00E8: case 0x00EA: case 0x00EB: C = TEXT('E'); break;
			case 0x00C0: case 0x00C2: case 0x00C4: case 0x00E0: case 0x00E2: case 0x00E4: C = TEXT('A'); break;
			case 0x00D4: case 0x00D6: case 0x00F4: case 0x00F6: C = TEXT('O'); break;
			case 0x00CE: case 0x00CF: case 0x00EE: case 0x00EF: C = TEXT('I'); break;
			case 0x00D9: case 0x00DB: case 0x00DC: case 0x00F9: case 0x00FB: case 0x00FC: C = TEXT('U'); break;
			case 0x00C7: case 0x00E7: C = TEXT('C'); break;
			default: break;
			}
			if (FChar::IsAlnum(C))
			{
				R.AppendChar(C);
			}
		}
		return R;
	}

	/** "Pouvoir : GEL (P)  ·  Fonction : SUPER SAUT  ·  tout le monde vole" lu dans Plugins/ChellesPouvoirs/Personnages.txt. */
	FString PouvoirDuPersoLobby(const FString& Nom)
	{
		static TMap<FString, FString> Cache;
		static bool bLu = false;
		if (!bLu)
		{
			bLu = true;
			TArray<FString> Lignes;
			FFileHelper::LoadFileToStringArray(Lignes, *FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesPouvoirs"), TEXT("Personnages.txt")));
			for (const FString& Brute : Lignes)
			{
				const FString Ligne = Brute.TrimStartAndEnd();
				FString Cle, Reste, Pouvoir, Fonction;
				if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Reste))
				{
					continue;
				}
				if (!Reste.Split(TEXT("|"), &Pouvoir, &Fonction))
				{
					Pouvoir = Reste;
				}
				Pouvoir.TrimStartAndEndInline();
				Fonction.TrimStartAndEndInline();
				const bool bSansPouvoir = Pouvoir.IsEmpty() || LobNormaliser(Pouvoir) == TEXT("AUCUN");
				FString Texte = bSansPouvoir ? FString(TEXT("Pas de pouvoir spécial")) : FString::Printf(TEXT("Pouvoir : %s (P)"), *Pouvoir.ToUpper());
				if (!Fonction.IsEmpty() && LobNormaliser(Fonction) != TEXT("AUCUNE"))
				{
					Texte += FString::Printf(TEXT("   ·   Fonction : %s"), *Fonction.ToUpper());
				}
				Texte += TEXT("   ·   tout le monde vole (ESPACE en l'air)");
				Cache.Add(LobNormaliser(Cle), Texte);
			}
		}
		if (const FString* Trouve = Cache.Find(LobNormaliser(Nom)))
		{
			return *Trouve;
		}
		const FString* Defaut = Cache.Find(TEXT("DEFAUT"));
		return Defaut ? *Defaut : FString();
	}

	/** Couleur d'une carte de tenue : une teinte par emplacement, un peu differente pour chaque choix. */
	FLinearColor CouleurChoix(int32 Emplacement, int32 Index, bool bAucun)
	{
		if (bAucun)
		{
			return FLinearColor(0.25f, 0.28f, 0.36f);
		}
		const float Teinte = FMath::Fmod(200.f + Emplacement * 47.f + (Index % 5) * 7.f, 360.f);
		return FLinearColor(Teinte, 0.75f, 0.85f).HSVToLinearRGB();
	}
}

int32 SMenuChelles::SexeLobby() const
{
	return (TenueChoisie.EstSpiderMan() && !TenueChoisie.EstPantera() && !TenueChoisie.EstPersoWraith())
		? int32(ChellesHabillage::SpiderMan) : TenueChoisie.Sexe;
}

void SMenuChelles::ChoisirSexeLobby(int32 Sexe)
{
	if (!ChellesHabillage::PersoDebloque(Sexe))
	{
		// Personnage VIP (Mr.SHEMS) : on demande son code avant
		PersoVIPDemande = Sexe;
		TexteCodeVIP.Reset();
		MessageCodeVIP.Reset();
		Afficher(EEcranMenu::CodeVIP);
		return;
	}
	ModifierTenue([Sexe](ChellesHabillage::FTenue& T)
	{
		if (T.Sexe != Sexe)
		{
			T.Sexe = Sexe;
			T.Modele.Reset();
		}
	});
}

FString SMenuChelles::NomPersoLobby() const
{
	if (const LobbyChelles::FModeLobby* M = LobbyChelles::TrouverModeLobby(ModeLobby))
	{
		return M->Nom;
	}
	const int32 Sexe = SexeLobby();
	if (TenueChoisie.EstPersoWraith())
	{
		const TCHAR* Nom = ChellesHabillage::NomPersoWraith(TenueChoisie.Sexe);
		return Nom ? FString(Nom) : FString(TEXT("SOLDAT"));
	}
	if (TenueChoisie.EstPantera())
	{
		return TEXT("PANTERA");
	}
	if (Sexe == int32(ChellesHabillage::Feminin))
	{
		return TEXT("FÉMININ");
	}
	if (Sexe == int32(ChellesHabillage::Masculin))
	{
		return TEXT("MASCULIN");
	}
	return TEXT("SPIDER-MALE");
}

void SMenuChelles::EcrireChoixLobby()
{
	if (!GConfig)
	{
		return;
	}
	const FString Mode = LobbyChelles::TrouverModeLobby(ModeLobby) ? ModeLobby : FString(TEXT("Soldat"));
	GConfig->SetString(TEXT("ChellesLobby"), TEXT("ModeDepart"), *Mode, GGameUserSettingsIni);
	// Lu par ChellesPouvoirs : pouvoir + fonction du personnage (Personnages.txt)
	GConfig->SetString(TEXT("ChellesPerso"), TEXT("Nom"), *NomPersoLobby(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void SMenuChelles::ChangerLobby(int32 Sens)
{
	// Fleches haut / bas : choix suivant dans la colonne active (gauche = personnages puis heros/vehicules, droite = tenue)
	if (RubanLobby == 0)
	{
		const TArray<LobbyChelles::FPersoLobby> Persos = LobbyChelles::PersosLobby();
		const TArray<LobbyChelles::FModeLobby>& Modes = LobbyChelles::ModesLobby();
		const int32 Total = Persos.Num() + Modes.Num();
		int32 i = Modes.IndexOfByPredicate([this](const LobbyChelles::FModeLobby& M) { return ModeLobby.Equals(M.Mode, ESearchCase::IgnoreCase); });
		i = (i >= 0) ? Persos.Num() + i : Persos.IndexOfByPredicate([this](const LobbyChelles::FPersoLobby& P) { return P.Sexe == SexeLobby(); });
		i = ((FMath::Max(i, 0) + Sens) % Total + Total) % Total;
		// Fleches : on passe la carte VIP tant qu'elle n'est pas debloquee (un clic dessus demande le code)
		for (int32 n = 0; n < Total && i < Persos.Num() && !ChellesHabillage::PersoDebloque(Persos[i].Sexe); ++n)
		{
			i = ((i + Sens) % Total + Total) % Total;
		}
		if (i >= Persos.Num())
		{
			ModeLobby = Modes[i - Persos.Num()].Mode;
			Afficher(EEcranMenu::Habillage);
		}
		else
		{
			ModeLobby = TEXT("Soldat");
			ChoisirSexeLobby(Persos[i].Sexe);
		}
		return;
	}
	const ChellesHabillage::FModele* Modele = ChellesHabillage::TrouverModele(TenueChoisie.Modele);
	if (!Modele || TenueChoisie.EstSpiderMan())
	{
		return;
	}
	if (EmplacementLobby <= 0 || !Modele->Emplacements.IsValidIndex(EmplacementLobby - 1))
	{
		ModifierTenue([Sens](ChellesHabillage::FTenue& T)
		{
			const TArray<const ChellesHabillage::FModele*> Liste = ChellesHabillage::ModelesDuSexe(T.Sexe);
			if (Liste.Num() == 0)
			{
				return;
			}
			int32 i = Liste.IndexOfByPredicate([&T](const ChellesHabillage::FModele* X) { return X->Id == T.Modele; });
			i = ((FMath::Max(i, 0) + Sens) % Liste.Num() + Liste.Num()) % Liste.Num();
			T.Modele = Liste[i]->Id;
		});
		return;
	}
	const ChellesHabillage::FEmplacement* PE = &Modele->Emplacements[EmplacementLobby - 1];
	ModifierTenue([PE, Sens](ChellesHabillage::FTenue& T)
	{
		const FString* Ici = T.Choix.Find(PE->Cle);
		const int32 N = PE->Lignes.Num();
		if (N == 0)
		{
			return;
		}
		const int32 i = ((FMath::Max(0, PE->IndexDe(Ici ? *Ici : FString())) + Sens) % N + N) % N;
		T.Choix.Add(PE->Cle, PE->Lignes[i].IsNone() ? FString() : PE->Lignes[i].ToString());
	});
}

void SMenuChelles::JouerDepuisLobby()
{
	ChellesHabillage::EnregistrerTenue(TenueChoisie);
	EcrireChoixLobby();
	if (EnPause())
	{
		// En jeu (Echap) : la tenue est appliquee, on passe sur le perso / vehicule choisi et on reprend la partie
		if (AMenuChelles* M = LeMenu())
		{
			M->Reprendre();
			M->LancerModeLobby();
		}
		return;
	}
	Afficher(EEcranMenu::Mode);
}

TSharedRef<SWidget> SMenuChelles::EcranHabillage()
{
	using namespace LobbyChelles;
	if (!bTenueLue)
	{
		TenueChoisie = ChellesHabillage::TenueLocale();
		bTenueLue = true;
	}
	if (!ApercuActeur.IsValid())
	{
		MontrerApercu();
	}
	const int32 SexeActuel = SexeLobby();
	if (!bModeLobbyLu)
	{
		bModeLobbyLu = true;
		ModeLobby = TEXT("Soldat");
		if (GConfig)
		{
			GConfig->GetString(TEXT("ChellesLobby"), TEXT("ModeDepart"), ModeLobby, GGameUserSettingsIni);
		}
	}
	const FModeLobby* ModeChoisi = TrouverModeLobby(ModeLobby);
	const ChellesHabillage::FModele* Modele = ChellesHabillage::TrouverModele(TenueChoisie.Modele);
	const bool bLook = !ModeChoisi && !TenueChoisie.EstSpiderMan() && Modele != nullptr;

	// Disposition "inversee" (demande du 24/11) : les onglets sont une colonne a gauche,
	// les rubans de cartes deviennent des colonnes verticales (personnages a gauche, tenue a droite),
	// le nom et les boutons VALIDER / ALEATOIRE sont en ligne au milieu.

	// ---- Colonne d'onglets a gauche ----
	TSharedRef<SVerticalBox> Onglets = SNew(SVerticalBox);
	auto AjouterOnglet = [&Onglets](const TCHAR* Texte, bool bActif, bool bPoint, TFunction<void()> Action)
	{
		Onglets->AddSlot().AutoHeight().Padding(FMargin(0.f, 4.f))
		[
			SNew(SBox).WidthOverride(190.f)
			[
				SNew(SCarteLobbyChelles)
				.Titre(Texte)
				.Puce(true)
				.Choisie(bActif)
				.Point(bPoint)
				.TaillePolice(20)
				.Taille(FVector2D(10.f, 48.f))
				.OnClic(FSimpleDelegate::CreateLambda(MoveTemp(Action)))
			]
		];
	};
	AjouterOnglet(EnPause() ? TEXT("REPRENDRE") : TEXT("JOUER"), false, false, [this]() { JouerDepuisLobby(); });
	AjouterOnglet(TEXT("CASIER"), true, false, []() {});
	if (EnPause() && IConsoleManager::Get().FindConsoleObject(TEXT("Chelles.Armements")))
	{
		AjouterOnglet(TEXT("ARMEMENTS"), false, true, [this]()
		{
			ChellesHabillage::EnregistrerTenue(TenueChoisie);
			AMenuChelles* M = LeMenu();
			UWorld* Monde = M ? M->GetWorld() : nullptr;
			if (M)
			{
				M->Reprendre();
			}
			if (GEngine && Monde)
			{
				GEngine->Exec(Monde, TEXT("Chelles.Armements Ouvrir"));
			}
		});
	}
	AjouterOnglet(TEXT("OPTIONS"), false, true, [this]() { Afficher(EEcranMenu::Options); });
	AjouterOnglet(TEXT("RETOUR"), false, false, [this]() { Retour(); });

	// ---- Nom du personnage en grand ----
	FString NomGrand;
	FString Description;
	if (ModeChoisi)
	{
		NomGrand = ModeChoisi->Nom;
		Description = ModeChoisi->Description;
	}
	else if (TenueChoisie.EstPersoWraith())
	{
		NomGrand = ChellesHabillage::NomPersoWraith(TenueChoisie.Sexe);
		Description = ChellesHabillage::EstPersoCree(TenueChoisie.Sexe)
			? TEXT("Toutes les armes du soldat (TAB = roue, I = ARMEMENTS). X = flash, O = changer de tir. J = danser, encore J = danse suivante (bouger pour arrêter). K = apparence suivante.")
			: TEXT("Héros du SOLDAT : toutes ses armes (TAB = roue, I = ARMEMENTS). X = flash, O = changer de tir. K = apparence suivante.");
	}
	else if (TenueChoisie.EstPantera())
	{
		NomGrand = TEXT("PANTERA");
		Description = TEXT("Tenue noire, yeux blancs. Il vole 20 secondes au maximum. Clic gauche = tirs des mains, O = changer de tir.");
	}
	else if (TenueChoisie.EstSpiderMan())
	{
		NomGrand = TEXT("SPIDER-MALE");
		Description = TEXT("Le héros de tout le jeu. Choisis FÉMININ ou MASCULIN pour créer ton propre look pièce par pièce.");
	}
	else if (Modele)
	{
		NomGrand = Modele->Nom.ToUpper();
		Description = Modele->bMannequin
			? TEXT("Mannequin tout fait : sa tenue ne change pas. Choisis un autre MODÈLE pour habiller ton joueur pièce par pièce.")
			: TEXT("Choisis un morceau de tenue (HAUT, BAS...) dans la colonne de droite, puis fais défiler les choix.");
	}
	else
	{
		NomGrand = SexeActuel == ChellesHabillage::Feminin ? TEXT("FÉMININ") : TEXT("MASCULIN");
		Description = TEXT("Packs de personnages introuvables (Action_Male_and_Female, Female_Mannequin) : tu restes en Spider-Male.");
	}
	// 24/11/2026 : chaque personnage a son pouvoir et sa fonction (Plugins/ChellesPouvoirs/Personnages.txt)
	const FString PouvoirTexte = PouvoirDuPersoLobby(NomPersoLobby());
	if (!PouvoirTexte.IsEmpty())
	{
		Description = PouvoirTexte + TEXT("\n") + Description;
	}

	auto Etiquette = [](const FString& Texte) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Texte))
			.Font(LobPolice(20))
			.ColorAndOpacity(FLinearColor::White)
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.05f, 0.7f));
	};
	// Fleche haut / bas qui fait defiler une colonne
	auto Fleche = [](const TCHAR* Sens, TSharedRef<SScrollBox> Colonne, float Pas) -> TSharedRef<SWidget>
	{
		return SNew(SCarteLobbyChelles)
			.Titre(Sens)
			.Puce(true)
			.TaillePolice(18)
			.Taille(FVector2D(10.f, 34.f))
			.OnClic(FSimpleDelegate::CreateLambda([Colonne, Pas]()
			{
				Colonne->SetScrollOffset(FMath::Max(0.f, Colonne->GetScrollOffset() + Pas));
			}));
	};
	auto ColonneDefilante = [&Fleche, &Etiquette](const FString& Titre, TSharedRef<SScrollBox> Colonne, float Largeur) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.WidthOverride(Largeur)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Etiquette(Titre) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Fleche(TEXT("HAUT"), Colonne, -360.f) ]
				+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(0.f, 6.f))[ Colonne ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Fleche(TEXT("BAS"), Colonne, 360.f) ]
			];
	};

	// ---- Colonne 1 : les personnages ----
	TSharedPtr<SWidget> CartePersoChoisie;
	TSharedRef<SScrollBox> Colonne1 = SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed);
	for (const FPersoLobby& P : PersosLobby())
	{
		const int32 Sexe = P.Sexe;
		const bool bChoisie = !ModeChoisi && (Sexe == SexeActuel);
		TSharedRef<SWidget> Carte = SNew(SCarteLobbyChelles)
			.Titre(P.Nom)
			.SousTitre(P.SousTitre)
			.Couleur(P.Couleur)
			.Taille(FVector2D(220.f, 100.f))
			.Choisie(bChoisie)
			.TaillePolice(24)
			.OnClic(FSimpleDelegate::CreateLambda([this, Sexe]()
			{
				RubanLobby = 0;
				ModeLobby = TEXT("Soldat");
				ChoisirSexeLobby(Sexe);
			}));
		if (bChoisie)
		{
			CartePersoChoisie = Carte;
		}
		Colonne1->AddSlot().HAlign(HAlign_Center).Padding(FMargin(4.f, 6.f))[ Carte ];
	}
	// Heros et vehicules (les anciens modes F1-F12) : on part directement avec
	for (const FModeLobby& M : ModesLobby())
	{
		const FString Mode = M.Mode;
		const bool bChoisie = (ModeChoisi == &M);
		TSharedRef<SWidget> Carte = SNew(SCarteLobbyChelles)
			.Titre(M.Nom)
			.SousTitre(M.SousTitre)
			.Couleur(M.Couleur)
			.Taille(FVector2D(220.f, 100.f))
			.Choisie(bChoisie)
			.TaillePolice(24)
			.OnClic(FSimpleDelegate::CreateLambda([this, Mode]()
			{
				RubanLobby = 0;
				ModeLobby = Mode;
				Afficher(EEcranMenu::Habillage);
			}));
		if (bChoisie)
		{
			CartePersoChoisie = Carte;
		}
		Colonne1->AddSlot().HAlign(HAlign_Center).Padding(FMargin(4.f, 6.f))[ Carte ];
	}

	// ---- Colonnes de la tenue (FEMININ / MASCULIN) : emplacements puis choix ----
	TSharedRef<SHorizontalBox> Droite = SNew(SHorizontalBox);
	TSharedPtr<SScrollBox> Colonne2Ptr;
	TSharedPtr<SWidget> CarteTenueChoisie;
	if (bLook)
	{
		// Puces des emplacements empilees : MODELE, puis HAUT, VESTE, BAS... (mannequin tout fait : MODELE seulement)
		const int32 NbEmplacements = Modele->bMannequin ? 0 : Modele->Emplacements.Num();
		EmplacementLobby = FMath::Clamp(EmplacementLobby, 0, NbEmplacements);
		TSharedRef<SVerticalBox> Puces = SNew(SVerticalBox);
		for (int32 e = 0; e <= NbEmplacements; ++e)
		{
			const FString Nom = (e == 0) ? FString(TEXT("MODÈLE")) : Modele->Emplacements[e - 1].Titre.ToUpper();
			Puces->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))
			[
				SNew(SCarteLobbyChelles)
				.Titre(Nom)
				.Puce(true)
				.Choisie(e == EmplacementLobby)
				.TaillePolice(17)
				.Taille(FVector2D(10.f, 40.f))
				.OnClic(FSimpleDelegate::CreateLambda([this, e]()
				{
					EmplacementLobby = e;
					RubanLobby = 1;
					Afficher(EEcranMenu::Habillage);
				}))
			];
		}
		Droite->AddSlot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(170.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 6.f))[ Etiquette(TEXT("TENUE")) ]
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed)
					+ SScrollBox::Slot()[ Puces ]
				]
			]
		];

		TSharedRef<SScrollBox> Colonne2 = SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed);
		Colonne2Ptr = Colonne2;
		if (EmplacementLobby == 0)
		{
			const TArray<const ChellesHabillage::FModele*> Liste = ChellesHabillage::ModelesDuSexe(TenueChoisie.Sexe);
			for (int32 i = 0; i < Liste.Num(); ++i)
			{
				const FString Id = Liste[i]->Id;
				const bool bChoisie = (Id == TenueChoisie.Modele);
				TSharedRef<SWidget> Carte = SNew(SCarteLobbyChelles)
					.Titre(Liste[i]->Nom.ToUpper())
					.SousTitre(Liste[i]->bMannequin ? TEXT("MANNEQUIN") : TEXT("MODÈLE"))
					.Couleur(CouleurChoix(0, i, false))
					.Taille(FVector2D(200.f, 84.f))
					.Choisie(bChoisie)
					.TaillePolice(19)
					.OnClic(FSimpleDelegate::CreateLambda([this, Id]()
					{
						RubanLobby = 1;
						ModifierTenue([Id](ChellesHabillage::FTenue& T) { T.Modele = Id; });
					}));
				if (bChoisie)
				{
					CarteTenueChoisie = Carte;
				}
				Colonne2->AddSlot().HAlign(HAlign_Center).Padding(FMargin(4.f, 5.f))[ Carte ];
			}
		}
		else
		{
			const ChellesHabillage::FEmplacement* PE = &Modele->Emplacements[EmplacementLobby - 1];
			const FString* Actuel = TenueChoisie.Choix.Find(PE->Cle);
			const int32 IndexActuel = FMath::Max(0, PE->IndexDe(Actuel ? *Actuel : FString()));
			for (int32 i = 0; i < PE->Lignes.Num(); ++i)
			{
				const bool bAucun = PE->Lignes[i].IsNone();
				const bool bChoisie = (i == IndexActuel);
				const FString Nom = PE->Noms.IsValidIndex(i) ? PE->Noms[i] : PE->Lignes[i].ToString();
				TSharedRef<SWidget> Carte = SNew(SCarteLobbyChelles)
					.Titre(Nom.ToUpper())
					.SousTitre(PE->Titre.ToUpper())
					.Couleur(CouleurChoix(EmplacementLobby, i, bAucun))
					.Taille(FVector2D(200.f, 84.f))
					.Choisie(bChoisie)
					.TaillePolice(19)
					.OnClic(FSimpleDelegate::CreateLambda([this, PE, i]()
					{
						RubanLobby = 1;
						ModifierTenue([PE, i](ChellesHabillage::FTenue& T)
						{
							if (PE->Lignes.IsValidIndex(i))
							{
								T.Choix.Add(PE->Cle, PE->Lignes[i].IsNone() ? FString() : PE->Lignes[i].ToString());
							}
						});
					}));
				if (bChoisie)
				{
					CarteTenueChoisie = Carte;
				}
				Colonne2->AddSlot().HAlign(HAlign_Center).Padding(FMargin(4.f, 5.f))[ Carte ];
			}
		}
		Droite->AddSlot().AutoWidth()
		[
			ColonneDefilante(EmplacementLobby == 0 ? FString(TEXT("MODÈLES")) : Modele->Emplacements[EmplacementLobby - 1].Titre.ToUpper(), Colonne2, 220.f)
		];
	}
	else
	{
		RubanLobby = 0;
	}

	// La carte choisie reste visible dans sa colonne
	if (CartePersoChoisie.IsValid())
	{
		Colonne1->ScrollDescendantIntoView(CartePersoChoisie, false, EDescendantScrollDestination::Center);
	}
	if (Colonne2Ptr.IsValid() && CarteTenueChoisie.IsValid())
	{
		Colonne2Ptr->ScrollDescendantIntoView(CarteTenueChoisie, false, EDescendantScrollDestination::Center);
	}

	// ---- Apercu du personnage ----
	TSharedRef<SWidget> Apercu = PinceauApercu.IsValid()
		? StaticCastSharedRef<SWidget>(SNew(SZoneApercuChelles).Acteur(ApercuActeur).Image(PinceauApercu.Get()).SansCadre(true))
		: StaticCastSharedRef<SWidget>(SNew(SBox));

	const bool bAleatoire = bLook && !Modele->bMannequin;

	// ---- Milieu : nom en ligne en haut, personnage, boutons en ligne en bas ----
	TSharedRef<SHorizontalBox> BoutonsBas = SNew(SHorizontalBox);
	BoutonsBas->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f))
	[
		SNew(SCarteLobbyChelles)
		.Titre(EnPause() ? TEXT("VALIDER ET REPRENDRE") : TEXT("VALIDER ET JOUER  >"))
		.Puce(true)
		.Choisie(true)
		.TaillePolice(24)
		.Taille(FVector2D(10.f, 58.f))
		.OnClic(FSimpleDelegate::CreateLambda([this]() { JouerDepuisLobby(); }))
	];
	if (bAleatoire)
	{
		BoutonsBas->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f))
		[
			SNew(SCarteLobbyChelles)
			.Titre(TEXT("ALÉATOIRE"))
			.Puce(true)
			.Point(true)
			.TaillePolice(20)
			.Taille(FVector2D(10.f, 58.f))
			.OnClic(FSimpleDelegate::CreateLambda([this]()
			{
				ModifierTenue([](ChellesHabillage::FTenue& T) { ChellesHabillage::Hasard(T); });
			}))
		];
	}

	TSharedRef<SWidget> Milieu = SNew(SVerticalBox)
		// Nom + petite etiquette sur une meme ligne, description dessous
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(NomGrand))
				.Font(LobPolice(NomGrand.Len() > 12 ? 46 : 64))
				.ColorAndOpacity(FLinearColor::White)
				.ShadowOffset(FVector2D(4.f, 4.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.08f, 0.7f))
				.RenderTransform(LobPenche(0.12f))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(20.f, 0.f, 0.f, 0.f))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor(0.f, 0.02f, 0.1f, 0.6f))
				.Padding(FMargin(14.f, 5.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(EnPause() ? TEXT("Ton personnage dans la partie") : TEXT("Ton personnage")))
					.Font(LobPolice(15, TEXT("Bold")))
					.ColorAndOpacity(FLinearColor(0.9f, 0.95f, 1.f))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 4.f))
		[
			SNew(SBox).MaxDesiredWidth(820.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Description))
				.Font(LobPolice(15, TEXT("Regular")))
				.ColorAndOpacity(FLinearColor(0.85f, 0.9f, 1.f, 0.85f))
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		]
		// Le personnage, qui retrecit si l'ecran est petit
		+ SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).Padding(FMargin(0.f, 6.f))
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox).WidthOverride(560.f).HeightOverride(840.f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()[ Apercu ]
					+ SOverlay::Slot()[ SNew(SBordsLobbyChelles) ]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			BoutonsBas
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Clic sur une carte pour choisir  •  Haut / Bas = choix suivant, Gauche / Droite = changer de colonne  •  Glisse sur le personnage pour le tourner")))
			.Font(LobPolice(13, TEXT("Bold")))
			.ColorAndOpacity(FLinearColor(0.75f, 0.85f, 1.f, 0.75f))
		];

	return SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SFondLobbyChelles)
		]
		+ SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			// Colonne d'onglets (barre verticale sombre)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor(0.f, 0.01f, 0.06f, 0.55f))
				.Padding(FMargin(20.f, 34.f, 20.f, 24.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 36.f))
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("CHELLES\n3D")))
						.Font(LobPolice(28))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FLinearColor::White)
						.RenderTransform(LobPenche())
						.RenderTransformPivot(FVector2D(0.5f, 0.5f))
					]
					+ SVerticalBox::Slot().AutoHeight()[ Onglets ]
					+ SVerticalBox::Slot().FillHeight(1.f)[ SNew(SSpacer) ]
				]
			]
			// Colonne des personnages
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(22.f, 34.f, 0.f, 24.f))
			[
				ColonneDefilante(TEXT("PERSONNAGES"), Colonne1, 240.f)
			]
			// Milieu
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(20.f, 30.f, 20.f, 22.f))
			[
				Milieu
			]
			// Colonnes de la tenue
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 80.f, 40.f, 24.f))
			[
				Droite
			]
		];
}

TSharedRef<SWidget> SMenuChelles::EcranMulti()
{
	AMenuChelles* M = LeMenu();
	const FString Ip = M ? M->AdresseLocale() : FString();
	const FText Info = FText::FromString(Ip.IsEmpty() ? FString(TEXT("Adresse IP de ce PC : inconnue"))
		: FString::Printf(TEXT("Adresse IP de ce PC sur le r\u00E9seau : %s"), *Ip));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("MULTI-PLAYER")), FText::FromString(TEXT("// JOUER \u00C0 PLUSIEURS"))) ]
		// Disposition inversee : les quatre choix sont en ligne
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("REJOINDRE SERVEUR")), 32, [this]()
				{
					Afficher(EEcranMenu::Serveurs);
					if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); }
				}) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("ADRESSE IP SERVEUR")), 32, [this]() { Afficher(EEcranMenu::AdresseIP); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("LAN LOCAL")), 32, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("SEARCH")), 32, [this]()
				{
					Afficher(EEcranMenu::Search);
					if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); }
				}) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TextePvp(); }, [](int32) { ChellesOptions::ChangerPvp(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("LAN LOCAL = cr\u00E9er ta partie : les autres PC de la maison pourront la rejoindre."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(Info) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranServeurs()
{
	AMenuChelles* M = LeMenu();
	TSharedRef<SVerticalBox> Liste = SNew(SVerticalBox);

	if (M && M->RechercheEnCours())
	{
		Liste->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Recherche des parties sur ton r\u00E9seau local..."))) ];
	}
	else if (M && M->Parties().Num() > 0)
	{
		for (int32 i = 0; i < M->Parties().Num(); ++i)
		{
			const FPartieTrouvee& P = M->Parties()[i];
			const FText Libelle = FText::FromString(FString::Printf(TEXT("%s    %d/%d JOUEURS    PVP %s    %d ms"), *P.Hote.ToUpper(), P.Joueurs, P.Places, P.bPvp ? TEXT("ON") : TEXT("OFF"), P.Ping));
			Liste->AddSlot().AutoHeight()[ Bouton(Libelle, 30, [this, i]() { if (AMenuChelles* Mn = LeMenu()) { Mn->RejoindrePartie(i); } }) ];
		}
	}
	else if (M && M->RechercheFaite())
	{
		Liste->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Aucun serveur trouv\u00E9. Sur l'autre PC, il faut choisir MULTI-PLAYER puis LAN LOCAL."))) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("SERVEURS")), FText::FromString(TEXT("// PARTIES SUR TON R\u00C9SEAU LOCAL"))) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.MaxDesiredHeight(380.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[ Liste ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonRetour() ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("ACTUALISER")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); } }, !(M && M->RechercheEnCours())) ]
		];
}

TSharedRef<SWidget> SMenuChelles::EcranAdresseIP()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("ADRESSE IP")), FText::FromString(TEXT("// REJOINDRE UN SERVEUR PAR SON ADRESSE"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 10.f))
		[
			SNew(SBox)
			.WidthOverride(560.f)
			[
				SAssignNew(ChampIP, SEditableTextBox)
				.Text(FText::FromString(TexteIP))
				.HintText(FText::FromString(TEXT("ex : 192.168.1.20")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30))
				.ForegroundColor(FSlateColor(FLinearColor::Black))
				.Justification(ETextJustify::Left)
				.OnTextChanged_Lambda([this](const FText& Nouveau) { TexteIP = Nouveau.ToString(); })
				.OnTextCommitted_Lambda([this](const FText& Nouveau, ETextCommit::Type Comment)
				{
					TexteIP = Nouveau.ToString();
					if (Comment == ETextCommit::OnEnter)
					{
						if (AMenuChelles* M = LeMenu()) { M->RejoindreIP(TexteIP); }
					}
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonRetour() ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("REJOINDRE")), 46, [this]() { if (AMenuChelles* M = LeMenu()) { M->RejoindreIP(TexteIP); } }) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("L'adresse du serveur s'affiche sur son \u00E9cran MULTI-PLAYER. Pour Internet, l'h\u00F4te doit ouvrir le port 7777 (UDP) de sa box."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ];
}

void SMenuChelles::ValiderCodeVIP()
{
	if (ChellesHabillage::DebloquerPerso(PersoVIPDemande, TexteCodeVIP))
	{
		const int32 Perso = PersoVIPDemande;
		PersoVIPDemande = INDEX_NONE;
		TexteCodeVIP.Reset();
		Son(ESonMenu::Clic);
		RubanLobby = 0;
		ModeLobby = TEXT("Soldat");
		Afficher(EEcranMenu::Habillage);
		ChoisirSexeLobby(Perso);
		return;
	}
	Son(ESonMenu::Retour);
	MessageCodeVIP = TEXT("CODE INCORRECT");
	TexteCodeVIP.Reset();
	if (ChampCodeVIP.IsValid())
	{
		ChampCodeVIP->SetText(FText::GetEmpty());
		FSlateApplication::Get().SetKeyboardFocus(ChampCodeVIP, EFocusCause::SetDirectly);
	}
}

TSharedRef<SWidget> SMenuChelles::EcranCodeVIP()
{
	const TCHAR* Nom = ChellesHabillage::NomPersoWraith(PersoVIPDemande);
	const FString NomVIP = Nom ? FString(Nom) : FString(TEXT("VIP"));
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("PERSONNAGE VIP")), FText::FromString(FString::Printf(TEXT("// %s EST R\u00C9SERV\u00C9 : TAPE LE CODE VIP"), *NomVIP))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 10.f))
		[
			SNew(SBox)
			.WidthOverride(360.f)
			[
				SAssignNew(ChampCodeVIP, SEditableTextBox)
				.Text(FText::GetEmpty())
				.IsPassword(true)
				.ClearKeyboardFocusOnCommit(false)
				.SelectAllTextWhenFocused(true)
				.HintText(FText::FromString(TEXT("code")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30))
				.ForegroundColor(FSlateColor(FLinearColor::Black))
				.Justification(ETextJustify::Center)
				.OnTextChanged_Lambda([this](const FText& Nouveau) { TexteCodeVIP = Nouveau.ToString(); })
				.OnTextCommitted_Lambda([this](const FText& Nouveau, ETextCommit::Type Comment)
				{
					TexteCodeVIP = Nouveau.ToString();
					if (Comment == ETextCommit::OnEnter)
					{
						ValiderCodeVIP();
					}
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonRetour() ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("VALIDER")), 46, [this]() { ValiderCodeVIP(); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 14.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return FText::FromString(MessageCodeVIP); })
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22))
			.ColorAndOpacity(FLinearColor(0.95f, 0.10f, 0.10f))
		]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Un seul code pour tous les personnages VIP, \u00E0 retaper \u00E0 chaque lancement du jeu."))) ];
}

TSharedRef<SWidget> SMenuChelles::EcranSearch()
{
	AMenuChelles* M = LeMenu();
	TSharedRef<SVerticalBox> Corps = SNew(SVerticalBox);
	// Disposition inversee : les boutons sont en ligne
	TSharedRef<SHorizontalBox> Choix = SNew(SHorizontalBox);

	if (!M || M->RechercheEnCours() || !M->RechercheFaite())
	{
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Je cherche une partie CHELLES 3D sur ton r\u00E9seau local..."))) ];
	}
	else if (M->Parties().Num() > 0)
	{
		const FPartieTrouvee& P = M->Parties()[0];
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(FString::Printf(TEXT("Partie trouv\u00E9e chez %s  (%d/%d joueurs, %d ms)"), *P.Hote, P.Joueurs, P.Places, P.Ping))) ];
		Choix->AddSlot().AutoWidth()[ Bouton(FText::FromString(TEXT("REJOINDRE")), 52, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->RejoindrePartie(0); } }) ];
		if (M->Parties().Num() > 1)
		{
			Choix->AddSlot().AutoWidth()[ Bouton(FText::FromString(FString::Printf(TEXT("VOIR LES %d PARTIES"), M->Parties().Num())), 30, [this]() { Afficher(EEcranMenu::Serveurs); }) ];
		}
		Choix->AddSlot().AutoWidth()[ Bouton(FText::FromString(TEXT("CR\u00C9ER MA PARTIE")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ];
	}
	else
	{
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Personne ne joue sur ton r\u00E9seau local pour l'instant. Tu peux cr\u00E9er ta partie : les autres la trouveront avec SEARCH. (Recherche sur Internet : prochaine \u00E9tape, avec Steam ou Epic.)"))) ];
		Choix->AddSlot().AutoWidth()[ Bouton(FText::FromString(TEXT("CR\u00C9ER MA PARTIE")), 46, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ];
		Choix->AddSlot().AutoWidth()[ Bouton(FText::FromString(TEXT("CHERCHER ENCORE")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); } }) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("SEARCH")), FText::FromString(TEXT("// RECHERCHE AUTOMATIQUE D'UNE PARTIE"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Corps ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ Choix ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::BoutonOnglet(const FText& Texte, EOngletOptions Cible)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 4.f))
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte(Texte)
			.TaillePolice(28)
			.Onglet(Onglet == Cible)
			.OnClic(FSimpleDelegate::CreateLambda([this, Cible]()
			{
				Onglet = Cible;
				Afficher(EEcranMenu::Options);
			})))
		];
}

TSharedRef<SWidget> SMenuChelles::EcranOptions()
{
	TSharedRef<SWidget> Contenu = OngletControles();
	switch (Onglet)
	{
	case EOngletOptions::Graphismes: Contenu = OngletGraphismes(); break;
	case EOngletOptions::Avances:    Contenu = OngletAvances(); break;
	case EOngletOptions::Audio:      Contenu = OngletAudio(); break;
	case EOngletOptions::Jeu:        Contenu = OngletJeu(); break;
	default: break;
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("OPTIONS")), FText::FromString(TEXT("// CLIC GAUCHE = SUIVANT   CLIC DROIT = PR\u00C9C\u00C9DENT   LB / RB / TAB = ONGLET"))) ]
		// Disposition inversee : les onglets sont une colonne a gauche, les reglages a droite
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.f, 0.f, 34.f, 0.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ BoutonOnglet(FText::FromString(TEXT("CONTR\u00D4LES")), EOngletOptions::Controles) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ BoutonOnglet(FText::FromString(TEXT("GRAPHISMES")), EOngletOptions::Graphismes) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ BoutonOnglet(FText::FromString(TEXT("AVANC\u00C9S")), EOngletOptions::Avances) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ BoutonOnglet(FText::FromString(TEXT("AUDIO")), EOngletOptions::Audio) ]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[ BoutonOnglet(FText::FromString(TEXT("JEU")), EOngletOptions::Jeu) ]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
			[
				SNew(SBox)
				.MinDesiredHeight(380.f)
				.VAlign(VAlign_Top)
				[
					Contenu
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::OngletControles()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteControle(); }, [](int32 S) { ChellesOptions::ChangerControle(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteClavier(); }, [](int32 S) { ChellesOptions::ChangerClavier(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteSensibilite(); }, [](int32 S) { ChellesOptions::ChangerSensibilite(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteInverserSouris(); }, [](int32 S) { ChellesOptions::ChangerInverserSouris(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteInverserManette(); }, [](int32 S) { ChellesOptions::ChangerInverserManette(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("RACCOURCIS CLAVIER >")), 30, [this]() { Afficher(EEcranMenu::Touches); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Clavier et manette marchent tous les deux en jeu. Manette : croix = choisir, A = valider, B = retour, gauche/droite = changer un r\u00E9glage."))) ];
}

TSharedRef<SWidget> SMenuChelles::OngletGraphismes()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteQualite(); }, [](int32 S) { ChellesOptions::ChangerQualite(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteAffichage(); }, [](int32 S) { ChellesOptions::ChangerAffichage(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteResolution(); }, [](int32 S) { ChellesOptions::ChangerResolution(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteVSync(); }, [](int32 S) { ChellesOptions::ChangerVSync(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteFPS(); }, [](int32 S) { ChellesOptions::ChangerFPS(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("D\u00C9TECTER MON PC")), 30, []() { ChellesGraphismes::DetecterMonPC(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SBox).Padding(FMargin(0.f, 2.f)).MaxDesiredWidth(900.f)[ SNew(STextBlock).Text_Lambda([]() { return FText::FromString(ChellesGraphismes::Message()); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16)).ColorAndOpacity(Magenta).AutoWrapText(true) ] ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Ombres, reflets, eau, effets, herbe, distance de vue, compteur FPS... un par un : onglet AVANC\u00C9S."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("QUALIT\u00C9 : BAS pour un PC faible ou un portable (image un peu moins fine, moins d'herbe, ombres et objets moins loin), MOYEN pour un PC moyen, HAUT = tout au maximum."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Enregistr\u00E9 automatiquement (GameUserSettings). Affichage et r\u00E9solution ne changent que dans le jeu lanc\u00E9 seul, pas dans la fen\u00EAtre de l'\u00E9diteur."))) ];
}

TSharedRef<SWidget> SMenuChelles::OngletAvances()
{
	// 25/11/2026 (agent Terrain) : chaque reglage graphique un par un, en deux colonnes
	const int32 Total = ChellesGraphismes::Nombre();
	const int32 Moitie = (Total + 1) / 2;
	auto Colonne = [this, Total](int32 Debut, int32 Fin)
	{
		TSharedRef<SVerticalBox> Boite = SNew(SVerticalBox);
		for (int32 i = Debut; i < Fin && i < Total; ++i)
		{
			Boite->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 1.f))
			[
				Enregistrer(SNew(SBoutonMenu)
				.Texte_Lambda([i]() { return ChellesGraphismes::Texte(i); })
				.TaillePolice(21)
				.LargeurNom(21.f * 11.5f)
				.OnClic(FSimpleDelegate::CreateLambda([i]() { ChellesGraphismes::Changer(i, 1); }))
				.OnClicDroit(FSimpleDelegate::CreateLambda([i]() { ChellesGraphismes::Changer(i, -1); })))
			];
		}
		return Boite;
	};
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.f, 0.f, 18.f, 0.f))[ Colonne(0, Moitie) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[ Colonne(Moitie, Total) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("D\u00C9TECTER MON PC")), 24, []() { ChellesGraphismes::DetecterMonPC(); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("TOUT EN AUTO")), 24, []() { ChellesGraphismes::ToutEnAuto(); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SBox).Padding(FMargin(0.f, 2.f)).MaxDesiredWidth(900.f)[ SNew(STextBlock).Text_Lambda([]() { return FText::FromString(ChellesGraphismes::Message()); }).Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15)).ColorAndOpacity(Magenta).AutoWrapText(true) ] ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("AUTO = suit la QUALIT\u00C9 BAS / MOYEN / HAUT. PC qui rame : enl\u00E8ve d'abord OMBRES DOUCES, BROUILLARD 3D, REFLETS, puis baisse OMBRES, HERBE et NETTET\u00C9 IMAGE. Tout est enregistr\u00E9 tout seul."))) ];
}

TSharedRef<SWidget> SMenuChelles::OngletAudio()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteVolume(); }, [](int32 S) { ChellesOptions::ChangerVolume(S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeGeneral(); }, [](float V) { ChellesOptions::DefinirVolumeGeneral(V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(0); }, [](int32 S) { ChellesOptions::ChangerCanal(0, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeMusique(); }, [](float V) { ChellesOptions::DefinirVolumeCanal(0, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(1); }, [](int32 S) { ChellesOptions::ChangerCanal(1, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeEffets(); }, [](float V) { ChellesOptions::DefinirVolumeCanal(1, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(2); }, [](int32 S) { ChellesOptions::ChangerCanal(2, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeCanal(2); }, [](float V) { ChellesOptions::DefinirVolumeCanal(2, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(3); }, [](int32 S) { ChellesOptions::ChangerCanal(3, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeCanal(3); }, [](float V) { ChellesOptions::DefinirVolumeCanal(3, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(4); }, [](int32 S) { ChellesOptions::ChangerCanal(4, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeCanal(4); }, [](float V) { ChellesOptions::DefinirVolumeCanal(4, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteSon(); }, [](int32 S) { ChellesOptions::ChangerSon(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Le volume g\u00E9n\u00E9ral agit sur tout le jeu. Musique / effets / voix agissent sur les sons class\u00E9s comme tels dans le pack FPS, et sur les sons de ce menu. Ambiance = bruits du quartier (ville, oiseaux, vent, nuit, neige, invasion). V\u00E9hicules = moteurs et klaxons (H = klaxonner en conduisant). En jeu : Page Haut / Page Bas = volume musique, N = couper, Fin = piste suivante."))) ];
}

TSharedRef<SWidget> SMenuChelles::OngletJeu()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteChat(); }, [](int32) { ChellesOptions::ChangerChat(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteChatGlobal(); }, [](int32) { ChellesOptions::ChangerChatGlobal(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteAide(); }, [](int32) { ChellesOptions::ChangerAide(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TextePvp(); }, [](int32) { ChellesOptions::ChangerPvp(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteDifficulte(); }, [](int32 S) { ChellesOptions::ChangerDifficulte(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("En jeu, ENTR\u00C9E ouvre le panneau de droite : chat entre joueurs (en multi) et aide Tuto & Astuces. B ouvre le CHAT GLOBAL \u00E0 gauche : tous les joueurs du jeu et les visiteurs du site chelles3d.fr. Le PVP est choisi par celui qui h\u00E9berge la partie. L'effet de la difficult\u00E9 sur les ennemis viendra apr\u00E8s le test LAN."))) ];
}

TSharedRef<SWidget> SMenuChelles::EcranTouches()
{
	TSharedRef<SVerticalBox> Liste = SNew(SVerticalBox);
	const int32 Nombre = ChellesOptions::NombreTouches();
	for (int32 i = 0; i < Nombre; ++i)
	{
		if (i == ChellesOptions::ListeTouches().Num())
		{
			Liste->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(16.f, 14.f, 0.f, 4.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("TOUCHES DES MODES CHELLES (pouvoirs, danse, v\u00E9hicules...)")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
				.ColorAndOpacity(Magenta)
			];
		}
		Liste->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 1.f))
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte_Lambda([this, i]() { return ChellesOptions::TexteTouche(i, ToucheEnAttente == i); })
			.TaillePolice(24)
			.LargeurNom(24.f * 16.f)
			.OnClic(FSimpleDelegate::CreateLambda([this, i]() { ToucheEnAttente = i; }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([i]() { ChellesOptions::ToucheParDefaut(i); })))
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("RACCOURCIS CLAVIER")), FText::FromString(TEXT("// CLIC = CHANGER LA TOUCHE   CLIC DROIT = TOUCHE D'ORIGINE   \u00C9CHAP = ANNULER"))) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.MaxDesiredHeight(470.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[ Liste ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text_Lambda([]() { return FText::FromString(ChellesOptions::GMessageTouches); })
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
			.ColorAndOpacity(Magenta)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonRetour() ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("TOUT REMETTRE PAR D\u00C9FAUT")), 28, []() { ChellesOptions::ToutesLesTouchesParDefaut(); }) ]
		];
}

TSharedRef<SWidget> SMenuChelles::EcranQuitter()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("QUITTER ?")), FText::FromString(TEXT("// \u00CATES-VOUS S\u00DBR DE VOULOIR QUITTER ?"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("NON, RESTER")), 56, [this]() { Retour(); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("OUI, QUITTER")), 56, [this]() { if (AMenuChelles* M = LeMenu()) { M->Quitter(); } }) ]
		];
}

TSharedRef<SWidget> SMenuChelles::Curseur(TFunction<float()> Lire, TFunction<void(float)> Ecrire)
{
	return SNew(SBox)
		.WidthOverride(260.f)
		.Padding(FMargin(10.f, 0.f, 0.f, 0.f))
		[
			SNew(SSlider)
			.Value_Lambda(MoveTemp(Lire))
			.StepSize(0.05f)
			.SliderBarColor(Alpha(Magenta, 0.8f))
			.SliderHandleColor(Cyan)
			.OnValueChanged_Lambda([Ecrire](float V) { Ecrire(FMath::RoundToFloat(V * 20.f) / 20.f); })
			.OnMouseCaptureEnd_Lambda([]() { Son(ESonMenu::Reglage); })
		];
}

TSharedRef<SWidget> SMenuChelles::EcranCredits()
{
	auto LigneCredit = [](const TCHAR* Role, const TCHAR* Nom)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Role))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
				.ColorAndOpacity(Alpha(Magenta, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Nom))
				.Font(Police(30, Magenta))
				.ColorAndOpacity(Cyan)
			];
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("CR\u00C9DITS")), FText::FromString(TEXT("// CHELLES 3D  -  ") + ChellesOptions::Version())) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 50.f, 0.f))[ LigneCredit(TEXT("CR\u00C9ATION"), TEXT("MERIDJA RACHID")) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 50.f, 0.f))[ LigneCredit(TEXT("DESIGN"), TEXT("MERIDJA DYLAN")) ]
			+ SHorizontalBox::Slot().AutoWidth()[ LigneCredit(TEXT("MOTEUR"), TEXT("UNREAL ENGINE 4.27")) ]
		]
		+ SVerticalBox::Slot().AutoHeight()[ LigneCredit(TEXT("PACKS UTILIS\u00C9S"), TEXT("FPS GAME  -  VEHICLE GAME  -  MARKETPLACE")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))[ BoutonRetour() ];
}
