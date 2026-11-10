// CHELLES 3D - Invasion zombies : message rouge sang qui degouline et defile de droite a gauche
#include "SAlerteZombies.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor SangFonce(0.20f, 0.f, 0.f, 1.f);
	const FLinearColor Sang(0.55f, 0.01f, 0.01f, 1.f);
	const FLinearColor SangClair(0.85f, 0.04f, 0.03f, 1.f);

	/** Nombre pseudo-aleatoire stable entre 0 et 1 (meme valeur a chaque image pour une meme goutte). */
	float Hasard(int32 Graine)
	{
		uint32 X = (uint32)Graine * 2654435761u + 0x9E3779B9u;
		X ^= X >> 15;
		X *= 0x85EBCA6Bu;
		X ^= X >> 13;
		return (float)(X & 0xFFFF) / 65535.f;
	}

	/** Temps pour que le texte traverse l'ecran une fois. */
	const float DureePassage = 10.f;
	const int32 NombreGouttes = 18;
}

void SAlerteZombies::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SAlerteZombies::Boite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos,
	const FVector2D& Taille, const FLinearColor& Couleur) const
{
	FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Taille), FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),
		ESlateDrawEffect::None, Couleur);
}

int32 SAlerteZombies::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (Etat.AgeAlerte >= 0.f && Etat.AgeAlerte < Etat.DureeAlerte)
	{
		PeindreAlerte(Sortie, Couche, G);
	}
	if (Etat.bInvasion)
	{
		PeindreCompteur(Sortie, Couche + 10, G);
	}
	if (Etat.AgeFin < 6.f && !Etat.MessageFin.IsEmpty())
	{
		PeindreFin(Sortie, Couche + 10, G);
	}
	return Couche + 20;
}

void SAlerteZombies::PeindreAlerte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Age = Etat.AgeAlerte;

	// Apparition / disparition douces
	const float Fondu = FMath::Clamp(Age / 0.6f, 0.f, 1.f) * FMath::Clamp((Etat.DureeAlerte - Age) / 1.2f, 0.f, 1.f);

	// Ecran qui pulse en rouge comme un gyrophare
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Age * 2.f * PI / 1.5f);
	Boite(Sortie, Couche, G, FVector2D::ZeroVector, Ecran, FLinearColor(0.6f, 0.f, 0.f, (0.05f + 0.10f * Pulse) * Fondu));

	// Bandeau sombre derriere le texte
	const float TaillePolice = FMath::Clamp(Ecran.Y * 0.085f, 28.f, 110.f);
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle("Bold", (int32)TaillePolice);
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TailleTexte = Mesure->Measure(Etat.Texte, Police);
	const float HautTexte = Ecran.Y * 0.5f - TailleTexte.Y * 0.5f;
	Boite(Sortie, Couche, G, FVector2D(0.f, HautTexte - TailleTexte.Y * 0.25f), FVector2D(Ecran.X, TailleTexte.Y * 1.9f),
		FLinearColor(0.f, 0.f, 0.f, 0.55f * Fondu));

	// Defilement de droite vers la gauche, en boucle pendant toute l'alerte
	const float Trajet = Ecran.X + TailleTexte.X;
	const float AgePassage = FMath::Fmod(Age, DureePassage);
	const float X = Ecran.X - Trajet * (AgePassage / DureePassage);

	// Petit tremblement "horreur"
	const FVector2D Tremble(FMath::Sin(Age * 37.f) * 1.5f, FMath::Cos(Age * 29.f) * 1.5f);
	const FVector2D Coin = FVector2D(X, HautTexte) + Tremble;

	// Coulures de sang sous les lettres (dessinees avant le texte pour partir "de dedans")
	for (int32 k = 0; k < NombreGouttes; ++k)
	{
		const float PosX = Coin.X + TailleTexte.X * (0.03f + 0.94f * Hasard(k * 3 + 1));
		const float Depart = 0.4f + 3.5f * Hasard(k * 3 + 2);          // quand la coulure commence
		const float LongueurMax = TailleTexte.Y * (0.35f + 1.1f * Hasard(k * 3 + 3));
		const float Largeur = FMath::Max(3.f, TaillePolice * (0.05f + 0.07f * Hasard(k * 7 + 5)));
		const float Temps = AgePassage - Depart;
		if (Temps <= 0.f || PosX < -50.f || PosX > Ecran.X + 50.f)
		{
			continue;
		}
		const float Vitesse = TailleTexte.Y * (0.25f + 0.35f * Hasard(k * 11 + 7));
		const float Longueur = FMath::Min(LongueurMax, Temps * Vitesse);
		const float HautCoulure = Coin.Y + TailleTexte.Y * 0.72f;

		// Coulure qui s'affine vers le bas
		Boite(Sortie, Couche + 1, G, FVector2D(PosX - Largeur * 0.5f, HautCoulure), FVector2D(Largeur, Longueur * 0.6f), Sang * FLinearColor(1, 1, 1, Fondu));
		Boite(Sortie, Couche + 1, G, FVector2D(PosX - Largeur * 0.35f, HautCoulure + Longueur * 0.55f), FVector2D(Largeur * 0.7f, Longueur * 0.45f), Sang * FLinearColor(1, 1, 1, Fondu));
		// Goutte ronde au bout
		const float Bulle = Largeur * 1.5f;
		Boite(Sortie, Couche + 2, G, FVector2D(PosX - Bulle * 0.5f, HautCoulure + Longueur - Bulle * 0.3f), FVector2D(Bulle, Bulle * 1.15f), SangFonce * FLinearColor(1, 1, 1, Fondu));
		Boite(Sortie, Couche + 3, G, FVector2D(PosX - Bulle * 0.15f, HautCoulure + Longueur - Bulle * 0.1f), FVector2D(Bulle * 0.25f, Bulle * 0.3f), FLinearColor(1.f, 0.3f, 0.3f, 0.5f * Fondu));

		// Quand la coulure est au maximum, une goutte se detache et tombe
		const float TempsChute = Temps - LongueurMax / Vitesse;
		if (TempsChute > 0.f)
		{
			const float Chute = FMath::Fmod(TempsChute, 1.6f);
			const float Y = HautCoulure + LongueurMax + Chute * Chute * 0.5f * 900.f;
			if (Y < Ecran.Y)
			{
				Boite(Sortie, Couche + 2, G, FVector2D(PosX - Bulle * 0.4f, Y), FVector2D(Bulle * 0.8f, Bulle * 1.3f), Sang * FLinearColor(1, 1, 1, Fondu * (1.f - Chute / 1.6f)));
			}
		}
	}

	// Le texte : contour noir, ombre de sang, puis rouge sang
	const FPaintGeometry Geo = G.ToPaintGeometry(Coin, TailleTexte);
	const float Ep = FMath::Max(2.f, TaillePolice * 0.04f);
	const FVector2D Decalages[] = { FVector2D(-Ep, 0), FVector2D(Ep, 0), FVector2D(0, -Ep), FVector2D(0, Ep), FVector2D(Ep, Ep) };
	for (const FVector2D& D : Decalages)
	{
		FSlateDrawElement::MakeText(Sortie, Couche + 4, G.ToPaintGeometry(Coin + D, TailleTexte), Etat.Texte, Police,
			ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, 0.9f * Fondu));
	}
	FSlateDrawElement::MakeText(Sortie, Couche + 5, G.ToPaintGeometry(Coin + FVector2D(0.f, Ep * 1.5f), TailleTexte), Etat.Texte, Police,
		ESlateDrawEffect::None, SangFonce * FLinearColor(1, 1, 1, Fondu));
	const FLinearColor Rouge = FMath::Lerp(Sang, SangClair, 0.3f + 0.7f * Pulse);
	FSlateDrawElement::MakeText(Sortie, Couche + 6, Geo, Etat.Texte, Police, ESlateDrawEffect::None, Rouge * FLinearColor(1, 1, 1, Fondu));
}

