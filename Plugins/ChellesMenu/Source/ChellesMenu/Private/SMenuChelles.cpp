// CHELLES 3D - Dessin du menu (Slate, sans asset), style cyberpunk
#include "SMenuChelles.h"

#include "MenuChelles.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// ---------------------------------------------------------------- Couleurs et polices

namespace
{
	const FLinearColor Cyan(FColor(0, 240, 255));
	const FLinearColor Magenta(FColor(255, 0, 170));
	const FLinearColor Violet(FColor(120, 40, 255));
	const FLinearColor Blanc(FColor(235, 250, 255));
	const FLinearColor Gris(FColor(90, 110, 125));
	const FLinearColor Fond(FColor(5, 7, 18, 225));

	FLinearColor Alpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, C.A * A);
	}

	FSlateFontInfo Police(int32 Taille, const FLinearColor& Halo, const TCHAR* Graisse = TEXT("Black"))
	{
		FSlateFontInfo P = FCoreStyle::GetDefaultFontStyle(Graisse, Taille);
		P.OutlineSettings.OutlineSize = FMath::Max(1, Taille / 22);
		P.OutlineSettings.OutlineColor = Alpha(Halo, 0.75f);
		return P;
	}

	float Horloge()
	{
		double T = FPlatformTime::Seconds();
		T -= FMath::FloorToDouble(T / 3600.0) * 3600.0;
		return float(T);
	}

	void Ligne(FSlateWindowElementList& Sortie, int32 Couche, const FGeometry& G, const TArray<FVector2D>& Points,
		const FLinearColor& Couleur, float Epaisseur)
	{
		FSlateDrawElement::MakeLines(Sortie, Couche, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Couleur, true, Epaisseur);
	}

	/** Contour a coins coupes (haut-gauche et bas-droite), facon interface futuriste. */
	TArray<FVector2D> Chanfrein(const FVector2D& T, float Coin)
	{
		TArray<FVector2D> P;
		P.Add(FVector2D(Coin, 0.f));
		P.Add(FVector2D(T.X, 0.f));
		P.Add(FVector2D(T.X, T.Y - Coin));
		P.Add(FVector2D(T.X - Coin, T.Y));
		P.Add(FVector2D(0.f, T.Y));
		P.Add(FVector2D(0.f, Coin));
		P.Add(FVector2D(Coin, 0.f));
		return P;
	}
}

// ---------------------------------------------------------------- Reglages (OPTIONS)

namespace ChellesOptions
{
	static const TCHAR* Section = TEXT("ChellesMenu");

	static float LireFloat(const TCHAR* Cle, float ParDefaut)
	{
		float V = ParDefaut;
		if (GConfig)
		{
			GConfig->GetFloat(Section, Cle, V, GGameUserSettingsIni);
		}
		return V;
	}

