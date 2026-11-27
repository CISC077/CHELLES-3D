// CHELLES 3D - Pluie du quartier (UE 4.27). Voir PluieChelles.h et Pluie.txt.
#include "PluieChelles.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Particles/ParticleSystemComponent.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* CheminMatGouttes = TEXT("/Game/Chelles/Pluie/M_Pluie_Gouttes.M_Pluie_Gouttes");
	const TCHAR* CheminMatFlaque = TEXT("/Game/Chelles/Pluie/M_Pluie_Flaque.M_Pluie_Flaque");
	const TCHAR* CheminMatSol = TEXT("/Game/Chelles/Pluie/M_Pluie_SolMouille.M_Pluie_SolMouille");
	const TCHAR* CheminSon = TEXT("/Game/Chelles/Pluie/S_Pluie_Boucle.S_Pluie_Boucle");

	bool EstOui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("1")) || V.Equals(TEXT("Vrai"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("On"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	float LireNombre(const FString& V)
	{
		return FCString::Atof(*V.Replace(TEXT(","), TEXT(".")));
	}

	FLinearColor LireCouleur(const FString& V, const FLinearColor& Defaut)
	{
		TArray<FString> P;
		V.ParseIntoArray(P, TEXT(";"));
		if (P.Num() < 3)
		{
			// "0.75,0.8,0.9" : la virgule separe les composantes
			P.Reset();
			V.ParseIntoArray(P, TEXT(","));
		}
		if (P.Num() < 3)
		{
			return Defaut;
		}
		return FLinearColor(LireNombre(P[0]), LireNombre(P[1]), LireNombre(P[2]));
	}

	template <typename T>
	T* Charger(const TCHAR* Chemin)
	{
		return LoadObject<T>(nullptr, Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Le composant utilise-t-il un materiau de route (MI_AsphaltRoad...) ? */
	bool EstRoute(UPrimitiveComponent* C, const TArray<FString>& Mots)
	{
		const int32 N = C->GetNumMaterials();
		for (int32 i = 0; i < N; ++i)
		{
			if (UMaterialInterface* M = C->GetMaterial(i))
			{
				const FString Nom = M->GetName();
				for (const FString& Mot : Mots)
				{
					if (!Mot.IsEmpty() && Nom.Contains(Mot))
					{
						return true;
					}
				}
			}
		}
		return false;
	}
}

APluieChelles::APluieChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // apres la camera
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	SetReplicates(false);
}

void APluieChelles::Message(const FString& Texte, bool bEcran) const
{
	UE_LOG(LogTemp, Log, TEXT("[Pluie] %s"), *Texte);
	if (bEcran && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(120, 190, 255), TEXT("Pluie : ") + Texte);
	}
}

// ---------------------------------------------------------------- Reglages

void APluieChelles::LireReglages()
{
	Reglages = FReglagesPluie();
	FString Dossier = FPaths::ProjectPluginsDir() / TEXT("ChellesPluie");
	if (TSharedPtr<IPlugin> P = IPluginManager::Get().FindPlugin(TEXT("ChellesPluie")))
	{
		Dossier = P->GetBaseDir();
	}
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *(Dossier / TEXT("Pluie.txt"))))
	{
		Message(TEXT("Pluie.txt introuvable : reglages par defaut."));
		return;
	}
	FReglagesPluie& R = Reglages;
	for (FString L : Lignes)
	{
		L.TrimStartAndEndInline();
		if (L.IsEmpty() || L.StartsWith(TEXT("#")))
		{
			continue;
		}
		FString Cle, V;
		if (!L.Split(TEXT("="), &Cle, &V))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		V.TrimStartAndEndInline();
		if (Cle == TEXT("Actif")) { R.bActif = EstOui(V); }
		else if (Cle == TEXT("Mode")) { R.Mode = V; }
		else if (Cle == TEXT("ChancePluie")) { R.ChancePluie = FMath::Clamp(FCString::Atoi(*V), 0, 100); }
		else if (Cle == TEXT("TrancheMinutes")) { R.TrancheMinutes = FMath::Clamp(FCString::Atoi(*V), 2, 240); }
		else if (Cle == TEXT("FonduSecondes")) { R.FonduSecondes = FMath::Clamp(LireNombre(V), 0.5f, 300.f); }
		else if (Cle == TEXT("VitesseGouttes")) { R.VitesseGouttes = FMath::Clamp(LireNombre(V), 1.f, 40.f); }
		else if (Cle == TEXT("Inclinaison")) { R.Inclinaison = FMath::Clamp(LireNombre(V), -45.f, 45.f); }
		else if (Cle == TEXT("NombreGouttes")) { R.NombreGouttes = FMath::Clamp(FCString::Atoi(*V), 100, 20000); }
		else if (Cle == TEXT("LongueurGoutte")) { R.LongueurGoutte = FMath::Clamp(LireNombre(V), 0.05f, 3.f); }
		else if (Cle == TEXT("LargeurGoutte")) { R.LargeurGoutte = FMath::Clamp(LireNombre(V), 0.1f, 10.f); }
		else if (Cle == TEXT("TailleZone")) { R.TailleZone = FMath::Clamp(LireNombre(V), 5.f, 500.f); }
		else if (Cle == TEXT("HauteurZone")) { R.HauteurZone = FMath::Clamp(LireNombre(V), 2.f, 200.f); }
		else if (Cle == TEXT("CouleurGouttes")) { R.CouleurGouttes = LireCouleur(V, R.CouleurGouttes); }
		else if (Cle == TEXT("LuminositeJour")) { R.LuminositeJour = FMath::Clamp(LireNombre(V), 0.f, 20.f); }
		else if (Cle == TEXT("LuminositeNuit")) { R.LuminositeNuit = FMath::Clamp(LireNombre(V), 0.f, 20.f); }
		else if (Cle == TEXT("OpaciteGouttes")) { R.OpaciteGouttes = FMath::Clamp(LireNombre(V), 0.f, 1.f); }
		else if (Cle == TEXT("VolumeSon")) { R.VolumeSon = FMath::Clamp(LireNombre(V), 0.f, 4.f); }
		else if (Cle == TEXT("VolumeAbri")) { R.VolumeAbri = FMath::Clamp(LireNombre(V), 0.f, 1.f); }
		else if (Cle == TEXT("Flaques")) { R.Flaques = FMath::Clamp(FCString::Atoi(*V), 0, 1000); }
		else if (Cle == TEXT("TailleFlaqueMin")) { R.TailleFlaqueMin = FMath::Clamp(LireNombre(V), 0.1f, 20.f); }
		else if (Cle == TEXT("TailleFlaqueMax")) { R.TailleFlaqueMax = FMath::Clamp(LireNombre(V), 0.1f, 20.f); }
		else if (Cle == TEXT("RayonFlaques")) { R.RayonFlaques = FMath::Clamp(LireNombre(V), 10.f, 20000.f); }
		else if (Cle == TEXT("MateriauxRoute"))
		{
			R.MotsRoute.Reset();
			V.ParseIntoArray(R.MotsRoute, TEXT(","));
			for (FString& M : R.MotsRoute)
			{
				M.TrimStartAndEndInline();
			}
		}
		else if (Cle == TEXT("RemplissageMinutes")) { R.RemplissageMinutes = FMath::Clamp(LireNombre(V), 0.05f, 120.f); }
		else if (Cle == TEXT("SechageMinutes")) { R.SechageMinutes = FMath::Clamp(LireNombre(V), 0.05f, 240.f); }
		else if (Cle == TEXT("SolMouille")) { R.bSolMouille = EstOui(V); }
		else if (Cle == TEXT("RayonSolMouille")) { R.RayonSolMouille = FMath::Clamp(LireNombre(V), 10.f, 500.f); }
		else if (Cle == TEXT("Brouillard")) { R.Brouillard = FMath::Clamp(LireNombre(V), 0.f, 20.f); }
		else if (Cle == TEXT("Couleurs")) { R.Couleurs = FMath::Clamp(LireNombre(V), 0.f, 2.f); }
		else if (Cle == TEXT("CouperNeige")) { R.bCouperNeige = EstOui(V); }
	}
	if (R.TailleFlaqueMax < R.TailleFlaqueMin)
	{
		Swap(R.TailleFlaqueMin, R.TailleFlaqueMax);
	}
}

