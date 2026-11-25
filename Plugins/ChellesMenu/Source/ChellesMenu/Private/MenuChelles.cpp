// CHELLES 3D - Menu de demarrage : logique (pause, Solo, Quitter, multijoueur LAN)
#include "MenuChelles.h"

#include "SMenuChelles.h"
#include "SChatChelles.h"
#include "ChatChelles.h"
#include "HabillageChelles.h"

#include "Components/AudioComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnlineSubsystemUtils.h"
#include "Sound/SoundBase.h"
#include "SocketSubsystem.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Blueprint/UserWidget.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	/** Le menu ne s'affiche qu'une fois par instance de jeu (une partie Play = une instance). */
	TWeakObjectPtr<UGameInstance> GInstanceDejaVue;
	/** Apres un voyage reseau (heberger / rejoindre), il faut rendre les commandes au joueur. */
	bool GRemettreLeJeu = false;
	/** Message a afficher si une connexion a echoue. */
	FString GMessageEchec;
	bool GEchecsBranches = false;

	const FName CleJeu(TEXT("JEU"));
	const FName ClePvp(TEXT("PVP"));
	const FString ValeurJeu(TEXT("CHELLES3D"));

	void SurEchecReseau(UWorld*, UNetDriver*, ENetworkFailure::Type, const FString& Erreur)
	{
		GInstanceDejaVue.Reset();
		GRemettreLeJeu = false;
		GMessageEchec = FString(TEXT("Connexion perdue ou impossible : ")) + Erreur;
	}

	void SurEchecVoyage(UWorld*, ETravelFailure::Type, const FString& Erreur)
	{
		GInstanceDejaVue.Reset();
		GRemettreLeJeu = false;
		GMessageEchec = FString(TEXT("Impossible de rejoindre : ")) + Erreur;
	}
}

AMenuChelles::AMenuChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f; // seulement pour relancer la musique en boucle
	SetCanBeDamaged(false);
	// Bibliotheque de sons de MERIDJA (importee par Tools/Menu/importer_musiques.py)
	// Junkala d'abord : c'est la musique qui accompagne l'ouverture du menu JOUER / OPTIONS / QUITTER
	const TCHAR* Menu[] = { TEXT("Menu_4_Junkala"), TEXT("Menu_1_RetroGame1"), TEXT("Menu_2_RetroGame2"), TEXT("Menu_3_RetroGame3") };
	for (const TCHAR* Nom : Menu)
	{
		MusiquesMenu.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/ChellesMenu/Audio/%s.%s"), Nom, Nom))));
	}
	// Jeu_0 = courte intro jouee une fois au lancement, puis les morceaux s'enchainent
	const TCHAR* Jeu[] = { TEXT("Jeu_0_Juhani"), TEXT("Jeu_1_AiFight"), TEXT("Jeu_2_ElectricStream"), TEXT("Jeu_3_Factory"),
		TEXT("Jeu_4_JumpingCyborg"), TEXT("Jeu_5_Overload"), TEXT("Jeu_6_SpaceRay"), TEXT("Jeu_7_Spaceship") };
	for (const TCHAR* Nom : Jeu)
	{
		MusiquesJeu.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/ChellesMenu/Audio/%s.%s"), Nom, Nom))));
	}
}

APlayerController* AMenuChelles::GetPC() const
{
	UWorld* Monde = GetWorld();
	return Monde ? Monde->GetFirstPlayerController() : nullptr;
}

void AMenuChelles::BeginPlay()
{
	Super::BeginPlay();

	UWorld* Monde = GetWorld();
	if (!Monde || !Monde->IsGameWorld())
	{
		return;
	}
	if (!GEchecsBranches && GEngine)
	{
		GEngine->OnNetworkFailure().AddStatic(&SurEchecReseau);
		GEngine->OnTravelFailure().AddStatic(&SurEchecVoyage);
		GEchecsBranches = true;
	}

	ChellesOptions::AppliquerAuDemarrage(Monde);
	// Test automatique du soir (Tools\Testeur, plugin ChellesTesteur) : pas de menu, on part directement en SOLO.
	if (FParse::Param(FCommandLine::Get(), TEXT("ChellesTest")))
	{
		bSoloAuto = true;
	}

	const ENetMode Mode = Monde->GetNetMode();
	if (Mode == NM_DedicatedServer)
	{
		return;
	}
	// Echap en jeu : on remplace l'ancien menu gris et blanc W_Pause (FPS_Game) par ce menu.
	// Minuterie du moteur (et pas Tick) : elle tourne aussi quand le jeu est en pause.
	if (!HSurveillancePause.IsValid())
	{
		HSurveillancePause = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &AMenuChelles::SurveillerPause), 0.05f);
	}
	UGameInstance* Instance = GetGameInstance();
	const bool bDejaVu = Instance && GInstanceDejaVue.Get() == Instance;

	if (Mode == NM_Client || bDejaVu || !bAfficherAuDemarrage)
	{
		bSeulementRemettreLeJeu = GRemettreLeJeu;
	}
	else
	{
		GInstanceDejaVue = Instance;
		bOuvrirLeMenu = true;
	}
	// On attend que le joueur existe (reglages souris, puis menu)
	Monde->GetTimerManager().SetTimer(Minuterie, this, &AMenuChelles::Attendre, 0.2f, true);
}

void AMenuChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (UWorld* Monde = GetWorld())
	{
		Monde->GetTimerManager().ClearTimer(Minuterie);
	}
	if (HSurveillancePause.IsValid())
	{
		FTicker::GetCoreTicker().RemoveTicker(HSurveillancePause);
		HSurveillancePause.Reset();
	}
	RetirerDelegues();
	RetirerChat();
	Fermer(false);
	if (MusiquePartie)
	{
		MusiquePartie->Stop();
		MusiquePartie = nullptr;
	}
	Super::EndPlay(Raison);
}

void AMenuChelles::Attendre()
{
	UWorld* Monde = GetWorld();
	APlayerController* Joueur = GetPC();
	if (!Monde || !Joueur || !Joueur->IsLocalController() || !Monde->GetGameViewport())
	{
		return; // on reessaie au prochain passage
	}
	Monde->GetTimerManager().ClearTimer(Minuterie);
	ChellesOptions::AppliquerAuJoueur(Joueur);

	if (bSeulementRemettreLeJeu)
	{
		GRemettreLeJeu = false;
		bSeulementRemettreLeJeu = false;
		Joueur->SetInputMode(FInputModeGameOnly());
		Joueur->bShowMouseCursor = false;
		LancerMusiqueJeu();
		return;
	}
	if (bOuvrirLeMenu)
	{
		bOuvrirLeMenu = false;
		if (bSoloAuto)
		{
			UE_LOG(LogTemp, Display, TEXT("[Menu] Test automatique : depart direct en SOLO."));
			Solo();
			return;
		}
		Ouvrir();
	}
}

void AMenuChelles::SoloDesQuePret()
{
	if (bOuvert)
	{
		Solo();
		return;
	}
	bSoloAuto = true;
}

void AMenuChelles::Ouvrir()
{
	UWorld* Monde = GetWorld();
	APlayerController* Joueur = GetPC();
	UGameViewportClient* Vue = Monde ? Monde->GetGameViewport() : nullptr;
	if (bOuvert || !Joueur || !Vue)
	{
		return;
	}

	Widget = SNew(SMenuChelles).Menu(this);
	Vue->AddViewportWidgetContent(Widget.ToSharedRef(), 1000);
	// L'ecran HABILLAGE (bouton JOUER) a besoin du catalogue : on le charge pendant que le joueur lit le menu.
	ChellesHabillage::PrechargerCatalogue();

	bOuvert = true;
	// Cache les messages techniques en haut a gauche (texture pool, passants...) pendant le menu
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessagesDisplay = false;
	}
	ModeInterface();
	// Menu PAUSE en multijoueur : on ne met pas la partie des autres joueurs en pause.
	if (!bEnPause || Monde->GetNetMode() == NM_Standalone)
	{
		Joueur->SetPause(true);
	}

	if (USoundBase* SonCharge = PisteDe(MusiquesMenu, PisteMenu, TEXT("/Game/Sounds/Music/Vehicle_Race_Cue.Vehicle_Race_Cue")))
	{
		Musique = UGameplayStatics::CreateSound2D(this, SonCharge, ChellesOptions::VolumeMusique(), 1.f, 0.f, nullptr, false, false);
		if (Musique)
		{
			Musique->bIsUISound = true; // continue pendant la pause
			if (!ChellesOptions::MusiqueCoupee())
			{
				Musique->FadeIn(1.5f, ChellesOptions::VolumeMusique());
			}
		}
	}

	if (!GMessageEchec.IsEmpty())
	{
		Message(GMessageEchec);
		GMessageEchec.Reset();
		Widget->Afficher(EEcranMenu::Multi);
	}
}

void AMenuChelles::ModeInterface()
{
	APlayerController* Joueur = GetPC();
	if (!bOuvert || !Joueur || !Widget.IsValid())
	{
		return;
	}
	Joueur->bShowMouseCursor = true;
	FInputModeUIOnly ModeUI;
	ModeUI.SetWidgetToFocus(Widget);
	ModeUI.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Joueur->SetInputMode(ModeUI);
}

