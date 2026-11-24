// CHELLES 3D - Tourelle offerte apres X minutes de jeu + case TOURELLE dans l'inventaire (voir TourelleCadeau.h)
#include "TourelleCadeau.h"

#include "TourelleChelles.h"

#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
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


#define LOCTEXT_NAMESPACE "ChellesTourelle"

namespace
{
	// Couleurs neon du jeu (menu, site)
	const FLinearColor Cyan = FLinearColor::FromSRGBColor(FColor(43, 231, 255));
	const FLinearColor Rose = FLinearColor::FromSRGBColor(FColor(255, 43, 214));
	const FLinearColor Fond = FLinearColor(0.012f, 0.f, 0.035f, 0.92f);
	const FLinearColor Gris = FLinearColor(0.35f, 0.35f, 0.42f, 1.f);

	/** Nom du fichier de sauvegarde de CE joueur (meme regle que ChellesPouvoirs : -Joueur=Nom, fenetres PIE). */
	FString Emplacement(const UObject* Contexte, const TCHAR* Base)
	{
		FString Nom = Base;
		FString Joueur;
		if (FParse::Value(FCommandLine::Get(), TEXT("Joueur="), Joueur))
		{
			FString Propre;
			for (const TCHAR C : Joueur)
			{
				if (FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-'))
				{
					Propre.AppendChar(C);
				}
			}
			if (!Propre.IsEmpty())
			{
				return Nom + TEXT("_") + Propre.Left(32);
			}
		}
		const UWorld* Monde = Contexte ? Contexte->GetWorld() : nullptr;
		if (Monde && Monde->WorldType == EWorldType::PIE)
		{
			const int32 Instance = Monde->GetOutermost()->PIEInstanceID;
			if (Instance > 0)
			{
				Nom += FString::Printf(TEXT("_PIE%d"), Instance + 1);
			}
		}
		return Nom;
	}

	/** Temps de jeu deja compte par le plugin XP (ChellesPouvoirs), lu sans dependre de lui. */
	double TempsDeLXP(const UObject* Contexte)
	{
		const FString Nom = Emplacement(Contexte, TEXT("ChellesXP"));
		if (!UGameplayStatics::DoesSaveGameExist(Nom, 0))
		{
			return 0.0;
		}
		USaveGame* Sauvegarde = UGameplayStatics::LoadGameFromSlot(Nom, 0);
		if (!Sauvegarde)
		{
			return 0.0;
		}
		if (FDoubleProperty* P = FindFProperty<FDoubleProperty>(Sauvegarde->GetClass(), TEXT("SecondesJouees")))
		{
			return P->GetPropertyValue_InContainer(Sauvegarde);
		}
		if (FFloatProperty* P = FindFProperty<FFloatProperty>(Sauvegarde->GetClass(), TEXT("SecondesJouees")))
		{
			return P->GetPropertyValue_InContainer(Sauvegarde);
		}
		return 0.0;
	}

	FSlateFontInfo Police(int32 Taille, bool bGras = true)
	{
		return FCoreStyle::GetDefaultFontStyle(bGras ? "Bold" : "Regular", Taille);
	}

	/** Un "bouton" neon (les clics sont lus par le filtre, pas par Slate : ils marchent meme sous un autre menu). */
	TSharedRef<SWidget> FaireBouton(const FText& Texte, TFunction<bool()> Survol, TFunction<bool()> Visible, TSharedPtr<SWidget>& Sortie)
	{
		TSharedRef<SWidget> Bouton = SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([Survol]() { return Survol() ? FSlateColor(Rose) : FSlateColor(Cyan); })
			.Padding(FMargin(2.f))
			.Visibility_Lambda([Visible]() { return Visible() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Lambda([Survol]() { return Survol() ? FSlateColor(FLinearColor(0.25f, 0.f, 0.2f, 1.f)) : FSlateColor(FLinearColor(0.02f, 0.f, 0.08f, 1.f)); })
				.Padding(FMargin(16.f, 7.f))
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(Texte)
					.Font(Police(14))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.ShadowColorAndOpacity(Rose)
					.ShadowOffset(FVector2D(1.f, 1.f))
				]
			];
		Sortie = Bouton;
		return Bouton;
	}
}

/** Voit les clics avant le reste de l'ecran : un clic sur POSER / RAMASSER est garde pour la tourelle. */
class FFiltreTourelle : public IInputProcessor
{
public:
	explicit FFiltreTourelle(ATourelleJoueur* InJoueur) : Joueur(InJoueur) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Evenement) override
	{
		ATourelleJoueur* J = Joueur.Get();
		if (J && Evenement.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			return J->Clic(Evenement.GetScreenSpacePosition());
		}
		return false;
	}

private:
	TWeakObjectPtr<ATourelleJoueur> Joueur;
};

