// CHELLES 3D - Courses de rue : dessin du chrono en jeu et ecran COURSES (style neon : cyan / magenta / or)
#include "SCoursesEcran.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor Cyan(0.f, 0.85f, 1.f, 1.f);
	const FLinearColor Magenta(1.f, 0.1f, 0.75f, 1.f);
	const FLinearColor Or(1.f, 0.78f, 0.2f, 1.f);
	const FLinearColor Rouge(0.95f, 0.15f, 0.15f, 1.f);
	const FLinearColor Vert(0.3f, 1.f, 0.55f, 1.f);
	const FLinearColor Blanc(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Gris(0.6f, 0.6f, 0.65f, 1.f);
	const FLinearColor FondNuit(0.01f, 0.01f, 0.04f, 0.78f);

	FString Chrono(float Secondes)
	{
		const int32 Ms = FMath::Max(0, FMath::FloorToInt(Secondes * 1000.f));
		return FString::Printf(TEXT("%d:%02d.%02d"), Ms / 60000, (Ms / 1000) % 60, (Ms / 10) % 100);
	}

	FString Ecart(int32 Ms)
	{
		const int32 A = FMath::Abs(Ms);
		return FString::Printf(TEXT("%s%d.%02d"), Ms > 0 ? TEXT("+") : TEXT("-"), A / 1000, (A / 10) % 100);
	}

	FLinearColor Alpha(FLinearColor C, float A)
	{
		C.A *= FMath::Clamp(A, 0.f, 1.f);
		return C;
	}
}

// ===================================================================================
//  Affichage en jeu
// ===================================================================================

void SCoursesEcran::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SCoursesEcran::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Couleur);
}

FVector2D SCoursesEcran::Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
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

void SCoursesEcran::PeindreRepere(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, float Echelle) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Marge = 60.f * Echelle;
	FVector2D P(Etat.Repere.X * Ecran.X, Etat.Repere.Y * Ecran.Y);
	const FVector2D Centre = Ecran * 0.5f;
	FVector2D Dir = P - Centre;
	if (Dir.IsNearlyZero())
	{
		Dir = FVector2D(0.f, 1.f);
	}
	const bool bDehors = Etat.bRepereDerriere || P.X < Marge || P.X > Ecran.X - Marge || P.Y < Marge || P.Y > Ecran.Y - Marge;
	if (bDehors)
	{
		const float Kx = Dir.X != 0.f ? (Ecran.X * 0.5f - Marge) / FMath::Abs(Dir.X) : BIG_NUMBER;
		const float Ky = Dir.Y != 0.f ? (Ecran.Y * 0.5f - Marge) / FMath::Abs(Dir.Y) : BIG_NUMBER;
		P = Centre + Dir * FMath::Min(Kx, Ky);
	}
	const FLinearColor Couleur = Etat.bRepereArrivee ? Magenta : (Etat.Etat == 0 ? Cyan : Or);
	const float Pulse = 0.75f + 0.25f * FMath::Sin((float)FMath::Fmod(FPlatformTime::Seconds() * 5.0, 2.0 * PI));
	const float R = 15.f * Echelle;
	TArray<FVector2D> Losange;
	Losange.Add(P + FVector2D(0.f, -R));
	Losange.Add(P + FVector2D(R, 0.f));
	Losange.Add(P + FVector2D(0.f, R));
	Losange.Add(P + FVector2D(-R, 0.f));
	Losange.Add(P + FVector2D(0.f, -R));
	FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Losange, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.7f), true, 6.f * Echelle);
	FSlateDrawElement::MakeLines(Sortie, Couche + 1, G.ToPaintGeometry(), Losange, ESlateDrawEffect::None, Couleur * Pulse, true, 3.f * Echelle);
	Boite(Sortie, Couche + 1, G, P - FVector2D(3.f, 3.f) * Echelle, FVector2D(6.f, 6.f) * Echelle, Couleur);
	Ecrire(Sortie, Couche + 2, G, Etat.DistanceRepere, P + FVector2D(0.f, R + 4.f * Echelle), FMath::RoundToInt(14 * Echelle), Couleur, 0.5f);
}

