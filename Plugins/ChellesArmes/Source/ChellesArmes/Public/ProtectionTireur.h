// CHELLES 3D - Protection du tireur (27/11/2026, agent Armements) - UE 4.27
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProtectionTireur.generated.h"

class APawn;
class AController;
class UDamageType;

/**
 * Le tireur ne se blesse plus avec ses propres balles (bug du 27/11 : le SNIPER tuait Rachid).
 * Chaque projectile qui apparait (classe contenant "Projectile") ignore le personnage qui l'a tire
 * et les objets accroches a lui (baton du skin Wukong, armes...). Sur chaque machine, sans Blueprint modifie.
 * Journal : chaque degat recu par le joueur local est ecrit ([Protection] ... de <cause>).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AProtectionTireur : public AActor
{
	GENERATED_BODY()

public:
	AProtectionTireur();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void ActeurApparu(AActor* Acteur);

	UFUNCTION()
	void DegatsRecus(AActor* Touche, float Degats, const UDamageType* Type, AController* Auteur, AActor* Cause);

	FDelegateHandle Abonnement;
	TWeakObjectPtr<APawn> PionSuivi;
	int32 Proteges = 0;
};
