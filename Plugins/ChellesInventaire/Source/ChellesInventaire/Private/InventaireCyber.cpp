// CHELLES 3D - Menu bibliotheque d'armes cyberpunk + bouton QUITTER (voir InventaireCyber.h).

#include "InventaireCyber.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"

// Optimisation 24/11/2026 : liste rapide des widgets (table par classe d'Unreal)
// au lieu de parcourir TOUS les objets du jeu (TObjectIterator) 5 fois par seconde.
static void ChellesListerWidgets(TArray<UUserWidget*>& Sortie)
{
	TArray<UObject*> Objets;
	GetObjectsOfClass(UUserWidget::StaticClass(), Objets, true, RF_ClassDefaultObject | RF_ArchetypeObject, EInternalObjectFlags::PendingKill);
	Sortie.Reset(Objets.Num());
	for (UObject* O : Objets)
	{
		if (UUserWidget* W = Cast<UUserWidget>(O))
		{
			Sortie.Add(W);
		}
	}
}


namespace
{
	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesInventaire"), TEXT("Inventaire.txt"));
	}

	bool EstOui(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		return T.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || T.Equals(TEXT("1")) || T.Equals(TEXT("True"), ESearchCase::IgnoreCase) || T.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	/** "0,240,255" -> couleur (0 a 255 par composante). Rend false si la ligne est mal ecrite. */
	bool LireCouleur(const FString& Texte, FLinearColor& Couleur)
	{
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(","), true);
		if (Morceaux.Num() < 3)
		{
			return false;
		}
		const float R = FMath::Clamp(FCString::Atof(*Morceaux[0].TrimStartAndEnd()), 0.f, 255.f) / 255.f;
		const float G = FMath::Clamp(FCString::Atof(*Morceaux[1].TrimStartAndEnd()), 0.f, 255.f) / 255.f;
		const float B = FMath::Clamp(FCString::Atof(*Morceaux[2].TrimStartAndEnd()), 0.f, 255.f) / 255.f;
		// Les couleurs de l'interface sont en sRGB : 0-255 comme dans un logiciel de dessin
		Couleur = FLinearColor::FromSRGBColor(FColor(FMath::RoundToInt(R * 255.f), FMath::RoundToInt(G * 255.f), FMath::RoundToInt(B * 255.f)));
		return true;
	}

	FLinearColor AvecAlpha(const FLinearColor& C, float A)
	{
		return FLinearColor(C.R, C.G, C.B, A);
	}

	bool EstPresqueBlanc(const FLinearColor& C)
	{
		return C.R > 0.7f && C.G > 0.7f && C.B > 0.7f;
	}

	/** Nom de la texture d'un pinceau (vide si couleur unie). */
	FString NomTexture(const FSlateBrush& Pinceau)
	{
		const UObject* Ressource = Pinceau.GetResourceObject();
		return Ressource ? Ressource->GetName() : FString();
	}

	bool EstUnCadre(const FString& Nom)
	{
		return Nom.Contains(TEXT("Border")) || Nom.Contains(TEXT("Frame")) || Nom.Contains(TEXT("Cadre"));
	}

	bool EstUnFond(const FString& Nom)
	{
		return Nom.IsEmpty() || Nom.Contains(TEXT("Background")) || Nom.Contains(TEXT("Blank"));
	}
}

/** Voit chaque clic et chaque touche avant le reste de l'ecran (ne les bloque pas). */
class FFermetureInventaire : public IInputProcessor
{
public:
	explicit FFermetureInventaire(AInventaireCyber* InProprietaire) : Proprietaire(InProprietaire) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Evenement) override
	{
		AInventaireCyber* A = Proprietaire.Get();
		if (A && Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			return A->ClicDansMenu(Evenement.GetScreenSpacePosition()); // true = clic pris par le menu
		}
		return false; // ailleurs, le clic continue son chemin normalement
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Evenement) override
	{
		AInventaireCyber* A = Proprietaire.Get();
		if (A && Evenement.GetKey() == EKeys::Escape && !Evenement.IsRepeat())
		{
			return A->DemanderFermeture(); // Echap ferme le menu (et ne fait rien d'autre dans ce cas)
		}
		return false;
	}

