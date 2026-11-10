// CHELLES 3D - Bulles noires (texte blanc) au-dessus des passants qui parlent. Slate, sans asset.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AActor;
class APlayerController;

struct FBulleParole
{
	TWeakObjectPtr<AActor> Passant;
	FString Texte;
	float Hauteur = 110.f;   // au-dessus du centre du passant (cm)
	double Debut = 0.0;
	double Fin = 0.0;
};

class SBullesParoles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBullesParoles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	/** Ajoute (ou remplace) la bulle d'un passant. */
	void Ajouter(AActor* Passant, const FString& Texte, float Hauteur, double Maintenant, float Duree);

	/** Retire les bulles terminees. */
	void Nettoyer(double Maintenant);

	TWeakObjectPtr<APlayerController> Joueur;
	TWeakObjectPtr<UWorld> Monde;

	/** Au-dela de cette distance (cm) la bulle n'est pas affichee. */
	float DistanceMax = 2500.f;
	int32 TaillePolice = 15;
	float Opacite = 0.85f;

private:
	TArray<FBulleParole> Bulles;

	void Rectangle(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G,
		const FVector2D& Coin, const FVector2D& Taille, float Rayon, const FLinearColor& Couleur) const;
	void Polygone(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G,
		const TArray<FVector2D>& Points, const FLinearColor& Couleur) const;
};
