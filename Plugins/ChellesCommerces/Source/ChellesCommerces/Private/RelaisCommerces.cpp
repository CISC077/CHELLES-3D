// CHELLES 3D - Commerces : relais par joueur
#include "CommercesChelles.h"

#include "Engine/World.h"
#include "EngineUtils.h"

ARelaisCommerces::ARelaisCommerces()
{
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bAlwaysRelevant = false;
	NetUpdateFrequency = 2.f;
	SetReplicatingMovement(false);
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

ACommercesChelles* ARelaisCommerces::Gestionnaire() const
{
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<ACommercesChelles> It(W); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARelaisCommerces::ServeurAcheter_Implementation(uint8 Commerce, uint8 Article)
{
	if (ACommercesChelles* G = Gestionnaire())
	{
		G->AcheterServeur(this, Commerce, Article);
	}
}

void ARelaisCommerces::ClientResultat_Implementation(uint8 Commerce, uint8 Article, bool bOk, const FString& Message)
{
	if (ACommercesChelles* G = Gestionnaire())
	{
		G->ResultatLocal(Commerce, Article, bOk, Message);
	}
}

void ARelaisCommerces::ClientPieces_Implementation(int32 Quantite, const FString& Raison)
{
	if (ACommercesChelles* G = Gestionnaire())
	{
		G->GagnerPieces(Quantite, Raison);
	}
}

void ARelaisCommerces::ClientMouvement_Implementation(uint8 Effet, float Multiplicateur, float Duree)
{
	if (ACommercesChelles* G = Gestionnaire())
	{
		G->MouvementLocal(Effet, Multiplicateur, Duree);
	}
}
