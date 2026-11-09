// CHELLES 3D - Panneau vertical a droite : chat entre joueurs + aide (Tuto & Astuces)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AMenuChelles;
class SEditableTextBox;
class SScrollBox;

namespace ChellesChat
{
	/** Message recu d'un joueur (appele par UChatChelles). */
	void AjouterMessageJoueur(const FString& Auteur, const FString& Texte);
	/** Reponse du chatbot d'aide, d'apres la bible du jeu (Content/ChellesMenu/Bible_Chelles.txt). */
	FString RepondreAide(const FString& Question);
}

class SChatChelles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChatChelles) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AMenuChelles>, Menu)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Entree en jeu : ouvre le panneau et met le curseur dans la zone de saisie. */
	void Ouvrir();
	void Fermer();
	bool EstOuvert() const { return bOuvert; }

	virtual void Tick(const FGeometry& Geo, const double Temps, const float Delta) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& Touche) override;

private:
	bool EnMulti() const;
	bool ChatDispo() const;
	bool AideDispo() const;
	void ChoisirOnglet(bool bVersAide);
	void Valider(const FText& Texte, ETextCommit::Type Comment);
	void Reconstruire();
	EVisibility Visibilite() const;
	TSharedRef<SWidget> BoutonOnglet(const FString& Nom, bool bOngletAide);

	TWeakObjectPtr<AMenuChelles> Menu;
	TSharedPtr<SEditableTextBox> Saisie;
	TSharedPtr<SScrollBox> Liste;
	bool bOuvert = false;
	bool bAide = false;
	int32 VuJoueurs = -1;
	int32 VuAide = -1;
	bool bVuOngletAide = false;
	double DernierMessage = -100.0;
};
