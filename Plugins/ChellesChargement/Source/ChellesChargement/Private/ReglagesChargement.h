// CHELLES 3D - Ecran de chargement : reglages et astuces (lus dans Plugins\ChellesChargement\Astuces.txt)
#pragma once

#include "CoreMinimal.h"

struct FAstuceChelles
{
	/** Touche affichee dans la pastille (vide = pas de pastille). */
	FString Touche;
	FString Texte;
};

struct FReglagesChargement
{
	TArray<FAstuceChelles> Astuces;

	/** Temps minimum a l'ecran (secondes). */
	float DureeMini = 2.5f;
	/** Au debut de la partie : l'ecran part au plus tard apres ce temps (secondes). */
	float DureeMaxi = 10.f;
	/** Une astuce change toutes les X secondes. */
	float SecondesParAstuce = 4.f;
	/** Ecran pendant le chargement d'une carte (jeu final, rejoindre un serveur). */
	bool bChargementCarte = true;
	/** Ecran au debut de la partie, le temps que les textures arrivent (aussi avec Jouer dans l'editeur). */
	bool bDebutPartie = true;

	static FReglagesChargement& Get();

	/** Remet les valeurs par defaut puis lit Astuces.txt s'il existe. */
	void Charger();

private:
	void AstucesParDefaut();
};
