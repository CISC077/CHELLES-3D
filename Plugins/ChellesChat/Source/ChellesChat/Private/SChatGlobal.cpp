// CHELLES 3D - Fenetre du chat global (Slate), a gauche de l'ecran
#include "SChatGlobal.h"

#include "ChatGlobalChelles.h"
#include "ConnexionChatGlobal.h"

#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	// Memes couleurs neon que le reste du jeu
	const FLinearColor CCyan(FColor(0, 240, 255));
	const FLinearColor CMagenta(FColor(255, 0, 170));
	const FLinearColor COr(FColor(255, 196, 64));
	const FLinearColor CBlanc(FColor(235, 250, 255));
	const FLinearColor CGris(FColor(160, 175, 200));
	const FLinearColor COrange(FColor(255, 140, 60));
	const FLinearColor CFond(FColor(5, 7, 18, 200));

	/** Le jeu (ou le site) est affiche avec une manette, le site avec un globe. */
	const TCHAR* IconeJeu = TEXT("\U0001F3AE");
	const TCHAR* IconeSite = TEXT("\U0001F310");

	struct FPolicesChat
	{
		FSlateFontInfo Texte;
		FSlateFontInfo Gras;
		FSlateFontInfo Petit;
		FSlateFontInfo Icone;
		bool bEmoji = false;
	};

	/**
	 * Polices : Roboto (comme le reste de l'interface) + la police emoji de Windows pour les
	 * emojis (manette, globe, annonces)... Sans la police emoji, on ecrit [JEU] / [SITE] a la place.
	 * Creees une seule fois et jamais detruites (elles servent jusqu'a la fermeture du jeu).
	 */
	const FPolicesChat& Polices()
	{
		static FPolicesChat* P = nullptr;
		if (P)
		{
			return *P;
		}
		P = new FPolicesChat();

		FString Emoji;
#if PLATFORM_WINDOWS
		FString Windows = FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR"));
		if (Windows.IsEmpty())
		{
			Windows = TEXT("C:/Windows");
		}
		Emoji = Windows / TEXT("Fonts/seguiemj.ttf");
#endif
		P->bEmoji = !Emoji.IsEmpty() && FPaths::FileExists(Emoji);

		auto Faire = [&](const TCHAR* Fichier, const TCHAR* Style, int32 Taille) -> FSlateFontInfo
		{
			const FString Base = FPaths::EngineContentDir() / TEXT("Slate/Fonts") / Fichier;
			if (!P->bEmoji || !FPaths::FileExists(Base))
			{
				return FCoreStyle::GetDefaultFontStyle(Style, Taille);
			}
			TSharedRef<FStandaloneCompositeFont> Police = MakeShared<FStandaloneCompositeFont>(NAME_None, Base, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			FCompositeSubFont& SousPolice = Police->SubTypefaces.AddDefaulted_GetRef();
			SousPolice.CharacterRanges.Add(FInt32Range::Inclusive(0x2190, 0x2BFF));   // fleches, symboles, etoiles
			SousPolice.CharacterRanges.Add(FInt32Range::Inclusive(0x1F000, 0x1FAFF)); // emojis
			SousPolice.Typeface.AppendFont(NAME_None, Emoji, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			return FSlateFontInfo(Police, Taille);
		};
		P->Texte = Faire(TEXT("Roboto-Regular.ttf"), TEXT("Regular"), 14);
		P->Gras = Faire(TEXT("Roboto-Bold.ttf"), TEXT("Bold"), 13);
		P->Petit = Faire(TEXT("Roboto-Regular.ttf"), TEXT("Regular"), 11);
		P->Icone = Faire(TEXT("Roboto-Regular.ttf"), TEXT("Regular"), 13);
		return *P;
	}

	double Heure() { return FPlatformTime::Seconds(); }
}

void SChatGlobal::Construct(const FArguments& InArgs)
{
	Proprietaire = InArgs._Proprietaire;
	const FSlateBrush* Pinceau = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FPolicesChat& P = Polices();

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	.Padding(FMargin(24.f, 110.f, 0.f, 0.f))
	[
		SNew(SBox)
		.WidthOverride(430.f)
		.HeightOverride(340.f)
		.Visibility(this, &SChatGlobal::Visibilite)
		[
			SNew(SBorder)
			.BorderImage(Pinceau)
			.BorderBackgroundColor(CFond)
			.Padding(FMargin(12.f))
			[
				SNew(SVerticalBox)
				// Titre + etat
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(P.bEmoji ? FString(TEXT("CHAT GLOBAL ")) + IconeSite : FString(TEXT("CHAT GLOBAL"))))
						.Font(PoliceTitre())
						.ColorAndOpacity(CCyan)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SChatGlobal::TexteEtat)
						.Font(P.Petit)
						.ColorAndOpacity(this, &SChatGlobal::CouleurEtat)
					]
				]
				// Lisere neon
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f))
				[
					SNew(SBox).HeightOverride(2.f)
					[
						SNew(SImage).Image(Pinceau).ColorAndOpacity(FSlateColor(CMagenta))
					]
				]
				// Chat sur OFF
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Visibility_Lambda([]() { return FConnexionChatGlobal::Get().OptionActive() ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text(FText::FromString(TEXT("Le chat global est sur OFF.\nPour parler avec les joueurs et les visiteurs du site chelles3d.fr : \u00C9CHAP > OPTIONS > JEU > CHAT GLOBAL : ON.")))
					.Font(P.Texte)
					.ColorAndOpacity(COr)
					.AutoWrapText(true)
				]
				// Messages
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SAssignNew(Liste, SScrollBox)
				]
				// Saisie
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
				[
					SNew(SBox)
					.Visibility_Lambda([this]()
					{
						const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
						return (bOuverte && C.OptionActive() && C.Etat() != EEtatChatGlobal::Bloque) ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						SAssignNew(Saisie, SEditableTextBox)
						.HintText(this, &SChatGlobal::Indice)
						.Font(P.Texte)
						.ClearKeyboardFocusOnCommit(false)
						.RevertTextOnEscape(true)
						.OnTextCommitted(this, &SChatGlobal::Valider)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return bOuverte ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(this, &SChatGlobal::TexteAide)
					.Font(P.Petit)
					.ColorAndOpacity(FLinearColor(CMagenta.R, CMagenta.G, CMagenta.B, 0.7f))
					.AutoWrapText(true)
				]
			]
		]
	];
	Reconstruire();
}

