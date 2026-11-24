// CHELLES 3D - Dessin du menu ARMEMENTS (Slate, sans asset), style "lobby" bleu. Voir MenuArmements.h
#include "SMenuArmements.h"

#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Espace de noms nomme (pas anonyme) : le plugin peut etre compile en "unity build" avec les autres .cpp.
namespace MenuArmementsDessin
{
	// Couleurs du menu (bleu "lobby")
	const FLinearColor MABleuNuit(0.020f, 0.035f, 0.200f, 0.94f);   // panneaux sombres
	const FLinearColor MABleuBarre(0.045f, 0.060f, 0.330f, 1.f);    // barre d'onglets
	const FLinearColor MABleuBouton(0.260f, 0.310f, 0.800f, 1.f);   // boutons
	const FLinearColor MABleuSurvol(0.420f, 0.470f, 0.950f, 1.f);
	const FLinearColor MALavande(0.700f, 0.740f, 1.000f, 1.f);      // categorie choisie, tri
	const FLinearColor MABleuPied(0.170f, 0.230f, 0.650f, 1.f);     // pieds de panneau
	const FLinearColor MATexteSombre(0.020f, 0.030f, 0.160f, 1.f);
	const FLinearColor MABleuOnglet(0.300f, 0.560f, 1.000f, 1.f);
	const FLinearColor MAJaune(1.f, 0.900f, 0.050f, 1.f);
	const FLinearColor MATransparent(0.f, 0.f, 0.f, 0.f);

	const FSlateBrush* MABlanc()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	FSlateFontInfo MAPolice(int32 Taille, bool bGras = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bGras ? TEXT("Bold") : TEXT("Regular"), Taille);
	}

	/** MAPenche le texte comme une ecriture "sport" (italique). */
	FSlateRenderTransform MAPenche(float Force = 0.2f)
	{
		return FSlateRenderTransform(FShear2D(-Force, 0.f));
	}

	FLinearColor MAPlus(const FLinearColor& C, float Facteur)
	{
		return FLinearColor(FMath::Min(1.f, C.R * Facteur), FMath::Min(1.f, C.G * Facteur), FMath::Min(1.f, C.B * Facteur), C.A);
	}

	FLinearColor MAAlpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}

	void MARect(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& C)
	{
		FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), MABlanc(), ESlateDrawEffect::None, C);
	}

	void MACadre(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, float E, const FLinearColor& C)
	{
		MARect(Sortie, Couche, G, Pos, FVector2D(Taille.X, E), C);
		MARect(Sortie, Couche, G, Pos + FVector2D(0.f, Taille.Y - E), FVector2D(Taille.X, E), C);
		MARect(Sortie, Couche, G, Pos + FVector2D(0.f, E), FVector2D(E, Taille.Y - 2.f * E), C);
		MARect(Sortie, Couche, G, Pos + FVector2D(Taille.X - E, E), FVector2D(E, Taille.Y - 2.f * E), C);
	}

	/** Triangles colores (un sommet = une couleur) : degrades et formes pleines. */
	void MASommets(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const TArray<FLinearColor>& Couleurs, const TArray<SlateIndex>& Indices)
	{
		if (!FSlateApplication::IsInitialized() || Points.Num() < 3)
		{
			return;
		}
		const FSlateResourceHandle Ressource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*MABlanc());
		TArray<FSlateVertex> Liste;
		Liste.Reserve(Points.Num());
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			FSlateVertex V;
			FMemory::Memzero(&V, sizeof(FSlateVertex));
			V.TexCoords[2] = 1.f;
			V.TexCoords[3] = 1.f;
			V.Position = G.LocalToAbsolute(Points[i]); // les sommets sont en coordonnees fenetre
			V.Color = Couleurs[FMath::Min(i, Couleurs.Num() - 1)].ToFColor(true);
			Liste.Add(V);
		}
		TArray<SlateIndex> Copie = Indices;
		FSlateDrawElement::MakeCustomVerts(Sortie, Couche, Ressource, Liste, Copie, nullptr, 0, 0);
	}

	void MADegrade(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille,
		const FLinearColor& Haut, const FLinearColor& Bas)
	{
		MASommets(Sortie, Couche, G,
			{ Pos, Pos + FVector2D(Taille.X, 0.f), Pos + Taille, Pos + FVector2D(0.f, Taille.Y) },
			{ Haut, Haut, Bas, Bas }, { 0, 1, 2, 0, 2, 3 });
	}

	/** MAEtoile pleine a 5 branches. */
	void MAEtoile(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre, float Rayon, const FLinearColor& C)
	{
		TArray<FVector2D> P;
		TArray<SlateIndex> I;
		P.Add(Centre);
		for (int32 k = 0; k < 10; ++k)
		{
			const float A = -PI * 0.5f + k * PI / 5.f;
			const float R = (k % 2 == 0) ? Rayon : Rayon * 0.45f;
			P.Add(Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		for (int32 k = 0; k < 10; ++k)
		{
			I.Add(0);
			I.Add(SlateIndex(1 + k));
			I.Add(SlateIndex(1 + (k + 1) % 10));
		}
		MASommets(Sortie, Couche, G, P, { C }, I);
	}

	void MADisque(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Centre, float Rayon, const FLinearColor& C)
	{
		TArray<FVector2D> P;
		TArray<SlateIndex> I;
		P.Add(Centre);
		const int32 N = 24;
		for (int32 k = 0; k < N; ++k)
		{
			const float A = k * 2.f * PI / N;
			P.Add(Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rayon);
		}
		for (int32 k = 0; k < N; ++k)
		{
			I.Add(0);
			I.Add(SlateIndex(1 + k));
			I.Add(SlateIndex(1 + (k + 1) % N));
		}
		MASommets(Sortie, Couche, G, P, { C }, I);
	}

	void MATrait(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points, const FLinearColor& C, float E)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, C, true, E);
	}

	FVector2D MAMesure(const FString& Texte, const FSlateFontInfo& MAPolice)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(10.f, 10.f);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Texte, MAPolice);
	}

	void MAEcrire(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FString& Texte,
		const FSlateFontInfo& MAPolice, const FLinearColor& C, bool bOmbre = true)
	{
		const FVector2D T = MAMesure(Texte, MAPolice);
		if (bOmbre)
		{
			FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Pos + FVector2D(1.5f, 1.5f), T), Texte, MAPolice,
				ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.05f, 0.75f * C.A));
		}
		FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Pos, T), Texte, MAPolice, ESlateDrawEffect::None, C);
	}

	/** Petite touche du clavier (Z, CTRL, ECHAP...) dessinee comme une touche. */
	TSharedRef<SWidget> MAToucheClavier(const FString& Texte, bool bClaire = false)
	{
		return SNew(SBorder)
			.BorderImage(MABlanc())
			.BorderBackgroundColor(bClaire ? FLinearColor(0.95f, 0.95f, 1.f) : MATexteSombre)
			.Padding(FMargin(6.f, 1.f))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Texte))
				.Font(MAPolice(12))
				.ColorAndOpacity(bClaire ? MATexteSombre : FLinearColor::White)
			];
	}

	/** MACategories de gauche : libelle et famille (# = categorie speciale). */
	struct MAFCategorie
	{
		const TCHAR* Libelle;
		const TCHAR* Cle;
	};
	const MAFCategorie MACategories[] = {
		{ TEXT("Tout"), TEXT("") },
		{ TEXT("Coffre"), TEXT("#Coffre") },
		{ TEXT("Pistolets"), TEXT("Pistolet") },
		{ TEXT("Fusils"), TEXT("Fusil") },
		{ TEXT("Pompes"), TEXT("Pompe") },
		{ TEXT("Précision"), TEXT("Precision") },
		{ TEXT("Lourdes"), TEXT("Lourde") },
		{ TEXT("Corps à corps"), TEXT("Melee") },
		{ TEXT("Énergie"), TEXT("Energie") },
		{ TEXT("Lancer"), TEXT("Lancer") },
		{ TEXT("Arsenal CHELLES"), TEXT("#Arsenal") },
		{ TEXT("Armes des packs"), TEXT("#Packs") },
	};
	const int32 MANbCategories = UE_ARRAY_COUNT(MACategories);

	/** Filtres de droite. 0 = tout afficher (aucun autre coche). */
	const TCHAR* const LibellesFiltres[] = {
		TEXT("Tout afficher"),
		TEXT("Ramassées"),
		TEXT("Pas encore trouvées"),
		TEXT("Légendaires"),
		TEXT("Épiques"),
		TEXT("Rares"),
		TEXT("Peu communes"),
		TEXT("Communes"),
	};
	const int32 MANbFiltres = UE_ARRAY_COUNT(LibellesFiltres);

	const TCHAR* const MATris[] = { TEXT("PAR DÉFAUT"), TEXT("RARETÉ"), TEXT("NOM"), TEXT("RAMASSÉES") };
	const int32 MANbTris = UE_ARRAY_COUNT(MATris);

	FString MANomFamille(const FString& Cle)
	{
		for (const MAFCategorie& C : MACategories)
		{
			if (Cle.Equals(C.Cle, ESearchCase::IgnoreCase))
			{
				return C.Libelle;
			}
		}
		return Cle.IsEmpty() ? FString(TEXT("Arme")) : Cle;
	}
}

