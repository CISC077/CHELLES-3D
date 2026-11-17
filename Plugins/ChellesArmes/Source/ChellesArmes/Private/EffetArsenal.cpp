// CHELLES 3D - Arsenal : petit effet visible par tous
#include "EffetArsenal.h"
#include "ArmesEffets.h"
#include "SonArsenalChelles.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

AEffetArsenal::AEffetArsenal()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	InitialLifeSpan = 1.5f;
	PrimaryActorTick.bCanEverTick = false;
	NetCullDistanceSquared = FMath::Square(6000.f);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AEffetArsenal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEffetArsenal, Genre);
	DOREPLIFETIME(AEffetArsenal, Couleur);
}

void AEffetArsenal::Creer(UWorld* Monde, uint8 NouveauGenre, const FVector& Ou, const FLinearColor& NouvelleCouleur, APawn* Auteur)
{
	if (!Monde)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Instigator = Auteur;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	const FTransform Ici(Ou);
	if (AEffetArsenal* E = Monde->SpawnActor<AEffetArsenal>(AEffetArsenal::StaticClass(), Ici, Params))
	{
		E->Genre = NouveauGenre;
		E->Couleur = NouvelleCouleur.ToFColor(true);
		E->FinishSpawning(Ici);
	}
}

void AEffetArsenal::BeginPlay()
{
	Super::BeginPlay();
	UWorld* W = GetWorld();
	if (!W || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Ici = GetActorLocation();
	switch (Genre)
	{
	case ChellesEffetArsenal::CoupSabre:
	{
		// Le joueur qui frappe entend deja son propre coup (joue tout de suite chez lui)
		APlayerController* PC = W->GetFirstPlayerController();
		const bool bMoi = PC && PC->GetPawn() && PC->GetPawn() == GetInstigator();
		if (!bMoi)
		{
			USonArsenalChelles::Jouer(this, ChellesArsenalSons::CoupSabre, Ici, 0.8f);
		}
		break;
	}
	case ChellesEffetArsenal::Etincelles:
		ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Etincelles(), Ici, 0.8f, 0.4f);
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::Impact, Ici, 0.7f, 0.6f);
		break;
	default:
		break;
	}
}
