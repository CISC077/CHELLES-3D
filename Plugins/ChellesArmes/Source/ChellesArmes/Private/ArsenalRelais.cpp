// CHELLES 3D - Arsenal : un relais par joueur
#include "ArsenalRelais.h"
#include "ArsenalChelles.h"

#include "Engine/World.h"
#include "EngineUtils.h"

AArsenalRelais::AArsenalRelais()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	NetUpdateFrequency = 2.f;
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
