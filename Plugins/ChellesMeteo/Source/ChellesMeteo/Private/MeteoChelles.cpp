// CHELLES 3D - Meteo du quartier (UE 4.27)
#include "MeteoChelles.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* CheminGoodSkyChelles = TEXT("/Game/Chelles/CycleJourNuit/BP_GoodSky_Chelles.BP_GoodSky_Chelles_C");
	const TCHAR* CheminGoodSky = TEXT("/Game/GoodSky/Blueprint/BP_GoodSky.BP_GoodSky_C");
	const TCHAR* CheminSapin = TEXT("/Game/Christmas_pack/Blueprints/Prefabs/BP_Christmas_tree.BP_Christmas_tree_C");
	const TCHAR* CheminGuirlande1 = TEXT("/Game/Christmas_pack/Blueprints/BP_decoration_tree_garland_variation_1.BP_decoration_tree_garland_variation_1_C");
	const TCHAR* CheminGuirlande2 = TEXT("/Game/Christmas_pack/Blueprints/BP_decoration_tree_garland_variation_2.BP_decoration_tree_garland_variation_2_C");

	// Chelles (Seine-et-Marne)
	const float Latitude = 48.88f;
	const float Longitude = 2.59f;

	FString Dossier()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesMeteo"));
	}

	void Message(const FString& Texte, bool bEcran = false)
	{
		UE_LOG(LogTemp, Log, TEXT("[Meteo] %s"), *Texte);
		if (bEcran && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, TEXT("Meteo : ") + Texte);
		}
	}

	bool Oui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("True"), ESearchCase::IgnoreCase) || V == TEXT("1");
	}

	float Nombre(const FString& V)
	{
		return FCString::Atof(*V.Replace(TEXT(","), TEXT(".")));
	}

	/** "0.03,0.12,0.75" -> couleur */
	bool LireCouleur(const FString& V, FLinearColor& Sortie)
	{
		TArray<FString> P;
		V.ParseIntoArray(P, TEXT(","), true);
		if (P.Num() < 3)
		{
			return false;
		}
		Sortie = FLinearColor(FCString::Atof(*P[0].TrimStartAndEnd()), FCString::Atof(*P[1].TrimStartAndEnd()),
			FCString::Atof(*P[2].TrimStartAndEnd()), 1.f);
		return true;
	}

	/** "17:30" ou "17.5" -> 17.5 ; "Auto" -> -1 */
	float LireHeure(const FString& V)
	{
		if (V.IsEmpty() || V.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
		{
			return -1.f;
		}
		FString H, M;
		if (V.Split(TEXT(":"), &H, &M) || V.Split(TEXT("h"), &H, &M))
		{
			return FMath::Clamp(FCString::Atof(*H) + FCString::Atof(*M) / 60.f, 0.f, 24.f);
		}
		return FMath::Clamp(Nombre(V), 0.f, 24.f);
	}

	/** "15/11" -> 1115 (MMJJ) */
	int32 LireDate(const FString& V, int32 ParDefaut)
	{
		FString J, M;
		if (!V.Split(TEXT("/"), &J, &M))
		{
			return ParDefaut;
		}
		const int32 Jour = FMath::Clamp(FCString::Atoi(*J), 1, 31);
		const int32 Mois = FMath::Clamp(FCString::Atoi(*M), 1, 12);
		return Mois * 100 + Jour;
	}

	float Lisse(float A, float B, float X)
	{
		if (B <= A)
		{
			return X >= B ? 1.f : 0.f;
		}
		const float T = FMath::Clamp((X - A) / (B - A), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float Luminance(const FLinearColor& C)
	{
		return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
	}

	/** Meme luminosite (x Facteur), teinte de Cible. */
	FLinearColor Teinter(const FLinearColor& C, const FLinearColor& Cible, float Facteur, float Minimum)
	{
		const float Lum = FMath::Max(Luminance(C) * Facteur, Minimum);
		const float K = Lum / FMath::Max(Luminance(Cible), 1e-4f);
		return FLinearColor(Cible.R * K, Cible.G * K, Cible.B * K, C.A);
	}

	FProperty* Propriete(UClass* Classe, const TCHAR* Morceau, bool bExact = false)
	{
		if (!Classe)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> P(Classe); P; ++P)
		{
			const FString Nom = P->GetName();
			if (bExact ? Nom.Equals(Morceau, ESearchCase::IgnoreCase) : Nom.Contains(Morceau, ESearchCase::IgnoreCase))
			{
				return *P;
			}
		}
		return nullptr;
	}

	bool EcrireFloat(UObject* Objet, const TCHAR* Morceau, float Valeur)
	{
		if (FFloatProperty* P = CastField<FFloatProperty>(Propriete(Objet ? Objet->GetClass() : nullptr, Morceau)))
		{
			P->SetPropertyValue_InContainer(Objet, Valeur);
			return true;
		}
		return false;
	}

	int32 EcrireBool(UObject* Objet, const TCHAR* Morceau, bool Valeur, bool bExact)
	{
		int32 N = 0;
		if (!Objet)
		{
			return 0;
		}
		for (TFieldIterator<FBoolProperty> P(Objet->GetClass()); P; ++P)
		{
			const FString Nom = P->GetName();
			if (bExact ? Nom.Equals(Morceau, ESearchCase::IgnoreCase) : Nom.Contains(Morceau, ESearchCase::IgnoreCase))
			{
				P->SetPropertyValue_InContainer(Objet, Valeur);
				++N;
			}
		}
		return N;
	}

	/** SkyPreset = "Custom Mode ( From Time Of Day )" */
	bool ReglerPresetCustom(UObject* Objet)
	{
		FProperty* P = Propriete(Objet->GetClass(), TEXT("SkyPreset"));
		UEnum* Enum = nullptr;
		FNumericProperty* Valeur = nullptr;
		if (FEnumProperty* PE = CastField<FEnumProperty>(P))
		{
			Enum = PE->GetEnum();
			Valeur = PE->GetUnderlyingProperty();
		}
		else if (FByteProperty* PB = CastField<FByteProperty>(P))
		{
			Enum = PB->Enum;
			Valeur = PB;
		}
		if (!Enum || !Valeur)
		{
			return false;
		}
		for (int32 i = 0; i < Enum->NumEnums(); ++i)
		{
			if (Enum->GetDisplayNameTextByIndex(i).ToString().Contains(TEXT("Custom Mode")))
			{
				Valeur->SetIntPropertyValue(P->ContainerPtrToValuePtr<void>(Objet), (int64)Enum->GetValueByIndex(i));
				return true;
			}
		}
		return false;
	}

	bool EstCourbeDeCiel(const FString& Nom)
	{
		return Nom.StartsWith(TEXT("Curve_")) && (Nom.Contains(TEXT("Dome")) || Nom.Contains(TEXT("Horizon"))
			|| Nom.Contains(TEXT("Overlay")) || Nom.Contains(TEXT("Cloud")) || Nom.Contains(TEXT("BackGround")));
	}

	const FLinearColor CouleursLumieres[] =
	{
		FLinearColor(1.f, 0.05f, 0.05f),   // rouge
		FLinearColor(0.1f, 1.f, 0.15f),    // vert
		FLinearColor(0.15f, 0.3f, 1.f),    // bleu
		FLinearColor(1.f, 0.7f, 0.1f),     // or
		FLinearColor(1.f, 0.2f, 0.8f),     // rose
		FLinearColor(0.1f, 0.9f, 1.f),     // cyan
	};
}

AMeteoChelles::AMeteoChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

// ---------------------------------------------------------------- Reglages

void AMeteoChelles::LireReglages()
{
	Reglages = FReglagesMeteo();
	TArray<FString> Lignes;
	const FString Chemin = FPaths::Combine(Dossier(), TEXT("Meteo.txt"));
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		Message(Chemin + TEXT(" introuvable : reglages par defaut."));
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		FString Cle, V;
		if (!Ligne.Split(TEXT("="), &Cle, &V))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		V.TrimStartAndEndInline();
		FReglagesMeteo& R = Reglages;

		if (Cle == TEXT("Actif")) { R.bActif = Oui(V); }
		else if (Cle == TEXT("Mode")) { R.Mode = V; }
		else if (Cle == TEXT("DureeJourneeMinutes")) { R.DureeJourneeMinutes = FMath::Max(1.f, Nombre(V)); }
		else if (Cle == TEXT("HeureFixe")) { R.HeureFixe = FMath::Max(0.f, LireHeure(V)); }
		else if (Cle == TEXT("Lever")) { R.LeverFixe = LireHeure(V); }
		else if (Cle == TEXT("Coucher")) { R.CoucherFixe = LireHeure(V); }
		else if (Cle == TEXT("BleuDebut")) { R.BleuDebut = Nombre(V); }
		else if (Cle == TEXT("BleuPlein")) { R.BleuPlein = Nombre(V); }
		else if (Cle == TEXT("OrangeDebut")) { R.OrangeDebut = Nombre(V); }
		else if (Cle == TEXT("OrangePlein")) { R.OrangePlein = Nombre(V); }
		else if (Cle == TEXT("OrangeFinMatin")) { R.OrangeFinMatin = Nombre(V); }
		else if (Cle == TEXT("BleuNuit")) { LireCouleur(V, R.BleuNuit); }
		else if (Cle == TEXT("OrangeNuit")) { LireCouleur(V, R.OrangeNuit); }
		else if (Cle == TEXT("LuminositeBleu")) { R.LuminositeBleu = FMath::Max(0.f, Nombre(V)); }
		else if (Cle == TEXT("LuminositeOrange")) { R.LuminositeOrange = FMath::Max(0.f, Nombre(V)); }
		else if (Cle == TEXT("LuminositeMinBleu")) { R.LuminositeMinBleu = FMath::Max(0.f, Nombre(V)); }
		else if (Cle == TEXT("LuminositeMinOrange")) { R.LuminositeMinOrange = FMath::Max(0.f, Nombre(V)); }
		else if (Cle == TEXT("BrouillardBleu")) { LireCouleur(V, R.BrouillardBleu); }
		else if (Cle == TEXT("BrouillardOrange")) { LireCouleur(V, R.BrouillardOrange); }
		else if (Cle == TEXT("AmbianceNuit")) { R.AmbianceNuit = FMath::Clamp(Nombre(V), 0.f, 2.f); }
		else if (Cle == TEXT("TailleCiel")) { R.TailleCiel = V.Equals(TEXT("Auto"), ESearchCase::IgnoreCase) ? -1.f : FMath::Max(0.1f, Nombre(V)); }
		else if (Cle == TEXT("Neige")) { R.Neige = V; }
		else if (Cle == TEXT("NeigeDebut")) { R.NeigeDebut = LireDate(V, R.NeigeDebut); }
		else if (Cle == TEXT("NeigeFin")) { R.NeigeFin = LireDate(V, R.NeigeFin); }
		else if (Cle == TEXT("NeigeEffet")) { R.NeigeEffet = V; }
		else if (Cle == TEXT("NeigeGrille")) { R.NeigeGrille = FMath::Clamp(FCString::Atoi(*V), 1, 5); }
		else if (Cle == TEXT("NeigeEcart")) { R.NeigeEcart = FMath::Clamp(Nombre(V), 1.f, 200.f); }
		else if (Cle == TEXT("NeigeHauteur")) { R.NeigeHauteur = FMath::Clamp(Nombre(V), 0.f, 500.f); }
		else if (Cle == TEXT("NeigeTaille")) { R.NeigeTaille = FMath::Clamp(Nombre(V), 0.1f, 20.f); }
		else if (Cle == TEXT("Sapins")) { R.Sapins = V; }
		else if (Cle == TEXT("SapinsDebut")) { R.SapinsDebut = LireDate(V, R.SapinsDebut); }
		else if (Cle == TEXT("SapinsFin")) { R.SapinsFin = LireDate(V, R.SapinsFin); }
		else if (Cle == TEXT("SapinsAuto")) { R.SapinsAuto = FMath::Clamp(FCString::Atoi(*V), 0, 200); }
		else if (Cle == TEXT("SapinsEcart")) { R.SapinsEcart = FMath::Clamp(Nombre(V), 2.f, 1000.f); }
		else if (Cle == TEXT("SapinsRayon")) { R.SapinsRayon = FMath::Clamp(Nombre(V), 10.f, 5000.f); }
		else if (Cle == TEXT("TailleSapin")) { R.TailleSapin = FMath::Clamp(Nombre(V), 0.1f, 20.f); }
		else if (Cle == TEXT("LumieresParSapin")) { R.LumieresParSapin = FMath::Clamp(FCString::Atoi(*V), 0, 12); }
		else if (Cle == TEXT("PuissanceLumieres")) { R.PuissanceLumieres = FMath::Clamp(Nombre(V), 0.f, 100000.f); }
		else if (Cle == TEXT("DistanceLumieres")) { R.DistanceLumieres = FMath::Clamp(Nombre(V), 5.f, 2000.f); }
		else if (Cle == TEXT("Clignotement")) { R.Clignotement = FMath::Clamp(Nombre(V), 0.f, 10.f); }
	}
}

