// CHELLES 3D - Menu de demarrage (JOUER / OPTIONS / QUITTER, Solo, Multi-Player LAN)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MenuChelles.generated.h"

class SMenuChelles;
class SChatChelles;
class APlayerController;
class UAudioComponent;
class USoundBase;

/** Sons du menu. */
enum class ESonMenu : uint8
{
	Survol,
	Clic,
	Reglage,
	Retour,
	Sortie
};

/** Une partie trouvee sur le reseau local. */
struct FPartieTrouvee
{
	int32 Index = INDEX_NONE; // position dans la recherche
	FString Hote;
	int32 Joueurs = 0;
	int32 Places = 0;
	int32 Ping = 0;
	bool bPvp = true;
};

/**
 * Pose automatiquement au lancement du jeu (voir ChellesMenuModule).
 * Affiche le menu une seule fois par session de jeu, met le jeu en pause derriere,
 * et gere le multijoueur sur le reseau local (OnlineSubsystem Null).
 */
UCLASS()
class CHELLESMENU_API AMenuChelles : public AActor
{
	GENERATED_BODY()

public:
	AMenuChelles();

	/** Decocher (sur un menu pose dans le niveau) pour ne pas afficher le menu au demarrage. */
	UPROPERTY(EditAnywhere, Category = "Chelles Menu")
	bool bAfficherAuDemarrage = true;

	/** Nombre de joueurs maximum d'une partie LAN. */
	UPROPERTY(EditAnywhere, Category = "Chelles Menu")
	int32 JoueursMax = 8;

	/** Sons du menu : pris dans le pack Vehicle Game deja present dans le projet (modifiables). */
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TSoftObjectPtr<USoundBase> MusiqueMenu = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/ChellesMenu/Audio/Musique_Menu.Musique_Menu")));
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TSoftObjectPtr<USoundBase> SonSurvol = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuCursor_UpDown_Stereo_Cue.MenuCursor_UpDown_Stereo_Cue")));
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TSoftObjectPtr<USoundBase> SonClic = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuAccept_Stereo_Cue.MenuAccept_Stereo_Cue")));
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TSoftObjectPtr<USoundBase> SonReglage = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuCursor_Sides_Stereo_Cue.MenuCursor_Sides_Stereo_Cue")));
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TSoftObjectPtr<USoundBase> SonRetour = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuBack_Stereo_Cue.MenuBack_Stereo_Cue")));
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TSoftObjectPtr<USoundBase> SonSortie = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuExitStereo_Cue.MenuExitStereo_Cue")));

	/** Playlist de la partie (apres SOLO ou en multi), jouee a la suite. Page Haut / Page Bas = volume, N = couper, Fin = piste suivante. */
	UPROPERTY(EditAnywhere, Category = "Chelles Menu|Sons")
	TArray<TSoftObjectPtr<USoundBase>> MusiquesJeu;

	/** La seule carte du jeu, en solo comme en multijoueur. */
	UPROPERTY(EditAnywhere, Category = "Chelles Menu")
	FString CarteDuJeu = TEXT("/Game/Maps/Rachid");

	void JouerSon(ESonMenu Quel);
	void MajVolumeMusique();
	/** Relance la musique si elle s'est terminee (appele par le menu). */
	void EntretenirMusique();

	// Appeles par les boutons du menu
	void Solo();
	void Quitter();
	void HebergerLAN();
	void Rechercher();
	void RejoindrePartie(int32 IndexDansListe);
	void RejoindreIP(const FString& Adresse);

	bool RechercheEnCours() const { return bRechercheEnCours; }
	bool RechercheFaite() const { return bRechercheFaite; }
	const TArray<FPartieTrouvee>& Parties() const { return PartiesTrouvees; }
	FText Etat() const { return MessageEtat; }
	FString AdresseLocale() const;
	APlayerController* GetPC() const;
	/** Donne la souris et le clavier au menu (et plus au jeu). */
	void ModeInterface();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float Delta) override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	void Attendre();
	void Ouvrir();
	void Fermer(bool bReprendreLeJeu);
	IOnlineSessionPtr Sessions() const;
	FString CheminCarte() const;
	void ApresNettoyage(TFunction<void()> Suite);
	void Creer();
	void Joindre();
	void Connecter(const FString& Adresse);
	void FinDestruction(FName Nom, bool bOk);
	void FinCreation(FName Nom, bool bOk);
	void FinRecherche(bool bOk);
	void FinJonction(FName Nom, EOnJoinSessionCompleteResult::Type Resultat);
	void Message(const FString& Texte);
	void Rafraichir();
	void RetirerDelegues();
	void LancerMusiqueJeu();
	/** Charge le son choisi, sinon un son de secours deja present dans le projet. */
	static USoundBase* Charger(TSoftObjectPtr<USoundBase>& Son, const TCHAR* Secours);
	void MusiquePlus();
	void MusiqueMoins();
	void MusiqueCouper();
	void MusiqueSuivante();
	/** Charge la piste Index de la playlist (saute celles qui manquent). */
	USoundBase* PisteDeLaPlaylist(int32& Index);
	void AnnoncerMusique();
	int32 Piste = 0;

	UPROPERTY(Transient)
	UAudioComponent* Musique = nullptr;

	UPROPERTY(Transient)
	UAudioComponent* MusiquePartie = nullptr;

	double DernierSurvol = 0.0;
	bool bQuitteEnCours = false;

	TSharedPtr<SMenuChelles> Widget;
	TSharedPtr<SChatChelles> PanneauChat;
	void CreerChat();
	void OuvrirChat();
	void RetirerChat();
	TSharedPtr<FOnlineSessionSearch> Recherche;
	FOnlineSessionSearchResult PartieChoisie;
	TArray<FPartieTrouvee> PartiesTrouvees;
	FText MessageEtat;
	bool bRechercheEnCours = false;
	bool bRechercheFaite = false;
	bool bOuvert = false;
	bool bSeulementRemettreLeJeu = false;
	bool bOuvrirLeMenu = false;
	FTimerHandle Minuterie;
	TFunction<void()> SuiteApresDestruction;
	FDelegateHandle HCreation;
	FDelegateHandle HRecherche;
	FDelegateHandle HJonction;
	FDelegateHandle HDestruction;
};
