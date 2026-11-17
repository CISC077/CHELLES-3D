// CHELLES 3D - Modes de survie : dessin de l'ecran en jeu et ecran MODES (style neon du menu : cyan / magenta)
#include "SSurvieEcran.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
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
	const FLinearColor Rouge(0.9f, 0.05f, 0.05f, 1.f);
	const FLinearColor Blanc(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Gris(0.6f, 0.6f, 0.65f, 1.f);
	const FLinearColor FondNuit(0.01f, 0.01f, 0.04f, 0.82f);

	FString Milliers(int32 N)
	{
		// 12500 -> "12 500"
		FString Brut = FString::FromInt(FMath::Abs(N));
		FString R;
		for (int32 i = 0; i < Brut.Len(); ++i)
		{
			if (i > 0 && (Brut.Len() - i) % 3 == 0)
			{
				R.AppendChar(TEXT(' '));
			}
			R.AppendChar(Brut[i]);
		}
		return N < 0 ? TEXT("-") + R : R;
	}
}

// ===================================================================================
//  Ecran en jeu
// ===================================================================================

void SSurvieEcran::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SSurvieEcran::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Couleur);
}

FVector2D SSurvieEcran::Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
	int32 Taille, const FLinearColor& Couleur, float Ancre, bool bGras) const
{
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", Taille);
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Dim = Mesure->Measure(Texte, Police);
	const FVector2D Coin(Pos.X - Dim.X * Ancre, Pos.Y);
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Coin + FVector2D(2.f, 2.f), Dim), Texte, Police, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.8f * Couleur.A));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Coin, Dim), Texte, Police, ESlateDrawEffect::None, Couleur);
	return Dim;
}

