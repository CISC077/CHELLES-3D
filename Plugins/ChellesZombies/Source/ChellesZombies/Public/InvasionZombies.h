// CHELLES 3D - Invasion zombies : le chef d'orchestre (ajoute tout seul au lancement du jeu)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InvasionZombies.generated.h"

class ACharacter;
class AController;
class APawn;
class UAudioComponent;
class UDamageType;
class UPrimitiveComponent;
class USireneChelles;
class SAlerteZombies;

/** Reglages lus dans Plugins\ChellesZombies\Zombies.txt (modifiable avec le Bloc-notes). */
struct FReglagesZombies
{
	bool bActif = true;
	float IntervalleMinutes = 120.f;
	float PremiereApresMinutes = 120.f;
	float DureeInvasionMinutes = 5.f;
	float DureeMessageSecondes = 30.f;
	int32 NombreZombies = 30;
	int32 EnPlusParJoueur = 10;
	int32 NombreMax = 60;
	float DegatsZombiePourcent = 20.f;
	float DegatsCorpsPourcent = 33.4f;
	float VitesseZombie = 420.f;
	float DistanceMinMetres = 30.f;
	float DistanceMaxMetres = 70.f;
	float VolumeSirene = 1.f;
	FString ClasseZombie = TEXT("/Game/FPS_Game/Blueprints/Bonus/AI/Zombie/BP_AI_CharZombie.BP_AI_CharZombie_C");
	FString PeauZombie = TEXT("/Game/DecayedZombieB/Mesh/SK_DecayedZombie_B_wPants.SK_DecayedZombie_B_wPants");
	FString Message = TEXT("ATTENTION PURGE ZOMBIE INVASION");
};

// Evenements pour les autres plugins (mode survie...). "Dynamiques" : un autre plugin s'y abonne
// par leur nom, sans avoir besoin de compiler avec celui-ci.
/** Un zombie vient d'etre tue au combat (Tueur peut etre nul). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FZombieTueChelles, ACharacter*, Zombie, AController*, Tueur, bool, bTete);
/** Un zombie vient d'etre touche sans mourir. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZombieToucheChelles, ACharacter*, Zombie, AController*, Auteur);
/** Un zombie va frapper un joueur : l'abonne peut mettre bCoupGereAilleurs a vrai pour annuler le coup. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCoupSurJoueurChelles, APawn*, Victime, float, Degats);

/** Ce que le serveur retient pour chaque zombie. */
struct FEtatZombie
{
	float Vie = 100.f;
	bool bMort = false;
	float Vitesse = 420.f;

	// Dernier coup "precis" (balle) recu : sert a savoir si c'etait la tete
	uint64 ImageCoupPrecis = 0;
	bool bCoupTete = false;
	FVector DirectionCoup = FVector::ZeroVector;

	TWeakObjectPtr<APawn> Cible;
	/** Dernier joueur qui l'a touche (pour les points du mode survie). */
	TWeakObjectPtr<AController> DernierAuteur;
	float ProchainChoixCible = 0.f;
	float ProchaineAttaque = 0.f;
	float CoupPrevuA = -1.f;
	TWeakObjectPtr<APawn> CibleDuCoup;

	FVector DernierePosition = FVector::ZeroVector;
	float ProchainControle = 0.f;
	int32 Bloque = 0;
	float ContournerJusqua = 0.f;
	float CoteContournement = 1.f;

	// Optimisation : loin des joueurs, son animation est calculee moins souvent
	bool bAllege = false;
};

UCLASS(NotPlaceable, Transient)
class CHELLESZOMBIES_API AInvasionZombies : public AActor
{
	GENERATED_BODY()

public:
	AInvasionZombies();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Console : Chelles.Zombies (lance tout de suite) */
	void LancerInvasion();
	/** Console : Chelles.ZombiesFin (arrete tout de suite) */
	void TerminerInvasion(bool bRepoussee);

	/** Le serveur (ou le jeu solo) decide de tout. */
	bool EstServeur() const;

	// ---- Pour les autres plugins (mode survie...), utilises par leur nom ----

	/** Lance une vague commandee avec les reglages Vague* ci-dessous (serveur). Rien si deja en cours. */
	UFUNCTION()
	void LancerVagueCommandee();

	/** Arrete la vague (ou l'invasion) en cours : les zombies restants s'effondrent (serveur). */
	UFUNCTION()
	void TerminerVagueCommandee();

	/** Vrai = l'invasion automatique toutes les 2 h ne se declenche pas (un mode de jeu a la main). */
	UPROPERTY(Transient)
	bool bInvasionsAutoEnPause = false;