// ====================================================================== Joueur

ATourelleJoueur::ATourelleJoueur()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // le menu d'inventaire peut mettre le jeu en pause
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bAlwaysRelevant = false;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ATourelleJoueur::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATourelleJoueur, MesTourelles, COND_OwnerOnly);
}

bool ATourelleJoueur::EstLocal() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC && PC->IsLocalController();
}

int32 ATourelleJoueur::NombrePosees() const
{
	int32 N = 0;
	for (const ATourelleChelles* T : MesTourelles)
	{
		if (IsValid(T))
		{
			++N;
		}
	}
	return N;
}

double ATourelleJoueur::SecondesComptees() const
{
	return ReglagesTourelle().bTempsTotal ? SecondesSauvegardees + SecondesPartie : SecondesPartie;
}

FString ATourelleJoueur::Resume() const
{
	return FString::Printf(TEXT("Joueur %s : %.1f min de jeu (%s), tourelle %s, %d posee(s)"), *GetNameSafe(GetOwner()),
		SecondesComptees() / 60.0, ReglagesTourelle().bTempsTotal ? TEXT("total") : TEXT("partie"),
		bOfferte ? TEXT("OFFERTE") : TEXT("pas encore"), NombrePosees());
}

void ATourelleJoueur::InitialiserLocal(APlayerController* PC)
{
	bLocalPret = true;
	const FReglagesTourelle& R = ReglagesTourelle();

	// ---- Sauvegarde du joueur
	NomSauvegarde = Emplacement(this, TEXT("ChellesTourelle"));
	UTourelleSauvegarde* Donnees = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(NomSauvegarde, 0))
	{
		Donnees = Cast<UTourelleSauvegarde>(UGameplayStatics::LoadGameFromSlot(NomSauvegarde, 0));
	}
	if (Donnees)
	{
		SecondesSauvegardees = Donnees->SecondesJouees;
		bOfferte = Donnees->bOfferte;
	}
	if (!Donnees || !Donnees->bTempsXPRepris)
	{
		// Premiere fois : on reprend le temps deja joue (compte par l'XP) pour ne pas repartir de zero
		const double DejaJoue = TempsDeLXP(this);
		SecondesSauvegardees = FMath::Max(SecondesSauvegardees, DejaJoue);
		UE_LOG(LogTemp, Log, TEXT("[Tourelle] Temps de jeu repris de l'XP : %.1f min"), DejaJoue / 60.0);
		Sauvegarder();
	}

	// ---- Case TOURELLE (a cote du menu d'inventaire) + message du cadeau
	UWorld* Monde = GetWorld();
	UGameViewportClient* LaVue = Monde ? Monde->GetGameViewport() : nullptr;
	if (LaVue)
	{
		TWeakObjectPtr<ATourelleJoueur> Moi(this);
		auto Case = [Moi]() { return Moi.IsValid() && Moi->bCaseVisible; };

		EHorizontalAlignment HA = HAlign_Right;
		EVerticalAlignment VA = VAlign_Center;
		if (R.PlaceCase.Equals(TEXT("Gauche"), ESearchCase::IgnoreCase)) { HA = HAlign_Left; }
		else if (R.PlaceCase.Equals(TEXT("Haut"), ESearchCase::IgnoreCase)) { HA = HAlign_Center; VA = VAlign_Top; }
		else if (R.PlaceCase.Equals(TEXT("Bas"), ESearchCase::IgnoreCase)) { HA = HAlign_Center; VA = VAlign_Bottom; }

		const int32 Minutes = FMath::RoundToInt(R.MinutesAvantCadeau);
		Ecran = SNew(SOverlay)
			.Visibility(EVisibility::HitTestInvisible)
			+ SOverlay::Slot()
			.HAlign(HA)
			.VAlign(VA)
			.Padding(FMargin(40.f))
			[
				SNew(SBox)
				.WidthOverride(330.f)
				.Visibility_Lambda([Case]() { return Case() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(Cyan)
					.Padding(FMargin(2.f))
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(Fond)
						.Padding(FMargin(16.f, 12.f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::Format(LOCTEXT("Bonus", "ARME BONUS - {0} MIN DE JEU"), FText::AsNumber(Minutes)))
								.Font(Police(10))
								.ColorAndOpacity(FSlateColor(Rose))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 6.f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Nom", "TOURELLE TURET 2026"))
								.Font(Police(20))
								.ColorAndOpacity(FSlateColor(Cyan))
								.ShadowColorAndOpacity(Rose)
								.ShadowOffset(FVector2D(1.5f, 1.5f))
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
							[
								SNew(STextBlock)
								.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteEtat() : FText(); })
								.Font(Police(11, false))
								.ColorAndOpacity(FSlateColor(FLinearColor::White))
								.AutoWrapText(true)
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
							[
								SNew(SBox)
								.HeightOverride(8.f)
								.Visibility_Lambda([Moi]() { return Moi.IsValid() && !Moi->APossedeLaTourelle() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
								[
									SNew(SProgressBar)
									.Percent_Lambda([Moi]() { return Moi.IsValid() ? TOptional<float>(Moi->Progression()) : TOptional<float>(0.f); })
									.FillColorAndOpacity(FSlateColor(Rose))
								]
							]
							+ SVerticalBox::Slot().AutoHeight()
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 10.f, 0.f)
								[
									FaireBouton(LOCTEXT("Poser", "POSER"),
										[Moi]() { return Moi.IsValid() && Moi->SurvolPoser(); },
										[Moi]() { return Moi.IsValid() && Moi->APossedeLaTourelle(); },
										BoutonPoser)
								]
								+ SHorizontalBox::Slot().AutoWidth()
								[
									FaireBouton(LOCTEXT("Ramasser", "RAMASSER"),
										[Moi]() { return Moi.IsValid() && Moi->SurvolRamasser(); },
										[Moi]() { return Moi.IsValid() && Moi->APossedeLaTourelle() && Moi->NombrePosees() > 0; },
										BoutonRamasser)
								]
							]
						]
					]
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 140.f, 0.f, 0.f))
			[
				SNew(SBorder)
				.Visibility_Lambda([Moi]() { return Moi.IsValid() && Moi->bMessageVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(Rose)
				.Padding(FMargin(2.f))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(Fond)
					.Padding(FMargin(26.f, 14.f))
					[
						SNew(STextBlock)
						.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteMessage : FText(); })
						.Font(Police(18))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(Cyan))
						.ShadowColorAndOpacity(Rose)
						.ShadowOffset(FVector2D(1.5f, 1.5f))
					]
				]
			];
		LaVue->AddViewportWidgetContent(Ecran.ToSharedRef(), 60);
		Vue = LaVue;
	}
	if (FSlateApplication::IsInitialized())
	{
		Filtre = MakeShared<FFiltreTourelle>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Filtre);
	}
	UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s"), *Resume());
}

