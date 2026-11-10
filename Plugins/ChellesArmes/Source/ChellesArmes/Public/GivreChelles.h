// CHELLES 3D - Armes : gel d'une cible touchee par le tir GLACIAL (cree par le serveur, vu par tous)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GivreChelles.generated.h"

class APawn;
class UPointLightComponent;
class UParticleSystemComponent;

/**
 * Petit acteur accroche a la cible pendant le gel : sur chaque machine il fige son animation
 * et ses mouvements, ajoute une lueur bleue, puis libere la cible en disparaissant.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AGivreChelles : public AActor
{
	GENERATED_BODY()

public:
	AGivreChelles();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Serveur : gele Cible pendant Duree secondes (un nouveau gel remplace l'ancien). */
	static void Geler(UWorld* Monde, APawn* Cible, float Duree, APawn* Auteur);

	UPROPERTY(ReplicatedUsing = OnRep_Cible)
	APawn* Cible = nullptr;

	UPROPERTY(Replicated)
	float Duree = 3.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	UFUNCTION()
	void OnRep_Cible();

	void Commencer();
	void Finir();

	TWeakObjectPtr<APawn> Gele;
	bool bPret = false;

	UPROPERTY()
	UPointLightComponent* Lueur = nullptr;

	UPROPERTY()
	UParticleSystemComponent* Brume = nullptr;
};