void AMenuChelles::Fermer(bool bReprendreLeJeu)
{
	UWorld* Monde = GetWorld();
	if (Widget.IsValid() && Monde)
	{
		if (UGameViewportClient* Vue = Monde->GetGameViewport())
		{
			Vue->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	Widget.Reset();
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessagesDisplay = true;
	}
	if (Musique)
	{
		Musique->bAutoDestroy = true;
		Musique->FadeOut(1.f, 0.f);
		Musique = nullptr;
	}

	const bool bEtaitOuvert = bOuvert;
	bOuvert = false;
	bEnPause = false;
	if (!bReprendreLeJeu || !bEtaitOuvert)
	{
		return;
	}
	if (APlayerController* Joueur = GetPC())
	{
		Joueur->SetPause(false);
		Joueur->SetInputMode(FInputModeGameOnly());
		Joueur->bShowMouseCursor = false;
	}
}

void AMenuChelles::Solo()
{
	Fermer(true);
	LancerMusiqueJeu();
	DebutSolo = FPlatformTime::Seconds();
	LancerModeLobby();
}

void AMenuChelles::LancerModeLobby()
{
	// Le lobby a ecrit le mode choisi ; c'est ChellesRaccourcis qui pilote la roue des modes (console, pas de lien entre plugins)
	UWorld* Monde = GetWorld();
	FString Mode;
	if (GConfig)
	{
		GConfig->GetString(TEXT("ChellesLobby"), TEXT("ModeDepart"), Mode, GGameUserSettingsIni);
	}
	Mode.TrimStartAndEndInline();
	if (Mode.IsEmpty() || !Monde || !GEngine || !IConsoleManager::Get().FindConsoleObject(TEXT("Chelles.Raccourcis")))
	{
		return;
	}
	GEngine->Exec(Monde, *FString::Printf(TEXT("Chelles.Raccourcis Mode %s"), *Mode));
}

void AMenuChelles::SandBox()
{
	Fermer(true);
	LancerMusiqueJeu();
	// Pas de DebutSolo : le joueur est deplace dans la zone SANDBOX, ce n'est pas un depart a memoriser
	LancerModeLobby();
	if (UWorld* Monde = GetWorld())
	{
		if (GEngine)
		{
			GEngine->Exec(Monde, TEXT("Chelles.SandBox 1"));
		}
	}
}

void AMenuChelles::MemoriserDepart()
{
	if (bDepartMemorise || DebutSolo < 0.0 || FPlatformTime::Seconds() - DebutSolo < 4.0)
	{
		return;
	}
	APlayerController* Joueur = GetPC();
	ACharacter* Perso = Joueur ? Cast<ACharacter>(Joueur->GetPawn()) : nullptr;
	if (Perso && Perso->GetCharacterMovement() && Perso->GetCharacterMovement()->IsMovingOnGround() && GConfig)
	{
		GConfig->SetString(TEXT("ChellesMenu"), TEXT("Depart"), *Perso->GetActorLocation().ToString(), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
		bDepartMemorise = true;
	}
}

void AMenuChelles::Tick(float Delta)
{
	Super::Tick(Delta);
	// Serveur : chaque joueur connecte recoit son composant de chat
	UChatChelles::EquiperJoueurs(GetWorld());
	MemoriserDepart();
	// Les sons du pack ne bouclent pas tous : on relance la musique de la partie quand elle se termine.
	// Quand une piste est finie, on passe a la suivante de la playlist.
	if (MusiquePartie && !MusiquePartie->IsPlaying() && !ChellesOptions::MusiqueCoupee())
	{
		MusiqueSuivante();
	}
}

USoundBase* AMenuChelles::PisteDe(TArray<TSoftObjectPtr<USoundBase>>& Liste, int32& Index, const TCHAR* Secours)
{
	const int32 Nombre = Liste.Num();
	for (int32 Essai = 0; Essai < Nombre; ++Essai)
	{
		const int32 i = ((Index + Essai) % Nombre + Nombre) % Nombre;
		if (USoundBase* SonCharge = Liste[i].LoadSynchronous())
		{
			Index = i;
			return SonCharge;
		}
	}
	return LoadObject<USoundBase>(nullptr, Secours, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

USoundBase* AMenuChelles::PisteDeLaPlaylist(int32& Index)
{
	return PisteDe(MusiquesJeu, Index, TEXT("/Game/Audio/Starter_Music_Cue.Starter_Music_Cue"));
}

void AMenuChelles::MusiqueSuivante()
{
	if (!MusiquePartie)
	{
		return;
	}
	++Piste;
	if (Piste >= MusiquesJeu.Num())
	{
		Piste = MusiquesJeu.Num() > 1 ? 1 : 0; // l'intro (piste 0) ne passe qu'au lancement
	}
	if (USoundBase* SonCharge = PisteDeLaPlaylist(Piste))
	{
		MusiquePartie->Stop();
		MusiquePartie->SetSound(SonCharge);
		MajVolumeMusique();
		if (!ChellesOptions::MusiqueCoupee())
		{
			MusiquePartie->FadeIn(1.f, FMath::Max(0.001f, ChellesOptions::VolumeMusique()));
		}
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(77011, 3.f, FColor(255, 0, 200), FString::Printf(TEXT("Musique : piste %d / %d  (Fin = suivante)"), Piste + 1, FMath::Max(1, MusiquesJeu.Num())));
	}
}

void AMenuChelles::LancerMusiqueJeu()
{
	APlayerController* Joueur = GetPC();
	if (!Joueur)
	{
		return;
	}
	// Touches de la musique (libres dans le projet : + et - servent deja a la roue)
	EnableInput(Joueur);
	if (InputComponent && !bTouchesJeu)
	{
		bTouchesJeu = true;
		InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AMenuChelles::MusiquePlus);
		InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AMenuChelles::MusiqueMoins);
		InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AMenuChelles::MusiqueCouper);
		InputComponent->BindKey(EKeys::End, IE_Pressed, this, &AMenuChelles::MusiqueSuivante);
		// Entree = panneau de droite (chat entre joueurs / aide Tuto & Astuces)
		InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AMenuChelles::OuvrirChat);
	}
	CreerChat();
	if (MusiquePartie)
	{
		return;
	}
	if (USoundBase* SonCharge = PisteDeLaPlaylist(Piste))
	{
		MusiquePartie = UGameplayStatics::CreateSound2D(this, SonCharge, 1.f, 1.f, 0.f, nullptr, false, false);
		if (MusiquePartie)
		{
			MusiquePartie->bIsUISound = true;
			MajVolumeMusique();
			if (!ChellesOptions::MusiqueCoupee())
			{
				MusiquePartie->FadeIn(2.f, FMath::Max(0.001f, ChellesOptions::VolumeMusique()));
			}
		}
	}
}

