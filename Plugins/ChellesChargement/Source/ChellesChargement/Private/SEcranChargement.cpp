// CHELLES 3D - Ecran de chargement (style lobby bleu, disposition inversee : barre VERTICALE a gauche)
// Tout est dessine a la main (aucun asset /Game) : l'ecran peut s'afficher pendant que la carte se charge.
#include "SEcranChargement.h"

#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Types/WidgetActiveTimerDelegate.h"

namespace EcranChelles
{
	// Memes couleurs que le lobby (SMenuChelles, theme bleu du 24/11)
	const FLinearColor CNuit(0.010f, 0.025f, 0.110f, 1.f);
	const FLinearColor CBleu(0.030f, 0.130f, 0.380f, 1.f);
	const FLinearColor CRoi(0.060f, 0.230f, 0.700f, 1.f);
	const FLinearColor CCyan(0.250f, 0.850f, 1.000f, 1.f);
	const FLinearColor CSombre(0.020f, 0.030f, 0.140f, 1.f);
	const FLinearColor CJaune(1.000f, 0.880f, 0.050f, 1.f);
	const FLinearColor CBlanc(0.960f, 0.970f, 1.000f, 1.f);

	FLinearColor AvecAlpha(const FLinearColor& C, float Alpha)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * Alpha);
	}

	FSlateFontInfo PoliceEcran(const TCHAR* Graisse, float TaillePolice)
	{
		return FCoreStyle::GetDefaultFontStyle(Graisse, FMath::Max(8, FMath::RoundToInt(TaillePolice)));
	}

	const FSlateBrush* PinceauBlanc()
	{
		return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	}

	FVector2D Mesurer(const FString& Chaine, const FSlateFontInfo& Police)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return FVector2D(Chaine.Len() * Police.Size * 0.62f, Police.Size * 1.35f);
		}
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Chaine, Police);
	}

	void DessinerBoite(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Dim, const FLinearColor& C)
	{
		FSlateDrawElement::MakeBox(Sortie, Couche, G.ToPaintGeometry(Pos, Dim), PinceauBlanc(), ESlateDrawEffect::None, C);
	}

	/** Geometrie d'une plaque penchee (comme les boutons du lobby). */
	FGeometry Penchee(const FGeometry& G, const FVector2D& Pos, const FVector2D& Dim, float Force)
	{
		return G.MakeChild(Dim, FSlateLayoutTransform(Pos), FSlateRenderTransform(FShear2D(-Force, 0.f)), FVector2D(0.5f, 0.5f));
	}

	void DessinerPlaque(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FVector2D& Dim, float Force, const FLinearColor& C)
	{
		FSlateDrawElement::MakeBox(Sortie, Couche, Penchee(G, Pos, Dim, Force).ToPaintGeometry(), PinceauBlanc(), ESlateDrawEffect::None, C);
	}

	void DessinerTexte(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const FVector2D& Pos, const FString& Chaine, const FSlateFontInfo& Police, const FLinearColor& C)
	{
		const FVector2D Dim = Mesurer(Chaine, Police);
		FSlateDrawElement::MakeText(Sortie, Couche, G.ToPaintGeometry(Pos, Dim), Chaine, Police, ESlateDrawEffect::None, C);
	}

	/** Coupe un texte trop long en deux lignes, au plus pres du milieu. */
	void CouperEnDeux(const FString& Chaine, FString& Ligne1, FString& Ligne2)
	{
		const int32 Milieu = Chaine.Len() / 2;
		int32 Meilleur = INDEX_NONE;
		for (int32 i = 0; i < Chaine.Len(); ++i)
		{
			if (Chaine[i] == TCHAR(' ') && (Meilleur == INDEX_NONE || FMath::Abs(i - Milieu) < FMath::Abs(Meilleur - Milieu)))
			{
				Meilleur = i;
			}
		}
		if (Meilleur == INDEX_NONE)
		{
			Ligne1 = Chaine;
			Ligne2.Empty();
			return;
		}
		Ligne1 = Chaine.Left(Meilleur);
		Ligne2 = Chaine.Mid(Meilleur + 1);
	}
}

