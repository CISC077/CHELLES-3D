// CHELLES 3D - Modes de survie : les calculs "purs" (sans monde Unreal), verifies par les tests automatiques.
//   - courbe de difficulte des vagues (Vagues.txt)
//   - economie des points (Refuge.txt)
//   - catalogue des achats (Refuge.txt)
#pragma once

#include "CoreMinimal.h"

/** Lit une ligne "Cle=Valeur" (les lignes vides et celles qui commencent par # ou ; sont ignorees). */
struct CHELLESSURVIE_API FLigneReglage
{
	static bool Decouper(const FString& LigneBrute, FString& Cle, FString& Valeur);
	static float Nombre(const FString& Valeur);
	static bool EstOui(const FString& Valeur);
};

/** Ce qu'une vague doit contenir, calcule a partir de Vagues.txt. */
struct CHELLESSURVIE_API FVagueCalculee
{
	int32 Numero = 1;
	int32 Nombre = 6;
	/** Vie d'un zombie (100 = 3 balles dans le corps). */
	float Vie = 100.f;
	float Vitesse = 300.f;
	/** Part de costauds (0 a 1). */
	float PartCostauds = 0.f;
	float VieCostaud = 300.f;
	/** Vague speciale "coureurs" : tous les zombies plus rapides. */
	bool bCoureurs = false;
};

/** Courbe de difficulte : chaque valeur part d'une base et monte a chaque vague, jusqu'a un plafond. */
struct CHELLESSURVIE_API FCourbeVagues
{
	int32 ZombiesVague1 = 6;
	float ZombiesEnPlusParVague = 3.f;
	/** Multiplicateur du nombre de zombies par joueur en plus du premier (0,5 = +50 % par joueur). */
	float EnPlusParJoueur = 0.5f;
	int32 ZombiesMax = 60;

	float VieVague1 = 100.f;
	float VieEnPlusParVague = 10.f;
	float VieMax = 400.f;

	float VitesseVague1 = 300.f;
	float VitesseEnPlusParVague = 15.f;
	float VitesseMax = 560.f;

	/** Les costauds arrivent a partir de cette vague, leur part monte ensuite jusqu'au plafond. */
	int32 CostaudsAPartirDe = 4;
	float PartCostaudsDepart = 0.05f;
	float PartCostaudsEnPlusParVague = 0.03f;
	float PartCostaudsMax = 0.35f;
	/** Vie d'un costaud = vie normale x ce nombre. */
	float VieCostaudFois = 3.f;

	/** Toutes les X vagues : vague de coureurs (0 = jamais). */
	int32 CoureursToutesLes = 5;
	float CoureursVitesseFois = 1.35f;

	float PauseEntreVaguesSecondes = 15.f;

	/** Lit les lignes de Vagues.txt (les cles inconnues sont ignorees). */
	void Lire(const TArray<FString>& Lignes);
	/** La vague numero Vague (1, 2, 3...) pour NombreJoueurs joueurs. */
	FVagueCalculee Calculer(int32 Vague, int32 NombreJoueurs) const;
};

/** Points gagnes et perdus (Refuge.txt). */
struct CHELLESSURVIE_API FEconomieRefuge
{
	int32 PointsDepart = 500;
	int32 PointsParTouche = 10;
	int32 PointsParKill = 60;
	int32 PointsBonusTete = 40;
	int32 PointsRelever = 50;
	/** Part des points perdue quand on tombe a terre (0,1 = 10 %). */
	float PerteATerre = 0.1f;
	int32 XPParKill = 2;
	int32 XPBonusTete = 2;

	void Lire(const TArray<FString>& Lignes);
	/** Points d'un coup sur un zombie : touche simple, mort, ou mort par la tete. */
	int32 PointsPourCoup(bool bMort, bool bTete) const;
	int32 XPPourKill(bool bTete) const;
	/** Points perdus en tombant a terre avec Points en poche. */
	int32 PerteQuandATerre(int32 Points) const;
	/** Vrai et Points diminue si on peut payer Prix, sinon faux et rien ne change. */
	static bool Payer(int32& Points, int32 Prix);
};

/** Une arme a vendre (Arme.Nom=Blueprint|Prix dans Refuge.txt). */
struct CHELLESSURVIE_API FArticleRefuge
{
	FString Nom;
	FString Classe;
	int32 Prix = 0;
};

struct CHELLESSURVIE_API FCatalogueRefuge
{
	TArray<FArticleRefuge> Armes;
	int32 PrixMunitions = 400;
	int32 PrixBarriere = 750;

	void Lire(const TArray<FString>& Lignes);
	const FArticleRefuge* Trouver(const FString& Nom) const;
	/** "BPC_WeaponRifle" -> "/Game/FPS_Game/Blueprints/Weapons/BPC_WeaponRifle.BPC_WeaponRifle_C" (un chemin complet reste tel quel). */
	static FString CheminClasse(const FString& NomOuChemin);
};
