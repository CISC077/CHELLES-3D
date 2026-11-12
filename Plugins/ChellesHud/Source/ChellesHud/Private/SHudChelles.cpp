// CHELLES 3D - HUD du joueur : le dessin (vie, niveau, XP, pouvoir, zombies, mini-carte)
// Les accents sont ecrits avec des codes unicode pour que Visual Studio les lise correctement.
#include "SHudChelles.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

// ---------------------------------------------------------------------------
// Paliers et pouvoirs (copie de PouvoirsTypes.h : a changer aux deux endroits)
// ---------------------------------------------------------------------------

namespace ChellesHudDonnees
{
	const TArray<FPalierHud>& Paliers()
	{
		static const TArray<FPalierHud> Liste =
		{
			{ TEXT("HABITANT"), 0 },
			{ TEXT("VOISIN"), 100 },
			{ TEXT("GARDIEN DU QUARTIER"), 250 },
			{ TEXT("FANT\u00D4ME DE CHELLES"), 500 },
			{ TEXT("CAM\u00C9L\u00C9ON"), 800 },
			{ TEXT("L\u00C9GENDE DE CHELLES"), 1200 }
		};
		return Liste;
	}

	const FPouvoirHud& Pouvoir(int32 Index)
	{
		static const FPouvoirHud Liste[NombrePouvoirs] =
		{
			{ TEXT("ENVOL"), 2, FLinearColor(0.25f, 0.85f, 1.f) },
			{ TEXT("BOULE DE FEU"), 3, FLinearColor(1.f, 0.45f, 0.08f) },
			{ TEXT("EFFET VERRE"), 4, FLinearColor(0.7f, 1.f, 0.95f) },
			{ TEXT("CAM\u00C9L\u00C9ON"), 5, FLinearColor(0.45f, 1.f, 0.35f) },
			{ TEXT("GEL"), 6, FLinearColor(0.55f, 0.75f, 1.f) }
		};
		return Liste[FMath::Clamp(Index, 0, NombrePouvoirs - 1)];
	}

	int32 NiveauPourXP(int32 XP)
	{
		int32 Niveau = 1;
		for (int32 i = 0; i < Paliers().Num(); ++i)
		{
			if (XP >= Paliers()[i].XP)
			{
				Niveau = i + 1;
			}
		}
		return Niveau;
	}
}

namespace
{
	// Memes couleurs que le menu et le panneau des pouvoirs (neon cyan / magenta sur bleu nuit, titres dores)
	const FLinearColor HudFond(0.01f, 0.02f, 0.06f, 0.78f);
	const FLinearColor HudCyanVif(0.f, 0.9f, 1.f, 1.f);
	const FLinearColor HudRose(1.f, 0.15f, 0.75f, 1.f);
	const FLinearColor HudDore(1.f, 0.78f, 0.25f, 1.f);
	const FLinearColor HudTerne(0.45f, 0.5f, 0.6f, 1.f);
	const FLinearColor HudSang(0.75f, 0.02f, 0.02f, 1.f);

	FString TexteDuree(float Secondes)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Secondes));
		if (S >= 60)
		{
			return FString::Printf(TEXT("%d:%02d"), S / 60, S % 60);
		}
		return FString::Printf(TEXT("%d s"), S);
	}

	/** Couleur de la barre de vie : vert, puis orange, puis rouge. */
	FLinearColor CouleurVie(float Part)
	{
		const FLinearColor Vert(0.2f, 0.95f, 0.35f, 1.f);
		const FLinearColor Orange(1.f, 0.6f, 0.05f, 1.f);
		const FLinearColor Rouge(1.f, 0.08f, 0.08f, 1.f);
		if (Part > 0.5f)
		{
			return FLinearColor::LerpUsingHSV(Orange, Vert, (Part - 0.5f) * 2.f);
		}
		return FLinearColor::LerpUsingHSV(Rouge, Orange, FMath::Clamp((Part - 0.15f) / 0.35f, 0.f, 1.f));
	}
}

