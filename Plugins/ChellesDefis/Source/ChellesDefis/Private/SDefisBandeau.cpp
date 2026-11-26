// CHELLES 3D - Defi de la semaine : dessin du bandeau (style neon du menu et du HUD : cyan / magenta / or)
#include "SDefisBandeau.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor Cyan(0.f, 0.85f, 1.f, 1.f);
	const FLinearColor Magenta(1.f, 0.1f, 0.75f, 1.f);
	const FLinearColor Or(1.f, 0.78f, 0.2f, 1.f);
	const FLinearColor Vert(0.3f, 1.f, 0.55f, 1.f);
	const FLinearColor Blanc(1.f, 1.f, 1.f, 1.f);
	const FLinearColor Gris(0.65f, 0.65f, 0.7f, 1.f);
	const FLinearColor FondNuit(0.01f, 0.01f, 0.04f, 0.82f);

	struct FLigne
	{
		FString Texte;
		int32 Taille;
		FLinearColor Couleur;
		bool bGras;
		float EspaceAvant;
	};

	FLinearColor AvecAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
	}

	FString Reste(double FinSecondes)
	{
		const int32 S = FMath::Max(0, FMath::FloorToInt((float)(FinSecondes - FPlatformTime::Seconds())));
		const int32 J = S / 86400, H = (S % 86400) / 3600, M = (S % 3600) / 60;
		if (J > 0) return FString::Printf(TEXT("encore %d j %d h"), J, H);
		if (H > 0) return FString::Printf(TEXT("encore %d h %02d"), H, M);
		return FString::Printf(TEXT("encore %d min"), FMath::Max(1, M));
	}
}

void SDefisBandeau::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SDefisBandeau::Montrer(const FDefiSemaine& InDefi, bool bInReussite, float InDuree)
{
	Defi = InDefi;
	bReussite = bInReussite;
	Duree = FMath::Max(3.f, InDuree);
	Debut = FPlatformTime::Seconds();
}

bool SDefisBandeau::EstVisible() const
{
	return FPlatformTime::Seconds() - Debut < Duree;
}

void SDefisBandeau::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")), ESlateDrawEffect::None, Couleur);
}

FVector2D SDefisBandeau::Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FString& Texte, const FVector2D& Pos,
	int32 Taille, const FLinearColor& Couleur, bool bGras) const
{
	if (Texte.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", Taille);
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Dim = Mesure->Measure(Texte, Police);
	const FVector2D Coin(FMath::RoundToFloat(Pos.X - Dim.X * 0.5f), FMath::RoundToFloat(Pos.Y));
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Coin + FVector2D(2.f, 2.f), Dim), Texte, Police, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.8f * Couleur.A));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Coin, Dim), Texte, Police, ESlateDrawEffect::None, Couleur);
	return Dim;
}

TArray<FString> SDefisBandeau::Couper(const FString& Texte, int32 Taille, float LargeurMax) const
{
	TArray<FString> Lignes;
	TArray<FString> Mots;
	Texte.ParseIntoArrayWS(Mots);
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle("Regular", Taille);
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FString Courante;
	for (const FString& Mot : Mots)
	{
		const FString Essai = Courante.IsEmpty() ? Mot : Courante + TEXT(" ") + Mot;
		if (!Courante.IsEmpty() && Mesure->Measure(Essai, Police).X > LargeurMax)
		{
			Lignes.Add(Courante);
			Courante = Mot;
		}
		else
		{
			Courante = Essai;
		}
	}
	if (!Courante.IsEmpty())
	{
		Lignes.Add(Courante);
	}
	// au plus 3 lignes de texte : le reste est sur le Discord
	if (Lignes.Num() > 3)
	{
		Lignes.SetNum(3);
		Lignes[2] += TEXT("\u2026");
	}
	return Lignes;
}