int32 APluieChelles::NiveauOptions() const
{
	// OPTIONS > AVANCES > PLUIE : 0 NON, 1 LEGERE, 2 COMPLETE ; absent = AUTO (suit QUALITE BAS -> LEGERE)
	int32 Choix = -1;
	float Qualite = -1.f;
	if (GConfig)
	{
		GConfig->GetInt(TEXT("ChellesGraphismes"), TEXT("Pluie"), Choix, GGameUserSettingsIni);
		GConfig->GetFloat(TEXT("ChellesMenu"), TEXT("Qualite"), Qualite, GGameUserSettingsIni);
	}
	if (Choix >= 0 && Choix <= 2)
	{
		return Choix;
	}
	return (Qualite >= 0.f && Qualite < 0.5f) ? 1 : 2;
}

bool APluieChelles::AverseVoulue() const
{
	if (!Reglages.bActif || Niveau == 0)
	{
		return false;
	}
	if (ForceMode >= 0)
	{
		return ForceMode == 1;
	}
	if (Reglages.Mode.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Reglages.Mode.Equals(TEXT("Toujours"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	if (Reglages.Mode.Equals(TEXT("Non"), ESearchCase::IgnoreCase) || Reglages.Mode.Equals(TEXT("Jamais"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	// Auto : le meme tirage sur tous les PC (date + heure), une averse dure une ou plusieurs tranches
	const FDateTime Maintenant = FDateTime::Now();
	const int64 Minutes = (int64)Maintenant.GetDayOfYear() * 1440 + Maintenant.GetHour() * 60 + Maintenant.GetMinute();
	const uint32 Tranche = (uint32)(Minutes / Reglages.TrancheMinutes);
	uint32 H = Tranche * 2654435761u ^ (uint32)Maintenant.GetYear() * 40503u;
	H ^= H >> 15;
	H *= 2246822519u;
	H ^= H >> 13;
	return (int32)(H % 100u) < Reglages.ChancePluie;
}

// ---------------------------------------------------------------- Demarrage

void APluieChelles::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return; // rien a afficher sur un serveur dedie
	}
	LireReglages();
	Niveau = NiveauOptions();
	Preparer();
	ProchainEtat = 3.f; // laisse le temps aux routes et au joueur d'apparaitre
}

void APluieChelles::FabriquerGouttes(UProceduralMeshComponent* C, int32 Graine) const
{
	// des traits verticaux (2 petits rectangles en croix par goutte) dans une boite TailleZone x TailleZone x HauteurZone
	FRandomStream Hasard(Graine);
	const float Demi = Reglages.TailleZone * 50.f;
	const float H = Reglages.HauteurZone * 100.f;
	const float W = Reglages.LargeurGoutte * 0.5f;
	const int32 N = Reglages.NombreGouttes;
	TArray<FVector> Points;
	TArray<int32> Triangles;
	TArray<FVector> Normales;
	TArray<FVector2D> UV;
	Points.Reserve(N * 8);
	Triangles.Reserve(N * 12);
	Normales.Reserve(N * 8);
	UV.Reserve(N * 8);
	for (int32 i = 0; i < N; ++i)
	{
		const float L = Reglages.LongueurGoutte * 100.f * Hasard.FRandRange(0.65f, 1.3f);
		const float X = Hasard.FRandRange(-Demi, Demi);
		const float Y = Hasard.FRandRange(-Demi, Demi);
		const float Z = Hasard.FRandRange(0.f, FMath::Max(H - L, 1.f));
		for (int32 q = 0; q < 2; ++q)
		{
			const FVector Cote = (q == 0) ? FVector(W, 0.f, 0.f) : FVector(0.f, W, 0.f);
			const int32 B = Points.Num();
			Points.Add(FVector(X, Y, Z + L) - Cote);
			Points.Add(FVector(X, Y, Z + L) + Cote);
			Points.Add(FVector(X, Y, Z) + Cote);
			Points.Add(FVector(X, Y, Z) - Cote);
			UV.Add(FVector2D(0.f, 0.f));
			UV.Add(FVector2D(1.f, 0.f));
			UV.Add(FVector2D(1.f, 1.f));
			UV.Add(FVector2D(0.f, 1.f));
			for (int32 k = 0; k < 4; ++k)
			{
				Normales.Add(FVector(0.f, 0.f, 1.f));
			}
			Triangles.Append({ B, B + 2, B + 1, B, B + 3, B + 2 });
		}
	}
	C->CreateMeshSection_LinearColor(0, Points, Triangles, Normales, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
}

void APluieChelles::Preparer()
{
	UMaterialInterface* MGouttes = Charger<UMaterialInterface>(CheminMatGouttes);
	if (!MGouttes)
	{
		Message(TEXT("Materiaux de pluie absents (/Game/Chelles/Pluie). Lancez Tools\\Pluie\\INSTALLER_PLUIE.bat (Unreal ferme)."), true);
		bPret = false;
		return;
	}
	MatGouttes = UMaterialInstanceDynamic::Create(MGouttes, this);
	// 2 motifs (le 2e est un autre tirage, tourne de 90 degres) x 2 etages qui se relaient pour faire tomber les gouttes
	for (int32 i = 0; i < 4; ++i)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this);
		C->SetUsingAbsoluteLocation(true);
		C->SetUsingAbsoluteRotation(true);
		C->SetUsingAbsoluteScale(true);
		C->bUseAsyncCooking = false;
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCastShadow(false);
		C->bReceivesDecals = false;
		C->bNeverDistanceCull = true;
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		FabriquerGouttes(C, 7750 + (i / 2) * 31);
		C->SetMaterial(0, MatGouttes);
		C->SetVisibility(false);
		Gouttes.Add(C);
	}

	if (USoundBase* S = Charger<USoundBase>(CheminSon))
	{
		Son = NewObject<UAudioComponent>(this);
		Son->bAutoActivate = false;
		Son->bAllowSpatialization = false;
		Son->bIsUISound = false;
		Son->SetSound(S);
		Son->SetupAttachment(RootComponent);
		Son->RegisterComponent();
	}
	else
	{
		Message(TEXT("Son de pluie absent (S_Pluie_Boucle) : pluie sans bruit."));
	}

	if (UMaterialInterface* MSol = Charger<UMaterialInterface>(CheminMatSol))
	{
		MatSolMouille = UMaterialInstanceDynamic::Create(MSol, this);
		SolMouille = NewObject<UDecalComponent>(this);
		SolMouille->SetUsingAbsoluteLocation(true);
		SolMouille->SetUsingAbsoluteRotation(true);
		SolMouille->SetUsingAbsoluteScale(true);
		SolMouille->SetDecalMaterial(MatSolMouille);
		SolMouille->SortOrder = 0;
		SolMouille->SetFadeScreenSize(0.f);
		SolMouille->SetupAttachment(RootComponent);
		SolMouille->RegisterComponent();
		SolMouille->SetWorldRotation(FRotator(-90.f, 0.f, 0.f)); // projette vers le bas
		SolMouille->SetVisibility(false);
	}

	Ambiance = NewObject<UPostProcessComponent>(this);
	Ambiance->bUnbound = true;
	Ambiance->Priority = 5.f;
	Ambiance->BlendWeight = 0.f;
	Ambiance->Settings.bOverride_ColorSaturation = true;
	Ambiance->Settings.bOverride_ColorContrast = true;
	Ambiance->SetupAttachment(RootComponent);
	Ambiance->RegisterComponent();

	for (TActorIterator<AExponentialHeightFog> It(GetWorld()); It; ++It)
	{
		if (UExponentialHeightFogComponent* F = It->GetComponent())
		{
			Brouillard = F;
			BrouillardOrigine = F->FogDensity;
			break;
		}
	}
	bPret = true;
	Message(FString::Printf(TEXT("Pret (mode %s, %d %% de chance par tranche de %d min, niveau options %d)."),
		*Reglages.Mode, Reglages.ChancePluie, Reglages.TrancheMinutes, Niveau));
}

void APluieChelles::PoserFlaques()
{
	bFlaquesPosees = true;
	UMaterialInterface* MFlaque = Charger<UMaterialInterface>(CheminMatFlaque);
	if (!MFlaque || Reglages.Flaques <= 0)
	{
		return;
	}
	UWorld* Monde = GetWorld();

	// centre : le joueur, sinon un point de depart
	FVector Centre = FVector::ZeroVector;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* P = PC->GetPawn())
		{
			Centre = P->GetActorLocation();
		}
		else if (PC->PlayerCameraManager)
		{
			Centre = PC->PlayerCameraManager->GetCameraLocation();
		}
	}
	if (Centre.IsNearlyZero())
	{
		for (TActorIterator<APlayerStart> It(Monde); It; ++It)
		{
			Centre = It->GetActorLocation();
			break;
		}
	}

	// toutes les routes (composants avec MI_AsphaltRoad) pres du centre, ponderees par leur surface
	struct FRoute
	{
		TWeakObjectPtr<UPrimitiveComponent> C;
		FBox Boite;
		float Poids;
	};
	TArray<FRoute> Routes;
	float Total = 0.f;
	const float Rayon = Reglages.RayonFlaques * 100.f;
	for (TActorIterator<AActor> It(Monde); It; ++It)
	{
		AActor* A = *It;
		if (!A || A == this || A->IsA<APawn>())
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Comps(A);
		for (UPrimitiveComponent* C : Comps)
		{
			if (!C || !C->IsRegistered() || !C->IsVisible() || C->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}
			if (!EstRoute(C, Reglages.MotsRoute))
			{
				continue;
			}
			const FBox B = C->Bounds.GetBox();
			if (FVector::Dist2D(B.GetCenter(), Centre) > Rayon + B.GetExtent().Size2D())
			{
				continue;
			}
			const FVector T = B.GetSize();
			const float P = FMath::Max(T.X * T.Y, 1.f);
			Routes.Add({ C, B, P });
			Total += P;
		}
	}
	if (Routes.Num() == 0)
	{
		Message(FString::Printf(TEXT("Aucune route avec '%s' trouvee : pas de flaques."), Reglages.MotsRoute.Num() > 0 ? *Reglages.MotsRoute[0] : TEXT("?")));
		return;
	}

	// 4 formes de flaques (une image 2x2), une instance de materiau par forme
	for (int32 v = 0; v < 4; ++v)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(MFlaque, this);
		M->SetScalarParameterValue(TEXT("VariationU"), (v % 2) * 0.5f);
		M->SetScalarParameterValue(TEXT("VariationV"), (v / 2) * 0.5f);
		M->SetScalarParameterValue(TEXT("Humidite"), 0.f);
		MatFlaques.Add(M);
	}

	FRandomStream Hasard(77500); // memes flaques pour tous les joueurs
	FCollisionQueryParams Params(TEXT("PluieFlaques"), false);
	int32 Posees = 0;
	const int32 Essais = Reglages.Flaques * 6;
	for (int32 e = 0; e < Essais && Posees < Reglages.Flaques; ++e)
	{
		float Tirage = Hasard.FRand() * Total;
		int32 k = 0;
		for (; k < Routes.Num() - 1; ++k)
		{
			Tirage -= Routes[k].Poids;
			if (Tirage <= 0.f)
			{
				break;
			}
		}
		const FRoute& R = Routes[k];
		const float X = Hasard.FRandRange(R.Boite.Min.X, R.Boite.Max.X);
		const float Y = Hasard.FRandRange(R.Boite.Min.Y, R.Boite.Max.Y);
		const float Taille = Hasard.FRandRange(Reglages.TailleFlaqueMin, Reglages.TailleFlaqueMax) * 50.f; // demi-taille en cm
		const float Lacet = Hasard.FRandRange(0.f, 360.f);
		const float Allonge = Hasard.FRandRange(0.6f, 1.f);
		const int32 Forme = Hasard.RandRange(0, 3);
		if (!R.C.IsValid() || FVector::Dist2D(FVector(X, Y, 0.f), Centre) > Rayon)
		{
			continue;
		}
		FHitResult Hit;
		const FVector Haut(X, Y, R.Boite.Max.Z + 200.f);
		const FVector Bas(X, Y, R.Boite.Min.Z - 200.f);
		if (!Monde->LineTraceSingleByChannel(Hit, Haut, Bas, ECC_Visibility, Params))
		{
			continue;
		}
		const bool bBonneRoute = Hit.Component.Get() == R.C.Get()
			|| (Hit.Component.IsValid() && EstRoute(Hit.Component.Get(), Reglages.MotsRoute));
		if (!bBonneRoute || Hit.ImpactNormal.Z < 0.85f)
		{
			continue; // une voiture, un mur, un trottoir, une pente... on essaie ailleurs
		}
		UDecalComponent* D = NewObject<UDecalComponent>(this);
		D->SetUsingAbsoluteLocation(true);
		D->SetUsingAbsoluteRotation(true);
		D->SetUsingAbsoluteScale(true);
		D->SetDecalMaterial(MatFlaques[Forme]);
		D->DecalSize = FVector(25.f, Taille, Taille * Allonge);
		D->SortOrder = 1;
		D->SetFadeScreenSize(0.004f);
		D->SetupAttachment(RootComponent);
		D->RegisterComponent();
		D->SetWorldLocationAndRotation(Hit.ImpactPoint, FRotator(-90.f, Lacet, 0.f));
		D->SetVisibility(false);
		DecalsFlaques.Add(D);
		++Posees;
	}
	Message(FString::Printf(TEXT("%d flaques posees sur %d morceaux de route."), Posees, Routes.Num()));
}

void APluieChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (UExponentialHeightFogComponent* F = Brouillard.Get())
	{
		if (BrouillardOrigine >= 0.f)
		{
			F->SetFogDensity(BrouillardOrigine);
		}
	}
	if (Son)
	{
		Son->Stop();
	}
	Super::EndPlay(Raison);
}

// ---------------------------------------------------------------- Chaque image

float APluieChelles::Jour() const
{
	// soleil au-dessus de l'horizon = jour (le ciel de ChellesMeteo fait tourner la lumiere du soleil)
	float Meilleur = -2.f;
	bool bTrouve = false;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent());
		if (!L || !L->IsVisible() || L->Mobility == EComponentMobility::Static)
		{
			continue;
		}
		const float Hauteur = -L->GetDirection().Z;
		if (!bTrouve || Hauteur > Meilleur)
		{
			Meilleur = Hauteur;
			bTrouve = true;
		}
	}
	if (!bTrouve)
	{
		return 1.f;
	}
	return FMath::SmoothStep(-0.08f, 0.12f, Meilleur);
}

void APluieChelles::MettreAJourNiveau()
{
	const int32 Nouveau = NiveauOptions();
	if (Nouveau != Niveau)
	{
		Niveau = Nouveau;
		static const TCHAR* Noms[] = { TEXT("NON"), TEXT("LEGERE"), TEXT("COMPLETE") };
		Message(FString::Printf(TEXT("Options : pluie %s."), Noms[FMath::Clamp(Niveau, 0, 2)]));
	}
	if (Niveau == 2 && !bFlaquesPosees && bPret)
	{
		PoserFlaques();
	}
}

