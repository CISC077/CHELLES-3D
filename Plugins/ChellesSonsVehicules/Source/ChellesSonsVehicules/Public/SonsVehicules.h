// CHELLES 3D - Sons des vehicules (UE 4.27)
// Bruit du moteur au ralenti, qui monte avec la vitesse et l'acceleration, et klaxon (touche H),
// pour tous les vehicules : voitures de la circulation, motoneige, voiture de la roue TAB, bus.
// Aucun autre plugin ni Blueprint n'est modifie : les vehicules sont reconnus par leur classe.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SonsVehicules.generated.h"

class UAudioComponent;
class USoundBase;
class APlayerController;

/** Famille de vehicule : choisit le son du moteur. */
enum class EGenreVehicule : uint8
{
	Voiture,    // voiture de la circulation (deplacee par le Circulation Spawner)
	Bus,        // BP_Bus_sity (deplace par le plugin ChellesBus)
	Motoneige,  // BP_Snowmobile
	Conduite    // tout autre vehicule a roues (voiture de la roue TAB, voitures des packs)
};

/** Reglages d'un moteur (lus dans SonsVehicules.txt). */
struct FMoteurReglages
{
	FString Chemin;
	float Volume = 0.6f;
	float Ralenti = 150.f;
	float RegimeMax = 4500.f;
	float Rapport = 22.f;   // km/h par rapport de boite
	float Grave = 1.f;      // multiplie la hauteur du son

	USoundBase* Son = nullptr;     // garde en memoire par ASonsVehicules::SonsGardes
	bool bChargementTente = false;
};

/** Reglages lus dans Plugins/ChellesSonsVehicules/SonsVehicules.txt */
struct FReglagesSonsVehicules
{
	bool bActif = true;
	float Volume = 1.f;
	FString ToucheKlaxon = TEXT("H");
	float DistanceMoteur = 4500.f; // cm
	int32 MoteursMax = 8;

	FMoteurReglages Voiture;
	FMoteurReglages Bus;
	FMoteurReglages Motoneige;

	FString Klaxon = TEXT("/Game/DV_Car_Horo/Sounds/Cues/SC_Car_Horn");
	float KlaxonVolume = 0.8f;
	float KlaxonBusGrave = 0.75f;

	bool bKlaxonIA = true;
	float KlaxonIAArret = 3.f;
	float KlaxonIAChance = 12.f;
	float KlaxonIAHasard = 50.f;
	bool bKlaxonBusDepart = true;
};

/** Un vehicule suivi par le gestionnaire. */
struct FVehiculeSuivi
{
	TWeakObjectPtr<AActor> Acteur;
	EGenreVehicule Genre = EGenreVehicule::Voiture;

	/** Notre moteur (cree a la demande, attache au vehicule). */
	TWeakObjectPtr<UAudioComponent> Moteur;
	/** Sons de moteur deja presents dans le Blueprint du pack (on les garde s'ils marchent). */
	TArray<TWeakObjectPtr<UAudioComponent>> MoteursPack;
	/** Klaxon du pack (motoneige) : on ne double pas le klaxon. */
	bool bKlaxonPack = false;
	/** Composant de roues du pack (WheeledVehicleMovementComponent), lu par reflexion. */
	TWeakObjectPtr<UActorComponent> Roues;

	FVector DernierePosition = FVector::ZeroVector;
	bool bPositionConnue = false;
	float Vitesse = 0.f;        // cm/s, lissee
	float Acceleration = 0.f;   // cm/s2, lissee
	float Regime = 0.f;         // tours/minute affiches au son
	float Variation = 1.f;      // chaque voiture a une voix un peu differente
	float VolumeActuel = 0.f;
	float TempsArret = 0.f;
	float ProchainKlaxon = 0.f;
	float Distance = BIG_NUMBER;
	bool bEcoute = false;       // fait partie des moteurs les plus proches
	bool bVu = false;

