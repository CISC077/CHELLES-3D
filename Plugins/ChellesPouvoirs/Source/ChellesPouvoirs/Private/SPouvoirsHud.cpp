// CHELLES 3D - XP et pouvoirs : affichage a l'ecran (barre d'XP, 5 cases de pouvoirs, annonces)
#include "SPouvoirsHud.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Couleurs Chelles 3D (meme famille que le menu : neon cyan / magenta sur bleu nuit)
	const FLinearColor HudNuit(0.01f, 0.02f, 0.06f, 0.78f);
	const FLinearColor HudCyan(0.f, 0.9f, 1.f, 1.f);
	const FLinearColor HudMagenta(1.f, 0.15f, 0.75f, 1.f);
	const FLinearColor HudOr(1.f, 0.78f, 0.25f, 1.f);
	const FLinearColor HudGris(0.45f, 0.5f, 0.6f, 1.f);

	FString HudDuree(float Secondes)
	{
		const int32 S = FMath::CeilToInt(Secondes);
		if (S >= 60)
		{
			return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
		}
		return FString::Printf(TEXT("%d s"), S);
	}

	/** Temps de jeu restant avant d'atteindre XPVoulue (10 XP par minute). */
	FString HudTempsAvant(int32 XPVoulue, int32 XP)
	{
		const int32 Minutes = FMath::Max(1, FMath::DivideAndRoundUp(XPVoulue - XP, ChellesPouvoirs::XPParMinute));
		if (Minutes >= 60)
		{
			return FString::Printf(TEXT("%d h %02d"), Minutes / 60, Minutes % 60);
		}
		return FString::Printf(TEXT("%d min"), Minutes);
	}
}

void SPouvoirsHud::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SPouvoirsHud::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None, Couleur);
}

void SPouvoirsHud::Cadre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FVector2D& Taille, const FLinearColor& Couleur, float Epaisseur) const
{
	TArray<FVector2D> Points;
	Points.Add(Pos);
	Points.Add(Pos + FVector2D(Taille.X, 0.f));
	Points.Add(Pos + Taille);
	Points.Add(Pos + FVector2D(0.f, Taille.Y));
	Points.Add(Pos);
	FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
}

void SPouvoirsHud::Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FString& Texte, const FSlateFontInfo& Police, const FLinearColor& Couleur, float Alignement) const
{
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Taille = Mesure->Measure(Texte, Police);
	const FVector2D Coin(Pos.X - Taille.X * Alignement, Pos.Y - Taille.Y * 0.5f);
	// ombre pour rester lisible sur la neige ou le ciel
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Coin + FVector2D(1.5f, 1.5f), Taille), Texte, Police,
		ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Couleur.A * 0.8f));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Coin, Taille), Texte, Police, ESlateDrawEffect::None, Couleur);
}

