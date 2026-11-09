// CHELLES 3D - Panneau vertical a droite : chat entre joueurs + aide (Tuto & Astuces)
#include "SChatChelles.h"

#include "MenuChelles.h"
#include "SMenuChelles.h"
#include "ChatChelles.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor CCyan(FColor(0, 240, 255));
	const FLinearColor CMagenta(FColor(255, 0, 170));
	const FLinearColor COr(FColor(255, 196, 64));
	const FLinearColor CBlanc(FColor(235, 250, 255));
	const FLinearColor CFond(FColor(5, 7, 18, 200));

	struct FLigneChat
	{
		FString Auteur;
		FString Texte;
		FLinearColor Couleur;
	};

	TArray<FLigneChat> GJoueurs;
	TArray<FLigneChat> GAide;
	double GDernierJoueur = -100.0;

	double HeureChat() { return FPlatformTime::Seconds(); }

	void Garder(TArray<FLigneChat>& Liste, const FLigneChat& L)
	{
		Liste.Add(L);
		if (Liste.Num() > 80)
		{
			Liste.RemoveAt(0, Liste.Num() - 80);
		}
	}

	// ---------------------------------------------------------- Chatbot d'aide

	struct FSujetAide
	{
		FString Titre;
		TArray<FString> Mots;
		FString Reponse;
	};

	TArray<FSujetAide> GBible;
	bool GBibleLue = false;

	/** Minuscules sans accents, pour comparer - Motoneige - et - motoneige -. */
	FString Normaliser(const FString& Texte)
	{
		FString R = Texte.ToLower();
		for (TCHAR& C : R)
		{
			switch (C)
			{
			case TEXT('\u00E0'): case TEXT('\u00E1'): case TEXT('\u00E2'): case TEXT('\u00E4'): case TEXT('\u00C0'): case TEXT('\u00C2'): C = TEXT('a'); break;
			case TEXT('\u00E9'): case TEXT('\u00E8'): case TEXT('\u00EA'): case TEXT('\u00EB'): case TEXT('\u00C9'): case TEXT('\u00C8'): case TEXT('\u00CA'): C = TEXT('e'); break;
			case TEXT('\u00EE'): case TEXT('\u00EF'): case TEXT('\u00CE'): C = TEXT('i'); break;
			case TEXT('\u00F4'): case TEXT('\u00F6'): case TEXT('\u00D4'): C = TEXT('o'); break;
			case TEXT('\u00F9'): case TEXT('\u00FB'): case TEXT('\u00FC'): case TEXT('\u00DB'): C = TEXT('u'); break;
			case TEXT('\u00E7'): case TEXT('\u00C7'): C = TEXT('c'); break;
			case TEXT('\''): case TEXT('-'): case TEXT('?'): case TEXT('!'): case TEXT(','): case TEXT('.'): C = TEXT(' '); break;
			default: break;
			}
		}
		return TEXT(" ") + R + TEXT(" ");
	}

	void LireBible()
	{
		GBibleLue = true;
		GBible.Reset();
		const FString Chemins[] = {
			FPaths::ProjectContentDir() / TEXT("ChellesMenu/Bible_Chelles.txt"),
			FPaths::ProjectDir() / TEXT("Tools/Menu/Bible_Chelles.txt")
		};
		FString Contenu;
		for (const FString& Chemin : Chemins)
		{
			if (FFileHelper::LoadFileToString(Contenu, *Chemin))
			{
				break;
			}
		}
		TArray<FString> Lignes;
		Contenu.ParseIntoArrayLines(Lignes, false);
		FSujetAide* Courant = nullptr;
		for (const FString& Brute : Lignes)
		{
			const FString Ligne = Brute.TrimStartAndEnd();
			if (Ligne.StartsWith(TEXT("#")))
			{
				continue;
			}
			if (Ligne.StartsWith(TEXT("[")) && Ligne.EndsWith(TEXT("]")))
			{
				FSujetAide& Sujet = GBible.AddDefaulted_GetRef();
				TArray<FString> Mots;
				Ligne.Mid(1, Ligne.Len() - 2).ParseIntoArray(Mots, TEXT(","), true);
				for (const FString& Mot : Mots)
				{
					const FString Propre = Mot.TrimStartAndEnd();
					if (!Propre.IsEmpty())
					{
						if (Sujet.Titre.IsEmpty())
						{
							Sujet.Titre = Propre;
						}
						Sujet.Mots.Add(Normaliser(Propre).TrimStartAndEnd());
					}
				}
				Courant = &Sujet;
				continue;
			}
			if (Courant && !Ligne.IsEmpty())
			{
				Courant->Reponse += (Courant->Reponse.IsEmpty() ? TEXT("") : TEXT("\n")) + Ligne;
			}
		}
		GBible.RemoveAll([](const FSujetAide& S) { return S.Reponse.IsEmpty(); });
	}

	FString ListeDesSujets()
	{
		TArray<FString> Titres;
		for (const FSujetAide& S : GBible)
		{
			Titres.Add(S.Titre);
		}
		return FString::Join(Titres, TEXT(", "));
	}
}