private:
	TWeakObjectPtr<AInventaireCyber> Proprietaire;
};

AInventaireCyber::AInventaireCyber()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // le menu peut mettre le jeu en pause
	SetReplicates(false);
}

void AInventaireCyber::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	if (FSlateApplication::IsInitialized())
	{
		Filtre = MakeShared<FFermetureInventaire>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Filtre);
	}
	UE_LOG(LogTemp, Log, TEXT("[Inventaire] Pret (actif=%d, quitter=%d)."), Reglages.bActif ? 1 : 0, Reglages.bQuitterRepare ? 1 : 0);
}

void AInventaireCyber::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Filtre.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Filtre);
	}
	Filtre.Reset();
	Super::EndPlay(Raison);
}

bool AInventaireCyber::VisibleSousCurseur(UButton* Bouton, const FVector2D& PositionEcran)
{
	if (!Bouton || !Bouton->GetCachedWidget().IsValid() || !Bouton->IsVisible())
	{
		return false;
	}
	if (!Bouton->GetCachedGeometry().IsUnderLocation(PositionEcran))
	{
		return false;
	}
	// Une case cachee par le defilement (liste BACKPACK) ne compte pas : le curseur doit aussi etre
	// dans chaque liste qui la contient. On remonte aussi a travers les cases (widgets dans le widget).
	UWidget* Courant = Bouton;
	for (int32 Garde = 0; Courant && Garde < 64; ++Garde)
	{
		if (!Courant->IsVisible())
		{
			return false;
		}
		if (UScrollBox* Liste = Cast<UScrollBox>(Courant))
		{
			if (Liste->GetCachedWidget().IsValid() && !Liste->GetCachedGeometry().IsUnderLocation(PositionEcran))
			{
				return false;
			}
		}
		UWidget* Parent = Courant->GetParent();
		if (!Parent)
		{
			// Racine d'une case : on continue avec la case elle-meme (UserWidget qui la contient)
			UUserWidget* Case = Courant->GetTypedOuter<UUserWidget>();
			Parent = (Case && Case != Courant) ? Case : nullptr;
		}
		Courant = Parent;
	}
	return true;
}

bool AInventaireCyber::ClicDansMenu(const FVector2D& PositionEcran)
{
	if (!Reglages.bActif || !Reglages.bQuitterRepare || Ouverts.Num() == 0)
	{
		return false;
	}
	UButton* Meilleur = nullptr;
	float PlusPetit = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UButton>& Faible : BoutonsMenus)
	{
		UButton* Bouton = Faible.Get();
		if (!VisibleSousCurseur(Bouton, PositionEcran))
		{
			continue;
		}
		// Plusieurs boutons l'un dans l'autre : on prend le plus petit (celui du dessus)
		const FVector2D Taille = Bouton->GetCachedGeometry().GetAbsoluteSize();
		const float Aire = Taille.X * Taille.Y;
		if (Aire < PlusPetit)
		{
			PlusPetit = Aire;
			Meilleur = Bouton;
		}
	}
	if (!Meilleur)
	{
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[Inventaire] Clic sur %s."), *Meilleur->GetName());
	BoutonAClic = Meilleur;
	return true;
}

void AInventaireCyber::RamasserBoutons(UUserWidget* Widget, int32 Profondeur)
{
	if (!Widget || !Widget->WidgetTree || Profondeur > 6)
	{
		return;
	}
	TArray<UUserWidget*> Enfants;
	Widget->WidgetTree->ForEachWidget([this, &Enfants](UWidget* Element)
	{
		if (UUserWidget* Sous = Cast<UUserWidget>(Element))
		{
			Enfants.Add(Sous);
		}
		else if (UButton* Bouton = Cast<UButton>(Element))
		{
			BoutonsMenus.AddUnique(Bouton);
		}
	});
	for (UUserWidget* Sous : Enfants)
	{
		RamasserBoutons(Sous, Profondeur + 1);
	}
}