void SSurvieEcran::Barre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille,
	float Part, const FLinearColor& Couleur) const
{
	Boite(Sortie, Couche, G, Pos, Taille, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Boite(Sortie, Couche + 1, G, Pos + FVector2D(2.f, 2.f), FVector2D((Taille.X - 4.f) * FMath::Clamp(Part, 0.f, 1.f), Taille.Y - 4.f), Couleur);
}

int32 SSurvieEcran::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Echelle = FMath::Clamp(Ecran.Y / 1080.f, 0.6f, 2.f);
	const int32 Grand = FMath::RoundToInt(44 * Echelle);
	const int32 Moyen = FMath::RoundToInt(24 * Echelle);
	const int32 Petit = FMath::RoundToInt(16 * Echelle);

	if (Etat.bActif)
	{
		// ---- En haut au centre : la vague ----
		const float Haut = 18.f * Echelle;
		const FString TitreVague = Etat.Vague > 0 ? FString::Printf(TEXT("VAGUE %d"), Etat.Vague) : FString(TEXT("PRÉPAREZ-VOUS"));
		Ecrire(Sortie, Couche + 2, G, TitreVague, FVector2D(Ecran.X * 0.5f, Haut), Grand, Etat.bCoureurs ? Magenta : Rouge, 0.5f);
		FString Sous;
		if (Etat.bVagueEnCours)
		{
			Sous = FString::Printf(TEXT("%s · %d zombie%s"), *Etat.NomMode, Etat.ZombiesRestants, Etat.ZombiesRestants > 1 ? TEXT("s") : TEXT(""));
			if (Etat.bCoureurs)
			{
				Sous += TEXT(" · COUREURS !");
			}
		}
		else if (Etat.SecondesPause > 0.f)
		{
			Sous = FString::Printf(TEXT("Prochaine vague dans %d s · achetez, réparez-vous"), FMath::CeilToInt(Etat.SecondesPause));
		}
		Ecrire(Sortie, Couche + 2, G, Sous, FVector2D(Ecran.X * 0.5f, Haut + Grand * 1.45f), Petit, Blanc, 0.5f, false);

		// ---- En bas a droite : mes points ----
		const FString TextePoints = Milliers(Etat.MesPoints) + TEXT(" PTS");
		Ecrire(Sortie, Couche + 2, G, TextePoints, FVector2D(Ecran.X - 30.f * Echelle, Ecran.Y - 150.f * Echelle), Grand, Or, 1.f);

		// ---- Au centre : ce qu'on peut faire avec E ----
		if (!Etat.Invite.IsEmpty() && Etat.MonEtat == 0)
		{
			const FVector2D Pos(Ecran.X * 0.5f, Ecran.Y * 0.62f);
			Ecrire(Sortie, Couche + 2, G, Etat.Invite, Pos, Moyen, Cyan, 0.5f);
		}
		if (Etat.RelanceDonnee >= 0.f && Etat.MonEtat == 0)
		{
			const FVector2D Taille(360.f * Echelle, 18.f * Echelle);
			Barre(Sortie, Couche + 2, G, FVector2D(Ecran.X * 0.5f - Taille.X * 0.5f, Ecran.Y * 0.62f + Moyen * 1.8f), Taille, Etat.RelanceDonnee, Cyan);
		}

		// ---- A terre / mort ----
		if (Etat.MonEtat == 1)
		{
			Boite(Sortie, Couche + 1, G, FVector2D::ZeroVector, Ecran, FLinearColor(0.45f, 0.f, 0.f, 0.35f));
			Ecrire(Sortie, Couche + 2, G, TEXT("À TERRE"), FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.38f), Grand, Rouge, 0.5f);
			Ecrire(Sortie, Couche + 2, G, FString::Printf(TEXT("Un coéquipier peut te relever (E maintenu près de toi) · %d s"), FMath::CeilToInt(Etat.SecondesATerre)),
				FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.38f + Grand * 1.5f), Moyen, Blanc, 0.5f, false);
			if (Etat.MaRelance > 0.f)
			{
				const FVector2D Taille(420.f * Echelle, 20.f * Echelle);
				Barre(Sortie, Couche + 2, G, FVector2D(Ecran.X * 0.5f - Taille.X * 0.5f, Ecran.Y * 0.55f), Taille, Etat.MaRelance, Cyan);
			}
		}
		else if (Etat.MonEtat == 2 && !Etat.bFin)
		{
			Boite(Sortie, Couche + 1, G, FVector2D::ZeroVector, Ecran, FLinearColor(0.f, 0.f, 0.f, 0.55f));
			Ecrire(Sortie, Couche + 2, G, TEXT("MORT"), FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.38f), Grand, Rouge, 0.5f);
			Ecrire(Sortie, Couche + 2, G, TEXT("Tu reviens au début de la prochaine vague si ton équipe tient bon."),
				FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.38f + Grand * 1.5f), Moyen, Blanc, 0.5f, false);
		}
	}

	if (Etat.bFin)
	{
		PeindreScores(Sortie, Couche + 10, G);
	}

	// ---- Message court (achat refuse, vague repoussee...) ----
	if (Etat.AgeMessage < 4.f && !Etat.Message.IsEmpty())
	{
		const float Fondu = FMath::Clamp((4.f - Etat.AgeMessage) / 0.6f, 0.f, 1.f);
		Ecrire(Sortie, Couche + 20, G, Etat.Message, FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.72f), Moyen, Or * FLinearColor(1.f, 1.f, 1.f, Fondu), 0.5f);
	}
	return Couche + 30;
}