namespace ChellesChat
{
	void AjouterMessageJoueur(const FString& Auteur, const FString& Texte)
	{
		Garder(GJoueurs, { Auteur, Texte, CCyan });
		GDernierJoueur = HeureChat();
	}

	FString RepondreAide(const FString& Question)
	{
		if (!GBibleLue)
		{
			LireBible();
		}
		if (GBible.Num() == 0)
		{
			return TEXT("La bible du jeu est introuvable (Content/ChellesMenu/Bible_Chelles.txt).");
		}
		const FString Q = Normaliser(Question);
		const FString QSeule = Q.TrimStartAndEnd();
		if (QSeule.IsEmpty() || QSeule == TEXT("sujets") || QSeule == TEXT("liste") || QSeule == TEXT("aide"))
		{
			return TEXT("Je peux t'aider sur : ") + ListeDesSujets() + TEXT(".");
		}
		int32 Meilleur = INDEX_NONE;
		int32 MeilleurScore = 0;
		for (int32 i = 0; i < GBible.Num(); ++i)
		{
			int32 Score = 0;
			for (const FString& Mot : GBible[i].Mots)
			{
				// mot entier (entoure d'espaces) = plus de points qu'un bout de mot
				if (Q.Contains(TEXT(" ") + Mot + TEXT(" ")))
				{
					Score += 10 + Mot.Len();
				}
				else if (Mot.Len() >= 4 && Q.Contains(Mot))
				{
					Score += Mot.Len();
				}
			}
			if (Score > MeilleurScore)
			{
				MeilleurScore = Score;
				Meilleur = i;
			}
		}
		if (Meilleur == INDEX_NONE)
		{
			return TEXT("Je n'ai pas trouv\u00E9. Essaie un de ces mots : ") + ListeDesSujets() + TEXT(".");
		}
		return GBible[Meilleur].Reponse;
	}
}

// ---------------------------------------------------------------- Panneau

void SChatChelles::Construct(const FArguments& InArgs)
{
	Menu = InArgs._Menu;
	const FSlateBrush* Blanc = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Fill)
	.Padding(FMargin(0.f, 90.f, 24.f, 140.f))
	[
		SNew(SBox)
		.WidthOverride(430.f)
		.Visibility(this, &SChatChelles::Visibilite)
		[
			SNew(SBorder)
			.BorderImage(Blanc)
			.BorderBackgroundColor(CFond)
			.Padding(FMargin(12.f))
			[
				SNew(SVerticalBox)
				// Onglets
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[ BoutonOnglet(TEXT("JOUEURS"), false) ]
					+ SHorizontalBox::Slot().AutoWidth()[ BoutonOnglet(TEXT("AIDE  /  TUTO & ASTUCES"), true) ]
				]
				// Lisere neon
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f))
				[
					SNew(SBox).HeightOverride(2.f)
					[
						SNew(SImage).Image(Blanc).ColorAndOpacity_Lambda([this]() { return FSlateColor(bAide ? COr : CCyan); })
					]
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
					.Visibility_Lambda([this]() { return bOuvert ? EVisibility::Visible : EVisibility::Collapsed; })
					[
						SAssignNew(Saisie, SEditableTextBox)
						.HintText_Lambda([this]() { return FText::FromString(bAide
							? FString(TEXT("Pose ta question (ex : roue, motoneige, multi)..."))
							: FString(TEXT("Ton message pour les joueurs..."))); })
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15))
						.ClearKeyboardFocusOnCommit(false)
						.RevertTextOnEscape(true)
						.OnTextCommitted(this, &SChatChelles::Valider)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return bOuvert ? EVisibility::Visible : EVisibility::Collapsed; })
					.Text(FText::FromString(TEXT("ENTR\u00C9E = envoyer   //   \u00C9CHAP = fermer   //   TAB = onglet")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11))
					.ColorAndOpacity(FLinearColor(CMagenta.R, CMagenta.G, CMagenta.B, 0.7f))
				]
			]
		]
	];
}

TSharedRef<SWidget> SChatChelles::BoutonOnglet(const FString& Nom, bool bOngletAide)
{
	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.Visibility_Lambda([this, bOngletAide]()
		{
			return (bOngletAide ? AideDispo() : ChatDispo()) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.OnClicked_Lambda([this, bOngletAide]() { ChoisirOnglet(bOngletAide); return FReply::Handled(); })
		.ContentPadding(FMargin(0.f, 0.f, 18.f, 0.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Nom))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15))
			.ColorAndOpacity_Lambda([this, bOngletAide]()
			{
				const bool bActif = (bAide == bOngletAide);
				const FLinearColor C = bOngletAide ? COr : CCyan;
				return FSlateColor(bActif ? C : FLinearColor(C.R, C.G, C.B, 0.4f));
			})
		];
}

