// CHELLES 3D - HUD du joueur : le dessin (Slate, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Styling/SlateBrush.h"

/** Un point sur la mini-carte : zombie, teleporteur ou autre joueur. */
struct FPointCarteChelles
{
	enum EType : uint8 { Zombie, Teleporteur, Joueur };

	/** Position sur la carte : -1..1 (X vers la droite, Y vers le bas), le joueur est au centre (0, 0). */
	FVector2D Pos = FVector2D::ZeroVector;
	EType Type = Zombie;
	/** Vrai si le point est plus loin que le bord : il est colle au bord de la carte. */
	bool bAuBord = false;
};

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

	// Mini-carte en haut a droite (bCarte = false : cachee avec Chelles.MiniCarte 0)
	bool bCarte = false;
	/** Vrai si la vue du quartier vue du ciel est prete (sinon fond sombre quadrille). */
	bool bImageCarte = false;
	/** Distance du centre au bord de la carte, en metres (pour l'echelle). */
	float CarteRayonMetres = 80.f;
	/** Direction du nord sur la carte (vecteur -1..1, Y vers le bas). */
	FVector2D CarteNord = FVector2D(0.f, -1.f);
	TArray<FPointCarteChelles> PointsCarte;

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

	/** Image du quartier vue du ciel (render target rempli par AHudChelles). */
	FSlateBrush BrushCarte;

private:
	/** Mini-carte en haut a droite ; renvoie sa hauteur (0 si cachee). */
	float DessinerCarte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Ecran) const;
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
