// CHELLES 3D - Missions : dessin du panneau en jeu et ecran MISSIONS (style neon du menu et du HUD : cyan / magenta / or)
#include "SMissionsEcran.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Cyan(0.f, 0.85f, 1.f, 1.f);
	const FLinearColor Magenta(1.f, 0.1f, 0.75f, 1.f);
	const FLinearColor Or(1.f, 0.78f, 0.2f, 1.f);
	const FLinearColor Rouge(0.95f, 0.08f, 0.08f, 1.f);
	const FLinearColor Vert(0.3f, 1.f, 0.55f, 1.f);
	const FLinearColor Blanc(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Gris(0.6f, 0.6f, 0.65f, 1.f);
	const FLinearColor FondNuit(0.01f, 0.01f, 0.04f, 0.78f);

	FString Chrono(float Secondes)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Secondes));
		return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
	}
}

// ===================================================================================
//  Panneau en jeu
// ===================================================================================

void SMissionsEcran::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SMissionsEcran::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Couleur);
}

FVector2D SMissionsEcran::Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
	int32 Taille, const FLinearColor& Couleur, float Ancre, bool bGras) const
{
	if (Texte.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", Taille);
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Dim = Mesure->Measure(Texte, Police);
	const FVector2D Coin(FMath::RoundToFloat(Pos.X - Dim.X * Ancre), FMath::RoundToFloat(Pos.Y));
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Coin + FVector2D(2.f, 2.f), Dim), Texte, Police, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.8f * Couleur.A));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Coin, Dim), Texte, Police, ESlateDrawEffect::None, Couleur);
	return Dim;
}

void SMissionsEcran::Barre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille,
	float Part, const FLinearColor& Couleur) const
{
	Boite(Sortie, Couche, G, Pos, Taille, FLinearColor(0.f, 0.f, 0.f, 0.65f));
	Boite(Sortie, Couche + 1, G, Pos + FVector2D(2.f, 2.f), FVector2D((Taille.X - 4.f) * FMath::Clamp(Part, 0.f, 1.f), Taille.Y - 4.f), Couleur);
}

void SMissionsEcran::PeindreRepere(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, float Echelle) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Marge = 60.f * Echelle;
	FVector2D P(Etat.Repere.X * Ecran.X, Etat.Repere.Y * Ecran.Y);

	// Derriere le joueur ou hors de l'ecran : on colle le repere au bord, dans la bonne direction
	// (pour un objectif derriere, AMissionsChelles donne deja un point hors de l'ecran du bon cote)
	const FVector2D Centre = Ecran * 0.5f;
	FVector2D Dir = P - Centre;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.f, 1.f);
	}
	const bool bDehors = Etat.bRepereDerriere || P.X < Marge || P.X > Ecran.X - Marge || P.Y < Marge || P.Y > Ecran.Y - Marge;
	if (bDehors && !Dir.IsNearlyZero())
	{
		const float Kx = Dir.X != 0.f ? (Ecran.X * 0.5f - Marge) / FMath::Abs(Dir.X) : BIG_NUMBER;
		const float Ky = Dir.Y != 0.f ? (Ecran.Y * 0.5f - Marge) / FMath::Abs(Dir.Y) : BIG_NUMBER;
		P = Centre + Dir * FMath::Min(Kx, Ky);
	}

	// Losange dore qui pulse
	const float Pulse = 0.75f + 0.25f * FMath::Sin((float)FMath::Fmod(FPlatformTime::Seconds() * 5.0, 2.0 * PI));
	const float R = 14.f * Echelle;
	TArray<FVector2D> Losange;
	Losange.Add(P + FVector2D(0.f, -R));
	Losange.Add(P + FVector2D(R, 0.f));
	Losange.Add(P + FVector2D(0.f, R));
	Losange.Add(P + FVector2D(-R, 0.f));
	Losange.Add(P + FVector2D(0.f, -R));
	FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Losange, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.7f), true, 6.f * Echelle);
	FSlateDrawElement::MakeLines(Sortie, Couche + 1, G.ToPaintGeometry(), Losange, ESlateDrawEffect::None, Or * Pulse, true, 3.f * Echelle);
	Boite(Sortie, Couche + 1, G, P - FVector2D(3.f, 3.f) * Echelle, FVector2D(6.f, 6.f) * Echelle, Or);

	const int32 Petit = FMath::RoundToInt(14 * Echelle);
	Ecrire(Sortie, Couche + 2, G, Etat.DistanceRepere, P + FVector2D(0.f, R + 4.f * Echelle), Petit, Or, 0.5f);
}