using namespace MenuArmementsDessin;

FLinearColor MenuArmementsStyle::CouleurRarete(int32 Rarete)
{
	switch (FMath::Clamp(Rarete, 1, 5))
	{
	case 5: return FLinearColor(0.95f, 0.42f, 0.08f);  // legendaire : orange
	case 4: return FLinearColor(0.62f, 0.16f, 0.88f);  // epique : violet
	case 3: return FLinearColor(0.05f, 0.52f, 0.95f);  // rare : bleu
	case 2: return FLinearColor(0.30f, 0.68f, 0.08f);  // peu commune : vert
	default: return FLinearColor(0.52f, 0.55f, 0.60f); // commune : gris
	}
}

FText MenuArmementsStyle::NomRarete(int32 Rarete)
{
	switch (FMath::Clamp(Rarete, 1, 5))
	{
	case 5: return FText::FromString(TEXT("LÉGENDAIRE"));
	case 4: return FText::FromString(TEXT("ÉPIQUE"));
	case 3: return FText::FromString(TEXT("RARE"));
	case 2: return FText::FromString(TEXT("PEU COMMUNE"));
	default: return FText::FromString(TEXT("COMMUNE"));
	}
}

// =====================================================================================
//  Fond
// =====================================================================================

int32 SFondArmements::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D L = G.GetLocalSize();
	MADegrade(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(0.035f, 0.085f, 0.40f, 1.f), FLinearColor(0.09f, 0.26f, 0.72f, 1.f));
	// Grands rayons de lumiere en biais (comme un decor de scene)
	for (int32 k = 0; k < 4; ++k)
	{
		const float X = L.X * (0.30f + 0.17f * k);
		const float Largeur = L.X * (0.05f + 0.02f * (k % 2));
		const FLinearColor C(1.f, 1.f, 1.f, 0.035f);
		const FLinearColor Rien(1.f, 1.f, 1.f, 0.f);
		MASommets(Sortie, Couche + 1, G,
			{ FVector2D(X, 0.f), FVector2D(X + Largeur, 0.f), FVector2D(X + Largeur - L.Y * 0.35f, L.Y), FVector2D(X - L.Y * 0.35f, L.Y) },
			{ C, C, Rien, Rien }, { 0, 1, 2, 0, 2, 3 });
	}
	// Vignette sombre en bas
	MADegrade(Sortie, Couche + 1, G, FVector2D(0.f, L.Y * 0.8f), FVector2D(L.X, L.Y * 0.2f), FLinearColor(0.f, 0.f, 0.08f, 0.f), FLinearColor(0.f, 0.f, 0.08f, 0.35f));
	return Couche + 2;
}

// =====================================================================================
//  Carte d'arme
// =====================================================================================

void SCarteArme::Construct(const FArguments& InArgs)
{
	Case = InArgs._Case;
	Taille = InArgs._Taille;
	bCoffre = InArgs._DansLeCoffre;
	bVide = InArgs._Vide;
	Choisie = InArgs._Choisie;
	Clic = InArgs._OnClic;
	DoubleClic = InArgs._OnDoubleClic;
	Dessinateur = SNew(SRoueArmes);
	SetClipping(EWidgetClipping::ClipToBounds);
	if (!bVide)
	{
		SetCursor(EMouseCursor::Hand);
	}
}

