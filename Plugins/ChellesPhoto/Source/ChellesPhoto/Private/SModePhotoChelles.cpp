// CHELLES 3D - Mode photo : affichage (style lobby bleu : bleu nuit, bleu roi, blanc, accent jaune)
#include "SModePhotoChelles.h"
#include "ModePhotoChelles.h"

#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace PhotoCouleurs
{
	static const FLinearColor BleuNuit(0.006f, 0.012f, 0.05f, 0.82f);
	static const FLinearColor BleuRoi(0.02f, 0.08f, 0.45f, 1.f);
	static const FLinearColor Jaune(1.f, 0.78f, 0.05f, 1.f);
	static const FLinearColor Blanc(1.f, 1.f, 1.f, 1.f);
	static const FLinearColor Gris(0.75f, 0.8f, 0.9f, 1.f);
}

static const FSlateBrush* BrosseBlanche()
{
	static const FSlateColorBrush Brosse(FLinearColor::White);
	return &Brosse;
}

static void Boite(FSlateWindowElementList& Liste, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Taille, const FLinearColor& Couleur)
{
	FSlateDrawElement::MakeBox(Liste, Couche, G.ToPaintGeometry(Pos, Taille), BrosseBlanche(), ESlateDrawEffect::None, Couleur);
}

static void Texte(FSlateWindowElementList& Liste, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FString& Chaine, int32 TaillePolice, bool bGras, const FLinearColor& Couleur)
{
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", TaillePolice);
	FSlateDrawElement::MakeText(Liste, Couche, G.ToPaintGeometry(Pos, FVector2D(2000.f, TaillePolice * 2.f)), Chaine, Police, ESlateDrawEffect::None, Couleur);
}

static float LargeurTexte(const FString& Chaine, int32 TaillePolice, bool bGras)
{
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", TaillePolice);
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Chaine, Police).X;
}

void SModePhotoChelles::Construct(const FArguments& InArgs, FModePhotoChelles* InMode)
{
	Mode = InMode;
	SetCanTick(false);
	SetVisibility(EVisibility::HitTestInvisible);
}

void SModePhotoChelles::Message(const FString& InTexte, bool bFlash)
{
	TexteMessage = InTexte;
	const double Maintenant = FSlateApplication::Get().GetCurrentTime();
	FinMessage = Maintenant + 4.0;
	if (bFlash)
	{
		DebutFlash = Maintenant;
	}
}

