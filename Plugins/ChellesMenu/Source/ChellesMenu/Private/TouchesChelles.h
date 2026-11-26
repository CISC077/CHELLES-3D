// CHELLES 3D - Touches des plugins CHELLES modifiables par chaque joueur (25/11/2026, agent Terrain)
// Les plugins (pouvoir P, danse J, ARMEMENTS I, console ², apparence K...) lisent leur touche dans
// le code ou dans leur .txt. Plutot que de modifier chaque plugin, un pre-processeur Slate traduit
// la touche choisie par le joueur en touche d'origine : si POUVOIR = G, appuyer sur G envoie P au jeu,
// et P seul ne fait plus rien. Rien n'est traduit dans les menus CHELLES, les zones de texte
// (chat, recherche), la console ouverte ou l'editeur hors Play.
// Choix enregistres dans GameUserSettings.ini, section [ChellesTouches].
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

namespace ChellesTouches
{
	/** Installe le traducteur de touches (une seule fois). */
	void Installer();

	int32 Nombre();
	FString Libelle(int32 i);
	/** Touche d'origine (celle que le plugin attend). */
	FKey Origine(int32 i);
	/** Touche actuellement choisie (= origine si rien n'est change). */
	FKey Actuelle(int32 i);
	/** Index de la ligne qui utilise deja cette touche (INDEX_NONE = libre). Sauf = ligne ignoree. */
	int32 QuiUtilise(const FKey& K, int32 Sauf = INDEX_NONE);
	void Definir(int32 i, const FKey& K);
	void ParDefaut(int32 i);
	void ToutParDefaut();
}
