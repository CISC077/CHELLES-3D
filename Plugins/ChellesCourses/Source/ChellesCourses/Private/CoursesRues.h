// CHELLES 3D - Courses de rue : lecture des rues de la circulation (ChellesCirculation) PAR LEUR NOM,
// et fabrication des parcours (une marche au hasard, toujours la meme, dans les rues).
#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Les rues : des points relies entre eux (dans les deux sens, pour la course). */
struct FGrapheRues
{
	TArray<FVector> Points;
	TArray<TArray<int32>> Voisins;

	int32 Num() const { return Points.Num(); }
};

namespace CoursesRues
{
	/**
	 * Lit Noeuds (Position, Sorties) et Troncons (De, Vers) du Circulation Spawner pose dans la carte.
	 * Rend faux (avec la raison) si le Spawner est absent ou si son reseau n'est pas encore construit.
	 */
	bool Lire(UWorld* Monde, FGrapheRues& Graphe, FString& Pourquoi);

	/**
	 * Fabrique un parcours d'environ LongueurCm, en partant d'un carrefour loin des departs deja pris.
	 * Graine = toujours le meme parcours avec les memes rues. Points[0] = depart, puis une porte tous les
	 * EcartCm, la derniere = arrivee. Rend faux si aucun parcours n'a ete trouve.
	 */
	bool Fabriquer(const FGrapheRues& Graphe, int32 Graine, float LongueurCm, float EcartCm,
		const TArray<FVector>& DepartsPris, float EcartDepartsCm, TArray<FVector>& Points, float& LongueurReelleCm);
}