void APluieChelles::MettreAJourGouttes(float DeltaSeconds)
{
	const bool bVisible = bPret && Force > 0.002f;
	APlayerCameraManager* CM = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CM)
	{
		return;
	}
	const FVector Cam = CM->GetCameraLocation();
	const int32 Actives = (Niveau >= 2) ? 4 : 2;
	const float H = Reglages.HauteurZone * 100.f;
	// gouttes accrochees au monde en hauteur (elles tombent vraiment), et qui suivent la camera a l'horizontale
	const double Chute = Temps * Reglages.VitesseGouttes * 100.0;
	const double Z0 = -(Chute - FMath::FloorToDouble(Chute / H) * H);
	const double Base = Z0 + FMath::FloorToDouble(((double)Cam.Z - 0.5 * H - Z0) / H) * H;
	for (int32 i = 0; i < Gouttes.Num(); ++i)
	{
		UProceduralMeshComponent* C = Gouttes[i];
		if (!C)
		{
			continue;
		}
		const bool bCelleCi = bVisible && i < Actives;
		if (C->IsVisible() != bCelleCi)
		{
			C->SetVisibility(bCelleCi);
		}
		if (!bCelleCi)
		{
			continue;
		}
		const int32 Motif = i / 2;
		const int32 Etage = i % 2;
		const FVector Pos(Cam.X, Cam.Y, (float)(Base + Etage * H));
		C->SetWorldLocationAndRotation(Pos, FRotator(0.f, Motif * 90.f, Reglages.Inclinaison));
	}

	if (MatGouttes && bVisible)
	{
		const float Lum = FMath::Lerp(Reglages.LuminositeNuit, Reglages.LuminositeJour, Jour());
		MatGouttes->SetScalarParameterValue(TEXT("Luminosite"), Lum);
		MatGouttes->SetScalarParameterValue(TEXT("Opacite"), Reglages.OpaciteGouttes * Force * FMath::Lerp(1.f, 0.15f, AbriLisse));
		MatGouttes->SetVectorParameterValue(TEXT("Couleur"), Reglages.CouleurGouttes);
	}

	// sol mouille : suit la camera
	if (SolMouille)
	{
		const bool bSol = bPret && Niveau >= 2 && Reglages.bSolMouille && Humidite > 0.01f;
		if (SolMouille->IsVisible() != bSol)
		{
			SolMouille->SetVisibility(bSol);
		}
		if (bSol)
		{
			const float R = Reglages.RayonSolMouille * 100.f;
			const FVector Taille(2000.f, R * 1.2f, R * 1.2f);
			if (!SolMouille->DecalSize.Equals(Taille, 1.f))
			{
				SolMouille->DecalSize = Taille;
				SolMouille->MarkRenderStateDirty();
			}
			SolMouille->SetWorldLocation(Cam);
			if (MatSolMouille)
			{
				MatSolMouille->SetScalarParameterValue(TEXT("Humidite"), Humidite);
				MatSolMouille->SetScalarParameterValue(TEXT("Rayon"), R);
			}
		}
	}

	// son
	if (Son)
	{
		const float Volume = Reglages.VolumeSon * Force * FMath::Lerp(1.f, Reglages.VolumeAbri, AbriLisse);
		if (Force > 0.002f && Niveau > 0)
		{
			if (!Son->IsPlaying())
			{
				Son->Play();
			}
			Son->SetVolumeMultiplier(FMath::Max(Volume, 0.001f));
		}
		else if (Son->IsPlaying())
		{
			Son->Stop();
		}
	}
}