void ATourelleJoueur::EndPlay(const EEndPlayReason::Type Raison)
{
	if (bLocalPret)
	{
		Sauvegarder();
	}
	if (Filtre.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Filtre);
	}
	Filtre.Reset();
	if (Ecran.IsValid() && Vue.IsValid())
	{
		Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
	}
	Ecran.Reset();
	if (HasAuthority())
	{
		DetruireTourelles();
	}
	Super::EndPlay(Raison);
}

void ATourelleJoueur::Sauvegarder()
{
	if (NomSauvegarde.IsEmpty())
	{
		return;
	}
	UTourelleSauvegarde* Donnees = Cast<UTourelleSauvegarde>(UGameplayStatics::CreateSaveGameObject(UTourelleSauvegarde::StaticClass()));
	if (!Donnees)
	{
		return;
	}
	Donnees->SecondesJouees = SecondesSauvegardees + SecondesPartie;
	Donnees->bOfferte = bOfferte;
	Donnees->bTempsXPRepris = true;
	if (UGameplayStatics::SaveGameToSlot(Donnees, NomSauvegarde, 0))
	{
		SecondesSauvegardees = Donnees->SecondesJouees;
		SecondesPartie = 0.0;
		// En mode "Partie", on garde le compte de la partie a part
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tourelle] impossible d'ecrire %s.sav"), *NomSauvegarde);
	}
}

