// CHELLES 3D - Dessin du menu (Slate, sans asset), style cyberpunk
#include "SMenuChelles.h"

#include "MenuChelles.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
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
	const FLinearColor Cyan(FColor(0, 240, 255));
	const FLinearColor Magenta(FColor(255, 0, 170));
	const FLinearColor Violet(FColor(120, 40, 255));
	const FLinearColor Or(FColor(255, 196, 64));
	const FLinearColor Blanc(FColor(235, 250, 255));
	const FLinearColor Gris(FColor(90, 110, 125));
	const FLinearColor Fond(FColor(5, 7, 18, 225));

	FLinearColor Alpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
	}

	FSlateFontInfo Police(int32 Taille, const FLinearColor& Halo, const TCHAR* Graisse = TEXT("Black"))
	{
		FSlateFontInfo P = FCoreStyle::GetDefaultFontStyle(Graisse, Taille);
		P.OutlineSettings.OutlineSize = FMath::Max(1, Taille / 22);
		P.OutlineSettings.OutlineColor = Alpha(Halo, 0.75f);
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
			Ajouter(TEXT("Utiliser"), TEXT("UTILISER / PRENDRE"), EKeys::E, TEXT("Use,Interact"));
			Ajouter(TEXT("Grenade"), TEXT("GRENADE"), EKeys::G, TEXT("GrenadeThrow"));
			Ajouter(TEXT("CorpsACorps"), TEXT("CORPS \u00C0 CORPS"), EKeys::V, TEXT("Melee"));
			Ajouter(TEXT("Inventaire"), TEXT("INVENTAIRE"), EKeys::I, TEXT("Inventory"));
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

	void AppliquerAuDemarrage(UWorld* Monde)
	{
		GMonde = Monde;
		AppliquerVolume();
		AppliquerQualite(QualiteChoisie(), false);
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

	static FText TexteTouche(int32 i, bool bEnAttente)
	{
		const TArray<FToucheChelles>& Liste = ListeTouches();
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
		if (Liste.IsValidIndex(i))
		{
			EcrireTexte(*Liste[i].Id, K == Liste[i].Defaut ? FString() : K.GetFName().ToString());
			ReAppliquerTout();
		}
	}

	static void ToucheParDefaut(int32 i)
	{
		const TArray<FToucheChelles>& Liste = ListeTouches();
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
	const FVector2D T = G.GetLocalSize();
	const float Temps = Horloge();
	TArray<FVector2D> P;
	P.SetNum(2);

	// Grille neon
	const float Pas = 64.f;
	for (float X = FMath::Fmod(T.X * 0.5f, Pas); X < T.X; X += Pas)
	{
		P[0] = FVector2D(X, 0.f);
		P[1] = FVector2D(X, T.Y);
		Ligne(Sortie, Couche, G, P, Alpha(Cyan, 0.045f), 1.f);
	}
	for (float Y = FMath::Fmod(T.Y * 0.5f, Pas); Y < T.Y; Y += Pas)
	{
		P[0] = FVector2D(0.f, Y);
		P[1] = FVector2D(T.X, Y);
		Ligne(Sortie, Couche, G, P, Alpha(Cyan, 0.045f), 1.f);
	}

	// Lignes de balayage (effet ecran)
	for (float Y = 0.f; Y < T.Y; Y += 4.f)
	{
		P[0] = FVector2D(0.f, Y);
		P[1] = FVector2D(T.X, Y);
		Ligne(Sortie, Couche, G, P, FLinearColor(0.f, 0.f, 0.f, 0.12f), 1.f);
	}

	// Bande lumineuse qui descend
	const float Bande = FMath::Fmod(Temps * 140.f, T.Y + 200.f) - 100.f;
	for (int32 i = 0; i < 12; ++i)
	{
		P[0] = FVector2D(0.f, Bande + i * 2.f);
		P[1] = FVector2D(T.X, Bande + i * 2.f);
		Ligne(Sortie, Couche + 1, G, P, Alpha(Cyan, 0.025f), 2.f);
	}

	// Cadre de l'interface
	const float M = 36.f;
	const float Pulse = 0.55f + 0.25f * FMath::Sin(Temps * 2.f);
	TArray<FVector2D> Haut;
	Haut.Add(FVector2D(M, M + 30.f));
	Haut.Add(FVector2D(M + 30.f, M));
	Haut.Add(FVector2D(T.X * 0.35f, M));
	Ligne(Sortie, Couche + 1, G, Haut, Alpha(Magenta, Pulse), 2.f);
	TArray<FVector2D> HautDroit;
	HautDroit.Add(FVector2D(T.X * 0.65f, M));
	HautDroit.Add(FVector2D(T.X - M, M));
	HautDroit.Add(FVector2D(T.X - M, M + 60.f));
	Ligne(Sortie, Couche + 1, G, HautDroit, Alpha(Cyan, Pulse), 2.f);
	TArray<FVector2D> Bas;
	Bas.Add(FVector2D(T.X - M, T.Y - M - 30.f));
	Bas.Add(FVector2D(T.X - M - 30.f, T.Y - M));
	Bas.Add(FVector2D(T.X * 0.65f, T.Y - M));
	Ligne(Sortie, Couche + 1, G, Bas, Alpha(Magenta, Pulse), 2.f);
	TArray<FVector2D> BasGauche;
	BasGauche.Add(FVector2D(T.X * 0.35f, T.Y - M));
	BasGauche.Add(FVector2D(M, T.Y - M));
	BasGauche.Add(FVector2D(M, T.Y - M - 60.f));
	Ligne(Sortie, Couche + 1, G, BasGauche, Alpha(Cyan, Pulse), 2.f);

	const FSlateFontInfo Mini = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(M + 40.f, M + 8.f), FVector2D(400.f, 20.f)),
		FString(TEXT("CHELLES//3D  -  SYS.MENU v1.0")), Mini, ESlateDrawEffect::None, Alpha(Cyan, 0.6f));
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(M + 40.f, T.Y - M - 26.f), FVector2D(400.f, 20.f)),
		FString(TEXT("\u00C9CHAP / B = RETOUR  //  A = VALIDER")), Mini, ESlateDrawEffect::None, Alpha(Magenta, 0.6f));

	// Numero de version en bas a droite
	const FSlateFontInfo PoliceVersion = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14);
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(T.X - M - 110.f, T.Y - M - 30.f), FVector2D(100.f, 24.f)),
		ChellesOptions::Version(), PoliceVersion, ESlateDrawEffect::None, Alpha(Cyan, 0.85f));

	// Etincelles neon et dorees qui montent derriere les boutons
	const FSlateBrush* Pinceau = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	for (int32 i = 0; i < 100; ++i)
	{
		const float Graine = FMath::Frac(FMath::Sin(i * 12.9898f) * 43758.5453f);
		const float Vitesse = 0.04f + 0.08f * FMath::Frac(Graine * 7.31f);
		const float Vie = FMath::Frac(Temps * Vitesse + Graine);
		const float X = T.X * (0.15f + 0.7f * FMath::Frac(Graine * 3.17f)) + FMath::Sin(Temps * 1.3f + i) * 18.f;
		const float Y = T.Y * (1.05f - Vie * 1.1f);
		const float Taille = 2.f + 3.f * FMath::Frac(Graine * 5.71f);
		const float Eclat = FMath::Sin(Vie * PI) * (0.35f + 0.4f * FMath::Frac(Graine * 9.13f));
		const FLinearColor Couleur = (i % 3 == 0) ? Or : ((i % 3 == 1) ? Cyan : Magenta);
		FSlateDrawElement::MakeBox(Sortie, Couche + 1, G.ToPaintGeometry(FVector2D(X - Taille * 1.5f, Y - Taille * 1.5f), FVector2D(Taille * 3.f, Taille * 3.f)),
			Pinceau, ESlateDrawEffect::None, Alpha(Couleur, Eclat * 0.15f));
		FSlateDrawElement::MakeBox(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(X - Taille * 0.5f, Y - Taille * 0.5f), FVector2D(Taille, Taille)),
			Pinceau, ESlateDrawEffect::None, Alpha(Couleur, Eclat));
	}

	return Couche + 3;
}