void APluieChelles::MettreAJourAbri()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager)
	{
		bAbri = false;
		return;
	}
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	FCollisionQueryParams Params(TEXT("PluieAbri"), false);
	Params.AddIgnoredActor(this);
	if (APawn* P = PC->GetPawn())
	{
		Params.AddIgnoredActor(P);
		TArray<AActor*> Accroches;
		P->GetAttachedActors(Accroches);
		Params.AddIgnoredActors(Accroches);
		if (AActor* Parent = P->GetAttachParentActor())
		{
			Params.AddIgnoredActor(Parent);
		}
	}
	if (AActor* Vue = PC->GetViewTarget())
	{
		Params.AddIgnoredActor(Vue);
	}
	FHitResult Hit;
	bAbri = GetWorld()->LineTraceSingleByChannel(Hit, Cam + FVector(0.f, 0.f, 30.f), Cam + FVector(0.f, 0.f, 4000.f), ECC_Visibility, Params);
}

void APluieChelles::MettreAJourAmbiance()
{
	const float Ciel = (Niveau >= 2) ? Force : Force * 0.5f;
	if (Ambiance)
	{
		const float S = FMath::Lerp(1.f, Reglages.Couleurs, Ciel);
		Ambiance->Settings.ColorSaturation = FVector4(S, S, S, 1.f);
		const float C = FMath::Lerp(1.f, 0.94f, Ciel);
		Ambiance->Settings.ColorContrast = FVector4(C, C, C, 1.f);
		Ambiance->BlendWeight = Ciel > 0.001f ? 1.f : 0.f;
	}
	if (UExponentialHeightFogComponent* F = Brouillard.Get())
	{
		if (BrouillardOrigine >= 0.f)
		{
			const float D = BrouillardOrigine * FMath::Lerp(1.f, Reglages.Brouillard, Ciel);
			if (!FMath::IsNearlyEqual(D, F->FogDensity, BrouillardOrigine * 0.01f + 1e-6f))
			{
				F->SetFogDensity(D);
			}
		}
	}
	// flaques
	const bool bFlaques = Niveau >= 2 && Humidite > 0.01f;
	for (UDecalComponent* D : DecalsFlaques)
	{
		if (D && D->IsVisible() != bFlaques)
		{
			D->SetVisibility(bFlaques);
		}
	}
	if (bFlaques && FMath::Abs(Humidite - DernierEnvoi) > 0.005f)
	{
		DernierEnvoi = Humidite;
		for (UMaterialInstanceDynamic* M : MatFlaques)
		{
			if (M)
			{
				M->SetScalarParameterValue(TEXT("Humidite"), Humidite);
			}
		}
	}
}

