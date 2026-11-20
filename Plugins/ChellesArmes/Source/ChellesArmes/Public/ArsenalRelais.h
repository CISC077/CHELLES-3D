// CHELLES 3D - Arsenal : un relais par joueur, cree par le serveur (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "ArsenalRelais.generated.h"

/**
 * Chaque joueur a son relais (proprietaire = son PlayerController), en solo comme en multi.
 * Le joueur demande au serveur de tirer ; le serveur cree le projectile, vu par tous.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AArsenalRelais : public AActor
{
	GENERATED_BODY()

public:
	AArsenalRelais();

	/** Blaster, bazooka ou grenade : tir depuis Origine (bout de l'arme) vers Direction. */
	UFUNCTION(Server, Reliable)
	void ServeurTirer(uint8 Arme, FVector Origine, FVector Direction);

	/** Bazooka : le point que vise le joueur (la fusee le suit). Envoye ~15 fois par seconde. */
	UFUNCTION(Server, Unreliable)
	void ServeurViser(FVector_NetQuantize Point);

	/** Sabre lumineux : coup donne depuis Camera dans la direction Direction. */
	UFUNCTION(Server, Reliable)
	void ServeurSabre(FVector Camera, FVector Direction);

	// --- Serveur seulement ---
	float DernierTir[8] = { -100.f, -100.f, -100.f, -100.f, -100.f, -100.f, -100.f, -100.f };
	FVector PointVise = FVector::ZeroVector;
	float HeureVise = -100.f;
};