	static void EcrireFloat(const TCHAR* Cle, float V)
	{
		if (GConfig)
		{
			GConfig->SetFloat(Section, Cle, V, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static bool LireBool(const TCHAR* Cle, bool ParDefaut)
	{
		bool V = ParDefaut;
		if (GConfig)
		{
			GConfig->GetBool(Section, Cle, V, GGameUserSettingsIni);
		}
		return V;
	}

	static void EcrireBool(const TCHAR* Cle, bool V)
	{
		if (GConfig)
		{
			GConfig->SetBool(Section, Cle, V, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static UGameUserSettings* Ecran()
	{
		return GEngine ? GEngine->GetGameUserSettings() : nullptr;
	}

	static TWeakObjectPtr<UWorld> GMonde;
	static TWeakObjectPtr<APlayerController> GJoueur;

	// --- Audio
	static float Volume() { return FMath::Clamp(LireFloat(TEXT("VolumeGeneral"), 1.f), 0.f, 1.f); }
	static bool SonCoupe() { return LireBool(TEXT("SonCoupe"), false); }

	static void AppliquerVolume()
	{
		const float V = SonCoupe() ? 0.f : Volume();
		FAudioDevice* Audio = GMonde.IsValid() ? GMonde->GetAudioDeviceRaw() : nullptr;
		if (!Audio && GEngine)
		{
			Audio = GEngine->GetMainAudioDeviceRaw();
		}
		if (Audio)
		{
			Audio->SetTransientMasterVolume(V);
		}
	}

	// --- Souris / manette
	static float Sensibilite() { return FMath::Clamp(LireFloat(TEXT("SensibiliteSouris"), 1.f), 0.1f, 5.f); }
	static bool InverserSouris() { return LireBool(TEXT("InverserSouris"), false); }
	static bool InverserManette() { return LireBool(TEXT("InverserManette"), false); }
	static bool Qwerty() { return LireBool(TEXT("ClavierQwerty"), false); }

	/** AZERTY : Z/Q pour avancer/aller a gauche.  QWERTY : W/A a la place (S et D ne bougent pas). */
	static void Disposition(UPlayerInput* Entrees, bool bQwerty)
	{
		const FKey Avant = bQwerty ? EKeys::Z : EKeys::W;
		const FKey Gauche = bQwerty ? EKeys::Q : EKeys::A;
		const FKey NouvelAvant = bQwerty ? EKeys::W : EKeys::Z;
		const FKey NouvelleGauche = bQwerty ? EKeys::A : EKeys::Q;

		TArray<FInputAxisKeyMapping> AChanger;
		for (const FInputAxisKeyMapping& M : Entrees->AxisMappings)
		{
			if (M.Key == Avant || M.Key == Gauche)
			{
				AChanger.Add(M);
			}
		}
		if (AChanger.Num() == 0)
		{
			return;
		}
		for (const FInputAxisKeyMapping& M : AChanger)
		{
			Entrees->RemoveAxisMapping(M);
			FInputAxisKeyMapping Nouveau = M;
			Nouveau.Key = (M.Key == Avant) ? NouvelAvant : NouvelleGauche;
			Entrees->AddAxisMapping(Nouveau);
		}
		Entrees->ForceRebuildingKeyMaps(false);
	}

	static void Inversion(UPlayerInput* Entrees, const FKey& Touche, bool bVoulu)
	{
		if (Entrees && Entrees->GetInvertAxisKey(Touche) != bVoulu)
		{
			Entrees->InvertAxisKey(Touche);
		}
	}

	void AppliquerAuJoueur(APlayerController* Joueur)
	{
		if (!Joueur)
		{
			return;
		}
		GJoueur = Joueur;
		if (UPlayerInput* Entrees = Joueur->PlayerInput)
		{
			Entrees->SetMouseSensitivity(Sensibilite());
			Inversion(Entrees, EKeys::MouseY, InverserSouris());
			Inversion(Entrees, EKeys::Gamepad_RightY, InverserManette());
			Disposition(Entrees, Qwerty());
		}
	}

	void AppliquerAuDemarrage(UWorld* Monde)
	{
		GMonde = Monde;
		AppliquerVolume();
	}

	bool ChatActif() { return LireBool(TEXT("Chat"), false); }
	bool Manette() { return LireBool(TEXT("Manette"), false); }
	int32 Difficulte() { return FMath::Clamp(FMath::RoundToInt(LireFloat(TEXT("Difficulte"), 1.f)), 0, 3); }

	static int32 Boucle(int32 Valeur, int32 Sens, int32 Nombre)
	{
		return ((Valeur + Sens) % Nombre + Nombre) % Nombre;
	}

	static FText Ligne(const TCHAR* Nom, const FString& Valeur)
	{
		return FText::FromString(FString::Printf(TEXT("%s   < %s >"), Nom, *Valeur));
	}

	// --- Affichage & graphisme
	static FText TexteQualite()
	{
		static const TCHAR* Noms[] = { TEXT("BAS"), TEXT("MOYEN"), TEXT("\u00C9LEV\u00C9"), TEXT("\u00C9PIQUE") };
		UGameUserSettings* R = Ecran();
		const int32 N = R ? R->GetOverallScalabilityLevel() : -1;
		return Ligne(TEXT("QUALIT\u00C9"), (N >= 0 && N <= 3) ? Noms[N] : TEXT("PERSONNALIS\u00C9E"));
	}

	static void ChangerQualite(int32 Sens)
	{
		if (UGameUserSettings* R = Ecran())
		{
			int32 N = R->GetOverallScalabilityLevel();
			N = Boucle(N < 0 ? 2 : N, Sens, 4);
			R->SetOverallScalabilityLevel(N);
			R->ApplySettings(false);
		}
	}

	static const EWindowMode::Type ModesFenetre[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	static FText TexteAffichage()
	{
		UGameUserSettings* R = Ecran();
		const EWindowMode::Type M = R ? R->GetFullscreenMode() : EWindowMode::Windowed;
		const TCHAR* Nom = M == EWindowMode::Fullscreen ? TEXT("PLEIN \u00C9CRAN")
			: M == EWindowMode::WindowedFullscreen ? TEXT("PLEIN \u00C9CRAN FEN\u00CATR\u00C9") : TEXT("FEN\u00CATRE");
		return Ligne(TEXT("AFFICHAGE"), Nom);
	}

	static void ChangerAffichage(int32 Sens)
	{
		if (UGameUserSettings* R = Ecran())
		{
			int32 Index = 0;
			for (int32 i = 0; i < 3; ++i)
			{
				if (ModesFenetre[i] == R->GetFullscreenMode())
				{
					Index = i;
				}
			}
			R->SetFullscreenMode(ModesFenetre[Boucle(Index, Sens, 3)]);
			R->ApplySettings(false);
		}
	}

	static TArray<FIntPoint> Resolutions()
	{
		TArray<FIntPoint> Liste;
		const FIntPoint Toutes[] = { FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160) };
		UGameUserSettings* R = Ecran();
		const FIntPoint Bureau = R ? R->GetDesktopResolution() : FIntPoint(1920, 1080);
		for (const FIntPoint& P : Toutes)
		{
			if (P.X <= Bureau.X && P.Y <= Bureau.Y)
			{
				Liste.Add(P);
			}
		}
		Liste.AddUnique(Bureau);
		Liste.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X * A.Y < B.X * B.Y; });
		return Liste;
	}

	static FText TexteResolution()
	{
		UGameUserSettings* R = Ecran();
		const FIntPoint P = R ? R->GetScreenResolution() : FIntPoint::ZeroValue;
		return Ligne(TEXT("R\u00C9SOLUTION"), FString::Printf(TEXT("%d x %d"), P.X, P.Y));
	}

	static void ChangerResolution(int32 Sens)
	{
		UGameUserSettings* R = Ecran();
		const TArray<FIntPoint> Liste = Resolutions();
		if (!R || Liste.Num() == 0)
		{
			return;
		}
		int32 Index = Liste.IndexOfByKey(R->GetScreenResolution());
		Index = Index == INDEX_NONE ? Liste.Num() - 1 : Boucle(Index, Sens, Liste.Num());
		R->SetScreenResolution(Liste[Index]);
		R->ApplySettings(false);
	}

	static FText TexteVSync()
	{
		UGameUserSettings* R = Ecran();
		return Ligne(TEXT("SYNCHRO VERTICALE"), (R && R->IsVSyncEnabled()) ? TEXT("OUI") : TEXT("NON"));
	}

	static void ChangerVSync(int32)
	{
		if (UGameUserSettings* R = Ecran())
		{
			R->SetVSyncEnabled(!R->IsVSyncEnabled());
			R->ApplySettings(false);
		}
	}

	static const float LimitesFPS[] = { 30.f, 60.f, 120.f, 144.f, 0.f };

	static FText TexteFPS()
	{
		UGameUserSettings* R = Ecran();
		const float L = R ? R->GetFrameRateLimit() : 0.f;
		return Ligne(TEXT("IMAGES / SECONDE"), L <= 0.f ? FString(TEXT("ILLIMIT\u00C9")) : FString::Printf(TEXT("%d"), FMath::RoundToInt(L)));
	}

	static void ChangerFPS(int32 Sens)
	{
		if (UGameUserSettings* R = Ecran())
		{
			int32 Index = 4;
			for (int32 i = 0; i < 5; ++i)
			{
				if (FMath::IsNearlyEqual(LimitesFPS[i], R->GetFrameRateLimit()))
				{
					Index = i;
				}
			}
			R->SetFrameRateLimit(LimitesFPS[Boucle(Index, Sens, 5)]);
			R->ApplySettings(false);
		}
	}

	// --- Audio
	static FText TexteVolume()
	{
		return Ligne(TEXT("VOLUME G\u00C9N\u00C9RAL"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Volume() * 100.f)));
	}

	static void ChangerVolume(int32 Sens)
	{
		int32 Pas = FMath::RoundToInt(Volume() * 10.f);
		Pas = Boucle(Pas, Sens, 11);
		EcrireFloat(TEXT("VolumeGeneral"), Pas / 10.f);
		AppliquerVolume();
	}

	static FText TexteSon()
	{
		return Ligne(TEXT("SON"), SonCoupe() ? TEXT("COUP\u00C9") : TEXT("ACTIV\u00C9"));
	}

	static void ChangerSon(int32)
	{
		EcrireBool(TEXT("SonCoupe"), !SonCoupe());
		AppliquerVolume();
	}

	// --- Clavier / joystick
	static FText TexteSensibilite()
	{
		return Ligne(TEXT("SENSIBILIT\u00C9 SOURIS"), FString::Printf(TEXT("%.1f"), Sensibilite()));
	}

	static void ChangerSensibilite(int32 Sens)
	{
		int32 Pas = FMath::RoundToInt(Sensibilite() * 10.f) / 2; // pas de 0.2
		Pas = FMath::Clamp(Pas + Sens, 1, 15);
		EcrireFloat(TEXT("SensibiliteSouris"), Pas * 0.2f);
		AppliquerAuJoueur(GJoueur.Get());
	}

	static FText TexteInverserSouris()
	{
		return Ligne(TEXT("SOURIS HAUT/BAS INVERS\u00C9"), InverserSouris() ? TEXT("OUI") : TEXT("NON"));
	}

	static void ChangerInverserSouris(int32)
	{
		EcrireBool(TEXT("InverserSouris"), !InverserSouris());
		AppliquerAuJoueur(GJoueur.Get());
	}

	static FText TexteInverserManette()
	{
		return Ligne(TEXT("JOYSTICK HAUT/BAS INVERS\u00C9"), InverserManette() ? TEXT("OUI") : TEXT("NON"));
	}

	static void ChangerInverserManette(int32)
	{
		EcrireBool(TEXT("InverserManette"), !InverserManette());
		AppliquerAuJoueur(GJoueur.Get());
	}

	static FText TexteControle()
	{
		return Ligne(TEXT("JOUER AVEC"), Manette() ? TEXT("MANETTE") : TEXT("CLAVIER + SOURIS"));
	}

	static void ChangerControle(int32)
	{
		EcrireBool(TEXT("Manette"), !Manette());
	}

	static FText TexteClavier()
	{
		return Ligne(TEXT("CLAVIER"), Qwerty() ? TEXT("QWERTY (WASD)") : TEXT("AZERTY (ZQSD)"));
	}

	static void ChangerClavier(int32)
	{
		EcrireBool(TEXT("ClavierQwerty"), !Qwerty());
		AppliquerAuJoueur(GJoueur.Get());
	}

	// --- Chat et difficulte
	static FText TexteChat()
	{
		return FText::FromString(FString::Printf(TEXT("CHAT : %s"), ChatActif() ? TEXT("ON") : TEXT("OFF")));
	}

	static void ChangerChat()
	{
		EcrireBool(TEXT("Chat"), !ChatActif());
	}

	static const TCHAR* NomsDifficulte[] = { TEXT("FACILE"), TEXT("NORMAL"), TEXT("DIFFICILE"), TEXT("EXTR\u00CAME") };

	static FText TexteDifficulte()
	{
		return Ligne(TEXT("DIFFICULT\u00C9"), NomsDifficulte[Difficulte()]);
	}

	static void ChangerDifficulte(int32 Sens)
	{
		EcrireFloat(TEXT("Difficulte"), float(Boucle(Difficulte(), Sens, 4)));
	}
}

// ---------------------------------------------------------------- Fond anime

void SDecorCyber::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

int32 SDecorCyber::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2D T = G.GetLocalSize();
	const float Temps = Horloge();
	TArray<FVector2D> P;
	P.SetNum(2);

	// Grille neon
	const float Pas = 64.f;
	for (float X = FMath::Fmod(T.X * 0.5f, Pas); X < T.X; X += Pas)
	{
		P[0] = FVector2D(X, 0.f);
		P[1] = FVector2D(X, T.Y);
		Ligne(Sortie, Couche, G, P, Alpha(Cyan, 0.045f), 1.f);
	}
	for (float Y = FMath::Fmod(T.Y * 0.5f, Pas); Y < T.Y; Y += Pas)
	{
		P[0] = FVector2D(0.f, Y);
		P[1] = FVector2D(T.X, Y);
		Ligne(Sortie, Couche, G, P, Alpha(Cyan, 0.045f), 1.f);
	}

	// Lignes de balayage (effet ecran)
	for (float Y = 0.f; Y < T.Y; Y += 4.f)
	{
		P[0] = FVector2D(0.f, Y);
		P[1] = FVector2D(T.X, Y);
		Ligne(Sortie, Couche, G, P, FLinearColor(0.f, 0.f, 0.f, 0.12f), 1.f);
	}

	// Bande lumineuse qui descend
	const float Bande = FMath::Fmod(Temps * 140.f, T.Y + 200.f) - 100.f;
	for (int32 i = 0; i < 12; ++i)
	{
		P[0] = FVector2D(0.f, Bande + i * 2.f);
		P[1] = FVector2D(T.X, Bande + i * 2.f);
		Ligne(Sortie, Couche + 1, G, P, Alpha(Cyan, 0.025f), 2.f);
	}

	// Cadre de l'interface
	const float M = 36.f;
	const float Pulse = 0.55f + 0.25f * FMath::Sin(Temps * 2.f);
	TArray<FVector2D> Haut;
	Haut.Add(FVector2D(M, M + 30.f));
	Haut.Add(FVector2D(M + 30.f, M));
	Haut.Add(FVector2D(T.X * 0.35f, M));
	Ligne(Sortie, Couche + 1, G, Haut, Alpha(Magenta, Pulse), 2.f);
	TArray<FVector2D> HautDroit;
	HautDroit.Add(FVector2D(T.X * 0.65f, M));
	HautDroit.Add(FVector2D(T.X - M, M));
	HautDroit.Add(FVector2D(T.X - M, M + 60.f));
	Ligne(Sortie, Couche + 1, G, HautDroit, Alpha(Cyan, Pulse), 2.f);
	TArray<FVector2D> Bas;
	Bas.Add(FVector2D(T.X - M, T.Y - M - 30.f));
	Bas.Add(FVector2D(T.X - M - 30.f, T.Y - M));
	Bas.Add(FVector2D(T.X * 0.65f, T.Y - M));
	Ligne(Sortie, Couche + 1, G, Bas, Alpha(Magenta, Pulse), 2.f);
	TArray<FVector2D> BasGauche;
	BasGauche.Add(FVector2D(T.X * 0.35f, T.Y - M));
	BasGauche.Add(FVector2D(M, T.Y - M));
	BasGauche.Add(FVector2D(M, T.Y - M - 60.f));
	Ligne(Sortie, Couche + 1, G, BasGauche, Alpha(Cyan, Pulse), 2.f);

	const FSlateFontInfo Mini = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10);
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(M + 40.f, M + 8.f), FVector2D(400.f, 20.f)),
		FString(TEXT("CHELLES//3D  -  SYS.MENU v1.0")), Mini, ESlateDrawEffect::None, Alpha(Cyan, 0.6f));
	FSlateDrawElement::MakeText(Sortie, Couche + 2, G.ToPaintGeometry(FVector2D(T.X - M - 260.f, T.Y - M - 26.f), FVector2D(260.f, 20.f)),
		FString(TEXT("\u00C9CHAP / B = RETOUR  //  A = VALIDER")), Mini, ESlateDrawEffect::None, Alpha(Magenta, 0.6f));

	return Couche + 3;
}

