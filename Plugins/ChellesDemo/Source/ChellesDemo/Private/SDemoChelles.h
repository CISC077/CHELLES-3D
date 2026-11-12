// CHELLES 3D - Demo : compteur en haut de l'ecran, alertes et ecran FIN DE LA DEMO (Slate).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Input/Reply.h"

class ADemoChelles;

class SDemoChelles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDemoChelles) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ADemoChelles>, Demo)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Bouton QUITTER LE JEU : recoit le clavier / la manette a la fin. */
	TSharedPtr<SWidget> BoutonPrincipal() const { return BoutonFacebook; }

	/** Jamais le clavier : sinon il serait vole au jeu et au menu. */
	virtual bool SupportsKeyboardFocus() const override { return false; }

private:
	EVisibility VisibiliteRacine() const;
	EVisibility VisibiliteCompteur() const;
	EVisibility VisibiliteAlerte() const;
	EVisibility VisibiliteFin() const;
	FText TexteCompteur() const;
	FSlateColor CouleurCompteur() const;
	FText TexteAlerte() const;
	FText TexteMessage() const;
	FText TexteLien() const;
	FReply ClicFacebook();
	FReply ClicQuitter();

	TWeakObjectPtr<ADemoChelles> Demo;
	TSharedPtr<SWidget> BoutonFacebook;
};
