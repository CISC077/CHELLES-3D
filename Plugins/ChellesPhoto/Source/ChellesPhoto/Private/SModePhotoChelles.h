// CHELLES 3D - Mode photo : ce qui s'affiche a l'ecran (aide, cadre TikTok, grille, flash). Jamais dans la photo.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FModePhotoChelles;

class SModePhotoChelles : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SModePhotoChelles) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FModePhotoChelles* InMode);

	/** Message en bas de l'ecran (quelques secondes), avec ou sans flash blanc */
	void Message(const FString& Texte, bool bFlash);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FModePhotoChelles* Mode = nullptr;
	FString TexteMessage;
	double FinMessage = 0.0;
	double DebutFlash = -10.0;
};
