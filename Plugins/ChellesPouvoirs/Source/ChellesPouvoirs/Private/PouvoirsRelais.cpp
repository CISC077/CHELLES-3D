// CHELLES 3D - XP et pouvoirs : un relais par joueur
#include "PouvoirsRelais.h"
#include "PouvoirsChelles.h"
#include "PouvoirsTypes.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

APouvoirsRelais::APouvoirsRelais()
{
	bReplicates = true;
	bAlwaysRelevant = true; // tout le monde doit voir les effets des autres
	NetUpdateFrequency = 10.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
	FinEffet.Init(-1.f, ChellesPouvoirs::NombrePouvoirs);
	FinRecharge.Init(-1.f, ChellesPouvoirs::NombrePouvoirs);
}

void APouvoirsRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APouvoirsRelais, XP);
	DOREPLIFETIME(APouvoirsRelais, Pion);
	DOREPLIFETIME(APouvoirsRelais, FinEffet);
	DOREPLIFETIME(APouvoirsRelais, FinRecharge);
	DOREPLIFETIME(APouvoirsRelais, CibleGel);
	DOREPLIFETIME(APouvoirsRelais, Deguisement);
}

float APouvoirsRelais::Maintenant() const
{
	const UWorld* W = GetWorld();
	if (!W)
	{
		return 0.f;
	}
	if (const AGameStateBase* Etat = W->GetGameState())
	{
		return Etat->GetServerWorldTimeSeconds();
	}
	return W->GetTimeSeconds();
}

bool APouvoirsRelais::EffetActif(int32 Quel) const
{
	return FinEffet.IsValidIndex(Quel) && FinEffet[Quel] > Maintenant();
}

float APouvoirsRelais::RestantEffet(int32 Quel) const
{
	return FinEffet.IsValidIndex(Quel) ? FMath::Max(0.f, FinEffet[Quel] - Maintenant()) : 0.f;
}

float APouvoirsRelais::RestantRecharge(int32 Quel) const
{
	return FinRecharge.IsValidIndex(Quel) ? FMath::Max(0.f, FinRecharge[Quel] - Maintenant()) : 0.f;
}

APouvoirsChelles* APouvoirsRelais::TrouverGestionnaire() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<APouvoirsChelles> It(W); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void APouvoirsRelais::ServeurXP_Implementation(int32 NouvelleXP)
{
	XP = FMath::Clamp(NouvelleXP, 0, 10000000);
	ForceNetUpdate();
}

void APouvoirsRelais::ServeurPouvoir_Implementation(uint8 Quel, FVector Origine, FRotator Visee)
{
	if (APouvoirsChelles* Gestionnaire = TrouverGestionnaire())
	{
		Gestionnaire->ExecuterServeur(this, int32(Quel), Origine, Visee);
	}
}

void APouvoirsRelais::ClientAvis_Implementation(const FString& Texte)
{
	if (APouvoirsChelles* Gestionnaire = TrouverGestionnaire())
	{
		Gestionnaire->Message(Texte);
	}
}
