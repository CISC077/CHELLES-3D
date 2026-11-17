// CHELLES 3D - MISSIONS DANS LE QUARTIER (ajoutees toutes seules au lancement du jeu)
//
//  1. ESCORTE DU TRAMWAY : le TRAMWAY (plugin ChellesTrain) est attaque a chaque arret. Restez pres de lui
//     et gardez-le en vie jusqu'a 3 arrets.
//  2. DEFENDRE LA MAIRIE : tenez la Mairie 3 minutes contre des vagues de zombies.
//  3. CHASSE AUX ZOMBIES : tuez 25 zombies en 4 minutes, la ou vous etes.
//
// Touche N = ecran MISSIONS (l'hote choisit en multijoueur). Reussite = XP pour les pouvoirs.
// Les zombies viennent du plugin ChellesZombies, pilote par le NOM de ses fonctions et variables
// (LancerVagueCommandee, VagueNombre...), comme le fait le Dernier Refuge : rien n'est modifie ailleurs.
// Reglages : Plugins\ChellesMissions\Missions.txt. Console : Chelles.Mission 1/2/3, Chelles.MissionFin,
// Chelles.MissionIci Mairie, Chelles.Missions (etat dans le journal).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Engine/NetSerialization.h"
#include "MissionsChelles.generated.h"

class AController;
class ACharacter;
class APawn;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class SMissionsEcran;
class SMissionsMenu;

/** Les missions (le numero est celui de la console Chelles.Mission). */
namespace EMissionChelles
{
	enum Type : uint8
	{
		Aucune = 0,
		Tramway = 1,
		Mairie = 2,
		Chasse = 3
	};
}

namespace EPhaseMission
{
	enum Type : uint8
	{
		Aucune = 0,
		Rejoindre = 1, // aller au tramway / a la Mairie
		EnCours = 2,
		Reussie = 3,
		Ratee = 4
	};
}

/** Reglages lus dans Plugins\ChellesMissions\Missions.txt (Bloc-notes, sans recompiler). */
struct FReglagesMissions
{
	bool bActif = true;
	FKey Touche = EKeys::N;
	float TempsPourRejoindre = 180.f;
	float AffichageFin = 8.f;
	float VieZombie = 100.f;
	float VitesseZombie = 420.f;
	float TousATerre = 10.f;

	// Escorte du tramway
	int32 TramArrets = 3;
	float TramVie = 100.f;
	float TramRayonDegats = 700.f;
	float TramDegatsParZombie = 2.f;
	float TramLongueur = 2600.f;
	int32 TramZombies = 8;
	int32 TramZombiesEnPlus = 3;
	float TramApparitionMin = 1500.f;
	float TramApparitionMax = 3500.f;
	float TramRejoindre = 3000.f;
	float TramDistanceMax = 7000.f;
	float TramAbandon = 25.f;
	int32 TramXP = 150;

	// Defendre la Mairie
	float MairieDuree = 180.f;
	float MairieVie = 100.f;
	float MairieRayonDegats = 1200.f;
	float MairieDegatsParZombie = 1.5f;
	int32 MairieZombies = 8;
	int32 MairieZombiesEnPlus = 3;
	float MairieApparitionMin = 2500.f;
	float MairieApparitionMax = 4500.f;
	float MairieRejoindre = 4000.f;
	int32 MairieXP = 150;

	// Chasse aux zombies
	int32 ChasseObjectif = 25;
	float ChasseDuree = 240.f;
	int32 ChasseZombies = 10;
	float ChasseApparitionMin = 1500.f;
	float ChasseApparitionMax = 4000.f;
	int32 ChasseXP = 100;
};

