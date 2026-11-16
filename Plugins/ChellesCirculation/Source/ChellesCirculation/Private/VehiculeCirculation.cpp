#include "VehiculeCirculation.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Couleurs de voitures courantes en France (blanc, gris, noir... et quelques couleurs vives). */
	const FLinearColor CouleursCarrosserie[] = {
		FLinearColor(0.80f, 0.80f, 0.80f), FLinearColor(0.80f, 0.80f, 0.80f), FLinearColor(0.02f, 0.02f, 0.02f),
		FLinearColor(0.02f, 0.02f, 0.02f), FLinearColor(0.35f, 0.36f, 0.38f), FLinearColor(0.15f, 0.15f, 0.16f),
		FLinearColor(0.55f, 0.56f, 0.58f), FLinearColor(0.02f, 0.05f, 0.20f), FLinearColor(0.45f, 0.02f, 0.02f),
		FLinearColor(0.10f, 0.20f, 0.45f), FLinearColor(0.05f, 0.15f, 0.07f), FLinearColor(0.60f, 0.45f, 0.20f)
	};

	/** Noms de parametres de couleur les plus courants dans les materiaux de voitures. */
	const TCHAR* ParametresCouleur[] = {
		TEXT("Color"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("Paint"), TEXT("PaintColor"),
		TEXT("Paint Color"), TEXT("BodyColor"), TEXT("Body Color"), TEXT("CarColor"), TEXT("Tint"), TEXT("Couleur")
	};

	/** Mots qui designent l'emplacement de materiau de la carrosserie. */
	const TCHAR* MotsCarrosserie[] = {
		TEXT("body"), TEXT("paint"), TEXT("carross"), TEXT("exterior"), TEXT("shell"), TEXT("color")
	};

	bool EstCarrosserie(const FString& Nom)
	{
		const FString N = Nom.ToLower();
		for (const TCHAR* Mot : MotsCarrosserie)
		{
			if (N.Contains(Mot)) { return true; }
		}
		return false;
	}
}

AVehiculeCirculation::AVehiculeCirculation()
{
	// Pas de Tick : le Circulation Spawner deplace toutes les voitures lui-meme.
	PrimaryActorTick.bCanEverTick = false;

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Racine->SetMobility(EComponentMobility::Movable);
	RootComponent = Racine;

	Caisse = CreateDefaultSubobject<UBoxComponent>(TEXT("Caisse"));
	Caisse->SetupAttachment(Racine);
	Caisse->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Caisse->SetGenerateOverlapEvents(false);
	Caisse->SetCanEverAffectNavigation(false); // une voiture qui roule ne doit pas reconstruire le NavMesh
	Caisse->InitBoxExtent(FVector(225.f, 90.f, 70.f));

	ModeleStatique = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ModeleStatique"));
	ModeleStatique->SetupAttachment(Racine);
	ModeleStatique->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ModeleStatique->SetGenerateOverlapEvents(false);
	ModeleStatique->SetCanEverAffectNavigation(false);

	ModeleSquelettique = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ModeleSquelettique"));
	ModeleSquelettique->SetupAttachment(Racine);
	ModeleSquelettique->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ModeleSquelettique->SetGenerateOverlapEvents(false);
	ModeleSquelettique->SetCanEverAffectNavigation(false);
	ModeleSquelettique->SetSimulatePhysics(false);
	ModeleSquelettique->bEnableUpdateRateOptimizations = true;
	ModeleSquelettique->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	ModeleSquelettique->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		CubeSecours = Cube.Object;
	}
}

void AVehiculeCirculation::ConfigurerSilhouette(float DistanceAffichage)
{
	if (!CubeSecours)
	{
		return;
	}
	// Une boite de la taille d'une citadine (4,4 m x 1,8 m x 1,45 m), couleur au hasard.
	const FVector Taille(440.f, 180.f, 145.f);
	ModeleStatique->SetStaticMesh(CubeSecours);
	ModeleStatique->SetRelativeRotation(FRotator::ZeroRotator);
	ModeleStatique->SetRelativeScale3D(Taille / 100.f);
	ModeleStatique->SetRelativeLocation(FVector(0.f, 0.f, Taille.Z * 0.5f));
	ModeleStatique->SetVisibility(true);
	ModeleSquelettique->SetVisibility(false);
	Longueur = Taille.X;
	Largeur = Taille.Y;
	Caisse->SetBoxExtent(Taille * 0.5f);
	Caisse->SetRelativeLocation(FVector(0.f, 0.f, Taille.Z * 0.5f));
	if (DistanceAffichage > 0.f)
	{
		ModeleStatique->SetCullDistance(DistanceAffichage);
	}
	if (UMaterialInstanceDynamic* Dynamique = ModeleStatique->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamique->SetVectorParameterValue(TEXT("Color"),
			CouleursCarrosserie[FMath::RandRange(0, UE_ARRAY_COUNT(CouleursCarrosserie) - 1)]);
	}
}

