// CHELLES 3D - Commerces (2026-11-26) : reglages communs (Plugins\ChellesCommerces\Commerces.txt), UE 4.27.
#pragma once

#include "CoreMinimal.h"

namespace ChellesCommerces
{
	/** Ce que fait un article achete. */
	enum EEffet : uint8
	{
		EffetVie = 0,     // + Valeur points de vie (999 = vie au maximum)
		EffetRegen,       // + Valeur points de vie par seconde pendant Duree s
		EffetVitesse,     // course x Valeur pendant Duree s
		EffetSaut,        // saut x Valeur pendant Duree s
		EffetBouclier,    // Valeur % des degats recus rendus pendant Duree s
		EffetXP,          // + Valeur XP (plugin ChellesPouvoirs)
		EffetMunitions,   // + Valeur munitions des armes lourdes (plugin ChellesArmes)
		EffetGrattage,    // carte a gratter : 0 a Valeur pieces
		EffetInconnu
	};

	struct FArticle
	{
		FString Nom;
		FString Description;
		int32 Prix = 10;
		uint8 Effet = EffetVie;
		float Valeur = 0.f;
		float Duree = 0.f;
	};

	struct FCommerce
	{
		FString Cle;            // Boulangerie, Tabac, Bar
		FString Nom;            // titre affiche
		FName Tag;              // etiquette du comptoir dans la carte
		FLinearColor Couleur = FLinearColor::White;
		TArray<FArticle> Articles;
	};

	struct FReglagesCommerces
	{
		// ---- Pieces
		int32 PiecesDepart = 50;
		int32 ParZombie = 3;
		int32 ParEnnemi = 5;
		float ParXP = 0.25f;        // pieces par point d'XP gagne
		float ParMinute = 0.f;      // pieces par minute de jeu
		// ---- 27/11 : cadeau du jour et week-end
		bool bCadeauJour = true;
		int32 CadeauBase = 20;      // pieces le 1er jour
		int32 CadeauParJour = 10;   // pieces en plus par jour de suite
		int32 CadeauMax = 100;      // plafond
		float MultiWeekEnd = 2.f;   // pieces x2 le samedi et le dimanche (1 = rien)
		// ---- Boutiques
		float Distance = 250.f;     // cm du comptoir pour ouvrir la boutique
		bool bAfficherPieces = true;
		FString SonAchat = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_LapComplete_Cue.UI_LapComplete_Cue");
		FString SonPieces = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_Countdown_Cue.UI_Countdown_Cue");
		TArray<FCommerce> Commerces;
	};

	/** Lit Plugins\ChellesCommerces\Commerces.txt (les cles absentes gardent leur valeur par defaut). */
	void ChargerReglages(FReglagesCommerces& R);

	/** Nom lisible d'un effet ("VIE", "VITESSE"...) et l'inverse. */
	uint8 EffetDepuisTexte(const FString& Texte);
	FString TexteEffet(const FArticle& A);
}
