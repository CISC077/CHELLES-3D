// CHELLES 3D - Armes : un relais par joueur
#include "ArmesRelais.h"
#include "ArmesChelles.h"

#include "Engine/World.h"
#include "EngineUtils.h"

AArmesRelais::AArmesRelais()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	NetUpdateFrequency = 2.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void AArmesRelais::ServeurTir_Implementation(uint8 Type, FVector Origine, FVector Direction)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	for (TActorIterator<AArmesChelles> It(W); It; ++It)
	{
		It->TirServeur(this, Type, Origine, Direction);
		return;
	}
}
