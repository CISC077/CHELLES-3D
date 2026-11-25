// CHELLES 3D - Les 5 bots ennemis en combinaison Sci-Fi (noir, rouge, jaune, bleu, kaki) - UE 4.27
//
//  - Le serveur (ou le jeu solo) pose les bots au hasard dans la carte a chaque lancement, au niveau
//    de la rue, loin des joueurs, et remplace celui qui meurt apres ReapparitionSecondes.
//  - Chaque bot est un soldat FPS_Game (AI_Soldier : son cerveau, ses animations, sa vie) habille
//    de la combinaison Sci-Fi de sa couleur (memes fichiers que la touche K du joueur, rien n'est modifie).
//  - Chaque bot VOLE (il decolle, tourne autour du joueur en l'air puis se repose), a une ARME
//    et un POUVOIR qui ne sont qu'a lui. Tout se regle dans Plugins\ChellesEnnemis\Bots.txt.
//  - Optimise : rien n'est calcule pour un bot loin de tous les joueurs (DistanceRepos).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BotsCombinaison.generated.h"

class ACharacter;
class APawn;
class UMaterialInterface;
class UParticleSystemComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class USkeletalMesh;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** Armes, pouvoirs et effets : voir le haut de BotsCombinaison.cpp (namespace ChellesBots). */

/** Un bot decrit dans Bots.txt (ligne Bot=...). */
struct FTypeBot
{
	FString Nom;
	FString Materiau;
	uint8 Arme = 0;
	uint8 Pouvoir = 0;
	FLinearColor Couleur = FLinearColor::White;
};

/** Reglages lus dans Plugins\ChellesEnnemis\Bots.txt. */
struct FReglagesBots
{
	bool bActif = true;
	int32 NombreDeChaque = 1;
	FString Classe = TEXT("/Game/FPS_Game/Blueprints/Bonus/AI/AI_Soldier.AI_Soldier_C");
	FString Corps = TEXT("/Game/SciFiSoldier/Meshes/SK_SciFiSoldier.SK_SciFiSoldier");
	float ReapparitionSecondes = 120.f;
	float VieFois = 3.f;
	float EspacementMetres = 60.f;
	float LoinDesJoueursMetres = 50.f;
	float EcartNiveauRueMetres = 12.f;

	float DistanceDetectionMetres = 50.f;
	float DistanceReposMetres = 150.f;

	bool bVol = true;
	float VolToutesLesSecondes = 15.f;
	float DureeVol = 8.f;
	float HauteurVolMetres = 8.f;
	float VitesseVol = 900.f;

	float PorteeArmeMetres = 35.f;
	float DegatsFois = 1.f;
	float RechargePouvoir = 12.f;
	bool bAfficherNom = true;

	TArray<FTypeBot> Types;

	static const FReglagesBots& Lire(bool bRelire = false);
};

/** Ce que tous les joueurs savent d'un bot (envoye par le serveur). */
USTRUCT()
struct FBotRep
{
	GENERATED_BODY()

	UPROPERTY()
	ACharacter* Pion = nullptr;

	UPROPERTY()
	uint8 Type = 0;

	UPROPERTY()
	bool bVol = false;
};

/** Ce que le serveur retient pour chaque bot. */
struct FBotServeur
{
	TWeakObjectPtr<ACharacter> Pion;
	uint8 Type = 0;
	float MortDepuis = -1.f;

	TWeakObjectPtr<APawn> Cible;
	float ProchaineDecision = 0.f;
	float ProchainTir = 0.f;
	int32 RafaleRestante = 0;
	float ProchainTirRafale = 0.f;
	float ProchainPouvoir = 0.f;

	bool bEnVol = false;
	float ProchainVol = 0.f;
	float FinVol = 0.f;
	FVector PointDecollage = FVector::ZeroVector;
	float Phase = 0.f;
	bool bPique = false;         // onde de choc : il descend en pique et frappe en touchant le sol

	float FoudreA = -1.f;        // foudre en attente
	FVector FoudreOu = FVector::ZeroVector;
	bool bEndormi = false;
};

/** Projectile tire par un bot (boule lumineuse de la couleur de l'arme). */
UCLASS(NotPlaceable, NotBlueprintable)
class CHELLESENNEMIS_API AProjectileBot : public AActor
{
	GENERATED_BODY()

public:
	AProjectileBot();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** A regler avant FinishSpawning (serveur). */
	void Preparer(uint8 InEffet, float InVitesse, float InTaille, FLinearColor InCouleur, float InDegatsPourcent, float InRayon, float InGelSecondes);

	UPROPERTY(VisibleAnywhere)
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Forme;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* Lumiere;

	UPROPERTY(VisibleAnywhere)
	UProjectileMovementComponent* Mouvement;

private:
	UFUNCTION()
	void AuChoc(UPrimitiveComponent* Composant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact);

	void Exploser(const FVector& Ou, AActor* ToucheDirect);
	void AppliquerApparence();

	UPROPERTY(Replicated)
	FLinearColor Couleur = FLinearColor::White;

