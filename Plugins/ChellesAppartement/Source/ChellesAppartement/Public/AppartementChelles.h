// CHELLES 3D - L'APPARTEMENT DU JOUEUR (26/11/2026)
//
// Touche F1 (Appartement.txt) : on rentre chez soi, F1 ou la porte SORTIE : on revient au meme endroit du quartier.
//  - GARDE-ROBE : les skins que le joueur a portes (rangees automatiquement a chaque visite), dalle HABILLAGE.
//  - GARAGE     : les vehicules que le joueur a conduits (voiture volee, motoneige, drone...), dalles PARTIR EN ...
//  - TROPHEES   : zombies tues, missions reussies, meilleure vague du Refuge, XP, temps de jeu, collection.
// Un appartement par joueur, cache tres haut dans le ciel : personne d'autre ne le voit.
// Tout est construit par le code avec les formes de base d'Unreal : aucun fichier .uasset, aucune carte a modifier.
#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "AppartementChelles.generated.h"

class ACharacter;
class AController;
class APawn;
class APlayerController;
class SWidget;
class UGameViewportClient;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace ChellesAppart
{
	/** Les trophees de la vitrine (l'ordre = l'ordre des socles). */
	enum ETrophee : uint8
	{
		Zombies = 0,
		Missions,
		Vague,
		XP,
		Heures,
		Collection,
		NombreTrophees
	};

	/** Ce que le serveur envoie au joueur. */
	enum ECompteur : uint8
	{
		CompteZombie = 0,
		CompteMission = 1,
		CompteVague = 2
	};

	/** Ce que fait une dalle lumineuse au sol. */
	enum EDalle : uint8
	{
		DalleSortie = 0,
		DalleHabillage = 1,
		DalleDepart = 2
	};
}

/** Une dalle PARTIR EN ... du garage : nom du mode de la roue (ChellesRoue) + nom affiche. */
struct FDepartAppartement
{
	FString Mode;
	FString Nom;
};

/** Appartement.txt (Bloc-notes, sans recompiler). */
struct CHELLESAPPARTEMENT_API FReglagesAppartement
{
	bool bActif = true;
	TArray<FKey> Touches;
	/** Ou est cache l'appartement (cm). Chaque joueur a le sien, decale de Ecart sur Y. */
	FVector Position = FVector(0.f, 0.f, 200000.f);
	float Ecart = 5000.f;
	FString Titre = TEXT("MON APPARTEMENT");
	bool bAnimerMannequins = true;
	int32 PlacesGardeRobe = 6;
	int32 PlacesGarage = 4;
	float TailleMaxVehicule = 800.f;
	TArray<FDepartAppartement> Departs;
	/** Bronze, argent, or de chaque trophee. */
	int32 Seuils[ChellesAppart::NombreTrophees][3];
	FLinearColor CouleurSol = FLinearColor(0.32f, 0.18f, 0.09f);
	FLinearColor CouleurMurs = FLinearColor(0.85f, 0.80f, 0.70f);
	FLinearColor CouleurGarage = FLinearColor(0.10f, 0.10f, 0.11f);
	FLinearColor CouleurNeon = FLinearColor(0.10f, 0.75f, 1.00f);
	float Lumiere = 8000.f;
	bool bMessages = true;
	FString SonTrophee;
	FString SonPorte;
};

CHELLESAPPARTEMENT_API const FReglagesAppartement& ReglagesAppartement();
CHELLESAPPARTEMENT_API void RechargerReglagesAppartement();

/** Un morceau d'un skin ou d'un vehicule range (corps, tete, roue...). */
USTRUCT()
struct FMorceauAppartement
{
	GENERATED_BODY()

	/** Chemin du SkeletalMesh ou du StaticMesh. */
	UPROPERTY(SaveGame)
	FString Maillage;

	UPROPERTY(SaveGame)
	bool bSquelette = true;

	UPROPERTY(SaveGame)
	TArray<FString> Materiaux;

	/** Place par rapport aux pieds du personnage (ou au centre du vehicule). */
	UPROPERTY(SaveGame)
	FTransform Place;

	/** Animation (Animation Blueprint) du corps, pour que le mannequin ne soit pas fige en T. */
	UPROPERTY(SaveGame)
	FString Anim;
};

