// CHELLES 3D - Dessin de la roue des armes (Slate, sans asset), facon GTA : gris-bleu transparent
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

struct FCaseArme
{
	FText Nom;
	/** Couleur de la case (bande exterieure). */
	FLinearColor Couleur = FLinearColor(0.45f, 0.62f, 0.85f);
	/** Image de l'arme (nulle = petit dessin selon Categorie). */
	TSharedPtr<FSlateBrush> Image;
	FString Categorie;
	/** Ramassee (ou arsenal disponible). Sinon la case est grisee. */
	bool bPossedee = true;
	/** Arme en main en ce moment. */
	bool bEnMain = false;
};

class SRoueArmes : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRoueArmes) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(700.f, 700.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	TArray<FCaseArme> Cases;
	int32 Survol = -1;
	FVector2D Curseur = FVector2D::ZeroVector; // relatif au centre, en fraction du rayon exterieur
	float Apparition = 1.f;                    // 0 -> 1 a l'ouverture

	/** Centre de la roue. */
	FText Titre;
	FText Munitions;
	FText Aide;

	/** Personnage joue (en bas a droite). */
	FText NomPerso;
	FLinearColor CouleurPerso = FLinearColor(0.f, 0.94f, 1.f);

private:
	void Secteur(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float RayonIn, float RayonOut, float AngleDebut, float AngleFin, const FLinearColor& Couleur) const;
	void Disque(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Rayon, const FLinearColor& Couleur) const;
	void Texte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		const FText& Contenu, const FSlateFontInfo& Police, const FLinearColor& Couleur) const;
	void Dessin(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Taille, const FString& Categorie, const FLinearColor& Couleur) const;
	void Personnage(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
		float Rayon, float Alpha) const;
};