FReply SCarteArme::OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	if (!bVide && Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Clic.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SCarteArme::OnMouseButtonDoubleClick(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	if (!bVide && Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		DoubleClic.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

int32 SCarteArme::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D L = G.GetLocalSize();
	const float S = FMath::Min(L.X, L.Y);
	const float M = FMath::Max(4.f, S * 0.04f);

	if (bVide)
	{
		// Case d'inventaire libre : simple cadre
		MARect(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(0.03f, 0.06f, 0.30f, 0.45f));
		MACadre(Sortie, Couche + 1, G, FVector2D::ZeroVector, L, 3.f, FLinearColor(0.22f, 0.30f, 0.80f, 0.8f));
		return Couche + 2;
	}

	const bool bSurvol = IsHovered();
	const bool bChoisie = Choisie.Get(false);
	const FLinearColor R = MenuArmementsStyle::CouleurRarete(Case.Rarete);

	// MACadre bleu nuit, puis fond degrade couleur de rarete
	MARect(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(0.05f, 0.09f, 0.34f, 1.f));
	const FVector2D Pos(M, M);
	const FVector2D Int = L - FVector2D(2.f * M, 2.f * M);
	MADegrade(Sortie, Couche + 1, G, Pos, Int, MAPlus(R, bSurvol ? 1.45f : 1.25f), MAPlus(R, bSurvol ? 0.85f : 0.65f));
	// Reflet en biais
	MASommets(Sortie, Couche + 2, G,
		{ Pos, Pos + FVector2D(Int.X * 0.55f, 0.f), Pos + FVector2D(0.f, Int.Y * 0.55f) },
		{ FLinearColor(1.f, 1.f, 1.f, 0.16f), FLinearColor(1.f, 1.f, 1.f, 0.f), FLinearColor(1.f, 1.f, 1.f, 0.f) }, { 0, 1, 2 });

	// Image de l'arme (ou petit dessin de sa famille)
	const FVector2D Centre = Pos + Int * FVector2D(0.5f, 0.47f);
	UObject* Ressource = Case.Image.IsValid() ? Case.Image->GetResourceObject() : nullptr;
	if (Ressource)
	{
		const FVector2D Brute = Case.Image->ImageSize;
		const float Ratio = (Brute.X > 1.f && Brute.Y > 1.f) ? Brute.X / Brute.Y : 1.f;
		const float Max = Int.X * 0.86f;
		const FVector2D T = Ratio >= 1.f ? FVector2D(Max, Max / Ratio) : FVector2D(Max * Ratio, Max);
		FSlateDrawElement::MakeBox(Sortie, Couche + 3, G.ToPaintGeometry(Centre - T * 0.5f, T), Case.Image.Get(),
			ESlateDrawEffect::None, FLinearColor::White);
	}
	else if (Dessinateur.IsValid())
	{
		Dessinateur->Dessin(Sortie, Couche + 3, G, Centre + FVector2D(1.5f, 2.f), S * 0.27f, Case.Categorie, FLinearColor(0.f, 0.f, 0.08f, 0.45f));
		Dessinateur->Dessin(Sortie, Couche + 4, G, Centre, S * 0.27f, Case.Categorie, FLinearColor::White);
	}

	// Nom en haut a gauche
	const FSlateFontInfo PoliceNom = MAPolice(FMath::Max(8, FMath::RoundToInt(S * 0.068f)));
	MAEcrire(Sortie, Couche + 5, G, Pos + FVector2D(5.f, 3.f), Case.Nom.ToString(), PoliceNom, FLinearColor::White);

	// Etoiles en bas a droite (1 a 5 selon la rarete)
	const float RayonEtoile = S * 0.05f;
	const int32 N = FMath::Clamp(Case.Rarete, 1, 5);
	for (int32 k = 0; k < N; ++k)
	{
		const FVector2D C = Pos + Int - FVector2D(RayonEtoile * 1.4f + k * RayonEtoile * 2.2f, RayonEtoile * 1.5f);
		MAEtoile(Sortie, Couche + 5, G, C + FVector2D(1.f, 1.5f), RayonEtoile, FLinearColor(0.f, 0.f, 0.05f, 0.5f));
		MAEtoile(Sortie, Couche + 6, G, C, RayonEtoile, FLinearColor::White);
	}

	// Badges
	if (Case.bEnMain)
	{
		const FSlateFontInfo P = MAPolice(FMath::Max(7, FMath::RoundToInt(S * 0.06f)));
		const FVector2D T = MAMesure(TEXT("EN MAIN"), P) + FVector2D(10.f, 2.f);
		const FVector2D B = Pos + FVector2D(0.f, Int.Y - T.Y);
		MARect(Sortie, Couche + 7, G, B, T, MAJaune);
		MAEcrire(Sortie, Couche + 8, G, B + FVector2D(5.f, 1.f), TEXT("EN MAIN"), P, MATexteSombre, false);
	}
	if (bCoffre)
	{
		// petit coffre jaune en haut a droite
		const float c = S * 0.12f;
		const FVector2D B = Pos + FVector2D(Int.X - c - 4.f, 4.f);
		MARect(Sortie, Couche + 7, G, B + FVector2D(0.f, c * 0.35f), FVector2D(c, c * 0.65f), MAJaune);
		MARect(Sortie, Couche + 7, G, B, FVector2D(c, c * 0.3f), MAPlus(MAJaune, 0.8f));
		MARect(Sortie, Couche + 8, G, B + FVector2D(c * 0.4f, c * 0.3f), FVector2D(c * 0.2f, c * 0.25f), MATexteSombre);
	}
	if (!Case.bPossedee)
	{
		// Pas encore trouvee (et ne peut pas etre donnee) : carte assombrie
		MARect(Sortie, Couche + 9, G, Pos, Int, FLinearColor(0.f, 0.f, 0.06f, 0.55f));
		const FSlateFontInfo P = MAPolice(FMath::Max(8, FMath::RoundToInt(S * 0.07f)));
		const FString Texte = TEXT("À TROUVER");
		const FVector2D T = MAMesure(Texte, P);
		MAEcrire(Sortie, Couche + 10, G, Pos + FVector2D((Int.X - T.X) * 0.5f, Int.Y * 0.5f - T.Y * 0.5f), Texte, P, FLinearColor(0.85f, 0.88f, 1.f));
	}

	// Choisie : cadre blanc epais ; survol : cadre clair fin
	if (bChoisie)
	{
		MACadre(Sortie, Couche + 11, G, FVector2D::ZeroVector, L, 4.f, FLinearColor::White);
		MACadre(Sortie, Couche + 11, G, FVector2D(4.f, 4.f), L - FVector2D(8.f, 8.f), 2.f, FLinearColor(0.55f, 0.75f, 1.f, 0.9f));
	}
	else if (bSurvol)
	{
		MACadre(Sortie, Couche + 11, G, FVector2D::ZeroVector, L, 2.f, FLinearColor(1.f, 1.f, 1.f, 0.7f));
	}
	return Couche + 12;
}

// =====================================================================================
//  Petites icones dessinees
// =====================================================================================

void SIconeArmements::Construct(const FArguments& InArgs)
{
	Forme = InArgs._Forme;
	Nombre = InArgs._Nombre;
	Taille = InArgs._Taille;
	Couleur = InArgs._Couleur;
	Coche = InArgs._Coche;
}

int32 SIconeArmements::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D L = G.GetLocalSize();
	const FVector2D C = L * 0.5f;
	const float S = FMath::Min(L.X, L.Y);
	switch (Forme)
	{
	case EForme::Etoiles:
	{
		const float R = L.Y * 0.45f;
		for (int32 k = 0; k < Nombre; ++k)
		{
			MAEtoile(Sortie, Couche, G, FVector2D(R + k * R * 2.3f, L.Y * 0.5f), R, Couleur);
		}
		break;
	}
	case EForme::Coche:
	{
		const bool bCoche = Coche.Get(false);
		MARect(Sortie, Couche, G, FVector2D::ZeroVector, L, FLinearColor(0.08f, 0.12f, 0.45f, 1.f));
		MACadre(Sortie, Couche + 1, G, FVector2D::ZeroVector, L, 2.5f, FLinearColor(0.45f, 0.52f, 0.95f, 1.f));
		if (bCoche)
		{
			MARect(Sortie, Couche + 1, G, FVector2D(4.f, 4.f), L - FVector2D(8.f, 8.f), MAJaune);
			MATrait(Sortie, Couche + 2, G, { FVector2D(S * 0.26f, S * 0.52f), FVector2D(S * 0.44f, S * 0.70f), FVector2D(S * 0.76f, S * 0.32f) }, MATexteSombre, 3.f);
		}
		break;
	}
	case EForme::Info:
	{
		MADisque(Sortie, Couche, G, C, S * 0.45f, Couleur);
		const FLinearColor Fond = Couleur.GetLuminance() > 0.5f ? MATexteSombre : FLinearColor::White;
		MADisque(Sortie, Couche + 1, G, C + FVector2D(0.f, -S * 0.2f), S * 0.065f, Fond);
		MARect(Sortie, Couche + 1, G, C + FVector2D(-S * 0.05f, -S * 0.07f), FVector2D(S * 0.1f, S * 0.3f), Fond);
		break;
	}
	case EForme::Filtres:
	{
		for (int32 k = 0; k < 3; ++k)
		{
			const float X = S * (0.25f + 0.25f * k);
			MATrait(Sortie, Couche, G, { FVector2D(X, S * 0.12f), FVector2D(X, S * 0.88f) }, Couleur, 2.5f);
			const float Y = S * ((k == 1) ? 0.66f : 0.34f);
			MADisque(Sortie, Couche + 1, G, FVector2D(X, Y), S * 0.09f, Couleur);
		}
		break;
	}
	case EForme::Fleche:
	{
		const float D = Nombre < 0 ? -1.f : 1.f;
		MATrait(Sortie, Couche, G, { C + FVector2D(-D * S * 0.18f, -S * 0.32f), C + FVector2D(D * S * 0.18f, 0.f), C + FVector2D(-D * S * 0.18f, S * 0.32f) }, Couleur, 4.f);
		break;
	}
	case EForme::Coffre:
	{
		MARect(Sortie, Couche, G, FVector2D(S * 0.1f, S * 0.42f), FVector2D(S * 0.8f, S * 0.45f), Couleur);
		MARect(Sortie, Couche, G, FVector2D(S * 0.1f, S * 0.15f), FVector2D(S * 0.8f, S * 0.22f), MAPlus(Couleur, 0.8f));
		MARect(Sortie, Couche + 1, G, FVector2D(S * 0.42f, S * 0.37f), FVector2D(S * 0.16f, S * 0.18f), MATexteSombre);
		break;
	}
	}
	return Couche + 3;
}

// =====================================================================================
//  Menu complet
// =====================================================================================

const FButtonStyle* SMenuArmements::StyleBouton(const FLinearColor& Normal, const FLinearColor& Survol, const FLinearColor& Appui)
{
	TSharedPtr<FButtonStyle> S = MakeShared<FButtonStyle>();
	S->SetNormal(FSlateColorBrush(Normal));
	S->SetHovered(FSlateColorBrush(Survol));
	S->SetPressed(FSlateColorBrush(Appui));
	S->SetDisabled(FSlateColorBrush(MAAlpha(Normal, 0.45f)));
	S->SetNormalPadding(FMargin(0.f));
	S->SetPressedPadding(FMargin(0.f));
	Styles.Add(S);
	return S.Get();
}

TSharedRef<SWidget> SMenuArmements::Bouton(TSharedRef<SWidget> Contenu, const FButtonStyle* Style, TFunction<void()> Action,
	FMargin Marge, TAttribute<bool> Actif)
{
	return SNew(SButton)
		.ButtonStyle(Style)
		.ContentPadding(Marge)
		.IsFocusable(false)
		.IsEnabled(Actif)
		.Cursor(EMouseCursor::Hand)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.OnClicked_Lambda([Action]()
		{
			if (Action)
			{
				Action();
			}
			return FReply::Handled();
		})
		[
			Contenu
		];
}

void SMenuArmements::Construct(const FArguments& InArgs)
{
	Filtres.Init(false, MANbFiltres);
	ToucheMenu = EKeys::I;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SFondArmements)
		]
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BarreOnglets()
			]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(70.f, 30.f, 70.f, 26.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(340.f)[ ColonneGauche() ]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(48.f, 0.f))
				[
					ColonneCentre()
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(340.f)[ ColonneDroite() ]
				]
			]
		]
		// Fleches sur les bords : categorie precedente / suivante
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f))
		[
			Bouton(SNew(SIconeArmements).Forme(SIconeArmements::EForme::Fleche).Nombre(-1).Taille(FVector2D(40.f, 60.f)),
				StyleBouton(MATransparent, FLinearColor(1.f, 1.f, 1.f, 0.08f), FLinearColor(1.f, 1.f, 1.f, 0.15f)),
				[this]() { ChoisirCategorie(-1); })
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f))
		[
			Bouton(SNew(SIconeArmements).Forme(SIconeArmements::EForme::Fleche).Nombre(1).Taille(FVector2D(40.f, 60.f)),
				StyleBouton(MATransparent, FLinearColor(1.f, 1.f, 1.f, 0.08f), FLinearColor(1.f, 1.f, 1.f, 0.15f)),
				[this]() { ChoisirCategorie(1); })
		]
	];
	ReconstruireCategories();
	ReconstruireGrille();
	ReconstruirePanneau();
	ReconstruireInventaire();
}

