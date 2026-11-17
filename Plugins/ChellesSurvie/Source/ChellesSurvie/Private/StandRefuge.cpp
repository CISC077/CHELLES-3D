// CHELLES 3D - Dernier Refuge : point d'achat ou barriere payante (formes de base du moteur, aucun asset du projet)
#include "StandRefuge.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FLinearColor CouleurArme(0.f, 0.85f, 1.f);       // cyan, comme le menu
	const FLinearColor CouleurMunitions(1.f, 0.55f, 0.f);  // orange
	const FLinearColor CouleurBarriere(0.85f, 0.05f, 0.1f); // rouge
}

AStandRefuge::AStandRefuge()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 2.f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));

	Bloc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bloc"));
	Bloc->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Bloc->SetStaticMesh(Cube.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Matiere(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Matiere.Succeeded())
	{
		Bloc->SetMaterial(0, Matiere.Object);
	}
	Bloc->SetCollisionProfileName(TEXT("BlockAll"));
	Bloc->SetCanEverAffectNavigation(false);

	Texte = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Texte"));
	Texte->SetupAttachment(RootComponent);
	Texte->SetHorizontalAlignment(EHTA_Center);
	Texte->SetVerticalAlignment(EVRTA_TextCenter);
	Texte->SetWorldSize(26.f);
	Texte->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AStandRefuge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStandRefuge, Type);
	DOREPLIFETIME(AStandRefuge, Libelle);
	DOREPLIFETIME(AStandRefuge, Prix);
	DOREPLIFETIME(AStandRefuge, bOuvert);
}

void AStandRefuge::BeginPlay()
{
	Super::BeginPlay();
	if (!Libelle.IsEmpty())
	{
		MettreEnForme();
	}
}

void AStandRefuge::Regler(ETypeStand NouveauType, const FString& NouveauLibelle, int32 NouveauPrix, const FString& Classe)
{
	Type = NouveauType;
	Libelle = NouveauLibelle;
	Prix = NouveauPrix;
	ClasseArme = Classe;
	bEnForme = false;
	MettreEnForme();
}

void AStandRefuge::MettreEnForme()
{
	bEnForme = true;

	FVector Taille(120.f, 50.f, 100.f); // cm
	FLinearColor Teinte = CouleurArme;
	if (Type == ETypeStand::Munitions)
	{
		Taille = FVector(80.f, 80.f, 80.f);
		Teinte = CouleurMunitions;
	}
	else if (Type == ETypeStand::Barriere)
	{
		Taille = FVector(30.f, 600.f, 250.f); // mur de 6 m de large, en travers de la rue
		Teinte = CouleurBarriere;
	}
	// Le cube du moteur fait 100 cm et son centre est au milieu : on le pose sur le sol
	Bloc->SetRelativeScale3D(Taille / 100.f);
	Bloc->SetRelativeLocation(FVector(0.f, 0.f, Taille.Z * 0.5f));

	if (!Couleur)
	{
		Couleur = Bloc->CreateDynamicMaterialInstance(0);
	}
	if (Couleur)
	{
		Couleur->SetVectorParameterValue(TEXT("Color"), Teinte);
	}

	Texte->SetRelativeLocation(FVector(0.f, 0.f, Taille.Z + 45.f));
	Texte->SetTextRenderColor(Teinte.ToFColor(true));
	Texte->SetText(FText::FromString(Type == ETypeStand::Barriere
		? FString::Printf(TEXT("BARRIERE - %d pts\n[E] ouvrir"), Prix)
		: FString::Printf(TEXT("%s - %d pts\n[E] acheter"), *Libelle, Prix)));

	OnRep_Ouvert();
}

void AStandRefuge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bEnForme && !Libelle.IsEmpty())
	{
		MettreEnForme(); // les reglages viennent d'arriver par le reseau
	}
	// Le prix se tourne toujours vers la camera du joueur de cette machine
	if (GetNetMode() != NM_DedicatedServer && Texte && Texte->IsVisible())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (PC->PlayerCameraManager)
			{
				const FVector Vers = PC->PlayerCameraManager->GetCameraLocation() - Texte->GetComponentLocation();
				Texte->SetWorldRotation(FRotator(0.f, Vers.Rotation().Yaw, 0.f));
			}
		}
	}
}

void AStandRefuge::Ouvrir()
{
	if (HasAuthority())
	{
		bOuvert = true;
		OnRep_Ouvert();
	}
}

void AStandRefuge::OnRep_Ouvert()
{
	const bool bVisible = !bOuvert;
	Bloc->SetVisibility(bVisible);
	Texte->SetVisibility(bVisible);
	Bloc->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

FString AStandRefuge::Invite() const
{
	if (Type == ETypeStand::Barriere)
	{
		return FString::Printf(TEXT("[E] Ouvrir la barrière - %d pts"), Prix);
	}
	if (Type == ETypeStand::Munitions)
	{
		return FString::Printf(TEXT("[E] Recharger les munitions - %d pts"), Prix);
	}
	return FString::Printf(TEXT("[E] %s - %d pts"), *Libelle, Prix);
}

float AStandRefuge::Portee() const
{
	// Distance au bord du bloc (pas a son centre) : une barriere de 6 m s'achete depuis n'importe quel endroit de sa longueur
	return 170.f;
}