// ---------------------------------------------------------------- Demarrage

void AMeteoChelles::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	if (!Reglages.bActif)
	{
		Message(TEXT("Actif=Non dans Meteo.txt : rien n'est change."));
		SetActorTickEnabled(false);
		return;
	}
	Demarrer();
}

void AMeteoChelles::Demarrer()
{
	const FDateTime Maintenant = FDateTime::Now();
	Depart = Maintenant.GetHour() + Maintenant.GetMinute() / 60.f;
	CalculerSoleil();

	if (GetNetMode() != NM_DedicatedServer)
	{
		PreparerCiel();
	}
	MettreAJourSapins();
	ProchainEtat = 0.f;

	Message(FString::Printf(TEXT("Pret. Mode %s, lever %02d:%02d, coucher %02d:%02d, ciel %s. Console : Chelles.Meteo Info"),
		*Reglages.Mode,
		FMath::FloorToInt(Lever), FMath::FloorToInt(FMath::Fmod(Lever, 1.f) * 60.f),
		FMath::FloorToInt(Coucher), FMath::FloorToInt(FMath::Fmod(Coucher, 1.f) * 60.f),
		Ciel ? (bCielCree ? TEXT("GoodSky ajoute") : TEXT("GoodSky de la map")) : TEXT("introuvable (brouillard seulement)")));
}

void AMeteoChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	// Fin de partie : le monde emporte tout seul ce qu'on a cree. On ne nettoie que si la meteo est retiree en cours de jeu.
	if (Raison != EEndPlayReason::Destroyed)
	{
		Super::EndPlay(Raison);
		return;
	}
	RetirerTousLesSapins();
	for (UParticleSystemComponent* F : Flocons)
	{
		if (F)
		{
			F->DestroyComponent();
		}
	}
	Flocons.Reset();
	if (bCielCree && Ciel)
	{
		Ciel->Destroy();
	}
	Ciel = nullptr;
	Super::EndPlay(Raison);
}

// ---------------------------------------------------------------- Heure et soleil

void AMeteoChelles::CalculerSoleil()
{
	// Lever / coucher du soleil a Chelles pour la date du PC (formules NOAA simplifiees)
	const FDateTime Maintenant = FDateTime::Now();
	const FTimespan Decalage = Maintenant - FDateTime::UtcNow();
	const float DecalageMinutes = FMath::RoundToFloat(Decalage.GetTotalMinutes() / 15.f) * 15.f;

	const float Gamma = 2.f * PI / 365.f * (Maintenant.GetDayOfYear() - 1);
	const float EqTemps = 229.18f * (0.000075f + 0.001868f * FMath::Cos(Gamma) - 0.032077f * FMath::Sin(Gamma)
		- 0.014615f * FMath::Cos(2.f * Gamma) - 0.040849f * FMath::Sin(2.f * Gamma));
	const float Decl = 0.006918f - 0.399912f * FMath::Cos(Gamma) + 0.070257f * FMath::Sin(Gamma)
		- 0.006758f * FMath::Cos(2.f * Gamma) + 0.000907f * FMath::Sin(2.f * Gamma)
		- 0.002697f * FMath::Cos(3.f * Gamma) + 0.00148f * FMath::Sin(3.f * Gamma);
	const float Lat = FMath::DegreesToRadians(Latitude);
	const float CosH = FMath::Cos(FMath::DegreesToRadians(90.833f)) / (FMath::Cos(Lat) * FMath::Cos(Decl)) - FMath::Tan(Lat) * FMath::Tan(Decl);
	const float AngleHoraire = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosH, -1.f, 1.f)));

	const float LeverMin = 720.f - 4.f * (Longitude + AngleHoraire) - EqTemps + DecalageMinutes;
	const float CoucherMin = 720.f - 4.f * (Longitude - AngleHoraire) - EqTemps + DecalageMinutes;

	Lever = Reglages.LeverFixe >= 0.f ? Reglages.LeverFixe : FMath::Clamp(LeverMin / 60.f, 3.f, 11.f);
	Coucher = Reglages.CoucherFixe >= 0.f ? Reglages.CoucherFixe : FMath::Clamp(CoucherMin / 60.f, 15.f, 23.f);
	if (Coucher <= Lever + 1.f)
	{
		Coucher = Lever + 1.f;
	}
}

float AMeteoChelles::CalculerHeure() const
{
	if (HeureForcee >= 0.f)
	{
		return HeureForcee;
	}
	if (Reglages.Mode.Equals(TEXT("Fixe"), ESearchCase::IgnoreCase))
	{
		return FMath::Fmod(Reglages.HeureFixe, 24.f);
	}
	if (Reglages.Mode.Equals(TEXT("Accelere"), ESearchCase::IgnoreCase))
	{
		const UWorld* Monde = GetWorld();
		const float Secondes = Monde ? Monde->GetRealTimeSeconds() : 0.f;
		return FMath::Fmod(Depart + Secondes / (Reglages.DureeJourneeMinutes * 60.f) * 24.f, 24.f);
	}
	const FDateTime Maintenant = FDateTime::Now();
	return Maintenant.GetHour() + Maintenant.GetMinute() / 60.f + Maintenant.GetSecond() / 3600.f;
}

/** Heure reelle -> heure du ciel GoodSky (dans GoodSky le soleil se leve a 6h et se couche a 18h). */
float AMeteoChelles::HeureCiel(float H) const
{
	const float Midi = (Lever + Coucher) * 0.5f;
	const float R[5] = { 0.f, Lever, Midi, Coucher, 24.f };
	const float C[5] = { 0.f, 6.f, 12.f, 18.f, 24.f };
	H = FMath::Clamp(H, 0.f, 24.f);
	for (int32 i = 0; i < 4; ++i)
	{
		if (H <= R[i + 1])
		{
			return FMath::Lerp(C[i], C[i + 1], (H - R[i]) / FMath::Max(R[i + 1] - R[i], 1e-3f));
		}
	}
	return 24.f;
}

float AMeteoChelles::HeureReelleDepuisCiel(float T) const
{
	const float Midi = (Lever + Coucher) * 0.5f;
	const float R[5] = { 0.f, Lever, Midi, Coucher, 24.f };
	const float C[5] = { 0.f, 6.f, 12.f, 18.f, 24.f };
	T = FMath::Clamp(T, 0.f, 24.f);
	for (int32 i = 0; i < 4; ++i)
	{
		if (T <= C[i + 1])
		{
			return FMath::Lerp(R[i], R[i + 1], (T - C[i]) / (C[i + 1] - C[i]));
		}
	}
	return 24.f;
}

