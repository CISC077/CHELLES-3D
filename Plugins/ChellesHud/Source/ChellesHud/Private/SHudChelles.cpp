// CHELLES 3D - HUD du joueur : le dessin (vie, niveau, XP, pouvoir, zombies, mini-carte GPS en bas a gauche)
// Les accents sont ecrits avec des codes unicode pour que Visual Studio les lise correctement.
#include "SHudChelles.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	/**
	 * 2026-11-22 (demande MERIDJA) : TousLesPouvoirs=Oui dans Plugins\ChellesPouvoirs\Pouvoirs.txt
	 * = tous les pouvoirs ouverts des le depart pour tout le monde. Lu une fois (absent = Oui).
	 */
	bool TousLesPouvoirsOuverts()
	{
		static int32 Lu = -1;
		if (Lu < 0)
		{
			Lu = 1;
			TArray<FString> Lignes;
			const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesPouvoirs"), TEXT("Pouvoirs.txt"));
			if (FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
			{
				for (const FString& Brute : Lignes)
				{
					FString Cle, Valeur;
					const FString Ligne = Brute.TrimStartAndEnd();
					if (!Ligne.StartsWith(TEXT("#")) && Ligne.Split(TEXT("="), &Cle, &Valeur)
						&& Cle.TrimStartAndEnd().Equals(TEXT("TousLesPouvoirs"), ESearchCase::IgnoreCase))
					{
						const FString V = Valeur.TrimStartAndEnd();
						Lu = (V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("True"), ESearchCase::IgnoreCase) || V == TEXT("1")) ? 1 : 0;
					}
				}
			}
		}
		return Lu == 1;
	}
}

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

void SHudChelles::Remplir(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
	const FLinearColor& Couleur, const FSlateBrush* Image, const FVector2D& CoinImage, const FVector2D& TailleImage) const
{
	if (Points.Num() < 3)
	{
		return;
	}
	const FSlateBrush* Pinceau = Image ? Image : FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FSlateResourceHandle Ressource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Pinceau);
	const FSlateRenderTransform Transformation = G.GetAccumulatedRenderTransform();
	const FColor CouleurSommets = Couleur.ToFColor(true);
	const FVector2D Echelle(1.f / FMath::Max(TailleImage.X, 1.f), 1.f / FMath::Max(TailleImage.Y, 1.f));

	TArray<FSlateVertex> Sommets;
	Sommets.Reserve(Points.Num());
	for (const FVector2D& P : Points)
	{
		const FVector2D UV = Image ? (P - CoinImage) * Echelle : FVector2D(0.5f, 0.5f);
		Sommets.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transformation, P, UV, CouleurSommets));
	}
	// eventail depuis le premier point
	TArray<SlateIndex> Indices;
	Indices.Reserve((Points.Num() - 2) * 3);
	for (int32 i = 1; i + 1 < Points.Num(); ++i)
	{
		Indices.Add(0);
		Indices.Add((SlateIndex)i);
		Indices.Add((SlateIndex)(i + 1));
	}
	// l'image vue du ciel n'a pas toujours un canal alpha plein : on l'ignore, seule la transparence de Couleur compte
	FSlateDrawElement::MakeCustomVerts(Sortie, Couche, Ressource, Sommets, Indices, nullptr, 0, 0,
		Image ? ESlateDrawEffect::IgnoreTextureAlpha : ESlateDrawEffect::None);
}

namespace
{
	/** Contour d'un rectangle aux coins arrondis (sens des aiguilles d'une montre, a l'ecran). */
	void PointsArrondis(TArray<FVector2D>& Points, const FVector2D& Pos, const FVector2D& Taille, float Rayon)
	{
		Points.Reset();
		Rayon = FMath::Min(Rayon, FMath::Min(Taille.X, Taille.Y) * 0.5f);
		const FVector2D Centres[4] =
		{
			Pos + FVector2D(Rayon, Rayon),
			Pos + FVector2D(Taille.X - Rayon, Rayon),
			Pos + FVector2D(Taille.X - Rayon, Taille.Y - Rayon),
			Pos + FVector2D(Rayon, Taille.Y - Rayon)
		};
		const int32 Pas = 8;
		for (int32 Coin = 0; Coin < 4; ++Coin)
		{
			// haut-gauche 180..270, haut-droite 270..360, bas-droite 0..90, bas-gauche 90..180 (Y vers le bas)
			const float Debut = PI + Coin * HALF_PI;
			for (int32 i = 0; i <= Pas; ++i)
			{
				const float A = Debut + HALF_PI * i / Pas;
				Points.Add(Centres[Coin] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rayon);
			}
		}
	}

