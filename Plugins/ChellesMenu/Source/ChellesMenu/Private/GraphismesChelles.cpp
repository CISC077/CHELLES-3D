// CHELLES 3D - OPTIONS > AVANCES : reglages graphiques detailles (25/11/2026, agent Terrain)
#include "GraphismesChelles.h"

#include "SMenuChelles.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Containers/Ticker.h"
#include "DynamicRHI.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "RenderCore.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SNullWidget.h"

namespace ChellesGraphismes
{
	static const TCHAR* Section = TEXT("ChellesGraphismes");

	enum class EType : uint8
	{
		Groupe,     // groupe de qualite Unreal (sg.ShadowQuality...), enregistre par GameUserSettings
		Preset,     // variable aussi reglee par QUALITE BAS/MOYEN/HAUT (priorite "reglage du jeu")
		Variable,   // variable de rendu (priorite "qualite"), AUTO = valeur donnee par la qualite Unreal
		Echelle,    // echelle de l'image (resolution 3D)
		Fps         // compteur d'images par seconde en haut a gauche
	};

	struct FChoix
	{
		const TCHAR* Nom;
		float Valeur;
	};

	struct FLigne
	{
		const TCHAR* Id;
		const TCHAR* Libelle;
		EType Type;
		int32 Groupe;                 // EType::Groupe : 0 ombres, 1 textures, 2 effets, 3 post-process, 4 materiaux, 5 anti-aliasing
		TArray<const TCHAR*> Variables;
		TArray<FChoix> Choix;         // Groupe : un nom par niveau 0..3 ; Preset / Variable : sans AUTO (ajoute devant)
	};

