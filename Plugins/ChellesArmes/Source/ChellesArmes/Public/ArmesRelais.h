// CHELLES 3D - Armes : un relais par joueur, cree par le serveur (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArmesRelais.generated.h"

/**
 * Chaque joueur a son relais (proprietaire = son PlayerController), en solo comme en multi.
 * Le joueur demande au serveur de lancer un tir special ; le serveur cree le projectile, vu par tous.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AArmesRelais : public AActor
{
	GENERATED_BODY()

public:
	AArmesRelais();

	/** Le joueur tire un tir special depuis Origine (bout du canon) dans la direction Direction. */
	UFUNCTION(Server, Reliable)
	void ServeurTir(uint8 Type, FVector Origine, FVector Direction);

	/** Serveur : heure du dernier tir accepte, par type (anti-mitraillage). */
	float DernierTir[8] = { -100.f, -100.f, -100.f, -100.f, -100.f, -100.f, -100.f, -100.f };
};
