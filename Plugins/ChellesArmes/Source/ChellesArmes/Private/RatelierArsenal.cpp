// CHELLES 3D - Ratelier d'arme lourde dans un cadre en bois (voir RatelierArsenal.h)
#include "RatelierArsenal.h"
#include "ArsenalTypes.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

using namespace ChellesArsenal;

namespace
{
	// Cadre : 150 cm de large, 70 cm de haut, poutres de 7 cm, planche du fond de 2 cm (axe X = sortie du mur)
	constexpr float RatelierLargeur = 150.f;
	constexpr float RatelierHauteur = 70.f;
	constexpr float RatelierPoutre = 7.f;
}

ARatelierArsenal::ARatelierArsenal()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 2.f;
	NetCullDistanceSquared = FMath::Square(15000.f);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// Pieces du cadre (taille en cm ; le cube du moteur mesure 100 cm)
	struct FPiece { const TCHAR* Nom; FVector Position; FVector Taille; };
	const float L = RatelierLargeur;
	const float H = RatelierHauteur;
	const float P = RatelierPoutre;
	const FPiece Pieces[] = {
		{ TEXT("Fond"),   FVector(1.f, 0.f, 0.f),               FVector(2.f, L, H) },
		{ TEXT("Haut"),   FVector(P * 0.5f, 0.f, H * 0.5f),     FVector(P + 2.f, L + P, P) },
		{ TEXT("Bas"),    FVector(P * 0.5f, 0.f, -H * 0.5f),    FVector(P + 2.f, L + P, P) },
		{ TEXT("Gauche"), FVector(P * 0.5f, -L * 0.5f, 0.f),    FVector(P + 2.f, P, H + P) },
		{ TEXT("Droite"), FVector(P * 0.5f, L * 0.5f, 0.f),     FVector(P + 2.f, P, H + P) },
		{ TEXT("CrochetG"), FVector(8.f, -L * 0.28f, -8.f),     FVector(12.f, 3.f, 3.f) },
		{ TEXT("CrochetD"), FVector(8.f, L * 0.28f, -8.f),      FVector(12.f, 3.f, 3.f) },
	};
	for (const FPiece& Piece : Pieces)
	{
		UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Piece.Nom);
		C->SetupAttachment(Racine);
		if (Cube.Succeeded())
		{
			C->SetStaticMesh(Cube.Object);
		}
		C->SetRelativeLocation(Piece.Position);
		C->SetRelativeScale3D(Piece.Taille / 100.f);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->bCastDynamicShadow = false;
		C->SetCullDistance(9000.f);
		Cadre.Add(C);
	}

	ModeleArme = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Arme"));
	ModeleArme->SetupAttachment(Racine);
	ModeleArme->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ModeleArme->SetGenerateOverlapEvents(false);
	ModeleArme->SetCanEverAffectNavigation(false);
	ModeleArme->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	ModeleArme->bCastDynamicShadow = false;
	ModeleArme->SetCullDistance(9000.f);

	ModeleSecours = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArmeSecours"));
	ModeleSecours->SetupAttachment(Racine);
	ModeleSecours->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ModeleSecours->SetCanEverAffectNavigation(false);
	ModeleSecours->SetVisibility(false);

	Etiquette = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Etiquette"));
	Etiquette->SetupAttachment(Racine);
	Etiquette->SetRelativeLocation(FVector(6.f, 0.f, H * 0.5f + 16.f));
	Etiquette->SetHorizontalAlignment(EHTA_Center);
	Etiquette->SetVerticalAlignment(EVRTA_TextCenter);
	Etiquette->SetWorldSize(13.f);
	Etiquette->SetTextRenderColor(FColor(255, 200, 80));
	Etiquette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Etiquette->SetCullDistance(2500.f);
}

void ARatelierArsenal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARatelierArsenal, Arme);
	DOREPLIFETIME(ARatelierArsenal, bVide);
}