USoundBase* AMenuChelles::Charger(TSoftObjectPtr<USoundBase>& SonCharge, const TCHAR* Secours)
{
	if (USoundBase* Trouve = SonCharge.LoadSynchronous())
	{
		return Trouve;
	}
	return LoadObject<USoundBase>(nullptr, Secours, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AMenuChelles::CreerChat()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (Vue && !PanneauChat.IsValid())
	{
		PanneauChat = SNew(SChatChelles).Menu(this);
		Vue->AddViewportWidgetContent(PanneauChat.ToSharedRef(), 900);
	}
	// Logo son en jeu (sous la mini-carte, a droite) : N = couper / remettre ;
	// cliquable quand la souris est visible (menu pause Echap du jeu).
	if (Vue && !IconeJeu.IsValid())
	{
		TWeakObjectPtr<AMenuChelles> Moi(this);
		IconeJeu = SNew(SBox)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 205.f, 34.f, 0.f))
			.Visibility_Lambda([Moi]() { return (Moi.IsValid() && !Moi->Widget.IsValid()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(SIconeSon)
					.OnClic(FSimpleDelegate::CreateLambda([Moi]() { if (Moi.IsValid()) { Moi->MusiqueCouper(); } }))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 2.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("N")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 12))
					.ColorAndOpacity(FLinearColor(0.f, 0.94f, 1.f, 0.8f))
				]
			];
		Vue->AddViewportWidgetContent(IconeJeu.ToSharedRef(), 850);
	}
}

void AMenuChelles::OuvrirChat()
{
	if (Widget.IsValid())
	{
		return; // pas pendant le menu
	}
	CreerChat();
	if (PanneauChat.IsValid() && !PanneauChat->EstOuvert())
	{
		PanneauChat->Ouvrir();
	}
}

void AMenuChelles::RetirerChat()
{
	if (PanneauChat.IsValid())
	{
		PanneauChat->Fermer();
		UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
		if (Vue)
		{
			Vue->RemoveViewportWidgetContent(PanneauChat.ToSharedRef());
		}
		PanneauChat.Reset();
	}
	if (IconeJeu.IsValid())
	{
		if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Vue->RemoveViewportWidgetContent(IconeJeu.ToSharedRef());
		}
		IconeJeu.Reset();
	}
}

void AMenuChelles::MusiquePlus()
{
	ChellesOptions::DefinirMusiqueCoupee(false);
	ChellesOptions::DefinirVolumeCanal(0, ChellesOptions::VolumeMusique() + 0.1f);
	if (MusiquePartie && !MusiquePartie->IsPlaying())
	{
		MusiquePartie->Play();
	}
	AnnoncerMusique();
}

void AMenuChelles::MusiqueMoins()
{
	ChellesOptions::DefinirVolumeCanal(0, ChellesOptions::VolumeMusique() - 0.1f);
	AnnoncerMusique();
}