void APluieChelles::MettreAJourNeige()
{
	if (!Reglages.bCouperNeige)
	{
		return;
	}
	const bool bCouper = Force > 0.05f && bPluieEnCours;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->GetClass()->GetName() != TEXT("MeteoChelles"))
		{
			continue;
		}
		bool bNeigeMeteo = false;
		if (FBoolProperty* P = FindFProperty<FBoolProperty>(A->GetClass(), TEXT("bNeigeEnCours")))
		{
			bNeigeMeteo = P->GetPropertyValue_InContainer(A);
		}
		TInlineComponentArray<UParticleSystemComponent*> Flocons(A);
		for (UParticleSystemComponent* F : Flocons)
		{
			if (!F)
			{
				continue;
			}
			if (bCouper && F->IsActive())
			{
				F->Deactivate();
				bNeigeCoupee = true;
			}
			else if (!bCouper && bNeigeCoupee && bNeigeMeteo && !F->IsActive())
			{
				F->Activate(true);
			}
		}
		break;
	}
	if (!bCouper && bNeigeCoupee)
	{
		bNeigeCoupee = false;
		Message(TEXT("Fin de l'averse : la neige revient."));
	}
}

void APluieChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;

	ProchainEtat -= DeltaSeconds;
	if (ProchainEtat <= 0.f)
	{
		ProchainEtat = 2.f;
		MettreAJourNiveau();
		const bool bVoulue = bPret && AverseVoulue();
		if (bVoulue != bPluieEnCours)
		{
			bPluieEnCours = bVoulue;
			Message(bVoulue ? TEXT("Il commence a pleuvoir.") : TEXT("La pluie s'arrete."));
		}
		MettreAJourNeige();
	}

	// fondu de l'averse, puis les flaques se remplissent / sechent
	const float Cible = bPluieEnCours ? 1.f : 0.f;
	Force = FMath::FInterpConstantTo(Force, Cible, DeltaSeconds, 1.f / Reglages.FonduSecondes);
	if (Force > 0.3f)
	{
		Humidite = FMath::Min(1.f, Humidite + DeltaSeconds * Force / (Reglages.RemplissageMinutes * 60.f));
	}
	else
	{
		Humidite = FMath::Max(0.f, Humidite - DeltaSeconds / (Reglages.SechageMinutes * 60.f));
	}

	if (Force > 0.002f)
	{
		ProchainAbri -= DeltaSeconds;
		if (ProchainAbri <= 0.f)
		{
			ProchainAbri = 0.4f;
			MettreAJourAbri();
		}
	}
	else
	{
		bAbri = false;
	}
	AbriLisse = FMath::FInterpConstantTo(AbriLisse, bAbri ? 1.f : 0.f, DeltaSeconds, 2.f);

	MettreAJourGouttes(DeltaSeconds);
	MettreAJourAmbiance();
}