/** Un skin ou un vehicule range dans l'appartement. */
USTRUCT()
struct FPieceAppartement
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FString Nom;

	UPROPERTY(SaveGame)
	TArray<FMorceauAppartement> Morceaux;

	/** Pour ne pas ranger deux fois la meme chose. */
	FString Cle() const;
};

/** Sauvegarde de l'appartement, sur le PC du joueur (Saved\SaveGames\ChellesAppartement.sav). */
UCLASS()
class CHELLESAPPARTEMENT_API UAppartementSauvegarde : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	int32 ZombiesTues = 0;

	UPROPERTY(SaveGame)
	int32 MissionsReussies = 0;

	UPROPERTY(SaveGame)
	int32 MeilleureVague = 0;

	UPROPERTY(SaveGame)
	int32 Visites = 0;

	/** Nombre de skins + vehicules differents ranges depuis le debut. */
	UPROPERTY(SaveGame)
	int32 Collection = 0;

	/** Rang deja annonce de chaque trophee (0 = rien, 1 bronze, 2 argent, 3 or). */
	UPROPERTY(SaveGame)
	TArray<int32> RangsAnnonces;

	UPROPERTY(SaveGame)
	TArray<FPieceAppartement> GardeRobe;

	UPROPERTY(SaveGame)
	TArray<FPieceAppartement> Garage;
};

/** Un par joueur : son appartement (murs, garde-robe, garage, trophees) et la touche F1. */
UCLASS(NotPlaceable, Transient)
class CHELLESAPPARTEMENT_API AAppartementJoueur : public AActor
{
	GENERATED_BODY()

public:
	AAppartementJoueur();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Numero de l'appartement (place dans le ciel). */
	UPROPERTY(ReplicatedUsing = QuandNumero)
	int32 Numero = 0;

	/** Vrai quand le joueur est chez lui. */
	UPROPERTY(ReplicatedUsing = QuandDedans)
	bool bDedans = false;

	UFUNCTION()
	void QuandNumero();

	UFUNCTION()
	void QuandDedans();

	UFUNCTION(Server, Reliable)
	void ServeurEntrer();

	UFUNCTION(Server, Reliable)
	void ServeurSortir();

	/** Le serveur compte un exploit pour ce joueur (sa sauvegarde est sur son PC). */
	UFUNCTION(Client, Reliable)
	void ClientCompter(uint8 Quoi, int32 Quantite);

	UFUNCTION(Client, Reliable)
	void ClientMessage(const FString& Texte);

	/** Vrai sur le PC du joueur a qui appartient cet appartement. */
	bool EstLocal() const;

	/** Touche F1 / console : rentrer ou sortir. */
	void EntrerOuSortir();
	void Sortir(const FString& ModeApres);

	FString Resume() const;
	void ViderRangements();
	void TrophesDeTest();

	/** Serveur : un zombie tue par ce joueur, une mission reussie, une vague atteinte. */
	void ServeurCompter(uint8 Quoi, int32 Quantite);

private:
	// ---- Construction (sur toutes les machines : le serveur a besoin des murs et du sol) ----
	void Placer();
	void Construire();
	UStaticMeshComponent* Bloc(UStaticMesh* Forme, const FVector& Centre, const FVector& Taille, const FLinearColor& Couleur,
		bool bCollision = true, const FRotator& Rotation = FRotator::ZeroRotator, USceneComponent* Parent = nullptr);
	UTextRenderComponent* Texte(const FString& Contenu, const FVector& Lieu, float Yaw, float Taille, const FColor& Couleur,
		USceneComponent* Parent = nullptr);
	UPointLightComponent* Lampe(const FVector& Lieu, float Intensite, const FLinearColor& Couleur, float Rayon);
	UMaterialInstanceDynamic* Peinture(const FLinearColor& Couleur);
	void AjouterDalle(uint8 Type, const FVector2D& Centre, const FVector2D& Taille, const FString& Mode, const FString& Titre,
		const FLinearColor& Couleur);
	void MettreAJourVisibilite();