void AMenuChelles::BasculerMusique()
{
	const bool bCoupee = !ChellesOptions::MusiqueCoupee();
	ChellesOptions::DefinirMusiqueCoupee(bCoupee);
	const float Volume = FMath::Max(0.001f, ChellesOptions::VolumeMusique());
	for (UAudioComponent* Lecteur : { Musique, MusiquePartie })
	{
		if (!Lecteur)
		{
			continue;
		}
		if (bCoupee)
		{
			Lecteur->FadeOut(0.5f, 0.f);
		}
		else
		{
			Lecteur->FadeIn(0.5f, Volume);
		}
	}
}

void AMenuChelles::MusiqueCouper()
{
	BasculerMusique();
	AnnoncerMusique();
}

void AMenuChelles::AnnoncerMusique()
{
	if (GEngine)
	{
		const FString Texte = ChellesOptions::MusiqueCoupee()
			? FString(TEXT("Musique : COUP\u00C9E  (N pour la remettre)"))
			: FString::Printf(TEXT("Musique : %d %%  (Page Haut / Page Bas, N = couper, Fin = suivante)"), FMath::RoundToInt(ChellesOptions::VolumeMusique() * 100.f));
		GEngine->AddOnScreenDebugMessage(77010, 2.5f, FColor(0, 240, 255), Texte);
	}
}

void AMenuChelles::OuvrirPause()
{
	if (bOuvert)
	{
		ModeInterface();
		return;
	}
	bEnPause = true;
	Ouvrir();
	if (!bOuvert)
	{
		bEnPause = false;
	}
}

void AMenuChelles::Reprendre()
{
	Fermer(true);
}

void AMenuChelles::OuvrirHabillage()
{
	OuvrirPause();
	if (bOuvert && Widget.IsValid())
	{
		Widget->Afficher(EEcranMenu::Habillage);
	}
}

namespace
{
	/** Console : Chelles.Habillage = ouvre l'ecran HABILLAGE (lobby) en pleine partie (onglet PERSONNAGE du menu ARMEMENTS). */
	void CommandeHabillage(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AMenuChelles> It(Monde); It; ++It)
		{
			It->OuvrirHabillage();
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeHabillage(
		TEXT("Chelles.Habillage"),
		TEXT("Ouvre l'ecran HABILLAGE (personnage) du menu CHELLES 3D en pleine partie"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeHabillage));

	/** Console : Chelles.Menu [Solo|Pause|Reprendre] (utilise par le test automatique, plugin ChellesTesteur). */
	void CommandeMenu(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString();
		for (TActorIterator<AMenuChelles> It(Monde); It; ++It)
		{
			if (Ordre.Equals(TEXT("Solo"), ESearchCase::IgnoreCase))
			{
				It->SoloDesQuePret();
			}
			else if (Ordre.Equals(TEXT("Pause"), ESearchCase::IgnoreCase))
			{
				It->OuvrirPause();
			}
			else if (Ordre.Equals(TEXT("Reprendre"), ESearchCase::IgnoreCase))
			{
				It->Reprendre();
			}
			else
			{
				UE_LOG(LogTemp, Display, TEXT("[Menu] Chelles.Menu Solo | Pause | Reprendre"));
			}
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeMenu(
		TEXT("Chelles.Menu"),
		TEXT("Menu CHELLES 3D : Solo = partir en solo, Pause = ouvrir le menu PAUSE, Reprendre = fermer le menu"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeMenu));
}

bool AMenuChelles::SurveillerPause(float /*Delta*/)
{
	UWorld* Monde = GetWorld();
	if (!Monde || !Monde->GetGameViewport())
	{
		return true;
	}
	bool bTrouve = false;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Ancien = *It;
		if (!Ancien || Ancien->IsPendingKill() || Ancien->GetWorld() != Monde || !Ancien->IsInViewport())
		{
			continue;
		}
		// W_Pause (et W_Settings ouvert depuis lui) : menus gris et blancs de FPS_Game.
		// Leur bouton "Save & Quit" ouvrait la carte MainMenu, rangee dans _Quarantaine : on restait bloque.
		const FString Nom = Ancien->GetClass()->GetName();
		if (Nom.StartsWith(TEXT("W_Pause")) || Nom.StartsWith(TEXT("W_Settings")))
		{
			Ancien->RemoveFromParent();
			bTrouve = true;
		}
	}
	if (!bTrouve)
	{
		return true;
	}
	UE_LOG(LogTemp, Display, TEXT("[Menu] Echap : ancien menu W_Pause remplace par le menu CHELLES 3D."));
	// Ecran FIN DE LA DEMO affiche (plugin ChellesDemo) : il garde la main, on n'ouvre rien par-dessous.
	if (IConsoleVariable* Fin = IConsoleManager::Get().FindConsoleVariable(TEXT("Chelles.DemoFinie")))
	{
		if (Fin->GetInt() != 0)
		{
			return true;
		}
	}
	OuvrirPause();
	return true;
}

void AMenuChelles::Quitter()
{
	if (bQuitteEnCours)
	{
		return;
	}
	bQuitteEnCours = true;
	JouerSon(ESonMenu::Sortie);
	if (Musique)
	{
		Musique->FadeOut(0.7f, 0.f);
	}
	// On laisse le son de sortie se jouer avant de fermer (les minuteries du jeu sont en pause).
	TWeakObjectPtr<AMenuChelles> Moi(this);
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Moi](float)
	{
		if (Moi.IsValid())
		{
			UKismetSystemLibrary::QuitGame(Moi.Get(), Moi->GetPC(), EQuitPreference::Quit, false);
		}
		return false;
	}), 0.8f);
}

