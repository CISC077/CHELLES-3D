// CHELLES 3D - Courses de rue : affichage en jeu (chrono, portes, repere, compte a rebours, arrivee)
// et ecran COURSES (touche Debut). Slate seulement, aucun asset.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

/** Ce que l'ecran doit montrer, rempli a chaque image par ACoursesChelles. */
struct FEtatEcranCourses
{
	uint8 Etat = 0;          // EEtatCourse
	FString Nom;
	float Secondes = 0.f;    // chrono
	int32 Porte = 0;         // portes franchies
	int32 Portes = 0;
	FString Record;          // "RECORD 1:23.45" ou vide
	bool bEcart = false;
	int32 EcartMs = 0;       // + = plus lent que le record
	float AgeEcart = 100.f;
	int32 Compte = 0;        // compte a rebours (0 = PARTEZ)
	float AgeEtat = 0.f;

	// Arrivee
	FString TempsFinal;
	bool bNouveauRecord = false;
	FString LigneRecord;     // "Ancien record : 1:30.00"
	FString LigneClassement; // "Classement : 3e sur 12"
	int32 XP = 0;
	FString RaisonAbandon;

	// Sur un depart, sans course
	FString Invitation;
	FString InvitationDetail;

	// Repere vers la prochaine porte / le depart montre (coordonnees 0..1 de l'ecran)
	bool bRepere = false;
	FVector2D Repere = FVector2D(0.5f, 0.5f);
	bool bRepereDerriere = false;
	bool bRepereArrivee = false;
	FString DistanceRepere;

	FString Message;
	float AgeMessage = 100.f;
};

class SCoursesEcran : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SCoursesEcran) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(800.f, 600.f); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometrie, const FSlateRect& Clip,
		FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override;

	FEtatEcranCourses Etat;

private:
	void Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const;
	FVector2D Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
		int32 Taille, const FLinearColor& Couleur, float Ancre = 0.f, bool bGras = true) const;
	void PeindreRepere(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, float Echelle) const;
};

/** Une course proposee dans l'ecran COURSES. */
struct FChoixCourse
{
	int32 Index = 0;
	FString Titre;
	FString Description;   // "2,1 km · 18 portes · depart a 350 m"
	FString Record;        // "Votre record : 1:23.45"
	TAttribute<FText> Classement; // meilleurs temps de tous les joueurs (mis a jour quand le serveur repond)
};

DECLARE_DELEGATE_OneParam(FQuandCourseChoisie, int32 /*Index*/);

/** Ecran COURSES : records, classement, montrer un depart, fermer. */
class SCoursesMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCoursesMenu) {}
		SLATE_ARGUMENT(FString, NomTouche)
		SLATE_ARGUMENT(FString, Aide)
		SLATE_ARGUMENT(TArray<FChoixCourse>, Choix)
		SLATE_EVENT(FQuandCourseChoisie, QuandChoisie)
		SLATE_EVENT(FSimpleDelegate, QuandFerme)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche) override;

	FKey ToucheFermer;

private:
	TSharedRef<class SWidget> Carte(const FChoixCourse& Choix);

	FQuandCourseChoisie QuandChoisie;
	FSimpleDelegate QuandFerme;
};
