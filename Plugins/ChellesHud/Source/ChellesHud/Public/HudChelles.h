// CHELLES 3D - HUD du joueur : vie, niveau, XP, pouvoir choisi, zombies restants (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HudChelles.generated.h"

class APawn;
class APlayerController;
class SHudChelles;

/** Console Chelles.Hud 0 / 1 : cache ou montre le HUD sur ce PC. */
extern CHELLESHUD_API bool GHudChellesAffiche;

/**
 * Un seul par niveau, ajoute tout seul au lancement du jeu (Play) sur chaque PC qui affiche le jeu.
 * Il ne fait que LIRE : la vie du personnage joue, l'XP et le pouvoir du plugin ChellesPouvoirs,
 * l'invasion du plugin ChellesZombies. Il ne depend d'aucun de ces plugins : si l'un manque,
 * la partie correspondante du HUD ne s'affiche simplement pas.
 * Console (touche ²) : Chelles.Hud 0 pour cacher, Chelles.Hud 1 pour remontrer.
 */
UCLASS(NotPlaceable, Transient, meta = (DisplayName = "HUD Chelles"))
class CHELLESHUD_API AHudChelles : public AActor
{
	GENERATED_BODY()

public:
	AHudChelles();

	/** Afficher le HUD (Chelles.Hud 0 / 1 dans la console). */
	UPROPERTY(EditAnywhere, Category = "HUD")
	bool bAfficher = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	APlayerController* GetPC() const;
	void CreerHud();
	void RetirerHud();
	void ChercherActeurs();
	void LireVie();
	void LirePouvoirs();
	void LireZombies();

	TSharedPtr<SHudChelles> Hud;

	TWeakObjectPtr<AActor> Pouvoirs;
	TWeakObjectPtr<AActor> Relais;
	TWeakObjectPtr<AActor> Invasion;
	float ProchaineRecherche = 0.f;

	/** Vie lue a l'image precedente, pour faire clignoter l'ecran en rouge quand on prend un coup. */
	float VieAvant = -1.f;
	TWeakObjectPtr<APawn> PionAvant;
};