/** Poids du bleu nuit et de l'orange (0 a 1) pour une heure reelle. */
void AMeteoChelles::Poids(float H, float& Bleu, float& Orange) const
{
	const float Nuit = FMath::Fmod(Lever - Coucher + 24.f, 24.f) * 60.f;          // duree de la nuit (minutes)
	const float S = FMath::Fmod(H - Coucher + 24.f, 24.f) * 60.f;                  // minutes depuis le coucher
	if (S > Nuit + 60.f)
	{
		Bleu = Orange = 0.f;
		return;
	}
	Bleu = Lisse(Reglages.BleuDebut, Reglages.BleuPlein, S) * (1.f - Lisse(Nuit - 30.f, Nuit + 15.f, S));
	const float FinOrange = Nuit - Reglages.OrangeFinMatin;
	Orange = Lisse(Reglages.OrangeDebut, Reglages.OrangePlein, S) * (1.f - Lisse(FinOrange, FinOrange + 45.f, S));
}

bool AMeteoChelles::PeriodeActive(const FString& Reglage, int32 Debut, int32 Fin) const
{
	if (ForceHiver >= 0)
	{
		return ForceHiver == 1;
	}
	if (Oui(Reglage))
	{
		return true;
	}
	if (Reglage.Equals(TEXT("Non"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	const FDateTime Maintenant = FDateTime::Now();
	const int32 Aujourdhui = Maintenant.GetMonth() * 100 + Maintenant.GetDay();
	return Debut <= Fin ? (Aujourdhui >= Debut && Aujourdhui <= Fin) : (Aujourdhui >= Debut || Aujourdhui <= Fin);
}

// ---------------------------------------------------------------- Ciel

void AMeteoChelles::PreparerCiel()
{
	UWorld* Monde = GetWorld();
	ADirectionalLight* Lumiere = nullptr;
	for (TActorIterator<AActor> It(Monde); It; ++It)
	{
		AActor* A = *It;
		if (!Lumiere)
		{
			if (ADirectionalLight* L = Cast<ADirectionalLight>(A))
			{
				Lumiere = L;
			}
		}
		if (!Brouillard.IsValid())
		{
			if (AExponentialHeightFog* F = Cast<AExponentialHeightFog>(A))
			{
				Brouillard = F->GetComponent();
			}
		}
		if (!Ambiance.IsValid())
		{
			if (ASkyLight* S = Cast<ASkyLight>(A))
			{
				Ambiance = S->GetLightComponent();
			}
		}
	}

	if (Lumiere)
	{
		if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Lumiere->GetLightComponent()))
		{
			Soleil = C;
			if (USceneComponent* Racine = Lumiere->GetRootComponent())
			{
				if (Racine->Mobility != EComponentMobility::Movable)
				{
					Racine->SetMobility(EComponentMobility::Movable);
				}
			}
			if (C->Mobility != EComponentMobility::Movable)
			{
				C->SetMobility(EComponentMobility::Movable);
			}
		}
	}
	if (UExponentialHeightFogComponent* F = Brouillard.Get())
	{
		BrouillardOrigine = F->FogInscatteringColor;
		BrouillardDirectionnelOrigine = F->DirectionalInscatteringColor;
	}
	if (USkyLightComponent* S = Ambiance.Get())
	{
		AmbianceOrigine = S->Intensity;
		if (S->Mobility == EComponentMobility::Static)
		{
			Ambiance = nullptr; // une SkyLight Static ne peut pas changer en jeu
		}
	}

	FabriquerCourbes();
	Ciel = TrouverOuCreerGoodSky(Lumiere);
	AppliquerHeure(0.f);
}

void AMeteoChelles::FabriquerCourbes()
{
	// Courbes d'origine du pack (BP_GoodSky, pas la copie orangee) : on les teinte selon l'heure reelle
	if (CourbesOrigine.Num() == 0)
	{
		UClass* Pack = LoadClass<AActor>(nullptr, CheminGoodSky, nullptr, LOAD_NoWarn);
		UObject* Defaut = Pack ? Pack->GetDefaultObject() : nullptr;
		if (!Defaut)
		{
			return;
		}
		for (TFieldIterator<FObjectProperty> P(Pack); P; ++P)
		{
			if (!EstCourbeDeCiel(P->GetName()))
			{
				continue;
			}
			if (UCurveLinearColor* Courbe = Cast<UCurveLinearColor>(P->GetObjectPropertyValue_InContainer(Defaut)))
			{
				CourbesOrigine.Add(Courbe);
				NomsCourbes.Add(P->GetFName());
				CourbesMeteo.Add(NewObject<UCurveLinearColor>(this));
			}
		}
	}

	const int32 Jour = FDateTime::Now().GetDayOfYear();
	JourCourbes = Jour;
	for (int32 i = 0; i < CourbesOrigine.Num(); ++i)
	{
		UCurveLinearColor* Origine = CourbesOrigine[i];
		UCurveLinearColor* Meteo = CourbesMeteo[i];
		for (int32 k = 0; k < 4; ++k)
		{
			Meteo->FloatCurves[k].Reset();
		}
		for (float T = 0.f; T <= 24.001f; T += 0.25f)
		{
			const FLinearColor C = Origine->GetLinearColorValue(T);
			float B = 0.f, O = 0.f;
			Poids(HeureReelleDepuisCiel(T), B, O);
			FLinearColor F = C + (Teinter(C, Reglages.BleuNuit, Reglages.LuminositeBleu, Reglages.LuminositeMinBleu) - C) * B;
			F = F + (Teinter(C, Reglages.OrangeNuit, Reglages.LuminositeOrange, Reglages.LuminositeMinOrange) - F) * O;
			F.A = C.A;
			const float V[4] = { F.R, F.G, F.B, F.A };
			for (int32 k = 0; k < 4; ++k)
			{
				const FKeyHandle Cle = Meteo->FloatCurves[k].AddKey(T, V[k]);
				Meteo->FloatCurves[k].SetKeyInterpMode(Cle, RCIM_Linear);
			}
		}
	}
	if (Ciel)
	{
		for (int32 i = 0; i < NomsCourbes.Num(); ++i)
		{
			if (FObjectProperty* P = CastField<FObjectProperty>(Propriete(Ciel->GetClass(), *NomsCourbes[i].ToString(), true)))
			{
				P->SetObjectPropertyValue_InContainer(Ciel, CourbesMeteo[i]);
			}
		}
	}
}

AActor* AMeteoChelles::TrouverOuCreerGoodSky(ADirectionalLight* Lumiere)
{
	UWorld* Monde = GetWorld();
	AActor* Trouve = nullptr;
	for (TActorIterator<AActor> It(Monde); It; ++It)
	{
		if (*It != this && It->GetClass()->GetName().Contains(TEXT("GoodSky")))
		{
			Trouve = *It;
			break;
		}
	}

	auto Regler = [this, Lumiere](AActor* A)
	{
		ReglerPresetCustom(A);
		EcrireBool(A, TEXT("Enable Time of Day"), true, true);
		EcrireBool(A, TEXT("Auto Day / Night Cycle"), true, false);
		// journee tres longue : c'est la meteo qui donne l'heure a chaque image
		EcrireFloat(A, TEXT("How long is a day"), 10000000.f);
		EcrireFloat(A, TEXT("Time of Day ( For Custom Mode )"), HeureCiel(CalculerHeure()));
		if (Lumiere)
		{
			if (FObjectProperty* P = CastField<FObjectProperty>(Propriete(A->GetClass(), TEXT("Directional light actor"))))
			{
				if (Lumiere->IsA(P->PropertyClass))
				{
					P->SetObjectPropertyValue_InContainer(A, Lumiere);
				}
			}
		}
		for (int32 i = 0; i < NomsCourbes.Num(); ++i)
		{
			if (FObjectProperty* P = CastField<FObjectProperty>(Propriete(A->GetClass(), *NomsCourbes[i].ToString(), true)))
			{
				P->SetObjectPropertyValue_InContainer(A, CourbesMeteo[i]);
			}
		}
	};

	if (Trouve)
	{
		Regler(Trouve);
		bCielCree = false;
		return Trouve;
	}

	UClass* Classe = LoadClass<AActor>(nullptr, CheminGoodSkyChelles, nullptr, LOAD_NoWarn);
	if (!Classe)
	{
		Classe = LoadClass<AActor>(nullptr, CheminGoodSky, nullptr, LOAD_NoWarn);
	}
	if (!Classe)
	{
		Message(TEXT("Pack GoodSky introuvable : seul le brouillard changera de couleur."));
		return nullptr;
	}

	auto Creer = [&](float Echelle) -> AActor*
	{
		const FTransform Place(FRotator::ZeroRotator, FVector::ZeroVector, FVector(Echelle));
		AActor* A = Monde->SpawnActorDeferred<AActor>(Classe, Place, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A)
		{
			return nullptr;
		}
		Regler(A);
		A->FinishSpawning(Place);
		return A;
	};

	float Echelle = Reglages.TailleCiel > 0.f ? Reglages.TailleCiel : 1.f;
	AActor* A = Creer(Echelle);
	if (A && Reglages.TailleCiel <= 0.f)
	{
		// Taille auto : le ciel doit englober tout le quartier (comme Tools/CycleJourNuit)
		float Rayon = 0.f;
		for (TActorIterator<AActor> It(Monde); It; ++It)
		{
			if (*It == A || *It == this || It->IsA<AExponentialHeightFog>() || It->IsA<ASkyLight>())
			{
				continue;
			}
			const FBox Boite = It->GetComponentsBoundingBox(false);
			if (!Boite.IsValid)
			{
				continue;
			}
			const FVector Ext = Boite.GetExtent();
			if (Ext.X > 2.0e6f || Ext.Y > 2.0e6f)
			{
				continue;
			}
			const FVector Centre = Boite.GetCenter();
			Rayon = FMath::Max(Rayon, FMath::Sqrt(Centre.X * Centre.X + Centre.Y * Centre.Y) + FMath::Max(Ext.X, Ext.Y));
		}
		const FBox BoiteCiel = A->GetComponentsBoundingBox(true);
		const float RayonCiel = BoiteCiel.IsValid ? FMath::Max(BoiteCiel.GetExtent().X, BoiteCiel.GetExtent().Y) : 0.f;
		if (RayonCiel > 1.f && Rayon > 0.f)
		{
			const float Voulu = FMath::Max(1.f, Rayon * 1.3f / RayonCiel);
			if (Voulu > 1.05f)
			{
				A->Destroy();
				A = Creer(Voulu);
				Echelle = Voulu;
			}
		}
	}
	if (A)
	{
		bCielCree = true;
		Message(FString::Printf(TEXT("Ciel GoodSky ajoute pour la partie (taille x%.1f, %d courbe(s) de couleur meteo)."),
			Echelle, CourbesMeteo.Num()));
	}
	return A;
}

void AMeteoChelles::AppliquerHeure(float DeltaSeconds)
{
	HeureActuelle = CalculerHeure();
	if (FDateTime::Now().GetDayOfYear() != JourCourbes)
	{
		CalculerSoleil();
		FabriquerCourbes();
	}

	if (Ciel)
	{
		EcrireFloat(Ciel, TEXT("Time of Day ( For Custom Mode )"), HeureCiel(HeureActuelle));
	}

	float Bleu = 0.f, Orange = 0.f;
	Poids(HeureActuelle, Bleu, Orange);
	NuitActuelle = FMath::Max(Bleu, Orange);

	if (UExponentialHeightFogComponent* F = Brouillard.Get())
	{
		FLinearColor C = BrouillardOrigine + (Reglages.BrouillardBleu - BrouillardOrigine) * Bleu;
		C = C + (Reglages.BrouillardOrange - C) * Orange;
		C.A = BrouillardOrigine.A;
		if (!C.Equals(F->FogInscatteringColor, 0.002f))
		{
			F->SetFogInscatteringColor(C);
			F->SetDirectionalInscatteringColor(BrouillardDirectionnelOrigine * (1.f - 0.8f * NuitActuelle));
		}
	}
	if (USkyLightComponent* S = Ambiance.Get())
	{
		const float I = AmbianceOrigine * FMath::Lerp(1.f, Reglages.AmbianceNuit, NuitActuelle);
		if (!FMath::IsNearlyEqual(I, S->Intensity, 0.005f))
		{
			S->SetIntensity(I);
		}
	}
}

// ---------------------------------------------------------------- Neige

void AMeteoChelles::MettreAJourNeige()
{
	const bool bVoulue = ForceNeige >= 0 ? ForceNeige == 1 : PeriodeActive(Reglages.Neige, Reglages.NeigeDebut, Reglages.NeigeFin);
	if (bVoulue && Flocons.Num() == 0)
	{
		FString Chemin = Reglages.NeigeEffet;
		if (!Chemin.Contains(TEXT(".")))
		{
			Chemin += TEXT(".") + FPaths::GetBaseFilename(Chemin);
		}
		UParticleSystem* Effet = LoadObject<UParticleSystem>(nullptr, *Chemin, nullptr, LOAD_NoWarn);
		if (!Effet)
		{
			Message(TEXT("Effet de neige introuvable : ") + Chemin);
			ForceNeige = 0;
			return;
		}
		const int32 N = Reglages.NeigeGrille;
		for (int32 i = 0; i < N * N; ++i)
		{
			UParticleSystemComponent* P = NewObject<UParticleSystemComponent>(this);
			P->bAutoActivate = false;
			P->SetUsingAbsoluteLocation(true);
			P->SetUsingAbsoluteRotation(true);
			P->SetUsingAbsoluteScale(true);
			P->SetTemplate(Effet);
			P->SetupAttachment(RootComponent);
			P->RegisterComponent();
			P->SetWorldScale3D(FVector(Reglages.NeigeTaille));
			Flocons.Add(P);
		}
	}
	if (bVoulue != bNeigeEnCours)
	{
		for (UParticleSystemComponent* P : Flocons)
		{
			if (P)
			{
				bVoulue ? P->Activate(true) : P->Deactivate();
			}
		}
		bNeigeEnCours = bVoulue;
		Message(bVoulue ? TEXT("Il neige.") : TEXT("Fin de la neige."));
	}
}

// ---------------------------------------------------------------- Sapins

void AMeteoChelles::MettreAJourSapins()
{
	const bool bVoulus = ForceSapins >= 0 ? ForceSapins == 1 : PeriodeActive(Reglages.Sapins, Reglages.SapinsDebut, Reglages.SapinsFin);
	if (bVoulus == bSapinsVisibles)
	{
		return;
	}
	bSapinsVisibles = bVoulus;
	if (bVoulus)
	{
		PoserTousLesSapins();
	}
	else
	{
		RetirerTousLesSapins();
	}
}

TArray<FVector4> AMeteoChelles::LireSapinsManuels() const
{
	TArray<FVector4> Places;
	TArray<FString> Lignes;
	FFileHelper::LoadFileToStringArray(Lignes, *FPaths::Combine(Dossier(), TEXT("Sapins_Positions.txt")));
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> P;
		Ligne.ParseIntoArray(P, TEXT(";"), true);
		if (P.Num() >= 3)
		{
			Places.Add(FVector4(Nombre(P[0]), Nombre(P[1]), Nombre(P[2]), P.Num() >= 4 ? Nombre(P[3]) : 0.f));
		}
	}
	return Places;
}