TSharedRef<SWidget> SMenuArmements::BarreOnglets()
{
	auto OngletInactif = [this](const TCHAR* Texte, int32 Index) -> TSharedRef<SWidget>
	{
		return Bouton(
			SNew(STextBlock)
			.Text(FText::FromString(Texte))
			.Font(MAPolice(26))
			.ColorAndOpacity(MABleuOnglet)
			.RenderTransform(MAPenche())
			.RenderTransformPivot(FVector2D(0.5f, 0.5f)),
			StyleBouton(MATransparent, FLinearColor(1.f, 1.f, 1.f, 0.07f), FLinearColor(1.f, 1.f, 1.f, 0.12f)),
			[this, Index]()
			{
				if (OnOnglet)
				{
					OnOnglet(Index);
				}
			},
			FMargin(30.f, 12.f));
	};

	return SNew(SBorder)
		.BorderImage(MABlanc())
		.BorderBackgroundColor(MABleuBarre)
		.Padding(FMargin(36.f, 0.f, 36.f, 0.f))
		[
			SNew(SHorizontalBox)
			// Onglet actif : fond blanc penche, trait jaune dessous
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill)
			[
				SNew(SBox)
				.RenderTransform(MAPenche(0.25f))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SNew(SBorder)
						.BorderImage(MABlanc())
						.BorderBackgroundColor(FLinearColor::White)
						.Padding(FMargin(34.f, 12.f))
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("ARMEMENTS")))
							.Font(MAPolice(28))
							.ColorAndOpacity(MATexteSombre)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBorder).BorderImage(MABlanc()).BorderBackgroundColor(MAJaune).Padding(FMargin(0.f, 3.f))
					]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(14.f, 0.f, 0.f, 0.f))
			[
				OngletInactif(TEXT("PERSONNAGE"), 1)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("CHELLES 3D")))
				.Font(MAPolice(24))
				.ColorAndOpacity(FLinearColor::White)
				.RenderTransform(MAPenche())
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			]
		];
}

