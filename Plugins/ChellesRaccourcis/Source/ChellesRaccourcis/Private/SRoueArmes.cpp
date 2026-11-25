// CHELLES 3D - Dessin de la roue des armes (Slate, sans asset), facon GTA, couleurs cyberpunk (neon cyan / magenta)
#include "SRoueArmes.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

// Espace de noms nomme (pas anonyme) : le plugin peut etre compile en "unity build" avec RaccourcisF.cpp.
namespace RoueArmesDessin
{
	const float DeuxPi = 2.f * PI;

	// Couleurs CYBERPUNK (2026-11-21, demande MERIDJA) : fond violet nuit transparent, neon cyan, survol magenta
	const FLinearColor FondCase(0.035f, 0.f, 0.09f, 0.72f);
	const FLinearColor FondSurvol(0.42f, 0.f, 0.36f, 0.86f);
	const FLinearColor FondEnMain(0.f, 0.20f, 0.30f, 0.78f);
	const FLinearColor FondCentre(0.015f, 0.f, 0.05f, 0.88f);
	const FLinearColor BleuClair(0.f, 0.94f, 1.f, 1.f);   // cyan neon
	const FLinearColor Magenta(1.f, 0.08f, 0.85f, 1.f);   // magenta neon
	const FLinearColor Jaune(1.f, 0.92f, 0.f, 1.f);       // jaune neon (munitions)
	const FLinearColor Gris(0.42f, 0.38f, 0.52f, 1.f);

	FLinearColor Alpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

	FVector2D Polaire(const FVector2D& Centre, float Rayon, float Angle)
	{
		return Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rayon;
	}

	void Trait(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const FLinearColor& Couleur, float Epaisseur)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
	}

	void Rond(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre, float Rayon,
		const FLinearColor& Couleur, float Epaisseur, int32 Cotes = 28)
	{
		TArray<FVector2D> Points;
		for (int32 i = 0; i <= Cotes; ++i)
		{
			Points.Add(Polaire(Centre, Rayon, DeuxPi * i / Cotes));
		}
		Trait(Sortie, Couche, G, Points, Couleur, Epaisseur);
	}

	/** Forme fermee a partir de points (-1..1) mis a l'echelle. */
	void Forme(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& C, float S,
		std::initializer_list<FVector2D> Points, const FLinearColor& Couleur, float Epaisseur)
	{
		TArray<FVector2D> P;
		for (const FVector2D& Q : Points)
		{
			P.Add(C + Q * S);
		}
		Trait(Sortie, Couche, G, P, Couleur, Epaisseur);
	}

	bool Est(const FString& Categorie, const TCHAR* Mot)
	{
		return Categorie.Contains(Mot, ESearchCase::IgnoreCase);
	}
}

using namespace RoueArmesDessin;

void SRoueArmes::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SRoueArmes::Secteur(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	float RayonIn, float RayonOut, float AngleDebut, float AngleFin, const FLinearColor& Couleur) const
{
	const FSlateBrush* Blanc = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FSlateResourceHandle Ressource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Blanc);
	const FColor C = Couleur.ToFColor(true);

	const int32 Pas = FMath::Max(2, FMath::CeilToInt((AngleFin - AngleDebut) / (PI / 48.f)));
	TArray<FSlateVertex> Sommets;
	TArray<SlateIndex> Indices;
	Sommets.Reserve((Pas + 1) * 2);
	Indices.Reserve(Pas * 6);

	auto Ajouter = [&](const FVector2D& Local)
	{
		FSlateVertex V;
		FMemory::Memzero(&V, sizeof(FSlateVertex));
		V.TexCoords[2] = 1.f;
		V.TexCoords[3] = 1.f;
		V.Position = G.LocalToAbsolute(Local); // les sommets sont en coordonnees fenetre
		V.Color = C;
		Sommets.Add(V);
	};
	for (int32 i = 0; i <= Pas; ++i)
	{
		const float A = FMath::Lerp(AngleDebut, AngleFin, float(i) / Pas);
		Ajouter(Polaire(Centre, RayonIn, A));
		Ajouter(Polaire(Centre, RayonOut, A));
	}
	for (int32 i = 0; i < Pas; ++i)
	{
		const SlateIndex B = SlateIndex(i * 2);
		Indices.Add(B);
		Indices.Add(B + 1);
		Indices.Add(B + 2);
		Indices.Add(B + 1);
		Indices.Add(B + 3);
		Indices.Add(B + 2);
	}
	FSlateDrawElement::MakeCustomVerts(Sortie, Couche, Ressource, Sommets, Indices, nullptr, 0, 0);
}