void SHudChelles::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SHudChelles::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None, Couleur);
}

void SHudChelles::Panneau(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FVector2D& Taille, const FLinearColor& Fond, const FLinearColor& Bord) const
{
	const float Coupe = 12.f;
	// Fond : un rectangle plein, avec deux coins coupes (haut-gauche, bas-droite)
	Boite(Sortie, Couche, G, Pos + FVector2D(Coupe, 0.f), FVector2D(Taille.X - Coupe, Taille.Y - Coupe), Fond);
	Boite(Sortie, Couche, G, Pos + FVector2D(0.f, Coupe), FVector2D(Coupe, Taille.Y - Coupe), Fond);
	Boite(Sortie, Couche, G, Pos + FVector2D(Coupe, Taille.Y - Coupe), FVector2D(Taille.X - 2.f * Coupe, Coupe), Fond);

	TArray<FVector2D> Points;
	Points.Add(Pos + FVector2D(Coupe, 0.f));
	Points.Add(Pos + FVector2D(Taille.X, 0.f));
	Points.Add(Pos + FVector2D(Taille.X, Taille.Y - Coupe));
	Points.Add(Pos + FVector2D(Taille.X - Coupe, Taille.Y));
	Points.Add(Pos + FVector2D(0.f, Taille.Y));
	Points.Add(Pos + FVector2D(0.f, Coupe));
	Points.Add(Pos + FVector2D(Coupe, 0.f));
	FSlateDrawElement::MakeLines(Sortie, Couche + 1, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Bord, true, 1.5f);
}

void SHudChelles::Ecrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FString& Texte, const FSlateFontInfo& Police, const FLinearColor& Couleur, float Alignement) const
{
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Taille = Mesure->Measure(Texte, Police);
	const FVector2D Coin(FMath::RoundToFloat(Pos.X - Taille.X * Alignement), FMath::RoundToFloat(Pos.Y - Taille.Y * 0.5f));
	// ombre pour rester lisible sur la neige ou le ciel
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Coin + FVector2D(1.5f, 1.5f), Taille), Texte, Police,
		ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Couleur.A * 0.8f));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Coin, Taille), Texte, Police, ESlateDrawEffect::None, Couleur);
}

void SHudChelles::Croix(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	float Taille, const FLinearColor& Couleur) const
{
	const float Epais = Taille * 0.34f;
	Boite(Sortie, Couche, G, Centre - FVector2D(Taille * 0.5f, Epais * 0.5f), FVector2D(Taille, Epais), Couleur);
	Boite(Sortie, Couche, G, Centre - FVector2D(Epais * 0.5f, Taille * 0.5f), FVector2D(Epais, Taille), Couleur);
}

void SHudChelles::TeteZombie(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	float Taille, const FLinearColor& Couleur) const
{
	const float R = Taille * 0.5f;
	auto Trait = [&](const TArray<FVector2D>& Points, float Epaisseur)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
	};
	// crane
	TArray<FVector2D> Crane;
	for (int32 i = 0; i <= 20; ++i)
	{
		const float A = PI + PI * i / 20.f;
		Crane.Add(Centre + FVector2D(FMath::Cos(A) * R, FMath::Sin(A) * R * 0.9f));
	}
	Crane.Add(Centre + FVector2D(R, 0.35f * R));
	Crane.Add(Centre + FVector2D(0.55f * R, 0.5f * R));
	Crane.Add(Centre + FVector2D(0.45f * R, R));
	Crane.Add(Centre + FVector2D(-0.45f * R, R));
	Crane.Add(Centre + FVector2D(-0.55f * R, 0.5f * R));
	Crane.Add(Centre + FVector2D(-R, 0.35f * R));
	// copie d'abord : ajouter Crane[0] directement plante si le tableau s'agrandit pendant l'ajout
	const FVector2D PremierPoint = Crane[0];
	Crane.Add(PremierPoint);
	Trait(Crane, 2.f);
	// yeux en croix
	for (int32 Cote = -1; Cote <= 1; Cote += 2)
	{
		const FVector2D Oeil = Centre + FVector2D(Cote * 0.38f * R, -0.1f * R);
		const float E = 0.18f * R;
		Trait({ Oeil + FVector2D(-E, -E), Oeil + FVector2D(E, E) }, 2.f);
		Trait({ Oeil + FVector2D(-E, E), Oeil + FVector2D(E, -E) }, 2.f);
	}
	// dents
	Trait({ Centre + FVector2D(-0.3f * R, 0.62f * R), Centre + FVector2D(0.3f * R, 0.62f * R) }, 1.5f);
	Trait({ Centre + FVector2D(0.f, 0.5f * R), Centre + FVector2D(0.f, 0.85f * R) }, 1.5f);
}

