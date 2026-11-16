// CHELLES 3D - Voitures de police en multijoueur : un relais par joueur
#include "RelaisPolice.h"
#include "PoliceChelles.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ARelaisPolice::ARelaisPolice()
{
	bReplicates = true;
	bAlwaysRelevant = true; // tout le monde doit savoir qui conduit quelle voiture (sirene, soldat cache)
	NetUpdateFrequency = 10.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void ARelaisPolice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARelaisPolice, Voiture);
	DOREPLIFETIME(ARelaisPolice, PietonCache);
	DOREPLIFETIME(ARelaisPolice, bSirene);
	DOREPLIFETIME(ARelaisPolice, bAdmin);
}

APoliceChelles* ARelaisPolice::Gestionnaire() const
{
	if (UWorld* Monde = GetWorld())
	{
		for (TActorIterator<APoliceChelles> It(Monde); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARelaisPolice::ServeurPoser_Implementation(FVector Lieu, float Yaw)
{
	APoliceChelles* G = Gestionnaire();
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!G || !PC)
	{
		return;
	}
	if (!G->EstAdministrateur(PC))
	{
		ClientAvis(TEXT("Seul l'administrateur du jeu (l'hote) peut poser des voitures de police."), 4.f);
		return;
	}
	if (G->PoserVoiture(Lieu, Yaw, true))
	{
		ClientAvis(FString::Printf(TEXT("Voiture de police posee (%d sur la carte). F9 en la visant pour la retirer."), G->Posees.Num()), 4.f);
	}
	else
	{
		ClientAvis(TEXT("Impossible de poser une voiture de police ici (voir le journal [Police])."), 4.f);
	}
}

void ARelaisPolice::ServeurRetirer_Implementation(FVector Point)
{
	APoliceChelles* G = Gestionnaire();
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!G || !PC)
	{
		return;
	}
	if (!G->EstAdministrateur(PC))
	{
		ClientAvis(TEXT("Seul l'administrateur du jeu (l'hote) peut retirer des voitures de police."), 4.f);
		return;
	}
	FString Raison;
	if (G->RetirerVoitureProche(Point, Raison))
	{
		ClientAvis(FString::Printf(TEXT("Voiture de police retiree (%d sur la carte)."), G->Posees.Num()), 4.f);
	}
	else
	{
		ClientAvis(Raison, 4.f);
	}
}

void ARelaisPolice::ServeurMonter_Implementation(APawn* VoitureVisee)
{
	if (APoliceChelles* G = Gestionnaire())
	{
		G->MonterServeur(this, VoitureVisee);
	}
}

void ARelaisPolice::ServeurDescendre_Implementation()
{
	if (APoliceChelles* G = Gestionnaire())
	{
		G->DescendreServeur(this);
	}
}

void ARelaisPolice::ServeurSirene_Implementation(bool bActive)
{
	bSirene = bActive;
	ForceNetUpdate();
}

void ARelaisPolice::ClientAvis_Implementation(const FString& Texte, float Duree)
{
	if (APoliceChelles* G = Gestionnaire())
	{
		G->Message(Texte, Duree);
	}
}
