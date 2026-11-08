// CHELLES 3D - Menu de demarrage : logique (pause, Solo, Quitter, multijoueur LAN)
#include "MenuChelles.h"

#include "SMenuChelles.h"

#include "Components/AudioComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IPAddress.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OnlineSubsystemUtils.h"
#include "Sound/SoundBase.h"
#include "SocketSubsystem.h"
#include "TimerManager.h"
#include "UObject/Package.h"

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
	MusiquesJeu.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ChellesMenu/Audio/Musique_Jeu.Musique_Jeu"))));
	MusiquesJeu.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ChellesMenu/Audio/Musique_Jeu2.Musique_Jeu2"))));
	MusiquesJeu.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ChellesMenu/Audio/Musique_Jeu3.Musique_Jeu3"))));
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

	const ENetMode Mode = Monde->GetNetMode();
	if (Mode == NM_DedicatedServer)
	{
		return;
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
	RetirerDelegues();
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
		Ouvrir();
	}
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

	bOuvert = true;
	ModeInterface();
	Joueur->SetPause(true);

	if (USoundBase* Son = Charger(MusiqueMenu, TEXT("/Game/Sounds/Music/Vehicle_Race_Cue.Vehicle_Race_Cue")))
	{
		Musique = UGameplayStatics::CreateSound2D(this, Son, ChellesOptions::VolumeMusique(), 1.f, 0.f, nullptr, false, true);
		if (Musique)
		{
			Musique->bIsUISound = true; // continue pendant la pause
			Musique->FadeIn(1.5f, ChellesOptions::VolumeMusique());
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
	if (Musique)
	{
		Musique->FadeOut(1.f, 0.f);
		Musique = nullptr;
	}

	const bool bEtaitOuvert = bOuvert;
	bOuvert = false;
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
}

void AMenuChelles::Tick(float Delta)
{
	Super::Tick(Delta);
	// Les sons du pack ne bouclent pas tous : on relance la musique de la partie quand elle se termine.
	// Quand une piste est finie, on passe a la suivante de la playlist.
	if (MusiquePartie && !MusiquePartie->IsPlaying() && !ChellesOptions::MusiqueCoupee())
	{
		MusiqueSuivante();
	}
}

USoundBase* AMenuChelles::PisteDeLaPlaylist(int32& Index)
{
	const int32 Nombre = MusiquesJeu.Num();
	for (int32 Essai = 0; Essai < Nombre; ++Essai)
	{
		const int32 i = ((Index + Essai) % Nombre + Nombre) % Nombre;
		if (USoundBase* Son = MusiquesJeu[i].LoadSynchronous())
		{
			Index = i;
			return Son;
		}
	}
	return LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Starter_Music_Cue.Starter_Music_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AMenuChelles::MusiqueSuivante()
{
	if (!MusiquePartie)
	{
		return;
	}
	++Piste;
	if (USoundBase* Son = PisteDeLaPlaylist(Piste))
	{
		MusiquePartie->Stop();
		MusiquePartie->SetSound(Son);
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
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AMenuChelles::MusiquePlus);
		InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AMenuChelles::MusiqueMoins);
		InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AMenuChelles::MusiqueCouper);
		InputComponent->BindKey(EKeys::End, IE_Pressed, this, &AMenuChelles::MusiqueSuivante);
	}
	if (MusiquePartie)
	{
		return;
	}
	if (USoundBase* Son = PisteDeLaPlaylist(Piste))
	{
		MusiquePartie = UGameplayStatics::CreateSound2D(this, Son, 1.f, 1.f, 0.f, nullptr, false, false);
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

USoundBase* AMenuChelles::Charger(TSoftObjectPtr<USoundBase>& Son, const TCHAR* Secours)
{
	if (USoundBase* Trouve = Son.LoadSynchronous())
	{
		return Trouve;
	}
	return LoadObject<USoundBase>(nullptr, Secours, nullptr, LOAD_NoWarn | LOAD_Quiet);
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

void AMenuChelles::MusiqueCouper()
{
	const bool bCoupee = !ChellesOptions::MusiqueCoupee();
	ChellesOptions::DefinirMusiqueCoupee(bCoupee);
	if (MusiquePartie)
	{
		if (bCoupee)
		{
			MusiquePartie->FadeOut(0.5f, 0.f);
		}
		else
		{
			MusiquePartie->FadeIn(0.5f, FMath::Max(0.001f, ChellesOptions::VolumeMusique()));
		}
	}
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
	USoundBase* Son = Choix->LoadSynchronous();
	if (!Son)
	{
		return;
	}
	const float Volume = ChellesOptions::VolumeEffets() * (Quel == ESonMenu::Survol ? 0.5f : 1.f);
	if (UAudioComponent* Lecteur = UGameplayStatics::CreateSound2D(this, Son, Volume, 1.f, 0.f, nullptr, false, true))
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
	if (Musique && bOuvert && !bQuitteEnCours && !Musique->IsPlaying())
	{
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