float SHudChelles::DessinerCarte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Ecran) const
{
	if (!Etat.bCarte)
	{
		return 0.f;
	}
	const FSlateFontInfo PolicePetite = FCoreStyle::GetDefaultFontStyle("Regular", 8);
	const FSlateFontInfo PoliceNord = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	// Carte carree en haut a droite, plus petite sur les petits ecrans
	const float Cote = FMath::RoundToFloat(FMath::Clamp(Ecran.Y * 0.22f, 150.f, 230.f));
	const float Bord = 6.f;
	const float Legende = 18.f;
	const FVector2D TaillePanneau(Cote + 2.f * Bord, Cote + 2.f * Bord + Legende);
	const FVector2D CoinPanneau(FMath::RoundToFloat(Ecran.X - TaillePanneau.X - 22.f), 22.f);
	Panneau(Sortie, Couche + 2, G, CoinPanneau, TaillePanneau, HudFond, HudCyanVif * FLinearColor(1.f, 1.f, 1.f, 0.6f));
	Boite(Sortie, Couche + 4, G, CoinPanneau + FVector2D(12.f, 0.f), FVector2D(70.f, 3.f), HudCyanVif);

	const FVector2D Coin = CoinPanneau + FVector2D(Bord, Bord);
	const float Demi = Cote * 0.5f;
	const FVector2D Centre = Coin + FVector2D(Demi, Demi);

	// ---- Fond : le quartier vu du ciel, sinon un quadrillage sombre ----
	if (Etat.bImageCarte && BrushCarte.GetResourceObject())
	{
		FSlateDrawElement::MakeBox(Sortie, Couche + 4, G.ToPaintGeometry(Coin, FVector2D(Cote, Cote)), &BrushCarte,
			ESlateDrawEffect::None, FLinearColor(0.8f, 0.82f, 0.9f, 1.f));
	}
	else
	{
		Boite(Sortie, Couche + 4, G, Coin, FVector2D(Cote, Cote), FLinearColor(0.02f, 0.05f, 0.1f, 0.9f));
		for (int32 i = 1; i < 8; ++i)
		{
			const float D = Cote * i / 8.f;
			Boite(Sortie, Couche + 5, G, Coin + FVector2D(D, 0.f), FVector2D(1.f, Cote), FLinearColor(0.f, 0.9f, 1.f, 0.07f));
			Boite(Sortie, Couche + 5, G, Coin + FVector2D(0.f, D), FVector2D(Cote, 1.f), FLinearColor(0.f, 0.9f, 1.f, 0.07f));
		}
	}

	// Cercle a mi-distance et croix au centre, pour juger les distances
	{
		TArray<FVector2D> Cercle;
		for (int32 i = 0; i <= 48; ++i)
		{
			const float A = 2.f * PI * i / 48.f;
			Cercle.Add(Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Demi * 0.5f);
		}
		FSlateDrawElement::MakeLines(Sortie, Couche + 5, G.ToPaintGeometry(), Cercle, ESlateDrawEffect::None,
			FLinearColor(0.f, 0.9f, 1.f, 0.25f), true, 1.f);
		Boite(Sortie, Couche + 5, G, FVector2D(Centre.X, Coin.Y), FVector2D(1.f, Cote), FLinearColor(0.f, 0.9f, 1.f, 0.12f));
		Boite(Sortie, Couche + 5, G, FVector2D(Coin.X, Centre.Y), FVector2D(Cote, 1.f), FLinearColor(0.f, 0.9f, 1.f, 0.12f));
	}

	// ---- Points : teleporteurs, autres joueurs, zombies ----
	auto Losange = [&](const FVector2D& P, float R, const FLinearColor& C, float Epaisseur)
	{
		TArray<FVector2D> L;
		L.Add(P + FVector2D(0.f, -R));
		L.Add(P + FVector2D(R, 0.f));
		L.Add(P + FVector2D(0.f, R));
		L.Add(P + FVector2D(-R, 0.f));
		L.Add(P + FVector2D(0.f, -R));
		FSlateDrawElement::MakeLines(Sortie, Couche + 6, G.ToPaintGeometry(), L, ESlateDrawEffect::None, C, true, Epaisseur);
	};
	const float Pulse = 0.65f + 0.35f * FMath::Abs(FMath::Sin(Etat.Horloge * 4.f));
	const FLinearColor RougeZombie(1.f, 0.12f, 0.1f, 1.f);
	const FLinearColor CyanJoueur(0.2f, 0.95f, 1.f, 1.f);
	for (const FPointCarteChelles& Point : Etat.PointsCarte)
	{
		const FVector2D P = Centre + FVector2D(FMath::Clamp(Point.Pos.X, -1.f, 1.f), FMath::Clamp(Point.Pos.Y, -1.f, 1.f)) * (Demi - 6.f);
		const float Alpha = Point.bAuBord ? 0.65f : 1.f;
		switch (Point.Type)
		{
		case FPointCarteChelles::Teleporteur:
		{
			const float R = Point.bAuBord ? 4.f : 6.f;
			Losange(P, R + 1.5f, FLinearColor(0.f, 0.f, 0.f, 0.6f * Alpha), 4.f);
			Losange(P, R, HudRose * FLinearColor(1.f, 1.f, 1.f, Alpha), 2.f);
			Boite(Sortie, Couche + 7, G, P - FVector2D(1.5f, 1.5f), FVector2D(3.f, 3.f), FLinearColor(1.f, 1.f, 1.f, Alpha));
			break;
		}
		case FPointCarteChelles::Joueur:
		{
			const float R = Point.bAuBord ? 3.f : 4.f;
			Boite(Sortie, Couche + 6, G, P - FVector2D(R + 1.f, R + 1.f), FVector2D(2.f * R + 2.f, 2.f * R + 2.f), FLinearColor(0.f, 0.f, 0.f, 0.6f * Alpha));
			Boite(Sortie, Couche + 7, G, P - FVector2D(R, R), FVector2D(2.f * R, 2.f * R), CyanJoueur * FLinearColor(1.f, 1.f, 1.f, Alpha));
			break;
		}
		default:
		{
			const float R = 3.f;
			Boite(Sortie, Couche + 6, G, P - FVector2D(R + 1.f, R + 1.f), FVector2D(2.f * R + 2.f, 2.f * R + 2.f), FLinearColor(0.f, 0.f, 0.f, 0.55f));
			Boite(Sortie, Couche + 7, G, P - FVector2D(R, R), FVector2D(2.f * R, 2.f * R), RougeZombie * FLinearColor(1.f, 1.f, 1.f, Pulse));
			break;
		}
		}
	}

	// ---- Le joueur : fleche doree au centre, toujours vers le haut (la carte tourne avec la camera) ----
	{
		TArray<FVector2D> Fleche;
		Fleche.Add(Centre + FVector2D(0.f, -8.f));
		Fleche.Add(Centre + FVector2D(6.f, 6.f));
		Fleche.Add(Centre + FVector2D(0.f, 3.f));
		Fleche.Add(Centre + FVector2D(-6.f, 6.f));
		Fleche.Add(Centre + FVector2D(0.f, -8.f));
		FSlateDrawElement::MakeLines(Sortie, Couche + 6, G.ToPaintGeometry(), Fleche, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.7f), true, 4.5f);
		FSlateDrawElement::MakeLines(Sortie, Couche + 7, G.ToPaintGeometry(), Fleche, ESlateDrawEffect::None, HudDore, true, 2.5f);
	}

	// ---- N (nord) colle au bord, dans la bonne direction ----
	{
		FVector2D Nord = Etat.CarteNord;
		const float Plus = FMath::Max(FMath::Abs(Nord.X), FMath::Abs(Nord.Y));
		if (Plus > KINDA_SMALL_NUMBER)
		{
			Nord /= Plus; // sur le bord du carre
			const FVector2D P = Centre + Nord * (Demi - 9.f);
			Boite(Sortie, Couche + 6, G, P - FVector2D(7.f, 7.f), FVector2D(14.f, 14.f), FLinearColor(0.f, 0.f, 0.f, 0.65f));
			Ecrire(Sortie, Couche + 7, G, P, TEXT("N"), PoliceNord, HudDore, 0.5f);
		}
	}

	// Echelle en haut a gauche de la carte
	Ecrire(Sortie, Couche + 6, G, Coin + FVector2D(5.f, 8.f), FString::Printf(TEXT("%d m"), FMath::RoundToInt(Etat.CarteRayonMetres)),
		PolicePetite, FLinearColor(1.f, 1.f, 1.f, 0.8f), 0.f);

	// ---- Legende sous la carte ----
	{
		const float Y = Coin.Y + Cote + Legende * 0.5f + 2.f;
		float X = Coin.X + 8.f;
		Losange(FVector2D(X, Y), 4.f, HudRose, 2.f);
		Ecrire(Sortie, Couche + 6, G, FVector2D(X + 8.f, Y), TEXT("TÉLÉPORTEUR"), PolicePetite, FLinearColor(1.f, 0.75f, 0.92f, 1.f), 0.f);
		X = Coin.X + Cote * 0.6f;
		Boite(Sortie, Couche + 6, G, FVector2D(X - 3.f, Y - 3.f), FVector2D(6.f, 6.f), RougeZombie);
		Ecrire(Sortie, Couche + 6, G, FVector2D(X + 7.f, Y), TEXT("ZOMBIE"), PolicePetite, FLinearColor(1.f, 0.75f, 0.75f, 1.f), 0.f);
	}

	return TaillePanneau.Y;
}