void AMenuChelles::JouerSon(ESonMenu Quel)
{
	TSoftObjectPtr<USoundBase>* Choix = &SonClic;
	switch (Quel)
	{
	case ESonMenu::Survol:  Choix = &SonSurvol; break;
	case ESonMenu::Reglage: Choix = &SonReglage; break;
	case ESonMenu::Retour:  Choix = &SonRetour; break;
	case ESonMenu::Sortie:  Choix = &SonSortie; break;
	default: break;
	}
	if (Quel == ESonMenu::Survol)
	{
		const double Maintenant = FPlatformTime::Seconds();
		if (Maintenant - DernierSurvol < 0.06)
		{
			return; // pas de crepitement quand la souris passe vite sur plusieurs boutons
		}
		DernierSurvol = Maintenant;
	}
	USoundBase* SonCharge = Choix->LoadSynchronous();
	if (!SonCharge)
	{
		return;
	}
	const float Volume = ChellesOptions::VolumeEffets() * (Quel == ESonMenu::Survol ? 0.5f : 1.f);
	if (UAudioComponent* Lecteur = UGameplayStatics::CreateSound2D(this, SonCharge, Volume, 1.f, 0.f, nullptr, false, true))
	{
		Lecteur->bIsUISound = true;
		Lecteur->Play();
	}
}

void AMenuChelles::MajVolumeMusique()
{
	const float Volume = FMath::Max(0.001f, ChellesOptions::VolumeMusique());
	if (Musique)
	{
		Musique->SetVolumeMultiplier(Volume);
	}
	if (MusiquePartie)
	{
		MusiquePartie->SetVolumeMultiplier(Volume);
	}
}

void AMenuChelles::EntretenirMusique()
{
	// Morceau du menu termine : on passe au suivant de la playlist du menu
	if (Musique && bOuvert && !bQuitteEnCours && !Musique->IsPlaying() && !ChellesOptions::MusiqueCoupee())
	{
		++PisteMenu;
		if (USoundBase* SonCharge = PisteDe(MusiquesMenu, PisteMenu, TEXT("/Game/Sounds/Music/Vehicle_Race_Cue.Vehicle_Race_Cue")))
		{
			Musique->SetSound(SonCharge);
		}
		Musique->Play();
	}
}

// ---------------------------------------------------------------- Multijoueur

IOnlineSessionPtr AMenuChelles::Sessions() const
{
	UWorld* Monde = GetWorld();
	IOnlineSubsystem* Sous = Online::GetSubsystem(Monde);
	if (!Sous)
	{
		Sous = Online::GetSubsystem(Monde, NULL_SUBSYSTEM);
	}
	return Sous ? Sous->GetSessionInterface() : nullptr;
}

FString AMenuChelles::CheminCarte() const
{
	// Une seule carte dans le projet : Rachid, pour le solo et le multijoueur.
	if (!CarteDuJeu.IsEmpty())
	{
		return CarteDuJeu;
	}
	UWorld* Monde = GetWorld();
	return Monde ? UWorld::RemovePIEPrefix(Monde->GetOutermost()->GetName()) : FString();
}

FString AMenuChelles::AdresseLocale() const
{
	ISocketSubsystem* Prises = ISocketSubsystem::Get();
	if (!Prises)
	{
		return FString();
	}
	bool bPeutTout = false;
	TSharedRef<FInternetAddr> Adresse = Prises->GetLocalHostAddr(*GLog, bPeutTout);
	return Adresse->IsValid() ? Adresse->ToString(false) : FString();
}

void AMenuChelles::Message(const FString& Texte)
{
	MessageEtat = FText::FromString(Texte);
	UE_LOG(LogTemp, Log, TEXT("[Menu] %s"), *Texte);
}

void AMenuChelles::Rafraichir()
{
	if (Widget.IsValid())
	{
		Widget->Rafraichir();
	}
}

