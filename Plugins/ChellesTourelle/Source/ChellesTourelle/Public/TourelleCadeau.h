// CHELLES 3D - La tourelle offerte aux joueurs restes longtemps dans le jeu
//
//  - Chaque joueur compte son temps de jeu (sur son PC, comme l'XP : Saved\SaveGames\ChellesTourelle.sav).
//    Le temps deja joue avant ce plugin est repris de l'XP (ChellesXP.sav).
//  - Apres 50 minutes (Tourelle.txt : MinutesAvantCadeau), un message annonce le cadeau et la tourelle
//    entre dans son inventaire pour toujours.
//  - Quand le menu d'inventaire des armes est ouvert (touche I), une case TOURELLE s'affiche a cote :
//    POSER = la tourelle est posee 2,5 m devant le joueur ; RAMASSER = elle revient dans l'inventaire.
//    Avant 50 minutes, la case montre le temps qui reste.
//  - Multijoueur : le serveur pose la tourelle, tout le monde la voit tirer.
//  Console (touche ²) : Chelles.Tourelle Info | Poser | Ramasser | Temps 50 | Relire
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "TourelleCadeau.generated.h"

class APlayerController;
class ATourelleChelles;
class FFiltreTourelle;
class SWidget;
class UGameViewportClient;

/** Sauvegarde du joueur (sur son PC). */
UCLASS()
class CHELLESTOURELLE_API UTourelleSauvegarde : public USaveGame
{
	GENERATED_BODY()

public:
	/** Temps de jeu total en secondes. */
	UPROPERTY()
	double SecondesJouees = 0.0;

	/** Vrai = la tourelle a ete offerte (elle reste dans l'inventaire pour toujours). */
	UPROPERTY()
	bool bOfferte = false;

	/** Vrai = le temps de jeu de l'XP (ChellesXP.sav) a deja ete repris. */
	UPROPERTY()
	bool bTempsXPRepris = false;
};

/** Un par joueur : son temps de jeu, son cadeau, sa case dans l'inventaire et ses tourelles posees. */
UCLASS(NotPlaceable, Transient)
class CHELLESTOURELLE_API ATourelleJoueur : public AActor
{
	GENERATED_BODY()

public:
	ATourelleJoueur();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Tourelles posees par ce joueur (le serveur les tient a jour). */
	UPROPERTY(Replicated)
	TArray<ATourelleChelles*> MesTourelles;

	/** Demande du joueur : poser une tourelle a cet endroit. */
	UFUNCTION(Server, Reliable)
	void ServeurPoser(FVector Lieu, float Cap);

	/** Demande du joueur : ramasser ses tourelles. */
	UFUNCTION(Server, Reliable)
	void ServeurRamasser();

	// ---- Machine du joueur
	bool EstLocal() const;
	bool APossedeLaTourelle() const { return bOfferte; }
	int32 NombrePosees() const;
	double SecondesComptees() const;
	/** Pose une tourelle devant le joueur (rend faux + raison si impossible). */
	bool Poser(FString& Raison);
	void Ramasser();
	/** Pour tester : regle le temps de jeu (en minutes). */
	void ReglerTemps(float Minutes);
	FString Resume() const;

	/** Appele par le filtre de la souris : clic sur un bouton de la case ? (vrai = clic garde) */
	bool Clic(const FVector2D& PositionEcran);

	// Lus par la case a l'ecran
	FText TexteEtat() const;
	float Progression() const;
	bool SurvolPoser() const;
	bool SurvolRamasser() const;
	bool bCaseVisible = false;
	bool bMessageVisible = false;
	TSharedPtr<SWidget> BoutonPoser;
	TSharedPtr<SWidget> BoutonRamasser;
	FText TexteMessage;

	/** Serveur : retire les tourelles de ce joueur (il quitte la partie). */
	void DetruireTourelles();

private:
	void InitialiserLocal(APlayerController* PC);
	void TickLocal(float DeltaSeconds, APlayerController* PC);
	void Sauvegarder();
	void Offrir();
	bool MenuInventaireOuvert() const;
	void AfficherMessage(const FText& Texte, float Duree);
	void JouerSon2D(int32 Genre, float Volume);
	static bool SurBouton(const TSharedPtr<SWidget>& Bouton, const FVector2D& Position);

	bool bLocalPret = false;
	bool bOfferte = false;
	double SecondesSauvegardees = 0.0;
	double SecondesPartie = 0.0;
	float ProchaineSauvegarde = 60.f;
	float ProchainControleMenu = 0.f;
	float FinMessage = 0.f;
	FString NomSauvegarde;
	/** 1 = poser, 2 = ramasser, a faire a l'image suivante (clic vu par le filtre). */
	int32 ActionDemandee = 0;

	TSharedPtr<SWidget> Ecran;
	TWeakObjectPtr<UGameViewportClient> Vue;
	TSharedPtr<FFiltreTourelle> Filtre;
};

/** Pose un ATourelleJoueur pour chaque joueur (seulement sur le serveur ou en solo). */
UCLASS(NotPlaceable, Transient)
class CHELLESTOURELLE_API ATourellesChelles : public AActor
{
	GENERATED_BODY()

public:
	ATourellesChelles();
	virtual void Tick(float DeltaSeconds) override;

private:
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<ATourelleJoueur>> Joueurs;
};
