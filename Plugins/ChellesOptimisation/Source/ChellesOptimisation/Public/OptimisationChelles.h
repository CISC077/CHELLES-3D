// CHELLES 3D - Optimisation : neige autour du joueur, fleurs de pres, PNJ lointains caches (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OptimisationChelles.generated.h"

class ACharacter;
class UPrimitiveComponent;
class UInstancedStaticMeshComponent;

/** Reglages lus dans Plugins\ChellesOptimisation\Optimisation.txt (modifiable avec le Bloc-notes). */
struct FReglagesOptimisation
{
	bool bActif = true;
	bool bPNJ = true;
	float DistancePNJ = 10000.f;        // cm
	bool bFleurs = true;
	float DistanceFleurs = 4000.f;      // cm
	float TailleMaxFleurs = 300.f;      // cm
	bool bNeigeLointaine = false;
	float TailleNeige = 6000.f;         // cm
};

/**
 * Ajoute tout seul au lancement du jeu, sur chaque machine (solo, serveur et joueurs en multi).
 * Ne touche a aucun fichier du projet : tout est fait en memoire pendant la partie.
 *  - Neige : la grande couche lointaine est coupee, seule la couche qui suit la camera reste.
 *  - Fleurs / herbe : distance d'affichage courte (chaque machine pour sa propre camera).
 *  - PNJ : distance d'affichage 100 m (chaque camera) ; au-dela de 100 m de tous les joueurs,
 *    animation, deplacement et IA en pause (decide par le serveur, rien n'est envoye sur le reseau).
 */
UCLASS(NotPlaceable, Transient)
class CHELLESOPTIMISATION_API AOptimisationChelles : public AActor
{
	GENERATED_BODY()

public:
	AOptimisationChelles();

	/** Lit Optimisation.txt. */
	void LireReglages();

	/** Neige : appele AVANT le BeginPlay des autres acteurs (pour que ChellesNeige parte des nouvelles valeurs). */
	void PreparerNeige();

	FReglagesOptimisation Reglages;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	// ------------------------------------------------------------------ PNJ
	struct FDistanceOrigine
	{
		TWeakObjectPtr<UPrimitiveComponent> Composant;
		float Origine = 0.f;
	};

	struct FEtatPNJ
	{
		TArray<FDistanceOrigine> Distances;
		bool bLoin = false;
		bool bGele = false;           // pause serveur (deplacement, IA, tick)
		bool bTickMesh = true;
		bool bTickMouvement = true;
		bool bTickActeur = true;
		bool bLogiquePausee = false;
	};

	void VerifierPNJ();
	void AppliquerDistanceAffichage(ACharacter* Perso, FEtatPNJ& Etat);
	void AjouterComposants(AActor* Acteur, FEtatPNJ& Etat);
	void Endormir(ACharacter* Perso, FEtatPNJ& Etat, bool bAutorite);
	void Reveiller(ACharacter* Perso, FEtatPNJ& Etat);
	void RendreTout(ACharacter* Perso, FEtatPNJ& Etat);
	void ReglerPassants();

	TMap<TWeakObjectPtr<ACharacter>, FEtatPNJ> PNJ;
	FTimerHandle MinuteurPNJ;

	// ------------------------------------------------------------------ Fleurs
	void PremierPassageFleurs();
	void VerifierHerbe();
	bool RaccourcirInstances(UInstancedStaticMeshComponent* Comp, bool bVerifierTaille);

	TArray<TWeakObjectPtr<AActor>> Landscapes;
	TSet<TWeakObjectPtr<UInstancedStaticMeshComponent>> HerbeTraitee;
	FTimerHandle MinuteurHerbe;
	int32 NbFoliage = 0;
	int32 NbPlantes = 0;
};
