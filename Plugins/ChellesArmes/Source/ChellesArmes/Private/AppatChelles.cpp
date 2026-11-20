// CHELLES 3D - Arsenal : appat de la grenade a zombies
#include "AppatChelles.h"

#include "Components/SceneComponent.h"

AAppatChelles::AAppatChelles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false); // les zombies "frappent" la grenade sans rien lui faire
	SetActorHiddenInGame(true);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Tags.Add(TEXT("ChellesAppat"));
}
