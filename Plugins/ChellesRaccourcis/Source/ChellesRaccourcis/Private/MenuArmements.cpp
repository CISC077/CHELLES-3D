// CHELLES 3D - Menu ARMEMENTS (touche I). Voir MenuArmements.h
#include "MenuArmements.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SMenuArmements.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

AMenuArmements::AMenuArmements()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorHiddenInGame(true);
	bReplicates = false; // un par machine (ecran du joueur)
	InputPriority = 26;  // passe avant la roue des armes (25) et le personnage (touche I du kit FPS)
	Touche = EKeys::I;
}

void AMenuArmements::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	LireCoffre();
	LierClavier();
}

void AMenuArmements::EndPlay(const EEndPlayReason::Type Raison)
{
	Fermer();
	Super::EndPlay(Raison);
}

void AMenuArmements::ChargerReglages()
{
	bActif = true;
	Touche = EKeys::I;
	bRemplacerInventaire = true;

	const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesRaccourcis"), TEXT("Raccourcis.txt"));
	TArray<FString> Lignes;
	FFileHelper::LoadFileToStringArray(Lignes, *Fichier);
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		FString Cle, Valeur;
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		const bool bOui = Valeur.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Valeur == TEXT("1");
		if (Cle.Equals(TEXT("MenuArmements"), ESearchCase::IgnoreCase))
		{
			bActif = bOui;
		}
		else if (Cle.Equals(TEXT("ToucheMenuArmements"), ESearchCase::IgnoreCase))
		{
			const FKey K(*Valeur);
			if (K.IsValid())
			{
				Touche = K;
			}
		}
		else if (Cle.Equals(TEXT("RemplacerInventaire"), ESearchCase::IgnoreCase))
		{
			bRemplacerInventaire = bOui;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[MenuArmements] Pret (%s, touche %s, ancien inventaire du kit %s)."),
		bActif ? TEXT("actif") : TEXT("coupe"), *Touche.ToString(), bRemplacerInventaire ? TEXT("remplace") : TEXT("garde"));
}

APlayerController* AMenuArmements::GetPC() const
{
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return (PC && PC->IsLocalController()) ? PC : nullptr;
}

ARoueArmes* AMenuArmements::Roue() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<ARoueArmes> It(W); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AMenuArmements::Message(const FString& Texte) const
{
	if (Widget.IsValid())
	{
		Widget->Dire(Texte);
	}
	else if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(int32(0xC4E116), 3.f, FColor(160, 200, 255), Texte);
	}
}

void AMenuArmements::LierClavier()
{
	if (bLieClavier || !bRemplacerInventaire)
	{
		return; // sans remplacement, la touche est lue dans Tick (le kit garde aussi la sienne)
	}
	if (APlayerController* PC = GetPC())
	{
		if (APlayerController* Ancien = PCLie.Get())
		{
			DisableInput(Ancien);
		}
		if (InputComponent)
		{
			InputComponent->KeyBindings.Reset();
		}
		EnableInput(PC);
		if (InputComponent && Touche.IsValid())
		{
			// La liaison "avale" la touche : le personnage du kit FPS ne recoit plus I (son vieux menu d'inventaire)
			InputComponent->BindKey(Touche, IE_Pressed, this, &AMenuArmements::Basculer);
			bLieClavier = true;
			PCLie = PC;
		}
	}
}

void AMenuArmements::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = GetPC();
	if (bLieClavier && PCLie.Get() != PC)
	{
		bLieClavier = false; // nouveau PlayerController (menu -> partie)
	}
	if (!bLieClavier)
	{
		LierClavier();
	}
	// Lecture directe de la touche en plus de la liaison (comme la roue TAB) : marche meme si un autre element la prend avant
	if (PC && bActif && !bOuvert && Touche.IsValid() && PC->WasInputKeyJustPressed(Touche))
	{
		Basculer();
	}

	const float Maintenant = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	if (bOuvert)
	{
		if (!Widget.IsValid() || !PC)
		{
			Fermer();
			return;
		}
		if (Maintenant < SurveillerJusqua && bRemplacerInventaire && FermerInventaireFPS())
		{
			ModeInterface(); // le vieux menu a repris la souris : on la reprend
		}
		for (int32 i = Rafraichissements.Num() - 1; i >= 0; --i)
		{
			if (Maintenant >= Rafraichissements[i])
			{
				Rafraichissements.RemoveAt(i);
				Rafraichir();
			}
		}
	}
}