int32 SMissionsEcran::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Echelle = FMath::Clamp(Ecran.Y / 1080.f, 0.6f, 2.f);
	const int32 Grand = FMath::RoundToInt(46 * Echelle);
	const int32 Moyen = FMath::RoundToInt(20 * Echelle);
	const int32 Normal = FMath::RoundToInt(15 * Echelle);
	const int32 Petit = FMath::RoundToInt(13 * Echelle);

	// ---- Panneau de la mission, en haut a gauche (la mini-carte est en haut a droite) ----
	if (Etat.Phase == 1 || Etat.Phase == 2)
	{
		const FVector2D Coin(22.f * Echelle, 22.f * Echelle);
		const float Largeur = 400.f * Echelle;
		const float Pad = 14.f * Echelle;
		float Y = Coin.Y + Pad;
		const float X = Coin.X + Pad + 6.f * Echelle;

		// On mesure d'abord la hauteur pour dessiner le fond
		float Hauteur = Pad * 2.f + Petit * 1.5f + Moyen * 1.4f + Normal * 1.5f;
		if (!Etat.Avancement.IsEmpty() || Etat.bChrono) { Hauteur += Moyen * 1.5f; }
		if (Etat.PartVie >= 0.f) { Hauteur += Normal * 1.4f + 16.f * Echelle; }
		if (Etat.ZombiesRestants > 0) { Hauteur += Normal * 1.4f; }
		if (!Etat.Detail.IsEmpty()) { Hauteur += Normal * 1.5f; }

		Boite(Sortie, Couche, G, Coin, FVector2D(Largeur, Hauteur), FondNuit);
		Boite(Sortie, Couche + 1, G, Coin, FVector2D(5.f * Echelle, Hauteur), Etat.bAlerte ? Rouge : Magenta);
		Boite(Sortie, Couche + 1, G, Coin, FVector2D(Largeur, 2.f * Echelle), Cyan);

		Ecrire(Sortie, Couche + 2, G, Etat.Phase == 1 ? TEXT("MISSION \u00B7 EN ROUTE") : TEXT("MISSION EN COURS"), FVector2D(X, Y), Petit, Cyan);
		Y += Petit * 1.5f;
		Ecrire(Sortie, Couche + 2, G, Etat.Titre, FVector2D(X, Y), Moyen, Or);
		Y += Moyen * 1.4f;
		Ecrire(Sortie, Couche + 2, G, Etat.Consigne, FVector2D(X, Y), Normal, Blanc, 0.f, false);
		Y += Normal * 1.5f;

		if (!Etat.Avancement.IsEmpty() || Etat.bChrono)
		{
			Ecrire(Sortie, Couche + 2, G, Etat.Avancement, FVector2D(X, Y), Moyen, Magenta);
			if (Etat.bChrono)
			{
				const bool bPresse = Etat.Secondes <= 20.f;
				Ecrire(Sortie, Couche + 2, G, Chrono(Etat.Secondes), FVector2D(Coin.X + Largeur - Pad, Y), Moyen, bPresse ? Rouge : Blanc, 1.f);
			}
			Y += Moyen * 1.5f;
		}
		if (Etat.PartVie >= 0.f)
		{
			Ecrire(Sortie, Couche + 2, G, FString::Printf(TEXT("%s  %d %%"), *Etat.NomVie, FMath::RoundToInt(Etat.PartVie * 100.f)), FVector2D(X, Y), Normal, Blanc);
			Y += Normal * 1.4f;
			const FLinearColor CouleurVie = Etat.PartVie > 0.5f ? Vert : (Etat.PartVie > 0.25f ? Or : Rouge);
			Barre(Sortie, Couche + 2, G, FVector2D(X, Y), FVector2D(Largeur - (X - Coin.X) - Pad, 12.f * Echelle), Etat.PartVie, CouleurVie);
			Y += 16.f * Echelle;
		}
		if (Etat.ZombiesRestants > 0)
		{
			Ecrire(Sortie, Couche + 2, G, FString::Printf(TEXT("Zombies : %d"), Etat.ZombiesRestants), FVector2D(X, Y), Normal, Rouge);
			Y += Normal * 1.4f;
		}
		if (!Etat.Detail.IsEmpty())
		{
			Ecrire(Sortie, Couche + 2, G, Etat.Detail, FVector2D(X, Y), Normal, Etat.bAlerte ? Rouge : Gris, 0.f, Etat.bAlerte);
		}

		if (Etat.bRepere)
		{
			PeindreRepere(Sortie, Couche + 3, G, Echelle);
		}
	}

	// ---- Fin de mission : grand titre au centre ----
	if ((Etat.Phase == 3 || Etat.Phase == 4) && Etat.AgeFin < 30.f)
	{
		const bool bReussie = Etat.Phase == 3;
		const float Apparition = FMath::Clamp(Etat.AgeFin / 0.4f, 0.f, 1.f);
		const float Y = Ecran.Y * 0.30f;
		Boite(Sortie, Couche, G, FVector2D(0.f, Y - 20.f * Echelle), FVector2D(Ecran.X, Grand * 3.2f), FLinearColor(0.f, 0.f, 0.f, 0.55f * Apparition));
		FLinearColor Couleur = bReussie ? Or : Rouge;
		Couleur.A = Apparition;
		Ecrire(Sortie, Couche + 1, G, bReussie ? TEXT("MISSION R\u00C9USSIE !") : TEXT("MISSION RAT\u00C9E"), FVector2D(Ecran.X * 0.5f, Y), Grand, Couleur, 0.5f);
		FString Sous = Etat.Titre;
		if (!Etat.Detail.IsEmpty())
		{
			Sous += TEXT("  \u00B7  ") + Etat.Detail;
		}
		FLinearColor CouleurSous = Blanc;
		CouleurSous.A = Apparition;
		Ecrire(Sortie, Couche + 1, G, Sous, FVector2D(Ecran.X * 0.5f, Y + Grand * 1.35f), Moyen, CouleurSous, 0.5f, false);
		if (bReussie && Etat.XP > 0)
		{
			FLinearColor CouleurXP = Cyan;
			CouleurXP.A = Apparition;
			Ecrire(Sortie, Couche + 1, G, FString::Printf(TEXT("+%d XP"), Etat.XP), FVector2D(Ecran.X * 0.5f, Y + Grand * 2.2f), Moyen, CouleurXP, 0.5f);
		}
	}

	// ---- Message court (ex. "Seul l'hote lance les missions") ----
	if (!Etat.Message.IsEmpty() && Etat.AgeMessage < 5.f)
	{
		FLinearColor Couleur = Cyan;
		Couleur.A = FMath::Clamp(5.f - Etat.AgeMessage, 0.f, 1.f);
		Ecrire(Sortie, Couche + 4, G, Etat.Message, FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.70f), Moyen, Couleur, 0.5f);
	}
	return Couche + 5;
}

