// CHELLES 3D - Menu de demarrage (JOUER / OPTIONS / QUITTER, Solo, Multi-Player LAN)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MenuChelles.generated.h"

class SMenuChelles;
class APlayerController;

/** Une partie trouvee sur le reseau local. */
struct FPartieTrouvee
{
	int32 Index = INDEX_NONE; // position dans la recherche
	FString Hote;
	int32 Joueurs = 0;
	int32 Places = 0;
	int32 Ping = 0;
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

	TSharedPtr<SMenuChelles> Widget;
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