int32 SCoursesEcran::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Echelle = FMath::Clamp(Ecran.Y / 1080.f, 0.6f, 2.f);
	const int32 Enorme = FMath::RoundToInt(110 * Echelle);
	const int32 Grand = FMath::RoundToInt(46 * Echelle);
	const int32 Chiffres = FMath::RoundToInt(34 * Echelle);
	const int32 Moyen = FMath::RoundToInt(20 * Echelle);
	const int32 Normal = FMath::RoundToInt(15 * Echelle);
	const int32 Petit = FMath::RoundToInt(13 * Echelle);
	const float Milieu = Ecran.X * 0.5f;

	// ---- Chrono en haut au centre (les missions sont en haut a gauche, la mini-carte en haut a droite) ----
	if (Etat.Etat == 1 || Etat.Etat == 2)
	{
		const float Largeur = 380.f * Echelle;
		const float Hauteur = (Petit * 1.5f + Chiffres * 1.25f + Normal * 1.5f) + 22.f * Echelle;
		const FVector2D Coin(Milieu - Largeur * 0.5f, 18.f * Echelle);
		Boite(Sortie, Couche, G, Coin, FVector2D(Largeur, Hauteur), FondNuit);
		Boite(Sortie, Couche + 1, G, Coin, FVector2D(Largeur, 2.f * Echelle), Cyan);
		Boite(Sortie, Couche + 1, G, Coin + FVector2D(0.f, Hauteur - 2.f * Echelle), FVector2D(Largeur, 2.f * Echelle), Magenta);

		float Y = Coin.Y + 10.f * Echelle;
		Ecrire(Sortie, Couche + 2, G, FString::Printf(TEXT("COURSE · %s"), *Etat.Nom), FVector2D(Milieu, Y), Petit, Cyan, 0.5f);
		Y += Petit * 1.5f;
		Ecrire(Sortie, Couche + 2, G, Chrono(Etat.Secondes), FVector2D(Milieu, Y), Chiffres, Blanc, 0.5f);
		Y += Chiffres * 1.25f;
		Ecrire(Sortie, Couche + 2, G, FString::Printf(TEXT("PORTE %d / %d"), Etat.Porte, Etat.Portes), FVector2D(Coin.X + 16.f * Echelle, Y), Normal, Magenta);
		Ecrire(Sortie, Couche + 2, G, Etat.Record, FVector2D(Coin.X + Largeur - 16.f * Echelle, Y), Normal, Or, 1.f);

		// Ecart avec le record a la derniere porte, 4 secondes
		if (Etat.bEcart && Etat.AgeEcart < 4.f)
		{
			const FLinearColor C = Alpha(Etat.EcartMs > 0 ? Rouge : Vert, 4.f - Etat.AgeEcart);
			Ecrire(Sortie, Couche + 2, G, Ecart(Etat.EcartMs), FVector2D(Milieu, Coin.Y + Hauteur + 8.f * Echelle), Moyen, C, 0.5f);
		}
	}

	// ---- Compte a rebours ----
	if (Etat.Etat == 1 || (Etat.Etat == 2 && Etat.AgeEtat < 1.f))
	{
		const bool bPartez = Etat.Etat == 2;
		const float Age = bPartez ? Etat.AgeEtat : FMath::Fmod(Etat.AgeEtat, 1.f);
		const FString Texte = bPartez ? FString(TEXT("PARTEZ !")) : FString::FromInt(FMath::Max(1, Etat.Compte));
		Ecrire(Sortie, Couche + 3, G, Texte, FVector2D(Milieu, Ecran.Y * 0.32f), Enorme, Alpha(bPartez ? Vert : Or, 1.4f - Age), 0.5f);
	}

	// ---- Arrivee / abandon : grand titre au centre ----
	if ((Etat.Etat == 3 || Etat.Etat == 4) && Etat.AgeEtat < 30.f)
	{
		const bool bArrivee = Etat.Etat == 3;
		const float A = FMath::Clamp(Etat.AgeEtat / 0.4f, 0.f, 1.f);
		const float Y = Ecran.Y * 0.26f;
		Boite(Sortie, Couche, G, FVector2D(0.f, Y - 20.f * Echelle), FVector2D(Ecran.X, Grand * (bArrivee ? 5.4f : 3.f)), FLinearColor(0.f, 0.f, 0.f, 0.55f * A));
		Ecrire(Sortie, Couche + 1, G, bArrivee ? TEXT("ARRIVÉE !") : TEXT("COURSE ABANDONNÉE"), FVector2D(Milieu, Y), Grand, Alpha(bArrivee ? Or : Rouge, A), 0.5f);
		float Y2 = Y + Grand * 1.35f;
		if (bArrivee)
		{
			Ecrire(Sortie, Couche + 1, G, FString::Printf(TEXT("%s  ·  %s"), *Etat.Nom, *Etat.TempsFinal), FVector2D(Milieu, Y2), Moyen, Alpha(Blanc, A), 0.5f);
			Y2 += Moyen * 1.5f;
			if (Etat.bNouveauRecord)
			{
				Ecrire(Sortie, Couche + 1, G, TEXT("NOUVEAU RECORD !"), FVector2D(Milieu, Y2), Moyen, Alpha(Magenta, A), 0.5f);
				Y2 += Moyen * 1.4f;
			}
			Ecrire(Sortie, Couche + 1, G, Etat.LigneRecord, FVector2D(Milieu, Y2), Normal, Alpha(Gris, A), 0.5f, false);
			Y2 += Normal * 1.5f;
			Ecrire(Sortie, Couche + 1, G, Etat.LigneClassement, FVector2D(Milieu, Y2), Normal, Alpha(Cyan, A), 0.5f);
			Y2 += Normal * 1.5f;
			if (Etat.XP > 0)
			{
				Ecrire(Sortie, Couche + 1, G, FString::Printf(TEXT("+%d XP"), Etat.XP), FVector2D(Milieu, Y2), Moyen, Alpha(Cyan, A), 0.5f);
			}
		}
		else
		{
			Ecrire(Sortie, Couche + 1, G, Etat.RaisonAbandon, FVector2D(Milieu, Y2), Moyen, Alpha(Blanc, A), 0.5f, false);
		}
	}

	// ---- Sur un depart : invitation en bas ----
	if (Etat.Etat == 0 && !Etat.Invitation.IsEmpty())
	{
		const float Largeur = 640.f * Echelle;
		const float Hauteur = Moyen * 1.5f + Normal * 1.5f + 20.f * Echelle;
		const FVector2D Coin(Milieu - Largeur * 0.5f, Ecran.Y * 0.74f);
		Boite(Sortie, Couche, G, Coin, FVector2D(Largeur, Hauteur), FondNuit);
		Boite(Sortie, Couche + 1, G, Coin, FVector2D(Largeur, 2.f * Echelle), Cyan);
		Ecrire(Sortie, Couche + 2, G, Etat.Invitation, FVector2D(Milieu, Coin.Y + 8.f * Echelle), Moyen, Cyan, 0.5f);
		Ecrire(Sortie, Couche + 2, G, Etat.InvitationDetail, FVector2D(Milieu, Coin.Y + 8.f * Echelle + Moyen * 1.5f), Normal, Blanc, 0.5f, false);
	}

	if (Etat.bRepere)
	{
		PeindreRepere(Sortie, Couche + 4, G, Echelle);
	}

	// ---- Message court ----
	if (!Etat.Message.IsEmpty() && Etat.AgeMessage < 5.f)
	{
		Ecrire(Sortie, Couche + 6, G, Etat.Message, FVector2D(Milieu, Ecran.Y * 0.66f), Moyen, Alpha(Cyan, 5.f - Etat.AgeMessage), 0.5f);
	}
	return Couche + 8;
}

