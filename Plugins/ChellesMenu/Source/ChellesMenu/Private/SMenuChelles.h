// CHELLES 3D - Dessin du menu (Slate, sans asset), style cyberpunk
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "HabillageChelles.h"

class AMenuChelles;
class APlayerController;
class SBox;
class AApercuHabillage;
struct FSlateBrush;
class SEditableTextBox;
class UWorld;

enum class EEcranMenu : uint8
{
	Principal,
	Habillage,
	Mode,
	Multi,
	Serveurs,
	AdresseIP,
	Search,
	Options,
	Touches,
	ConfirmerQuitter,
	Credits
};

enum class EOngletOptions : uint8
{
	Controles,
	Graphismes,
	Audio,
	Jeu
};


/** Reglages du menu OPTIONS, enregistres dans GameUserSettings.ini (section ChellesMenu). */
namespace ChellesOptions
{
	void AppliquerAuDemarrage(UWorld* Monde);
	void AppliquerAuJoueur(APlayerController* Joueur);
	bool ChatActif();
	bool AideActive();
	bool PvpActif();
	bool Manette();
	int32 Difficulte(); // 0 Facile, 1 Normal, 2 Difficile, 3 Extreme
	float VolumeMusique();
	float VolumeEffets();
	float VolumeGeneral();
	void DefinirVolumeGeneral(float V);
	void DefinirVolumeCanal(int32 Canal, float V);
	bool MusiqueCoupee();
	void DefinirMusiqueCoupee(bool b);
	FString Version();
}

/** Fond anime : grille neon, lignes de balayage, cadre. */
class SDecorCyber : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDecorCyber) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;
};

/** Petit logo haut-parleur (barre quand la musique est coupee), en haut a droite du menu. */
class SIconeSon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SIconeSon) {}
		SLATE_EVENT(FSimpleDelegate, OnClic)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(46.f, 46.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement) override;

private:
	FSimpleDelegate Clic;
};

/** Bouton texte : neon cyan, cadre a coins coupes qui pulse au survol. */
class SBoutonMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBoutonMenu)
		: _TaillePolice(56)
		, _Actif(true)
		, _Onglet(false)
		{}
		SLATE_ATTRIBUTE(FText, Texte)
		SLATE_ARGUMENT(int32, TaillePolice)
		SLATE_ARGUMENT(bool, Actif)
		SLATE_ARGUMENT(bool, Onglet)
		SLATE_EVENT(FSimpleDelegate, OnClic)
		SLATE_EVENT(FSimpleDelegate, OnClicDroit)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FReply OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement) override;
	virtual void OnMouseEnter(const FGeometry& Geometrie, const FPointerEvent& Evenement) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	/** Sens > 0 : clic gauche ; Sens < 0 : clic droit (choix precedent). */
	void Cliquer(int32 Sens);
	void SetSelectionne(bool b) { bSelectionne = b; }
	bool EstReglage() const { return bReglage; }

private:
	FSimpleDelegate Clic;
	FSimpleDelegate ClicDroit;
	bool bActif = true;
	bool bReglage = false;
	bool bSelectionne = false;
	bool bOnglet = false;
};

class SMenuChelles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMenuChelles) {}
		SLATE_ARGUMENT(TWeakObjectPtr<AMenuChelles>, Menu)
	SLATE_END_ARGS()

	virtual ~SMenuChelles();
	void Construct(const FArguments& InArgs);
	void Afficher(EEcranMenu NouvelEcran);
	/** Reconstruit l'ecran courant (apres une recherche de parties). */
	void Rafraichir();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement) override;
	virtual void Tick(const FGeometry& Geometrie, const double Temps, const float Delta) override;

private:
	TSharedRef<SWidget> Construire(EEcranMenu E);
	TSharedRef<SWidget> EcranPrincipal();
	void OuvrirLien(const TCHAR* Cle, const TCHAR* Nom);
	TSharedRef<SWidget> EcranMode();
	// HABILLAGE (apres JOUER) : sexe, tenue, look ; apercu 3D a droite
	TSharedRef<SWidget> EcranHabillage();
	TSharedRef<SWidget> ReglageHabillage(const FString& NomReglage, const FString& ValeurReglage, TFunction<void(int32)> Modifier);
	void ModifierTenue(TFunction<void(ChellesHabillage::FTenue&)> Changement);
	void MontrerApercu();
	void RetirerApercu();
	ChellesHabillage::FTenue TenueChoisie;
	bool bTenueLue = false;
	TWeakObjectPtr<AApercuHabillage> ApercuActeur;
	TSharedPtr<FSlateBrush> PinceauApercu;
	TSharedRef<SWidget> EcranMulti();
	TSharedRef<SWidget> EcranServeurs();
	TSharedRef<SWidget> EcranAdresseIP();
	TSharedRef<SWidget> EcranSearch();
	TSharedRef<SWidget> EcranOptions();
	TSharedRef<SWidget> EcranTouches();
	TSharedRef<SWidget> EcranQuitter();
	TSharedRef<SWidget> EcranCredits();
	TSharedRef<SWidget> Curseur(TFunction<float()> Lire, TFunction<void(float)> Ecrire);
	TSharedRef<SWidget> OngletControles();
	TSharedRef<SWidget> OngletGraphismes();
	TSharedRef<SWidget> OngletAudio();
	TSharedRef<SWidget> OngletJeu();
	TSharedRef<SWidget> BoutonOnglet(const FText& Texte, EOngletOptions Cible);
	void ChangerOnglet(int32 Sens);

	TSharedRef<SWidget> Bouton(const FText& Texte, int32 Taille, TFunction<void()> Action, bool bActif = true);
	TSharedRef<SWidget> Reglage(TFunction<FText()> Texte, TFunction<void(int32)> Changer);
	TSharedRef<SWidget> Titre(const FText& Texte, const FText& SousTitre);
	TSharedRef<SWidget> TexteEtat();
	TSharedRef<SWidget> Petit(const FText& Texte);
	TSharedRef<SWidget> BoutonRetour();

	void Retour();
	void Deplacer(int32 Sens);
	TSharedRef<SBoutonMenu> Enregistrer(TSharedRef<SBoutonMenu> Nouveau);
	AMenuChelles* LeMenu() const;

	TWeakObjectPtr<AMenuChelles> Menu;
	EEcranMenu Ecran = EEcranMenu::Principal;
	TSharedPtr<SBox> Zone;
	TSharedPtr<SEditableTextBox> ChampIP;
	FString TexteIP;
	FText MessageLien;
	float Age = 0.f;
	TArray<TWeakPtr<SBoutonMenu>> Boutons;
	int32 Selection = INDEX_NONE;
	bool bNavigation = false;
	EOngletOptions Onglet = EOngletOptions::Controles;
	int32 ToucheEnAttente = INDEX_NONE;
};
