// CHELLES 3D - Dessin de la roue des modes (Slate, sans asset)
#include "SRoueModes.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	const float Pi2 = 2.f * PI;

	FLinearColor AvecAlpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

	FVector2D Polaire(const FVector2D& Centre, float Rayon, float Angle)
	{
		return Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rayon;
	}

	void Lignes(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const FLinearColor& Couleur, float Epaisseur)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
	}

	void Cercle(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre, float Rayon,
		const FLinearColor& Couleur, float Epaisseur, int32 Cotes = 32)
	{
		TArray<FVector2D> Points;
		for (int32 i = 0; i <= Cotes; ++i)
		{
			Points.Add(Polaire(Centre, Rayon, Pi2 * i / Cotes));
		}
		Lignes(Sortie, Couche, G, Points, Couleur, Epaisseur);
	}
}

void SRoueModes::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

float SRoueModes::RayonPour(const FVector2D& Taille)
{
	return FMath::Clamp(FMath::Min(Taille.X, Taille.Y) * 0.30f, 150.f, 340.f);
}

void SRoueModes::Secteur(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
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

void SRoueModes::Disque(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	float Rayon, const FLinearColor& Couleur) const
{
	Secteur(Sortie, Couche, G, Centre, 0.f, Rayon, 0.f, Pi2, Couleur);
}

void SRoueModes::Texte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre,
	const FText& Contenu, const FSlateFontInfo& Police, const FLinearColor& Couleur) const
{
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Taille = Mesure->Measure(Contenu, Police);
	FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Centre - Taille * 0.5f, Taille), Contenu, Police,
		ESlateDrawEffect::None, Couleur);
}