void AMenuChelles::RetirerDelegues()
{
	IOnlineSessionPtr S = Sessions();
	if (!S.IsValid())
	{
		return;
	}
	S->ClearOnCreateSessionCompleteDelegate_Handle(HCreation);
	S->ClearOnFindSessionsCompleteDelegate_Handle(HRecherche);
	S->ClearOnJoinSessionCompleteDelegate_Handle(HJonction);
	S->ClearOnDestroySessionCompleteDelegate_Handle(HDestruction);
}

void AMenuChelles::ApresNettoyage(TFunction<void()> Suite)
{
	IOnlineSessionPtr S = Sessions();
	if (S.IsValid() && S->GetNamedSession(NAME_GameSession))
	{
		SuiteApresDestruction = MoveTemp(Suite);
		HDestruction = S->AddOnDestroySessionCompleteDelegate_Handle(
			FOnDestroySessionCompleteDelegate::CreateUObject(this, &AMenuChelles::FinDestruction));
		if (!S->DestroySession(NAME_GameSession))
		{
			S->ClearOnDestroySessionCompleteDelegate_Handle(HDestruction);
			TFunction<void()> Ensuite = MoveTemp(SuiteApresDestruction);
			if (Ensuite)
			{
				Ensuite();
			}
		}
		return;
	}
	Suite();
}

void AMenuChelles::FinDestruction(FName, bool)
{
	if (IOnlineSessionPtr S = Sessions())
	{
		S->ClearOnDestroySessionCompleteDelegate_Handle(HDestruction);
	}
	TFunction<void()> Ensuite = MoveTemp(SuiteApresDestruction);
	if (Ensuite)
	{
		Ensuite();
	}
}

void AMenuChelles::HebergerLAN()
{
	if (!Sessions().IsValid())
	{
		Message(TEXT("Multijoueur indisponible : le module en ligne d'Unreal n'est pas charg\u00E9."));
		return;
	}
	Message(TEXT("Cr\u00E9ation de ta partie sur le r\u00E9seau local..."));
	TWeakObjectPtr<AMenuChelles> Moi(this);
	ApresNettoyage([Moi]()
	{
		if (Moi.IsValid())
		{
			Moi->Creer();
		}
	});
}

void AMenuChelles::Creer()
{
	IOnlineSessionPtr S = Sessions();
	if (!S.IsValid())
	{
		return;
	}
	FOnlineSessionSettings Reglages;
	Reglages.bIsLANMatch = true;
	Reglages.NumPublicConnections = FMath::Clamp(JoueursMax, 2, 64);
	Reglages.bShouldAdvertise = true;
	Reglages.bUsesPresence = true;
	Reglages.bAllowJoinInProgress = true;
	Reglages.bAllowJoinViaPresence = true;
	Reglages.Set(SETTING_MAPNAME, CheminCarte(), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Reglages.Set(CleJeu, ValeurJeu, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Reglages.Set(ClePvp, FString(ChellesOptions::PvpActif() ? TEXT("1") : TEXT("0")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	HCreation = S->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &AMenuChelles::FinCreation));
	if (!S->CreateSession(0, NAME_GameSession, Reglages))
	{
		S->ClearOnCreateSessionCompleteDelegate_Handle(HCreation);
		Message(TEXT("Impossible de cr\u00E9er la partie."));
	}
}

void AMenuChelles::FinCreation(FName, bool bOk)
{
	if (IOnlineSessionPtr S = Sessions())
	{
		S->ClearOnCreateSessionCompleteDelegate_Handle(HCreation);
	}
	if (!bOk)
	{
		Message(TEXT("Impossible de cr\u00E9er la partie."));
		return;
	}
	const FString Carte = CheminCarte();
	Message(TEXT("Partie cr\u00E9\u00E9e ! Chargement de la carte..."));
	GRemettreLeJeu = true;
	UGameplayStatics::OpenLevel(this, FName(*Carte), true, TEXT("listen"));
}

void AMenuChelles::Rechercher()
{
	IOnlineSessionPtr S = Sessions();
	if (!S.IsValid())
	{
		Message(TEXT("Multijoueur indisponible : le module en ligne d'Unreal n'est pas charg\u00E9."));
		return;
	}
	if (bRechercheEnCours)
	{
		return;
	}
	Recherche = MakeShared<FOnlineSessionSearch>();
	Recherche->bIsLanQuery = true;
	Recherche->MaxSearchResults = 50;
	Recherche->PingBucketSize = 50;
	Recherche->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	PartiesTrouvees.Reset();
	bRechercheEnCours = true;
	Message(TEXT("Recherche d'une partie CHELLES 3D sur ton r\u00E9seau local..."));
	Rafraichir();

	HRecherche = S->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &AMenuChelles::FinRecherche));
	if (!S->FindSessions(0, Recherche.ToSharedRef()))
	{
		S->ClearOnFindSessionsCompleteDelegate_Handle(HRecherche);
		bRechercheEnCours = false;
		Message(TEXT("La recherche n'a pas pu d\u00E9marrer."));
		Rafraichir();
	}
}