void SPouvoirsHud::Icone(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	float Taille, int32 Quel, const FLinearColor& Couleur) const
{
	const float R = Taille * 0.5f;
	auto Trait = [&](const TArray<FVector2D>& Points, float Epaisseur)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
	};
	auto Cercle = [&](const FVector2D& C, float Rayon)
	{
		TArray<FVector2D> P;
		for (int32 i = 0; i <= 24; ++i)
		{
			const float A = 2.f * PI * i / 24.f;
			P.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rayon);
		}
		Trait(P, 2.f);
	};

	switch (Quel)
	{
	case ChellesPouvoirs::Envol: // deux chevrons vers le haut
		Trait({ Centre + FVector2D(-R, 0.1f * R), Centre + FVector2D(0.f, -0.7f * R), Centre + FVector2D(R, 0.1f * R) }, 3.f);
		Trait({ Centre + FVector2D(-R, 0.8f * R), Centre + FVector2D(0.f, 0.f), Centre + FVector2D(R, 0.8f * R) }, 3.f);
		break;
	case ChellesPouvoirs::BouleDeFeu: // flamme
	{
		TArray<FVector2D> P;
		P.Add(Centre + FVector2D(0.f, -R));
		P.Add(Centre + FVector2D(0.35f * R, -0.35f * R));
		P.Add(Centre + FVector2D(0.7f * R, 0.25f * R));
		P.Add(Centre + FVector2D(0.45f * R, 0.85f * R));
		P.Add(Centre + FVector2D(0.f, R));
		P.Add(Centre + FVector2D(-0.45f * R, 0.85f * R));
		P.Add(Centre + FVector2D(-0.7f * R, 0.25f * R));
		P.Add(Centre + FVector2D(-0.25f * R, -0.2f * R));
		P.Add(Centre + FVector2D(0.f, -R));
		Trait(P, 2.5f);
		Cercle(Centre + FVector2D(0.f, 0.45f * R), 0.3f * R);
		break;
	}
	case ChellesPouvoirs::Verre: // losange de verre avec reflet
		Trait({ Centre + FVector2D(0.f, -R), Centre + FVector2D(R, 0.f), Centre + FVector2D(0.f, R), Centre + FVector2D(-R, 0.f), Centre + FVector2D(0.f, -R) }, 2.5f);
		Trait({ Centre + FVector2D(-0.45f * R, -0.1f * R), Centre + FVector2D(-0.1f * R, -0.45f * R) }, 2.f);
		break;
	case ChellesPouvoirs::Cameleon: // deux silhouettes qui se croisent
		Cercle(Centre + FVector2D(-0.35f * R, -0.45f * R), 0.3f * R);
		Cercle(Centre + FVector2D(0.35f * R, -0.45f * R), 0.3f * R);
		Trait({ Centre + FVector2D(-0.85f * R, R), Centre + FVector2D(-0.35f * R, 0.f), Centre + FVector2D(0.15f * R, R) }, 2.5f);
		Trait({ Centre + FVector2D(-0.15f * R, R), Centre + FVector2D(0.35f * R, 0.f), Centre + FVector2D(0.85f * R, R) }, 2.5f);
		break;
	case ChellesPouvoirs::Gel: // flocon
	default:
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = PI / 2.f + i * PI / 3.f;
			const FVector2D D(FMath::Cos(A), FMath::Sin(A));
			Trait({ Centre - D * R, Centre + D * R }, 2.5f);
			for (int32 Bout = -1; Bout <= 1; Bout += 2)
			{
				const FVector2D Pointe = Centre + D * R * 0.6f * Bout;
				const FVector2D Perp(-D.Y, D.X);
				Trait({ Pointe + (Perp - D * Bout) * 0.25f * R, Pointe, Pointe + (-Perp - D * Bout) * 0.25f * R }, 2.f);
			}
		}
		break;
	}
}