FSlateFontInfo SChatGlobal::PoliceTitre()
{
	FSlateFontInfo F = Polices().Gras;
	F.Size = 15;
	return F;
}

EVisibility SChatGlobal::Visibilite() const
{
	if (bOuverte)
	{
		return EVisibility::Visible;
	}
	const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	// Fenetre fermee : on montre quelques secondes les nouveaux messages, sans gener le jeu
	if (C.OptionActive() && C.Etat() == EEtatChatGlobal::Connecte && Heure() - C.DernierMessage() < 8.0)
	{
		return EVisibility::HitTestInvisible;
	}
	return EVisibility::Collapsed;
}

FText SChatGlobal::TexteEtat() const
{
	const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	if (!C.OptionActive())
	{
		return FText::FromString(TEXT("OFF"));
	}
	switch (C.Etat())
	{
	case EEtatChatGlobal::SansPseudo: return FText::FromString(TEXT("choisis ton pseudo"));
	case EEtatChatGlobal::Connexion:  return FText::FromString(TEXT("connexion..."));
	case EEtatChatGlobal::Attente:    return FText::FromString(TEXT("reconnexion..."));
	case EEtatChatGlobal::Bloque:     return FText::FromString(TEXT("indisponible"));
	case EEtatChatGlobal::Connecte:
		return FText::FromString(FString::Printf(TEXT("%s  \u2022  %d en ligne, dont %d en jeu"), *C.PseudoServeur(), C.EnLigne(), C.JoueursDuJeu()));
	default:
		return FText::FromString(TEXT("hors ligne"));
	}
}

FSlateColor SChatGlobal::CouleurEtat() const
{
	const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	if (C.OptionActive() && C.Etat() == EEtatChatGlobal::Connecte)
	{
		return FSlateColor(FLinearColor(0.2f, 1.f, 0.4f));
	}
	return FSlateColor(CGris);
}

FText SChatGlobal::Indice() const
{
	const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	if (C.Etat() == EEtatChatGlobal::SansPseudo)
	{
		return FText::FromString(TEXT("Choisis ton pseudo (3 \u00E0 20 lettres) puis ENTR\u00C9E"));
	}
	return FText::FromString(TEXT("Ton message pour tout le monde (jeu + site)..."));
}

FText SChatGlobal::TexteAide() const
{
	return FText::FromString(Polices().bEmoji
		? FString::Printf(TEXT("%s = joueur du jeu   %s = visiteur du site\nENTR\u00C9E = envoyer  //  \u00C9CHAP = fermer  //  /pseudo Nom = changer de pseudo"), IconeJeu, IconeSite)
		: FString(TEXT("[JEU] = joueur du jeu   [SITE] = visiteur du site\nENTR\u00C9E = envoyer  //  \u00C9CHAP = fermer  //  /pseudo Nom = changer de pseudo")));
}