// ---------------------------------------------------------------- Bouton

void SBoutonMenu::Construct(const FArguments& InArgs)
{
	Clic = InArgs._OnClic;
	ClicDroit = InArgs._OnClicDroit;
	bActif = InArgs._Actif;
	bReglage = InArgs._OnClicDroit.IsBound();
	bOnglet = InArgs._Onglet;
	SetCursor(EMouseCursor::Hand);

	const int32 Taille = InArgs._TaillePolice;
	ChildSlot
	.Padding(FMargin(Taille * 0.7f, Taille * 0.12f))
	.HAlign(HAlign_Center)
	[
		SNew(STextBlock)
		.Text(InArgs._Texte)
		.Font(Police(Taille, Magenta))
		.Justification(ETextJustify::Center)
		.ShadowOffset(FVector2D(0.f, 3.f))
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
		.ColorAndOpacity_Lambda([this]()
		{
			if (!bActif)
			{
				return FSlateColor(Gris);
			}
			return FSlateColor((IsHovered() || bSelectionne || bOnglet) ? Blanc : Cyan);
		})
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
	if (bActif && (IsHovered() || bSelectionne))
	{
		const FVector2D T = G.GetLocalSize();
		const float Pulse = 0.7f + 0.3f * FMath::Sin(Horloge() * 7.f);

		TArray<FSlateGradientStop> Degrade;
		Degrade.Add(FSlateGradientStop(FVector2D(0.f, 0.f), Alpha(Magenta, 0.45f)));
		Degrade.Add(FSlateGradientStop(FVector2D(T.X * 0.5f, 0.f), Alpha(Violet, 0.30f)));
		Degrade.Add(FSlateGradientStop(FVector2D(T.X, 0.f), Alpha(Cyan, 0.20f)));
		FSlateDrawElement::MakeGradient(Sortie, Couche, G.ToPaintGeometry(), Degrade, Orient_Vertical);

		const float Coin = FMath::Min(T.Y * 0.35f, 22.f);
		const TArray<FVector2D> Contour = Chanfrein(T, Coin);
		Ligne(Sortie, Couche + 1, G, Contour, Alpha(Cyan, 0.25f * Pulse), 8.f); // halo
		Ligne(Sortie, Couche + 2, G, Contour, Alpha(Cyan, Pulse), 2.5f);

		// Petits marqueurs magenta sur les cotes
		TArray<FVector2D> Marque;
		Marque.Add(FVector2D(-14.f, T.Y * 0.3f));
		Marque.Add(FVector2D(-14.f, T.Y * 0.7f));
		Ligne(Sortie, Couche + 2, G, Marque, Magenta, 4.f);
		Marque[0].X = Marque[1].X = T.X + 14.f;
		Ligne(Sortie, Couche + 2, G, Marque, Magenta, 4.f);
	}
	if (bOnglet)
	{
		const FVector2D T = G.GetLocalSize();
		TArray<FVector2D> Trait;
		Trait.Add(FVector2D(T.X * 0.15f, T.Y - 2.f));
		Trait.Add(FVector2D(T.X * 0.85f, T.Y - 2.f));
		Ligne(Sortie, Couche + 2, G, Trait, Alpha(Magenta, 0.35f), 9.f);
		Ligne(Sortie, Couche + 2, G, Trait, Magenta, 3.f);
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
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(40.f, 80.f))
		[
			SAssignNew(Zone, SBox)
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
	const int32 N = ((int32(Onglet) + Sens) % 4 + 4) % 4;
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
	if (Zone.IsValid())
	{
		Zone->SetContent(Construire(Ecran));
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
	default:
		break;
	}
}

// ---------------------------------------------------------------- Briques

TSharedRef<SWidget> SMenuChelles::Bouton(const FText& Texte, int32 Taille, TFunction<void()> Action, bool bActif)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, Taille * 0.12f))
		.HAlign(HAlign_Center)
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
		.HAlign(HAlign_Center)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte_Lambda(MoveTemp(Texte))
			.TaillePolice(30)
			.OnClic(FSimpleDelegate::CreateLambda([Changer]() { Changer(1); }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([Changer]() { Changer(-1); })))
		];
}