void SEcranChargement::Construct(const FArguments& InArgs)
{
	NomCarte = FPaths::GetBaseFilename(InArgs._Carte);
	bSuperpose = InArgs._Superpose;
	const FReglagesChargement& Reglages = FReglagesChargement::Get();
	Astuces = Reglages.Astuces;
	SecondesParAstuce = FMath::Max(1.5f, Reglages.SecondesParAstuce);
	PremiereAstuce = Astuces.Num() > 0 ? FMath::RandRange(0, Astuces.Num() - 1) : 0;
	Debut = FPlatformTime::Seconds();
	if (bSuperpose)
	{
		SetVisibility(EVisibility::HitTestInvisible);
	}
	if (FSlateApplication::IsInitialized())
	{
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SEcranChargement::Animer));
	}
}

EActiveTimerReturnType SEcranChargement::Animer(double InCurrentTime, float InDeltaTime)
{
	return EstFini() ? EActiveTimerReturnType::Stop : EActiveTimerReturnType::Continue;
}

void SEcranChargement::Terminer()
{
	if (Fin < 0.0)
	{
		Fin = FPlatformTime::Seconds();
	}
}

bool SEcranChargement::EstFini() const
{
	return Fin >= 0.0 && FPlatformTime::Seconds() - Fin > 1.0;
}

float SEcranChargement::Avancement(double Maintenant) const
{
	const float Ecoule = float(Maintenant - Debut);
	const float Base = 0.92f * (1.f - FMath::Exp(-Ecoule / 4.f));
	if (Fin < 0.0)
	{
		return Base;
	}
	const float F = FMath::Clamp(float(Maintenant - Fin) / 0.35f, 0.f, 1.f);
	return FMath::Lerp(Base, 1.f, F);
}

float SEcranChargement::Opacite(double Maintenant) const
{
	if (Fin < 0.0)
	{
		return 1.f;
	}
	return 1.f - FMath::Clamp(float(Maintenant - Fin - 0.35) / 0.6f, 0.f, 1.f);
}

FVector2D SEcranChargement::ComputeDesiredSize(float) const
{
	return FVector2D(1920.f, 1080.f);
}