void SChatGlobal::Ouvrir()
{
	bOuverte = true;
	HeureOuverture = Heure();
	if (AChatGlobalChelles* A = Proprietaire.Get())
	{
		if (APlayerController* PC = A->GetPC())
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(Saisie);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
			PC->bShowMouseCursor = true;
		}
	}
	if (Saisie.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Saisie, EFocusCause::SetDirectly);
	}
	VersionVue = -1; // redessine et descend en bas de la liste
}

void SChatGlobal::Fermer()
{
	if (!bOuverte)
	{
		return;
	}
	bOuverte = false;
	if (Saisie.IsValid())
	{
		Saisie->SetText(FText::GetEmpty());
	}
	if (AChatGlobalChelles* A = Proprietaire.Get())
	{
		if (APlayerController* PC = A->GetPC())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

FReply SChatGlobal::OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& Touche)
{
	if (bOuverte && (Touche.GetKey() == EKeys::Escape || Touche.GetKey() == EKeys::Gamepad_FaceButton_Right))
	{
		Fermer();
		return FReply::Handled();
	}
	return SCompoundWidget::OnPreviewKeyDown(Geo, Touche);
}

void SChatGlobal::Valider(const FText& Texte, ETextCommit::Type Comment)
{
	if (Comment == ETextCommit::OnCleared)
	{
		Fermer(); // Echap
		return;
	}
	if (Comment != ETextCommit::OnEnter)
	{
		return;
	}
	FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	const FString Message = Texte.ToString().TrimStartAndEnd();
	if (Saisie.IsValid())
	{
		Saisie->SetText(FText::GetEmpty());
	}
	if (Message.IsEmpty())
	{
		Fermer(); // Entree sur une ligne vide = fermer
		return;
	}
	const bool bChoixPseudo = C.Etat() == EEtatChatGlobal::SansPseudo || Message.StartsWith(TEXT("/pseudo"), ESearchCase::IgnoreCase);
	C.Envoyer(Message);
	if (!bChoixPseudo)
	{
		Fermer(); // comme le chat ENTREE : on revient au jeu apres l'envoi
	}
}

void SChatGlobal::Tick(const FGeometry& Geo, const double Temps, const float Delta)
{
	SCompoundWidget::Tick(Geo, Temps, Delta);
	// La touche B qui a ouvert la fenetre ne doit pas s'ecrire dans la zone de saisie
	if (bOuverte && Saisie.IsValid() && Heure() - HeureOuverture < 0.3)
	{
		const FString Deja = Saisie->GetText().ToString();
		if (Deja == TEXT("b") || Deja == TEXT("B"))
		{
			Saisie->SetText(FText::GetEmpty());
		}
	}
	const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	if (C.Version() != VersionVue)
	{
		Reconstruire();
	}
}

void SChatGlobal::Reconstruire()
{
	const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
	VersionVue = C.Version();
	if (!Liste.IsValid())
	{
		return;
	}
	const FPolicesChat& P = Polices();
	Liste->ClearChildren();

	if (C.Lignes().Num() == 0 && C.Etat() == EEtatChatGlobal::SansPseudo)
	{
		Liste->AddSlot()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Bienvenue sur le chat global de CHELLES 3D ! Tu parles en direct avec les autres joueurs et avec les visiteurs du site chelles3d.fr.\nChoisis d'abord ton pseudo ci-dessous.")))
			.Font(P.Texte)
			.ColorAndOpacity(COr)
			.AutoWrapText(true)
		];
	}

	for (const FLigneChatGlobal& L : C.Lignes())
	{
		if (L.Source == ESourceChatGlobal::Systeme || L.Source == ESourceChatGlobal::Local)
		{
			Liste->AddSlot()
			.Padding(FMargin(0.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(L.Texte))
				.Font(P.Petit)
				.ColorAndOpacity(L.Source == ESourceChatGlobal::Local ? COrange : CGris)
				.AutoWrapText(true)
			];
			continue;
		}
		const bool bJeu = L.Source == ESourceChatGlobal::Jeu;
		const FString Icone = P.bEmoji ? FString(bJeu ? IconeJeu : IconeSite) : FString(bJeu ? TEXT("[JEU]") : TEXT("[SITE]"));
		const FLinearColor Couleur = L.bAdmin ? COr : (bJeu ? CCyan : CMagenta);
		Liste->AddSlot()
		.Padding(FMargin(0.f, 3.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Icone))
					.Font(P.Icone)
					.ColorAndOpacity(Couleur)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(L.bAdmin ? L.Pseudo + TEXT("  (admin)") : L.Pseudo))
					.Font(P.Gras)
					.ColorAndOpacity(Couleur)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(L.Texte))
				.Font(P.Texte)
				.ColorAndOpacity(CBlanc)
				.AutoWrapText(true)
			]
		];
	}
	Liste->ScrollToEnd();
}