int32 SPouvoirsHud::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace ChellesPouvoirs;

	if (!Etat.bVisible)
	{
		return Couche;
	}
	const FVector2D Ecran = G.GetLocalSize();
	const FSlateFontInfo PoliceTitre = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo PoliceCase = FCoreStyle::GetDefaultFontStyle("Bold", 9);
	const FSlateFontInfo PolicePetite = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const FSlateFontInfo PoliceBanniere = FCoreStyle::GetDefaultFontStyle("Bold", 34);
	const FSlateFontInfo PoliceSousBanniere = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	const FSlateFontInfo PoliceEtat = FCoreStyle::GetDefaultFontStyle("Bold", 22);

	// --- Teinte plein ecran : effet verre (moi) ou gele (moi) ---
	if (Etat.MoiGele > 0.f)
	{
		Boite(Sortie, Couche, G, FVector2D::ZeroVector, Ecran, FLinearColor(0.55f, 0.8f, 1.f, 0.32f));
		Ecrire(Sortie, Couche + 1, G, FVector2D(Ecran.X * 0.5f, Ecran.Y * 0.38f),
			FString::Printf(TEXT("G\u00C9L\u00C9 !  %s"), *HudDuree(Etat.MoiGele)), PoliceEtat, FLinearColor(0.85f, 0.95f, 1.f), 0.5f);
	}
	else if (Etat.MoiVerre > 0.f)
	{
		Boite(Sortie, Couche, G, FVector2D::ZeroVector, Ecran, FLinearColor(0.6f, 1.f, 0.95f, 0.10f));
		Ecrire(Sortie, Couche + 1, G, FVector2D(Ecran.X * 0.5f, 70.f),
			FString::Printf(TEXT("EFFET VERRE : PERSONNE NE TE VOIT  %s"), *HudDuree(Etat.MoiVerre)), PoliceTitre, FLinearColor(0.75f, 1.f, 0.95f), 0.5f);
	}

	// --- Panneau en bas au centre ---
	const float Largeur = 470.f;
	const float Hauteur = 132.f;
	const FVector2D Coin(FMath::RoundToFloat((Ecran.X - Largeur) * 0.5f), Ecran.Y - Hauteur - 18.f);
	Boite(Sortie, Couche + 2, G, Coin, FVector2D(Largeur, Hauteur), HudNuit);
	Boite(Sortie, Couche + 3, G, Coin, FVector2D(Largeur, 2.f), HudCyan);

	const TArray<FPalier>& Liste = Paliers();
	const int32 Niveau = FMath::Clamp(Etat.Niveau, 1, Liste.Num());
	const bool bMax = Niveau >= Liste.Num();
	const int32 Bas = Liste[Niveau - 1].XP;
	const int32 Haut = bMax ? Bas : Liste[Niveau].XP;

	Ecrire(Sortie, Couche + 3, G, Coin + FVector2D(14.f, 16.f),
		FString::Printf(TEXT("NIV. %d   %s"), Niveau, Liste[Niveau - 1].Titre), PoliceTitre, HudCyan, 0.f);
	Ecrire(Sortie, Couche + 3, G, Coin + FVector2D(Largeur - 14.f, 16.f),
		bMax ? FString::Printf(TEXT("%d XP"), Etat.XP) : FString::Printf(TEXT("%d / %d XP"), Etat.XP, Haut), PoliceTitre, HudMagenta, 1.f);

	// Barre d'XP
	const FVector2D Barre = Coin + FVector2D(14.f, 30.f);
	const float LargeurBarre = Largeur - 28.f;
	Boite(Sortie, Couche + 3, G, Barre, FVector2D(LargeurBarre, 7.f), FLinearColor(0.1f, 0.12f, 0.2f, 1.f));
	const float Part = bMax ? 1.f : FMath::Clamp(float(Etat.XP - Bas) / float(FMath::Max(1, Haut - Bas)), 0.f, 1.f);
	Boite(Sortie, Couche + 4, G, Barre, FVector2D(LargeurBarre * Part, 7.f), bMax ? HudOr : HudCyan);

	// Cases des pouvoirs
	const float TailleCase = 58.f;
	const float Ecart = (Largeur - 28.f - TailleCase * NombrePouvoirs) / (NombrePouvoirs - 1);
	for (int32 i = 0; i < NombrePouvoirs; ++i)
	{
		const FInfoPouvoir& P = Pouvoir(i);
		const bool bDebloque = Niveau >= P.Niveau;
		const FVector2D Case = Coin + FVector2D(14.f + i * (TailleCase + Ecart), 44.f);
		const FVector2D Milieu = Case + FVector2D(TailleCase * 0.5f, TailleCase * 0.5f - 4.f);

		Boite(Sortie, Couche + 3, G, Case, FVector2D(TailleCase, TailleCase), bDebloque ? FLinearColor(0.04f, 0.07f, 0.16f, 1.f) : FLinearColor(0.03f, 0.03f, 0.05f, 1.f));
		const FLinearColor CouleurIcone = bDebloque ? P.Couleur : HudGris * FLinearColor(1.f, 1.f, 1.f, 0.5f);
		Icone(Sortie, Couche + 4, G, Milieu, 28.f, i, CouleurIcone);

		// Recharge : la case se remplit de sombre par le haut
		if (bDebloque && Etat.Recharge[i] > 0.f && Etat.Effet[i] <= 0.f)
		{
			const float Total = FMath::Max(0.1f, P.Recharge);
			const float Reste = FMath::Clamp(Etat.Recharge[i] / Total, 0.f, 1.f);
			Boite(Sortie, Couche + 5, G, Case, FVector2D(TailleCase, TailleCase * Reste), FLinearColor(0.f, 0.f, 0.f, 0.65f));
			Ecrire(Sortie, Couche + 6, G, Milieu, HudDuree(Etat.Recharge[i]), PoliceTitre, FLinearColor::White, 0.5f);
		}
		if (Etat.Effet[i] > 0.f)
		{
			Ecrire(Sortie, Couche + 6, G, Milieu, HudDuree(Etat.Effet[i]), PoliceTitre, HudOr, 0.5f);
		}
		if (!bDebloque)
		{
			Ecrire(Sortie, Couche + 6, G, Milieu, HudTempsAvant(Liste[FMath::Min(P.Niveau, Liste.Num()) - 1].XP, Etat.XP), PoliceCase, FLinearColor(0.8f, 0.8f, 0.85f), 0.5f);
		}

		// Cadre : magenta = pouvoir choisi, couleur du pouvoir = effet en cours
		if (Etat.Effet[i] > 0.f)
		{
			Cadre(Sortie, Couche + 7, G, Case, FVector2D(TailleCase, TailleCase), HudOr, 3.f);
		}
		else if (i == Etat.Selection)
		{
			Cadre(Sortie, Couche + 7, G, Case, FVector2D(TailleCase, TailleCase), HudMagenta, 3.f);
		}
		else
		{
			Cadre(Sortie, Couche + 7, G, Case, FVector2D(TailleCase, TailleCase), bDebloque ? P.Couleur * FLinearColor(1.f, 1.f, 1.f, 0.5f) : HudGris * FLinearColor(1.f, 1.f, 1.f, 0.3f), 1.f);
		}
		Ecrire(Sortie, Couche + 4, G, Case + FVector2D(TailleCase * 0.5f, TailleCase + 9.f), P.Nom, PoliceCase,
			bDebloque ? FLinearColor(0.85f, 0.9f, 1.f) : HudGris, 0.5f);
	}

	Ecrire(Sortie, Couche + 3, G, FVector2D(Coin.X + Largeur * 0.5f, Coin.Y - 12.f),
		TEXT("J : UTILISER LE POUVOIR     Y : CHANGER DE POUVOIR"), PolicePetite, FLinearColor(0.8f, 0.85f, 0.95f, 0.85f), 0.5f);

	// --- Petit message au-dessus du panneau ---
	if (!Etat.Info.IsEmpty() && Etat.AgeInfo < 3.5f)
	{
		const float Alpha = FMath::Clamp(3.5f - Etat.AgeInfo, 0.f, 1.f);
		Ecrire(Sortie, Couche + 8, G, FVector2D(Ecran.X * 0.5f, Coin.Y - 36.f), Etat.Info, PoliceTitre, FLinearColor(1.f, 1.f, 1.f, Alpha), 0.5f);
	}

	// --- Grande annonce (nouveau niveau) ---
	if (!Etat.Banniere.IsEmpty() && Etat.AgeBanniere < 6.f)
	{
		const float Entree = FMath::Clamp(Etat.AgeBanniere / 0.3f, 0.f, 1.f);
		const float Alpha = FMath::Min(Entree, FMath::Clamp(6.f - Etat.AgeBanniere, 0.f, 1.f));
		const float Y = Ecran.Y * 0.24f;
		Boite(Sortie, Couche + 8, G, FVector2D(0.f, Y - 46.f), FVector2D(Ecran.X, 98.f), FLinearColor(0.01f, 0.02f, 0.06f, 0.6f * Alpha));
		Boite(Sortie, Couche + 9, G, FVector2D(Ecran.X * 0.5f - 220.f * Entree, Y - 46.f), FVector2D(440.f * Entree, 2.f), HudMagenta * FLinearColor(1.f, 1.f, 1.f, Alpha));
		Ecrire(Sortie, Couche + 9, G, FVector2D(Ecran.X * 0.5f, Y - 10.f), Etat.Banniere, PoliceBanniere, HudOr * FLinearColor(1.f, 1.f, 1.f, Alpha), 0.5f);
		Ecrire(Sortie, Couche + 9, G, FVector2D(Ecran.X * 0.5f, Y + 30.f), Etat.SousBanniere, PoliceSousBanniere, HudCyan * FLinearColor(1.f, 1.f, 1.f, Alpha), 0.5f);
	}

	return Couche + 11;
}