void ATourelleJoueur::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController() || !ReglagesTourelle().bActif)
	{
		return;
	}
	if (!bLocalPret)
	{
		InitialiserLocal(PC);
	}
	TickLocal(DeltaSeconds, PC);
}

void ATourelleJoueur::TickLocal(float DeltaSeconds, APlayerController* PC)
{
	const FReglagesTourelle& R = ReglagesTourelle();
	UWorld* Monde = GetWorld();
	const float Maintenant = Monde->GetRealTimeSeconds();

	// ---- Temps de jeu (rien en pause)
	if (!Monde->IsPaused())
	{
		SecondesPartie += DeltaSeconds;
		if (!R.bTempsTotal)
		{
			// compte "dans la partie" : on ne garde pas le temps des autres parties
			SecondesSauvegardees = 0.0;
		}
	}
	if (Maintenant >= ProchaineSauvegarde)
	{
		ProchaineSauvegarde = Maintenant + 60.f;
		if (R.bTempsTotal)
		{
			Sauvegarder();
		}
	}
	if (!bOfferte && SecondesComptees() >= R.MinutesAvantCadeau * 60.0)
	{
		Offrir();
	}

	// ---- Actions demandees par un clic
	if (ActionDemandee != 0)
	{
		const int32 Action = ActionDemandee;
		ActionDemandee = 0;
		if (Action == 1)
		{
			FString Raison;
			if (!Poser(Raison))
			{
				AfficherMessage(FText::FromString(Raison), 3.f);
			}
		}
		else
		{
			Ramasser();
		}
	}
	if (R.ToucheRapide.IsValid() && PC->WasInputKeyJustPressed(R.ToucheRapide) && bOfferte)
	{
		FString Raison;
		if (NombrePosees() > 0 && NombrePosees() >= R.TourellesParJoueur)
		{
			Ramasser();
		}
		else if (!Poser(Raison))
		{
			AfficherMessage(FText::FromString(Raison), 3.f);
		}
	}

	// ---- Case TOURELLE : seulement quand le menu d'inventaire est ouvert
	if (Maintenant >= ProchainControleMenu)
	{
		ProchainControleMenu = Maintenant + 0.2f;
		bCaseVisible = MenuInventaireOuvert();
	}
	if (bMessageVisible && Maintenant >= FinMessage)
	{
		bMessageVisible = false;
	}
}

