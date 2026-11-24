// CHELLES 3D - Dessin du menu ARMEMENTS (Slate, sans asset). Voir MenuArmements.h
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "InputCoreTypes.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "SRoueArmes.h"

class SBox;
class SEditableTextBox;
class SScrollBox;
class SUniformGridPanel;
class SVerticalBox;
class SHorizontalBox;

/** Couleurs et dessins communs du menu (fond bleu, rarete...). */
namespace MenuArmementsStyle
{
	FLinearColor CouleurRarete(int32 Rarete);
	FText NomRarete(int32 Rarete);
}

/** Fond plein ecran : degrade bleu de gauche a droite, bandes de lumiere horizontales. */
class SFondArmements : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFondArmements) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) {}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100.f, 100.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;
};

/** Une carte d'arme : fond couleur de rarete, image (ou petit dessin), etoiles, badges. */
class SCarteArme : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCarteArme)
		: _Taille(150.f)
		, _DansLeCoffre(false)
		, _Vide(false)
		{}
		SLATE_ARGUMENT(FCaseArme, Case)
		SLATE_ARGUMENT(float, Taille)
		SLATE_ARGUMENT(bool, DansLeCoffre)
		/** Case d'inventaire vide (cadre seulement). */
		SLATE_ARGUMENT(bool, Vide)
		SLATE_ATTRIBUTE(bool, Choisie)
		SLATE_EVENT(FSimpleDelegate, OnClic)
		SLATE_EVENT(FSimpleDelegate, OnDoubleClic)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Taille, Taille); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometrie, const FPointerEvent& Evenement) override;

private:
	FCaseArme Case;
	float Taille = 150.f;
	bool bCoffre = false;
	bool bVide = false;
	TAttribute<bool> Choisie;
	FSimpleDelegate Clic;
	FSimpleDelegate DoubleClic;
	/** Sert seulement a dessiner la silhouette d'une arme sans image (meme dessin que la roue TAB). */
	TSharedPtr<SRoueArmes> Dessinateur;
};

/** Petite etoile / case a cocher / icone dessinee (sans police speciale). */
class SIconeArmements : public SLeafWidget
{
public:
	enum class EForme : uint8 { Etoiles, Coche, Info, Filtres, Fleche, Coffre };

	SLATE_BEGIN_ARGS(SIconeArmements)
		: _Forme(EForme::Etoiles)
		, _Nombre(1)
		, _Taille(FVector2D(24.f, 24.f))
		, _Couleur(FLinearColor::White)
		{}
		SLATE_ARGUMENT(EForme, Forme)
		SLATE_ARGUMENT(int32, Nombre)
		SLATE_ARGUMENT(FVector2D, Taille)
		SLATE_ARGUMENT(FLinearColor, Couleur)
		SLATE_ATTRIBUTE(bool, Coche)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override { return Taille; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	EForme Forme = EForme::Etoiles;
	int32 Nombre = 1;
	FVector2D Taille;
	FLinearColor Couleur;
	TAttribute<bool> Coche;
};

class SMenuArmements : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMenuArmements) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Nouvelle liste (apres une action ou a l'ouverture) : garde la categorie, la recherche, la carte choisie. */
	void MettreAJour(const TArray<FCaseArme>& NouvellesCases, const TSet<FString>& Coffre);
	/** Message en bas du menu (ex : "Les armes se prennent en mode Soldat"). */
	void Dire(const FString& Texte);

	/** Actions (remplies par AMenuArmements). Index = case de la liste complete. */
	TFunction<void(int32)> OnEquiper;
	TFunction<void(int32)> OnCoffre;
	TFunction<void(int32)> OnLacher;
	TFunction<void(int32)> OnOnglet;
	TFunction<void()> OnFermer;
	TFunction<void()> OnRelire;
	/** Touche qui ferme aussi le menu (celle qui l'ouvre, I par defaut). */
	FKey ToucheMenu;

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Evenement) override;
	virtual void Tick(const FGeometry& Geometrie, const double Temps, const float Delta) override;

private:
	// Construction des morceaux
	// Disposition inversee : onglets en colonne a gauche, categories/filtres en barres en haut,
	// sac (inventaire) en colonne, boutons empiles a droite sous la fiche.
	TSharedRef<SWidget> ColonneOnglets();
	TSharedRef<SWidget> BandeauHaut();
	TSharedRef<SWidget> ZoneCentre();
	TSharedRef<SWidget> ColonneFiche();
	TSharedRef<SWidget> BoutonBas(const FText& Texte, TFunction<void()> Action, TAttribute<bool> Actif);
	TSharedRef<SWidget> Bouton(TSharedRef<SWidget> Contenu, const FButtonStyle* Style, TFunction<void()> Action,
		FMargin Marge = FMargin(0.f), TAttribute<bool> Actif = true);
	const FButtonStyle* StyleBouton(const FLinearColor& Normal, const FLinearColor& Survol, const FLinearColor& Appui);

	void ReconstruireCategories();
	void ReconstruireFiltres();
	void ReconstruireGrille();
	void ReconstruirePanneau();
	void ReconstruireInventaire();

	// Filtres et tri
	bool Visible(int32 Index) const;
	TArray<int32> ListeVisible() const;
	void ChoisirCategorie(int32 Sens);
	void Choisir(int32 Index);
	void Deplacer(int32 DX, int32 DY);
	FText TexteTri() const;
	int32 Choisie() const;

	TArray<FCaseArme> Cases;
	TSet<FString> LeCoffre;
	int32 Categorie = 0;
	FString Recherche;
	int32 Tri = 0;
	/** Filtres (cases a cocher de droite). */
	TArray<bool> Filtres;
	FString NomChoisi;
	FText TexteMessage;
	float AgeMessage = 100.f;
	int32 Colonnes = 6;

	TSharedPtr<SHorizontalBox> BoiteCategories;
	TSharedPtr<SHorizontalBox> BoiteFiltres;
	TSharedPtr<SUniformGridPanel> Grille;
	TSharedPtr<SScrollBox> Defilement;
	TSharedPtr<SBox> Panneau;
	TSharedPtr<SVerticalBox> Inventaire;
	TSharedPtr<SEditableTextBox> ChampRecherche;
	TArray<TSharedPtr<FButtonStyle>> Styles;
};