int32 SModePhotoChelles::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Liste, int32 Couche, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (!Mode)
	{
		return Couche;
	}
	using namespace PhotoCouleurs;
	const FEtatPhoto E = Mode->Etat();
	const FVector2D T = G.GetLocalSize();
	const double Maintenant = FSlateApplication::Get().GetCurrentTime();

	// --- Cadre TikTok 9:16 : bandes sombres a gauche et a droite (la photo est recadree pareil)
	float CadreX = 0.f;
	float CadreL = T.X;
	if (E.bCadreTikTok)
	{
		CadreL = FMath::Min(T.X, T.Y * 9.f / 16.f);
		CadreX = (T.X - CadreL) * 0.5f;
		Boite(Liste, Couche, G, FVector2D(0.f, 0.f), FVector2D(CadreX, T.Y), FLinearColor(0.f, 0.f, 0.f, 0.65f));
		Boite(Liste, Couche, G, FVector2D(CadreX + CadreL, 0.f), FVector2D(CadreX, T.Y), FLinearColor(0.f, 0.f, 0.f, 0.65f));
		Boite(Liste, Couche, G, FVector2D(CadreX - 2.f, 0.f), FVector2D(2.f, T.Y), Jaune);
		Boite(Liste, Couche, G, FVector2D(CadreX + CadreL, 0.f), FVector2D(2.f, T.Y), Jaune);
	}

	// --- Grille des tiers
	if (E.bGrille)
	{
		const FLinearColor Ligne(1.f, 1.f, 1.f, 0.35f);
		for (int32 i = 1; i <= 2; ++i)
		{
			Boite(Liste, Couche, G, FVector2D(CadreX + CadreL * i / 3.f, 0.f), FVector2D(1.f, T.Y), Ligne);
			Boite(Liste, Couche, G, FVector2D(CadreX, T.Y * i / 3.f), FVector2D(CadreL, 1.f), Ligne);
		}
	}

	// --- Plaque "MODE PHOTO" en haut a gauche + filtre et zoom
	{
		const FString Titre = TEXT("MODE PHOTO");
		const float L = LargeurTexte(Titre, 20, true) + 36.f;
		Boite(Liste, Couche, G, FVector2D(24.f, 24.f), FVector2D(L, 44.f), Blanc);
		Boite(Liste, Couche, G, FVector2D(24.f, 68.f), FVector2D(L, 5.f), Jaune);
		Texte(Liste, Couche + 1, G, FVector2D(42.f, 31.f), Titre, 20, true, BleuRoi);
		FString Infos = FString::Printf(TEXT("FILTRE : %s    ZOOM : %d°"), *E.Filtre, FMath::RoundToInt(E.Fov));
		if (E.bFlou) { Infos += TEXT("    FLOU DE FOND"); }
		if (E.bCadreTikTok) { Infos += TEXT("    CADRE TIKTOK 9:16"); }
		Infos += E.bFige ? TEXT("    MONDE FIGÉ") : TEXT("    LE MONDE CONTINUE (MULTIJOUEUR)");
		const float LI = LargeurTexte(Infos, 11, true) + 24.f;
		Boite(Liste, Couche, G, FVector2D(24.f, 80.f), FVector2D(LI, 26.f), BleuNuit);
		Texte(Liste, Couche + 1, G, FVector2D(36.f, 84.f), Infos, 11, true, Blanc);
	}

	// --- Aide (touches) en bas a gauche, empilee en colonne
	if (E.bAide)
	{
		struct FLigne { FString Touche; FString Action; };
		TArray<FLigne> Lignes;
		Lignes.Add({ TEXT("CLIC GAUCHE / ENTRÉE"), TEXT("PRENDRE LA PHOTO") });
		Lignes.Add({ TEXT("Z Q S D / FLÈCHES"), TEXT("DÉPLACER LA CAMÉRA") });
		Lignes.Add({ TEXT("ESPACE / C"), TEXT("MONTER / DESCENDRE") });
		Lignes.Add({ TEXT("SOURIS"), TEXT("REGARDER") });
		Lignes.Add({ TEXT("MAJ / ALT"), TEXT("PLUS VITE / PLUS LENT") });
		Lignes.Add({ TEXT("MOLETTE"), TEXT("ZOOM") });
		Lignes.Add({ TEXT("CLIC DROIT + MOLETTE"), TEXT("PENCHER") });
		Lignes.Add({ TEXT("F"), TEXT("FILTRE SUIVANT") });
		Lignes.Add({ TEXT("B"), TEXT("FLOU DE FOND (NET AU CENTRE)") });
		Lignes.Add({ TEXT("T"), TEXT("CADRE TIKTOK 9:16") });
		Lignes.Add({ TEXT("G"), TEXT("GRILLE DES TIERS") });
		Lignes.Add({ TEXT("X"), E.bPersoCache ? TEXT("MONTRER MON PERSO") : TEXT("CACHER MON PERSO") });
		Lignes.Add({ TEXT("R"), TEXT("REMETTRE LA CAMÉRA") });
		Lignes.Add({ TEXT("O"), TEXT("OUVRIR LE DOSSIER DES PHOTOS") });
		Lignes.Add({ TEXT("H"), TEXT("CACHER CETTE AIDE") });
		Lignes.Add({ FString::Printf(TEXT("%s / ÉCHAP"), *E.ToucheMode), TEXT("QUITTER LE MODE PHOTO") });

		const float HL = 22.f;
		const float Largeur = 430.f;
		const float Hauteur = Lignes.Num() * HL + 24.f + (E.Partage.IsEmpty() ? 0.f : 30.f);
		const FVector2D Pos(24.f, T.Y - Hauteur - 24.f);
		Boite(Liste, Couche, G, Pos, FVector2D(Largeur, Hauteur), BleuNuit);
		Boite(Liste, Couche, G, Pos, FVector2D(5.f, Hauteur), Jaune);
		for (int32 i = 0; i < Lignes.Num(); ++i)
		{
			const FVector2D P(Pos.X + 18.f, Pos.Y + 12.f + i * HL);
			Texte(Liste, Couche + 1, G, P, Lignes[i].Touche, 10, true, Jaune);
			Texte(Liste, Couche + 1, G, P + FVector2D(185.f, 0.f), Lignes[i].Action, 10, false, Blanc);
		}
		if (!E.Partage.IsEmpty())
		{
			Texte(Liste, Couche + 1, G, FVector2D(Pos.X + 18.f, Pos.Y + Hauteur - 30.f), E.Partage, 9, false, Gris);
		}
	}
	else
	{
		Texte(Liste, Couche + 1, G, FVector2D(24.f, T.Y - 36.f), TEXT("H = AIDE"), 10, true, FLinearColor(1.f, 1.f, 1.f, 0.5f));
	}

	// --- Message (photo enregistree...) en bas au centre
	if (Maintenant < FinMessage && !TexteMessage.IsEmpty())
	{
		const float Alpha = FMath::Clamp(static_cast<float>(FinMessage - Maintenant), 0.f, 1.f);
		const float L = LargeurTexte(TexteMessage, 13, true) + 40.f;
		const FVector2D Pos((T.X - L) * 0.5f, T.Y - 90.f);
		Boite(Liste, Couche + 2, G, Pos, FVector2D(L, 36.f), FLinearColor(1.f, 1.f, 1.f, Alpha));
		Boite(Liste, Couche + 2, G, Pos + FVector2D(0.f, 36.f), FVector2D(L, 4.f), FLinearColor(Jaune.R, Jaune.G, Jaune.B, Alpha));
		Texte(Liste, Couche + 3, G, Pos + FVector2D(20.f, 8.f), TexteMessage, 13, true, FLinearColor(BleuRoi.R, BleuRoi.G, BleuRoi.B, Alpha));
	}

	// --- Flash blanc juste apres la photo
	const float DepuisFlash = static_cast<float>(Maintenant - DebutFlash);
	if (DepuisFlash >= 0.f && DepuisFlash < 0.35f)
	{
		Boite(Liste, Couche + 4, G, FVector2D::ZeroVector, T, FLinearColor(1.f, 1.f, 1.f, 0.8f * (1.f - DepuisFlash / 0.35f)));
	}
	return Couche + 4;
}