void AMeteoChelles::PoserTousLesSapins()
{
	UWorld* Monde = GetWorld();
	int32 Manuels = 0;
	for (const FVector4& P : LireSapinsManuels())
	{
		Manuels += PoserUnSapin(FVector(P.X, P.Y, P.Z), P.W, true) ? 1 : 0;
	}

	// Sapins automatiques sur l'herbe (Landscape), memes places pour tous les joueurs
	FVector Centre = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(Monde); It; ++It)
	{
		Centre = It->GetActorLocation();
		break;
	}
	FRandomStream Hasard(77500);
	const float Rayon = Reglages.SapinsRayon * 100.f;
	const float Ecart = Reglages.SapinsEcart * 100.f;
	TArray<FVector> Poses;
	for (const FSapinMeteo& S : SapinsPoses)
	{
		if (S.Sapin)
		{
			Poses.Add(S.Sapin->GetActorLocation());
		}
	}
	FCollisionQueryParams Requete(TEXT("SapinsMeteo"), false, this);
	if (Ciel)
	{
		Requete.AddIgnoredActor(Ciel); // le dome du ciel ne doit pas arreter les rayons
	}
	FCollisionObjectQueryParams Objets;
	Objets.AddObjectTypesToQuery(ECC_WorldStatic);
	Objets.AddObjectTypesToQuery(ECC_WorldDynamic);
	int32 Auto = 0;
	for (int32 Essai = 0; Essai < Reglages.SapinsAuto * 60 && Auto < Reglages.SapinsAuto; ++Essai)
	{
		const float Angle = Hasard.FRandRange(0.f, 2.f * PI);
		const float Dist = Rayon * FMath::Sqrt(Hasard.FRand());
		const FVector XY = Centre + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		const float Lacet = Hasard.FRandRange(0.f, 360.f);
		FHitResult Impact;
		if (!Monde->LineTraceSingleByChannel(Impact, XY + FVector(0, 0, 30000.f), XY - FVector(0, 0, 30000.f), ECC_Visibility, Requete))
		{
			continue;
		}
		const AActor* Touche = Impact.GetActor();
		if (!Touche || !Touche->GetClass()->GetName().Contains(TEXT("Landscape")) || Impact.ImpactNormal.Z < 0.9f)
		{
			continue;
		}
		bool bTropPres = false;
		for (const FVector& Autre : Poses)
		{
			if (FVector::Dist2D(Autre, Impact.ImpactPoint) < Ecart)
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}
		// de la place autour (pas dans un mur, un arbre ou une voiture)
		const float Hauteur = 350.f * Reglages.TailleSapin;
		const FCollisionShape Forme = FCollisionShape::MakeCapsule(180.f * Reglages.TailleSapin, Hauteur * 0.5f);
		if (Monde->OverlapAnyTestByObjectType(Impact.ImpactPoint + FVector(0, 0, Hauteur * 0.5f + 40.f), FQuat::Identity, Objets, Forme, Requete))
		{
			continue;
		}
		if (PoserUnSapin(Impact.ImpactPoint, Lacet, false))
		{
			Poses.Add(Impact.ImpactPoint);
			++Auto;
		}
	}
	Message(FString::Printf(TEXT("%d sapin(s) de Noel pose(s) (%d a la main, %d automatique(s))."), Manuels + Auto, Manuels, Auto));
}