TSharedRef<SWidget> SMenuChelles::Titre(const FText& Texte, const FText& SousTitre)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(Texte)
			.Font(Police(72, Cyan))
			.ColorAndOpacity(Magenta)
			.ShadowOffset(FVector2D(4.f, 4.f))
			.ShadowColorAndOpacity(Alpha(Violet, 0.6f))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Text(SousTitre)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
			.ColorAndOpacity(Alpha(Cyan, 0.8f))
		];
}

TSharedRef<SWidget> SMenuChelles::Petit(const FText& Texte)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 6.f))
		.HAlign(HAlign_Center)
		.MaxDesiredWidth(900.f)
		[
			SNew(STextBlock)
			.Text(Texte)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16))
			.ColorAndOpacity(Alpha(Blanc, 0.75f))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SMenuChelles::TexteEtat()
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 14.f))
		.HAlign(HAlign_Center)
		.MaxDesiredWidth(900.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { AMenuChelles* M = LeMenu(); return M ? M->Etat() : FText::GetEmpty(); })
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
			.ColorAndOpacity(Magenta)
			.Justification(ETextJustify::Center)
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
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("CHELLES 3D")), FText::FromString(TEXT("// FPS  -  QUARTIER DE CHELLES"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("JOUER")), 64, [this]() { Afficher(EEcranMenu::Habillage); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("OPTIONS")), 64, [this]() { Afficher(EEcranMenu::Options); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("CR\u00C9DITS")), 30, [this]() { Afficher(EEcranMenu::Credits); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("QUITTER")), 64, [this]() { Afficher(EEcranMenu::ConfirmerQuitter); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 14.f, 0.f, 0.f)).HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("DISCORD")), 22, [this]() { OuvrirLien(TEXT("LienDiscord"), TEXT("Discord")); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("FACEBOOK")), 22, [this]() { OuvrirLien(TEXT("LienFacebook"), TEXT("Facebook")); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("YOUTUBE")), 22, [this]() { OuvrirLien(TEXT("LienYoutube"), TEXT("YouTube")); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("TIKTOK")), 22, [this]() { OuvrirLien(TEXT("LienTiktok"), TEXT("TikTok")); }) ]
			+ SHorizontalBox::Slot().AutoWidth()[ Bouton(FText::FromString(TEXT("SITE")), 22, [this]() { OuvrirLien(TEXT("LienSite"), TEXT("le site")); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return MessageLien; })
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
			.ColorAndOpacity(Alpha(Or, 0.9f))
			.Justification(ETextJustify::Center)
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
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("SOLO")), 64, [this]() { if (AMenuChelles* M = LeMenu()) { M->Solo(); } }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("MULTI-PLAYER")), 64, [this]() { Afficher(EEcranMenu::Multi); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))[ BoutonRetour() ];
}