int32 SEcranChargement::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	namespace EC = EcranChelles;
	const FGeometry& G = AllottedGeometry;
	FSlateWindowElementList& Sortie = OutDrawElements;

	const double Maintenant = FPlatformTime::Seconds();
	const float Ecoule = float(Maintenant - Debut);
	const float A = Opacite(Maintenant) * InWidgetStyle.GetColorAndOpacityTint().A;
	if (A <= 0.001f)
	{
		return LayerId;
	}
	const FVector2D Dim = G.GetLocalSize();
	const float W = Dim.X;
	const float H = Dim.Y;
	if (W < 10.f || H < 10.f)
	{
		return LayerId;
	}
	const float S = FMath::Clamp(FMath::Min(H / 1080.f, W / 1600.f), 0.35f, 3.f);
	int32 Couche = LayerId;

	// ------------------------------------------------ 1. Fond bleu nuit + degrade + bandes penchees
	EC::DessinerBoite(Sortie, Couche, G, FVector2D::ZeroVector, Dim, EC::AvecAlpha(EC::CNuit, A));
	{
		TArray<FSlateGradientStop> Arrets;
		Arrets.Add(FSlateGradientStop(FVector2D(0.f, 0.f), EC::AvecAlpha(EC::CBleu, A)));
		Arrets.Add(FSlateGradientStop(FVector2D(W, H), EC::AvecAlpha(EC::CNuit, 0.f)));
		FSlateDrawElement::MakeGradient(Sortie, ++Couche, G.ToPaintGeometry(), Arrets, Orient_Vertical);
	}
	const float BandeX = W * 0.05f;
	const float BandeL = W * 0.44f;
	EC::DessinerPlaque(Sortie, ++Couche, G, FVector2D(BandeX, -H * 0.1f), FVector2D(BandeL, H * 1.2f), 0.22f, EC::AvecAlpha(EC::CRoi, 0.50f * A));
	EC::DessinerPlaque(Sortie, ++Couche, G, FVector2D(BandeX + BandeL + 18.f * S, -H * 0.1f), FVector2D(10.f * S, H * 1.2f), 0.22f, EC::AvecAlpha(EC::CJaune, 0.85f * A));
	EC::DessinerPlaque(Sortie, ++Couche, G, FVector2D(BandeX + BandeL + 44.f * S, -H * 0.1f), FVector2D(3.f * S, H * 1.2f), 0.22f, EC::AvecAlpha(EC::CCyan, 0.55f * A));
	{
		// reflet lumineux qui passe lentement
		const float Cycle = FMath::Fmod(Ecoule * 0.22f, 1.5f);
		EC::DessinerPlaque(Sortie, ++Couche, G, FVector2D(Cycle * W - W * 0.25f, -H * 0.1f), FVector2D(W * 0.08f, H * 1.2f), 0.22f, EC::AvecAlpha(EC::CBlanc, 0.05f * A));
	}

	// ------------------------------------------------ 2. Barre de chargement VERTICALE a gauche
	const float P = Avancement(Maintenant);
	const FVector2D BarrePos(60.f * S, H * 0.14f);
	const FVector2D BarreDim(16.f * S, H * 0.66f);
	EC::DessinerBoite(Sortie, ++Couche, G, BarrePos - FVector2D(3.f * S, 3.f * S), BarreDim + FVector2D(6.f * S, 6.f * S), EC::AvecAlpha(EC::CBlanc, 0.30f * A));
	EC::DessinerBoite(Sortie, ++Couche, G, BarrePos, BarreDim, EC::AvecAlpha(EC::CSombre, 0.95f * A));
	{
		const float Haut = BarreDim.Y * P;
		EC::DessinerBoite(Sortie, ++Couche, G, FVector2D(BarrePos.X, BarrePos.Y + BarreDim.Y - Haut), FVector2D(BarreDim.X, Haut), EC::AvecAlpha(EC::CJaune, A));
		EC::DessinerBoite(Sortie, ++Couche, G, FVector2D(BarrePos.X, BarrePos.Y + BarreDim.Y - Haut), FVector2D(BarreDim.X, 3.f * S), EC::AvecAlpha(EC::CBlanc, 0.9f * A));
		for (int32 k = 1; k < 10; ++k)
		{
			const float Y = BarrePos.Y + BarreDim.Y * (1.f - k / 10.f);
			EC::DessinerBoite(Sortie, Couche, G, FVector2D(BarrePos.X + BarreDim.X + 6.f * S, Y), FVector2D((k == 5 ? 12.f : 7.f) * S, 2.f * S), EC::AvecAlpha(EC::CCyan, 0.6f * A));
		}
		const FString Pourcent = FString::Printf(TEXT("%d %%"), FMath::RoundToInt(P * 100.f));
		EC::DessinerTexte(Sortie, ++Couche, G, FVector2D(BarrePos.X - 6.f * S, BarrePos.Y - 44.f * S), Pourcent, EC::PoliceEcran(TEXT("Black"), 22.f * S), EC::AvecAlpha(EC::CJaune, A));
	}
	{
		// petite roue qui tourne sous la barre
		const FVector2D Centre(BarrePos.X + BarreDim.X * 0.5f, BarrePos.Y + BarreDim.Y + 52.f * S);
		const int32 Tour = int32(Ecoule * 12.f);
		for (int32 k = 0; k < 8; ++k)
		{
			const float Angle = k * 2.f * PI / 8.f;
			const FVector2D Point = Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * 18.f * S;
			const float Lueur = 0.2f + 0.8f * float((k - Tour) & 7) / 7.f;
			EC::DessinerBoite(Sortie, Couche, G, Point - FVector2D(3.f * S, 3.f * S), FVector2D(6.f * S, 6.f * S), EC::AvecAlpha(EC::CJaune, Lueur * A));
		}
	}

	// ------------------------------------------------ 3. Titre CHELLES 3D + plaque jaune
	const float X0 = 150.f * S;
	const float YTitre = H * 0.16f;
	const FString Titre = TEXT("CHELLES 3D");
	const FSlateFontInfo PoliceTitre = EC::PoliceEcran(TEXT("Black"), 118.f * S);
	const FVector2D DimTitre = EC::Mesurer(Titre, PoliceTitre);
	EC::DessinerTexte(Sortie, ++Couche, G, FVector2D(X0 + 7.f * S, YTitre + 7.f * S), Titre, PoliceTitre, EC::AvecAlpha(EC::CSombre, 0.85f * A));
	EC::DessinerTexte(Sortie, ++Couche, G, FVector2D(X0, YTitre), Titre, PoliceTitre, EC::AvecAlpha(EC::CBlanc, A));

	const FString SousTitre = bSuperpose ? FString(TEXT("ARRIVÉE DANS LE QUARTIER")) : FString(TEXT("CHARGEMENT DU QUARTIER"));
	const FSlateFontInfo PoliceSous = EC::PoliceEcran(TEXT("Black"), 30.f * S);
	const FVector2D DimSous = EC::Mesurer(SousTitre, PoliceSous);
	const FVector2D PosSous(X0 + 10.f * S, YTitre + DimTitre.Y + 2.f * S);
	const FVector2D DimPlaqueSous(DimSous.X + 60.f * S, DimSous.Y + 16.f * S);
	{
		const FGeometry Plaque = EC::Penchee(G, PosSous, DimPlaqueSous, 0.18f);
		FSlateDrawElement::MakeBox(Sortie, ++Couche, Plaque.ToPaintGeometry(), EC::PinceauBlanc(), ESlateDrawEffect::None, EC::AvecAlpha(EC::CJaune, A));
		EC::DessinerTexte(Sortie, ++Couche, Plaque, FVector2D(30.f * S, 8.f * S), SousTitre, PoliceSous, EC::AvecAlpha(EC::CSombre, A));
	}
	{
		const FString Points = FString::ChrN(1 + (int32(Ecoule * 2.5f) % 3), TCHAR('.'));
		const FString Ligne = (NomCarte.IsEmpty() ? FString(TEXT("Préparation des rues de Chelles")) : FString(TEXT("Carte : ")) + NomCarte) + Points;
		EC::DessinerTexte(Sortie, ++Couche, G, FVector2D(X0 + 14.f * S, PosSous.Y + DimPlaqueSous.Y + 20.f * S), Ligne, EC::PoliceEcran(TEXT("Bold"), 22.f * S), EC::AvecAlpha(EC::CCyan, A));
	}

	// ------------------------------------------------ 4. Plaque blanche penchee : l'astuce
	if (Astuces.Num() > 0)
	{
		const int32 Rang = int32(Ecoule / SecondesParAstuce);
		const int32 Indice = (PremiereAstuce + Rang) % Astuces.Num();
		const float Local = Ecoule - Rang * SecondesParAstuce;
		const float Fondu = FMath::Clamp(Local / 0.35f, 0.f, 1.f) * FMath::Clamp((SecondesParAstuce - Local) / 0.35f, 0.f, 1.f);
		const FAstuceChelles& Astuce = Astuces[Indice];

		const FVector2D PosPlaque(W * 0.33f, H * 0.68f);
		const FVector2D DimPlaque(W * 0.60f, 150.f * S);
		EC::DessinerPlaque(Sortie, ++Couche, G, PosPlaque + FVector2D(10.f * S, 10.f * S), DimPlaque, 0.18f, EC::AvecAlpha(EC::CSombre, 0.65f * A));
		const FGeometry Plaque = EC::Penchee(G, PosPlaque, DimPlaque, 0.18f);
		FSlateDrawElement::MakeBox(Sortie, ++Couche, Plaque.ToPaintGeometry(), EC::PinceauBlanc(), ESlateDrawEffect::None, EC::AvecAlpha(EC::CBlanc, A));
		EC::DessinerBoite(Sortie, ++Couche, Plaque, FVector2D(0.f, DimPlaque.Y - 6.f * S), FVector2D(DimPlaque.X, 6.f * S), EC::AvecAlpha(EC::CRoi, A));

		// etiquette jaune "ASTUCE 3/15" au-dessus
		{
			const FString Etiquette = FString::Printf(TEXT("ASTUCE  %d/%d"), Indice + 1, Astuces.Num());
			const FSlateFontInfo PoliceEtiquette = EC::PoliceEcran(TEXT("Black"), 20.f * S);
			const FVector2D DimEtiquette = EC::Mesurer(Etiquette, PoliceEtiquette);
			const FVector2D DimPlaqueEtiquette(DimEtiquette.X + 40.f * S, DimEtiquette.Y + 10.f * S);
			const FGeometry PlaqueEtiquette = EC::Penchee(G, PosPlaque + FVector2D(36.f * S, -DimPlaqueEtiquette.Y + 8.f * S), DimPlaqueEtiquette, 0.18f);
			FSlateDrawElement::MakeBox(Sortie, ++Couche, PlaqueEtiquette.ToPaintGeometry(), EC::PinceauBlanc(), ESlateDrawEffect::None, EC::AvecAlpha(EC::CJaune, A));
			EC::DessinerTexte(Sortie, ++Couche, PlaqueEtiquette, FVector2D(20.f * S, 5.f * S), Etiquette, PoliceEtiquette, EC::AvecAlpha(EC::CSombre, A));
		}

		float Curseur = 44.f * S;
		const float AContenu = A * Fondu;
		if (!Astuce.Touche.IsEmpty())
		{
			const FSlateFontInfo PoliceTouche = EC::PoliceEcran(TEXT("Black"), (Astuce.Touche.Len() > 3 ? 24.f : 36.f) * S);
			const FVector2D DimTouche = EC::Mesurer(Astuce.Touche, PoliceTouche);
			const FVector2D DimPastille(FMath::Max(88.f * S, DimTouche.X + 40.f * S), 88.f * S);
			const FVector2D PosPastille(Curseur, (DimPlaque.Y - DimPastille.Y) * 0.5f);
			EC::DessinerBoite(Sortie, ++Couche, Plaque, PosPastille, DimPastille, EC::AvecAlpha(EC::CSombre, AContenu));
			EC::DessinerBoite(Sortie, ++Couche, Plaque, PosPastille + FVector2D(0.f, DimPastille.Y - 5.f * S), FVector2D(DimPastille.X, 5.f * S), EC::AvecAlpha(EC::CJaune, AContenu));
			EC::DessinerTexte(Sortie, ++Couche, Plaque, PosPastille + (DimPastille - DimTouche) * 0.5f, Astuce.Touche, PoliceTouche, EC::AvecAlpha(EC::CJaune, AContenu));
			Curseur += DimPastille.X + 30.f * S;
		}

		const float Largeur = DimPlaque.X - Curseur - 40.f * S;
		FString Ligne1 = Astuce.Texte;
		FString Ligne2;
		float TaillePolice = 30.f * S;
		FSlateFontInfo PoliceAstuce = EC::PoliceEcran(TEXT("Bold"), TaillePolice);
		while (EC::Mesurer(Ligne1, PoliceAstuce).X > Largeur && TaillePolice > 21.f * S)
		{
			TaillePolice -= 2.f * S;
			PoliceAstuce = EC::PoliceEcran(TEXT("Bold"), TaillePolice);
		}
		if (EC::Mesurer(Ligne1, PoliceAstuce).X > Largeur)
		{
			EC::CouperEnDeux(Astuce.Texte, Ligne1, Ligne2);
			TaillePolice = 28.f * S;
			PoliceAstuce = EC::PoliceEcran(TEXT("Bold"), TaillePolice);
			while ((EC::Mesurer(Ligne1, PoliceAstuce).X > Largeur || EC::Mesurer(Ligne2, PoliceAstuce).X > Largeur) && TaillePolice > 14.f * S)
			{
				TaillePolice -= 2.f * S;
				PoliceAstuce = EC::PoliceEcran(TEXT("Bold"), TaillePolice);
			}
		}
		const FVector2D Dim1 = EC::Mesurer(Ligne1, PoliceAstuce);
		const float HautTexte = Ligne2.IsEmpty() ? Dim1.Y : Dim1.Y * 2.f;
		const float Y0 = (DimPlaque.Y - HautTexte) * 0.5f - 3.f * S;
		EC::DessinerTexte(Sortie, ++Couche, Plaque, FVector2D(Curseur, Y0), Ligne1, PoliceAstuce, EC::AvecAlpha(EC::CSombre, AContenu));
		if (!Ligne2.IsEmpty())
		{
			EC::DessinerTexte(Sortie, Couche, Plaque, FVector2D(Curseur, Y0 + Dim1.Y), Ligne2, PoliceAstuce, EC::AvecAlpha(EC::CSombre, AContenu));
		}
	}

	// ------------------------------------------------ 5. Pied de page
	EC::DessinerTexte(Sortie, ++Couche, G, FVector2D(X0 + 14.f * S, H - 58.f * S), TEXT("chelles3d.fr"), EC::PoliceEcran(TEXT("Bold"), 16.f * S), EC::AvecAlpha(EC::CBlanc, 0.45f * A));

	return Couche;
}
