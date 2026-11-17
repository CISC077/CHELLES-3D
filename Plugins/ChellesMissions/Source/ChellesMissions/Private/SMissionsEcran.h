// CHELLES 3D - Missions : affichage en jeu (panneau de mission, repere vers l'objectif, fin de mission)
// et ecran MISSIONS (touche N). Slate seulement, aucun asset.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

/** Ce que l'ecran doit montrer, rempli a chaque image par AMissionsChelles. */
struct FEtatEcranMissions
{
	uint8 Mission = 0;
	uint8 Phase = 0;
	FString Titre;          // "ESCORTE DU TRAMWAY"
	FString Consigne;       // "Gardez le TRAMWAY en vie jusqu'au 3e arret"
	FString Avancement;     // "ARR\u00CATS 1 / 3"
	float Secondes = 0.f;
	bool bChrono = false;
	FString NomVie;         // "TRAMWAY" / "MAIRIE"
	float PartVie = -1.f;   // 0 a 1, -1 = pas de barre
	int32 ZombiesRestants = 0;
	FString Detail;
	bool bAlerte = false;
	int32 XP = 0;
	float AgeFin = 0.f;

	// Repere vers l'objectif (coordonnees 0..1 de l'ecran)
	bool bRepere = false;
	FVector2D Repere = FVector2D(0.5f, 0.5f);
	bool bRepereDerriere = false;
	FString DistanceRepere;

	FString Message;
	float AgeMessage = 100.f;
};

class SMissionsEcran : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMissionsEcran) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	FEtatEcranMissions Etat;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const;
	/** Ecrit un texte avec une ombre ; Ancre 0 = a gauche de Pos, 0,5 = centre sur Pos, 1 = a droite. Rend la taille. */
	FVector2D Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
		int32 Taille, const FLinearColor& Couleur, float Ancre = 0.f, bool bGras = true) const;
	void Barre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille,
		float Part, const FLinearColor& Couleur) const;
	void PeindreRepere(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, float Echelle) const;
};

/** Une mission proposee dans l'ecran MISSIONS. */
struct FChoixMission
{
	uint8 Numero = 0;
	FString Titre;
	FString Description;
	FString Recompense;
	bool bDisponible = true;
	FString PourquoiPas; // si indisponible
};

DECLARE_DELEGATE_OneParam(FQuandMissionChoisie, uint8 /*Mission, 0 = abandonner*/);

/** Ecran MISSIONS : choisir une mission, abandonner la mission en cours, fermer. */
class SMissionsMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMissionsMenu) : _bPeutChoisir(true), _bMissionEnCours(false) {}
		SLATE_ARGUMENT(bool, bPeutChoisir)
		SLATE_ARGUMENT(bool, bMissionEnCours)
		SLATE_ARGUMENT(FString, NomTouche)
		SLATE_ARGUMENT(TArray<FChoixMission>, Choix)
		SLATE_EVENT(FQuandMissionChoisie, QuandChoisie)
		SLATE_EVENT(FSimpleDelegate, QuandFerme)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche) override;

	/** Touche qui ferme aussi l'ecran (la meme que pour l'ouvrir). */
	FKey ToucheFermer;

private:
	TSharedRef<class SWidget> Bouton(const FChoixMission& Choix);

	FQuandMissionChoisie QuandChoisie;
	FSimpleDelegate QuandFerme;
	bool bPeutChoisir = true;
	bool bMissionEnCours = false;
};
