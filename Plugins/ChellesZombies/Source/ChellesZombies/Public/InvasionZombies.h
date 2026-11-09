// CHELLES 3D - Invasion zombies : le chef d'orchestre (ajoute tout seul au lancement du jeu)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InvasionZombies.generated.h"

class ACharacter;
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
	float ProchainChoixCible = 0.f;
	float ProchaineAttaque = 0.f;
	float CoupPrevuA = -1.f;
	TWeakObjectPtr<APawn> CibleDuCoup;

	FVector DernierePosition = FVector::ZeroVector;
	float ProchainControle = 0.f;
	int32 Bloque = 0;
	float ContournerJusqua = 0.f;
	float CoteContournement = 1.f;
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
	void MulticastFin(bool bRepoussee);

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
	void Tuer(ACharacter* Zombie, bool bTete, const FVector& Direction);
	void Habiller(ACharacter* Zombie);

	void JoueursVivants(TArray<APawn*>& Resultat) const;
	static bool EstVivant(APawn* Joueur);
	static float VieMax(APawn* Joueur);

	FReglagesZombies Reglages;

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
