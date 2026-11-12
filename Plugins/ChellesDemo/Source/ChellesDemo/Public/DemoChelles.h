// CHELLES 3D - Version demo (beta) : 20 minutes de jeu puis ecran FIN DE LA DEMO.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoChelles.generated.h"

class APlayerController;
class SDemoChelles;

/**
 * Pose automatiquement au lancement du jeu (voir ChellesDemoModule), sur chaque PC, sans reseau.
 * Compte le temps de jeu (pas le temps passe dans les menus ni en pause),
 * affiche un petit compteur en haut, previent a 5 min et 1 min,
 * puis met le jeu en pause et affiche l'ecran FIN DE LA DEMO.
 *
 * Actif seulement dans le jeu exporte (pas dans l'editeur).
 * Pour tester dans l'editeur : console (touche 2) -> Chelles.Demo 1   (1 minute)   /   Chelles.Demo 0 (arreter)
 * Reglages : Config/DefaultGame.ini, section [ChellesDemo].
 */
UCLASS(NotPlaceable, Transient)
class CHELLESDEMO_API ADemoChelles : public AActor
{
	GENERATED_BODY()

public:
	ADemoChelles();

	/** Vrai si la demo doit tourner dans ce jeu (jeu exporte + Active=True, ou test lance depuis la console). */
	static bool DemoActive();
	/** Console Chelles.Demo <minutes> : lance un test dans l'editeur (0 = arreter). */
	static void LancerTest(UWorld* Monde, float Minutes);

	float SecondesRestantes() const;
	bool EstTerminee() const { return bTerminee; }
	/** Vrai quand le joueur est vraiment en train de jouer (pas en pause, pas dans un menu). */
	bool EnTrainDeJouer() const;
	/** Message d'alerte a afficher (vide si aucun). */
	FText Alerte() const;

	FText Message() const;
	FString Lien() const;
	void OuvrirLien();
	void QuitterLeJeu();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float Delta) override;

private:
	void Terminer();
	void MaintenirFin();
	void AjouterEcran();
	void RetirerEcran();
	void Memoriser();
	APlayerController* Joueur() const;

	TSharedPtr<SDemoChelles> Ecran;
	bool bTerminee = false;
	double ProchaineMemorisation = 0.0;
	double FinAlerte = 0.0;
	FText TexteAlerte;
	bool bAlerte5 = false;
	bool bAlerte1 = false;
};