TSharedRef<SWidget> SMenuArmements::ColonneGauche()
{
	return SNew(SVerticalBox)
		// Recherche
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(62.f)
			[
				SNew(SBorder)
				.BorderImage(MABlanc())
				.BorderBackgroundColor(FLinearColor(0.33f, 0.38f, 0.86f, 1.f))
				.Padding(FMargin(16.f, 0.f))
				.VAlign(VAlign_Center)
				[
					SAssignNew(ChampRecherche, SEditableTextBox)
					.HintText(FText::FromString(TEXT("Rechercher...")))
					.Font(MAPolice(22))
					.BackgroundColor(MATransparent)
					.ForegroundColor(FLinearColor::White)
					.OnTextChanged_Lambda([this](const FText& Texte)
					{
						Recherche = Texte.ToString().TrimStartAndEnd();
						ReconstruireGrille();
					})
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSpacer).Size(FVector2D(1.f, 44.f)) ]
		// MACategories
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBorder)
			.BorderImage(MABlanc())
			.BorderBackgroundColor(MABleuNuit)
			.Padding(FMargin(8.f, 16.f, 8.f, 8.f))
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(BoiteCategories, SVerticalBox)
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(MABlanc())
			.BorderBackgroundColor(MABleuPied)
			.Padding(FMargin(8.f, 6.f))
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
				[
					MAToucheClavier(TEXT("CTRL"), true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("CATÉGORIES"))).Font(MAPolice(17)).ColorAndOpacity(FLinearColor::White)
				]
			]
		];
}

TSharedRef<SWidget> SMenuArmements::ColonneCentre()
{
	const FButtonStyle* StyleLavande = StyleBouton(MALavande, FLinearColor(0.84f, 0.87f, 1.f), FLinearColor(0.6f, 0.65f, 0.98f));
	return SNew(SVerticalBox)
		// Titre
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox).HeightOverride(62.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("ARMEMENTS")))
				.Font(MAPolice(54))
				.ColorAndOpacity(FLinearColor(0.93f, 0.95f, 1.f))
				.ShadowOffset(FVector2D(3.f, 3.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.1f, 0.6f))
				.RenderTransform(MAPenche(0.22f))
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSpacer).Size(FVector2D(1.f, 30.f)) ]
		// Tri + nombre d'objets + relire
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Bouton(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
					[
						MAToucheClavier(TEXT("Z"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text_Lambda([this]() { return TexteTri(); }).Font(MAPolice(21)).ColorAndOpacity(MATexteSombre)
					],
					StyleLavande,
					[this]()
					{
						Tri = (Tri + 1) % MANbTris;
						ReconstruireGrille();
					},
					FMargin(20.f, 9.f))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(20.f, 0.f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%d OBJETS"), ListeVisible().Num())); })
				.Font(MAPolice(23))
				.ColorAndOpacity(MALavande)
			]
			+ SHorizontalBox::Slot().FillWidth(1.f)[ SNew(SSpacer) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				Bouton(
					SNew(STextBlock).Text(FText::FromString(TEXT("•••"))).Font(MAPolice(22)).ColorAndOpacity(MATexteSombre),
					StyleLavande,
					[this]()
					{
						if (OnRelire)
						{
							OnRelire();
						}
						Dire(TEXT("Liste des armes relue"));
					},
					FMargin(14.f, 2.f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSpacer).Size(FVector2D(1.f, 16.f)) ]
		// Grille de cartes
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SAssignNew(Defilement, SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(Grille, SUniformGridPanel).SlotPadding(FMargin(8.f))
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSpacer).Size(FVector2D(1.f, 14.f)) ]
		// Cases de l'inventaire du soldat
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SAssignNew(Inventaire, SHorizontalBox)
		]
		+ SVerticalBox::Slot().AutoHeight()[ SNew(SSpacer).Size(FVector2D(1.f, 18.f)) ]
		// Boutons
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f))
			[
				BoutonBas(FText::FromString(TEXT("ÉQUIPER")), [this]()
				{
					if (OnEquiper && Choisie() != INDEX_NONE)
					{
						OnEquiper(Choisie());
					}
				}, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]() { return Choisie() != INDEX_NONE && Cases[Choisie()].bPossedee; })))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f))
			[
				Bouton(
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const int32 i = Choisie();
						const bool bDedans = i != INDEX_NONE && LeCoffre.Contains(Cases[i].Nom.ToString());
						return FText::FromString(bDedans ? TEXT("RETIRER DU COFFRE") : TEXT("AJOUTER AU COFFRE"));
					})
					.Font(MAPolice(22))
					.ColorAndOpacity(MATexteSombre)
					.Justification(ETextJustify::Center),
					StyleBouton(MABleuBouton, MABleuSurvol, MALavande),
					[this]()
					{
						if (OnCoffre && Choisie() != INDEX_NONE)
						{
							OnCoffre(Choisie());
						}
					},
					FMargin(34.f, 10.f),
					TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]() { return Choisie() != INDEX_NONE; })))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(8.f, 0.f))
			[
				BoutonBas(FText::FromString(TEXT("LÂCHER")), [this]()
				{
					if (OnLacher && Choisie() != INDEX_NONE)
					{
						OnLacher(Choisie());
					}
				}, TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([this]() { return Choisie() != INDEX_NONE && Cases[Choisie()].bEnMain; })))
			]
		]
		// Message (ex : pas en mode Soldat)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return TexteMessage; })
			.Font(MAPolice(18))
			.ColorAndOpacity_Lambda([this]() { return FSlateColor(MAAlpha(MAJaune, FMath::Clamp(4.f - AgeMessage, 0.f, 1.f))); })
			.ShadowOffset(FVector2D(1.f, 1.f))
		];
}

