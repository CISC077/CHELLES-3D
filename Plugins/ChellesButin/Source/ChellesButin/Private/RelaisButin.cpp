// CHELLES 3D - Butin : relais par joueur
#include "RelaisButin.h"
#include "ButinChelles.h"
#include "CoffreButin.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ARelaisButin::ARelaisButin()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	NetUpdateFrequency = 4.f;
	SetReplicatingMovement(false);
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void ARelaisButin::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARelaisButin, Serie);
	DOREPLIFETIME(ARelaisButin, bRadar);
	DOREPLIFETIME(ARelaisButin, bFrappePrete);
}

AButinChelles* ARelaisButin::Gestionnaire() const
{
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AButinChelles> It(W); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARelaisButin::ServeurOuvrir_Implementation(ACoffreButin* Coffre)
{
	if (AButinChelles* G = Gestionnaire())
	{
		G->OuvrirServeur(this, Coffre);
	}
}

void ARelaisButin::ServeurPing_Implementation(FVector_NetQuantize Point, uint8 Genre)
{
	if (AButinChelles* G = Gestionnaire())
	{
		G->PingServeur(this, Point, Genre);
	}
}

void ARelaisButin::ServeurFrappe_Implementation(FVector_NetQuantize Point)
{
	if (AButinChelles* G = Gestionnaire())
	{
		G->FrappeServeur(this, Point);
	}
}

void ARelaisButin::ClientAnnonce_Implementation(const FString& Texte, uint8 Couleur, bool bSon)
{
	if (AButinChelles* G = Gestionnaire())
	{
		G->Annonce(Texte, Couleur, bSon);
	}
}

void ARelaisButin::ClientXP_Implementation(int32 Quantite, const FString& Raison)
{
	if (AButinChelles* G = Gestionnaire())
	{
		G->DonnerXPLocale(Quantite, Raison);
	}
}