// ---------------------------------------------------------------- Bouton

void SBoutonMenu::Construct(const FArguments& InArgs)
{
	Clic = InArgs._OnClic;
	ClicDroit = InArgs._OnClicDroit;
	bActif = InArgs._Actif;
	bReglage = InArgs._OnClicDroit.IsBound();
	SetCursor(EMouseCursor::Hand);

	const int32 Taille = InArgs._TaillePolice;
	ChildSlot
	.Padding(FMargin(Taille * 0.7f, Taille * 0.12f))
	.HAlign(HAlign_Center)
	[
		SNew(STextBlock)
		.Text(InArgs._Texte)
		.Font(Police(Taille, Magenta))
		.Justification(ETextJustify::Center)
		.ShadowOffset(FVector2D(0.f, 3.f))
		.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f))
		.ColorAndOpacity_Lambda([this]()
		{
			if (!bActif)
			{
				return FSlateColor(Gris);
			}
			return FSlateColor((IsHovered() || bSelectionne) ? Blanc : Cyan);
		})
	];
}

FReply SBoutonMenu::OnMouseButtonDown(const FGeometry& Geometrie, const FPointerEvent& Evenement)
{
	if (!bActif)
	{
		return FReply::Handled();
	}
	if (Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Clic.ExecuteIfBound();
		return FReply::Handled();
	}
	if (Evenement.GetEffectingButton() == EKeys::RightMouseButton)
	{
		ClicDroit.ExecuteIfBound();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

int32 SBoutonMenu::OnPaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip,
	FSlateWindowElementList& Sortie, int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (bActif && (IsHovered() || bSelectionne))
	{
		const FVector2D T = G.GetLocalSize();
		const float Pulse = 0.7f + 0.3f * FMath::Sin(Horloge() * 7.f);

		TArray<FSlateGradientStop> Degrade;
		Degrade.Add(FSlateGradientStop(FVector2D(0.f, 0.f), Alpha(Magenta, 0.45f)));
		Degrade.Add(FSlateGradientStop(FVector2D(T.X * 0.5f, 0.f), Alpha(Violet, 0.30f)));
		Degrade.Add(FSlateGradientStop(FVector2D(T.X, 0.f), Alpha(Cyan, 0.20f)));
		FSlateDrawElement::MakeGradient(Sortie, Couche, G.ToPaintGeometry(), Degrade, Orient_Vertical);

		const float Coin = FMath::Min(T.Y * 0.35f, 22.f);
		const TArray<FVector2D> Contour = Chanfrein(T, Coin);
		Ligne(Sortie, Couche + 1, G, Contour, Alpha(Cyan, 0.25f * Pulse), 8.f); // halo
		Ligne(Sortie, Couche + 2, G, Contour, Alpha(Cyan, Pulse), 2.5f);

		// Petits marqueurs magenta sur les cotes
		TArray<FVector2D> Marque;
		Marque.Add(FVector2D(-14.f, T.Y * 0.3f));
		Marque.Add(FVector2D(-14.f, T.Y * 0.7f));
		Ligne(Sortie, Couche + 2, G, Marque, Magenta, 4.f);
		Marque[0].X = Marque[1].X = T.X + 14.f;
		Ligne(Sortie, Couche + 2, G, Marque, Magenta, 4.f);
	}
	return SCompoundWidget::OnPaint(Args, G, Clip, Sortie, Couche + 3, Style, bParentEnabled);
}

void SBoutonMenu::Cliquer(int32 Sens)
{
	if (!bActif)
	{
		return;
	}
	if (Sens < 0 && ClicDroit.IsBound())
	{
		ClicDroit.Execute();
	}
	else
	{
		Clic.ExecuteIfBound();
	}
}

// ---------------------------------------------------------------- Menu

void SMenuChelles::Construct(const FArguments& InArgs)
{
	Menu = InArgs._Menu;

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBackgroundBlur)
			.BlurStrength(6.f)
		]
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.ColorAndOpacity(Fond)
		]
		+ SOverlay::Slot()
		[
			SNew(SDecorCyber)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(40.f, 80.f))
		[
			SAssignNew(Zone, SBox)
		]
	];
	Afficher(EEcranMenu::Principal);
}