bool ATourelleJoueur::MenuInventaireOuvert() const
{
	const UWorld* Monde = GetWorld();
	const TArray<FString>& Noms = ReglagesTourelle().MenusInventaire;
	TArray<UUserWidget*> Widgets;
	ChellesListerWidgets(Widgets);
	for (UUserWidget* W : Widgets)
	{
		if (!IsValid(W) || W->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || W->GetWorld() != Monde)
		{
			continue;
		}
		if (!W->IsInViewport() && !W->GetParent())
		{
			continue;
		}
		if (!W->GetCachedWidget().IsValid() || !W->IsVisible())
		{
			continue;
		}
		for (const UClass* K = W->GetClass(); K && K != UUserWidget::StaticClass(); K = K->GetSuperClass())
		{
			const FString Nom = K->GetName();
			for (const FString& Morceau : Noms)
			{
				if (Nom.StartsWith(Morceau, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void ATourelleJoueur::Offrir()
{
	bOfferte = true;
	Sauvegarder();
	const int32 Minutes = FMath::RoundToInt(ReglagesTourelle().MinutesAvantCadeau);
	AfficherMessage(FText::Format(LOCTEXT("Cadeau", "CADEAU ! {0} minutes de jeu : la TOURELLE TURET 2026\nest dans votre inventaire (touche I)"),
		FText::AsNumber(Minutes)), 9.f);
	JouerSon2D(3, 0.8f);
	UE_LOG(LogTemp, Log, TEXT("[Tourelle] Tourelle offerte a %s (%.1f min de jeu)"), *GetNameSafe(GetOwner()), SecondesComptees() / 60.0);
}

void ATourelleJoueur::AfficherMessage(const FText& Texte, float Duree)
{
	TexteMessage = Texte;
	bMessageVisible = true;
	FinMessage = GetWorld()->GetRealTimeSeconds() + Duree;
}

void ATourelleJoueur::JouerSon2D(int32 Genre, float Volume)
{
	USonTourelleChelles* Son = NewObject<USonTourelleChelles>(this);
	Son->Genre = Genre;
	UAudioComponent* Lecteur = UGameplayStatics::SpawnSound2D(this, Son, Volume, 1.f, 0.f, nullptr, false, true);
	if (!Lecteur)
	{
		return;
	}
	TWeakObjectPtr<UAudioComponent> Faible(Lecteur);
	FTimerHandle Minuteur;
	GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
	{
		if (Faible.IsValid())
		{
			Faible->Stop();
		}
	}), Genre == 3 ? 0.9f : 0.3f, false);
}

FText ATourelleJoueur::TexteEtat() const
{
	const FReglagesTourelle& R = ReglagesTourelle();
	if (!bOfferte)
	{
		const int32 Reste = FMath::Max(1, FMath::CeilToInt((R.MinutesAvantCadeau * 60.0 - SecondesComptees()) / 60.0));
		return FText::Format(LOCTEXT("Bloquee", "Offerte apres {0} minutes de jeu.\nEncore {1} min."),
			FText::AsNumber(FMath::RoundToInt(R.MinutesAvantCadeau)), FText::AsNumber(Reste));
	}
	const int32 N = NombrePosees();
	if (N == 0)
	{
		return LOCTEXT("Prete", "Dans votre inventaire. POSER = elle se pose devant vous et tire toute seule sur les ennemis.");
	}
	return FText::Format(LOCTEXT("Posee", "Posee sur la carte ({0}/{1}). POSER = la deplacer devant vous, RAMASSER = la reprendre."),
		FText::AsNumber(N), FText::AsNumber(R.TourellesParJoueur));
}

float ATourelleJoueur::Progression() const
{
	const double Total = FMath::Max(1.0, (double)ReglagesTourelle().MinutesAvantCadeau * 60.0);
	return (float)FMath::Clamp(SecondesComptees() / Total, 0.0, 1.0);
}

bool ATourelleJoueur::SurBouton(const TSharedPtr<SWidget>& Bouton, const FVector2D& Position)
{
	if (!Bouton.IsValid() || Bouton->GetVisibility() == EVisibility::Collapsed)
	{
		return false;
	}
	const FGeometry& Geo = Bouton->GetTickSpaceGeometry();
	return Geo.GetLocalSize().X > 1.f && Geo.IsUnderLocation(Position);
}

bool ATourelleJoueur::SurvolPoser() const
{
	return bCaseVisible && FSlateApplication::IsInitialized() && SurBouton(BoutonPoser, FSlateApplication::Get().GetCursorPos());
}

bool ATourelleJoueur::SurvolRamasser() const
{
	return bCaseVisible && FSlateApplication::IsInitialized() && SurBouton(BoutonRamasser, FSlateApplication::Get().GetCursorPos());
}

bool ATourelleJoueur::Clic(const FVector2D& PositionEcran)
{
	if (!bCaseVisible || !bOfferte)
	{
		return false;
	}
	if (SurBouton(BoutonPoser, PositionEcran))
	{
		ActionDemandee = 1;
		return true;
	}
	if (NombrePosees() > 0 && SurBouton(BoutonRamasser, PositionEcran))
	{
		ActionDemandee = 2;
		return true;
	}
	return false;
}

bool ATourelleJoueur::Poser(FString& Raison)
{
	if (!bOfferte)
	{
		Raison = FString::Printf(TEXT("Tourelle offerte apres %d minutes de jeu."), FMath::RoundToInt(ReglagesTourelle().MinutesAvantCadeau));
		return false;
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		Raison = TEXT("Pas de personnage pour poser la tourelle.");
		return false;
	}
	// Devant le joueur, posee sur le sol
	const FRotator Direction(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector Devant = Pion->GetActorLocation() + Direction.Vector() * ReglagesTourelle().DistancePose;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PoseTourelleChelles), false, Pion);
	FHitResult Sol;
	FVector Lieu = Devant;
	if (GetWorld()->LineTraceSingleByChannel(Sol, Devant + FVector(0.f, 0.f, 200.f), Devant - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		Lieu = Sol.ImpactPoint;
	}
	else
	{
		Lieu.Z -= 90.f; // a peu pres les pieds du personnage
	}
	ServeurPoser(Lieu, Direction.Yaw);
	JouerSon2D(1, 0.5f);
	if (ReglagesTourelle().bFermerInventaire && bCaseVisible)
	{
		// Le plugin ChellesInventaire ferme proprement le menu d'armes (rien ne se passe s'il est absent)
		PC->ConsoleCommand(TEXT("Chelles.Inventaire Fermer"), false);
	}
	return true;
}

void ATourelleJoueur::Ramasser()
{
	ServeurRamasser();
	JouerSon2D(2, 0.5f);
}

void ATourelleJoueur::ReglerTemps(float Minutes)
{
	SecondesSauvegardees = ReglagesTourelle().bTempsTotal ? FMath::Max(0.0, (double)Minutes * 60.0) : 0.0;
	SecondesPartie = ReglagesTourelle().bTempsTotal ? 0.0 : FMath::Max(0.0, (double)Minutes * 60.0);
	if (SecondesComptees() < ReglagesTourelle().MinutesAvantCadeau * 60.0)
	{
		bOfferte = false; // pour refaire l'essai du cadeau
	}
	if (ReglagesTourelle().bTempsTotal)
	{
		Sauvegarder();
	}
	UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s"), *Resume());
}

// ---------------------------------------------------------------------- Serveur

void ATourelleJoueur::ServeurPoser_Implementation(FVector Lieu, float Cap)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	const FReglagesTourelle& R = ReglagesTourelle();
	if (!Pion || !R.bActif || FVector::Dist(Pion->GetActorLocation(), Lieu) > R.DistancePose + 1500.f)
	{
		return;
	}
	MesTourelles.RemoveAll([](ATourelleChelles* T) { return !IsValid(T); });
	// Deja le maximum : la plus ancienne est deplacee
	while (MesTourelles.Num() >= R.TourellesParJoueur)
	{
		if (MesTourelles[0])
		{
			MesTourelles[0]->Destroy();
		}
		MesTourelles.RemoveAt(0);
	}
	UClass* Classe = R.ClassePosee.IsEmpty() ? nullptr : LoadClass<ATourelleChelles>(nullptr, *R.ClassePosee, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Classe)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tourelle] %s introuvable : tourelle en formes simples"), *R.ClassePosee);
		Classe = ATourelleChelles::StaticClass();
	}
	FActorSpawnParameters Params;
	Params.Owner = PC;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ATourelleChelles* Tourelle = GetWorld()->SpawnActor<ATourelleChelles>(Classe, Lieu, FRotator(0.f, Cap, 0.f), Params);
	if (!Tourelle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tourelle] Impossible de poser la tourelle de %s"), *GetNameSafe(PC));
		return;
	}
	Tourelle->Tireur = PC;
	Tourelle->Tags.AddUnique(FName(TEXT("ChellesTourelleJoueur")));
	MesTourelles.Add(Tourelle);
	UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s pose %s en %s"), *GetNameSafe(PC), *Tourelle->GetName(), *Lieu.ToString());
}

void ATourelleJoueur::ServeurRamasser_Implementation()
{
	DetruireTourelles();
}

void ATourelleJoueur::DetruireTourelles()
{
	for (ATourelleChelles* T : MesTourelles)
	{
		if (IsValid(T))
		{
			T->Destroy();
		}
	}
	MesTourelles.Reset();
}

// ====================================================================== Gestionnaire (serveur)

ATourellesChelles::ATourellesChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ATourellesChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	// Joueurs partis : on enleve leur relais (et leurs tourelles)
	for (auto It = Joueurs.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (ATourelleJoueur* Ancien = It->Value.Get())
			{
				Ancien->Destroy();
			}
			It.RemoveCurrent();
		}
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		TWeakObjectPtr<ATourelleJoueur>& Relais = Joueurs.FindOrAdd(PC);
		if (Relais.IsValid())
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Relais = Monde->SpawnActor<ATourelleJoueur>(ATourelleJoueur::StaticClass(), FTransform::Identity, Params);
	}
}

#undef LOCTEXT_NAMESPACE
