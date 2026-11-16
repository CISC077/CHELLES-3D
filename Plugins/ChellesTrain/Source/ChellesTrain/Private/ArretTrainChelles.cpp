// CHELLES 3D - Arret de train (voir ArretTrainChelles.h)
#include "ArretTrainChelles.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AArretTrainChelles::AArretTrainChelles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // chaque machine pose ses propres panneaux

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylindre(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MateriauBase(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Poteau de 3 m
	Poteau = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Poteau"));
	Poteau->SetupAttachment(Racine);
	if (Cylindre.Succeeded()) { Poteau->SetStaticMesh(Cylindre.Object); }
	if (MateriauBase.Succeeded()) { Poteau->SetMaterial(0, MateriauBase.Object); }
	Poteau->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Poteau->SetRelativeScale3D(FVector(0.09f, 0.09f, 3.f));
	Poteau->SetCanEverAffectNavigation(false);

	// Panneau en haut du poteau (face a la route = axe X de l'acteur)
	Panneau = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panneau"));
	Panneau->SetupAttachment(Racine);
	if (Cube.Succeeded()) { Panneau->SetStaticMesh(Cube.Object); }
	if (MateriauBase.Succeeded()) { Panneau->SetMaterial(0, MateriauBase.Object); }
	Panneau->SetRelativeLocation(FVector(0.f, 0.f, 275.f));
	Panneau->SetRelativeScale3D(FVector(0.05f, 0.9f, 0.55f));
	Panneau->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panneau->SetCanEverAffectNavigation(false);

	// Bande jaune au bord du trottoir, le long de la route (le "quai")
	Quai = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Quai"));
	Quai->SetupAttachment(Racine);
	if (Cube.Succeeded()) { Quai->SetStaticMesh(Cube.Object); }
	if (MateriauBase.Succeeded()) { Quai->SetMaterial(0, MateriauBase.Object); }
	Quai->SetRelativeLocation(FVector(110.f, 0.f, 1.f));
	Quai->SetRelativeScale3D(FVector(0.3f, 22.f, 0.02f));
	Quai->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Quai->SetCastShadow(false);
	Quai->SetCanEverAffectNavigation(false);

	auto CreerTexte = [this](const TCHAR* Nom, const FVector& Position, const FRotator& Rotation, float Taille, const FColor& Couleur)
	{
		UTextRenderComponent* Texte = CreateDefaultSubobject<UTextRenderComponent>(Nom);
		Texte->SetupAttachment(Racine);
		Texte->SetRelativeLocation(Position);
		Texte->SetRelativeRotation(Rotation);
		Texte->SetHorizontalAlignment(EHTA_Center);
		Texte->SetVerticalAlignment(EVRTA_TextCenter);
		Texte->SetWorldSize(Taille);
		Texte->SetTextRenderColor(Couleur);
		Texte->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Texte;
	};
	TexteTrain = CreerTexte(TEXT("TexteTrain"), FVector(3.5f, 0.f, 287.f), FRotator::ZeroRotator, 26.f, FColor::White);
	TexteTrainDos = CreerTexte(TEXT("TexteTrainDos"), FVector(-3.5f, 0.f, 287.f), FRotator(0.f, 180.f, 0.f), 26.f, FColor::White);
	TexteNom = CreerTexte(TEXT("TexteNom"), FVector(3.5f, 0.f, 262.f), FRotator::ZeroRotator, 10.f, FColor::Yellow);
	TexteNomDos = CreerTexte(TEXT("TexteNomDos"), FVector(-3.5f, 0.f, 262.f), FRotator(0.f, 180.f, 0.f), 10.f, FColor::Yellow);
	TexteTrain->SetText(FText::FromString(TEXT("TRAIN")));
	TexteTrainDos->SetText(FText::FromString(TEXT("TRAIN")));

	// Banc (pack Bus_Station), derriere le poteau
	Banc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Banc"));
	Banc->SetupAttachment(Racine);
	Banc->SetRelativeLocation(FVector(-120.f, 0.f, 0.f));
	Banc->SetCanEverAffectNavigation(false);

#if WITH_EDITORONLY_DATA
	Icone = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icone"));
	if (Icone)
	{
		Icone->SetupAttachment(Racine);
		Icone->SetRelativeLocation(FVector(0.f, 0.f, 350.f));
	}
	Fleche = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("CoteRoute"));
	if (Fleche)
	{
		// La fleche montre la route : tournez l'arret pour qu'elle pointe vers la chaussee.
		Fleche->SetupAttachment(Racine);
		Fleche->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
		Fleche->ArrowColor = FColor(30, 120, 255);
		Fleche->ArrowSize = 2.f;
	}
#endif
}

void AArretTrainChelles::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	MettreAJourApparence();
}

void AArretTrainChelles::DefinirNom(const FString& Nom)
{
	NomArret = Nom;
	MettreAJourApparence();
}

void AArretTrainChelles::MettreAJourApparence()
{
	// Le code est ecrit sans accents : quelques mots sont affiches avec leurs accents
	FString Nom = NomArret;
	Nom.ReplaceInline(TEXT("Arret"), TEXT("Arr\u00EAt"), ESearchCase::CaseSensitive);
	Nom.ReplaceInline(TEXT("Marechal"), TEXT("Mar\u00E9chal"), ESearchCase::CaseSensitive);
	Nom.ReplaceInline(TEXT("Resistance"), TEXT("R\u00E9sistance"), ESearchCase::CaseSensitive);
	Nom.ReplaceInline(TEXT("Freres"), TEXT("Fr\u00E8res"), ESearchCase::CaseSensitive);
	Nom.ReplaceInline(TEXT("Ilette"), TEXT("\u00CElette"), ESearchCase::CaseSensitive);
	Nom.ReplaceInline(TEXT("Rene Salle"), TEXT("Ren\u00E9 Sall\u00E9"), ESearchCase::CaseSensitive);
	TexteNom->SetText(FText::FromString(Nom));
	TexteNomDos->SetText(FText::FromString(Nom));

	if (Panneau->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mid = Panneau->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), CouleurPanneau);
		}
	}
	if (Poteau->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mid = Poteau->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.35f, 0.38f));
		}
	}
	if (Quai->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mid = Quai->CreateAndSetMaterialInstanceDynamic(0))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.75f, 0.05f));
		}
	}

	if (bAfficherBanc && !Banc->GetStaticMesh())
	{
		if (UStaticMesh* MeshBanc = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Bus_Station/Meshes/SM_Bench_4.SM_Bench_4"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Banc->SetStaticMesh(MeshBanc);
		}
	}
	else if (!bAfficherBanc && Banc->GetStaticMesh())
	{
		Banc->SetStaticMesh(nullptr);
	}
}