void AMenuArmements::Basculer()
{
	if (ImageBasculee == GFrameCounter)
	{
		return; // liaison + lecture directe dans la meme image : une seule fois
	}
	ImageBasculee = GFrameCounter;
	if (bOuvert)
	{
		Fermer();
	}
	else
	{
		Ouvrir();
	}
}

void AMenuArmements::ModeInterface()
{
	APlayerController* PC = GetPC();
	if (!PC || !Widget.IsValid())
	{
		return;
	}
	PC->bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Widget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
	}
}

void AMenuArmements::Ouvrir()
{
	APlayerController* PC = GetPC();
	UWorld* Monde = GetWorld();
	if (!bActif || bOuvert || !PC || !Monde || PC->IsPaused())
	{
		return;
	}
	ARoueArmes* R = Roue();
	if (!R)
	{
		Message(TEXT("Menu ARMEMENTS : roue des armes introuvable (plugin ChellesRaccourcis)"));
		return;
	}
	UGameViewportClient* Vue = Monde->GetGameViewport();
	if (!Vue)
	{
		return;
	}

	Widget = SNew(SMenuArmements);
	Widget->ToucheMenu = Touche;
	TWeakObjectPtr<AMenuArmements> Moi(this);
	Widget->OnEquiper = [Moi](int32 i) { if (Moi.IsValid()) Moi->Equiper(i); };
	Widget->OnCoffre = [Moi](int32 i) { if (Moi.IsValid()) Moi->BasculerCoffre(i); };
	Widget->OnLacher = [Moi](int32 i) { if (Moi.IsValid()) Moi->Lacher(i); };
	Widget->OnOnglet = [Moi](int32 i) { if (Moi.IsValid()) Moi->Onglet(i); };
	Widget->OnFermer = [Moi]() { if (Moi.IsValid()) Moi->Fermer(); };
	Widget->OnRelire = [Moi]() { if (Moi.IsValid()) Moi->Rafraichir(); };
	// Les messages de la roue (pas en mode Soldat, deja en main...) s'affichent dans le menu
	R->Ecouteur = [Moi](const FString& Texte)
	{
		if (Moi.IsValid() && Moi->Widget.IsValid())
		{
			Moi->Widget->Dire(Texte);
		}
	};

	bOuvert = true;
	Rafraichir();
	Vue->AddViewportWidgetContent(Widget.ToSharedRef(), 120);
	PC->FlushPressedKeys();
	ModeInterface();
	SurveillerJusqua = Monde->GetRealTimeSeconds() + 1.5f;
	if (bRemplacerInventaire)
	{
		FermerInventaireFPS();
	}
	UE_LOG(LogTemp, Log, TEXT("[MenuArmements] Menu ouvert (%d armes)."), Cibles.Num());
}

