// CHELLES 3D - Butin : coffre lumineux
#include "CoffreButin.h"
#include "ButinTypes.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

using namespace ChellesButin;

ACoffreButin::ACoffreButin()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	NetCullDistanceSquared = FMath::Square(30000.f);
	NetUpdateFrequency = 2.f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylindre(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Modele = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Modele"));
	Modele->SetupAttachment(Racine);
	Modele->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Modele->SetCanEverAffectNavigation(false);
	Modele->SetCullDistance(15000.f);
	if (Cube.Succeeded())
	{
		Modele->SetStaticMesh(Cube.Object); // remplace par le coffre du pack au demarrage
		Modele->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.55f));
		Modele->SetRelativeLocation(FVector(0.f, 0.f, 27.5f));
	}

	// Colonne de lumiere au-dessus du coffre : on la voit de loin
	Colonne = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Colonne"));
	Colonne->SetupAttachment(Racine);
	Colonne->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Colonne->SetCanEverAffectNavigation(false);
	Colonne->CastShadow = false;
	Colonne->SetCullDistance(30000.f);
	if (Cylindre.Succeeded())
	{
		Colonne->SetStaticMesh(Cylindre.Object);
		Colonne->SetRelativeScale3D(FVector(0.06f, 0.06f, 22.f));
		Colonne->SetRelativeLocation(FVector(0.f, 0.f, 1150.f));
	}

	Lumiere = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lumiere"));
	Lumiere->SetupAttachment(Racine);
	Lumiere->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	Lumiere->SetIntensity(3000.f);
	Lumiere->SetAttenuationRadius(400.f);
	Lumiere->SetCastShadows(false);
	Lumiere->SetMobility(EComponentMobility::Movable);
}

void ACoffreButin::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACoffreButin, Rarete);
	DOREPLIFETIME(ACoffreButin, bOuvert);
}

void ACoffreButin::BeginPlay()
{
	Super::BeginPlay();
	Habiller();
}

void ACoffreButin::OnRep_Etat()
{
	Habiller();
}

void ACoffreButin::Colorer(UStaticMeshComponent* C, const FLinearColor& Couleur)
{
	if (!C)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Deja = Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)))
	{
		Deja->SetVectorParameterValue(TEXT("Color"), Couleur);
		return;
	}
	UMaterialInterface* Forme = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0, Forme ? Forme : C->GetMaterial(0)))
	{
		M->SetVectorParameterValue(TEXT("Color"), Couleur);
	}
}

void ACoffreButin::Habiller()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FLinearColor Couleur = CouleurRarete(Rarete);
	Lumiere->SetLightColor(Couleur);
	Lumiere->SetIntensity(bOuvert ? 600.f : (Rarete == Legendaire ? 6000.f : 3000.f));
	Colonne->SetVisibility(!bOuvert);
	if (!bHabille || bHabilleOuvert != bOuvert)
	{
		FReglagesButin R;
		ChargerReglagesButin(R);
		UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, bOuvert ? *R.ModeleOuvert : *R.ModeleCoffre, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!M && bOuvert)
		{
			M = LoadObject<UStaticMesh>(nullptr, *R.ModeleCoffre, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (M)
		{
			Modele->SetStaticMesh(M);
			// le plus grand cote du coffre fait environ 1 m, pose sur le sol
			const FBoxSphereBounds B = M->GetBounds();
			const float Grand = 2.f * FMath::Max(B.BoxExtent.X, B.BoxExtent.Y);
			const float K = Grand > 1.f ? FMath::Clamp(100.f / Grand, 0.05f, 20.f) : 1.f;
			Modele->SetRelativeScale3D(FVector(K));
			Modele->SetRelativeLocation(FVector(-B.Origin.X * K, -B.Origin.Y * K, -(B.Origin.Z - B.BoxExtent.Z) * K));
		}
		else if (!bHabille)
		{
			Colorer(Modele, Couleur * 0.6f); // pas de pack : simple boite coloree
		}
		bHabille = true;
		bHabilleOuvert = bOuvert;
	}
	Colorer(Colonne, Couleur);
	if (bOuvert && !bSonJoue)
	{
		bSonJoue = true;
		FReglagesButin R;
		ChargerReglagesButin(R);
		if (USoundBase* Son = LoadObject<USoundBase>(nullptr, *R.SonCoffre, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Son, Centre(), 1.f, 1.f);
		}
	}
}

void ACoffreButin::Ouvrir()
{
	if (bOuvert)
	{
		return;
	}
	bOuvert = true;
	ForceNetUpdate();
	Habiller();
	SetLifeSpan(8.f);
}

FVector ACoffreButin::Centre() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 40.f);
}

void ACoffreButin::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;
	if (GetNetMode() == NM_DedicatedServer || bOuvert)
	{
		return;
	}
	// La lumiere "respire" doucement
	const float Base = Rarete == Legendaire ? 6000.f : 3000.f;
	Lumiere->SetIntensity(Base * (0.75f + 0.25f * FMath::Sin(Temps * 3.f)));
}