bool AMeteoChelles::PoserUnSapin(const FVector& Sol, float Lacet, bool bManuel)
{
	UWorld* Monde = GetWorld();
	UClass* Classe = LoadClass<AActor>(nullptr, CheminSapin, nullptr, LOAD_NoWarn);
	if (!Monde || !Classe)
	{
		Message(TEXT("Sapin du Christmas_pack introuvable : ") + FString(CheminSapin));
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Place(FRotator(0.f, Lacet, 0.f), Sol, FVector(Reglages.TailleSapin));
	AActor* Sapin = Monde->SpawnActor<AActor>(Classe, Place, Params);
	if (!Sapin)
	{
		return false;
	}
	Sapin->Tags.Add(TEXT("ChellesMeteoSapin"));

	FSapinMeteo Info;
	Info.Sapin = Sapin;
	Info.bManuel = bManuel;
	Info.Phase = FMath::FRand();

	// Guirlandes lumineuses : on prend les materiaux allumes des guirlandes du pack (variation 1 ou 2)
	const bool bPremiere = (SapinsPoses.Num() % 2) == 0;
	UClass* ClasseGuirlande = LoadClass<AActor>(nullptr, bPremiere ? CheminGuirlande1 : CheminGuirlande2, nullptr, LOAD_NoWarn);
	TInlineComponentArray<UStaticMeshComponent*> Maillages(Sapin);
	for (UStaticMeshComponent* M : Maillages)
	{
		if (!M || !M->GetStaticMesh() || !M->GetStaticMesh()->GetName().Contains(TEXT("garland")) || !ClasseGuirlande)
		{
			continue;
		}
		// la guirlande du pack reste cachee mais vivante : si elle clignote, ses materiaux clignotent aussi ici
		AActor* Guirlande = Monde->SpawnActor<AActor>(ClasseGuirlande, M->GetComponentTransform(), Params);
		if (!Guirlande)
		{
			continue;
		}
		Guirlande->SetActorHiddenInGame(true);
		Guirlande->SetActorEnableCollision(false);
		Guirlande->AttachToActor(Sapin, FAttachmentTransformRules::KeepWorldTransform);
		TInlineComponentArray<UStaticMeshComponent*> Allumees(Guirlande);
		for (UStaticMeshComponent* G : Allumees)
		{
			if (G && G->GetStaticMesh() == M->GetStaticMesh())
			{
				for (int32 i = 0; i < G->GetNumMaterials() && i < M->GetNumMaterials(); ++i)
				{
					M->SetMaterial(i, G->GetMaterial(i));
				}
				break;
			}
		}
		Info.Guirlandes.Add(Guirlande);
	}

	// Petites lumieres colorees autour du sapin (allumees la nuit, pres des joueurs)
	const FBox Boite = Sapin->GetComponentsBoundingBox(false);
	if (Boite.IsValid && GetNetMode() != NM_DedicatedServer)
	{
		const float Haut = Boite.GetSize().Z;
		const float Large = FMath::Max(Boite.GetExtent().X, Boite.GetExtent().Y);
		for (int32 i = 0; i < Reglages.LumieresParSapin; ++i)
		{
			const float A = 2.f * PI * (i + 0.5f) / Reglages.LumieresParSapin + FMath::DegreesToRadians(Lacet);
			const float Niveau = 0.25f + 0.5f * ((i % 3) / 2.f);
			const FVector Pos(Boite.GetCenter().X + FMath::Cos(A) * Large * 0.9f * (1.f - Niveau * 0.6f),
				Boite.GetCenter().Y + FMath::Sin(A) * Large * 0.9f * (1.f - Niveau * 0.6f),
				Boite.Min.Z + Haut * Niveau);
			UPointLightComponent* L = NewObject<UPointLightComponent>(Sapin);
			L->SetMobility(EComponentMobility::Movable);
			L->SetCastShadows(false);
			if (Sapin->GetRootComponent())
			{
				L->SetupAttachment(Sapin->GetRootComponent());
			}
			L->RegisterComponent();
			L->SetWorldLocation(Pos);
			L->SetAttenuationRadius(FMath::Max(300.f, Haut * 0.7f));
			L->SetLightColor(CouleursLumieres[(i + SapinsPoses.Num()) % UE_ARRAY_COUNT(CouleursLumieres)]);
			L->SetIntensity(0.f);
			L->SetVisibility(false);
			Info.Lumieres.Add(L);
		}
	}
	SapinsPoses.Add(Info);
	return true;
}

void AMeteoChelles::RetirerTousLesSapins()
{
	for (FSapinMeteo& S : SapinsPoses)
	{
		for (AActor* G : S.Guirlandes)
		{
			if (G)
			{
				G->Destroy();
			}
		}
		if (S.Sapin)
		{
			S.Sapin->Destroy();
		}
	}
	SapinsPoses.Reset();
}

void AMeteoChelles::AnimerLumieres(float DeltaSeconds)
{
	if (SapinsPoses.Num() == 0 || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FVector Camera = FVector::ZeroVector;
	if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		Camera = CM->GetCameraLocation();
	}
	const float Distance = Reglages.DistanceLumieres * 100.f;
	const float Allume = Lisse(0.15f, 0.6f, NuitActuelle);
	for (FSapinMeteo& S : SapinsPoses)
	{
		if (!S.Sapin)
		{
			continue;
		}
		const bool bProche = FVector::DistSquared(Camera, S.Sapin->GetActorLocation()) < Distance * Distance;
		for (int32 i = 0; i < S.Lumieres.Num(); ++i)
		{
			UPointLightComponent* L = S.Lumieres[i];
			if (!L)
			{
				continue;
			}
			const bool bVisible = bProche && Allume > 0.01f;
			if (L->IsVisible() != bVisible)
			{
				L->SetVisibility(bVisible);
			}
			if (!bVisible)
			{
				continue;
			}
			const float Onde = FMath::Sin(2.f * PI * (Temps * 0.8f * Reglages.Clignotement + S.Phase + i / 3.f));
			const float Force = 0.3f + 0.7f * FMath::Max(0.f, Onde);
			L->SetIntensity(Reglages.PuissanceLumieres * Force * Allume);
		}
	}
}

void AMeteoChelles::PoserSapinDevantJoueur()
{
	APawn* Joueur = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Joueur)
	{
		Message(TEXT("Pas de joueur pour poser un sapin."), true);
		return;
	}
	const FRotator Regard(0.f, Joueur->GetActorRotation().Yaw, 0.f);
	const FVector Devant = Joueur->GetActorLocation() + Regard.Vector() * 400.f;
	FCollisionQueryParams Requete(TEXT("PoserSapin"), false, Joueur);
	Requete.AddIgnoredActor(this);
	FHitResult Impact;
	if (!GetWorld()->LineTraceSingleByChannel(Impact, Devant + FVector(0, 0, 500.f), Devant - FVector(0, 0, 3000.f), ECC_Visibility, Requete))
	{
		Message(TEXT("Pas de sol devant vous pour poser le sapin."), true);
		return;
	}
	const float Lacet = Regard.Yaw + 180.f;
	const FString Chemin = FPaths::Combine(Dossier(), TEXT("Sapins_Positions.txt"));
	const FString Ligne = FString::Printf(TEXT("%.0f;%.0f;%.0f;%.0f\r\n"), Impact.ImpactPoint.X, Impact.ImpactPoint.Y, Impact.ImpactPoint.Z, Lacet);
	FFileHelper::SaveStringToFile(Ligne, *Chemin, FFileHelper::EEncodingOptions::ForceAnsi, &IFileManager::Get(), FILEWRITE_Append);
	if (!bSapinsVisibles)
	{
		ForceSapins = 1;
		MettreAJourSapins();
	}
	else
	{
		PoserUnSapin(Impact.ImpactPoint, Lacet, true);
	}
	Message(TEXT("Sapin pose et garde dans Sapins_Positions.txt : ") + Ligne.TrimEnd(), true);
}