void SRoueModes::Icone(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& C,
	float S, EIconeRoue Type, const FLinearColor& Couleur) const
{
	const float E = FMath::Max(2.f, S * 0.09f); // epaisseur du trait
	switch (Type)
	{
	case EIconeRoue::Viseur:
	{
		Cercle(Sortie, Couche, G, C, S * 0.62f, Couleur, E);
		const FVector2D Axes[] = { FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f) };
		for (const FVector2D& D : Axes)
		{
			Lignes(Sortie, Couche, G, { C + D * S * 0.3f, C + D * S * 1.0f }, Couleur, E);
		}
		Cercle(Sortie, Couche, G, C, S * 0.07f, Couleur, E, 8);
		break;
	}
	case EIconeRoue::Toile:
	{
		for (int32 i = 0; i < 8; ++i)
		{
			Lignes(Sortie, Couche, G, { C, Polaire(C, S, Pi2 * i / 8.f) }, Couleur, E * 0.8f);
		}
		for (float R : { 0.36f, 0.68f, 1.0f })
		{
			TArray<FVector2D> Anneau;
			for (int32 i = 0; i <= 8; ++i)
			{
				// legerement creuse entre deux fils, comme une vraie toile
				const float A0 = Pi2 * i / 8.f;
				Anneau.Add(Polaire(C, S * R, A0));
				if (i < 8)
				{
					Anneau.Add(Polaire(C, S * R * 0.86f, A0 + Pi2 / 16.f));
				}
			}
			Lignes(Sortie, Couche, G, Anneau, Couleur, E * 0.7f);
		}
		break;
	}
	case EIconeRoue::Flocon:
	{
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = -PI / 2.f + Pi2 * i / 6.f;
			const FVector2D Bout = Polaire(C, S, A);
			Lignes(Sortie, Couche, G, { C, Bout }, Couleur, E);
			const FVector2D Branche = Polaire(C, S * 0.58f, A);
			Lignes(Sortie, Couche, G, { Polaire(Branche, S * 0.3f, A - 0.9f), Branche, Polaire(Branche, S * 0.3f, A + 0.9f) }, Couleur, E * 0.8f);
		}
		break;
	}
	case EIconeRoue::Voiture:
	{
		const FVector2D P[] = {
			FVector2D(-1.f, 0.30f), FVector2D(-1.f, -0.02f), FVector2D(-0.62f, -0.12f), FVector2D(-0.36f, -0.48f),
			FVector2D(0.32f, -0.48f), FVector2D(0.6f, -0.12f), FVector2D(1.f, -0.04f), FVector2D(1.f, 0.30f), FVector2D(-1.f, 0.30f)
		};
		TArray<FVector2D> Carrosserie;
		for (const FVector2D& Q : P)
		{
			Carrosserie.Add(C + Q * S);
		}
		Lignes(Sortie, Couche, G, Carrosserie, Couleur, E);
		Lignes(Sortie, Couche, G, { C + FVector2D(-0.02f, -0.46f) * S, C + FVector2D(-0.02f, -0.12f) * S }, Couleur, E * 0.7f);
		Cercle(Sortie, Couche, G, C + FVector2D(-0.55f, 0.34f) * S, S * 0.2f, Couleur, E, 14);
		Cercle(Sortie, Couche, G, C + FVector2D(0.55f, 0.34f) * S, S * 0.2f, Couleur, E, 14);
		break;
	}
	case EIconeRoue::FerACheval:
	{
		// Fer a cheval : un U ouvert vers le bas, avec les trous des clous
		TArray<FVector2D> Fer;
		const int32 Cotes = 20;
		for (int32 i = 0; i <= Cotes; ++i)
		{
			const float A = FMath::Lerp(PI * 0.82f, PI * 2.18f, float(i) / Cotes); // de bas-gauche a bas-droite par le haut
			Fer.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * S * 0.85f);
		}
		Lignes(Sortie, Couche, G, Fer, Couleur, E * 1.6f);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = FMath::Lerp(PI * 0.95f, PI * 2.05f, float(i) / 5.f);
			Cercle(Sortie, Couche, G, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * S * 0.85f, S * 0.05f, FLinearColor(0.02f, 0.02f, 0.03f, Couleur.A), E * 0.6f, 6);
		}
		break;
	}
	case EIconeRoue::Eclair:
	{
		// Eclair en zigzag, ferme
		const FVector2D P[] = {
			FVector2D(0.18f, -1.f), FVector2D(-0.52f, 0.12f), FVector2D(-0.04f, 0.12f),
			FVector2D(-0.22f, 1.f), FVector2D(0.52f, -0.16f), FVector2D(0.04f, -0.16f), FVector2D(0.18f, -1.f)
		};
		TArray<FVector2D> Zigzag;
		for (const FVector2D& Q : P)
		{
			Zigzag.Add(C + Q * S);
		}
		Lignes(Sortie, Couche, G, Zigzag, Couleur, E);
		break;
	}
	case EIconeRoue::Flamme:
	{
		// Flamme : une goutte pointue vers le haut, avec une petite flamme dedans
		TArray<FVector2D> Exterieur;
		TArray<FVector2D> Interieur;
		const int32 Pas = 24;
		for (int32 i = 0; i <= Pas; ++i)
		{
			const float T = Pi2 * i / Pas; // 0 = pointe en haut
			const float X = FMath::Sin(T) * FMath::Sin(T * 0.5f);
			const float Y = -FMath::Cos(T);
			Exterieur.Add(C + FVector2D(X * 0.8f, Y * 0.95f) * S);
			Interieur.Add(C + FVector2D(X * 0.38f, Y * 0.42f + 0.5f) * S);
		}
		Lignes(Sortie, Couche, G, Exterieur, Couleur, E);
		Lignes(Sortie, Couche, G, Interieur, AvecAlpha(Couleur, 0.8f), E * 0.7f);
		break;
	}
	case EIconeRoue::Epees:
	{
		// Deux epees croisees : lame, garde, pommeau
		for (float Sens : { 1.f, -1.f })
		{
			const FVector2D Pointe = C + FVector2D(0.85f * Sens, -0.85f) * S;
			const FVector2D Garde = C + FVector2D(-0.42f * Sens, 0.42f) * S;
			const FVector2D Bout = C + FVector2D(-0.8f * Sens, 0.8f) * S;
			Lignes(Sortie, Couche, G, { Pointe, Garde }, Couleur, E);
			Lignes(Sortie, Couche, G, { Garde, Bout }, Couleur, E * 1.4f);
			const FVector2D Travers = FVector2D(0.22f, 0.22f * Sens) * S;
			Lignes(Sortie, Couche, G, { Garde - Travers, Garde + Travers }, Couleur, E);
		}
		break;
	}
	case EIconeRoue::Griffes:
	{
		// PANTERA : trois coups de griffe en biais (traits courbes), et deux yeux en amande au-dessus
		for (int32 i = -1; i <= 1; ++i)
		{
			const FVector2D Depart = C + FVector2D(-0.55f + 0.42f * i, -0.35f) * S;
			TArray<FVector2D> Griffe;
			for (int32 k = 0; k <= 6; ++k)
			{
				const float t = k / 6.f;
				Griffe.Add(Depart + FVector2D(0.7f * t + 0.12f * FMath::Sin(t * PI), 1.05f * t) * S);
			}
			Lignes(Sortie, Couche, G, Griffe, Couleur, E * (i == 0 ? 1.2f : 0.95f));
		}
		for (float Sens : { -1.f, 1.f })
		{
			const FVector2D Oeil = C + FVector2D(0.42f * Sens, -0.78f) * S;
			const FVector2D Coin = FVector2D(0.2f * Sens, -0.07f) * S;
			Lignes(Sortie, Couche, G, { Oeil - Coin, Oeil + FVector2D(0.f, -0.07f) * S, Oeil + Coin, Oeil + FVector2D(0.f, 0.06f) * S, Oeil - Coin }, FLinearColor(1.f, 1.f, 1.f, Couleur.A), E * 0.7f);
		}
		break;
	}
	case EIconeRoue::Question:
	{
		const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(12, FMath::RoundToInt(S * 1.7f)));
		Texte(Sortie, Couche, G, C, FText::FromString(TEXT("?")), Police, Couleur);
		break;
	}
	case EIconeRoue::Plus:
	{
		Lignes(Sortie, Couche, G, { C - FVector2D(S * 0.6f, 0.f), C + FVector2D(S * 0.6f, 0.f) }, Couleur, E);
		Lignes(Sortie, Couche, G, { C - FVector2D(0.f, S * 0.6f), C + FVector2D(0.f, S * 0.6f) }, Couleur, E);
		Cercle(Sortie, Couche, G, C, S * 0.95f, AvecAlpha(Couleur, 0.6f), E * 0.6f);
		break;
	}
	default:
	{
		TArray<FVector2D> Etoile;
		for (int32 i = 0; i <= 10; ++i)
		{
			Etoile.Add(Polaire(C, (i % 2 == 0) ? S : S * 0.42f, -PI / 2.f + Pi2 * i / 10.f));
		}
		Lignes(Sortie, Couche, G, Etoile, Couleur, E);
		break;
	}
	}
}

