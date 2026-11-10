// CHELLES 3D - Roue des modes en multijoueur : un relais par joueur
#include "RoueRelais.h"
#include "RoueModes.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ARoueRelais::ARoueRelais()
{
	bReplicates = true;
	bAlwaysRelevant = true; // tout le monde doit voir le costume des autres
	NetUpdateFrequency = 10.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void ARoueRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARoueRelais, Mode);
	DOREPLIFETIME(ARoueRelais, PionHabille);
	DOREPLIFETIME(ARoueRelais, PionCache);
}

ARoueModes* ARoueRelais::TrouverRoue() const
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return nullptr;
	}
	for (TActorIterator<ARoueModes> It(Monde); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ARoueRelais::ServeurChoisir_Implementation(int32 Index)
{
	if (ARoueModes* Roue = TrouverRoue())
	{
		Roue->ChoisirModeReseau(this, Index);
	}
}

void ARoueRelais::ClientAvis_Implementation(const FString& Texte)
{
	if (ARoueModes* Roue = TrouverRoue())
	{
		Roue->Message(Texte);
	}
}
