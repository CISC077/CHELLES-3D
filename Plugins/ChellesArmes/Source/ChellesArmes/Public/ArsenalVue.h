// CHELLES 3D - Arsenal : l'arme vue dans les mains du joueur (seulement sur sa machine)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArsenalTypes.h"
#include "ArsenalVue.generated.h"

class UAudioComponent;
class UPointLightComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Modele de l'arme accroche a la camera du joueur (vue 1re personne), dessine avec des formes simples
 * du moteur : aucun fichier a importer. Le sabre s'allume, bourdonne et frappe ; le blaster recule.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AArsenalVue : public AActor
{
	GENERATED_BODY()

public:
	AArsenalVue();

	virtual void Tick(float DeltaSeconds) override;

	/** Dessine l'arme demandee (Aucune = rien). */
	void Construire(uint8 NouvelleArme, const ChellesArsenal::FReglagesArsenal& R);

	/** Point de depart des tirs (bout du canon). */
	FVector Bouche() const;

	/** Coup de sabre / recul du blaster / fusee partie / grenade lancee. */
	void Tirer();

	uint8 Arme = 0;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	void Vider();

	UPROPERTY()
	USceneComponent* Racine = nullptr;

	UPROPERTY()
	USceneComponent* Pivot = nullptr;

	UPROPERTY()
	USceneComponent* PointBouche = nullptr;

	UPROPERTY()
	TArray<USceneComponent*> Pieces;

	UPROPERTY()
	UStaticMeshComponent* Lame = nullptr;

	UPROPERTY()
	UStaticMeshComponent* Ogive = nullptr;

	UPROPERTY()
	UPointLightComponent* Lueur = nullptr;

	/** Mitrailleuse lourde : les canons qui tournent. */
	UPROPERTY()
	USceneComponent* Canons = nullptr;
	float VitesseCanons = 0.f;

	UPROPERTY()
	UAudioComponent* Bourdon = nullptr;

	FVector PositionRepos = FVector::ZeroVector;
	FRotator PoseRepos = FRotator::ZeroRotator;
	float Temps = 0.f;
	float DebutAllumage = -100.f;
	float DebutTir = -100.f;
	float Rechargement = 1.f;
	float LongueurLame = 100.f;
};