void AMenuArmements::Fermer()
{
	if (!bOuvert)
	{
		return;
	}
	bOuvert = false;
	Rafraichissements.Reset();
	if (ARoueArmes* R = Roue())
	{
		R->Ecouteur = nullptr;
	}
	UWorld* Monde = GetWorld();
	if (Widget.IsValid() && Monde)
	{
		if (UGameViewportClient* Vue = Monde->GetGameViewport())
		{
			Vue->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	Widget.Reset();
	Images.Reset();
	Cibles.Reset();
	ImageBasculee = GFrameCounter; // la touche qui ferme ne rouvre pas tout de suite
	if (APlayerController* PC = GetPC())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		PC->FlushPressedKeys();
	}
}

void AMenuArmements::Rafraichir()
{
	ARoueArmes* R = Roue();
	if (!R || !Widget.IsValid())
	{
		return;
	}
	TArray<FCaseArme> Cases;
	R->ListerArmes(Cases, Cibles, Images);
	Widget->MettreAJour(Cases, LeCoffre);
}

void AMenuArmements::RafraichirBientot()
{
	const float Maintenant = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	Rafraichissements.Reset();
	Rafraichissements.Add(Maintenant + 0.15f);
	Rafraichissements.Add(Maintenant + 0.8f);
	Rafraichissements.Add(Maintenant + 1.8f); // changement d'arme avec animation : il faut attendre un peu
}

void AMenuArmements::Equiper(int32 Index)
{
	ARoueArmes* R = Roue();
	if (!R || !Cibles.IsValidIndex(Index))
	{
		return;
	}
	R->ChoisirCible(Cibles[Index]);
	RafraichirBientot();
}

void AMenuArmements::Lacher(int32 Index)
{
	ARoueArmes* R = Roue();
	if (!R || !Cibles.IsValidIndex(Index))
	{
		return;
	}
	// L'arme en main se lit a nouveau au moment du clic
	TArray<FCaseArme> Cases;
	TArray<FCibleRoueArmes> Frais;
	R->ListerArmes(Cases, Frais, Images);
	const bool bEnMain = Cases.IsValidIndex(Index) && Cases[Index].bEnMain;
	if (R->LacherArme(Cibles[Index], bEnMain))
	{
		Message(TEXT("Arme lâchée"));
	}
	RafraichirBientot();
}

void AMenuArmements::BasculerCoffre(int32 Index)
{
	ARoueArmes* R = Roue();
	if (!R || !Widget.IsValid())
	{
		return;
	}
	TArray<FCaseArme> Cases;
	R->ListerArmes(Cases, Cibles, Images);
	if (!Cases.IsValidIndex(Index))
	{
		return;
	}
	const FString Nom = Cases[Index].Nom.ToString();
	if (LeCoffre.Remove(Nom) > 0)
	{
		Message(FString::Printf(TEXT("%s retirée du coffre"), *Nom));
	}
	else
	{
		LeCoffre.Add(Nom);
		Message(FString::Printf(TEXT("%s ajoutée au coffre"), *Nom));
	}
	EcrireCoffre();
	Widget->MettreAJour(Cases, LeCoffre);
}

void AMenuArmements::Onglet(int32 Index)
{
	if (Index == 1)
	{
		// PERSONNAGE : ouvre le menu HABILLAGE du plugin ChellesMenu (commande console, pas de lien direct entre plugins)
		if (IConsoleManager::Get().FindConsoleObject(TEXT("Chelles.Habillage")))
		{
			UWorld* Monde = GetWorld();
			Fermer();
			if (GEngine && Monde)
			{
				GEngine->Exec(Monde, TEXT("Chelles.Habillage"));
			}
			return;
		}
		Message(TEXT("Menu PERSONNAGE : pas encore disponible (recompile ChellesMenu)"));
	}
}

bool AMenuArmements::FermerInventaireFPS()
{
	// Le vieux menu du kit (W_WeaponInventory_UI / _Skin_B) s'ouvre aussi sur I : on le retire et on remet son etat a "ferme".
	bool bRetire = false;
	UWorld* Monde = GetWorld();
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (!W || W->GetWorld() != Monde || !W->IsInViewport())
		{
			continue;
		}
		if (W->GetClass()->GetName().Contains(TEXT("WeaponInventory")))
		{
			W->RemoveFromParent();
			bRetire = true;
		}
	}
	if (bRetire)
	{
		APlayerController* PC = GetPC();
		if (APawn* Pion = PC ? PC->GetPawn() : nullptr)
		{
			for (TFieldIterator<FBoolProperty> It(Pion->GetClass()); It; ++It)
			{
				const FString Nom = It->GetName();
				if (Nom.Contains(TEXT("Inventory")) && Nom.Contains(TEXT("Open")))
				{
					It->SetPropertyValue_InContainer(Pion, false);
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[MenuArmements] Ancien menu d'inventaire du kit ferme (remplace par ARMEMENTS)."));
	}
	return bRetire;
}

void AMenuArmements::LireCoffre()
{
	LeCoffre.Reset();
	FString Texte;
	if (GConfig && GConfig->GetString(TEXT("ChellesArmements"), TEXT("Coffre"), Texte, GGameUserSettingsIni))
	{
		TArray<FString> Noms;
		Texte.ParseIntoArray(Noms, TEXT("|"), true);
		for (const FString& N : Noms)
		{
			LeCoffre.Add(N);
		}
	}
}

void AMenuArmements::EcrireCoffre()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetString(TEXT("ChellesArmements"), TEXT("Coffre"), *FString::Join(LeCoffre.Array(), TEXT("|")), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AMenuArmements::Commande(const TArray<FString>& Arguments)
{
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Ouvrir"));
	if (Ordre.Equals(TEXT("Fermer"), ESearchCase::IgnoreCase))
	{
		Fermer();
	}
	else if (Ordre.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		Fermer();
		ChargerReglages();
		bLieClavier = false;
		Message(FString::Printf(TEXT("Menu ARMEMENTS : touche %s, %s"), *Touche.ToString(), bActif ? TEXT("actif") : TEXT("coupe")));
	}
	else if (Ordre == TEXT("0") || Ordre == TEXT("1"))
	{
		bActif = (Ordre == TEXT("1"));
		if (!bActif)
		{
			Fermer();
		}
		Message(bActif ? TEXT("Menu ARMEMENTS : actif") : TEXT("Menu ARMEMENTS : coupe"));
	}
	else
	{
		Ouvrir();
	}
}
