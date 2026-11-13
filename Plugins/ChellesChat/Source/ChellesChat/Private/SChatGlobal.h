// CHELLES 3D - Fenetre du chat global (Slate), a gauche de l'ecran
#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AChatGlobalChelles;
class SEditableTextBox;
class SScrollBox;

class SChatGlobal : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatGlobal) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AChatGlobalChelles>, Proprietaire)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void Ouvrir();
	void Fermer();
	bool EstOuverte() const { return bOuverte; }

	virtual void Tick(const FGeometry& Geo, const double Temps, const float Delta) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& Touche) override;

private:
	void Valider(const FText& Texte, ETextCommit::Type Comment);
	void Reconstruire();
	EVisibility Visibilite() const;
	FText TexteEtat() const;
	FText TexteAide() const;
	FText Indice() const;
	FSlateColor CouleurEtat() const;
	static FSlateFontInfo PoliceTitre();

	TWeakObjectPtr<AChatGlobalChelles> Proprietaire;
	TSharedPtr<SEditableTextBox> Saisie;
	TSharedPtr<SScrollBox> Liste;
	bool bOuverte = false;
	double HeureOuverture = 0.0;
	int32 VersionVue = -1;
};