AMenuChelles* SMenuChelles::LeMenu() const
{
	return Menu.Get();
}

void SMenuChelles::Tick(const FGeometry& Geometrie, const double Temps, const float Delta)
{
	SCompoundWidget::Tick(Geometrie, Temps, Delta);
	AMenuChelles* M = LeMenu();
	if (!M)
	{
		return;
	}
	// D'autres scripts du jeu peuvent cacher la souris au demarrage : on la garde visible.
	if (APlayerController* Joueur = M->GetPC())
	{
		Joueur->bShowMouseCursor = true;
	}
	// ... et remettre les commandes au jeu : on reprend la main deux fois au debut.
	const float Avant = Age;
	Age += Delta;
	if ((Avant < 0.5f && Age >= 0.5f) || (Avant < 1.5f && Age >= 1.5f))
	{
		M->ModeInterface();
	}
}

FReply SMenuChelles::OnKeyDown(const FGeometry& Geometrie, const FKeyEvent& Touche)
{
	const FKey K = Touche.GetKey();
	if (K == EKeys::Escape || K == EKeys::Gamepad_FaceButton_Right || K == EKeys::Gamepad_Special_Left)
	{
		Retour();
		return FReply::Handled();
	}
	if (K == EKeys::Up || K == EKeys::Gamepad_DPad_Up || K == EKeys::Gamepad_LeftStick_Up)
	{
		Deplacer(-1);
		return FReply::Handled();
	}
	if (K == EKeys::Down || K == EKeys::Gamepad_DPad_Down || K == EKeys::Gamepad_LeftStick_Down)
	{
		Deplacer(1);
		return FReply::Handled();
	}
	TSharedPtr<SBoutonMenu> Choisi = Boutons.IsValidIndex(Selection) ? Boutons[Selection].Pin() : TSharedPtr<SBoutonMenu>();
	if (K == EKeys::Enter || K == EKeys::SpaceBar || K == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (Choisi.IsValid())
		{
			Choisi->Cliquer(1);
		}
		else
		{
			Deplacer(0);
		}
		return FReply::Handled();
	}
	const bool bGauche = (K == EKeys::Left || K == EKeys::Gamepad_DPad_Left || K == EKeys::Gamepad_LeftStick_Left);
	const bool bDroite = (K == EKeys::Right || K == EKeys::Gamepad_DPad_Right || K == EKeys::Gamepad_LeftStick_Right);
	if ((bGauche || bDroite) && Choisi.IsValid() && Choisi->EstReglage())
	{
		Choisi->Cliquer(bDroite ? 1 : -1);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMenuChelles::Deplacer(int32 Sens)
{
	bNavigation = true;
	if (Boutons.Num() == 0)
	{
		return;
	}
	if (Boutons.IsValidIndex(Selection))
	{
		if (TSharedPtr<SBoutonMenu> Ancien = Boutons[Selection].Pin())
		{
			Ancien->SetSelectionne(false);
		}
	}
	Selection = Boutons.IsValidIndex(Selection) ? ((Selection + Sens) % Boutons.Num() + Boutons.Num()) % Boutons.Num() : 0;
	if (TSharedPtr<SBoutonMenu> Nouveau = Boutons[Selection].Pin())
	{
		Nouveau->SetSelectionne(true);
	}
}

TSharedRef<SBoutonMenu> SMenuChelles::Enregistrer(TSharedRef<SBoutonMenu> Nouveau)
{
	Boutons.Add(Nouveau);
	return Nouveau;
}

void SMenuChelles::Afficher(EEcranMenu NouvelEcran)
{
	const bool bMemeEcran = (Ecran == NouvelEcran);
	const int32 AncienneSelection = Selection;
	Ecran = NouvelEcran;
	Boutons.Reset();
	Selection = INDEX_NONE;
	if (Zone.IsValid())
	{
		Zone->SetContent(Construire(Ecran));
	}
	if (bNavigation || ChellesOptions::Manette())
	{
		// A la manette : un bouton est toujours en surbrillance
		Selection = (bMemeEcran && Boutons.IsValidIndex(AncienneSelection)) ? AncienneSelection - 1 : -1;
		Deplacer(1);
	}
	if (FSlateApplication::IsInitialized())
	{
		if (Ecran == EEcranMenu::AdresseIP && ChampIP.IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(ChampIP, EFocusCause::SetDirectly);
		}
		else
		{
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
		}
	}
}

void SMenuChelles::Rafraichir()
{
	if (Ecran == EEcranMenu::Serveurs || Ecran == EEcranMenu::Search)
	{
		Afficher(Ecran);
	}
}

void SMenuChelles::Retour()
{
	switch (Ecran)
	{
	case EEcranMenu::Mode:
	case EEcranMenu::Options:
		Afficher(EEcranMenu::Principal);
		break;
	case EEcranMenu::Multi:
		Afficher(EEcranMenu::Mode);
		break;
	case EEcranMenu::Serveurs:
	case EEcranMenu::AdresseIP:
	case EEcranMenu::Search:
		Afficher(EEcranMenu::Multi);
		break;
	case EEcranMenu::OptControles:
	case EEcranMenu::OptGraphismes:
	case EEcranMenu::OptAudio:
	case EEcranMenu::OptDifficulte:
		Afficher(EEcranMenu::Options);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------- Briques

TSharedRef<SWidget> SMenuChelles::Bouton(const FText& Texte, int32 Taille, TFunction<void()> Action, bool bActif)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, Taille * 0.12f))
		.HAlign(HAlign_Center)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte(Texte)
			.TaillePolice(Taille)
			.Actif(bActif)
			.OnClic(FSimpleDelegate::CreateLambda(MoveTemp(Action))))
		];
}

