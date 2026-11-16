// CHELLES 3D - Ligne de train autour de la carte (ajoutee toute seule au lancement du jeu).
//  - Les arrets sont ecrits dans Plugins\ChellesTrain\Train.txt (adresses de Chelles) : le jeu les retrouve
//    sur la carte (Base Adresse Nationale, une seule fois, gardes dans ArretsTrouves.txt) et pose les panneaux.
//  - Le train du pack Intercity_Train suit les routes de la circulation (ChellesCirculation), au milieu de la rue
//    comme un tramway, sur des rails dessines sur la route. Il fait le tour en boucle.
//  - A chaque arret : portes ouvertes 30 s (cote trottoir), puis fermeture et depart.
//  - Les voitures de la circulation qui sont sur son passage disparaissent, et reviennent apres.
// Solo et multijoueur : le serveur conduit le train, chaque machine l'affiche.
// Console : Chelles.Train (etat) / Chelles.TrainVoir (vous emmene au prochain arret)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "Interfaces/IHttpRequest.h"
#include "TrainChelles.generated.h"

class AArretTrainChelles;
class APawn;
class UBoxComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class SWidget;

/** Un arret ecrit dans Train.txt */
struct FArretEcrit
{
	FString Nom;
	FString Adresse;
	bool bCoordonnees = false;
	FVector2D Position = FVector2D::ZeroVector; // cm, repere de la carte
};

/** Reglages lus dans Train.txt */
struct FReglagesTrain
{
	bool bActif = true;
	TArray<FArretEcrit> Arrets;
	float RegrouperMetres = 150.f;
	bool bOrdreAuto = true;
	float DistanceMaxArretRouteMetres = 70.f;
	int32 NombreDeVoitures = 2;
	float Echelle = 1.f;
	float VitesseKmh = 35.f;
	float AttenteSecondes = 30.f;
	bool bMilieuDeLaRoute = true;
	bool bRails = true;
	bool bTraverses = true;
	bool bPassagers = true;
	bool bRetirerVoitures = true;
	bool bPortesDeuxCotes = false;
	bool bInverserSens = false;
	float HauteurRails = 0.f;
	float HauteurPlancher = -1.f;
	bool bMessages = true;
	double LatitudeCentre = 48.8795;
	double LongitudeCentre = 2.5912;
};

/** Un arret tel que les joueurs le recoivent (pour poser le panneau chez eux). */
USTRUCT()
struct FArretTrainInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize Position = FVector::ZeroVector;

	UPROPERTY()
	float Lacet = 0.f;

	UPROPERTY()
	FString Nom;

	/** Distance sur le trajet ou la tete du train s'arrete. */
	UPROPERTY()
	float DistanceTete = 0.f;

	/** Pose a la main dans la carte : pas de panneau a ajouter. */
	UPROPERTY()
	bool bManuel = false;
};

/** Une porte du train (vue sur chaque machine). */
struct FPorteTrain
{
	TWeakObjectPtr<UStaticMeshComponent> Composant;
	TWeakObjectPtr<UBoxComponent> Bloqueur;
	/** Marchepied (rampe) qui sort quand la porte s'ouvre, pour monter depuis la route. */
	TWeakObjectPtr<UStaticMeshComponent> Rampe;
	FVector PositionFermee = FVector::ZeroVector;
	FVector Glissement = FVector::ZeroVector;
	bool bCoteQuai = true;
};

/** Une voiture du train (vue sur chaque machine). */
struct FVoitureTrain
{
	TWeakObjectPtr<USceneComponent> Racine;
	/** Boite de la voiture dans son repere (X = avant du train, Y = droite), sans l'echelle. */
	FBox Boite = FBox(ForceInit);
	float Plancher = 100.f;
	float DemiEcartBogies = 900.f;
	float Longueur = 2600.f; // avec l'echelle
	float Decalage = 0.f;    // distance entre l'avant du train et l'avant de cette voiture (avec l'echelle)
	TArray<FPorteTrain> Portes;
	float ZLisse = 0.f;
	bool bZInitialise = false;
};

UCLASS(NotPlaceable, NotBlueprintable)
class CHELLESTRAIN_API ATrainChelles : public AActor
{
	GENERATED_BODY()

public:
	ATrainChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool EstServeur() const { return GetLocalRole() == ROLE_Authority; }

	static const FReglagesTrain& Reglages();

