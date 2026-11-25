// CHELLES 3D - PvP : un relais par joueur (coups recus, a terre, kits de survie, scores de la manche)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PvPRelais.generated.h"

class APawn;
class APlayerController;
class AController;
class APvPChelles;

UENUM()
enum class EEtatJoueurPvP : uint8
{
	Debout,
	ATerre,
	Mort
};

/**
 * Cree par le serveur pour chaque joueur (proprietaire = son PlayerController).
 * Tout le monde voit l'etat de tout le monde (qui est a terre, ou, tableau des scores).
 * Le joueur ne fait que DEMANDER (jeter un kit) : le serveur verifie et decide.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESPVP_API APvPRelais : public AActor
{
	GENERATED_BODY()

public:
	APvPRelais();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Demandes du joueur ----
	UFUNCTION(Server, Reliable)
	void ServeurJeterKit();

	// ---- Reponses du serveur ----
	UFUNCTION(Client, Reliable)
	void ClientAvis(const FString& Texte);

	// ---- Etat connu de tous ----
	UPROPERTY(Replicated)
	FString Nom;

	UPROPERTY(Replicated)
	APawn* Pion = nullptr;

	UPROPERTY(Replicated)
	EEtatJoueurPvP Etat = EEtatJoueurPvP::Debout;

	/** Coups recus d'autres joueurs depuis la derniere fois debout. */
	UPROPERTY(Replicated)
	int32 Coups = 0;

	UPROPERTY(Replicated)
	int32 Kits = 0;

	/** Heure (horloge du serveur) ou le joueur a terre meurt si personne ne lui jette de kit. */
	UPROPERTY(Replicated)
	float FinATerre = 0.f;

	// Scores de la manche
	UPROPERTY(Replicated)
	int32 MisesATerre = 0;

	UPROPERTY(Replicated)
	int32 Eliminations = 0;

	UPROPERTY(Replicated)
	int32 Reanimations = 0;

	UPROPERTY(Replicated)
	int32 FoisATerre = 0;

	// ---- Serveur seulement ----
	/** Le dernier joueur qui m'a touche (pour lui compter la mise a terre / l'elimination). */
	TWeakObjectPtr<AController> DernierAttaquant;
	float HeureDernierCoup = -1000.f;
	/** Coup en cours d'un autre joueur : la vie d'avant, remise apres le calcul de FPS_Game. */
	bool bCoupEnCours = false;
	float VieAvantCoup = 0.f;
	/** Le coup de grace (fin du delai a terre) : laisse passer jusqu'a FPS_Game. */
	bool bCoupDeGrace = false;
	float HeureCoupDeGrace = 0.f;
	/** Pion sur lequel on ecoute les degats. */
	TWeakObjectPtr<APawn> PionEcoute;
	float ProchainKitPermis = 0.f;

	APlayerController* GetPC() const;
	/** Heure commune a toutes les machines (horloge du serveur). */
	float Maintenant() const;

private:
	APvPChelles* Gestionnaire() const;
};