int32 SHudChelles::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	using namespace ChellesHudDonnees;

	if (!Etat.bVisible)
	{
		return Couche;
	}
	const FVector2D Ecran = G.GetLocalSize();
	const FSlateFontInfo PoliceTitre = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo PoliceNormale = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	const FSlateFontInfo PolicePetite = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const FSlateFontInfo PoliceChiffre = FCoreStyle::GetDefaultFontStyle("Bold", 34);

	// =====================================================================
	// Bords de l'ecran en rouge : coup recu (flash) ou vie tres basse (pulsation)
	// =====================================================================
	if (Etat.bVie)
	{
		const float Part = Etat.VieMax > 0.f ? Etat.Vie / Etat.VieMax : 1.f;
		float Rouge = FMath::Clamp(1.f - Etat.AgeCoup / 0.6f, 0.f, 1.f) * 0.45f;
		if (!Etat.bMort && Part < 0.25f)
		{
			Rouge = FMath::Max(Rouge, 0.18f + 0.12f * FMath::Sin(Etat.Horloge * 6.f));
		}
		if (Rouge > 0.01f)
		{
			const float Bord = FMath::Min(Ecran.X, Ecran.Y) * 0.06f;
			for (int32 Bande = 0; Bande < 4; ++Bande)
			{
				const float A = Rouge * (1.f - Bande / 4.f);
				const float Dec = Bord * Bande / 4.f;
				const FLinearColor C(0.7f, 0.f, 0.f, A * 0.5f);
				const float Ep = Bord / 4.f;
				Boite(Sortie, Couche, G, FVector2D(0.f, Dec), FVector2D(Ecran.X, Ep), C);
				Boite(Sortie, Couche, G, FVector2D(0.f, Ecran.Y - Dec - Ep), FVector2D(Ecran.X, Ep), C);
				Boite(Sortie, Couche, G, FVector2D(Dec, 0.f), FVector2D(Ep, Ecran.Y), C);
				Boite(Sortie, Couche, G, FVector2D(Ecran.X - Dec - Ep, 0.f), FVector2D(Ep, Ecran.Y), C);
			}
		}
	}

	// =====================================================================
	// Panneau en bas a gauche : VIE, NIVEAU + XP, POUVOIR
	// =====================================================================
	const float Largeur = 360.f;
	const float Marge = 14.f;
	const float HauteurVie = 50.f;
	const float HauteurPouvoirs = Etat.bPouvoirs ? 74.f : 0.f;
	const float Hauteur = HauteurVie + HauteurPouvoirs + 6.f;
	const FVector2D Coin(22.f, FMath::RoundToFloat(Ecran.Y - Hauteur - 22.f));
	Panneau(Sortie, Couche + 2, G, Coin, FVector2D(Largeur, Hauteur), HudFond, HudCyanVif * FLinearColor(1.f, 1.f, 1.f, 0.6f));
	Boite(Sortie, Couche + 4, G, Coin + FVector2D(12.f, 0.f), FVector2D(90.f, 3.f), HudCyanVif);

	// ---- VIE ----
	{
		const float Y = Coin.Y + 16.f;
		const FLinearColor CouleurTitre = Etat.bMort ? HudSang : FLinearColor(1.f, 0.35f, 0.35f, 1.f);
		Croix(Sortie, Couche + 4, G, FVector2D(Coin.X + Marge + 6.f, Y), 12.f, CouleurTitre);
		Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Marge + 18.f, Y), TEXT("VIE"), PoliceTitre, CouleurTitre, 0.f);

		const FVector2D Barre(Coin.X + Marge, Y + 12.f);
		const float LargeurBarre = Largeur - 2.f * Marge;
		Boite(Sortie, Couche + 4, G, Barre, FVector2D(LargeurBarre, 12.f), FLinearColor(0.08f, 0.09f, 0.14f, 1.f));
		if (Etat.bVie)
		{
			const float Part = Etat.VieMax > 0.f ? FMath::Clamp(Etat.Vie / Etat.VieMax, 0.f, 1.f) : 0.f;
			Boite(Sortie, Couche + 5, G, Barre, FVector2D(LargeurBarre * Part, 12.f), CouleurVie(Part));
			// reflet en haut de la barre
			Boite(Sortie, Couche + 6, G, Barre, FVector2D(LargeurBarre * Part, 3.f), FLinearColor(1.f, 1.f, 1.f, 0.25f));
			// graduations tous les 10 %
			for (int32 i = 1; i < 10; ++i)
			{
				Boite(Sortie, Couche + 6, G, Barre + FVector2D(LargeurBarre * i / 10.f, 0.f), FVector2D(1.f, 12.f), FLinearColor(0.f, 0.f, 0.f, 0.35f));
			}
			const FString Chiffres = Etat.bMort ? FString(TEXT("MORT"))
				: FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(FMath::Max(0.f, Etat.Vie)), FMath::RoundToInt(Etat.VieMax));
			Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Largeur - Marge, Y), Chiffres, PoliceTitre, FLinearColor::White, 1.f);
		}
		else
		{
			Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Largeur - Marge, Y), TEXT("\u2014"), PoliceTitre, HudTerne, 1.f);
		}
	}

	// ---- NIVEAU + XP + POUVOIR ----
	if (Etat.bPouvoirs)
	{
		const TArray<FPalierHud>& Liste = Paliers();
		const int32 Niveau = FMath::Clamp(NiveauPourXP(Etat.XP), 1, Liste.Num());
		const bool bMax = Niveau >= Liste.Num();
		const int32 Bas = Liste[Niveau - 1].XP;
		const int32 Haut = bMax ? Bas : Liste[Niveau].XP;

		const float Y = Coin.Y + HauteurVie + 10.f;
		Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Marge, Y),
			FString::Printf(TEXT("NIV. %d  %s"), Niveau, Liste[Niveau - 1].Titre), PoliceTitre, HudDore, 0.f);
		Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Largeur - Marge, Y),
			bMax ? FString::Printf(TEXT("%d XP  MAX"), Etat.XP) : FString::Printf(TEXT("%d / %d XP"), Etat.XP, Haut), PoliceNormale, HudRose, 1.f);

		const FVector2D Barre(Coin.X + Marge, Y + 11.f);
		const float LargeurBarre = Largeur - 2.f * Marge;
		const float Part = bMax ? 1.f : FMath::Clamp(float(Etat.XP - Bas) / float(FMath::Max(1, Haut - Bas)), 0.f, 1.f);
		Boite(Sortie, Couche + 4, G, Barre, FVector2D(LargeurBarre, 6.f), FLinearColor(0.1f, 0.12f, 0.2f, 1.f));
		Boite(Sortie, Couche + 5, G, Barre, FVector2D(LargeurBarre * Part, 6.f), bMax ? HudDore : HudCyanVif);

		// Pouvoir : celui choisi avec Y ; sinon celui en cours ; sinon le dernier debloque
		int32 Quel = Etat.Selection;
		if (Quel < 0 || Quel >= NombrePouvoirs)
		{
			Quel = -1;
			for (int32 i = 0; i < NombrePouvoirs; ++i)
			{
				if (Etat.Effet[i] > 0.f)
				{
					Quel = i;
				}
			}
			if (Quel < 0)
			{
				for (int32 i = 0; i < NombrePouvoirs; ++i)
				{
					if (Niveau >= Pouvoir(i).Niveau)
					{
						Quel = i;
					}
				}
			}
		}

		const float YP = Y + 36.f;
		Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Marge, YP), TEXT("POUVOIR"), PolicePetite, HudTerne, 0.f);
		const float XNom = Coin.X + Marge + 62.f;
		if (Quel < 0)
		{
			const int32 Manque = Liste[FMath::Min(Pouvoir(0).Niveau, Liste.Num()) - 1].XP - Etat.XP;
			const int32 Minutes = FMath::Max(1, FMath::DivideAndRoundUp(Manque, XPParMinute));
			Ecrire(Sortie, Couche + 4, G, FVector2D(XNom, YP),
				FString::Printf(TEXT("aucun pour l'instant (ENVOL dans %d min)"), Minutes), PoliceNormale, HudTerne, 0.f);
		}
		else
		{
			const FPouvoirHud& P = Pouvoir(Quel);
			const bool bDebloque = Niveau >= P.Niveau;
			// pastille de couleur
			Boite(Sortie, Couche + 4, G, FVector2D(XNom, YP - 5.f), FVector2D(10.f, 10.f), bDebloque ? P.Couleur : HudTerne);
			Ecrire(Sortie, Couche + 4, G, FVector2D(XNom + 16.f, YP), P.Nom, PoliceTitre, bDebloque ? P.Couleur : HudTerne, 0.f);

			FString Etiquette;
			FLinearColor CouleurEtiquette = FLinearColor::White;
			if (!bDebloque)
			{
				Etiquette = FString::Printf(TEXT("BLOQU\u00C9 (niv. %d)"), P.Niveau);
				CouleurEtiquette = HudTerne;
			}
			else if (Etat.Effet[Quel] > 0.f)
			{
				Etiquette = FString::Printf(TEXT("ACTIF  %s"), *TexteDuree(Etat.Effet[Quel]));
				CouleurEtiquette = HudDore;
			}
			else if (Etat.Recharge[Quel] > 0.f)
			{
				Etiquette = FString::Printf(TEXT("RECHARGE  %s"), *TexteDuree(Etat.Recharge[Quel]));
				CouleurEtiquette = FLinearColor(0.75f, 0.78f, 0.85f, 1.f);
			}
			else
			{
				Etiquette = TEXT("PR\u00CAT  \u2022  J");
				CouleurEtiquette = FLinearColor(0.3f, 1.f, 0.5f, 1.f);
			}
			Ecrire(Sortie, Couche + 4, G, FVector2D(Coin.X + Largeur - Marge, YP), Etiquette, PoliceNormale, CouleurEtiquette, 1.f);
		}
	}

	// =====================================================================
	// Mini-carte en haut a droite
	// =====================================================================
	const float HauteurCarte = DessinerCarte(Sortie, Couche, G, Ecran);

	// =====================================================================
	// Panneau en haut a droite (sous la mini-carte) : invasion de zombies en cours
	// =====================================================================
	if (Etat.bZombies && Etat.bInvasion)
	{
		const float LargeurZ = 250.f;
		const float HauteurZ = 104.f;
		const FVector2D CoinZ(FMath::RoundToFloat(Ecran.X - LargeurZ - 22.f), HauteurCarte > 0.f ? 22.f + HauteurCarte + 12.f : 22.f);
		const float Pulse = 0.6f + 0.4f * FMath::Abs(FMath::Sin(Etat.Horloge * 3.f));
		Panneau(Sortie, Couche + 2, G, CoinZ, FVector2D(LargeurZ, HauteurZ), FLinearColor(0.08f, 0.f, 0.f, 0.8f),
			HudSang * FLinearColor(1.f, 1.f, 1.f, Pulse));
		Boite(Sortie, Couche + 4, G, CoinZ + FVector2D(12.f, 0.f), FVector2D(90.f, 3.f), FLinearColor(1.f, 0.1f, 0.1f, Pulse));

		Ecrire(Sortie, Couche + 4, G, FVector2D(CoinZ.X + LargeurZ * 0.5f, CoinZ.Y + 16.f), TEXT("INVASION ZOMBIE"), PoliceTitre,
			FLinearColor(1.f, 0.15f, 0.12f, Pulse), 0.5f);

		TeteZombie(Sortie, Couche + 4, G, FVector2D(CoinZ.X + 40.f, CoinZ.Y + 56.f), 34.f, FLinearColor(0.55f, 0.85f, 0.35f, 1.f));
		Ecrire(Sortie, Couche + 4, G, FVector2D(CoinZ.X + 74.f, CoinZ.Y + 54.f), FString::FromInt(FMath::Max(0, Etat.ZombiesRestants)),
			PoliceChiffre, FLinearColor::White, 0.f);
		Ecrire(Sortie, Couche + 4, G, FVector2D(CoinZ.X + LargeurZ - Marge, CoinZ.Y + 48.f),
			Etat.ZombiesRestants > 1 ? TEXT("zombies") : TEXT("zombie"), PoliceNormale, FLinearColor(1.f, 0.8f, 0.8f, 1.f), 1.f);
		Ecrire(Sortie, Couche + 4, G, FVector2D(CoinZ.X + LargeurZ - Marge, CoinZ.Y + 64.f),
			Etat.ZombiesRestants > 1 ? TEXT("restants") : TEXT("restant"), PoliceNormale, FLinearColor(1.f, 0.8f, 0.8f, 1.f), 1.f);

		if (Etat.SecondesInvasion > 0.f)
		{
			Ecrire(Sortie, Couche + 4, G, FVector2D(CoinZ.X + LargeurZ * 0.5f, CoinZ.Y + HauteurZ - 14.f),
				FString::Printf(TEXT("tiens bon encore %s"), *TexteDuree(Etat.SecondesInvasion)), PolicePetite,
				FLinearColor(1.f, 0.7f, 0.7f, 0.9f), 0.5f);
		}
	}

	return Couche + 8;
}