	static const TArray<FLigne>& Lignes()
	{
		static TArray<FLigne> L;
		if (L.Num() > 0)
		{
			return L;
		}
		const TArray<FChoix> Niveaux = { { TEXT("BAS"), 0.f }, { TEXT("MOYEN"), 1.f }, { TEXT("HAUT"), 2.f }, { TEXT("ÉPIQUE"), 3.f } };
		const TArray<FChoix> NonOui = { { TEXT("NON"), 0.f }, { TEXT("OUI"), 1.f } };

		// --- Colonne 1 : ombres, lumiere, eau
		L.Add({ TEXT("Ombres"), TEXT("OMBRES"), EType::Groupe, 0, {},
			{ { TEXT("DÉSACTIVÉES"), 0.f }, { TEXT("BASSES"), 1.f }, { TEXT("MOYENNES"), 2.f }, { TEXT("HAUTES"), 3.f } } });
		L.Add({ TEXT("DistanceOmbres"), TEXT("DISTANCE OMBRES"), EType::Preset, 0, { TEXT("r.Shadow.DistanceScale") },
			{ { TEXT("25 %"), 0.25f }, { TEXT("50 %"), 0.5f }, { TEXT("75 %"), 0.75f }, { TEXT("100 %"), 1.f } } });
		L.Add({ TEXT("Reflets"), TEXT("REFLETS"), EType::Variable, 0, { TEXT("r.SSR.Quality") },
			{ { TEXT("NON"), 0.f }, { TEXT("BAS"), 1.f }, { TEXT("HAUT"), 3.f } } });
		L.Add({ TEXT("Eau"), TEXT("EAU ET VERRE"), EType::Variable, 0, { TEXT("r.RefractionQuality") },
			{ { TEXT("SIMPLES"), 0.f }, { TEXT("RÉALISTES"), 2.f } } });
		L.Add({ TEXT("Effets"), TEXT("EFFETS / FLUIDES"), EType::Groupe, 2, {}, Niveaux });
		L.Add({ TEXT("Brouillard"), TEXT("BROUILLARD 3D"), EType::Variable, 0, { TEXT("r.VolumetricFog") }, NonOui });
		L.Add({ TEXT("Occlusion"), TEXT("OMBRES DOUCES"), EType::Variable, 0, { TEXT("r.AmbientOcclusionLevels") },
			{ { TEXT("NON"), 0.f }, { TEXT("OUI"), 3.f } } });
		L.Add({ TEXT("PostProcess"), TEXT("POST-PROCESS"), EType::Groupe, 3, {}, Niveaux });
		L.Add({ TEXT("Bloom"), TEXT("HALO LUMIÈRES"), EType::Variable, 0, { TEXT("r.BloomQuality") },
			{ { TEXT("NON"), 0.f }, { TEXT("OUI"), 5.f } } });
		L.Add({ TEXT("FlouMouvement"), TEXT("FLOU MOUVEMENT"), EType::Variable, 0, { TEXT("r.MotionBlurQuality") },
			{ { TEXT("NON"), 0.f }, { TEXT("OUI"), 4.f } } });
		L.Add({ TEXT("Profondeur"), TEXT("FLOU DE FOND"), EType::Variable, 0, { TEXT("r.DepthOfFieldQuality") },
			{ { TEXT("NON"), 0.f }, { TEXT("OUI"), 2.f } } });

		// --- Colonne 2 : terrain, distance, image
		L.Add({ TEXT("DistanceVue"), TEXT("DISTANCE DE VUE"), EType::Preset, 0, { TEXT("r.ViewDistanceScale") },
			{ { TEXT("40 %"), 0.4f }, { TEXT("60 %"), 0.6f }, { TEXT("80 %"), 0.8f }, { TEXT("100 %"), 1.f }, { TEXT("150 %"), 1.5f } } });
		L.Add({ TEXT("Vegetation"), TEXT("HERBE / PLANTES"), EType::Preset, 0, { TEXT("foliage.DensityScale"), TEXT("grass.DensityScale") },
			{ { TEXT("AUCUNE"), 0.f }, { TEXT("25 %"), 0.25f }, { TEXT("50 %"), 0.5f }, { TEXT("75 %"), 0.75f }, { TEXT("100 %"), 1.f } } });
		L.Add({ TEXT("DetailObjets"), TEXT("DÉTAIL OBJETS"), EType::Preset, 0, { TEXT("r.StaticMeshLODDistanceScale") },
			{ { TEXT("BAS"), 2.5f }, { TEXT("MOYEN"), 1.5f }, { TEXT("HAUT"), 1.f } } });
		L.Add({ TEXT("DetailPersos"), TEXT("DÉTAIL PERSOS"), EType::Preset, 0, { TEXT("r.SkeletalMeshLODBias") },
			{ { TEXT("BAS"), 2.f }, { TEXT("MOYEN"), 1.f }, { TEXT("HAUT"), 0.f } } });
		L.Add({ TEXT("Textures"), TEXT("TEXTURES"), EType::Groupe, 1, {}, Niveaux });
		L.Add({ TEXT("Materiaux"), TEXT("MATÉRIAUX"), EType::Groupe, 4, {}, Niveaux });
		L.Add({ TEXT("AntiAliasing"), TEXT("ANTI-CRÉNELAGE"), EType::Groupe, 5, {},
			{ { TEXT("NON"), 0.f }, { TEXT("BAS"), 1.f }, { TEXT("MOYEN"), 2.f }, { TEXT("HAUT"), 3.f } } });
		L.Add({ TEXT("SoleilReflet"), TEXT("REFLETS SOLEIL"), EType::Variable, 0, { TEXT("r.LensFlareQuality") },
			{ { TEXT("NON"), 0.f }, { TEXT("OUI"), 2.f } } });
		L.Add({ TEXT("Echelle"), TEXT("NETTETÉ IMAGE"), EType::Echelle, 0, {},
			{ { TEXT("50 %"), 50.f }, { TEXT("60 %"), 60.f }, { TEXT("70 %"), 70.f }, { TEXT("80 %"), 80.f }, { TEXT("90 %"), 90.f }, { TEXT("100 %"), 100.f } } });
		L.Add({ TEXT("Fps"), TEXT("COMPTEUR FPS"), EType::Fps, 0, {}, NonOui });
		return L;
	}

