// CHELLES 3D - Wraith : un relais par joueur
#include "WraithRelais.h"

#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

AWraithRelais::AWraithRelais()
{
	bReplicates = true;
	bAlwaysRelevant = true; // les autres joueurs doivent connaitre le skin
	NetUpdateFrequency = 2.f;
	SetActorHiddenInGame(true);
	PrimaryActorTick.bCanEverTick = false;
}

void AWraithRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWraithRelais, Joueur);
	DOREPLIFETIME(AWraithRelais, Skin);
}

void AWraithRelais::ServeurSkin_Implementation(int32 NouveauSkin)
{
	Skin = FMath::Clamp(NouveauSkin, -1, 63);
	ForceNetUpdate();
}