// ---------------------------------------------------------------- HABILLAGE (sexe, tenue, look)

/** Zone de l'apercu 3D : glisser a la souris pour faire tourner le personnage. */
class SZoneApercuChelles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SZoneApercuChelles) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AApercuHabillage>, Acteur)
		SLATE_ARGUMENT(const FSlateBrush*, Image)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Acteur = InArgs._Acteur;
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
	Changement(TenueChoisie);
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
		.HAlign(HAlign_Center)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte(Libelle)
			.TaillePolice(22)
			.OnClic(FSimpleDelegate::CreateLambda([Modifier]() { Modifier(1); }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([Modifier]() { Modifier(-1); })))
		];
}

TSharedRef<SWidget> SMenuChelles::EcranHabillage()
{
	if (!bTenueLue)
	{
		TenueChoisie = ChellesHabillage::TenueLocale();
		bTenueLue = true;
	}
	if (!ApercuActeur.IsValid())
	{
		MontrerApercu();
	}
	const int32 SexeActuel = (TenueChoisie.EstSpiderMan() && !TenueChoisie.EstPantera()) ? int32(ChellesHabillage::SpiderMan) : TenueChoisie.Sexe;

	TSharedRef<SVerticalBox> Gauche = SNew(SVerticalBox);
	Gauche->AddSlot().AutoHeight()[ Titre(FText::FromString(TEXT("HABILLAGE")), FText::FromString(TEXT("// SEXE, TENUE ET LOOK DE TON JOUEUR"))) ];

	// SEXE : trois gros boutons
	auto BoutonSexe = [this, SexeActuel](const TCHAR* Nom, int32 Sexe) -> TSharedRef<SWidget>
	{
		const FString Libelle = (SexeActuel == Sexe) ? FString::Printf(TEXT("[ %s ]"), Nom) : FString(Nom);
		return Bouton(FText::FromString(Libelle), 30, [this, Sexe]()
		{
			ModifierTenue([Sexe](ChellesHabillage::FTenue& T)
			{
				if (T.Sexe != Sexe)
				{
					T.Sexe = Sexe;
					T.Modele.Reset();
				}
			});
		});
	};
	Gauche->AddSlot().AutoHeight().HAlign(HAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[ BoutonSexe(TEXT("F\u00C9MININ"), ChellesHabillage::Feminin) ]
		+ SHorizontalBox::Slot().AutoWidth()[ BoutonSexe(TEXT("MASCULIN"), ChellesHabillage::Masculin) ]
		+ SHorizontalBox::Slot().AutoWidth()[ BoutonSexe(TEXT("SPIDER-MALE"), ChellesHabillage::SpiderMan) ]
		+ SHorizontalBox::Slot().AutoWidth()[ BoutonSexe(TEXT("PANTERA"), ChellesHabillage::Pantera) ]
	];

	const ChellesHabillage::FModele* Modele = ChellesHabillage::TrouverModele(TenueChoisie.Modele);
	if (TenueChoisie.EstPantera())
	{
		Gauche->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Tu joues PANTERA : tenue noire, masque noir, yeux blancs.\nIl vole comme Spider-Male, mais 20 secondes maximum : ensuite il doit se poser. Clic gauche = tirs des mains (laser, feu, glace, foudre), O = changer de tir.\nTu commences en PANTERA ; TAB pour changer de mode."))) ];
	}
	else if (TenueChoisie.EstSpiderMan())
	{
		Gauche->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Tu joues SPIDER-MALE, le h\u00E9ros que tu as d\u00E9j\u00E0 dans tout le jeu.\nChoisis F\u00C9MININ ou MASCULIN pour cr\u00E9er ton propre look : visage, cheveux, haut, bas, chaussures, chapeau, lunettes..."))) ];
	}
	else if (!Modele)
	{
		Gauche->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Packs de personnages introuvables dans Content (Action_Male_and_Female, Female_Mannequin). Tu restes en Spider-Male."))) ];
	}
	else
	{
		Gauche->AddSlot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
		[
			ReglageHabillage(TEXT("MOD\u00C8LE"), Modele->Nom, [this](int32 Sens)
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
			})
		];
		if (Modele->bMannequin)
		{
			Gauche->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Mannequin tout fait du pack Female Mannequin : sa tenue ne change pas. Choisis un mod\u00E8le CLASSIQUE, GRANDE ou ATHL\u00C9TIQUE pour habiller ton joueur pi\u00E8ce par pi\u00E8ce."))) ];
		}
		for (const ChellesHabillage::FEmplacement& E : Modele->Emplacements)
		{
			const FString* Actuel = TenueChoisie.Choix.Find(E.Cle);
			const int32 Index = FMath::Max(0, E.IndexDe(Actuel ? *Actuel : FString()));
			const ChellesHabillage::FEmplacement* PE = &E;
			Gauche->AddSlot().AutoHeight()
			[
				ReglageHabillage(E.Titre, E.Noms.IsValidIndex(Index) ? E.Noms[Index] : FString(), [this, PE](int32 Sens)
				{
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
				})
			];
		}
	}

	Gauche->AddSlot().AutoHeight().Padding(FMargin(0.f, 14.f, 0.f, 0.f)).HAlign(HAlign_Center)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Bouton(FText::FromString(TEXT("AL\u00C9ATOIRE")), 30, [this]()
			{
				ModifierTenue([](ChellesHabillage::FTenue& T) { ChellesHabillage::Hasard(T); });
			}, !TenueChoisie.EstSpiderMan() && Modele != nullptr)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			Bouton(FText::FromString(TEXT("VALIDER >")), 30, [this]()
			{
				ChellesHabillage::EnregistrerTenue(TenueChoisie);
				Afficher(EEcranMenu::Mode);
			})
		]
	];
	Gauche->AddSlot().AutoHeight()[ BoutonRetour() ];
	Gauche->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Clic gauche / droite (ou fl\u00E8ches) pour changer. Ton look est vu par les autres joueurs en multi. La roue TAB garde Spider-Male, PANTERA, motoneige, cheval..."))) ];

	TSharedRef<SWidget> Droite = PinceauApercu.IsValid()
		? StaticCastSharedRef<SWidget>(SNew(SZoneApercuChelles).Acteur(ApercuActeur).Image(PinceauApercu.Get()))
		: StaticCastSharedRef<SWidget>(SNew(SBox));

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 40.f, 0.f))
		[
			SNew(SBox).MinDesiredWidth(760.f)[ Gauche ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).WidthOverride(400.f).HeightOverride(600.f)[ Droite ]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("< GLISSE POUR TOURNER >")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12))
				.ColorAndOpacity(Alpha(Magenta, 0.8f))
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
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("REJOINDRE SERVEUR")), 46, [this]()
			{
				Afficher(EEcranMenu::Serveurs);
				if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); }
			}) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("ADRESSE IP SERVEUR")), 46, [this]() { Afficher(EEcranMenu::AdresseIP); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TextePvp(); }, [](int32) { ChellesOptions::ChangerPvp(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("LAN LOCAL")), 46, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("LAN LOCAL = cr\u00E9er ta partie : les autres PC de la maison pourront la rejoindre."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("SEARCH")), 46, [this]()
			{
				Afficher(EEcranMenu::Search);
				if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); }
			}) ]
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
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("ACTUALISER")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); } }, !(M && M->RechercheEnCours())) ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranAdresseIP()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("ADRESSE IP")), FText::FromString(TEXT("// REJOINDRE UN SERVEUR PAR SON ADRESSE"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f))
		[
			SNew(SBox)
			.WidthOverride(560.f)
			[
				SAssignNew(ChampIP, SEditableTextBox)
				.Text(FText::FromString(TexteIP))
				.HintText(FText::FromString(TEXT("ex : 192.168.1.20")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30))
				.ForegroundColor(FSlateColor(FLinearColor::Black))
				.Justification(ETextJustify::Center)
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
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("REJOINDRE")), 46, [this]() { if (AMenuChelles* M = LeMenu()) { M->RejoindreIP(TexteIP); } }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("L'adresse du serveur s'affiche sur son \u00E9cran MULTI-PLAYER. Pour Internet, l'h\u00F4te doit ouvrir le port 7777 (UDP) de sa box."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranSearch()
{
	AMenuChelles* M = LeMenu();
	TSharedRef<SVerticalBox> Corps = SNew(SVerticalBox);

	if (!M || M->RechercheEnCours() || !M->RechercheFaite())
	{
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Je cherche une partie CHELLES 3D sur ton r\u00E9seau local..."))) ];
	}
	else if (M->Parties().Num() > 0)
	{
		const FPartieTrouvee& P = M->Parties()[0];
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(FString::Printf(TEXT("Partie trouv\u00E9e chez %s  (%d/%d joueurs, %d ms)"), *P.Hote, P.Joueurs, P.Places, P.Ping))) ];
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("REJOINDRE")), 52, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->RejoindrePartie(0); } }) ];
		if (M->Parties().Num() > 1)
		{
			Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(FString::Printf(TEXT("VOIR LES %d PARTIES"), M->Parties().Num())), 30, [this]() { Afficher(EEcranMenu::Serveurs); }) ];
		}
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("CR\u00C9ER MA PARTIE")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ];
	}
	else
	{
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Personne ne joue sur ton r\u00E9seau local pour l'instant. Tu peux cr\u00E9er ta partie : les autres la trouveront avec SEARCH. (Recherche sur Internet : prochaine \u00E9tape, avec Steam ou Epic.)"))) ];
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("CR\u00C9ER MA PARTIE")), 46, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ];
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("CHERCHER ENCORE")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); } }) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("SEARCH")), FText::FromString(TEXT("// RECHERCHE AUTOMATIQUE D'UNE PARTIE"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Corps ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::BoutonOnglet(const FText& Texte, EOngletOptions Cible)
{
	return SNew(SBox)
		.Padding(FMargin(4.f, 0.f))
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
	case EOngletOptions::Audio:      Contenu = OngletAudio(); break;
	case EOngletOptions::Jeu:        Contenu = OngletJeu(); break;
	default: break;
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("OPTIONS")), FText::FromString(TEXT("// CLIC GAUCHE = SUIVANT   CLIC DROIT = PR\u00C9C\u00C9DENT   LB / RB / TAB = ONGLET"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 18.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonOnglet(FText::FromString(TEXT("CONTR\u00D4LES")), EOngletOptions::Controles) ]
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonOnglet(FText::FromString(TEXT("GRAPHISMES")), EOngletOptions::Graphismes) ]
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonOnglet(FText::FromString(TEXT("AUDIO")), EOngletOptions::Audio) ]
			+ SHorizontalBox::Slot().AutoWidth()[ BoutonOnglet(FText::FromString(TEXT("JEU")), EOngletOptions::Jeu) ]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.MinDesiredHeight(380.f)
			.VAlign(VAlign_Top)
			[
				Contenu
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
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("QUALIT\u00C9 : BAS pour un PC faible ou un portable (image un peu moins fine, moins d'herbe, ombres et objets moins loin), MOYEN pour un PC moyen, HAUT = tout au maximum."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Enregistr\u00E9 automatiquement (GameUserSettings). Affichage et r\u00E9solution ne changent que dans le jeu lanc\u00E9 seul, pas dans la fen\u00EAtre de l'\u00E9diteur."))) ];
}

TSharedRef<SWidget> SMenuChelles::OngletAudio()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteVolume(); }, [](int32 S) { ChellesOptions::ChangerVolume(S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeGeneral(); }, [](float V) { ChellesOptions::DefinirVolumeGeneral(V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(0); }, [](int32 S) { ChellesOptions::ChangerCanal(0, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeMusique(); }, [](float V) { ChellesOptions::DefinirVolumeCanal(0, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(1); }, [](int32 S) { ChellesOptions::ChangerCanal(1, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeEffets(); }, [](float V) { ChellesOptions::DefinirVolumeCanal(1, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(2); }, [](int32 S) { ChellesOptions::ChangerCanal(2, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeCanal(2); }, [](float V) { ChellesOptions::DefinirVolumeCanal(2, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Reglage([]() { return ChellesOptions::TexteCanal(3); }, [](int32 S) { ChellesOptions::ChangerCanal(3, S); }) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Curseur([]() { return ChellesOptions::VolumeCanal(3); }, [](float V) { ChellesOptions::DefinirVolumeCanal(3, V); }) ]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
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
	const int32 Nombre = ChellesOptions::ListeTouches().Num();
	for (int32 i = 0; i < Nombre; ++i)
	{
		Liste->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 1.f))
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte_Lambda([this, i]() { return ChellesOptions::TexteTouche(i, ToucheEnAttente == i); })
			.TaillePolice(24)
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
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 10.f, 0.f, 0.f))[ Bouton(FText::FromString(TEXT("TOUT REMETTRE PAR D\u00C9FAUT")), 28, []() { ChellesOptions::ToutesLesTouchesParDefaut(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranQuitter()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("QUITTER ?")), FText::FromString(TEXT("// \u00CATES-VOUS S\u00DBR DE VOULOIR QUITTER ?"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("NON, RESTER")), 56, [this]() { Retour(); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("OUI, QUITTER")), 56, [this]() { if (AMenuChelles* M = LeMenu()) { M->Quitter(); } }) ];
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
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Role))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
				.ColorAndOpacity(Alpha(Magenta, 0.9f))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Nom))
				.Font(Police(30, Magenta))
				.ColorAndOpacity(Cyan)
			];
	};

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("CR\u00C9DITS")), FText::FromString(TEXT("// CHELLES 3D  -  ") + ChellesOptions::Version())) ]
		+ SVerticalBox::Slot().AutoHeight()[ LigneCredit(TEXT("CR\u00C9ATION"), TEXT("MERIDJA RACHID")) ]
		+ SVerticalBox::Slot().AutoHeight()[ LigneCredit(TEXT("DESIGN"), TEXT("MERIDJA DYLAN")) ]
		+ SVerticalBox::Slot().AutoHeight()[ LigneCredit(TEXT("MOTEUR"), TEXT("UNREAL ENGINE 4.27")) ]
		+ SVerticalBox::Slot().AutoHeight()[ LigneCredit(TEXT("PACKS UTILIS\u00C9S"), TEXT("FPS GAME  -  VEHICLE GAME  -  MARKETPLACE")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))[ BoutonRetour() ];
}