	/** Klaxon en cours (IA ou joueur). */
	TWeakObjectPtr<UAudioComponent> KlaxonEnCours;
	float FinKlaxon = 0.f;      // 0 = tenu par le joueur
	int32 CoupsRestants = 0;
	float ProchainCoup = 0.f;
};

UCLASS(NotPlaceable, Transient)
class CHELLESSONSVEHICULES_API ASonsVehicules : public AActor
{
	GENERATED_BODY()

public:
	ASonsVehicules();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Joue un klaxon sur ce vehicule (appele aussi par le relais reseau). Duree <= 0 : tenu par le joueur. */
	void Klaxonner(AActor* Vehicule, float Duree, int32 Coups = 1);
	/** Arrete un klaxon tenu. */
	void ArreterKlaxon(AActor* Vehicule);

	/** Console : Chelles.Vehicules [0|1|Info|Relire|Klaxon] */
	void Commande(const TArray<FString>& Arguments);

private:
	void LireReglages();
	void Rechercher();
	void Ajouter(AActor* Acteur, EGenreVehicule Genre);
	void MettreAJourVehicule(FVehiculeSuivi& V, float Dt, float Maintenant, const FVector& Ecoute, float VolumeGlobal);
	void KlaxonsIA(float Maintenant);
	void KlaxonJoueur(float Maintenant);
	void MettreAJourKlaxon(FVehiculeSuivi& V, float Maintenant);
	void CreerLesRelais();
	void CouperTout();

	FVehiculeSuivi* Trouver(AActor* Acteur);
	FMoteurReglages& ReglagesMoteur(EGenreVehicule Genre);
	USoundBase* ChargerSon(FMoteurReglages& M);
	USoundBase* SonKlaxon();
	float VolumeOptions() const;
	float RegimeCible(const FVehiculeSuivi& V, const FMoteurReglages& M) const;
	bool MoteurPackActif(const FVehiculeSuivi& V) const;
	bool EstConduitParUnJoueur(const AActor* Acteur) const;
	APlayerController* JoueurLocal() const;
	AActor* VehiculeDuJoueur() const;

	FReglagesSonsVehicules Reglages;
	TArray<FVehiculeSuivi> Vehicules;

	UPROPERTY(Transient)
	USoundBase* SonKlaxonCharge = nullptr;

	/** Garde les sons charges en memoire (sinon le ramasse-miettes peut les retirer). */
	UPROPERTY(Transient)
	TArray<USoundBase*> SonsGardes;
	bool bKlaxonTente = false;

	/** Volume d'origine des sons des packs (pour leur appliquer le reglage du menu). */
	/** X = volume voulu par le pack, Y = dernier volume que nous avons mis (si le pack le change, on suit). */
	TMap<TWeakObjectPtr<UAudioComponent>, FVector2D> VolumesPack;

	TWeakObjectPtr<UClass> ClasseCirculation;
	bool bClasseCirculationCherchee = false;

	bool bCoupe = false;
	bool bKlaxonJoueurTenu = false;
	TWeakObjectPtr<AActor> VehiculeKlaxonJoueur;
	float ProchaineRecherche = 0.f;
	float ProchainKlaxonHasard = 20.f;
	float ProchainKlaxonGlobal = 0.f;
	float ProchainRelais = 0.f;
	float VolumeOptionsCache = 1.f;
	float ProchaineLectureOptions = 0.f;
};

/** Un par joueur en multijoueur : fait entendre le klaxon du joueur chez tout le monde. */
UCLASS(NotPlaceable, NotBlueprintable)
class CHELLESSONSVEHICULES_API ARelaisKlaxonChelles : public AActor
{
	GENERATED_BODY()

public:
	ARelaisKlaxonChelles();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServeurKlaxon(AActor* Vehicule, bool bAppuye);

	UFUNCTION(NetMulticast, Reliable)
	void TousKlaxon(AActor* Vehicule, bool bAppuye);
};