TSharedRef<SWidget> SMenuChelles::Reglage(TFunction<FText()> Texte, TFunction<void(int32)> Changer)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 4.f))
		.HAlign(HAlign_Center)
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte_Lambda(MoveTemp(Texte))
			.TaillePolice(30)
			.OnClic(FSimpleDelegate::CreateLambda([Changer]() { Changer(1); }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([Changer]() { Changer(-1); })))
		];
}

TSharedRef<SWidget> SMenuChelles::Titre(const FText& Texte, const FText& SousTitre)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(Texte)
			.Font(Police(72, Cyan))
			.ColorAndOpacity(Magenta)
			.ShadowOffset(FVector2D(4.f, 4.f))
			.ShadowColorAndOpacity(Alpha(Violet, 0.6f))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.f, 0.f, 0.f, 28.f))
		[
			SNew(STextBlock)
			.Text(SousTitre)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16))
			.ColorAndOpacity(Alpha(Cyan, 0.8f))
		];
}

TSharedRef<SWidget> SMenuChelles::Petit(const FText& Texte)
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 6.f))
		.HAlign(HAlign_Center)
		.MaxDesiredWidth(900.f)
		[
			SNew(STextBlock)
			.Text(Texte)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16))
			.ColorAndOpacity(Alpha(Blanc, 0.75f))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SMenuChelles::TexteEtat()
{
	return SNew(SBox)
		.Padding(FMargin(0.f, 14.f))
		.HAlign(HAlign_Center)
		.MaxDesiredWidth(900.f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { AMenuChelles* M = LeMenu(); return M ? M->Etat() : FText::GetEmpty(); })
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
			.ColorAndOpacity(Magenta)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SMenuChelles::BoutonRetour()
{
	return Bouton(FText::FromString(TEXT("< RETOUR")), 30, [this]() { Retour(); });
}

// ---------------------------------------------------------------- Ecrans

TSharedRef<SWidget> SMenuChelles::Construire(EEcranMenu E)
{
	switch (E)
	{
	case EEcranMenu::Mode:          return EcranMode();
	case EEcranMenu::Multi:         return EcranMulti();
	case EEcranMenu::Serveurs:      return EcranServeurs();
	case EEcranMenu::AdresseIP:     return EcranAdresseIP();
	case EEcranMenu::Search:        return EcranSearch();
	case EEcranMenu::Options:       return EcranOptions();
	case EEcranMenu::OptControles:  return EcranControles();
	case EEcranMenu::OptGraphismes: return EcranGraphismes();
	case EEcranMenu::OptAudio:      return EcranAudio();
	case EEcranMenu::OptDifficulte: return EcranDifficulte();
	default:                        return EcranPrincipal();
	}
}

TSharedRef<SWidget> SMenuChelles::EcranPrincipal()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("CHELLES 3D")), FText::FromString(TEXT("// FPS  -  QUARTIER DE CHELLES"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("JOUER")), 64, [this]() { Afficher(EEcranMenu::Mode); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("OPTIONS")), 64, [this]() { Afficher(EEcranMenu::Options); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("QUITTER")), 64, [this]() { if (AMenuChelles* M = LeMenu()) { M->Quitter(); } }) ];
}