// ===================================================================================
//  Ecran MISSIONS (touche N)
// ===================================================================================

void SMissionsMenu::Construct(const FArguments& InArgs)
{
	QuandChoisie = InArgs._QuandChoisie;
	QuandFerme = InArgs._QuandFerme;
	bPeutChoisir = InArgs._bPeutChoisir;
	bMissionEnCours = InArgs._bMissionEnCours;

	const FSlateFontInfo Titre = FCoreStyle::GetDefaultFontStyle("Bold", 40);
	const FSlateFontInfo Texte = FCoreStyle::GetDefaultFontStyle("Regular", 15);

	FString Aide;
	if (!bPeutChoisir)
	{
		Aide = TEXT("Seul l'h\u00F4te de la partie lance les missions. \u00C9chap = fermer.");
	}
	else if (bMissionEnCours)
	{
		Aide = TEXT("Une mission est d\u00E9j\u00E0 en cours : terminez-la ou abandonnez-la. \u00C9chap = fermer.");
	}
	else
	{
		Aide = FString::Printf(TEXT("Choisis une mission. Touche %s ou \u00C9chap = fermer."), *InArgs._NomTouche);
	}

	TSharedRef<SVerticalBox> Liste = SNew(SVerticalBox);
	Liste->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("MISSIONS DANS LE QUARTIER"))).Font(Titre).ColorAndOpacity(Cyan)
	];
	Liste->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
	[
		SNew(STextBlock).Text(FText::FromString(Aide)).Font(Texte).ColorAndOpacity(Gris).AutoWrapText(true)
	];
	for (const FChoixMission& Choix : InArgs._Choix)
	{
		Liste->AddSlot().AutoHeight().Padding(0.f, 4.f)[Bouton(Choix)];
	}

	TSharedRef<SHorizontalBox> Bas = SNew(SHorizontalBox);
	if (bMissionEnCours && bPeutChoisir)
	{
		Bas->AddSlot().AutoWidth().Padding(0.f, 0.f, 12.f, 0.f)
		[
			SNew(SButton)
			.ButtonColorAndOpacity(FLinearColor(0.45f, 0.02f, 0.02f, 1.f))
			.ContentPadding(FMargin(14.f, 6.f))
			.OnClicked_Lambda([this]() { QuandChoisie.ExecuteIfBound(0); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("ABANDONNER LA MISSION"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14)).ColorAndOpacity(Blanc)
			]
		];
	}
	Bas->AddSlot().AutoWidth()
	[
		SNew(SButton)
		.ContentPadding(FMargin(14.f, 6.f))
		.OnClicked_Lambda([this]() { QuandFerme.ExecuteIfBound(); return FReply::Handled(); })
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("FERMER"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]
	];
	Liste->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f).HAlign(HAlign_Right)[Bas];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(780.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FondNuit)
				.Padding(FMargin(36.f, 28.f))
				[
					Liste
				]
			]
		]
	];
}