	static FString GMessage;
	static TWeakObjectPtr<UWorld> GMonde;

	static UGameUserSettings* Reglages()
	{
		return GEngine ? GEngine->GetGameUserSettings() : nullptr;
	}

	// ------------------------------------------------ choix enregistres (-1 = AUTO)

	static int32 LireChoix(const FLigne& L)
	{
		int32 V = -1;
		if (GConfig)
		{
			GConfig->GetInt(Section, L.Id, V, GGameUserSettingsIni);
		}
		return (V >= 0 && V < L.Choix.Num()) ? V : -1;
	}

	static void EcrireChoix(const FLigne& L, int32 V)
	{
		if (!GConfig)
		{
			return;
		}
		if (V < 0)
		{
			GConfig->RemoveKey(Section, L.Id, GGameUserSettingsIni);
		}
		else
		{
			GConfig->SetInt(Section, L.Id, V, GGameUserSettingsIni);
		}
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	static int32* ChampGroupe(Scalability::FQualityLevels& Q, int32 Groupe)
	{
		switch (Groupe)
		{
		case 0: return &Q.ShadowQuality;
		case 1: return &Q.TextureQuality;
		case 2: return &Q.EffectsQuality;
		case 3: return &Q.PostProcessQuality;
		case 4: return &Q.ShadingQuality;
		case 5: return &Q.AntiAliasingQuality;
		default: return nullptr;
		}
	}

	// ------------------------------------------------ variables de rendu

	/** Valeurs d'origine (avant tout changement de ce menu), pour revenir a AUTO. */
	static TMap<FString, float>& ValeursOrigine()
	{
		static TMap<FString, float> M;
		return M;
	}

	static void Memoriser()
	{
		static bool bFait = false;
		if (bFait)
		{
			return;
		}
		bFait = true;
		for (const FLigne& L : Lignes())
		{
			for (const TCHAR* Nom : L.Variables)
			{
				if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Nom))
				{
					ValeursOrigine().Add(Nom, V->GetFloat());
				}
			}
		}
	}

	static void Definir(const TCHAR* Nom, float Valeur, EConsoleVariableFlags Priorite)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Nom))
		{
			V->Set(*FString::SanitizeFloat(Valeur), Priorite);
		}
	}

	static bool CompteurVoulu()
	{
		for (const FLigne& L : Lignes())
		{
			if (L.Type == EType::Fps)
			{
				return LireChoix(L) == 1;
			}
		}
		return false;
	}

	// ------------------------------------------------ mesure : temps du jeu (processeur), du rendu, de la carte graphique

	struct FMesure
	{
		float Image = 0.f;  // ms par image
		float Jeu = 0.f;    // ms du fil du jeu (Blueprints, IA, physique...)
		float Rendu = 0.f;  // ms du fil de rendu (preparation des objets a dessiner)
		float Gpu = 0.f;    // ms de la carte graphique
		float Pire = 0.f;   // pire image depuis la derniere ligne [Perf]
	};

	static FMesure GMesure;
	static bool GCompteurEcranActif();

	static void Mesurer()
	{
		static uint64 DerniereImage = 0;
		if (GFrameCounter == DerniereImage)
		{
			return; // une fois par image
		}
		DerniereImage = GFrameCounter;
		const float A = 0.05f; // lissage (appele a chaque image, voir MesurerChaqueImage)
		auto Lisser = [A](float& Valeur, float Nouvelle) { Valeur = (Valeur <= 0.f) ? Nouvelle : FMath::Lerp(Valeur, Nouvelle, A); };
		const float ImageMs = FMath::Max((float)FApp::GetDeltaTime(), 0.0001f) * 1000.f;
		GMesure.Pire = FMath::Max(GMesure.Pire, ImageMs);
		Lisser(GMesure.Image, ImageMs);
		Lisser(GMesure.Jeu, (float)FPlatformTime::ToMilliseconds(GGameThreadTime));
		Lisser(GMesure.Rendu, (float)FPlatformTime::ToMilliseconds(GRenderThreadTime));
		Lisser(GMesure.Gpu, (float)FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	}

	static const TCHAR* Limite()
	{
		const float M = FMath::Max3(GMesure.Jeu, GMesure.Rendu, GMesure.Gpu);
		if (M <= 0.f) { return TEXT("?"); }
		if (M == GMesure.Gpu) { return TEXT("CARTE GRAPHIQUE"); }
		if (M == GMesure.Rendu) { return TEXT("RENDU (trop d'objets)"); }
		return TEXT("JEU (processeur)");
	}

	/** Toutes les 2 s dans le journal (compteur affiche, ou jeu lance par le testeur -ChellesTest). */
	static bool JournalPerf(float)
	{
		if (!GMonde.IsValid())
		{
			return true;
		}
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ChellesTest"));
		if (!bTest && !GCompteurEcranActif())
		{
			return true;
		}
		UE_LOG(LogTemp, Log, TEXT("[Perf] fps=%d image=%.1f pire=%.0f jeu=%.1f rendu=%.1f gpu=%.1f limite=%s"),
			FMath::RoundToInt(1000.f / FMath::Max(GMesure.Image, 0.1f)), GMesure.Image, GMesure.Pire, GMesure.Jeu, GMesure.Rendu, GMesure.Gpu, Limite());
		GMesure.Pire = 0.f;
		return true;
	}

	/** A chaque image (sinon la moyenne lissee ne bougeait qu'une fois toutes les 2 s et gardait la saccade du chargement). */
	static bool MesurerChaqueImage(float)
	{
		static const bool bTest = FParse::Param(FCommandLine::Get(), TEXT("ChellesTest"));
		if (GMonde.IsValid() && (bTest || GCompteurEcranActif()))
		{
			Mesurer();
		}
		return true;
	}

	// ------------------------------------------------ compteur FPS (Slate, marche aussi dans le jeu exporte)

	static TSharedPtr<SWidget> GCompteur;
	static TWeakObjectPtr<UGameViewportClient> GCompteurEcran;

	static bool GCompteurEcranActif()
	{
		return GCompteurEcran.IsValid();
	}

	static void RetirerCompteur()
	{
		if (GCompteur.IsValid() && GCompteurEcran.IsValid())
		{
			GCompteurEcran->RemoveViewportWidgetContent(GCompteur.ToSharedRef());
		}
		GCompteurEcran.Reset();
	}

	static void MontrerCompteur(bool bVoulu)
	{
		RetirerCompteur();
		UWorld* Monde = GMonde.Get();
		UGameViewportClient* Ecran = Monde ? Monde->GetGameViewport() : nullptr;
		if (!bVoulu || !Ecran)
		{
			return;
		}
		if (!GCompteur.IsValid())
		{
			auto FaireTexte = []()
			{
				Mesurer();
				return FText::FromString(FString::Printf(TEXT("%d FPS  |  %.1f ms\nJEU %.1f  RENDU %.1f  GPU %.1f\nfreine : %s"),
					FMath::RoundToInt(1000.f / FMath::Max(GMesure.Image, 0.1f)), GMesure.Image, GMesure.Jeu, GMesure.Rendu, GMesure.Gpu, Limite()));
			};
			auto FaireCouleur = []()
			{
				const float Fps = 1.f / FMath::Max((float)FApp::GetDeltaTime(), 0.0001f);
				return FSlateColor(Fps >= 50.f ? FLinearColor(0.3f, 1.f, 0.4f) : (Fps >= 30.f ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor(1.f, 0.3f, 0.25f)));
			};
			GCompteur = SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(FMargin(12.f, 8.f))
				.Visibility(EVisibility::HitTestInvisible)
				[
					SNew(STextBlock)
					.Text_Lambda(FaireTexte)
					.ColorAndOpacity_Lambda(FaireCouleur)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
					.ShadowOffset(FVector2D(1.f, 1.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				];
		}
		Ecran->AddViewportWidgetContent(GCompteur.ToSharedRef(), 1000);
		GCompteurEcran = Ecran;
	}

	// ------------------------------------------------ application

	/** Remet tout dans l'ordre : origine -> qualite Unreal -> prereglage QUALITE -> choix du joueur. */
	static void ReappliquerTout()
	{
		Memoriser();
		for (const FLigne& L : Lignes())
		{
			const EConsoleVariableFlags Priorite = (L.Type == EType::Preset) ? ECVF_SetByGameSetting : ECVF_SetByScalability;
			for (const TCHAR* Nom : L.Variables)
			{
				if (const float* Origine = ValeursOrigine().Find(Nom))
				{
					Definir(Nom, *Origine, Priorite);
				}
			}
		}
		if (UGameUserSettings* R = Reglages())
		{
			R->ApplyNonResolutionSettings(); // groupes de qualite Unreal (ombres, textures...)
		}
		ChellesOptions::ReappliquerQualite(); // BAS / MOYEN / HAUT
		bool bHerbe = false;
		for (const FLigne& L : Lignes())
		{
			if (L.Type != EType::Preset && L.Type != EType::Variable)
			{
				continue;
			}
			const int32 C = LireChoix(L);
			if (C < 0)
			{
				continue;
			}
			const EConsoleVariableFlags Priorite = (L.Type == EType::Preset) ? ECVF_SetByGameSetting : ECVF_SetByScalability;
			for (const TCHAR* Nom : L.Variables)
			{
				Definir(Nom, L.Choix[C].Valeur, Priorite);
			}
			bHerbe |= FCString::Strcmp(L.Id, TEXT("Vegetation")) == 0;
		}
		if (bHerbe && GEngine)
		{
			GEngine->Exec(GMonde.Get(), TEXT("grass.FlushCache")); // l'herbe du terrain repousse avec la nouvelle densite
		}
	}

	int32 Nombre()
	{
		return Lignes().Num();
	}

	FText Texte(int32 i)
	{
		const TArray<FLigne>& Liste = Lignes();
		if (!Liste.IsValidIndex(i))
		{
			return FText::GetEmpty();
		}
		const FLigne& L = Liste[i];
		FString Valeur;
		UGameUserSettings* R = Reglages();
		switch (L.Type)
		{
		case EType::Groupe:
		{
			int32 N = 3;
			if (R)
			{
				if (int32* Champ = ChampGroupe(R->ScalabilityQuality, L.Groupe))
				{
					N = *Champ;
				}
			}
			Valeur = L.Choix[FMath::Clamp(N, 0, L.Choix.Num() - 1)].Nom;
			break;
		}
		case EType::Echelle:
		{
			float Normal = 1.f, Actuel = 100.f, Mini = 0.f, Maxi = 100.f;
			if (R)
			{
				R->GetResolutionScaleInformationEx(Normal, Actuel, Mini, Maxi);
			}
			Valeur = FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Actuel));
			break;
		}
		default:
		{
			const int32 C = LireChoix(L);
			Valeur = (C < 0) ? FString(L.Type == EType::Fps ? TEXT("NON") : TEXT("AUTO")) : FString(L.Choix[C].Nom);
			break;
		}
		}
		return FText::FromString(FString::Printf(TEXT("%s   < %s >"), L.Libelle, *Valeur));
	}

	void Changer(int32 i, int32 Sens)
	{
		const TArray<FLigne>& Liste = Lignes();
		if (!Liste.IsValidIndex(i))
		{
			return;
		}
		const FLigne& L = Liste[i];
		const int32 S = Sens < 0 ? -1 : 1;
		UGameUserSettings* R = Reglages();
		GMessage.Empty();
		switch (L.Type)
		{
		case EType::Groupe:
			if (R)
			{
				if (int32* Champ = ChampGroupe(R->ScalabilityQuality, L.Groupe))
				{
					const int32 N = L.Choix.Num();
					*Champ = ((FMath::Clamp(*Champ, 0, N - 1) + S) % N + N) % N;
					R->ApplyNonResolutionSettings();
					R->SaveSettings();
					ReappliquerTout(); // les choix fins passent apres la qualite Unreal
				}
			}
			break;
		case EType::Echelle:
			if (R)
			{
				float Normal = 1.f, Actuel = 100.f, Mini = 0.f, Maxi = 100.f;
				R->GetResolutionScaleInformationEx(Normal, Actuel, Mini, Maxi);
				int32 Index = L.Choix.Num() - 1;
				float Ecart = 1000.f;
				for (int32 k = 0; k < L.Choix.Num(); ++k)
				{
					if (FMath::Abs(L.Choix[k].Valeur - Actuel) < Ecart)
					{
						Ecart = FMath::Abs(L.Choix[k].Valeur - Actuel);
						Index = k;
					}
				}
				const int32 N = L.Choix.Num();
				Index = ((Index + S) % N + N) % N;
				R->SetResolutionScaleValueEx(L.Choix[Index].Valeur);
				R->ApplySettings(false);
			}
			break;
		case EType::Fps:
		{
			const bool bOui = LireChoix(L) != 1;
			EcrireChoix(L, bOui ? 1 : 0);
			MontrerCompteur(bOui);
			break;
		}
		default:
		{
			// AUTO (-1) puis chaque choix, en boucle
			const int32 N = L.Choix.Num() + 1;
			const int32 Actuel = LireChoix(L) + 1;
			const int32 Nouveau = ((Actuel + S) % N + N) % N;
			EcrireChoix(L, Nouveau - 1);
			ReappliquerTout();
			if (FCString::Strcmp(L.Id, TEXT("Vegetation")) == 0)
			{
				GMessage = TEXT("Herbe / plantes : l'herbe du terrain change tout de suite, les plantes posées à la main au prochain lancement.");
			}
			break;
		}
		}
	}

	void AppliquerAuDemarrage(UWorld* Monde)
	{
		GMonde = Monde;
		static bool bJournal = false;
		if (!bJournal)
		{
			bJournal = true;
			FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&MesurerChaqueImage), 0.f);
			FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&JournalPerf), 2.f);
		}
		ReappliquerTout();
		MontrerCompteur(CompteurVoulu());
	}

	void ToutEnAuto()
	{
		for (const FLigne& L : Lignes())
		{
			if (L.Type == EType::Preset || L.Type == EType::Variable)
			{
				EcrireChoix(L, -1);
			}
		}
		ReappliquerTout();
		GMessage = TEXT("Tous les réglages suivent maintenant la QUALITÉ (onglet GRAPHISMES).");
	}

	void QualiteChangee()
	{
		ToutEnAuto();
		GMessage.Empty();
	}

	FString DetecterMonPC()
	{
		UGameUserSettings* R = Reglages();
		if (!R)
		{
			return FString();
		}
		R->RunHardwareBenchmark();
		R->ApplyHardwareBenchmarkResults(); // groupes de qualite + nettete, enregistres
		const Scalability::FQualityLevels& Q = R->ScalabilityQuality;
		const float Moyenne = (Q.ShadowQuality + Q.TextureQuality + Q.EffectsQuality + Q.PostProcessQuality + Q.ViewDistanceQuality + Q.FoliageQuality) / 6.f;
		const int32 Qualite = Moyenne < 1.5f ? 0 : (Moyenne < 2.5f ? 1 : 2);
		if (GConfig)
		{
			GConfig->SetFloat(TEXT("ChellesMenu"), TEXT("Qualite"), (float)Qualite, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
		ToutEnAuto();
		static const TCHAR* Noms[] = { TEXT("BAS"), TEXT("MOYEN"), TEXT("HAUT") };
		GMessage = FString::Printf(TEXT("Ton PC a été testé : QUALITÉ %s choisie (réglages ajustés à ta carte graphique)."), Noms[Qualite]);
		return GMessage;
	}

	FString Message()
	{
		return GMessage;
	}
}
