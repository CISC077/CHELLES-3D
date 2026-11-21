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

	/** Position par rapport au joueur (0, 0), X vers la droite, Y vers le bas ; 1 = Chelles.MiniCarteZoom metres. */
	FVector2D Pos = FVector2D::ZeroVector;
	EType Type = Zombie;
	/** Vrai : s'il est hors de la carte, il reste colle au bord (pour montrer la direction). Faux : il disparait. */
	bool bGarderAuBord = false;
};

/** Forme de la mini-carte facon GPS (en bas a gauche) : partagee par le dessin et la camera vue du ciel. */
namespace ChellesCarte
{
	static constexpr float Largeur = 360.f;
	static constexpr float Hauteur = 240.f;
	/** Hauteur de la fleche du joueur dans la carte (0 = en haut, 1 = en bas) : on voit surtout ce qui est devant. */
	static constexpr float PlaceJoueur = 0.78f;
	/** Taille de l'image vue du ciel (meme forme que la carte). */
	static constexpr int32 ImageX = 384;
	static constexpr int32 ImageY = 256;
}

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

	// Mini-carte en bas a gauche (bCarte = false : cachee avec Chelles.MiniCarte 0)
	bool bCarte = false;
	/** Vrai si la vue du quartier vue du ciel est prete (sinon fond sombre quadrille). */
	bool bImageCarte = false;
	/** Distance du centre au bord de la carte, en metres (pour l'echelle). */
	float CarteRayonMetres = 80.f;
	/** Direction du nord sur la carte (vecteur -1..1, Y vers le bas). */
	FVector2D CarteNord = FVector2D(0.f, -1.f);
	TArray<FPointCarteChelles> PointsCarte;
	/** Coins arrondis avec l'image dedans (Chelles.MiniCarteCoins 0 = image carree, si jamais l'image ne s'affiche pas). */
	bool bCoinsArrondis = true;

	// Itineraire cyan vers la destination (mission en cours, sinon le teleporteur le plus proche)
	bool bDestination = false;
	/** Destination, meme repere que les points. */
	FVector2D Destination = FVector2D::ZeroVector;
	/** Le trajet (joueur -> destination), meme repere que les points. */
	TArray<FVector2D> Itineraire;
	/** Longueur du trajet en kilometres. */
	float DistanceKm = 0.f;
	/** Ce qu'est la destination (MISSION, TELEPORTEUR). */
	FString NomDestination;

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
	/** Mini-carte facon GPS en bas a gauche ; renvoie sa hauteur (0 si cachee). */
	float DessinerCarte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Ecran) const;
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Couleur) const;
	/** Panneau aux coins coupes (comme le menu) : fond + contour. */
	void Panneau(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FVector2D& Taille, const FLinearColor& Fond, const FLinearColor& Bord) const;
	/** Ecrit un texte ; Alignement 0 = a gauche de Pos, 0.5 = centre sur Pos, 1 = a droite. */
	void Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
		const FString& Texte, const FSlateFontInfo& Police, const FLinearColor& Couleur, float Alignement) const;
	/** Remplit une forme (convexe ou en eventail depuis le 1er point) ; avec Image, l'image est collee dedans (Coin/Taille = cadre de l'image). */
	void Remplir(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const FLinearColor& Couleur, const FSlateBrush* Image = nullptr, const FVector2D& CoinImage = FVector2D::ZeroVector,
		const FVector2D& TailleImage = FVector2D(1.f, 1.f)) const;
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