TSharedRef<SWidget> SMenuArmements::BoutonBas(const FText& Texte, TFunction<void()> Action, TAttribute<bool> Actif)
{
	return Bouton(
		SNew(STextBlock).Text(Texte).Font(MAPolice(22)).ColorAndOpacity(MATexteSombre).Justification(ETextJustify::Center),
		StyleBouton(MABleuBouton, MABleuSurvol, MALavande),
		Action,
		FMargin(60.f, 10.f),
		Actif);
}

TSharedRef<SWidget> SMenuArmements::ColonneDroite()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(Panneau, SBox) // remplace juste apres par ReconstruirePanneau (boutons + contenu)
		]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SSpacer)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f, 18.f, 0.f, 0.f))
		[
			Bouton(
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
				[
					MAToucheClavier(TEXT("ÉCHAP"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("QUITTER"))).Font(MAPolice(22)).ColorAndOpacity(MATexteSombre)
				],
				StyleBouton(MABleuBouton, MABleuSurvol, MALavande),
				[this]()
				{
					if (OnFermer)
					{
						OnFermer();
					}
				},
				FMargin(26.f, 10.f))
		];
}

// =====================================================================================
//  Contenus qui changent
// =====================================================================================

void SMenuArmements::ReconstruireCategories()
{
	if (!BoiteCategories.IsValid())
	{
		return;
	}
	BoiteCategories->ClearChildren();
	for (int32 k = 0; k < MANbCategories; ++k)
	{
		const bool bChoisie = (k == Categorie);
		BoiteCategories->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))
		[
			SNew(SBox).HeightOverride(60.f)
			[
				Bouton(
					SNew(STextBlock)
					.Text(FText::FromString(MACategories[k].Libelle))
					.Font(MAPolice(24, bChoisie))
					.ColorAndOpacity(bChoisie ? MATexteSombre : FLinearColor(0.9f, 0.92f, 1.f)),
					bChoisie ? StyleBouton(MALavande, FLinearColor(0.84f, 0.87f, 1.f), MALavande)
						: StyleBouton(MATransparent, FLinearColor(1.f, 1.f, 1.f, 0.08f), FLinearColor(1.f, 1.f, 1.f, 0.14f)),
					[this, k]()
					{
						Categorie = k;
						ReconstruireCategories();
						ReconstruireGrille();
					},
					FMargin(24.f, 0.f))
			]
		];
	}
}