// ---------------------------------------------------------------- Console

void APluieChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
	if (Quoi.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Quoi.Equals(TEXT("1")))
	{
		ForceMode = 1;
		ProchainEtat = 0.f;
		Message(TEXT("Averse forcee (Chelles.Pluie Auto pour revenir a la meteo)."), true);
	}
	else if (Quoi.Equals(TEXT("Non"), ESearchCase::IgnoreCase) || Quoi.Equals(TEXT("0")))
	{
		ForceMode = 0;
		ProchainEtat = 0.f;
		Message(TEXT("Pluie coupee (Chelles.Pluie Auto pour revenir a la meteo)."), true);
	}
	else if (Quoi.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
	{
		ForceMode = -1;
		ProchainEtat = 0.f;
		Message(TEXT("Retour a la meteo automatique."), true);
	}
	else if (Quoi.Equals(TEXT("Flaques"), ESearchCase::IgnoreCase))
	{
		Humidite = 1.f;
		DernierEnvoi = -1.f;
		Message(TEXT("Flaques remplies tout de suite."), true);
	}
	else if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		LireReglages();
		ProchainEtat = 0.f;
		Message(TEXT("Pluie.txt relu."), true);
	}
	else
	{
		static const TCHAR* Noms[] = { TEXT("NON"), TEXT("LEGERE"), TEXT("COMPLETE") };
		Message(FString::Printf(TEXT("averse %s, force %.0f %%, flaques %.0f %% (%d), options %s, abri %s, mode %s%s"),
			bPluieEnCours ? TEXT("oui") : TEXT("non"), Force * 100.f, Humidite * 100.f, DecalsFlaques.Num(),
			Noms[FMath::Clamp(Niveau, 0, 2)], bAbri ? TEXT("oui") : TEXT("non"), *Reglages.Mode,
			bPret ? TEXT("") : TEXT(" - MATERIAUX ABSENTS : lancez INSTALLER_PLUIE.bat")), true);
	}
}