bool SChatChelles::EnMulti() const
{
	const AMenuChelles* M = Menu.Get();
	const UWorld* Monde = M ? M->GetWorld() : nullptr;
	return Monde && Monde->GetNetMode() != NM_Standalone;
}

bool SChatChelles::ChatDispo() const
{
	return EnMulti() && ChellesOptions::ChatActif();
}

bool SChatChelles::AideDispo() const
{
	return ChellesOptions::AideActive();
}

EVisibility SChatChelles::Visibilite() const
{
	if (bOuvert)
	{
		return EVisibility::Visible;
	}
	// Panneau ferme : on montre quelques secondes les nouveaux messages des joueurs
	if (ChatDispo() && HeureChat() - GDernierJoueur < 8.0)
	{
		return EVisibility::HitTestInvisible;
	}
	return EVisibility::Collapsed;
}

void SChatChelles::ChoisirOnglet(bool bVersAide)
{
	bAide = bVersAide;
	if (bAide && GAide.Num() == 0)
	{
		Garder(GAide, { TEXT("AIDE"), TEXT("Salut ! Je suis l'aide de CHELLES 3D : tuto, astuces et touches. Pose ta question, ou tape \u00AB sujets \u00BB pour voir tout ce que je connais."), COr });
	}
	VuJoueurs = VuAide = -1; // force la mise a jour
	if (bOuvert && Saisie.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Saisie, EFocusCause::SetDirectly);
	}
}

void SChatChelles::Ouvrir()
{
	if (!ChatDispo() && !AideDispo())
	{
		return;
	}
	bOuvert = true;
	ChoisirOnglet(ChatDispo() ? (bAide && AideDispo()) : true);
	if (AMenuChelles* M = Menu.Get())
	{
		if (APlayerController* PC = M->GetPC())
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
}

void SChatChelles::Fermer()
{
	if (!bOuvert)
	{
		return;
	}
	bOuvert = false;
	if (Saisie.IsValid())
	{
		Saisie->SetText(FText::GetEmpty());
	}
	if (AMenuChelles* M = Menu.Get())
	{
		if (APlayerController* PC = M->GetPC())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}
	FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

FReply SChatChelles::OnPreviewKeyDown(const FGeometry& Geo, const FKeyEvent& Touche)
{
	if (bOuvert && (Touche.GetKey() == EKeys::Escape || Touche.GetKey() == EKeys::Gamepad_FaceButton_Right))
	{
		Fermer();
		return FReply::Handled();
	}
	if (bOuvert && Touche.GetKey() == EKeys::Tab && ChatDispo() && AideDispo())
	{
		ChoisirOnglet(!bAide); // Tab = changer d'onglet
		return FReply::Handled();
	}
	return SCompoundWidget::OnPreviewKeyDown(Geo, Touche);
}

void SChatChelles::Valider(const FText& Texte, ETextCommit::Type Comment)
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
	if (bAide)
	{
		Garder(GAide, { TEXT("TOI"), Message, CBlanc });
		Garder(GAide, { TEXT("AIDE"), ChellesChat::RepondreAide(Message), COr });
		return; // on reste ouvert pour poser d'autres questions
	}
	AMenuChelles* M = Menu.Get();
	UChatChelles* Chat = M ? UChatChelles::DuJoueur(M->GetPC()) : nullptr;
	if (Chat)
	{
		Chat->Envoyer(Message);
	}
	else
	{
		Garder(GJoueurs, { TEXT("CHAT"), TEXT("Pas encore connect\u00E9 au serveur, r\u00E9essaie dans un instant."), CMagenta });
	}
	Fermer();
}

void SChatChelles::Tick(const FGeometry& Geo, const double Temps, const float Delta)
{
	SCompoundWidget::Tick(Geo, Temps, Delta);
	if (bOuvert && !ChatDispo() && !AideDispo())
	{
		Fermer();
	}
	// Si le chat n'est pas dispo (solo ou chat OFF), on affiche l'aide
	if (!bAide && !ChatDispo() && AideDispo())
	{
		ChoisirOnglet(true);
	}
	const int32 Compte = bAide ? GAide.Num() : GJoueurs.Num();
	if (bVuOngletAide != bAide || Compte != (bAide ? VuAide : VuJoueurs))
	{
		Reconstruire();
	}
}

void SChatChelles::Reconstruire()
{
	bVuOngletAide = bAide;
	VuAide = GAide.Num();
	VuJoueurs = GJoueurs.Num();
	if (!Liste.IsValid())
	{
		return;
	}
	Liste->ClearChildren();
	const TArray<FLigneChat>& Lignes = bAide ? GAide : GJoueurs;
	for (const FLigneChat& L : Lignes)
	{
		Liste->AddSlot()
		.Padding(FMargin(0.f, 3.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(L.Auteur))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))
				.ColorAndOpacity(L.Couleur)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(L.Texte))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
				.ColorAndOpacity(CBlanc)
				.AutoWrapText(true)
			]
		];
	}
	Liste->ScrollToEnd();
}