void SRoueArmes::Disque(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	float Rayon, const FLinearColor& Couleur) const
{
	Secteur(Sortie, Couche, G, Centre, 0.f, Rayon, 0.f, DeuxPi, Couleur);
}

void SRoueArmes::Texte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	const FText& Contenu, const FSlateFontInfo& Police, const FLinearColor& Couleur) const
{
	if (Contenu.IsEmpty())
	{
		return;
	}
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Taille = Mesure->Measure(Contenu, Police);
	// petite ombre pour rester lisible sur la neige
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Centre - Taille * 0.5f + FVector2D(1.5f, 1.5f), Taille), Contenu, Police,
		ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.7f * Couleur.A));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Centre - Taille * 0.5f, Taille), Contenu, Police,
		ESlateDrawEffect::None, Couleur);
}

void SRoueArmes::Dessin(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& C,
	float S, const FString& Cat, const FLinearColor& Couleur) const
{
	// Petits dessins quand l'arme n'a pas d'image (vue de cote, canon vers la droite)
	const float E = FMath::Max(2.f, S * 0.10f);
	if (Est(Cat, TEXT("Melee")) || Est(Cat, TEXT("Sabre")) || Est(Cat, TEXT("Epee")))
	{
		Trait(Sortie, Couche, G, { C + FVector2D(-0.8f, 0.8f) * S, C + FVector2D(0.85f, -0.85f) * S }, Couleur, E);
		Trait(Sortie, Couche, G, { C + FVector2D(-0.65f, 0.25f) * S, C + FVector2D(-0.25f, 0.65f) * S }, Couleur, E);
	}
	else if (Est(Cat, TEXT("Lancer")) || Est(Cat, TEXT("Grenade")))
	{
		Rond(Sortie, Couche, G, C + FVector2D(0.f, 0.15f) * S, S * 0.62f, Couleur, E);
		Forme(Sortie, Couche, G, C, S, { FVector2D(-0.2f, -0.45f), FVector2D(-0.2f, -0.75f), FVector2D(0.25f, -0.75f), FVector2D(0.25f, -0.45f) }, Couleur, E);
		Trait(Sortie, Couche, G, { C + FVector2D(0.25f, -0.7f) * S, C + FVector2D(0.6f, -0.95f) * S }, Couleur, E * 0.7f);
	}
	else if (Est(Cat, TEXT("Energie")) || Est(Cat, TEXT("Laser")))
	{
		Forme(Sortie, Couche, G, C, S, { FVector2D(0.18f, -1.f), FVector2D(-0.52f, 0.12f), FVector2D(-0.04f, 0.12f),
			FVector2D(-0.22f, 1.f), FVector2D(0.52f, -0.16f), FVector2D(0.04f, -0.16f), FVector2D(0.18f, -1.f) }, Couleur, E);
	}
	else if (Est(Cat, TEXT("Pistolet")))
	{
		Forme(Sortie, Couche, G, C, S, { FVector2D(-0.75f, -0.45f), FVector2D(0.8f, -0.45f), FVector2D(0.8f, -0.1f),
			FVector2D(-0.15f, -0.1f), FVector2D(-0.3f, 0.75f), FVector2D(-0.75f, 0.75f), FVector2D(-0.6f, -0.1f), FVector2D(-0.75f, -0.45f) }, Couleur, E);
	}
	else
	{
		// fusil / pompe / precision / lourde : long canon + crosse
		const float Long = (Est(Cat, TEXT("Precision")) || Est(Cat, TEXT("Lourde"))) ? 1.f : 0.85f;
		Forme(Sortie, Couche, G, C, S, { FVector2D(-1.f, -0.05f), FVector2D(-0.55f, -0.3f), FVector2D(Long, -0.3f),
			FVector2D(Long, -0.08f), FVector2D(0.05f, -0.08f), FVector2D(-0.05f, 0.45f), FVector2D(-0.3f, 0.45f),
			FVector2D(-0.25f, 0.05f), FVector2D(-0.55f, 0.05f), FVector2D(-1.f, 0.35f), FVector2D(-1.f, -0.05f) }, Couleur, E);
		if (Est(Cat, TEXT("Precision")))
		{
			Forme(Sortie, Couche, G, C, S, { FVector2D(-0.3f, -0.55f), FVector2D(0.3f, -0.55f), FVector2D(0.3f, -0.38f), FVector2D(-0.3f, -0.38f), FVector2D(-0.3f, -0.55f) }, Couleur, E * 0.7f);
		}
	}
}