	UPROPERTY(Transient) int32 VagueNombre = 10;
	/** Vie d'un zombie normal (100 = 3 balles dans le corps, comme l'invasion). */
	UPROPERTY(Transient) float VagueVie = 100.f;
	UPROPERTY(Transient) float VagueVitesse = 420.f;
	/** Part des zombies "costauds" (0 a 1) : plus de vie, un peu plus lents, plus grands. */
	UPROPERTY(Transient) float VaguePartCostauds = 0.f;
	UPROPERTY(Transient) float VagueVieCostaud = 300.f;
	/** Les zombies apparaissent autour de ce point (la zone de jeu), entre RayonMin et RayonMax (cm). */
	UPROPERTY(Transient) FVector VagueCentre = FVector::ZeroVector;
	UPROPERTY(Transient) float VagueRayonMin = 1500.f;
	UPROPERTY(Transient) float VagueRayonMax = 4000.f;
	UPROPERTY(Transient) FString VagueMessage;
	UPROPERTY(Transient) float VagueDureeMessage = 5.f;
	UPROPERTY(Transient) float VagueVolumeSirene = 0.6f;

	UPROPERTY(BlueprintAssignable)
	FZombieTueChelles OnZombieTue;
	UPROPERTY(BlueprintAssignable)
	FZombieToucheChelles OnZombieTouche;
	UPROPERTY(BlueprintAssignable)
	FCoupSurJoueurChelles OnCoupSurJoueur;
	/** Mis a vrai par un abonne de OnCoupSurJoueur : le coup n'est pas applique (ex. joueur mis "a terre"). */
	UPROPERTY(Transient)
	bool bCoupGereAilleurs = false;

	/** Tag pose sur un joueur a terre ou mort en mode survie : les zombies l'ignorent. */
	static const FName MarqueJoueurHorsJeu;

protected:
	// ---- Etat partage avec les joueurs connectes ----
	UPROPERTY(Replicated)
	bool bInvasionEnCours = false;

	UPROPERTY(Replicated)
	int32 ZombiesRestants = 0;

	UPROPERTY(Replicated)
	float SecondesRestantes = 0.f;

	UPROPERTY(Replicated)
	TArray<ACharacter*> Zombies;

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAlerte(const FString& Texte, float Duree, float Volume);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFin(const FString& Texte);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastMort(ACharacter* Zombie, bool bTete, FVector Direction);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastAttaque(ACharacter* Zombie);

	// ---- Degats recus par les zombies (serveur) ----
	UFUNCTION()
	void ZombieToucheQuelconque(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur);

	UFUNCTION()
	void ZombieTouchePrecis(AActor* Touche, float Degats, AController* Auteur, FVector PointImpact, UPrimitiveComponent* Composant,
		FName Os, FVector Direction, const UDamageType* TypeDegats, AActor* Causeur);

private:
	void ChargerReglages();
	void TickServeur(float Maintenant);
	void TickLocal(float DeltaSeconds);

	void ChoisirFoyers();
	bool TrouverPointApparition(const FVector& Autour, float DistMin, float DistMax, FVector& Resultat) const;
	ACharacter* FaireApparaitreZombie(const FVector& Sol);
	void PreparerZombie(ACharacter* Zombie);
	void PiloterZombie(ACharacter* Zombie, FEtatZombie& Etat, float Maintenant);
	void Tuer(ACharacter* Zombie, bool bTete, const FVector& Direction, AController* Tueur = nullptr);
	void Habiller(ACharacter* Zombie);

	void JoueursVivants(TArray<APawn*>& Resultat) const;
	static bool EstVivant(APawn* Joueur);
	static float VieMax(APawn* Joueur);

	FReglagesZombies Reglages;

	/** Vague commandee en cours (au lieu d'une invasion automatique). */
	bool bModeVague = false;

	// Serveur
	float ProchaineInvasion = 0.f;
	float DebutInvasion = 0.f;
	float FinInvasion = 0.f;
	int32 AFaireApparaitre = 0;
	float ProchaineApparition = 0.f;
	TArray<FVector> Foyers;
	TMap<TWeakObjectPtr<ACharacter>, FEtatZombie> Etats;
	float ProchaineSynchro = 0.f;

	UPROPERTY(Transient)
	UClass* ClasseZombie = nullptr;

	// Ecran et son (sur chaque PC qui affiche le jeu)
	TSharedPtr<SAlerteZombies> Ecran;
	float AgeAlerte = -1.f;
	float DureeAlerte = 30.f;
	FString TexteAlerte;
	float AgeFin = 100.f;
	FString MessageFin;
	bool bSireneArretee = true;

	UPROPERTY(Transient)
	USireneChelles* Sirene = nullptr;

	UPROPERTY(Transient)
	UAudioComponent* SonSirene = nullptr;

	UPROPERTY(Transient)
	class USkeletalMesh* Peau = nullptr;

	UPROPERTY(Transient)
	class UAnimMontage* MontageAttaque = nullptr;
};