int32 SDefisBandeau::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const double Age = FPlatformTime::Seconds() - Debut;
	if (Age < 0.0 || Age >= Duree || !Defi.bValide)
	{
		return Couche;
	}
	// Apparition 0,4 s, disparition 0,8 s
	const float Alpha = FMath::Clamp(FMath::Min((float)Age / 0.4f, (Duree - (float)Age) / 0.8f), 0.f, 1.f);

	const FVector2D Ecran = G.GetLocalSize();
	const float Echelle = FMath::Clamp(Ecran.Y / 1080.f, 0.6f, 2.f);
	const float Largeur = FMath::Min(Ecran.X - 40.f, 880.f * Echelle);
	const float Marge = 22.f * Echelle;
	const int32 Grand = FMath::RoundToInt(28 * Echelle);
	const int32 Moyen = FMath::RoundToInt(17 * Echelle);
	const int32 Normal = FMath::RoundToInt(15 * Echelle);
	const int32 Petit = FMath::RoundToInt(13 * Echelle);

	// ---- Les lignes du bandeau ----
	TArray<FLigne> Lignes;
	if (bReussite)
	{
		Lignes.Add({ TEXT("DÉFI DE LA SEMAINE RÉUSSI !"), Grand, Vert, true, 0.f });
		Lignes.Add({ Defi.Titre, Moyen, Blanc, true, 6.f });
		if (Defi.XP > 0)
		{
			Lignes.Add({ FString::Printf(TEXT("+%d XP pour tes pouvoirs. Bravo !"), Defi.XP), Moyen, Or, true, 8.f });
		}
		if (!Defi.MoiTexte.IsEmpty())
		{
			Lignes.Add({ TEXT("Ton résultat : ") + Defi.MoiTexte, Normal, Cyan, false, 6.f });
		}
	}
	else
	{
		Lignes.Add({ FString::Printf(TEXT("DÉFI DE LA SEMAINE %d   •   %s"), Defi.Numero, *Reste(Defi.FinSecondes)), Normal, Or, true, 0.f });
		Lignes.Add({ Defi.Titre, Grand, Blanc, true, 4.f });
		bool bPremiere = true;
		for (const FString& Morceau : Couper(Defi.Texte, Normal, Largeur - 2.f * Marge))
		{
			Lignes.Add({ Morceau, Normal, Cyan, false, bPremiere ? 8.f : 1.f });
			bPremiere = false;
		}
		if (Defi.bVerifiable)
		{
			FString Info = TEXT("Objectif : ") + Defi.ObjectifTexte;
			if (Defi.XP > 0)
			{
				Info += FString::Printf(TEXT("   •   +%d XP"), Defi.XP);
			}
			Lignes.Add({ Info, Moyen, Or, true, 10.f });

			if (!Defi.bMoi)
			{
				Lignes.Add({ TEXT("Choisis ton pseudo dans le chat (touche B) pour être classé"), Petit, Gris, false, 6.f });
			}
			else if (Defi.bMoiReussi)
			{
				Lignes.Add({ TEXT("Tu as déjà réussi ce défi : ") + Defi.MoiTexte, Normal, Vert, true, 6.f });
			}
			else if (Defi.MoiRang > 0)
			{
				Lignes.Add({ FString::Printf(TEXT("Ton meilleur résultat : %s  (%d%s)"), *Defi.MoiTexte, Defi.MoiRang, Defi.MoiRang == 1 ? TEXT("er") : TEXT("e")), Normal, Blanc, false, 6.f });
			}
			if (!Defi.Premier.IsEmpty())
			{
				FString Tete = TEXT("En tête : ") + Defi.Premier;
				if (Defi.Reussis == 1)
				{
					Tete += TEXT("   \u2022   1 joueur a r\u00E9ussi");
				}
				else if (Defi.Reussis > 1)
				{
					Tete += FString::Printf(TEXT("   \u2022   %d joueurs ont r\u00E9ussi"), Defi.Reussis);
				}
				Lignes.Add({ Tete, Petit, Magenta, false, 4.f });
			}
		}
		else
		{
			Lignes.Add({ TEXT("Poste ta participation sur le Discord CHELLES 3D"), Moyen, Or, true, 10.f });
		}
	}

	// ---- Hauteur totale ----
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	float Hauteur = Marge * 2.f;
	for (const FLigne& L : Lignes)
	{
		Hauteur += L.EspaceAvant * Echelle + Mesure->GetMaxCharacterHeight(FCoreStyle::GetDefaultFontStyle(L.bGras ? "Bold" : "Regular", L.Taille));
	}

	const float X0 = FMath::RoundToFloat((Ecran.X - Largeur) * 0.5f);
	const float Y0 = FMath::RoundToFloat(70.f * Echelle - (1.f - Alpha) * 24.f * Echelle);
	const FLinearColor Accent = bReussite ? Vert : Magenta;

	Boite(Sortie, Couche, G, FVector2D(X0, Y0), FVector2D(Largeur, Hauteur), AvecAlpha(FondNuit, Alpha));
	Boite(Sortie, Couche + 1, G, FVector2D(X0, Y0), FVector2D(Largeur, 3.f * Echelle), AvecAlpha(Accent, Alpha));
	Boite(Sortie, Couche + 1, G, FVector2D(X0, Y0 + Hauteur - 2.f * Echelle), FVector2D(Largeur, 2.f * Echelle), AvecAlpha(Cyan, Alpha));
	Boite(Sortie, Couche + 1, G, FVector2D(X0, Y0), FVector2D(2.f * Echelle, Hauteur), AvecAlpha(Accent, Alpha * 0.6f));
	Boite(Sortie, Couche + 1, G, FVector2D(X0 + Largeur - 2.f * Echelle, Y0), FVector2D(2.f * Echelle, Hauteur), AvecAlpha(Accent, Alpha * 0.6f));

	float Y = Y0 + Marge;
	const float Centre = X0 + Largeur * 0.5f;
	for (const FLigne& L : Lignes)
	{
		Y += L.EspaceAvant * Echelle;
		Ecrire(Sortie, Couche + 2, G, L.Texte, FVector2D(Centre, Y), L.Taille, AvecAlpha(L.Couleur, Alpha), L.bGras);
		Y += Mesure->GetMaxCharacterHeight(FCoreStyle::GetDefaultFontStyle(L.bGras ? "Bold" : "Regular", L.Taille));
	}
	return Couche + 4;
}