void SSurvieEcran::PeindreScores(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Echelle = FMath::Clamp(Ecran.Y / 1080.f, 0.6f, 2.f);
	const int32 Grand = FMath::RoundToInt(48 * Echelle);
	const int32 Moyen = FMath::RoundToInt(22 * Echelle);

	const FVector2D Taille(FMath::Min(Ecran.X - 40.f, 1000.f * Echelle), (180.f + 44.f * FMath::Max(1, Etat.Scores.Num())) * Echelle);
	const FVector2D Coin((Ecran.X - Taille.X) * 0.5f, Ecran.Y * 0.18f);
	Boite(Sortie, Couche, G, FVector2D::ZeroVector, Ecran, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	Boite(Sortie, Couche + 1, G, Coin, Taille, FondNuit);
	Boite(Sortie, Couche + 2, G, Coin, FVector2D(Taille.X, 4.f), Magenta);
	Boite(Sortie, Couche + 2, G, Coin + FVector2D(0.f, Taille.Y - 4.f), FVector2D(Taille.X, 4.f), Cyan);

	Ecrire(Sortie, Couche + 3, G, Etat.TitreFin, FVector2D(Ecran.X * 0.5f, Coin.Y + 20.f * Echelle), Grand, Rouge, 0.5f);
	Ecrire(Sortie, Couche + 3, G, FString::Printf(TEXT("Vague atteinte : %d"), Etat.Vague), FVector2D(Ecran.X * 0.5f, Coin.Y + 20.f * Echelle + Grand * 1.4f), Moyen, Or, 0.5f);

	// Colonnes : nom, points, kills, tetes, a terre, releves
	const float Y0 = Coin.Y + 150.f * Echelle;
	const float Colonnes[6] = { 0.06f, 0.45f, 0.60f, 0.72f, 0.84f, 0.95f };
	const TCHAR* Entetes[6] = { TEXT("JOUEUR"), TEXT("POINTS"), TEXT("KILLS"), TEXT("TÊTES"), TEXT("À TERRE"), TEXT("RELEVÉS") };
	for (int32 c = 0; c < 6; ++c)
	{
		Ecrire(Sortie, Couche + 3, G, Entetes[c], FVector2D(Coin.X + Taille.X * Colonnes[c], Y0), FMath::RoundToInt(15 * Echelle), Cyan, c == 0 ? 0.f : 1.f);
	}
	for (int32 i = 0; i < Etat.Scores.Num(); ++i)
	{
		const FLigneScore& L = Etat.Scores[i];
		const float Y = Y0 + (36.f + 40.f * i) * Echelle;
		const FLinearColor Teinte = L.bMoi ? Or : Blanc;
		Ecrire(Sortie, Couche + 3, G, L.Nom, FVector2D(Coin.X + Taille.X * Colonnes[0], Y), Moyen, Teinte, 0.f);
		Ecrire(Sortie, Couche + 3, G, Milliers(L.Points), FVector2D(Coin.X + Taille.X * Colonnes[1], Y), Moyen, Teinte, 1.f);
		Ecrire(Sortie, Couche + 3, G, FString::FromInt(L.Kills), FVector2D(Coin.X + Taille.X * Colonnes[2], Y), Moyen, Teinte, 1.f);
		Ecrire(Sortie, Couche + 3, G, FString::FromInt(L.Tetes), FVector2D(Coin.X + Taille.X * Colonnes[3], Y), Moyen, Teinte, 1.f);
		Ecrire(Sortie, Couche + 3, G, FString::FromInt(L.FoisATerre), FVector2D(Coin.X + Taille.X * Colonnes[4], Y), Moyen, Teinte, 1.f);
		Ecrire(Sortie, Couche + 3, G, FString::FromInt(L.Releves), FVector2D(Coin.X + Taille.X * Colonnes[5], Y), Moyen, Teinte, 1.f);
	}
	Ecrire(Sortie, Couche + 3, G, FString::Printf(TEXT("Retour au jeu libre dans %d s"), FMath::CeilToInt(Etat.SecondesFin)),
		FVector2D(Ecran.X * 0.5f, Coin.Y + Taille.Y - 40.f * Echelle), FMath::RoundToInt(15 * Echelle), Gris, 0.5f, false);
}

// ===================================================================================
//  Ecran MODES
// ===================================================================================

void SModesSurvie::Construct(const FArguments& InArgs)
{
	QuandChoisi = InArgs._QuandChoisi;
	QuandFerme = InArgs._QuandFerme;
	bPeutChoisir = InArgs._bPeutChoisir;

	const FSlateFontInfo Titre = FCoreStyle::GetDefaultFontStyle("Bold", 40);
	const FSlateFontInfo Texte = FCoreStyle::GetDefaultFontStyle("Regular", 15);

	TSharedRef<SVerticalBox> Liste = SNew(SVerticalBox);
	Liste->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
	[
		SNew(STextBlock).Text(FText::FromString(TEXT("CHOISIS TON MODE"))).Font(Titre).ColorAndOpacity(Cyan)
	];
	Liste->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(bPeutChoisir
			? FString(TEXT("Tu peux revenir ici à tout moment avec F7. Échap = fermer."))
			: FString(TEXT("Seul l'hôte de la partie choisit le mode. Échap = fermer."))))
		.Font(Texte).ColorAndOpacity(Gris)
	];
	Liste->AddSlot().AutoHeight().Padding(0.f, 4.f)[Bouton(TEXT("JEU LIBRE"), TEXT("Balade dans Chelles, invasion zombie toutes les 2 h (comme avant)."), 0, true)];
	Liste->AddSlot().AutoHeight().Padding(0.f, 4.f)[Bouton(TEXT("DERNIER REFUGE"), TEXT("Survie par vagues sans fin au parc de la Mairie. Points, armes, barrières. 1 à 8 joueurs."), 1, true)];

	// Vague de depart (pour s'entrainer sur les vagues difficiles)
	Liste->AddSlot().AutoHeight().Padding(24.f, 2.f, 0.f, 8.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Commencer à la vague  "))).Font(Texte).ColorAndOpacity(Blanc)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).Text(FText::FromString(TEXT(" - "))).IsEnabled(bPeutChoisir)
			.OnClicked_Lambda([this]() { VagueDepart = FMath::Max(1, VagueDepart - 1); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.f, 0.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(Or)
			.Text_Lambda([this]() { return FText::AsNumber(VagueDepart); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).Text(FText::FromString(TEXT(" + "))).IsEnabled(bPeutChoisir)
			.OnClicked_Lambda([this]() { VagueDepart = FMath::Min(100, VagueDepart + 1); return FReply::Handled(); })
		]
	];

	Liste->AddSlot().AutoHeight().Padding(0.f, 4.f)[Bouton(TEXT("RIVALITÉ DE QUARTIER"), TEXT("Équipe Gambetta contre équipe Mairie. Bientôt."), 2, false)];
	Liste->AddSlot().AutoHeight().Padding(0.f, 4.f)[Bouton(TEXT("TRAVERSÉE DE CHELLES"), TEXT("Coopération, navette Ligne 77, atelier. Bientôt."), 3, false)];
	Liste->AddSlot().AutoHeight().Padding(0.f, 4.f)[Bouton(TEXT("CONTAGION"), TEXT("Un humain contre les zombies joués par les autres. Bientôt."), 4, false)];
	Liste->AddSlot().AutoHeight().Padding(0.f, 18.f, 0.f, 0.f).HAlign(HAlign_Right)
	[
		SNew(SButton)
		.Text(FText::FromString(TEXT("FERMER")))
		.OnClicked_Lambda([this]() { QuandFerme.ExecuteIfBound(); return FReply::Handled(); })
	];

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.f)
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

TSharedRef<SWidget> SModesSurvie::Bouton(const FString& TitreBouton, const FString& Detail, uint8 Mode, bool bDisponible)
{
	const bool bActif = bDisponible && bPeutChoisir;
	return SNew(SButton)
		.IsEnabled(bActif)
		.ButtonColorAndOpacity(bDisponible ? FLinearColor(0.f, 0.25f, 0.35f, 1.f) : FLinearColor(0.12f, 0.12f, 0.14f, 1.f))
		.ContentPadding(FMargin(18.f, 10.f))
		.OnClicked(this, &SModesSurvie::Choisir, Mode)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(TitreBouton)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
				.ColorAndOpacity(bDisponible ? (Mode == 0 ? Blanc : Magenta) : Gris)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Detail)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(bDisponible ? Blanc : Gris).AutoWrapText(true)
			]
		];
}

FReply SModesSurvie::Choisir(uint8 Mode)
{
	QuandChoisi.ExecuteIfBound(Mode, VagueDepart);
	return FReply::Handled();
}

FReply SModesSurvie::OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche)
{
	if (Touche.GetKey() == EKeys::Escape || Touche.GetKey() == EKeys::F7)
	{
		QuandFerme.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometrie, Touche);
}
