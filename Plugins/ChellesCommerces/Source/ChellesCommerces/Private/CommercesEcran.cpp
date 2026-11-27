// CHELLES 3D - Commerces : ecran (compteur de pieces a droite, fenetre de la boutique au comptoir), Slate
#include "CommercesChelles.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace ChellesCommerces;

namespace
{
	const FSlateBrush* FondCommerces()
	{
		static FSlateColorBrush Fond(FLinearColor::White);
		return &Fond;
	}

	const FLinearColor OrCommerces(1.f, 0.82f, 0.28f);
}

void ACommercesChelles::CreerEcran()
{
	UWorld* W = GetWorld();
	UGameViewportClient* VueJeu = W ? W->GetGameViewport() : nullptr;
	if (!VueJeu || Ecran.IsValid())
	{
		return;
	}
	TWeakObjectPtr<ACommercesChelles> Moi(this);
	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 22.f, 160.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SBorder)
				.BorderImage(FondCommerces())
				.BorderBackgroundColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.6f))
				.Padding(FMargin(12.f, 5.f))
				.Visibility_Lambda([Moi]()
				{
					return (Moi.IsValid() && Moi->LireReglages().bAfficherPieces) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
				})
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
					.ColorAndOpacity(OrCommerces)
					.ShadowOffset(FVector2D(1.f, 1.f))
					.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
					.Text_Lambda([Moi]()
					{
						return Moi.IsValid() ? FText::FromString(FString::Printf(TEXT("%d  PI\u00C8CES"), Moi->Pieces())) : FText::GetEmpty();
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				.ColorAndOpacity_Lambda([Moi]()
				{
					const float Age = Moi.IsValid() ? Moi->AgeDuGain() : 100.f;
					const float A = FMath::Clamp(1.f - (Age - 2.5f) / 1.f, 0.f, 1.f);
					return FSlateColor(FLinearColor(1.f, 0.9f, 0.5f, A));
				})
				.Text_Lambda([Moi]()
				{
					return (Moi.IsValid() && Moi->AgeDuGain() < 3.5f) ? FText::FromString(Moi->TexteGain()) : FText::GetEmpty();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.Justification(ETextJustify::Right)
				.ColorAndOpacity(FLinearColor(0.55f, 0.95f, 1.f, 1.f))
				.ShadowOffset(FVector2D(1.f, 1.f))
				.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
				.Text_Lambda([Moi]()
				{
					return Moi.IsValid() ? FText::FromString(Moi->TexteBonus()) : FText::GetEmpty();
				})
			]
		];
	VueJeu->AddViewportWidgetContent(Ecran.ToSharedRef(), 6);
}

void ACommercesChelles::RetirerEcran()
{
	if (!Ecran.IsValid())
	{
		return;
	}
	if (UWorld* W = GetWorld())
	{
		if (UGameViewportClient* VueJeu = W->GetGameViewport())
		{
			VueJeu->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
	}
	Ecran.Reset();
}

TSharedRef<SWidget> ACommercesChelles::CreerFenetre(int32 Commerce)
{
	TWeakObjectPtr<ACommercesChelles> Moi(this);
	const FCommerce& C = Reglages.Commerces[Commerce];
	const FLinearColor Couleur = C.Couleur;

	TSharedRef<SVerticalBox> Liste = SNew(SVerticalBox);
	for (int32 a = 0; a < C.Articles.Num(); ++a)
	{
		const FArticle& A = C.Articles[a];
		const int32 Prix = A.Prix;
		Liste->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 3.f))
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FLinearColor(0.16f, 0.17f, 0.22f, 1.f))
				.ContentPadding(FMargin(12.f, 7.f))
				.IsEnabled_Lambda([Moi, Prix]()
				{
					return Moi.IsValid() && !Moi->AchatEnCours() && Moi->Pieces() >= Prix;
				})
				.OnClicked_Lambda([Moi, a]()
				{
					if (Moi.IsValid())
					{
						Moi->Acheter(a);
					}
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
							.ColorAndOpacity(FLinearColor::White)
							.Text(FText::FromString(A.Nom))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
							.ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.85f, 1.f))
							.Text(FText::FromString(A.Description.IsEmpty() ? TexteEffet(A) : A.Description))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(14.f, 0.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
						.ColorAndOpacity(OrCommerces)
						.Text(FText::FromString(FString::Printf(TEXT("%d pi\u00E8ces"), Prix)))
					]
				]
			];
	}

	return SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(560.f)
			[
				SNew(SBorder)
				.BorderImage(FondCommerces())
				.BorderBackgroundColor(FLinearColor(0.03f, 0.04f, 0.07f, 0.93f))
				.Padding(FMargin(0.f))
				[
					SNew(SVerticalBox)
					// bandeau de couleur du commerce
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.BorderImage(FondCommerces())
						.BorderBackgroundColor(Couleur)
						.Padding(FMargin(18.f, 10.f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
								.ColorAndOpacity(FLinearColor(0.03f, 0.03f, 0.05f, 1.f))
								.Text(FText::FromString(C.Nom))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
								.ColorAndOpacity(FLinearColor(0.03f, 0.03f, 0.05f, 1.f))
								.Text_Lambda([Moi]()
								{
									return Moi.IsValid() ? FText::FromString(FString::Printf(TEXT("Tu as %d pi\u00E8ces"), Moi->Pieces())) : FText::GetEmpty();
								})
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(16.f, 10.f, 16.f, 4.f))
					[
						Liste
					]
					// message (achat reussi, pas assez de pieces...)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(18.f, 6.f))
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.AutoWrapText(true)
						.ColorAndOpacity_Lambda([Moi]()
						{
							return FSlateColor(Moi.IsValid() ? Moi->CouleurMessage() : FLinearColor::White);
						})
						.Text_Lambda([Moi]()
						{
							return Moi.IsValid() ? FText::FromString(Moi->TexteMessage()) : FText::GetEmpty();
						})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(16.f, 4.f, 16.f, 14.f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
							.ColorAndOpacity(FLinearColor(0.6f, 0.62f, 0.7f, 1.f))
							.Text(FText::FromString(TEXT("Gagne des pi\u00E8ces en tuant des zombies et en gagnant de l'XP")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonColorAndOpacity(FLinearColor(0.55f, 0.12f, 0.12f, 1.f))
							.ContentPadding(FMargin(16.f, 6.f))
							.OnClicked_Lambda([Moi]()
							{
								if (Moi.IsValid())
								{
									Moi->FermerBoutique(true);
								}
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
								.ColorAndOpacity(FLinearColor::White)
								.Text(FText::FromString(TEXT("FERMER")))
							]
						]
					]
				]
			]
		];
}