TSharedRef<SWidget> SMenuChelles::EcranMode()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("MODE")), FText::FromString(TEXT("// CHOISIS TA FA\u00C7ON DE JOUER"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("SOLO")), 64, [this]() { if (AMenuChelles* M = LeMenu()) { M->Solo(); } }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("MULTI-PLAYER")), 64, [this]() { Afficher(EEcranMenu::Multi); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranMulti()
{
	AMenuChelles* M = LeMenu();
	const FString Ip = M ? M->AdresseLocale() : FString();
	const FText Info = FText::FromString(Ip.IsEmpty() ? FString(TEXT("Adresse IP de ce PC : inconnue"))
		: FString::Printf(TEXT("Adresse IP de ce PC sur le r\u00E9seau : %s"), *Ip));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("MULTI-PLAYER")), FText::FromString(TEXT("// JOUER \u00C0 PLUSIEURS"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("REJOINDRE SERVEUR")), 46, [this]()
			{
				Afficher(EEcranMenu::Serveurs);
				if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); }
			}) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("ADRESSE IP SERVEUR")), 46, [this]() { Afficher(EEcranMenu::AdresseIP); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("LAN LOCAL")), 46, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("LAN LOCAL = cr\u00E9er ta partie : les autres PC de la maison pourront la rejoindre."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("SEARCH")), 46, [this]()
			{
				Afficher(EEcranMenu::Search);
				if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); }
			}) ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(Info) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranServeurs()
{
	AMenuChelles* M = LeMenu();
	TSharedRef<SVerticalBox> Liste = SNew(SVerticalBox);

	if (M && M->RechercheEnCours())
	{
		Liste->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Recherche des parties sur ton r\u00E9seau local..."))) ];
	}
	else if (M && M->Parties().Num() > 0)
	{
		for (int32 i = 0; i < M->Parties().Num(); ++i)
		{
			const FPartieTrouvee& P = M->Parties()[i];
			const FText Libelle = FText::FromString(FString::Printf(TEXT("%s    %d/%d JOUEURS    %d ms"), *P.Hote.ToUpper(), P.Joueurs, P.Places, P.Ping));
			Liste->AddSlot().AutoHeight()[ Bouton(Libelle, 30, [this, i]() { if (AMenuChelles* Mn = LeMenu()) { Mn->RejoindrePartie(i); } }) ];
		}
	}
	else if (M && M->RechercheFaite())
	{
		Liste->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Aucun serveur trouv\u00E9. Sur l'autre PC, il faut choisir MULTI-PLAYER puis LAN LOCAL."))) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("SERVEURS")), FText::FromString(TEXT("// PARTIES SUR TON R\u00C9SEAU LOCAL"))) ]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.MaxDesiredHeight(380.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()[ Liste ]
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("ACTUALISER")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); } }, !(M && M->RechercheEnCours())) ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranAdresseIP()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("ADRESSE IP")), FText::FromString(TEXT("// REJOINDRE UN SERVEUR PAR SON ADRESSE"))) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 10.f))
		[
			SNew(SBox)
			.WidthOverride(560.f)
			[
				SAssignNew(ChampIP, SEditableTextBox)
				.Text(FText::FromString(TexteIP))
				.HintText(FText::FromString(TEXT("ex : 192.168.1.20")))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30))
				.ForegroundColor(FSlateColor(FLinearColor::Black))
				.Justification(ETextJustify::Center)
				.OnTextChanged_Lambda([this](const FText& Nouveau) { TexteIP = Nouveau.ToString(); })
				.OnTextCommitted_Lambda([this](const FText& Nouveau, ETextCommit::Type Comment)
				{
					TexteIP = Nouveau.ToString();
					if (Comment == ETextCommit::OnEnter)
					{
						if (AMenuChelles* M = LeMenu()) { M->RejoindreIP(TexteIP); }
					}
				})
			]
		]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("REJOINDRE")), 46, [this]() { if (AMenuChelles* M = LeMenu()) { M->RejoindreIP(TexteIP); } }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("L'adresse du serveur s'affiche sur son \u00E9cran MULTI-PLAYER. Pour Internet, l'h\u00F4te doit ouvrir le port 7777 (UDP) de sa box."))) ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranSearch()
{
	AMenuChelles* M = LeMenu();
	TSharedRef<SVerticalBox> Corps = SNew(SVerticalBox);

	if (!M || M->RechercheEnCours() || !M->RechercheFaite())
	{
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Je cherche une partie CHELLES 3D sur ton r\u00E9seau local..."))) ];
	}
	else if (M->Parties().Num() > 0)
	{
		const FPartieTrouvee& P = M->Parties()[0];
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(FString::Printf(TEXT("Partie trouv\u00E9e chez %s  (%d/%d joueurs, %d ms)"), *P.Hote, P.Joueurs, P.Places, P.Ping))) ];
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("REJOINDRE")), 52, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->RejoindrePartie(0); } }) ];
		if (M->Parties().Num() > 1)
		{
			Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(FString::Printf(TEXT("VOIR LES %d PARTIES"), M->Parties().Num())), 30, [this]() { Afficher(EEcranMenu::Serveurs); }) ];
		}
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("CR\u00C9ER MA PARTIE")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ];
	}
	else
	{
		Corps->AddSlot().AutoHeight()[ Petit(FText::FromString(TEXT("Personne ne joue sur ton r\u00E9seau local pour l'instant. Tu peux cr\u00E9er ta partie : les autres la trouveront avec SEARCH. (Recherche sur Internet : prochaine \u00E9tape, avec Steam ou Epic.)"))) ];
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("CR\u00C9ER MA PARTIE")), 46, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->HebergerLAN(); } }) ];
		Corps->AddSlot().AutoHeight()[ Bouton(FText::FromString(TEXT("CHERCHER ENCORE")), 30, [this]() { if (AMenuChelles* Mn = LeMenu()) { Mn->Rechercher(); } }) ];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("SEARCH")), FText::FromString(TEXT("// RECHERCHE AUTOMATIQUE D'UNE PARTIE"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Corps ]
		+ SVerticalBox::Slot().AutoHeight()[ TexteEtat() ]
		+ SVerticalBox::Slot().AutoHeight()[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranOptions()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("OPTIONS")), FText::FromString(TEXT("// R\u00C9GLAGES DU JEU"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("CLAVIER OU JOYSTICK")), 40, [this]() { Afficher(EEcranMenu::OptControles); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("AFFICHAGE & GRAPHISME")), 40, [this]() { Afficher(EEcranMenu::OptGraphismes); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("AUDIO")), 40, [this]() { Afficher(EEcranMenu::OptAudio); }) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 5.f))
		[
			Enregistrer(SNew(SBoutonMenu)
			.Texte_Lambda([]() { return ChellesOptions::TexteChat(); })
			.TaillePolice(40)
			.OnClic(FSimpleDelegate::CreateLambda([]() { ChellesOptions::ChangerChat(); }))
			.OnClicDroit(FSimpleDelegate::CreateLambda([]() { ChellesOptions::ChangerChat(); })))
		]
		+ SVerticalBox::Slot().AutoHeight()[ Bouton(FText::FromString(TEXT("DIFFICULT\u00C9S")), 40, [this]() { Afficher(EEcranMenu::OptDifficulte); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranControles()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("CLAVIER / JOYSTICK")), FText::FromString(TEXT("// CLIC GAUCHE = SUIVANT   CLIC DROIT = PR\u00C9C\u00C9DENT"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteControle(); }, [](int32 S) { ChellesOptions::ChangerControle(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteClavier(); }, [](int32 S) { ChellesOptions::ChangerClavier(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteSensibilite(); }, [](int32 S) { ChellesOptions::ChangerSensibilite(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteInverserSouris(); }, [](int32 S) { ChellesOptions::ChangerInverserSouris(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteInverserManette(); }, [](int32 S) { ChellesOptions::ChangerInverserManette(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Clavier et manette marchent tous les deux en jeu. Manette : croix = choisir, A = valider, B = retour, gauche/droite = changer un r\u00E9glage."))) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranGraphismes()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("AFFICHAGE & GRAPHISME")), FText::FromString(TEXT("// CLIC GAUCHE = SUIVANT   CLIC DROIT = PR\u00C9C\u00C9DENT"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteQualite(); }, [](int32 S) { ChellesOptions::ChangerQualite(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteAffichage(); }, [](int32 S) { ChellesOptions::ChangerAffichage(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteResolution(); }, [](int32 S) { ChellesOptions::ChangerResolution(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteVSync(); }, [](int32 S) { ChellesOptions::ChangerVSync(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteFPS(); }, [](int32 S) { ChellesOptions::ChangerFPS(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Affichage et r\u00E9solution ne changent que dans le jeu lanc\u00E9 seul (pas dans la fen\u00EAtre de l'\u00E9diteur)."))) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 12.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranAudio()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("AUDIO")), FText::FromString(TEXT("// CLIC GAUCHE = SUIVANT   CLIC DROIT = PR\u00C9C\u00C9DENT"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteVolume(); }, [](int32 S) { ChellesOptions::ChangerVolume(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteSon(); }, [](int32 S) { ChellesOptions::ChangerSon(S); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))[ BoutonRetour() ];
}

TSharedRef<SWidget> SMenuChelles::EcranDifficulte()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[ Titre(FText::FromString(TEXT("DIFFICULT\u00C9S")), FText::FromString(TEXT("// CLIC GAUCHE = SUIVANT   CLIC DROIT = PR\u00C9C\u00C9DENT"))) ]
		+ SVerticalBox::Slot().AutoHeight()[ Reglage([]() { return ChellesOptions::TexteDifficulte(); }, [](int32 S) { ChellesOptions::ChangerDifficulte(S); }) ]
		+ SVerticalBox::Slot().AutoHeight()[ Petit(FText::FromString(TEXT("Le choix est enregistr\u00E9. Son effet sur les ennemis sera branch\u00E9 \u00E0 l'\u00E9tape suivante."))) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 18.f, 0.f, 0.f))[ BoutonRetour() ];
}