void AMeteoChelles::RetirerSapinProche()
{
	APawn* Joueur = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Joueur)
	{
		return;
	}
	int32 Meilleur = INDEX_NONE;
	float Mieux = 1500.f * 1500.f;
	for (int32 i = 0; i < SapinsPoses.Num(); ++i)
	{
		if (SapinsPoses[i].bManuel && SapinsPoses[i].Sapin)
		{
			const float D = FVector::DistSquared2D(SapinsPoses[i].Sapin->GetActorLocation(), Joueur->GetActorLocation());
			if (D < Mieux)
			{
				Mieux = D;
				Meilleur = i;
			}
		}
	}
	if (Meilleur == INDEX_NONE)
	{
		Message(TEXT("Aucun sapin pose a la main a moins de 15 m."), true);
		return;
	}
	const FVector Place = SapinsPoses[Meilleur].Sapin->GetActorLocation();
	const FString Chemin = FPaths::Combine(Dossier(), TEXT("Sapins_Positions.txt"));
	TArray<FString> Lignes, Gardees;
	FFileHelper::LoadFileToStringArray(Lignes, *Chemin);
	bool bEnleve = false;
	for (const FString& L : Lignes)
	{
		TArray<FString> P;
		L.ParseIntoArray(P, TEXT(";"), true);
		if (!bEnleve && !L.StartsWith(TEXT("#")) && P.Num() >= 3
			&& FVector::Dist2D(FVector(Nombre(P[0]), Nombre(P[1]), 0.f), Place) < 50.f)
		{
			bEnleve = true;
			continue;
		}
		Gardees.Add(L);
	}
	FFileHelper::SaveStringArrayToFile(Gardees, *Chemin, FFileHelper::EEncodingOptions::ForceAnsi);
	for (AActor* G : SapinsPoses[Meilleur].Guirlandes)
	{
		if (G)
		{
			G->Destroy();
		}
	}
	SapinsPoses[Meilleur].Sapin->Destroy();
	SapinsPoses.RemoveAt(Meilleur);
	Message(TEXT("Sapin retire."), true);
}

