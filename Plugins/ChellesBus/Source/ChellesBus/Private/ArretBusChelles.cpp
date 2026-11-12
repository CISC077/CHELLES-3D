// CHELLES 3D - Arret de bus (voir ArretBusChelles.h)
#include "ArretBusChelles.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AArretBusChelles::AArretBusChelles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylindre(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MateriauBase(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Poteau de 2,8 m
	Poteau = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Poteau"));
	Poteau->SetupAttachment(Racine);
	if (Cylindre.Succeeded()) { Poteau->SetStaticMesh(Cylindre.Object); }
	if (MateriauBase.Succeeded()) { Poteau->SetMaterial(0, MateriauBase.Object); }
	Poteau->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	Poteau->SetRelativeScale3D(FVector(0.08f, 0.08f, 2.8f));
	Poteau->SetCanEverAffectNavigation(false);

	// Panneau en haut du poteau (face a la route = axe X de l'acteur)
	Panneau = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panneau"));
	Panneau->SetupAttachment(Racine);
	if (Cube.Succeeded()) { Panneau->SetStaticMesh(Cube.Object); }
	if (MateriauBase.Succeeded()) { Panneau->SetMaterial(0, MateriauBase.Object); }
	Panneau->SetRelativeLocation(FVector(0.f, 0.f, 255.f));
	Panneau->SetRelativeScale3D(FVector(0.05f, 0.7f, 0.5f));
	Panneau->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Panneau->SetCanEverAffectNavigation(false);

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
	TexteBus = CreerTexte(TEXT("TexteBus"), FVector(3.5f, 0.f, 265.f), FRotator::ZeroRotator, 26.f, FColor::White);
	TexteBusDos = CreerTexte(TEXT("TexteBusDos"), FVector(-3.5f, 0.f, 265.f), FRotator(0.f, 180.f, 0.f), 26.f, FColor::White);
	TexteNom = CreerTexte(TEXT("TexteNom"), FVector(3.5f, 0.f, 242.f), FRotator::ZeroRotator, 9.f, FColor::Yellow);
	TexteBus->SetText(FText::FromString(TEXT("BUS")));
	TexteBusDos->SetText(FText::FromString(TEXT("BUS")));

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
		Icone->SetRelativeLocation(FVector(0.f, 0.f, 330.f));
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

void AArretBusChelles::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Le code est ecrit sans accents : le mot "Arret" est affiche avec son accent sur le panneau
	FString Nom = NomArret;
	Nom.ReplaceInline(TEXT("Arret"), TEXT("Arr\u00EAt"), ESearchCase::CaseSensitive);
	TexteNom->SetText(FText::FromString(Nom));

	if (Panneau->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Mid = Panneau->CreateAndSetMaterialInstanceDynamic(0);
		if (Mid) { Mid->SetVectorParameterValue(TEXT("Color"), CouleurPanneau); }
	}
	if (Poteau->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Mid = Poteau->CreateAndSetMaterialInstanceDynamic(0);
		if (Mid) { Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.35f, 0.35f, 0.38f)); }
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