bool SMenuArmements::Visible(int32 Index) const
{
	if (!Cases.IsValidIndex(Index))
	{
		return false;
	}
	const FCaseArme& C = Cases[Index];
	const FString Cle = MACategories[FMath::Clamp(Categorie, 0, MANbCategories - 1)].Cle;
	if (Cle == TEXT("#Coffre"))
	{
		if (!LeCoffre.Contains(C.Nom.ToString()))
		{
			return false;
		}
	}
	else if (Cle == TEXT("#Arsenal"))
	{
		if (!C.bArsenal)
		{
			return false;
		}
	}
	else if (Cle == TEXT("#Packs"))
	{
		if (!C.bPack)
		{
			return false;
		}
	}
	else if (!Cle.IsEmpty() && !C.Categorie.Equals(Cle, ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (!Recherche.IsEmpty() && !C.Nom.ToString().Contains(Recherche) && !MANomFamille(C.Categorie).Contains(Recherche)
		&& !MenuArmementsStyle::NomRarete(C.Rarete).ToString().Contains(Recherche))
	{
		return false;
	}
	// Filtres : possession (1-2) et rarete (3-7), chaque groupe en "ou"
	if (Filtres[1] || Filtres[2])
	{
		if (!((Filtres[1] && C.bRamassee) || (Filtres[2] && !C.bRamassee)))
		{
			return false;
		}
	}
	bool bUneRarete = false;
	for (int32 k = 3; k < MANbFiltres; ++k)
	{
		bUneRarete |= Filtres[k];
	}
	if (bUneRarete && !Filtres[3 + (5 - FMath::Clamp(C.Rarete, 1, 5))])
	{
		return false;
	}
	return true;
}

TArray<int32> SMenuArmements::ListeVisible() const
{
	TArray<int32> Liste;
	for (int32 i = 0; i < Cases.Num(); ++i)
	{
		if (Visible(i))
		{
			Liste.Add(i);
		}
	}
	if (Tri != 0)
	{
		const TArray<FCaseArme>& C = Cases;
		const int32 Mode = Tri;
		Liste.StableSort([&C, Mode](int32 A, int32 B)
		{
			if (Mode == 1 && C[A].Rarete != C[B].Rarete)
			{
				return C[A].Rarete > C[B].Rarete;
			}
			if (Mode == 3 && C[A].bRamassee != C[B].bRamassee)
			{
				return C[A].bRamassee;
			}
			if (Mode == 2 || Mode == 1)
			{
				return C[A].Nom.ToString() < C[B].Nom.ToString();
			}
			return A < B;
		});
	}
	return Liste;
}

int32 SMenuArmements::Choisie() const
{
	if (NomChoisi.IsEmpty())
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < Cases.Num(); ++i)
	{
		if (Cases[i].Nom.ToString() == NomChoisi)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void SMenuArmements::Choisir(int32 Index)
{
	NomChoisi = Cases.IsValidIndex(Index) ? Cases[Index].Nom.ToString() : FString();
	ReconstruirePanneau();
}

void SMenuArmements::ReconstruireGrille()
{
	if (!Grille.IsValid())
	{
		return;
	}
	Grille->ClearChildren();
	const TArray<int32> Liste = ListeVisible();
	if (Liste.Num() == 0)
	{
		Grille->AddSlot(0, 0)
		[
			SNew(SBox).Padding(FMargin(20.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Categorie == 1 && Recherche.IsEmpty()
					? TEXT("Le coffre est vide : choisis une arme puis AJOUTER AU COFFRE.")
					: TEXT("Aucune arme ici.")))
				.Font(MAPolice(20, false))
				.ColorAndOpacity(MALavande)
			]
		];
		return;
	}
	if (Choisie() == INDEX_NONE || !Liste.Contains(Choisie()))
	{
		// Rien de choisi (ou cache par un filtre) : l'arme en main, sinon la premiere
		int32 Defaut = Liste[0];
		for (int32 i : Liste)
		{
			if (Cases[i].bEnMain)
			{
				Defaut = i;
				break;
			}
		}
		NomChoisi = Cases[Defaut].Nom.ToString();
		ReconstruirePanneau();
	}
	for (int32 k = 0; k < Liste.Num(); ++k)
	{
		const int32 i = Liste[k];
		const FString Nom = Cases[i].Nom.ToString();
		Grille->AddSlot(k % Colonnes, k / Colonnes)
		[
			SNew(SCarteArme)
			.Case(Cases[i])
			.Taille(150.f)
			.DansLeCoffre(LeCoffre.Contains(Nom))
			.Choisie_Lambda([this, Nom]() { return NomChoisi == Nom; })
			.OnClic_Lambda([this, i]() { Choisir(i); })
			.OnDoubleClic_Lambda([this, i]()
			{
				Choisir(i);
				if (OnEquiper)
				{
					OnEquiper(i);
				}
			})
		];
	}
}

void SMenuArmements::ReconstruireInventaire()
{
	if (!Inventaire.IsValid())
	{
		return;
	}
	Inventaire->ClearChildren();
	// Armes vraiment portees par le soldat (pas l'arsenal), comme les cases de son inventaire
	TArray<int32> Portees;
	for (int32 i = 0; i < Cases.Num(); ++i)
	{
		if (Cases[i].bRamassee && !Cases[i].bArsenal)
		{
			Portees.Add(i);
		}
	}
	const int32 NbCases = FMath::Max(5, Portees.Num());
	for (int32 k = 0; k < NbCases; ++k)
	{
		const bool bPleine = Portees.IsValidIndex(k);
		const int32 i = bPleine ? Portees[k] : INDEX_NONE;
		const FString Nom = bPleine ? Cases[i].Nom.ToString() : FString();
		Inventaire->AddSlot().AutoWidth().Padding(FMargin(5.f, 0.f))
		[
			SNew(SBox).WidthOverride(104.f).HeightOverride(104.f)
			[
				bPleine
					? StaticCastSharedRef<SWidget>(SNew(SCarteArme)
						.Case(Cases[i])
						.Taille(104.f)
						.DansLeCoffre(LeCoffre.Contains(Nom))
						.Choisie_Lambda([this, Nom]() { return NomChoisi == Nom; })
						.OnClic_Lambda([this, i]() { Choisir(i); })
						.OnDoubleClic_Lambda([this, i]()
						{
							Choisir(i);
							if (OnEquiper)
							{
								OnEquiper(i);
							}
						}))
					: StaticCastSharedRef<SWidget>(SNew(SCarteArme).Vide(true).Taille(104.f))
			]
		];
	}
}

void SMenuArmements::ReconstruirePanneau()
{
	if (!Panneau.IsValid())
	{
		return;
	}
	auto BoutonVue = [this](bool bFiltres) -> TSharedRef<SWidget>
	{
		const bool bActifIci = (bPanneauFiltres == bFiltres);
		return SNew(SBox)
			.HeightOverride(62.f)
			[
				Bouton(
					SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
					[
						SNew(SIconeArmements)
						.Forme(bFiltres ? SIconeArmements::EForme::Filtres : SIconeArmements::EForme::Info)
						.Taille(FVector2D(30.f, 30.f))
						.Couleur(bActifIci ? MATexteSombre : FLinearColor::White)
					],
					bActifIci ? StyleBouton(FLinearColor(0.88f, 0.9f, 1.f), FLinearColor::White, MALavande)
						: StyleBouton(MABleuNuit, FLinearColor(0.1f, 0.14f, 0.45f, 1.f), MABleuPied),
					[this, bFiltres]()
					{
						bPanneauFiltres = bFiltres;
						ReconstruirePanneau();
					})
			];
	};

	TSharedRef<SVerticalBox> Contenu = SNew(SVerticalBox);
	if (bPanneauFiltres)
	{
		bool bUnFiltre = false;
		for (int32 k = 1; k < MANbFiltres; ++k)
		{
			bUnFiltre |= Filtres[k];
		}
		for (int32 k = 0; k < MANbFiltres; ++k)
		{
			const bool bCoche = (k == 0) ? !bUnFiltre : Filtres[k];
			Contenu->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))
			[
				Bouton(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 16.f, 0.f))
					[
						SNew(SIconeArmements).Forme(SIconeArmements::EForme::Coche).Taille(FVector2D(30.f, 30.f)).Coche(bCoche)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString(LibellesFiltres[k])).Font(MAPolice(22)).ColorAndOpacity(FLinearColor(0.92f, 0.94f, 1.f))
					],
					StyleBouton(MATransparent, FLinearColor(1.f, 1.f, 1.f, 0.07f), FLinearColor(1.f, 1.f, 1.f, 0.12f)),
					[this, k]()
					{
						if (k == 0)
						{
							for (bool& F : Filtres)
							{
								F = false;
							}
						}
						else
						{
							Filtres[k] = !Filtres[k];
						}
						ReconstruirePanneau();
						ReconstruireGrille();
					},
					FMargin(14.f, 8.f))
			];
		}
	}
	else
	{
		const int32 i = Choisie();
		if (i == INDEX_NONE)
		{
			Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f))
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Choisis une arme dans la grille."))).Font(MAPolice(20, false)).ColorAndOpacity(MALavande).AutoWrapText(true)
			];
		}
		else
		{
			const FCaseArme& C = Cases[i];
			FString Etat;
			if (C.bEnMain)
			{
				Etat = TEXT("EN MAIN");
			}
			else if (C.bRamassee)
			{
				Etat = TEXT("Ramassée");
			}
			else if (C.bPossedee)
			{
				Etat = TEXT("Pas ramassée : ÉQUIPER te la donne");
			}
			else
			{
				Etat = C.bArsenal ? TEXT("Pas prête (prends-la sur un râtelier)") : TEXT("À trouver sur la carte");
			}
			auto Ligne = [](const FString& Titre, const FString& Valeur) -> TSharedRef<SWidget>
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 8.f, 0.f))
					[
						SNew(STextBlock).Text(FText::FromString(Titre)).Font(MAPolice(17)).ColorAndOpacity(MALavande)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Text(FText::FromString(Valeur)).Font(MAPolice(17, false)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
					];
			};
			Contenu->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f))
			[
				SNew(SBox).WidthOverride(200.f).HeightOverride(200.f)
				[
					SNew(SCarteArme).Case(C).Taille(200.f).DansLeCoffre(LeCoffre.Contains(C.Nom.ToString()))
				]
			];
			Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 4.f))
			[
				SNew(STextBlock).Text(C.Nom).Font(MAPolice(28)).ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
				.RenderTransform(MAPenche(0.15f)).RenderTransformPivot(FVector2D(0.5f, 0.5f))
			];
			Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 2.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 12.f, 0.f))
				[
					SNew(STextBlock).Text(MenuArmementsStyle::NomRarete(C.Rarete)).Font(MAPolice(19))
					.ColorAndOpacity(MAPlus(MenuArmementsStyle::CouleurRarete(C.Rarete), 1.35f))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SIconeArmements).Forme(SIconeArmements::EForme::Etoiles).Nombre(FMath::Clamp(C.Rarete, 1, 5))
					.Taille(FVector2D(FMath::Clamp(C.Rarete, 1, 5) * 21.f, 18.f)).Couleur(FLinearColor::White)
				]
			];
			Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 10.f, 16.f, 2.f))[ Ligne(TEXT("Famille :"), C.bArsenal ? FString(TEXT("Arsenal CHELLES")) : MANomFamille(C.Categorie)) ];
			if (!C.Munitions.IsEmpty())
			{
				Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 2.f))[ Ligne(TEXT("Munitions :"), C.Munitions.ToString()) ];
			}
			Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 2.f))[ Ligne(TEXT("État :"), Etat) ];
			if (C.bPack)
			{
				Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 2.f))[ Ligne(TEXT("Modèle :"), TEXT("arme d'un pack")) ];
			}
			if (LeCoffre.Contains(C.Nom.ToString()))
			{
				Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 2.f))[ Ligne(TEXT("Coffre :"), TEXT("oui")) ];
			}
			Contenu->AddSlot().AutoHeight().Padding(FMargin(16.f, 14.f, 16.f, 4.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Double-clic sur une carte = ÉQUIPER.\nFlèches = changer de carte, Entrée = ÉQUIPER.")))
				.Font(MAPolice(14, false))
				.ColorAndOpacity(MAAlpha(MALavande, 0.8f))
				.AutoWrapText(true)
			];
		}
	}

	Panneau->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)[ BoutonVue(false) ]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(6.f, 0.f, 0.f, 0.f))[ BoutonVue(true) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))
		[
			SNew(SBox).HeightOverride(640.f)
			[
				SNew(SBorder)
				.BorderImage(MABlanc())
				.BorderBackgroundColor(MABleuNuit)
				.Padding(FMargin(8.f, 12.f))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()[ Contenu ]
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(MABlanc())
			.BorderBackgroundColor(MABleuPied)
			.Padding(FMargin(8.f, 6.f))
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 10.f, 0.f))
				[
					MAToucheClavier(TEXT("ALT"), true)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(bPanneauFiltres ? TEXT("FILTRES") : TEXT("FICHE DE L'ARME"))).Font(MAPolice(17)).ColorAndOpacity(FLinearColor::White)
				]
			]
		]);
}

