// CHELLES 3D - Bus pendant l'invasion zombie (ajoute tout seul au lancement du jeu).
//  - Pendant l'invasion : toutes les voitures de la circulation sont retirees (cachees), la route est vide.
//  - Le bus BP_Bus_sity (pack Bus_sity) fait le tour de tous les arrets "BP_Arret_Bus" poses dans la carte,
//    en suivant les routes de la circulation (ChellesCirculation). Son trajet s'adapte a la position des arrets.
//  - A chaque arret, les portes restent fermees : les joueurs les ouvrent / ferment eux-memes (touche E pres d'une porte).
//  - Attente a l'arret : 20 s si le joueur est dehors, 10 s si un joueur est dans le bus.
// Solo et multijoueur : le serveur conduit le bus, chaque machine l'affiche.
// Reglages : Plugins\ChellesBus\Bus.txt   -   Console : Chelles.Bus (tester sans invasion) / Chelles.BusFin
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "BusChelles.generated.h"

class APawn;
class APlayerController;
class UAnimSequence;
class UBoxComponent;
class USkeletalMeshComponent;
class SWidget;

/** Reglages lus dans Bus.txt */
struct FReglagesBus
{
	bool bActif = true;
	bool bRetirerVoitures = true;
	FString ClasseBus = TEXT("/Game/Bus_sity/Blueprints/Bus_sity/BP_Bus_sity.BP_Bus_sity_C");
	float VitesseKmh = 30.f;
	float AttenteDehorsSecondes = 20.f;
	float AttenteDedansSecondes = 10.f;
	FString TouchePorte = TEXT("E");
	float DistancePorteMetres = 3.5f;
	float HauteurPlancher = 35.f;
	float CorrectionOrientation = 0.f;
	float DistanceMaxArretRouteMetres = 40.f;
	bool bMessageDepart = true;
};

/** Une porte du bus (vue sur chaque machine). */
struct FPorteBus
{
	TWeakObjectPtr<USkeletalMeshComponent> Composant;
	TWeakObjectPtr<UAnimSequence> Animation;
	TWeakObjectPtr<UBoxComponent> Bloqueur;
	/** Centre de la porte dans le repere du bus. */
	FVector CentreLocal = FVector::ZeroVector;
	FVector DemiTailleLocale = FVector(40.f, 10.f, 100.f);
	bool bOuverteAffichee = false;
};

UCLASS(NotPlaceable, NotBlueprintable)
class CHELLESBUS_API ABusChelles : public AActor
{
	GENERATED_BODY()

public:
	ABusChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool EstServeur() const { return GetLocalRole() == ROLE_Authority; }

	/** Console Chelles.Bus : bus et route vide tout de suite, sans attendre l'invasion. */
	void ForcerService(bool bOui);

	/** Serveur : un joueur ouvre ou ferme une porte (verifie qu'il est assez pres et que le bus est a l'arret). */
	void BasculerPorte(int32 Index, APawn* Joueur);

	static const FReglagesBus& Reglages();

protected:
	// ------------------------------------------------------------------ Etat partage (serveur -> joueurs)

	/** Invasion en cours : route vide + bus. */
	UPROPERTY(ReplicatedUsing = OnRep_Service)
	bool bServiceActif = false;

	UPROPERTY(ReplicatedUsing = OnRep_Bus)
	APawn* Bus = nullptr;

	/** Le trajet du bus (boucle qui passe par tous les arrets), en points tous les quelques metres. */
	UPROPERTY(ReplicatedUsing = OnRep_Trajet)
	TArray<FVector_NetQuantize> Trajet;

	UPROPERTY(Replicated)
	float ParcouruServeur = 0.f;

	UPROPERTY(Replicated)
	float VitesseServeur = 0.f;

	/** 0 = roule, 1 = a l'arret (portes manoeuvrables), 2 = ferme ses portes avant de partir, 3 = termine son service. */
	UPROPERTY(Replicated)
	uint8 Etat = 0;

	/** Secondes avant le depart (a l'arret). */
	UPROPERTY(Replicated)
	float DepartDans = 0.f;

	/** Bit 0 = porte avant ouverte, bit 1 = porte arriere ouverte... */
	UPROPERTY(ReplicatedUsing = OnRep_Portes)
	uint8 Portes = 0;

	UPROPERTY(Replicated)
	FString NomArretActuel;

	UFUNCTION()
	void OnRep_Service();
	UFUNCTION()
	void OnRep_Bus();
	UFUNCTION()
	void OnRep_Trajet();
	UFUNCTION()
	void OnRep_Portes();

	// ------------------------------------------------------------------ Serveur
	bool bForce = false;
	/** L'invasion est finie mais des joueurs sont dans le bus : il les depose au prochain arret. */
	bool bFinDemandee = false;
	float ProchaineVerification = 0.f;
	float Parcouru = 0.f;
	float Vitesse = 0.f;
	TArray<float> DistancesArrets;
	TArray<FString> NomsArrets;
	int32 ProchainArret = 0;
	float TempsALArret = 0.f;
	float FinFermeture = 0.f;
	float FinDuServiceAuPlusTard = 0.f;

	bool LireInvasion() const;
	void DemarrerService();
	void ArreterService();
	void TickServeur(float DeltaSeconds);
	bool ConstruireTrajet(const FVector& PresDe, float& OutDepart, int32& OutPremierArret);
	void CreerLesRelais();
	bool UnJoueurDansLeBus() const;
	bool JoueurDevantLeBus() const;
	void ListerJoueurs(TArray<APawn*>& OutJoueurs) const;

	// ------------------------------------------------------------------ Chaque machine
	float ParcouruLocal = 0.f;
	TArray<float> Cumuls;
	float LongueurTrajet = 0.f;
	bool bBusPrepare = false;
	TArray<FPorteBus> PortesBus;
	FBox BoiteLocale = FBox(ForceInit);
	float PlancherLocal = 0.f;
	float ZLisse = 0.f;
	bool bZInitialise = false;
	float FinMessage = 0.f;
	FString Message;
	TSharedPtr<SWidget> Ecran;
	FString TexteAide;
	float ProchainNettoyageWidgets = 0.f;
	int32 NettoyagesRestants = 0;

	/** Voitures cachees par ce plugin (pour les remettre exactement comme avant). */
	TArray<TWeakObjectPtr<AActor>> VoituresCachees;
	TArray<TWeakObjectPtr<AActor>> CirculationsArretees;
	bool bVoituresRetirees = false;
	float ProchainRetrait = 0.f;

	void RetirerVoitures();
	void RemettreVoitures();
	void PreparerBus();
	void PlacerBus(float S, float DeltaSeconds, bool bTeleport);
	FVector PointA(float S) const;
	float Boucler(float S) const;
	void AppliquerPortes(bool bInstantane);
	void TickLocal(float DeltaSeconds);
	void CreerEcran();
	FText TexteEcran() const;
	int32 PorteLaPlusProche(const APawn* Joueur, float DistanceMax) const;
	bool EstDansLeBus(const FVector& Position) const;
	void CacherCompteurDuBus();
};

/** Un par joueur en multijoueur : transmet au serveur "j'ouvre / je ferme cette porte". */
UCLASS(NotPlaceable, NotBlueprintable)
class CHELLESBUS_API ABusRelaisChelles : public AActor
{
	GENERATED_BODY()

public:
	ABusRelaisChelles();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServeurBasculerPorte(int32 Index);
};