TSharedRef<SWidget> SMissionsMenu::Bouton(const FChoixMission& Choix)
{
	const bool bActif = Choix.bDisponible && bPeutChoisir && !bMissionEnCours;
	const uint8 Numero = Choix.Numero;
	FString Ligne = Choix.Description;
	if (!Choix.bDisponible && !Choix.PourquoiPas.IsEmpty())
	{
		Ligne += TEXT("\n") + Choix.PourquoiPas;
	}
	return SNew(SButton)
		.IsEnabled(bActif)
		.ButtonColorAndOpacity(Choix.bDisponible ? FLinearColor(0.f, 0.25f, 0.35f, 1.f) : FLinearColor(0.12f, 0.12f, 0.14f, 1.f))
		.ContentPadding(FMargin(18.f, 10.f))
		.OnClicked_Lambda([this, Numero]() { QuandChoisie.ExecuteIfBound(Numero); return FReply::Handled(); })
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d.  %s"), Numero, *Choix.Titre)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(Choix.bDisponible ? Magenta : Gris)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Choix.Recompense))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15)).ColorAndOpacity(Choix.bDisponible ? Or : Gris)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Ligne)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(Choix.bDisponible ? Blanc : Gris).AutoWrapText(true)
			]
		];
}

FReply SMissionsMenu::OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche)
{
	if (Touche.GetKey() == EKeys::Escape || (ToucheFermer.IsValid() && Touche.GetKey() == ToucheFermer))
	{
		QuandFerme.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometrie, Touche);
}