void ARatelierArsenal::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FReglagesArsenal R;
	ChargerReglages(R);
	if (UMaterialInterface* Bois = LoadObject<UMaterialInterface>(nullptr, *R.BoisCadre, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		for (UStaticMeshComponent* C : Cadre)
		{
			if (C)
			{
				C->SetMaterial(0, Bois);
			}
		}
	}
	Habiller();
}

void ARatelierArsenal::OnRep_Etat()
{
	Habiller();
}

void ARatelierArsenal::Habiller()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ArmeHabillee != Arme)
	{
		ArmeHabillee = Arme;
		FReglagesArsenal R;
		ChargerReglages(R);
		const FString& Chemin = (Arme == LanceFlammes) ? R.ModeleFlammes : R.ModeleMitrailleuse;
		UObject* Modele = LoadObject<UObject>(nullptr, *Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet);
		USkeletalMesh* Squelette = Cast<USkeletalMesh>(Modele);
		UStaticMesh* Statique = Cast<UStaticMesh>(Modele);
		if (!Squelette && !Statique)
		{
			Statique = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		}
		// L'arme est couchee a l'horizontale dans le cadre, mise a la taille du cadre (90 % de la largeur)
		FBoxSphereBounds Bornes(ForceInit);
		USceneComponent* Affiche = nullptr;
		if (Squelette)
		{
			ModeleArme->SetSkeletalMesh(Squelette);
			ModeleArme->SetVisibility(true);
			ModeleSecours->SetVisibility(false);
			Bornes = Squelette->GetBounds();
			Affiche = ModeleArme;
		}
		else if (Statique)
		{
			ModeleSecours->SetStaticMesh(Statique);
			ModeleSecours->SetVisibility(true);
			ModeleArme->SetVisibility(false);
			Bornes = Statique->GetBounds();
			Affiche = ModeleSecours;
		}
		if (Affiche)
		{
			const FVector Taille = Bornes.BoxExtent * 2.f;
			const float PlusLong = FMath::Max3(Taille.X, Taille.Y, Taille.Z);
			const float Echelle = PlusLong > 1.f ? (RatelierLargeur * 0.85f) / PlusLong : 1.f;
			// On aligne le plus grand cote du modele avec la largeur du cadre (axe Y du ratelier)
			FRotator Rot = FRotator::ZeroRotator;
			if (Taille.X >= Taille.Y && Taille.X >= Taille.Z)
			{
				Rot = FRotator(0.f, 90.f, 0.f);
			}
			else if (Taille.Z > Taille.Y)
			{
				Rot = FRotator(0.f, 0.f, 90.f);
			}
			Affiche->SetRelativeRotation(Rot);
			Affiche->SetRelativeScale3D(FVector(Echelle));
			const FVector Decalage = Rot.RotateVector(Bornes.Origin * Echelle);
			Affiche->SetRelativeLocation(FVector(14.f, 0.f, 0.f) - Decalage);
		}
	}
	const bool bFlammes = Arme == LanceFlammes;
	ModeleArme->SetHiddenInGame(bVide);
	ModeleSecours->SetHiddenInGame(bVide);
	FReglagesArsenal R;
	ChargerReglages(R);
	const int32 XP = bFlammes ? R.FlammesXP : R.MitrailleuseXP;
	FString Texte = NomArme(Arme);
	if (bVide)
	{
		Texte += TEXT("\n(revient bientot)");
	}
	else if (XP > 0)
	{
		Texte += FString::Printf(TEXT("\n%d XP  -  touche %s"), XP, *R.ToucheRatelier);
	}
	else
	{
		Texte += FString::Printf(TEXT("\ntouche %s"), *R.ToucheRatelier);
	}
	Etiquette->SetText(FText::FromString(Texte));
}

FVector ARatelierArsenal::Centre() const
{
	return GetActorLocation() + GetActorForwardVector() * 15.f;
}

void ARatelierArsenal::Vider(float Attente)
{
	bVide = true;
	Retour = Attente;
	ForceNetUpdate();
	Habiller();
}

void ARatelierArsenal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !bVide)
	{
		return;
	}
	Retour -= DeltaSeconds;
	if (Retour <= 0.f)
	{
		bVide = false;
		ForceNetUpdate();
		Habiller();
	}
}