	// ---- Rangements (sur le PC du joueur seulement) ----
	void ChargerSauvegarde();
	void EcrireSauvegarde();
	bool Capturer(APawn* Pion, bool bVehicule, FPieceAppartement& Sortie) const;
	bool Ranger(TArray<FPieceAppartement>& Liste, const FPieceAppartement& Piece, int32 Max);
	void SurveillerVehicule();
	void RemplirTout();
	void ViderExpositions();
	void Exposer(const FPieceAppartement& Piece, USceneComponent* Socle, const FVector& TailleMax, bool bAnimer);
	void RemplirVitrine();
	int32 Valeur(int32 Trophee) const;
	int32 Rang(int32 Trophee) const;
	void AnnoncerTrophees(bool bMessage);
	void LireXP();
	void VerifierDalles();
	void Message(const FString& Texte);
	void Son(const FString& Chemin);

	// ---- Serveur ----
	void TickServeur();

	UPROPERTY(Transient)
	USceneComponent* Racine = nullptr;

	/** Ce qui est refait a chaque visite (mannequins, vehicules, coupes). */
	UPROPERTY(Transient)
	USceneComponent* Expositions = nullptr;

	UPROPERTY(Transient)
	TMap<uint32, UMaterialInstanceDynamic*> Peintures;

	UPROPERTY(Transient)
	UStaticMesh* Cube = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Cylindre = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Cone = nullptr;

	UPROPERTY(Transient)
	UStaticMesh* Sphere = nullptr;

	UPROPERTY(Transient)
	UAppartementSauvegarde* Sauvegarde = nullptr;

	struct FDalle
	{
		uint8 Type = 0;
		FVector2D Centre;
		FVector2D Taille;
		FString Mode;
		bool bArmee = true;
	};
	TArray<FDalle> Dalles;

	FString NomSauvegarde;
	FStreamableManager Chargeur;
	TSharedPtr<FStreamableHandle> Chargement;

	UPROPERTY(Transient)
	TArray<USceneComponent*> SoclesGardeRobe;

	UPROPERTY(Transient)
	TArray<USceneComponent*> SoclesGarage;

	int32 XPTotale = 0;
	double SecondesJouees = 0.0;
	bool bConstruit = false;
	bool bAAfficher = false;
	FString ModeApresSortie;
	float ProchainEssai = 0.f;
	float ProchaineVerif = 0.f;
	TWeakObjectPtr<APawn> DernierPion;
	float DepuisNouveauPion = 0.f;
	bool bPionNote = false;
	TSharedPtr<SWidget> Bandeau;
	TWeakObjectPtr<UGameViewportClient> VueBandeau;
	float FinBandeau = 0.f;

	// Serveur : pour revenir au meme endroit
	TWeakObjectPtr<APawn> PionDedans;
	FVector RetourLieu = FVector::ZeroVector;
	FRotator RetourRotation = FRotator::ZeroRotator;
	uint8 RetourMarche = 0;
	int32 DerniereVagueEnvoyee = 0;
};

/** Serveur (ou partie solo) : un appartement par joueur + ecoute des zombies, missions et vagues. */
UCLASS(NotPlaceable, Transient)
class CHELLESAPPARTEMENT_API AAppartementGestion : public AActor
{
	GENERATED_BODY()

public:
	AAppartementGestion();

	virtual void Tick(float DeltaSeconds) override;

	/** Evenement OnZombieTue de ChellesZombies (branche par son nom, sans lien entre plugins). */
	UFUNCTION()
	void QuandZombieTue(ACharacter* Zombie, AController* Tueur, bool bTete);

	AAppartementJoueur* AppartementDe(const AController* Joueur) const;

private:
	void Joueurs();
	void Ecouter();
	AActor* Trouver(const TCHAR* NomClasse, TWeakObjectPtr<AActor>& Cache, float& Prochain);

	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AAppartementJoueur>> Appartements;
	TWeakObjectPtr<AActor> Invasion;
	TWeakObjectPtr<AActor> InvasionBranchee;
	TWeakObjectPtr<AActor> Missions;
	TWeakObjectPtr<AActor> Survie;
	float ProchainInvasion = 0.f;
	float ProchainMissions = 0.f;
	float ProchainSurvie = 0.f;
	float ProchainTour = 0.f;
	int32 PhaseAvant = 0;
	int32 VagueAvant = 0;
};
