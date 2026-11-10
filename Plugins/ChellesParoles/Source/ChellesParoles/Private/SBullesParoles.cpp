// CHELLES 3D - Bulles noires (texte blanc) au-dessus des passants qui parlent.
#include "SBullesParoles.h"

#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Coupe une phrase trop longue en plusieurs lignes (environ Max caracteres par ligne). */
	TArray<FString> CouperEnLignes(const FString& Texte, int32 Max)
	{
		TArray<FString> Mots;
		Texte.ParseIntoArrayWS(Mots);
		TArray<FString> Lignes;
		FString Ligne;
		for (const FString& Mot : Mots)
		{
			if (!Ligne.IsEmpty() && Ligne.Len() + 1 + Mot.Len() > Max)
			{
				Lignes.Add(Ligne);
				Ligne.Reset();
			}
			Ligne += Ligne.IsEmpty() ? Mot : TEXT(" ") + Mot;
		}
		if (!Ligne.IsEmpty())
		{
			Lignes.Add(Ligne);
		}
		return Lignes;
	}
}

void SBullesParoles::Construct(const FArguments& InArgs)
{
	// Les bulles ne doivent jamais attraper la souris ni les clics.
	SetVisibility(EVisibility::HitTestInvisible);
}

void SBullesParoles::Ajouter(AActor* Passant, const FString& Texte, float Hauteur, double Maintenant, float Duree)
{
	Bulles.RemoveAll([Passant](const FBulleParole& B) { return B.Passant.Get() == Passant; });
	FBulleParole B;
	B.Passant = Passant;
	B.Texte = Texte;
	B.Hauteur = Hauteur;
	B.Debut = Maintenant;
	B.Fin = Maintenant + Duree;
	Bulles.Add(B);
}

void SBullesParoles::Nettoyer(double Maintenant)
{
	Bulles.RemoveAll([Maintenant](const FBulleParole& B) { return !B.Passant.IsValid() || Maintenant > B.Fin; });
}

void SBullesParoles::Polygone(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G,
	const TArray<FVector2D>& Points, const FLinearColor& Couleur) const
{
	if (Points.Num() < 3)
	{
		return;
	}
	const FSlateBrush* Blanc = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FSlateResourceHandle Ressource = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Blanc);
	const FColor C = Couleur.ToFColor(true);

	TArray<FSlateVertex> Sommets;
	TArray<SlateIndex> Indices;
	for (const FVector2D& P : Points)
	{
		FSlateVertex V;
		FMemory::Memzero(&V, sizeof(FSlateVertex));
		V.TexCoords[2] = 1.f;
		V.TexCoords[3] = 1.f;
		V.Position = G.LocalToAbsolute(P); // coordonnees fenetre
		V.Color = C;
		Sommets.Add(V);
	}
	// Polygone convexe : un eventail de triangles depuis le premier point.
	for (int32 i = 1; i + 1 < Points.Num(); ++i)
	{
		Indices.Add(0);
		Indices.Add(SlateIndex(i));
		Indices.Add(SlateIndex(i + 1));
	}
	FSlateDrawElement::MakeCustomVerts(Sortie, Couche, Ressource, Sommets, Indices, nullptr, 0, 0);
}