void SRoueArmes::Personnage(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& C,
	float R, float A) const
{
	// Icone du personnage joue (en bas a droite) : silhouette dans un rond de la couleur du mode
	Disque(Sortie, Couche, G, C, R, Alpha(FondCentre, A));
	Secteur(Sortie, Couche + 1, G, C, R - 3.f, R, 0.f, DeuxPi, Alpha(CouleurPerso, A));
	const FLinearColor Trace = Alpha(FLinearColor::White, 0.95f * A);
	Rond(Sortie, Couche + 2, G, C + FVector2D(0.f, -R * 0.28f), R * 0.22f, Trace, 2.5f, 20);
	TArray<FVector2D> Epaules;
	for (int32 i = 0; i <= 16; ++i)
	{
		const float Angle = FMath::Lerp(PI * 1.08f, PI * 1.92f, float(i) / 16.f);
		Epaules.Add(C + FVector2D(0.f, R * 0.62f) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R * 0.52f);
	}
	Trait(Sortie, Couche + 2, G, Epaules, Trace, 2.5f);
	Texte(Sortie, Couche + 2, G, C + FVector2D(0.f, R + 16.f), NomPerso, FCoreStyle::GetDefaultFontStyle("Bold", 12), Alpha(FLinearColor::White, A));
}