bool AInventaireCyber::DemanderFermeture()
{
	if (!Reglages.bActif || !Reglages.bQuitterRepare || Ouverts.Num() == 0)
	{
		return false;
	}
	bFermetureDemandee = true;
	return true;
}

void AInventaireCyber::ChargerReglages()
{
	Reglages = FReglagesInventaire();
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *CheminReglages()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventaire] %s introuvable : reglages par defaut."), *CheminReglages());
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
		{
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		if (Cle == TEXT("Actif")) Reglages.bActif = EstOui(Valeur);
		else if (Cle == TEXT("QuitterRepare")) Reglages.bQuitterRepare = EstOui(Valeur);
		else if (Cle == TEXT("CouleurCadres")) LireCouleur(Valeur, Reglages.Cadres);
		else if (Cle == TEXT("CouleurTexte")) LireCouleur(Valeur, Reglages.Texte);
		else if (Cle == TEXT("CouleurLueur")) LireCouleur(Valeur, Reglages.Lueur);
		else if (Cle == TEXT("CouleurFond")) LireCouleur(Valeur, Reglages.Fond);
		else if (Cle == TEXT("OpaciteFond")) Reglages.OpaciteFond = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 100.f) / 100.f;
		else if (Cle == TEXT("CouleurBouton")) LireCouleur(Valeur, Reglages.Bouton);
		else if (Cle == TEXT("CouleurBoutonSurvol")) LireCouleur(Valeur, Reglages.BoutonSurvol);
		else if (Cle == TEXT("CouleurBoutonClic")) LireCouleur(Valeur, Reglages.BoutonClic);
		else if (Cle == TEXT("CouleurIcones")) LireCouleur(Valeur, Reglages.Icones);
	}
}

bool AInventaireCyber::EstMenuInventaire(const UUserWidget* Widget)
{
	for (const UClass* K = Widget ? Widget->GetClass() : nullptr; K; K = K->GetSuperClass())
	{
		if (K->GetName().StartsWith(TEXT("W_WeaponInventory_UI")))
		{
			return true;
		}
	}
	return false;
}

void AInventaireCyber::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Reglages.bActif)
	{
		return;
	}
	if (UButton* Bouton = BoutonAClic.Get())
	{
		BoutonAClic = nullptr;
		Bouton->OnClicked.Broadcast(); // comme un vrai clic : Equip, Drop, Use Item, case d'arme, Close...
	}
	if (bFermetureDemandee)
	{
		bFermetureDemandee = false;
		FermerInventaire();
	}
	const float Maintenant = GetWorld()->GetRealTimeSeconds();
	if (Maintenant < ProchainControle)
	{
		return;
	}
	ProchainControle = Maintenant + 0.2f;

	Ouverts.Reset();
	BoutonsQuitter.Reset();
	BoutonsMenus.Reset();
	UWorld* Monde = GetWorld();
	TArray<UUserWidget*> Widgets;
	ChellesListerWidgets(Widgets);
	for (UUserWidget* W : Widgets)
	{
		if (!IsValid(W) || W->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || W->GetWorld() != Monde)
		{
			continue;
		}
		if (!EstMenuInventaire(W))
		{
			continue;
		}
		const bool bAffiche = (W->IsInViewport() || W->GetParent() != nullptr) && W->GetCachedWidget().IsValid();
		if (!bAffiche)
		{
			continue;
		}
		Ouverts.Add(W);
		Styliser(W, 0);
		if (Reglages.bQuitterRepare)
		{
			BrancherQuitter(W);
			RamasserBoutons(W, 0);
			// La souris doit etre visible pour se servir du menu
			if (APlayerController* PC = W->GetOwningPlayer())
			{
				PC->bShowMouseCursor = true;
			}
		}
	}
}

