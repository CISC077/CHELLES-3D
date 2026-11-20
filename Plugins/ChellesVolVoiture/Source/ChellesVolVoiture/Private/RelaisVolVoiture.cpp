// CHELLES 3D - Vol de voiture : un relais par joueur (voir RelaisVolVoiture.h)
#include "RelaisVolVoiture.h"
#include "VolVoitureChelles.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ARelaisVolVoiture::ARelaisVolVoiture()
{
	bReplicates = true;
	bAlwaysRelevant = true; // tout le monde doit savoir quel pieton est cache
	NetUpdateFrequency = 5.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void ARelaisVolVoiture::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARelaisVolVoiture, Etoiles);
	DOREPLIFETIME(ARelaisVolVoiture, Voiture);
	DOREPLIFETIME(ARelaisVolVoiture, PietonCache);
}

AVolVoitureChelles* ARelaisVolVoiture::Gestionnaire() const
{
	if (UWorld* Monde = GetWorld())
	{
		for (TActorIterator<AVolVoitureChelles> It(Monde); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARelaisVolVoiture::ServeurVolerCirculation_Implementation(FVector Lieu, float Yaw, const FString& Modele)
{
	if (AVolVoitureChelles* G = Gestionnaire())
	{
		G->VolerServeur(this, Lieu, Yaw, Modele);
	}
}

void ARelaisVolVoiture::ServeurMonter_Implementation(APawn* VoitureVisee)
{
	if (AVolVoitureChelles* G = Gestionnaire())
	{
		G->MonterServeur(this, VoitureVisee, false);
	}
}

void ARelaisVolVoiture::ServeurDescendre_Implementation()
{
	if (AVolVoitureChelles* G = Gestionnaire())
	{
		G->DescendreServeur(this);
	}
}

void ARelaisVolVoiture::ClientAvis_Implementation(const FString& Texte, float Duree)
{
	if (AVolVoitureChelles* G = Gestionnaire())
	{
		G->Message(Texte, Duree);
	}
}