void AMenuChelles::FinRecherche(bool bOk)
{
	if (IOnlineSessionPtr S = Sessions())
	{
		S->ClearOnFindSessionsCompleteDelegate_Handle(HRecherche);
	}
	bRechercheEnCours = false;
	bRechercheFaite = true;
	PartiesTrouvees.Reset();

	if (Recherche.IsValid())
	{
		for (int32 i = 0; i < Recherche->SearchResults.Num(); ++i)
		{
			const FOnlineSessionSearchResult& Resultat = Recherche->SearchResults[i];
			FString Jeu;
			Resultat.Session.SessionSettings.Get(CleJeu, Jeu);
			if (!Jeu.IsEmpty() && Jeu != ValeurJeu)
			{
				continue; // une autre application Unreal
			}
			FPartieTrouvee Partie;
			Partie.Index = i;
			Partie.Hote = Resultat.Session.OwningUserName;
			Partie.Places = Resultat.Session.SessionSettings.NumPublicConnections;
			Partie.Joueurs = FMath::Max(1, Partie.Places - Resultat.Session.NumOpenPublicConnections);
			Partie.Ping = Resultat.PingInMs;
			FString Pvp;
			Resultat.Session.SessionSettings.Get(ClePvp, Pvp);
			Partie.bPvp = (Pvp != TEXT("0"));
			PartiesTrouvees.Add(Partie);
		}
	}
	PartiesTrouvees.Sort([](const FPartieTrouvee& A, const FPartieTrouvee& B) { return A.Ping < B.Ping; });

	if (!bOk)
	{
		Message(TEXT("La recherche a \u00E9chou\u00E9."));
	}
	else if (PartiesTrouvees.Num() == 0)
	{
		Message(TEXT("Aucune partie trouv\u00E9e sur ton r\u00E9seau local."));
	}
	else
	{
		Message(FString::Printf(TEXT("%d partie(s) trouv\u00E9e(s) sur ton r\u00E9seau local."), PartiesTrouvees.Num()));
	}
	Rafraichir();
}

void AMenuChelles::RejoindrePartie(int32 IndexDansListe)
{
	if (!Recherche.IsValid() || !PartiesTrouvees.IsValidIndex(IndexDansListe))
	{
		return;
	}
	const int32 Index = PartiesTrouvees[IndexDansListe].Index;
	if (!Recherche->SearchResults.IsValidIndex(Index))
	{
		return;
	}
	PartieChoisie = Recherche->SearchResults[Index];
	Message(FString::Printf(TEXT("Connexion \u00E0 la partie de %s..."), *PartiesTrouvees[IndexDansListe].Hote));
	TWeakObjectPtr<AMenuChelles> Moi(this);
	ApresNettoyage([Moi]()
	{
		if (Moi.IsValid())
		{
			Moi->Joindre();
		}
	});
}

void AMenuChelles::Joindre()
{
	IOnlineSessionPtr S = Sessions();
	if (!S.IsValid())
	{
		return;
	}
	HJonction = S->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &AMenuChelles::FinJonction));
	if (!S->JoinSession(0, NAME_GameSession, PartieChoisie))
	{
		S->ClearOnJoinSessionCompleteDelegate_Handle(HJonction);
		Message(TEXT("Impossible de rejoindre cette partie."));
	}
}

void AMenuChelles::FinJonction(FName, EOnJoinSessionCompleteResult::Type Resultat)
{
	IOnlineSessionPtr S = Sessions();
	if (S.IsValid())
	{
		S->ClearOnJoinSessionCompleteDelegate_Handle(HJonction);
	}
	if (Resultat != EOnJoinSessionCompleteResult::Success || !S.IsValid())
	{
		Message(TEXT("Impossible de rejoindre cette partie (pleine ou ferm\u00E9e)."));
		return;
	}
	FString Adresse;
	if (!S->GetResolvedConnectString(NAME_GameSession, Adresse))
	{
		Message(TEXT("Adresse de la partie introuvable."));
		return;
	}
	Connecter(Adresse);
}

void AMenuChelles::RejoindreIP(const FString& Adresse)
{
	const FString Propre = Adresse.TrimStartAndEnd();
	if (Propre.IsEmpty())
	{
		Message(TEXT("Tape d'abord l'adresse IP du serveur (ex : 192.168.1.20)."));
		return;
	}
	Message(FString::Printf(TEXT("Connexion \u00E0 %s..."), *Propre));
	Connecter(Propre);
}

void AMenuChelles::Connecter(const FString& Adresse)
{
	APlayerController* Joueur = GetPC();
	if (!Joueur)
	{
		return;
	}
	GRemettreLeJeu = true;
	Joueur->ClientTravel(Adresse, ETravelType::TRAVEL_Absolute);
}
