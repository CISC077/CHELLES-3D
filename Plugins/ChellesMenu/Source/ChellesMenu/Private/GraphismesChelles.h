// CHELLES 3D - OPTIONS > AVANCES : reglages graphiques detailles (25/11/2026, agent Terrain)
// Ombres, reflets, effets/fluides, eau, post-process, vegetation, distance de vue, anti-aliasing,
// echelle de l'image, compteur FPS... Chaque reglage est enregistre dans GameUserSettings.ini
// (section [ChellesGraphismes], ou la qualite Unreal pour les groupes OMBRES, TEXTURES...).
// AUTO = le reglage suit le prereglage QUALITE BAS / MOYEN / HAUT de l'onglet GRAPHISMES.
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace ChellesGraphismes
{
	/** Nombre de lignes de reglage dans l'onglet AVANCES. */
	int32 Nombre();
	/** "NOM   < VALEUR >" pour la ligne i. */
	FText Texte(int32 i);
	/** Sens > 0 : choix suivant, Sens < 0 : choix precedent. */
	void Changer(int32 i, int32 Sens);

	/** Au debut de chaque partie (apres le prereglage QUALITE) : remet les choix du joueur. */
	void AppliquerAuDemarrage(UWorld* Monde);
	/** Le joueur a change QUALITE : tous les reglages fins repassent en AUTO. */
	void QualiteChangee();
	/** Test de la machine par Unreal, puis le meilleur prereglage pour ce PC. Renvoie le message a afficher. */
	FString DetecterMonPC();
	/** Tous les reglages fins en AUTO. */
	void ToutEnAuto();
	/** Derniere information a afficher sous les reglages (vide = rien). */
	FString Message();
}