/**
 * Un seul par niveau. Le serveur (ou le jeu solo) le cree, il arrive chez les autres joueurs par le reseau.
 * - Serveur : demarre la mission, lance les vagues de zombies, compte les arrets / kills / la vie, decide
 *   de la reussite ou de l'echec.
 * - Chaque machine : ecran MISSIONS (touche N), panneau de la mission en haut a gauche, balise lumineuse
 *   et repere a l'ecran vers l'objectif, XP de reussite.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient)
class CHELLESMISSIONS_API AMissionsChelles : public AActor
{
	GENERATED_BODY()

public:
	AMissionsChelles();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static const FReglagesMissions& Reglages();
	bool EstServeur() const { return GetLocalRole() == ROLE_Authority; }

	// ---- Serveur ----
	/** Lance une mission (1 tramway, 2 Mairie, 3 chasse). Rend le message d'erreur, vide si c'est parti. */
	FString Demarrer(uint8 Quelle);
	/** Arrete la mission en cours (abandon). */
	void Abandonner();

	// ---- Console ----
	void CommandeMissionIci(const TArray<FString>& Arguments);
	void DecrireEtat() const;
	void OuvrirMenu();
	void FermerMenu();

	/** Evenement de ChellesZombies (branche par son nom) : un zombie tue au combat. */
	UFUNCTION()
	void QuandZombieTue(ACharacter* Zombie, AController* Tueur, bool bTete);

	// ---- Etat connu de tous ----
	UPROPERTY(Replicated)
	uint8 Mission = EMissionChelles::Aucune;

	UPROPERTY(Replicated)
	uint8 Phase = EPhaseMission::Aucune;

	/** Temps restant affiche (pour rejoindre, pour tenir, pour la chasse). */
	UPROPERTY(Replicated)
	float Secondes = 0.f;

	/** Vie du tramway / de la Mairie (0 a VieMax). */
	UPROPERTY(Replicated)
	float Vie = 0.f;

	UPROPERTY(Replicated)
	float VieMax = 0.f;

	/** Arrets atteints / zombies tues. */
	UPROPERTY(Replicated)
	int32 Avancement = 0;

	UPROPERTY(Replicated)
	int32 Objectif = 0;

	UPROPERTY(Replicated)
	int32 ZombiesRestants = 0;

	/** Ou aller (balise lumineuse + repere a l'ecran). */
	UPROPERTY(Replicated)
	FVector_NetQuantize Cible = FVector::ZeroVector;

	UPROPERTY(Replicated)
	bool bCibleConnue = false;

	/** Ligne d'information (prochain arret, alerte "revenez", raison de l'echec...). */
	UPROPERTY(Replicated)
	FString Detail;

	/** Alerte en rouge (ex. le tramway est abandonne, la Mairie est attaquee). */
	UPROPERTY(Replicated)
	bool bAlerte = false;

	/** XP donnee a chaque joueur si la mission est reussie. */
	UPROPERTY(Replicated)
	int32 XPGagnee = 0;

	/** Pour griser les boutons de l'ecran MISSIONS. */
	UPROPERTY(Replicated)
	bool bTramwayDispo = false;

	UPROPERTY(Replicated)
	bool bMairieConnue = false;

	UPROPERTY(Replicated)
	bool bZombiesDispo = false;

	/** Message court sur l'ecran de cette machine. */
	void MessageLocal(const FString& Texte);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// ---- Serveur ----
	void ChargerPositions();
	void SauverPositions() const;
	void TickServeur(float DeltaSeconds);
	void TickTramway(float DeltaSeconds, float Heure);
	void TickMairie(float DeltaSeconds, float Heure);
	void TickChasse(float DeltaSeconds, float Heure);
	void Finir(bool bReussie, const FString& Raison);
	void Nettoyer();

	AActor* Invasion() const;
	AActor* Tramway() const;
	void BrancherInvasion();
	bool VagueEnCours() const;
	bool LancerVague(int32 Nombre, const FVector& Centre, float RayonMin, float RayonMax, const FString& MessageVague);
	void TerminerNotreVague();
	/** Zombies debout de ChellesZombies. */
	void ZombiesDebout(TArray<AActor*>& Resultat) const;
	void JoueursDebout(TArray<APawn*>& Resultat) const;
	bool ModeSurvieActif() const;

	/** Points du tramway (avant, milieu, arriere) d'apres son trajet et sa position. */
	bool PositionTramway(TArray<FVector>& Points);
	FVector PointSurTrajet(float S) const;
	FString NomArretTramway(int32 Index) const;

	/** Nombre de zombies a moins de Rayon d'un des points (en 2D, et pas trop haut ni trop bas). */
	int32 ZombiesPres(const TArray<FVector>& Points, float Rayon) const;
	bool JoueurPres(const FVector& Point, float Rayon) const;

	// Etat serveur
	float FinPhase = 0.f;
	float ProchaineVague = 0.f;
	int32 NumeroVague = 0;
	bool bNotreVague = false;
	bool bPauseAutoAvant = false;
	bool bPauseAutoPosee = false;
	int32 EtatTramAvant = -1;
	float HorsDePortee = 0.f;
	float SansJoueur = 0.f;
	float ProchainControle = 0.f;
	TWeakObjectPtr<AActor> InvasionBranchee;

	bool bMairiePlacee = false;
	FVector CentreMairie = FVector::ZeroVector;

	TArray<FVector> TrajetTram;
	TArray<float> CumulsTram;

	// ---- Chaque machine ----
	void TickLocal(float DeltaSeconds);
	void CreerEcran();
	void RetirerEcran();
	void ChoisirDansMenu(uint8 Quelle);
	void MettreAJourBalise(float DeltaSeconds);

	TSharedPtr<SMissionsEcran> Ecran;
	TSharedPtr<SMissionsMenu> Menu;
	FString Message;
	float AgeMessage = 100.f;
	uint8 PhaseVueAvant = EPhaseMission::Aucune;
	float AgeFinLocale = 100.f;

	UPROPERTY(Transient)
	UStaticMeshComponent* Balise = nullptr;

	UPROPERTY(Transient)
	UPointLightComponent* LumiereBalise = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MatBalise = nullptr;

	float PhaseBalise = 0.f;
};
