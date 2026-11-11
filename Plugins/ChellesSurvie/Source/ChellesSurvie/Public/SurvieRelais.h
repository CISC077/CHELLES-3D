// CHELLES 3D - Modes de survie : un relais par joueur (points, etat debout / a terre / mort)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvieRelais.generated.h"

class APawn;
class APlayerController;
class ASurvieChelles;
class AStandRefuge;

UENUM()
enum class EEtatJoueurSurvie : uint8
{
	Debout,
	ATerre,
	Mort
};

/**
 * Cree par le serveur pour chaque joueur (proprietaire = son PlayerController), en solo comme en multi.
 * Tout le monde voit les points et l'etat de tout le monde (tableau des scores, "relever un coequipier").
 * Le joueur ne fait que DEMANDER (acheter, relever) : le serveur verifie et decide.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESSURVIE_API ASurvieRelais : public AActor
{
	GENERATED_BODY()

public:
	ASurvieRelais();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Demandes du joueur ----
	UFUNCTION(Server, Reliable)
	void ServeurAcheter(AStandRefuge* Stand);

	/** Le joueur tient (ou lache) la touche E pres d'un coequipier a terre. */
	UFUNCTION(Server, Reliable)
	void ServeurRelever(ASurvieRelais* Cible, bool bAppuie);

	/** L'hote choisit un mode dans l'ecran MODES (0 = jeu libre, 1 = Dernier Refuge). */
	UFUNCTION(Server, Reliable)
	void ServeurChoisirMode(uint8 Mode, int32 VagueDepart);

	// ---- Reponses du serveur ----
	UFUNCTION(Client, Reliable)
	void ClientAvis(const FString& Texte);

	/** XP gagnee en tuant des zombies : ajoutee a l'XP des pouvoirs sur le PC du joueur. */
	UFUNCTION(Client, Reliable)
	void ClientDonnerXP(int32 Quantite);

	/** Remplit les munitions des armes du joueur sur sa machine (les armes FPS_Game ne sont pas toutes repliquees). */
	UFUNCTION(Client, Reliable)
	void ClientRecharger();

	/** Fait apparaitre l'arme achetee sur la machine du joueur (si elle n'est pas repliquee par le reseau). */
	UFUNCTION(Client, Reliable)
	void ClientCreerArme(const FString& Chemin, FVector Lieu);

	// ---- Etat connu de tous ----
	UPROPERTY(Replicated)
	FString Nom;

	UPROPERTY(Replicated)
	APawn* Pion = nullptr;

	UPROPERTY(Replicated)
	int32 Points = 0;

	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Tetes = 0;

	UPROPERTY(Replicated)
	int32 FoisATerre = 0;

	UPROPERTY(Replicated)
	int32 Releves = 0;

	UPROPERTY(Replicated)
	EEtatJoueurSurvie Etat = EEtatJoueurSurvie::Debout;

	/** Heure (horloge du serveur) ou un joueur a terre meurt s'il n'est pas releve. */
	UPROPERTY(Replicated)
	float FinATerre = 0.f;

	/** 0 a 1 : avancement de la relance en cours (un coequipier tient E). */
	UPROPERTY(Replicated)
	float Relance = 0.f;

	/** Serveur seulement : qui est en train de me relever. */
	TWeakObjectPtr<ASurvieRelais> Releveur;

	/** Serveur seulement : arme achetee pas encore ramassee (on ne revend pas tant qu'elle traine). */
	TWeakObjectPtr<AActor> ArmeAuSol;

	APlayerController* GetPC() const;
	/** Heure commune a toutes les machines (horloge du serveur). */
	float Maintenant() const;

private:
	ASurvieChelles* Gestionnaire() const;
};
