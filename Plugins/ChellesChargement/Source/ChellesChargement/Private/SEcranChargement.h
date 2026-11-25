// CHELLES 3D - Ecran de chargement (Slate pur, sans asset : il peut tourner sur le fil de chargement)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "ReglagesChargement.h"

class SEcranChargement : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEcranChargement)
		: _Carte()
		, _Superpose(false)
	{}
		/** Nom de la carte qui se charge (peut etre vide). */
		SLATE_ARGUMENT(FString, Carte)
		/** true = affiche par-dessus le jeu au debut de la partie (fondu a la fin). */
		SLATE_ARGUMENT(bool, Superpose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** La barre file a 100 % puis l'ecran disparait en fondu. */
	void Terminer();
	/** true quand le fondu de fin est fini. */
	bool EstFini() const;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

private:
	EActiveTimerReturnType Animer(double InCurrentTime, float InDeltaTime);
	float Avancement(double Maintenant) const;
	float Opacite(double Maintenant) const;

	double Debut = 0.0;
	double Fin = -1.0;
	FString NomCarte;
	bool bSuperpose = false;
	int32 PremiereAstuce = 0;
	float SecondesParAstuce = 4.f;
	TArray<FAstuceChelles> Astuces;
};
