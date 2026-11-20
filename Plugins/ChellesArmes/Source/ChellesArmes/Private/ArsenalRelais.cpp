// CHELLES 3D - Arsenal : un relais par joueur
#include "ArsenalRelais.h"
#include "ArsenalChelles.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "RatelierArsenal.h"

AArsenalRelais::AArsenalRelais()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	NetUpdateFrequency = 5.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

static AArsenalChelles* GestionnaireArsenal(UWorld* W)
{
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<AArsenalChelles> It(W); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AArsenalRelais::ServeurTirer_Implementation(uint8 Arme, FVector Origine, FVector Direction)
{
	if (AArsenalChelles* G = GestionnaireArsenal(GetWorld()))
	{
		G->TirServeur(this, Arme, Origine, Direction);
	}
}

void AArsenalRelais::ServeurViser_Implementation(FVector_NetQuantize Point)
{
	PointVise = Point;
	if (UWorld* W = GetWorld())
	{
		HeureVise = W->GetTimeSeconds();
	}
}

void AArsenalRelais::ServeurSabre_Implementation(FVector Camera, FVector Direction)
{
	if (AArsenalChelles* G = GestionnaireArsenal(GetWorld()))
	{
		G->SabreServeur(this, Camera, Direction);
	}
}

void AArsenalRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArsenalRelais, ChargeFlammes);
	DOREPLIFETIME(AArsenalRelais, ChargeMitrailleuse);
}

void AArsenalRelais::ServeurFlammes_Implementation(FVector Camera, FVector Direction)
{
	if (AArsenalChelles* G = GestionnaireArsenal(GetWorld()))
	{
		G->FlammesServeur(this, Camera, Direction);
	}
}

void AArsenalRelais::ServeurPrendre_Implementation(ARatelierArsenal* Ratelier)
{
	if (AArsenalChelles* G = GestionnaireArsenal(GetWorld()))
	{
		G->PrendreServeur(this, Ratelier);
	}
}

void AArsenalRelais::ClientAvis_Implementation(const FString& Texte)
{
	if (AArsenalChelles* G = GestionnaireArsenal(GetWorld()))
	{
		G->AvisLocal(Texte);
	}
}

void AArsenalRelais::ClientPris_Implementation(uint8 Arme)
{
	if (AArsenalChelles* G = GestionnaireArsenal(GetWorld()))
	{
		G->ArmeLourdePrise(Arme);
	}
}