	UPROPERTY(Replicated)
	float Taille = 0.3f;

	UPROPERTY(Replicated)
	float Vitesse = 3000.f;

	uint8 Effet = 0;
	float DegatsPourcent = 5.f;
	float Rayon = 0.f;
	float GelSecondes = 0.f;
	bool bFini = false;
};

/** Le chef des bots : un seul par partie, cree au lancement par le module (serveur), envoye aux joueurs. */
UCLASS(NotPlaceable, Transient)
class CHELLESENNEMIS_API ABotsCombinaison : public AActor
{
	GENERATED_BODY()

public:
	ABotsCombinaison();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool EstServeur() const { return GetNetMode() != NM_Client; }

	/** Vrai si ce personnage est un bot en combinaison (serveur et joueurs). */
	static bool EstUnBot(const ACharacter* Pion);

	/** Tag pose sur chaque bot (serveur). */
	static const FName MarqueBot;

	/** Console : Chelles.Bots [Relire|Plus] */
	void Commande(const TArray<FString>& Arguments);

	/** Degats a un joueur (pourcentage de sa vie max), avec le bot comme auteur. Serveur. */
	static void BlesserJoueur(APawn* Joueur, float Pourcent, AActor* Auteur);

	/** Effet visible chez tous les joueurs. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastEffet(uint8 Effet, FVector Ou, float TailleEffet);

	/** Ralentit (Facteur 0.4) ou gele (Facteur 0) un joueur pendant Duree secondes, sur toutes les machines. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFreiner(APawn* Joueur, float Facteur, float Duree);

	/** Projette un joueur (onde de choc), sur toutes les machines. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastProjeter(ACharacter* Joueur, FVector Elan);

	/** Le chef de la partie (nul s'il n'existe pas). */
	static ABotsCombinaison* Trouver(const UWorld* Monde);

	/** Joueurs vivants de la partie. */
	void JoueursVivants(TArray<APawn*>& Resultat) const;

private:
	UFUNCTION()
	void OnRep_Bots();

	// Serveur
	void PreparerPlaces();
	bool ChoisirPlace(const ACharacter* Modele, FVector& Resultat);
	ACharacter* PoserBot(uint8 Type);
	void PoserTout();
	void Surveiller(float Maintenant);
	void Piloter(FBotServeur& B, ACharacter* E, float Maintenant, float DeltaSeconds);
	void Decoller(FBotServeur& B, ACharacter* E, float Maintenant);
	void Atterrir(FBotServeur& B, ACharacter* E);
	void VolerVers(FBotServeur& B, ACharacter* E, float Maintenant);
	void Tirer(FBotServeur& B, ACharacter* E, APawn* Cible);
	void LancerPouvoir(FBotServeur& B, ACharacter* E, APawn* Cible, float Maintenant);
	void OndeDeChoc(FBotServeur& B, ACharacter* E);
	AProjectileBot* Lancer(ACharacter* E, const FVector& Depart, const FVector& Direction, uint8 Effet, float Vitesse, float TailleTir,
		FLinearColor CouleurTir, float Pourcent, float RayonTir, float Gel, float DureeVie);
	FVector BoucheDuCanon(ACharacter* E) const;
	bool VoitLeJoueur(ACharacter* E, APawn* Joueur) const;
	bool EstMort(ACharacter* E) const;
	void MettreAJourRep();

	// Sur chaque PC qui affiche le jeu
	void Habiller(ACharacter* E, uint8 Type);
	void MontrerVol(ACharacter* E, uint8 Type, bool bVol);
	void TournerLesNoms();
	void FreinsQuiFinissent(float Maintenant);

	UPROPERTY(ReplicatedUsing = OnRep_Bots)
	TArray<FBotRep> Bots;

	TArray<FBotServeur> Serveur;
	TArray<FVector> Places;
	bool bPlacesPretes = false;
	bool bPoseFaite = false;
	float PoseA = 4.f;
	float ProchaineSurveillance = 0.f;
	float ProchainHabillage = 0.f;

	UPROPERTY(Transient)
	USkeletalMesh* CorpsSciFi = nullptr;

	UPROPERTY(Transient)
	TArray<UMaterialInterface*> MateriauxTypes;

	UPROPERTY(Transient)
	UClass* ClasseBot = nullptr;

	/** Bots deja habilles sur ce PC. */
	TSet<TWeakObjectPtr<ACharacter>> Habilles;

	/** Flammes des pieds pendant le vol (sur ce PC). */
	TMap<TWeakObjectPtr<ACharacter>, TArray<TWeakObjectPtr<UParticleSystemComponent>>> Reacteurs;

	/** Noms au-dessus des tetes (sur ce PC). */
	TArray<TWeakObjectPtr<UTextRenderComponent>> Noms;

	/** Joueurs ralentis ou geles (sur chaque machine) : vitesse d'origine et fin. */
	struct FFrein
	{
		float VitesseOrigine = 600.f;
		float VolOrigine = 600.f;
		float Fin = 0.f;
	};
	TMap<TWeakObjectPtr<APawn>, FFrein> Freins;
};