	void PointsCercle(TArray<FVector2D>& Points, const FVector2D& Centre, float Rayon, int32 Pas = 20)
	{
		Points.Reset();
		for (int32 i = 0; i < Pas; ++i)
		{
			const float A = 2.f * PI * i / Pas;
			Points.Add(Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rayon);
		}
	}

	/** Ramene un point dans le rectangle [Min, Max] sur la ligne qui part de Depart (qui est dedans). Renvoie vrai s'il etait dehors. */
	bool RamenerDansCadre(FVector2D& P, const FVector2D& Depart, const FVector2D& Min, const FVector2D& Max)
	{
		if (P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y)
		{
			return false;
		}
		const FVector2D D = P - Depart;
		float T = 1.f;
		if (D.X > KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Max.X - Depart.X) / D.X); }
		if (D.X < -KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Min.X - Depart.X) / D.X); }
		if (D.Y > KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Max.Y - Depart.Y) / D.Y); }
		if (D.Y < -KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Min.Y - Depart.Y) / D.Y); }
		P = Depart + D * FMath::Max(T, 0.f);
		return true;
	}
}

float SHudChelles::DessinerCarte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Ecran) const
{
	if (!Etat.bCarte)
	{
		return 0.f;
	}
	const FSlateFontInfo PoliceDistance = FCoreStyle::GetDefaultFontStyle("Bold", 13);
	const FSlateFontInfo PolicePetite = FCoreStyle::GetDefaultFontStyle("Bold", 7);
	const FSlateFontInfo PoliceNord = FCoreStyle::GetDefaultFontStyle("Bold", 8);

	// Carte facon GPS en bas a gauche : cadre arrondi semi-transparent, le joueur en bas au centre
	const FVector2D Taille(ChellesCarte::Largeur, ChellesCarte::Hauteur);
	const FVector2D Coin(22.f, FMath::RoundToFloat(Ecran.Y - Taille.Y - 22.f));
	const float Arrondi = 18.f;
	const FVector2D Joueur = Coin + FVector2D(Taille.X * 0.5f, Taille.Y * ChellesCarte::PlaceJoueur);
	// 1 unite de position = la moitie de la largeur de la carte
	const float Echelle = Taille.X * 0.5f;

	TArray<FVector2D> Forme;
	PointsArrondis(Forme, Coin, Taille, Arrondi);

	// ---- Fond : verre fume, puis le quartier vu du ciel (ou un quadrillage leger) ----
	Remplir(Sortie, Couche + 2, G, Forme, FLinearColor(0.12f, 0.14f, 0.17f, 0.55f));
	if (Etat.bImageCarte && BrushCarte.GetResourceObject())
	{
		const FLinearColor Teinte(0.78f, 0.8f, 0.84f, 0.8f);
		if (Etat.bCoinsArrondis)
		{
			Remplir(Sortie, Couche + 3, G, Forme, Teinte, &BrushCarte, Coin, Taille);
		}
		else
		{
			FSlateDrawElement::MakeBox(Sortie, Couche + 3, G.ToPaintGeometry(Coin + FVector2D(4.f, 4.f), Taille - FVector2D(8.f, 8.f)),
				&BrushCarte, ESlateDrawEffect::IgnoreTextureAlpha, Teinte);
		}
	}
	else
	{
		for (int32 i = 1; i < 9; ++i)
		{
			const float X = Taille.X * i / 9.f;
			Boite(Sortie, Couche + 3, G, Coin + FVector2D(X, 6.f), FVector2D(1.f, Taille.Y - 12.f), FLinearColor(1.f, 1.f, 1.f, 0.05f));
		}
		for (int32 i = 1; i < 6; ++i)
		{
			const float Y = Taille.Y * i / 6.f;
			Boite(Sortie, Couche + 3, G, Coin + FVector2D(6.f, Y), FVector2D(Taille.X - 12.f, 1.f), FLinearColor(1.f, 1.f, 1.f, 0.05f));
		}
	}
	// voile sombre en haut, pour que la distance reste lisible
	Boite(Sortie, Couche + 4, G, Coin + FVector2D(Arrondi, 0.f), FVector2D(Taille.X - 2.f * Arrondi, 30.f), FLinearColor(0.f, 0.f, 0.f, 0.12f));

	// contour clair
	{
		TArray<FVector2D> Contour = Forme;
		Contour.Add(Forme[0]);
		FSlateDrawElement::MakeLines(Sortie, Couche + 9, G.ToPaintGeometry(), Contour, ESlateDrawEffect::None,
			FLinearColor(1.f, 1.f, 1.f, 0.35f), true, 1.5f);
	}

	// Tout ce qui suit reste dans la carte
	const FVector2D Min = Coin + FVector2D(12.f, 12.f);
	const FVector2D Max = Coin + Taille - FVector2D(12.f, 12.f);
	Sortie.PushClip(FSlateClippingZone(G.LocalToAbsolute(Coin + FVector2D(2.f, 2.f)), G.LocalToAbsolute(Coin + FVector2D(Taille.X - 2.f, 2.f)),
		G.LocalToAbsolute(Coin + FVector2D(2.f, Taille.Y - 2.f)), G.LocalToAbsolute(Coin + Taille - FVector2D(2.f, 2.f))));

	auto VersEcran = [&](const FVector2D& Pos) { return Joueur + Pos * Echelle; };
	const FLinearColor Turquoise(0.05f, 0.88f, 0.8f, 1.f);
	TArray<FVector2D> Tmp;

	// ---- Itineraire cyan ----
	if (Etat.bDestination && Etat.Itineraire.Num() >= 2)
	{
		TArray<FVector2D> Trace;
		Trace.Reserve(Etat.Itineraire.Num());
		for (const FVector2D& P : Etat.Itineraire)
		{
			Trace.Add(VersEcran(P));
		}
		FSlateDrawElement::MakeLines(Sortie, Couche + 5, G.ToPaintGeometry(), Trace, ESlateDrawEffect::None,
			FLinearColor(0.f, 0.25f, 0.25f, 0.55f), true, 9.f);
		FSlateDrawElement::MakeLines(Sortie, Couche + 6, G.ToPaintGeometry(), Trace, ESlateDrawEffect::None, Turquoise, true, 6.f);
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
		FSlateDrawElement::MakeLines(Sortie, Couche + 7, G.ToPaintGeometry(), L, ESlateDrawEffect::None, C, true, Epaisseur);
	};
	const float Pulse = 0.65f + 0.35f * FMath::Abs(FMath::Sin(Etat.Horloge * 4.f));
	const FLinearColor RougeZombie(1.f, 0.12f, 0.1f, 1.f);
	const FLinearColor CyanJoueur(0.2f, 0.95f, 1.f, 1.f);
	for (const FPointCarteChelles& Point : Etat.PointsCarte)
	{
		FVector2D P = VersEcran(Point.Pos);
		const bool bAuBord = RamenerDansCadre(P, Joueur, Min, Max);
		if (bAuBord && !Point.bGarderAuBord)
		{
			continue;
		}
		const float Alpha = bAuBord ? 0.75f : 1.f;
		switch (Point.Type)
		{
		case FPointCarteChelles::Teleporteur:
		{
			// petite pastille sombre arrondie, losange rose dedans
			PointsArrondis(Tmp, P - FVector2D(9.f, 9.f), FVector2D(18.f, 18.f), 5.f);
			Remplir(Sortie, Couche + 6, G, Tmp, FLinearColor(0.05f, 0.08f, 0.14f, 0.8f * Alpha));
			Losange(P, 5.5f, HudRose * FLinearColor(1.f, 1.f, 1.f, Alpha), 2.f);
			Boite(Sortie, Couche + 8, G, P - FVector2D(1.5f, 1.5f), FVector2D(3.f, 3.f), FLinearColor(1.f, 1.f, 1.f, Alpha));
			break;
		}
		case FPointCarteChelles::Joueur:
		{
			PointsCercle(Tmp, P, 5.5f, 14);
			Remplir(Sortie, Couche + 6, G, Tmp, FLinearColor(0.f, 0.f, 0.f, 0.6f * Alpha));
			PointsCercle(Tmp, P, 4.f, 14);
			Remplir(Sortie, Couche + 7, G, Tmp, CyanJoueur * FLinearColor(1.f, 1.f, 1.f, Alpha));
			break;
		}
		default:
		{
			PointsCercle(Tmp, P, 4.5f, 12);
			Remplir(Sortie, Couche + 6, G, Tmp, FLinearColor(0.f, 0.f, 0.f, 0.55f));
			PointsCercle(Tmp, P, 3.2f, 12);
			Remplir(Sortie, Couche + 7, G, Tmp, RougeZombie * FLinearColor(1.f, 1.f, 1.f, Pulse));
			break;
		}
		}
	}

	// ---- Repere de la destination (epingle turquoise), colle au bord si elle est loin ----
	if (Etat.bDestination)
	{
		FVector2D P = VersEcran(Etat.Destination);
		RamenerDansCadre(P, Joueur, Min + FVector2D(0.f, 14.f), Max);
		const FVector2D Tete = P - FVector2D(0.f, 13.f);
		TArray<FVector2D> Pointe;
		Pointe.Add(P);
		Pointe.Add(Tete + FVector2D(-6.f, 3.5f));
		Pointe.Add(Tete + FVector2D(6.f, 3.5f));
		PointsCercle(Tmp, Tete, 9.f, 18);
		Remplir(Sortie, Couche + 7, G, Tmp, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Remplir(Sortie, Couche + 8, G, Pointe, Turquoise);
		PointsCercle(Tmp, Tete, 7.5f, 18);
		Remplir(Sortie, Couche + 8, G, Tmp, Turquoise);
		PointsCercle(Tmp, Tete, 3.f, 12);
		Remplir(Sortie, Couche + 9, G, Tmp, FLinearColor(0.02f, 0.1f, 0.12f, 1.f));
	}

	// ---- N (nord) : rond sombre sur le bord, dans la bonne direction ----
	{
		const FVector2D Nord = Etat.CarteNord;
		if (Nord.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			FVector2D P = Joueur + Nord.GetSafeNormal() * 2000.f;
			RamenerDansCadre(P, Joueur, Coin + FVector2D(12.f, 12.f), Coin + Taille - FVector2D(12.f, 12.f));
			PointsCercle(Tmp, P, 9.f, 18);
			Remplir(Sortie, Couche + 8, G, Tmp, FLinearColor(0.02f, 0.03f, 0.05f, 0.85f));
			Ecrire(Sortie, Couche + 9, G, P, TEXT("N"), PoliceNord, FLinearColor::White, 0.5f);
		}
	}

	// ---- Le joueur : rond noir et fleche blanche, en bas au centre (la carte tourne avec la camera) ----
	{
		PointsCercle(Tmp, Joueur, 12.f, 24);
		Remplir(Sortie, Couche + 8, G, Tmp, FLinearColor(0.02f, 0.02f, 0.03f, 0.9f));
		TArray<FVector2D> Fleche;
		Fleche.Add(Joueur + FVector2D(0.f, 2.5f));  // creux (centre de l'eventail)
		Fleche.Add(Joueur + FVector2D(-5.5f, 6.f));
		Fleche.Add(Joueur + FVector2D(0.f, -7.f));
		Fleche.Add(Joueur + FVector2D(5.5f, 6.f));
		Remplir(Sortie, Couche + 9, G, Fleche, FLinearColor::White);
	}

	Sortie.PopClip();

	// ---- Distance en haut a gauche (comme un GPS), sinon l'echelle ----
	if (Etat.bDestination)
	{
		Ecrire(Sortie, Couche + 10, G, Coin + FVector2D(14.f, 16.f), FString::Printf(TEXT("%.2f km"), Etat.DistanceKm),
			PoliceDistance, FLinearColor::White, 0.f);
		if (!Etat.NomDestination.IsEmpty())
		{
			Ecrire(Sortie, Couche + 10, G, Coin + FVector2D(15.f, 33.f), Etat.NomDestination, PolicePetite,
				FLinearColor(0.6f, 1.f, 0.95f, 0.9f), 0.f);
		}
	}
	else
	{
		Ecrire(Sortie, Couche + 10, G, Coin + FVector2D(14.f, 16.f), FString::Printf(TEXT("%d m"), FMath::RoundToInt(Etat.CarteRayonMetres)),
			PolicePetite, FLinearColor(1.f, 1.f, 1.f, 0.8f), 0.f);
	}

	return Taille.Y;
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
	// Mini-carte facon GPS, tout en bas a gauche
	// =====================================================================
	const float HauteurCarte = DessinerCarte(Sortie, Couche, G, Ecran);

	// =====================================================================
	// Panneau en bas a gauche, juste au-dessus de la mini-carte : VIE, NIVEAU + XP, POUVOIR
	// =====================================================================
	const float Largeur = 360.f;
	const float Marge = 14.f;
	const float HauteurVie = 50.f;
	const float HauteurPouvoirs = Etat.bPouvoirs ? 74.f : 0.f;
	const float Hauteur = HauteurVie + HauteurPouvoirs + 6.f;
	const float SousLePanneau = HauteurCarte > 0.f ? HauteurCarte + 10.f : 0.f;
	const FVector2D Coin(22.f, FMath::RoundToFloat(Ecran.Y - Hauteur - 22.f - SousLePanneau));
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
					if (TousLesPouvoirsOuverts() || Niveau >= Pouvoir(i).Niveau)
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
			const bool bDebloque = TousLesPouvoirsOuverts() || Niveau >= P.Niveau;
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
				// Vol sans limite (ENVOL d'une heure) : pas de compte a rebours
				Etiquette = Etat.Effet[Quel] > 1800.f ? FString(TEXT("EN VOL")) : FString::Printf(TEXT("ACTIF  %s"), *TexteDuree(Etat.Effet[Quel]));
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
	// Panneau en haut a droite : invasion de zombies en cours
	// =====================================================================
	if (Etat.bZombies && Etat.bInvasion)
	{
		const float LargeurZ = 250.f;
		const float HauteurZ = 104.f;
		const FVector2D CoinZ(FMath::RoundToFloat(Ecran.X - LargeurZ - 22.f), 22.f);
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

	return Couche + 11;
}