// ===================================================================================
//  Ecran COURSES (touche Debut)
// ===================================================================================

void SCoursesMenu::Construct(const FArguments& InArgs)
{
	QuandChoisie = InArgs._QuandChoisie;
	QuandFerme = InArgs._QuandFerme;

	TSharedRef<SVerticalBox> Cartes = SNew(SVerticalBox);
	for (const FChoixCourse& Choix : InArgs._Choix)
	{
		Cartes->AddSlot().AutoHeight().Padding(0.f, 4.f)[Carte(Choix)];
	}
	if (InArgs._Choix.Num() == 0)
	{
		Cartes->AddSlot().AutoHeight()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Aucune course pour l'instant : les rues de la circulation ne sont pas encore prêtes.")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).ColorAndOpacity(Gris).AutoWrapText(true)
		];
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(860.f)
			.MaxDesiredHeight(900.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FondNuit)
				.Padding(FMargin(36.f, 28.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("COURSES DE RUE"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 40)).ColorAndOpacity(Cyan)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
					[
						SNew(STextBlock).Text(FText::FromString(InArgs._Aide)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).ColorAndOpacity(Gris).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SScrollBox) + SScrollBox::Slot()[Cartes]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 0.f).HAlign(HAlign_Right)
					[
						SNew(SButton)
						.ContentPadding(FMargin(14.f, 6.f))
						.OnClicked_Lambda([this]() { QuandFerme.ExecuteIfBound(); return FReply::Handled(); })
						[
							SNew(STextBlock).Text(FText::FromString(TEXT("FERMER"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SCoursesMenu::Carte(const FChoixCourse& Choix)
{
	const int32 Index = Choix.Index;
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.f, 0.18f, 0.26f, 0.9f))
		.Padding(FMargin(18.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%d.  %s"), Index + 1, *Choix.Titre)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(Magenta)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FText::FromString(Choix.Description)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(Blanc)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					SNew(STextBlock).Text(FText::FromString(Choix.Record)).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14)).ColorAndOpacity(Or)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(Choix.Classement).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(Cyan).AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FLinearColor(0.f, 0.35f, 0.45f, 1.f))
				.ContentPadding(FMargin(12.f, 8.f))
				.OnClicked_Lambda([this, Index]() { QuandChoisie.ExecuteIfBound(Index); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("MONTRER LE DÉPART"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).ColorAndOpacity(Blanc)
				]
			]
		];
}

FReply SCoursesMenu::OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche)
{
	if (Touche.GetKey() == EKeys::Escape || (ToucheFermer.IsValid() && Touche.GetKey() == ToucheFermer))
	{
		QuandFerme.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(Geometrie, Touche);
}