void AInventaireCyber::Styliser(UUserWidget* Widget, int32 Profondeur)
{
	if (!Widget || !Widget->WidgetTree || Profondeur > 6)
	{
		return;
	}
	TArray<UUserWidget*> Enfants;
	Widget->WidgetTree->ForEachWidget([this, &Enfants](UWidget* Element)
	{
		if (UUserWidget* Sous = Cast<UUserWidget>(Element))
		{
			Enfants.Add(Sous); // les cases d'armes (W_SlotData_UI...) ont leur propre arbre
			return;
		}
		if (Element && !DejaStylises.Contains(Element))
		{
			DejaStylises.Add(Element);
			StyliserUn(Element);
		}
	});
	for (UUserWidget* Sous : Enfants)
	{
		Styliser(Sous, Profondeur + 1);
	}
}

void AInventaireCyber::StyliserUn(UWidget* Element)
{
	if (UBorder* Bord = Cast<UBorder>(Element))
	{
		const FLinearColor Avant = Bord->BrushColor;
		if (Avant.A < 0.01f)
		{
			return; // invisible dans le pack : on n'y touche pas
		}
		const FString Nom = NomTexture(Bord->Background);
		if (EstUnFond(Nom))
		{
			Bord->SetBrushColor(AvecAlpha(Reglages.Fond, FMath::Min(Avant.A, Reglages.OpaciteFond)));
		}
		else
		{
			Bord->SetBrushColor(AvecAlpha(Reglages.Cadres, Avant.A)); // les cadres blancs deviennent neon
		}
		return;
	}
	if (UImage* Img = Cast<UImage>(Element))
	{
		const FLinearColor Avant = Img->ColorAndOpacity;
		if (Avant.A < 0.01f)
		{
			return;
		}
		const FString Nom = NomTexture(Img->Brush);
		if (EstUnCadre(Nom))
		{
			Img->SetColorAndOpacity(AvecAlpha(Reglages.Cadres, Avant.A));
		}
		else if (EstUnFond(Nom))
		{
			Img->SetColorAndOpacity(AvecAlpha(Reglages.Fond, FMath::Min(Avant.A, Reglages.OpaciteFond)));
		}
		else
		{
			Img->SetColorAndOpacity(AvecAlpha(Reglages.Icones, Avant.A)); // icones des armes
		}
		return;
	}
	if (UTextBlock* Texte = Cast<UTextBlock>(Element))
	{
		if (Texte->ColorAndOpacity.IsColorSpecified())
		{
			const FLinearColor Avant = Texte->ColorAndOpacity.GetSpecifiedColor();
			if (EstPresqueBlanc(Avant))
			{
				Texte->SetColorAndOpacity(FSlateColor(AvecAlpha(Reglages.Texte, Avant.A)));
			}
		}
		Texte->SetShadowColorAndOpacity(AvecAlpha(Reglages.Lueur, 0.85f));
		Texte->SetShadowOffset(FVector2D(1.5f, 1.5f));
		return;
	}
	if (UButton* Bouton = Cast<UButton>(Element))
	{
		FButtonStyle Style = Bouton->WidgetStyle;
		Style.Normal.TintColor = FSlateColor(AvecAlpha(Reglages.Bouton, 0.9f));
		Style.Hovered.TintColor = FSlateColor(AvecAlpha(Reglages.BoutonSurvol, 0.95f));
		Style.Pressed.TintColor = FSlateColor(AvecAlpha(Reglages.BoutonClic, 1.f));
		Bouton->SetStyle(Style);
		return;
	}
}