// ---------------------------------------------------------------- Chaque image

void AMeteoChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;
	const bool bEcran = GetNetMode() != NM_DedicatedServer;

	if (bEcran)
	{
		AppliquerHeure(DeltaSeconds);
	}

	ProchainEtat -= DeltaSeconds;
	if (ProchainEtat <= 0.f)
	{
		ProchainEtat = 5.f;
		if (bEcran)
		{
			MettreAJourNeige();
		}
		MettreAJourSapins();
	}

	if (bEcran && bNeigeEnCours && Flocons.Num() > 0)
	{
		if (APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector Centre = CM->GetCameraLocation() + FVector(0.f, 0.f, Reglages.NeigeHauteur * 100.f);
			const int32 N = Reglages.NeigeGrille;
			const float Ecart = Reglages.NeigeEcart * 100.f;
			for (int32 i = 0; i < Flocons.Num(); ++i)
			{
				if (Flocons[i])
				{
					const FVector Decalage(((i % N) - (N - 1) * 0.5f) * Ecart, ((i / N) - (N - 1) * 0.5f) * Ecart, 0.f);
					Flocons[i]->SetWorldLocation(Centre + Decalage);
				}
			}
		}
	}

	AnimerLumieres(DeltaSeconds);
}

// ---------------------------------------------------------------- Console

void AMeteoChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
	const FString Valeur = Arguments.Num() > 1 ? Arguments[1] : FString();
	const int32 Force = Valeur.IsEmpty() ? 1 : (Oui(Valeur) ? 1 : 0);

	if (Quoi.Equals(TEXT("Heure"), ESearchCase::IgnoreCase))
	{
		HeureForcee = FMath::Fmod(FMath::Max(0.f, LireHeure(Valeur)), 24.f);
		Message(FString::Printf(TEXT("Heure forcee a %.2f (Chelles.Meteo Auto pour revenir)."), HeureForcee), true);
	}
	else if (Quoi.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
	{
		HeureForcee = -1.f;
		ForceHiver = ForceNeige = ForceSapins = -1;
		ProchainEtat = 0.f;
		Message(TEXT("Retour a la normale (heure et saison du PC)."), true);
	}
	else if (Quoi.Equals(TEXT("Hiver"), ESearchCase::IgnoreCase))
	{
		ForceHiver = Force;
		ForceNeige = ForceSapins = -1;
		ProchainEtat = 0.f;
		Message(Force ? TEXT("Hiver force : neige + sapins.") : TEXT("Hiver coupe."), true);
	}
	else if (Quoi.Equals(TEXT("Neige"), ESearchCase::IgnoreCase))
	{
		ForceNeige = Force;
		ProchainEtat = 0.f;
	}
	else if (Quoi.Equals(TEXT("Sapins"), ESearchCase::IgnoreCase))
	{
		ForceSapins = Force;
		ProchainEtat = 0.f;
	}
	else if (Quoi.Equals(TEXT("PoserSapin"), ESearchCase::IgnoreCase))
	{
		PoserSapinDevantJoueur();
	}
	else if (Quoi.Equals(TEXT("RetirerSapin"), ESearchCase::IgnoreCase))
	{
		RetirerSapinProche();
	}
	else if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		LireReglages();
		CalculerSoleil();
		FabriquerCourbes();
		RetirerTousLesSapins();
		bSapinsVisibles = false;
		for (UParticleSystemComponent* F : Flocons)
		{
			if (F)
			{
				F->DestroyComponent();
			}
		}
		Flocons.Reset();
		bNeigeEnCours = false;
		ProchainEtat = 0.f;
		Message(TEXT("Meteo.txt relu."), true);
	}
	else
	{
		float Bleu = 0.f, Orange = 0.f;
		Poids(HeureActuelle, Bleu, Orange);
		float Tangage = 0.f;
		if (UDirectionalLightComponent* S = Soleil.Get())
		{
			Tangage = S->GetComponentRotation().Pitch;
		}
		Message(FString::Printf(TEXT("%02d:%02d (ciel GoodSky %.2f) | lever %.2f coucher %.2f | bleu %d%% orange %d%% | soleil %.0f deg | ciel %s | brouillard %s | neige %s | %d sapin(s)"),
			FMath::FloorToInt(HeureActuelle), FMath::FloorToInt(FMath::Fmod(HeureActuelle, 1.f) * 60.f), HeureCiel(HeureActuelle),
			Lever, Coucher, FMath::RoundToInt(Bleu * 100.f), FMath::RoundToInt(Orange * 100.f), Tangage,
			Ciel ? TEXT("OK") : TEXT("absent"), Brouillard.IsValid() ? TEXT("OK") : TEXT("absent"),
			bNeigeEnCours ? TEXT("oui") : TEXT("non"), SapinsPoses.Num()), true);
	}
}