// =====================================================================================
//  Mise a jour, clavier
// =====================================================================================

void SMenuArmements::MettreAJour(const TArray<FCaseArme>& NouvellesCases, const TSet<FString>& Coffre)
{
	Cases = NouvellesCases;
	LeCoffre = Coffre;
	ReconstruireGrille();
	ReconstruireInventaire();
	ReconstruirePanneau();
}

void SMenuArmements::Dire(const FString& Texte)
{
	TexteMessage = FText::FromString(Texte);
	AgeMessage = 0.f;
}

void SMenuArmements::Tick(const FGeometry& Geometrie, const double Temps, const float Delta)
{
	SCompoundWidget::Tick(Geometrie, Temps, Delta);
	AgeMessage += Delta;
}

FText SMenuArmements::TexteTri() const
{
	return FText::FromString(FString(TEXT("TRI : ")) + MATris[FMath::Clamp(Tri, 0, MANbTris - 1)]);
}

void SMenuArmements::ChoisirCategorie(int32 Sens)
{
	Categorie = ((Categorie + Sens) % MANbCategories + MANbCategories) % MANbCategories;
	ReconstruireCategories();
	ReconstruireGrille();
}

void SMenuArmements::Deplacer(int32 DX, int32 DY)
{
	const TArray<int32> Liste = ListeVisible();
	if (Liste.Num() == 0)
	{
		return;
	}
	int32 Pos = Liste.IndexOfByKey(Choisie());
	if (Pos == INDEX_NONE)
	{
		Pos = 0;
	}
	else
	{
		Pos = FMath::Clamp(Pos + DX + DY * Colonnes, 0, Liste.Num() - 1);
	}
	Choisir(Liste[Pos]);
}

FReply SMenuArmements::OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Evenement)
{
	const FKey K = Evenement.GetKey();
	const bool bSaisie = ChampRecherche.IsValid() && (ChampRecherche->HasKeyboardFocus() || ChampRecherche->HasFocusedDescendants());
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::Gamepad_Special_Right
		|| (!bSaisie && ToucheMenu.IsValid() && K == ToucheMenu))
	{
		if (bSaisie && K == EKeys::Escape)
		{
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly); // Echap quitte d'abord la recherche
			return FReply::Handled();
		}
		if (OnFermer)
		{
			OnFermer();
		}
		return FReply::Handled();
	}
	if (bSaisie)
	{
		return FReply::Unhandled();
	}
	if (K == EKeys::Z)
	{
		Tri = (Tri + 1) % MANbTris;
		ReconstruireGrille();
	}
	else if (K == EKeys::LeftControl || K == EKeys::RightControl || K == EKeys::Gamepad_RightShoulder)
	{
		ChoisirCategorie(1);
	}
	else if (K == EKeys::Gamepad_LeftShoulder)
	{
		ChoisirCategorie(-1);
	}
	else if (K == EKeys::LeftAlt || K == EKeys::RightAlt || K == EKeys::Gamepad_FaceButton_Top)
	{
		bPanneauFiltres = !bPanneauFiltres;
		ReconstruirePanneau();
	}
	else if (K == EKeys::Left || K == EKeys::Gamepad_DPad_Left)
	{
		Deplacer(-1, 0);
	}
	else if (K == EKeys::Right || K == EKeys::Gamepad_DPad_Right)
	{
		Deplacer(1, 0);
	}
	else if (K == EKeys::Up || K == EKeys::Gamepad_DPad_Up)
	{
		Deplacer(0, -1);
	}
	else if (K == EKeys::Down || K == EKeys::Gamepad_DPad_Down)
	{
		Deplacer(0, 1);
	}
	else if (K == EKeys::Enter || K == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (OnEquiper && Choisie() != INDEX_NONE)
		{
			OnEquiper(Choisie());
		}
	}
	else
	{
		return FReply::Unhandled();
	}
	return FReply::Handled();
}