void SAlerteZombies::PeindreCompteur(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle("Bold", (int32)FMath::Clamp(Ecran.Y * 0.03f, 14.f, 34.f));
	const int32 S = FMath::Max(0, FMath::CeilToInt(Etat.SecondesRestantes));
	const FString Ligne = FString::Printf(TEXT("ZOMBIES : %d     FIN DE LA PURGE : %d:%02d"), Etat.Restants, S / 60, S % 60);

	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Taille = Mesure->Measure(Ligne, Police);
	const FVector2D Coin(Ecran.X * 0.5f - Taille.X * 0.5f, Ecran.Y * 0.04f);

	Boite(Sortie, Couche, G, Coin - FVector2D(16.f, 6.f), Taille + FVector2D(32.f, 12.f), FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Boite(Sortie, Couche + 1, G, FVector2D(Coin.X - 16.f, Coin.Y + Taille.Y + 4.f), FVector2D(Taille.X + 32.f, 3.f), Sang);
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(Coin + FVector2D(2.f, 2.f), Taille), Ligne, Police, ESlateDrawEffect::None, FLinearColor::Black);
	FSlateDrawElement::MakeText(Sortie, Couche + 3, G.ToPaintGeometry(Coin, Taille), Ligne, Police, ESlateDrawEffect::None, SangClair);
}

void SAlerteZombies::PeindreFin(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G) const
{
	const FVector2D Ecran = G.GetLocalSize();
	const float Alpha = FMath::Clamp(Etat.AgeFin / 0.4f, 0.f, 1.f) * FMath::Clamp((6.f - Etat.AgeFin) / 1.f, 0.f, 1.f);
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle("Bold", (int32)FMath::Clamp(Ecran.Y * 0.06f, 22.f, 80.f));
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Taille = Mesure->Measure(Etat.MessageFin, Police);
	const FVector2D Coin(Ecran.X * 0.5f - Taille.X * 0.5f, Ecran.Y * 0.38f - Taille.Y * 0.5f);

	Boite(Sortie, Couche, G, FVector2D(0.f, Coin.Y - 10.f), FVector2D(Ecran.X, Taille.Y + 20.f), FLinearColor(0.f, 0.f, 0.f, 0.5f * Alpha));
	FSlateDrawElement::MakeText(Sortie, Couche + 1, G.ToPaintGeometry(Coin + FVector2D(3.f, 3.f), Taille), Etat.MessageFin, Police,
		ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Alpha));
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(Coin, Taille), Etat.MessageFin, Police,
		ESlateDrawEffect::None, FLinearColor(1.f, 0.85f, 0.75f, Alpha));
}
