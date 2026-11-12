// CHELLES 3D - Demo : compteur en haut de l'ecran, alertes et ecran FIN DE LA DEMO (Slate).
#include "SDemoChelles.h"
#include "DemoChelles.h"

#include "Styling/CoreStyle.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

namespace
{
	// Memes couleurs que le menu (style cyberpunk)
	const FLinearColor DemoCyan(FColor(0, 240, 255));
	const FLinearColor DemoMagenta(FColor(255, 0, 170));
	const FLinearColor DemoOr(FColor(255, 196, 64));
	const FLinearColor DemoBlanc(FColor(235, 250, 255));
	const FLinearColor DemoRouge(FColor(255, 70, 70));

	FSlateFontInfo DemoPolice(const TCHAR* Graisse, int32 Taille, const FLinearColor& Halo)
	{
		FSlateFontInfo P = FCoreStyle::GetDefaultFontStyle(Graisse, Taille);
		P.OutlineSettings.OutlineSize = FMath::Max(1, Taille / 22);
		P.OutlineSettings.OutlineColor = FLinearColor(Halo.R, Halo.G, Halo.B, 0.6f);
		return P;
	}

	TSharedRef<SWidget> DemoBouton(const FText& Texte, const FLinearColor& Couleur, FOnClicked QuandClic, TSharedPtr<SWidget>& Sortie)
	{
		TSharedRef<SButton> Bouton = SNew(SButton)
			.ButtonColorAndOpacity(Couleur)
			.ContentPadding(FMargin(34.f, 12.f))
			.HAlign(HAlign_Center)
			.OnClicked(QuandClic)
			[
				SNew(STextBlock)
				.Text(Texte)
				.Font(DemoPolice(TEXT("Bold"), 20, FLinearColor::Black))
				.ColorAndOpacity(FLinearColor::White)
			];
		Sortie = Bouton;
		return Bouton;
	}
}

void SDemoChelles::Construct(const FArguments& InArgs)
{
	Demo = InArgs._Demo;
	const FSlateBrush* Blanc = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	TSharedPtr<SWidget> BoutonQuitter;

	ChildSlot
	[
		SNew(SOverlay)

		// ---- Petit compteur en haut au centre
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(SBorder)
			.Visibility(this, &SDemoChelles::VisibiliteCompteur)
			.BorderImage(Blanc)
			.BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.08f, 0.6f))
			.Padding(FMargin(14.f, 4.f))
			[
				SNew(STextBlock)
				.Text(this, &SDemoChelles::TexteCompteur)
				.Font(DemoPolice(TEXT("Bold"), 15, FLinearColor::Black))
				.ColorAndOpacity(this, &SDemoChelles::CouleurCompteur)
			]
		]

		// ---- Alerte (5 min / 1 min)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.f, 120.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Visibility(this, &SDemoChelles::VisibiliteAlerte)
			.Text(this, &SDemoChelles::TexteAlerte)
			.Font(DemoPolice(TEXT("Black"), 34, DemoMagenta))
			.ColorAndOpacity(DemoOr)
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f))
		]

		// ---- Ecran FIN DE LA DEMO
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.Visibility(this, &SDemoChelles::VisibiliteFin)
			.BorderImage(Blanc)
			.BorderBackgroundColor(FLinearColor(0.01f, 0.015f, 0.045f, 0.95f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(980.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("CHELLES 3D  \u00B7  VERSION B\u00CATA")))
						.Font(DemoPolice(TEXT("Bold"), 20, DemoMagenta))
						.ColorAndOpacity(DemoCyan)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 10.f, 0.f, 4.f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("FIN DE LA D\u00C9MO")))
						.Font(DemoPolice(TEXT("Black"), 72, DemoMagenta))
						.ColorAndOpacity(DemoOr)
						.ShadowOffset(FVector2D(3.f, 3.f))
						.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Merci d'avoir jou\u00E9 !")))
						.Font(DemoPolice(TEXT("Bold"), 24, DemoCyan))
						.ColorAndOpacity(DemoBlanc)
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 34.f, 0.f, 30.f)
					[
						SNew(STextBlock)
						.Text(this, &SDemoChelles::TexteMessage)
						.Font(DemoPolice(TEXT("Regular"), 22, FLinearColor::Black))
						.ColorAndOpacity(DemoBlanc)
						.AutoWrapText(true)
						.Justification(ETextJustify::Center)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						DemoBouton(FText::FromString(TEXT("REJOINDRE CHELLES 3D SUR FACEBOOK")), FLinearColor(FColor(24, 119, 242)),
							FOnClicked::CreateSP(this, &SDemoChelles::ClicFacebook), BoutonFacebook)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 8.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.Text(this, &SDemoChelles::TexteLien)
						.Font(DemoPolice(TEXT("Regular"), 14, FLinearColor::Black))
						.ColorAndOpacity(DemoCyan)
					]

					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 30.f, 0.f, 0.f)
					[
						DemoBouton(FText::FromString(TEXT("QUITTER LE JEU")), DemoMagenta,
							FOnClicked::CreateSP(this, &SDemoChelles::ClicQuitter), BoutonQuitter)
					]
				]
			]
		]
	];
}

EVisibility SDemoChelles::VisibiliteCompteur() const
{
	const ADemoChelles* D = Demo.Get();
	// toujours affiche (meme dans les menus) pour qu'on voie que la demo tourne
	return (D && !D->EstTerminee()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SDemoChelles::VisibiliteAlerte() const
{
	const ADemoChelles* D = Demo.Get();
	return (D && !D->EstTerminee() && !D->Alerte().IsEmpty()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

EVisibility SDemoChelles::VisibiliteFin() const
{
	const ADemoChelles* D = Demo.Get();
	return (D && D->EstTerminee()) ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SDemoChelles::TexteCompteur() const
{
	const ADemoChelles* D = Demo.Get();
	const int32 Reste = D ? FMath::CeilToInt(D->SecondesRestantes()) : 0;
	const bool bEnPause = D && !D->EnTrainDeJouer();
	return FText::FromString(FString::Printf(TEXT("D\u00C9MO B\u00CATA  \u00B7  %d:%02d%s"), Reste / 60, Reste % 60,
		bEnPause ? TEXT("  (en pause : menu)") : TEXT("")));
}

FSlateColor SDemoChelles::CouleurCompteur() const
{
	const ADemoChelles* D = Demo.Get();
	return (D && D->SecondesRestantes() <= 60.f) ? DemoRouge : DemoCyan;
}

FText SDemoChelles::TexteAlerte() const
{
	const ADemoChelles* D = Demo.Get();
	return D ? D->Alerte() : FText::GetEmpty();
}

FText SDemoChelles::TexteMessage() const
{
	const ADemoChelles* D = Demo.Get();
	return D ? D->Message() : FText::GetEmpty();
}

FText SDemoChelles::TexteLien() const
{
	const ADemoChelles* D = Demo.Get();
	return D ? FText::FromString(D->Lien()) : FText::GetEmpty();
}

FReply SDemoChelles::ClicFacebook()
{
	if (ADemoChelles* D = Demo.Get())
	{
		D->OuvrirLien();
	}
	return FReply::Handled();
}

FReply SDemoChelles::ClicQuitter()
{
	if (ADemoChelles* D = Demo.Get())
	{
		D->QuitterLeJeu();
	}
	return FReply::Handled();
}