void AInventaireCyber::BrancherQuitter(UUserWidget* Menu)
{
	if (!Menu->WidgetTree)
	{
		return;
	}
	UButton* Quitter = Cast<UButton>(Menu->WidgetTree->FindWidget(TEXT("CloseInventory_BTN")));
	if (!Quitter)
	{
		// Au cas ou le bouton a un autre nom : on cherche un bouton qui contient Close ou Quit
		Menu->WidgetTree->ForEachWidget([&Quitter](UWidget* Element)
		{
			UButton* B = Cast<UButton>(Element);
			if (!Quitter && B && (B->GetName().Contains(TEXT("Close")) || B->GetName().Contains(TEXT("Quit"))))
			{
				Quitter = B;
			}
		});
	}
	if (Quitter)
	{
		BoutonsQuitter.AddUnique(Quitter);
		if (!DejaBranches.Contains(Menu))
		{
			DejaBranches.Add(Menu);
			Quitter->SetIsEnabled(true);
			Quitter->OnClicked.AddUniqueDynamic(this, &AInventaireCyber::FermerInventaire);
			UE_LOG(LogTemp, Log, TEXT("[Inventaire] Bouton QUITTER branche sur %s (%s)."), *Quitter->GetName(), *Menu->GetClass()->GetName());
		}
	}
	else if (!DejaBranches.Contains(Menu))
	{
		DejaBranches.Add(Menu);
		UE_LOG(LogTemp, Warning, TEXT("[Inventaire] Bouton QUITTER introuvable dans %s."), *Menu->GetClass()->GetName());
	}
}

void AInventaireCyber::FermerInventaire()
{
	// On garde la liste a part : RemoveFromParent peut changer les widgets pendant la boucle
	TArray<TWeakObjectPtr<UUserWidget>> AFermer = Ouverts;
	if (AFermer.Num() == 0)
	{
		UWorld* Monde = GetWorld();
		TArray<UUserWidget*> Widgets;
		ChellesListerWidgets(Widgets);
		for (UUserWidget* W : Widgets)
		{
			if (IsValid(W) && W->GetWorld() == Monde && EstMenuInventaire(W) && (W->IsInViewport() || W->GetParent()))
			{
				AFermer.Add(W);
			}
		}
	}
	for (const TWeakObjectPtr<UUserWidget>& Faible : AFermer)
	{
		if (UUserWidget* Menu = Faible.Get())
		{
			RendreLaMain(Menu);
		}
	}
	Ouverts.Reset();
	BoutonsQuitter.Reset();
	BoutonsMenus.Reset();
}

void AInventaireCyber::RendreLaMain(UUserWidget* Menu)
{
	APlayerController* PC = Menu->GetOwningPlayer();
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	Menu->RemoveFromParent();
	UE_LOG(LogTemp, Log, TEXT("[Inventaire] Menu %s ferme par QUITTER."), *Menu->GetClass()->GetName());
	if (!PC)
	{
		return;
	}
	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
	PC->ResetIgnoreInputFlags();
	if (APawn* Pion = PC->GetPawn())
	{
		Pion->EnableInput(PC);
		// Le personnage retient souvent "inventaire ouvert" : on le remet a faux pour que la touche rouvre le menu
		for (TFieldIterator<FBoolProperty> It(Pion->GetClass()); It; ++It)
		{
			const FString Nom = It->GetName();
			if (Nom.Contains(TEXT("Inventory"), ESearchCase::IgnoreCase)
				&& (Nom.Contains(TEXT("Open"), ESearchCase::IgnoreCase) || Nom.Contains(TEXT("Show"), ESearchCase::IgnoreCase)
					|| Nom.Contains(TEXT("Visible"), ESearchCase::IgnoreCase) || Nom.Contains(TEXT("Active"), ESearchCase::IgnoreCase)))
			{
				It->SetPropertyValue_InContainer(Pion, false);
			}
		}
	}
}

void AInventaireCyber::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
	if (Quoi == TEXT("0"))
	{
		Reglages.bActif = false;
	}
	else if (Quoi == TEXT("1"))
	{
		Reglages.bActif = true;
	}
	else if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		ChargerReglages();
		DejaStylises.Reset(); // recolorer avec les nouvelles couleurs
	}
	else if (Quoi.Equals(TEXT("Fermer"), ESearchCase::IgnoreCase))
	{
		FermerInventaire();
	}
	UE_LOG(LogTemp, Display, TEXT("[Inventaire] actif=%d quitter=%d menus ouverts=%d elements colores=%d"),
		Reglages.bActif ? 1 : 0, Reglages.bQuitterRepare ? 1 : 0, Ouverts.Num(), DejaStylises.Num());
}