int32 SRoueArmes::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D Taille = G.GetLocalSize();
	const FVector2D Centre = Taille * 0.5f;
	const float T = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(Apparition, 0.f, 1.f), 3.f);
	const float R = FMath::Clamp(FMath::Min(Taille.X, Taille.Y) * 0.36f, 170.f, 380.f) * (0.88f + 0.12f * T);
	const float RIn = R * 0.44f;
	const int32 N = Cases.Num();

	// Fond legerement assombri, teinte violette (on voit encore le jeu derriere)
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None, FLinearColor(0.03f, 0.f, 0.06f, 0.38f * T));

	// Anneau de fond violet nuit + halo neon : cyan dehors, magenta dedans
	Secteur(Sortie, Couche + 1, G, Centre, RIn - 6.f, R + 6.f, 0.f, DeuxPi, FLinearColor(0.05f, 0.f, 0.10f, 0.40f * T));
	Secteur(Sortie, Couche + 1, G, Centre, R + 8.f, R + 22.f, 0.f, DeuxPi, Alpha(BleuClair, 0.07f * T));
	Secteur(Sortie, Couche + 1, G, Centre, R + 8.f, R + 14.f, 0.f, DeuxPi, Alpha(BleuClair, 0.14f * T));
	Secteur(Sortie, Couche + 2, G, Centre, R + 6.f, R + 9.f, 0.f, DeuxPi, Alpha(BleuClair, 0.85f * T));
	Secteur(Sortie, Couche + 2, G, Centre, RIn - 9.f, RIn - 6.f, 0.f, DeuxPi, Alpha(Magenta, 0.75f * T));

	const FSlateFontInfo PoliceCase = FCoreStyle::GetDefaultFontStyle("Bold", N > 12 ? 9 : 11);
	const FSlateFontInfo PoliceTitre = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	const FSlateFontInfo PoliceMunitions = FCoreStyle::GetDefaultFontStyle("Bold", 26);
	const FSlateFontInfo PoliceAide = FCoreStyle::GetDefaultFontStyle("Regular", 11);

	const float Part = N > 0 ? DeuxPi / N : DeuxPi;
	const float Ecart = FMath::Min(0.02f, Part * 0.06f);
	for (int32 i = 0; i < N; ++i)
	{
		const FCaseArme& Case = Cases[i];
		const bool bSurvol = (i == Survol);
		const float Milieu = -PI / 2.f + Part * i; // case 0 en haut, puis sens des aiguilles d'une montre
		const float A0 = Milieu - Part * 0.5f + Ecart;
		const float A1 = Milieu + Part * 0.5f - Ecart;
		const float ROut = bSurvol ? R + 10.f : R;

		FLinearColor Fond = bSurvol ? FondSurvol : FondCase;
		if (Case.bEnMain && !bSurvol)
		{
			Fond = FondEnMain;
		}
		Fond.A *= T;
		Secteur(Sortie, Couche + 3, G, Centre, RIn, ROut, A0, A1, Fond);

		// Bande exterieure : couleur de l'arme (plus epaisse si en main ou visee)
		const FLinearColor Teinte = bSurvol ? Magenta : (Case.bPossedee ? Case.Couleur : Gris);
		const float Bande = bSurvol ? 7.f : (Case.bEnMain ? 6.f : 3.f);
		Secteur(Sortie, Couche + 4, G, Centre, ROut - Bande, ROut, A0, A1, Alpha(Teinte, (Case.bPossedee ? 0.95f : 0.45f) * T));
		if (Case.bEnMain)
		{
			// Arme en main : lisere interieur cyan neon
			Secteur(Sortie, Couche + 4, G, Centre, RIn, RIn + 4.f, A0, A1, Alpha(BleuClair, 0.95f * T));
		}

		// Image ou dessin
		const FVector2D PosIcone = Polaire(Centre, FMath::Lerp(RIn, ROut, 0.50f), Milieu) - FVector2D(0.f, 8.f);
		const float Largeur = FMath::Min((R - RIn) * 0.56f, FMath::Lerp(RIn, R, 0.5f) * Part * 0.78f);
		const float Opacite = (Case.bPossedee ? 1.f : 0.38f) * T;
		if (Case.Image.IsValid() && Case.Image->GetResourceObject())
		{
			const FVector2D Source = Case.Image->ImageSize.X > 1.f ? Case.Image->ImageSize : FVector2D(128.f, 128.f);
			const float Echelle = FMath::Min(Largeur / Source.X, (Largeur * 0.62f) / Source.Y);
			const FVector2D Boite = Source * Echelle * (bSurvol ? 1.12f : 1.f);
			FSlateDrawElement::MakeBox(Sortie, Couche + 5, G.ToPaintGeometry(PosIcone - Boite * 0.5f, Boite), Case.Image.Get(),
				ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Opacite));
		}
		else
		{
			Dessin(Sortie, Couche + 5, G, PosIcone, Largeur * 0.36f * (bSurvol ? 1.1f : 1.f), Case.Categorie,
				Alpha(bSurvol ? FLinearColor::White : (Case.bPossedee ? BleuClair : Gris), Opacite));
		}
		Texte(Sortie, Couche + 5, G, PosIcone + FVector2D(0.f, Largeur * 0.36f + 12.f), Case.Nom, PoliceCase,
			Alpha(Case.bPossedee ? FLinearColor::White : Gris, (Case.bPossedee ? 1.f : 0.7f) * T));
	}

	// Disque central : nom + munitions
	Disque(Sortie, Couche + 3, G, Centre, RIn - 10.f, Alpha(FondCentre, T));
	Secteur(Sortie, Couche + 4, G, Centre, RIn - 13.f, RIn - 10.f, 0.f, DeuxPi, Alpha(BleuClair, 0.80f * T));

	// Aiguille vers la case visee
	if (Curseur.SizeSquared() > 0.0025f)
	{
		const float Angle = FMath::Atan2(Curseur.Y, Curseur.X);
		Secteur(Sortie, Couche + 5, G, Centre, RIn - 20.f, RIn - 15.f, Angle - 0.2f, Angle + 0.2f,
			Alpha(Magenta, (0.35f + 0.6f * FMath::Clamp(Curseur.Size(), 0.f, 1.f)) * T));
	}

	Texte(Sortie, Couche + 6, G, Centre - FVector2D(0.f, 26.f), Titre, PoliceTitre, Alpha(BleuClair, T));
	Texte(Sortie, Couche + 6, G, Centre + FVector2D(0.f, 8.f), Munitions, PoliceMunitions, Alpha(Jaune, T));
	Texte(Sortie, Couche + 6, G, Centre + FVector2D(0.f, 40.f), Aide, PoliceAide, Alpha(FLinearColor(0.95f, 0.6f, 1.f), T));
	Texte(Sortie, Couche + 6, G, Centre + FVector2D(0.f, 58.f), FText::FromString(TEXT("clic droit : annuler")),
		FCoreStyle::GetDefaultFontStyle("Regular", 9), Alpha(FLinearColor(0.5f, 0.55f, 0.6f), T));

	// Personnage joue, en bas a droite de l'ecran
	const float RPerso = FMath::Clamp(Taille.Y * 0.055f, 34.f, 60.f);
	Personnage(Sortie, Couche + 3, G, FVector2D(Taille.X - RPerso - 40.f, Taille.Y - RPerso - 70.f), RPerso, T);

	return Couche + 8;
}