void AVehiculeCirculation::ReinitialiserConduite()
{
	Itineraire.Reset();
	TronconPrecedent = INDEX_NONE;
	Distance = 0.f;
	Vitesse = 0.f;
	CarrefoursReserves.Reset();
	AttenteCarrefour = 0.f;
	CarrefourForce = INDEX_NONE;
	CarrefourDemande = INDEX_NONE;
	DistancePieton = BIG_NUMBER;
	ProchainTestPieton = 0.f;
	TempsBloqueParPieton = 0.f;
	TempsALArret = 0.f;
	TempsBloqueEnCroisement = 0.f;
	FinPassageForce = 0.f;
	bPoseInitialisee = false;
}

bool AVehiculeCirculation::Configurer(const FModeleVehicule& Modele, float DistanceAffichage, bool bCouleurAleatoire)
{
	UMeshComponent* Composant = nullptr;
	FBoxSphereBounds Bornes;

	if (Modele.MeshStatique)
	{
		ModeleStatique->SetStaticMesh(Modele.MeshStatique);
		ModeleStatique->SetVisibility(true);
		ModeleSquelettique->SetVisibility(false);
		Bornes = Modele.MeshStatique->GetBounds();
		Composant = ModeleStatique;
	}
	else if (Modele.MeshSquelettique)
	{
		ModeleSquelettique->SetSkeletalMesh(Modele.MeshSquelettique);
		ModeleSquelettique->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		ModeleSquelettique->SetComponentTickEnabled(false); // pose de repos : aucun calcul d'animation
		ModeleSquelettique->SetVisibility(true);
		ModeleStatique->SetVisibility(false);
		Bornes = Modele.MeshSquelettique->GetImportedBounds();
		Composant = ModeleSquelettique;
	}
	else
	{
		return false;
	}

	// Le modele est tourne pour que l'avant de la voiture soit vers +X (sens de la marche),
	// puis decale pour que le centre soit sur l'acteur et les roues sur le sol.
	const FRotator Correction(0.f, Modele.CorrectionOrientation, 0.f);
	const FVector OrigineTournee = Correction.RotateVector(Bornes.Origin);
	const FVector EtendueTournee = Correction.RotateVector(Bornes.BoxExtent).GetAbs();
	Composant->SetRelativeRotation(Correction);
	Composant->SetRelativeLocation(FVector(-OrigineTournee.X, -OrigineTournee.Y, EtendueTournee.Z - OrigineTournee.Z));

	Longueur = EtendueTournee.X * 2.f;
	Largeur = EtendueTournee.Y * 2.f;
	Caisse->SetBoxExtent(EtendueTournee);
	Caisse->SetRelativeLocation(FVector(0.f, 0.f, EtendueTournee.Z));

	if (DistanceAffichage > 0.f)
	{
		Composant->SetCullDistance(DistanceAffichage);
	}
	if (bCouleurAleatoire)
	{
		ColorerCarrosserie(Composant);
	}
	return true;
}

void AVehiculeCirculation::ColorerCarrosserie(UMeshComponent* Composant)
{
	const FLinearColor Couleur = CouleursCarrosserie[FMath::RandRange(0, UE_ARRAY_COUNT(CouleursCarrosserie) - 1)];
	const TArray<FName> Emplacements = Composant->GetMaterialSlotNames();
	const int32 Nombre = Composant->GetNumMaterials();
	for (int32 i = 0; i < Nombre; ++i)
	{
		const UMaterialInterface* Materiau = Composant->GetMaterial(i);
		const FString NomEmplacement = Emplacements.IsValidIndex(i) ? Emplacements[i].ToString() : FString();
		if (!Materiau || !(EstCarrosserie(NomEmplacement) || EstCarrosserie(Materiau->GetName())))
		{
			continue;
		}
		// On ne cree un materiau dynamique que si l'un des parametres de couleur existe vraiment.
		const TCHAR* ParametreTrouve = nullptr;
		for (const TCHAR* Parametre : ParametresCouleur)
		{
			FLinearColor Valeur;
			if (Materiau->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(Parametre)), Valeur))
			{
				ParametreTrouve = Parametre;
				break;
			}
		}
		if (!ParametreTrouve)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* Dynamique = Composant->CreateAndSetMaterialInstanceDynamic(i))
		{
			Dynamique->SetVectorParameterValue(FName(ParametreTrouve), Couleur);
		}
	}
}