void SBullesParoles::Rectangle(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G,
	const FVector2D& Coin, const FVector2D& Taille, float Rayon, const FLinearColor& Couleur) const
{
	// Rectangle aux coins arrondis, dessine comme un seul polygone (pas de superposition = opacite uniforme).
	const float R = FMath::Min(Rayon, FMath::Min(Taille.X, Taille.Y) * 0.5f);
	const FVector2D Centres[4] = {
		FVector2D(Coin.X + Taille.X - R, Coin.Y + R),            // haut droit
		FVector2D(Coin.X + Taille.X - R, Coin.Y + Taille.Y - R), // bas droit
		FVector2D(Coin.X + R, Coin.Y + Taille.Y - R),            // bas gauche
		FVector2D(Coin.X + R, Coin.Y + R)                        // haut gauche
	};
	TArray<FVector2D> Points;
	const int32 Pas = 6;
	for (int32 c = 0; c < 4; ++c)
	{
		const float Depart = -HALF_PI + c * HALF_PI;
		for (int32 i = 0; i <= Pas; ++i)
		{
			const float A = Depart + HALF_PI * float(i) / Pas;
			Points.Add(Centres[c] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
	}
	Polygone(Sortie, Couche, G, Points, Couleur);
}

int32 SBullesParoles::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	APlayerController* PC = Joueur.Get();
	UWorld* W = Monde.Get();
	if (!PC || !W || Bulles.Num() == 0)
	{
		return Couche;
	}

	int32 LargeurVue = 0, HauteurVue = 0;
	PC->GetViewportSize(LargeurVue, HauteurVue);
	if (LargeurVue <= 0 || HauteurVue <= 0)
	{
		return Couche;
	}
	// Pixels de l'ecran -> unites Slate (tient compte de l'echelle d'affichage).
	const FVector2D Taille = G.GetLocalSize();
	const FVector2D Echelle(Taille.X / LargeurVue, Taille.Y / HauteurVue);

	FVector PosCamera;
	FRotator RotCamera;
	PC->GetPlayerViewPoint(PosCamera, RotCamera);

	const double Maintenant = W->GetTimeSeconds();
	const FSlateFontInfo Police = FCoreStyle::GetDefaultFontStyle("Bold", TaillePolice);
	const TSharedRef<FSlateFontMeasure> Mesure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D Marge(12.f, 7.f);
	const float Pointe = 10.f;

	// Les bulles les plus lointaines d'abord, pour que les proches passent devant.
	TArray<const FBulleParole*> Ordre;
	for (const FBulleParole& B : Bulles)
	{
		if (B.Passant.IsValid() && Maintenant <= B.Fin)
		{
			Ordre.Add(&B);
		}
	}
	Ordre.Sort([&PosCamera](const FBulleParole& A, const FBulleParole& B)
	{
		return FVector::DistSquared(A.Passant->GetActorLocation(), PosCamera) > FVector::DistSquared(B.Passant->GetActorLocation(), PosCamera);
	});

	int32 CoucheMax = Couche;
	int32 CoucheBulle = Couche;
	for (const FBulleParole* B : Ordre)
	{
		const FVector Tete = B->Passant->GetActorLocation() + FVector(0.f, 0.f, B->Hauteur);
		const float Distance = FVector::Dist(Tete, PosCamera);
		if (Distance > DistanceMax)
		{
			continue;
		}
		FVector2D Ecran;
		if (!PC->ProjectWorldLocationToScreen(Tete, Ecran, false))
		{
			continue; // derriere la camera
		}
		const FVector2D Pointeur = Ecran * Echelle;

		// Apparition / disparition en douceur.
		const float Alpha = FMath::Clamp(float(FMath::Min(Maintenant - B->Debut, B->Fin - Maintenant)) / 0.2f, 0.f, 1.f);
		if (Alpha <= 0.f)
		{
			continue;
		}

		const TArray<FString> Lignes = CouperEnLignes(B->Texte, 32);
		TArray<FVector2D> TaillesLignes;
		FVector2D TailleTexte(0.f, 0.f);
		for (const FString& L : Lignes)
		{
			const FVector2D T = Mesure->Measure(L, Police);
			TaillesLignes.Add(T);
			TailleTexte.X = FMath::Max(TailleTexte.X, T.X);
			TailleTexte.Y += T.Y;
		}
		const FVector2D TailleBulle = TailleTexte + Marge * 2.f;
		const FVector2D Coin(Pointeur.X - TailleBulle.X * 0.5f, Pointeur.Y - Pointe - TailleBulle.Y);

		// Hors de l'ecran : on ne dessine pas.
		if (Coin.X > Taille.X || Coin.Y > Taille.Y || Coin.X + TailleBulle.X < 0.f || Coin.Y + TailleBulle.Y < 0.f)
		{
			continue;
		}

		const FLinearColor Noir(0.f, 0.f, 0.f, Opacite * Alpha);
		Rectangle(Sortie, CoucheBulle, G, Coin, TailleBulle, 10.f, Noir);

		// Petite pointe vers la tete du passant.
		TArray<FVector2D> Triangle;
		Triangle.Add(FVector2D(Pointeur.X - 8.f, Coin.Y + TailleBulle.Y));
		Triangle.Add(FVector2D(Pointeur.X + 8.f, Coin.Y + TailleBulle.Y));
		Triangle.Add(Pointeur);
		Polygone(Sortie, CoucheBulle, G, Triangle, Noir);

		// Texte blanc, centre, ligne par ligne.
		float Y = Coin.Y + Marge.Y;
		for (int32 i = 0; i < Lignes.Num(); ++i)
		{
			const FVector2D Position(Pointeur.X - TaillesLignes[i].X * 0.5f, Y);
			FSlateDrawElement::MakeText(Sortie, CoucheBulle + 1, G.ToPaintGeometry(Position, TaillesLignes[i]), Lignes[i], Police,
				ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, Alpha));
			Y += TaillesLignes[i].Y;
		}

		CoucheMax = CoucheBulle + 1;
		CoucheBulle += 2;
	}
	return CoucheMax;
}
