// CHELLES 3D - PvP : le kit de survie qu'on jette a un ami a terre
#include "KitSurvie.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AKitSurvie::AKitSurvie()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	NetUpdateFrequency = 30.f;
	bAlwaysRelevant = true;
	Tags.Add(TEXT("ChellesKitSurvie"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// La boite (1 m de base dans le moteur : 36 x 26 x 18 cm)
	Boite = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boite"));
	RootComponent = Boite;
	if (Cube.Succeeded())
	{
		Boite->SetStaticMesh(Cube.Object);
	}
	Boite->SetWorldScale3D(FVector(0.36f, 0.26f, 0.18f));
	Boite->SetCollisionProfileName(TEXT("PhysicsActor"));
	Boite->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // ne bouscule pas les joueurs
	Boite->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Boite->SetSimulatePhysics(false); // active au lancement (serveur)
	Boite->SetLinearDamping(0.2f);
	Boite->SetAngularDamping(1.5f);
	Boite->SetMassOverrideInKg(NAME_None, 2.f, true);

	// La croix blanche sur le dessus (taille relative a la boite)
	CroixA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CroixA"));
	CroixA->SetupAttachment(Boite);
	CroixB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CroixB"));
	CroixB->SetupAttachment(Boite);
	for (UStaticMeshComponent* Piece : { CroixA, CroixB })
	{
		if (Cube.Succeeded())
		{
			Piece->SetStaticMesh(Cube.Object);
		}
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetRelativeLocation(FVector(0.f, 0.f, 50.5f));
	}
	CroixA->SetRelativeScale3D(FVector(0.62f, 0.18f, 0.03f));
	CroixB->SetRelativeScale3D(FVector(0.18f, 0.85f, 0.03f));

	// Petite lueur verte : on voit le kit tomber, meme la nuit
	Lueur = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lueur"));
	Lueur->SetupAttachment(Boite);
	Lueur->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	Lueur->SetIntensity(1500.f);
	Lueur->SetAttenuationRadius(350.f);
	Lueur->SetLightColor(FLinearColor(0.2f, 1.f, 0.35f));
	Lueur->SetCastShadows(false);
}

void AKitSurvie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AKitSurvie, CheminModele);
}

void AKitSurvie::BeginPlay()
{
	Super::BeginPlay();
	Colorer(Boite, FLinearColor(0.05f, 0.55f, 0.15f));
	Colorer(CroixA, FLinearColor(1.f, 1.f, 1.f));
	Colorer(CroixB, FLinearColor(1.f, 1.f, 1.f));
	if (!CheminModele.IsEmpty())
	{
		QuandModeleRecu();
	}
}

void AKitSurvie::Colorer(UStaticMeshComponent* Piece, const FLinearColor& Couleur)
{
	if (!Piece)
	{
		return;
	}
	UMaterialInterface* Base = Piece->GetMaterial(0);
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!Base)
	{
		return;
	}
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, this);
	Mat->SetVectorParameterValue(TEXT("Color"), Couleur);
	Piece->SetMaterial(0, Mat);
}

void AKitSurvie::Lancer(const FVector& Vitesse)
{
	Boite->SetSimulatePhysics(true);
	Boite->SetPhysicsLinearVelocity(Vitesse);
	Boite->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(-180.f, 180.f)));
}

void AKitSurvie::ChangerModele(const FString& Chemin)
{
	CheminModele = Chemin;
	QuandModeleRecu();
}

void AKitSurvie::QuandModeleRecu()
{
	if (CheminModele.IsEmpty())
	{
		return;
	}
	UStaticMesh* Modele = LoadObject<UStaticMesh>(nullptr, *CheminModele);
	if (!Modele)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PvP] Modele du kit introuvable : %s (la boite verte est gardee)."), *CheminModele);
		return;
	}
	// Le modele du projet remplace la boite : on garde sa taille d'origine, on cache la croix
	CroixA->SetVisibility(false);
	CroixB->SetVisibility(false);
	Boite->SetStaticMesh(Modele);
	Boite->SetMaterial(0, nullptr);
	Boite->EmptyOverrideMaterials();
	Boite->SetWorldScale3D(FVector(1.f));
}
