// CHELLES 3D - Arsenal : degats, explosions et poussee (serveur)
#pragma once

#include "CoreMinimal.h"

class AActor;
class APawn;
class UWorld;
struct FHitResult;

namespace ChellesArsenalDegats
{
	/** Reglage PVP du menu (en solo : toujours vrai, il n'y a que des PNJ). */
	bool PvpActif(UWorld* Monde);

	/** Zombie de l'invasion (plugin ChellesZombies) ? */
	bool EstZombie(const AActor* Acteur);

	/** Peut-on blesser cette cible ? (pas soi-meme, pas les autres joueurs si PVP coupe) */
	bool PeutBlesser(const AActor* Cible, const APawn* Lanceur, bool bToucherJoueurs);

	/** Coup precis : on cherche l'os touche, le plugin Zombies decide (tete = mort, corps = 33 %). */
	void Blesser(AActor* Cible, float Quantite, const FHitResult& Impact, const FVector& Direction, APawn* Lanceur, AActor* Cause);

	/** Coup qui tue un zombie a coup sur (touche "a la tete"). Les autres cibles prennent Quantite. */
	void Trancher(AActor* Cible, float Quantite, const FVector& Point, const FVector& Direction, APawn* Lanceur, AActor* Cause);

	/**
	 * Explosion : degats autour, les personnages sont projetes, les objets physiques pousses.
	 * Les zombies proches du centre (moitie du rayon) sont tues net.
	 */
	void Explosion(UWorld* Monde, const FVector& Point, float Rayon, float Degats, float Poussee, APawn* Lanceur, AActor* Cause, bool bToucherJoueurs);
}