	/** Console : etat du train dans le journal. */
	void DecrireEtat() const;
	/** Console : le joueur local est emmene a cote du prochain arret. */
	void EmmenerAuProchainArret(APawn* Joueur) const;

protected:
	// ------------------------------------------------------------------ Etat partage (serveur -> joueurs)

	/** Le trajet du train (boucle qui passe par tous les arrets), en points tous les quelques metres. */
	UPROPERTY(ReplicatedUsing = OnRep_Trajet)
	TArray<FVector_NetQuantize> Trajet;

	UPROPERTY(ReplicatedUsing = OnRep_Arrets)
	TArray<FArretTrainInfo> ArretsInfos;

	UPROPERTY(Replicated)
	float ParcouruServeur = 0.f;

	UPROPERTY(Replicated)
	float VitesseServeur = 0.f;

	/** 0 = roule, 1 = a l'arret portes ouvertes, 2 = fermeture des portes. */
	UPROPERTY(Replicated)
	uint8 Etat = 0;

	UPROPERTY(Replicated)
	float DepartDans = 0.f;

	/** Arret ou le train est / vers lequel il roule. */
	UPROPERTY(Replicated)
	int32 ArretCible = 0;

	UFUNCTION()
	void OnRep_Trajet();
	UFUNCTION()
	void OnRep_Arrets();

	// ------------------------------------------------------------------ Serveur
	bool bTrajetConstruit = false;
	bool bGeocodageLance = false;
	int32 RequetesEnCours = 0;
	float FinAttenteGeocodage = 0.f;
	float ProchainEssaiTrajet = 0.f;
	int32 EssaisTrajet = 0;
	float Parcouru = 0.f;
	float Vitesse = 0.f;
	float TempsALArret = 0.f;
	float FinFermeture = 0.f;
	/** Adresse -> latitude / longitude (ArretsTrouves.txt + reponses de la BAN). */
	TMap<FString, FVector2D> AdressesTrouvees;
	TMap<FString, FString> LibellesTrouves;

	void LancerGeocodage();
	void DemanderAdresse(const FString& Adresse, int32 Essai);
	void ReponseAdresse(FHttpRequestPtr Requete, FHttpResponsePtr Reponse, bool bReussi, FString Adresse, int32 Essai);
	void LireArretsTrouves();
	void EcrireArretsTrouves() const;
	bool ConstruireTrajet();
	void TickServeur(float DeltaSeconds);
	bool JoueurDevantLeTrain() const;
	void ListerJoueurs(TArray<APawn*>& OutJoueurs) const;

	// ------------------------------------------------------------------ Chaque machine
	float ParcouruLocal = 0.f;
	TArray<float> Cumuls;
	float LongueurTrajet = 0.f;
	float LongueurTrain = 0.f;
	bool bTrainPrepare = false;
	bool bPortesOuvertesAffichees = false;
	float OuverturePortes = 0.f; // 0 = fermees, 1 = ouvertes (animation)
	TArray<FVoitureTrain> Voitures;
	TArray<TWeakObjectPtr<AArretTrainChelles>> PanneauxPoses;

	UPROPERTY(Transient)
	UHierarchicalInstancedStaticMeshComponent* Rails = nullptr;
	UPROPERTY(Transient)
	UHierarchicalInstancedStaticMeshComponent* TraversesHism = nullptr;

	FString TexteAide;
	TSharedPtr<SWidget> Ecran;

	/** Voitures de la circulation cachees par ce plugin (pour les remettre comme avant). */
	TSet<TWeakObjectPtr<AActor>> VoituresCachees;
	float ProchainRetrait = 0.f;

	void PreparerTrain();
	void CreerVoiture(int32 Index, UStaticMesh* Corps, bool bRetournee, bool bLocomotive);
	void PoserRails();
	void PoserPanneaux();
	void PlacerTrain(float STete, float DeltaSeconds, bool bTeleport);
	void AnimerPortes(float DeltaSeconds);
	void RetirerVoituresSurLePassage();
	void RemettreToutesLesVoitures();
	FVector PointA(float S) const;
	FVector DirectionA(float S) const;
	float Boucler(float S) const;
	bool SolSous(const FVector& P, float ZIndice, float& OutZ) const;
	bool EstDansLeTrain(const FVector& Position) const;
	void TickLocal(float DeltaSeconds);
	void CreerEcran();
	FText TexteEcran() const;
	FString NomAffiche(int32 IndexArret) const;
};
