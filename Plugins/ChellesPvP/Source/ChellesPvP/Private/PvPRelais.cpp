// CHELLES 3D - PvP : relais d'un joueur
#include "PvPRelais.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#include "PvPChelles.h"

APvPRelais::APvPRelais()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 10.f;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void APvPRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APvPRelais, Nom);
	DOREPLIFETIME(APvPRelais, Pion);
	DOREPLIFETIME(APvPRelais, Etat);
	DOREPLIFETIME(APvPRelais, Coups);
	DOREPLIFETIME(APvPRelais, Kits);
	DOREPLIFETIME(APvPRelais, FinATerre);
	DOREPLIFETIME(APvPRelais, MisesATerre);
	DOREPLIFETIME(APvPRelais, Eliminations);
	DOREPLIFETIME(APvPRelais, Reanimations);
	DOREPLIFETIME(APvPRelais, FoisATerre);
}

APlayerController* APvPRelais::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

float APvPRelais::Maintenant() const
{
	const UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return 0.f;
	}
	const AGameStateBase* EtatJeu = Monde->GetGameState();
	return EtatJeu ? EtatJeu->GetServerWorldTimeSeconds() : Monde->GetTimeSeconds();
}

APvPChelles* APvPRelais::Gestionnaire() const
{
	for (TActorIterator<APvPChelles> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void APvPRelais::ServeurJeterKit_Implementation()
{
	if (APvPChelles* G = Gestionnaire())
	{
		G->JeterKit(this);
	}
}

void APvPRelais::ClientAvis_Implementation(const FString& Texte)
{
	if (APvPChelles* G = Gestionnaire())
	{
		G->MessageLocal(Texte);
	}
}
