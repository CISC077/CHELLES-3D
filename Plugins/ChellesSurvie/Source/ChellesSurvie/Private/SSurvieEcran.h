// CHELLES 3D - Modes de survie : affichage en jeu (vague, points, a terre, scores) et ecran MODES (Slate, sans asset)
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SCompoundWidget.h"

/** Une ligne du tableau des scores. */
struct FLigneScore
{
	FString Nom;
	int32 Points = 0;
	int32 Kills = 0;
	int32 Tetes = 0;
	int32 FoisATerre = 0;
	int32 Releves = 0;
	uint8 Etat = 0; // 0 debout, 1 a terre, 2 mort
	bool bMoi = false;
};

/** Ce que l'ecran doit montrer, rempli a chaque image par ASurvieChelles. */
struct FEtatEcranSurvie
{
	bool bActif = false;          // une partie de survie est en cours
	FString NomMode;
	int32 Vague = 0;
	int32 ZombiesRestants = 0;
	bool bVagueEnCours = false;
	bool bCoureurs = false;
	float SecondesPause = 0.f;     // > 0 : prochaine vague dans...
	int32 MesPoints = 0;

	uint8 MonEtat = 0;            // 0 debout, 1 a terre, 2 mort
	float SecondesATerre = 0.f;
	float MaRelance = 0.f;        // un coequipier me releve (0 a 1)

	FString Invite;               // "[E] Fusil - 1200 pts"
	float RelanceDonnee = -1.f;   // je releve quelqu'un (0 a 1), -1 = non

	bool bFin = false;
	FString TitreFin;
	float SecondesFin = 0.f;
	TArray<FLigneScore> Scores;

	FString Message;
	float AgeMessage = 100.f;
};

class SSurvieEcran : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSurvieEcran) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	FEtatEcranSurvie Etat;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const;
	/** Ecrit un texte avec une ombre ; Ancre 0 = a gauche de Pos, 0,5 = centre sur Pos, 1 = a droite. */
	FVector2D Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
		int32 Taille, const FLinearColor& Couleur, float Ancre = 0.f, bool bGras = true) const;
	void Barre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille,
		float Part, const FLinearColor& Couleur) const;
	void PeindreScores(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const;
};

DECLARE_DELEGATE_TwoParams(FChoixModeSurvie, uint8 /*Mode*/, int32 /*VagueDepart*/);

/** Ecran MODES : choisir Jeu libre / Dernier Refuge (les autres modes arrivent plus tard). */
class SModesSurvie : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SModesSurvie) : _bPeutChoisir(true) {}
		SLATE_ARGUMENT(bool, bPeutChoisir)
		SLATE_EVENT(FChoixModeSurvie, QuandChoisi)
		SLATE_EVENT(FSimpleDelegate, QuandFerme)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche) override;

private:
	TSharedRef<class SWidget> Bouton(const FString& Titre, const FString& Detail, uint8 Mode, bool bDisponible);
	FReply Choisir(uint8 Mode);

	FChoixModeSurvie QuandChoisi;
	FSimpleDelegate QuandFerme;
	bool bPeutChoisir = true;
	int32 VagueDepart = 1;
};
