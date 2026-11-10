// CHELLES 3D - HUD du joueur : le dessin (Slate, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

/** Ce que le HUD doit afficher, rempli a chaque image par AHudChelles. */
struct FEtatHudChelles
{
	bool bVisible = true;

	// Vie du personnage joue (bVie = false : ce personnage n'a pas de vie, ex. cheval, motoneige)
	bool bVie = false;
	float Vie = 0.f;
	float VieMax = 100.f;
	bool bMort = false;
	/** Secondes depuis le dernier coup recu (pour le flash rouge). */
	float AgeCoup = 100.f;

	// XP et pouvoirs (bPouvoirs = false : plugin ChellesPouvoirs absent)
	bool bPouvoirs = false;
	int32 XP = 0;
	/** Pouvoir choisi avec Y (-1 = inconnu). */
	int32 Selection = -1;
	float Effet[5] = {};
	float Recharge[5] = {};

	// Invasion de zombies (bZombies = false : plugin ChellesZombies absent)
	bool bZombies = false;
	bool bInvasion = false;
	int32 ZombiesRestants = 0;
	float SecondesInvasion = 0.f;

	/** Temps qui passe, pour les clignotements. */
	float Horloge = 0.f;
};

class SHudChelles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHudChelles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	FEtatHudChelles Etat;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Couleur) const;
	/** Panneau aux coins coupes (comme le menu) : fond + contour. */
	void Panneau(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Fond, const FLinearColor& Bord) const;
	/** Ecrit un texte ; Alignement 0 = a gauche de Pos, 0.5 = centre sur Pos, 1 = a droite. */
	void Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FString& Texte, const FSlateFontInfo& Police, const FLinearColor& Couleur, float Alignement) const;
	void Croix(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Taille, const FLinearColor& Couleur) const;
	void TeteZombie(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Taille, const FLinearColor& Couleur) const;
};

/** Paliers et pouvoirs : MEMES valeurs que Plugins\ChellesPouvoirs\Source\ChellesPouvoirs\Public\PouvoirsTypes.h. */
namespace ChellesHudDonnees
{
	struct FPalierHud
	{
		const TCHAR* Titre;
		int32 XP;
	};

	struct FPouvoirHud
	{
		const TCHAR* Nom;
		int32 Niveau;
		FLinearColor Couleur;
	};

	static constexpr int32 NombrePouvoirs = 5;
	static constexpr int32 XPParMinute = 10;

	const TArray<FPalierHud>& Paliers();
	const FPouvoirHud& Pouvoir(int32 Index);
	int32 NiveauPourXP(int32 XP);
}