int32 SRoueModes::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 N = Cases.Num();
	if (N == 0)
	{
		return Couche;
	}

	const FVector2D Taille = G.GetLocalSize();
	const FVector2D Centre = Taille * 0.5f;
	const float T = FMath::InterpEaseOut(0.f, 1.f, FMath::Clamp(Apparition, 0.f, 1.f), 3.f);
	const float R = RayonPour(Taille) * (0.86f + 0.14f * T);
	const float RIn = R * 0.42f;
	const float Part = Pi2 / N;
	const float Ecart = FMath::Min(0.035f, Part * 0.08f);

	// Fond assombri
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None, FLinearColor(0.f, 0.01f, 0.03f, 0.45f * T));

	// Halo exterieur
	Secteur(Sortie, Couche + 1, G, Centre, R + 10.f, R + 13.f, 0.f, Pi2, FLinearColor(1.f, 1.f, 1.f, 0.10f * T));

	const FSlateFontInfo PoliceCase = FCoreStyle::GetDefaultFontStyle("Bold", 15);
	const FSlateFontInfo PolicePetite = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	const FSlateFontInfo PoliceCentre = FCoreStyle::GetDefaultFontStyle("Bold", 22);
	const FSlateFontInfo PoliceAide = FCoreStyle::GetDefaultFontStyle("Regular", 11);

	for (int32 i = 0; i < N; ++i)
	{
		const FCaseRoue& Case = Cases[i];
		const bool bSurvol = (i == Survol);
		const float Milieu = -PI / 2.f + Part * i;
		const float A0 = Milieu - Part * 0.5f + Ecart;
		const float A1 = Milieu + Part * 0.5f - Ecart;
		const float ROut = bSurvol ? R + 12.f : R;

		const FLinearColor Teinte = (Case.bDisponible || Case.bLibre) ? Case.Couleur : FLinearColor(0.35f, 0.35f, 0.38f);
		FLinearColor Fond = bSurvol
			? FLinearColor::LerpUsingHSV(FLinearColor(0.05f, 0.06f, 0.08f), Teinte, 0.55f)
			: FLinearColor(0.035f, 0.04f, 0.055f);
		Fond.A = (bSurvol ? 0.92f : 0.78f) * T;

		Secteur(Sortie, Couche + 2, G, Centre, RIn, ROut, A0, A1, Fond);
		// Bande de couleur sur le bord exterieur
		Secteur(Sortie, Couche + 3, G, Centre, ROut - (bSurvol ? 8.f : 5.f), ROut, A0, A1, AvecAlpha(Teinte, ((Case.bDisponible || Case.bLibre) ? 1.f : 0.5f) * T));

		const float RIcone = FMath::Lerp(RIn, ROut, 0.47f);
		const FVector2D PosIcone = Polaire(Centre, RIcone, Milieu) - FVector2D(0.f, 10.f);
		const float TailleIcone = (R - RIn) * (bSurvol ? 0.24f : 0.21f);
		const FLinearColor CouleurTrait = (bSurvol && !Case.bLibre) ? FLinearColor::White : AvecAlpha(Teinte, (Case.bDisponible || Case.bLibre) ? 1.f : 0.6f);
		Icone(Sortie, Couche + 4, G, PosIcone, TailleIcone, Case.Icone, AvecAlpha(CouleurTrait, T));

		Texte(Sortie, Couche + 4, G, PosIcone + FVector2D(0.f, TailleIcone + 16.f), Case.Nom, PoliceCase,
			AvecAlpha(Case.bDisponible ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.62f), T));

		if (i == Actuel)
		{
			// Petit repere " actuel " pres du centre
			const FVector2D PosRepere = Polaire(Centre, RIn + 16.f, Milieu);
			Disque(Sortie, Couche + 4, G, PosRepere, 4.5f, AvecAlpha(Teinte, T));
		}
	}

	// Disque central
	const bool bChoix = Cases.IsValidIndex(Survol);
	const FLinearColor CouleurChoix = bChoix ? ((Cases[Survol].bDisponible || Cases[Survol].bLibre) ? Cases[Survol].Couleur : FLinearColor(0.5f, 0.5f, 0.52f)) : FLinearColor(0.7f, 0.75f, 0.8f);
	Disque(Sortie, Couche + 2, G, Centre, RIn - 10.f, FLinearColor(0.015f, 0.02f, 0.03f, 0.92f * T));
	Secteur(Sortie, Couche + 3, G, Centre, RIn - 13.f, RIn - 10.f, 0.f, Pi2, AvecAlpha(CouleurChoix, 0.9f * T));

	// Aiguille vers la case visee
	if (Curseur.SizeSquared() > 0.0025f)
	{
		const float Angle = FMath::Atan2(Curseur.Y, Curseur.X);
		const float Longueur = FMath::Clamp(Curseur.Size(), 0.f, 1.f);
		const FVector2D Pointe = Polaire(Centre, RIn - 22.f, Angle);
		Secteur(Sortie, Couche + 4, G, Centre, RIn - 20.f, RIn - 15.f, Angle - 0.22f, Angle + 0.22f, AvecAlpha(CouleurChoix, (0.4f + 0.6f * Longueur) * T));
		Disque(Sortie, Couche + 4, G, Pointe, 3.f, AvecAlpha(CouleurChoix, T));
	}

	const FText Titre = bChoix ? Cases[Survol].Nom : FText::FromString(TEXT("Mode de jeu"));
	Texte(Sortie, Couche + 5, G, Centre - FVector2D(0.f, 12.f), Titre, PoliceCentre, AvecAlpha(FLinearColor::White, T));

	FText Aide;
	if (bChoix && Cases[Survol].bLibre)
	{
		Aide = FText::FromString(TEXT("Case libre : bient\u00F4t une fonction"));
	}
	else if (bChoix && !Cases[Survol].bDisponible)
	{
		Aide = FText::FromString(TEXT("Indisponible"));
	}
	else if (bChoix && Survol == Actuel)
	{
		Aide = FText::FromString(TEXT("Déjà actif"));
	}
	else if (bChoix)
	{
		Aide = FText::FromString(TEXT("Relâche Tab pour choisir"));
	}
	else
	{
		Aide = FText::FromString(TEXT("Bouge la souris vers un mode"));
	}
	Texte(Sortie, Couche + 5, G, Centre + FVector2D(0.f, 20.f), Aide, PoliceAide, AvecAlpha(FLinearColor(0.75f, 0.8f, 0.85f), T));
	Texte(Sortie, Couche + 5, G, Centre + FVector2D(0.f, 40.f), FText::FromString(TEXT("clic droit : annuler")), PolicePetite,
		AvecAlpha(FLinearColor(0.5f, 0.55f, 0.6f), T));

	return Couche + 6;
}
