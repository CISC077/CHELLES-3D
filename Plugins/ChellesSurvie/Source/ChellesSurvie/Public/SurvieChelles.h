// CHELLES 3D - Modes de survie : le gestionnaire de partie (ajoute tout seul au lancement du jeu)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvieCalculs.h"
#include "SurvieRelais.h"
#include "SurvieChelles.generated.h"

class AController;
class ACharacter;
class APawn;
class APlayerController;
class ASurvieRelais;
class AStandRefuge;
class SSurvieEcran;
class SModesSurvie;

UENUM()
enum class EModeSurvie : uint8
{
	Libre,       // balade + invasion toutes les 2 h (comme avant)
	Refuge,      // DERNIER REFUGE : vagues sans fin
	Rivalite,    // a venir
	Traversee,   // a venir
	Contagion    // a venir
};

UENUM()
enum class EEtatPartie : uint8
{
	Inactif,
	Pause,       // entre deux vagues (ou preparation)
	Vague,
	Fin          // ecran des scores
};

/** Une place enregistree avec Chelles.StandIci (Refuge_Positions.txt). */
struct FPlaceStand
{
	uint8 Type = 0; // ETypeStand
	FString Article; // nom de l'arme (Arme) ou prix (Barriere)
	FVector Position = FVector::ZeroVector;
	float Lacet = 0.f;
};

/**
 * Un seul par niveau. Le serveur (ou le jeu solo) le cree, il arrive chez les autres joueurs par le reseau.
 * - Serveur : lance et arrete les modes, enchaine les vagues (via le plugin ChellesZombies, par le nom de
 *   ses fonctions : pas besoin de compiler les deux plugins ensemble), compte les points, gere "a terre".
 * - Chaque machine : ecran MODES (F7), affichage de la vague / des points, touche E (acheter, relever).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESSURVIE_API ASurvieChelles : public AActor
{
	GENERATED_BODY()

public:
	ASurvieChelles();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool EstServeur() const;
	float Maintenant() const;

	// ---- Serveur ----
	void Demarrer(EModeSurvie NouveauMode, int32 VagueDepart);
	void Arreter();
	void Acheter(ASurvieRelais* Acheteur, AStandRefuge* Stand);
	void Relever(ASurvieRelais* Releveur, ASurvieRelais* Cible, bool bAppuie);
	void Avis(ASurvieRelais* Destinataire, const FString& Texte);
	void AvisATous(const FString& Texte);

	// ---- Console (touche ²) ----
	void CommandeRefugeIci();
	void CommandeStandIci(const TArray<FString>& Arguments);
	void CommandeStandRetirer();
	void CommandePoints(int32 Quantite);
	void CommandeVague(int32 Numero);
	void OuvrirModes();

	// ---- Etat connu de tous ----
	UPROPERTY(Replicated)
	EModeSurvie Mode = EModeSurvie::Libre;

	UPROPERTY(Replicated)
	EEtatPartie Etat = EEtatPartie::Inactif;

	UPROPERTY(Replicated)
	int32 Vague = 0;

	/** Heure (horloge du serveur) de fin de la pause ou de l'ecran des scores. */
	UPROPERTY(Replicated)
	float FinPause = 0.f;

	UPROPERTY(Replicated)
	int32 ZombiesRestants = 0;

	UPROPERTY(Replicated)
	bool bVagueCoureurs = false;

	/** Message de fin ("LE REFUGE EST TOMBÉ"...). */
	UPROPERTY(Replicated)
	FString TitreFin;

	/** Liste des relais (pour l'ecran des scores). */
	void Relais(TArray<ASurvieRelais*>& Resultat) const;
	ASurvieRelais* MonRelais() const;
	ASurvieRelais* RelaisDe(AController* Controleur) const;

	// ---- Abonnements aux evenements de ChellesZombies (par leur nom) ----
	UFUNCTION()
	void QuandZombieTue(ACharacter* Zombie, AController* Tueur, bool bTete);
	UFUNCTION()
	void QuandZombieTouche(ACharacter* Zombie, AController* Auteur);
	UFUNCTION()
	void QuandCoupSurJoueur(APawn* Victime, float Degats);

	/** Message court affiche sur l'ecran de cette machine. */
	void MessageLocal(const FString& Texte);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// Serveur
	void ChargerReglages();
	void ChargerPositions();
	void SauverPositions() const;
	void GererRelais();
	void TickServeur(float Heure);
	void LancerVagueSuivante();
	void PoserStands();
	void RetirerStands();
	void PlacerJoueursAuRefuge(bool bSeulementLesMorts);
	void MettreATerre(ASurvieRelais* R);
	void Remettre(ASurvieRelais* R, bool bReapparaitre);
	void TuerJoueur(ASurvieRelais* R);
	void VerifierFin();
	void DonnerPoints(AController* Controleur, int32 Points);
	AActor* Invasion();
	void BrancherInvasion();
	int32 NombreJoueurs() const;
	bool TrouverSol(const FVector& Autour, float Rayon, FVector& Sol) const;
	FVector CentreParDefaut() const;

	// Chaque machine
	void TickLocal(float DeltaSeconds);
	void CreerEcran();
	void RetirerEcran();
	void FermerModes();
	void ChoisirMode(uint8 Quel, int32 VagueDepart);
	AStandRefuge* StandProche(APawn* Pion) const;
	ASurvieRelais* CoequipierATerreProche(APawn* Pion) const;
	void AppliquerEtatLocal(ASurvieRelais* R);

	// Reglages (serveur)
	FCourbeVagues Courbe;
	FEconomieRefuge Economie;
	FCatalogueRefuge Catalogue;
	float DureeATerre = 30.f;
	float DureeRelance = 3.f;
	float RayonApparitionMin = 1500.f;
	float RayonApparitionMax = 3500.f;
	float VolumeSireneVague = 0.6f;

	// Positions (serveur)
	bool bCentreConnu = false;
	FVector Centre = FVector::ZeroVector;
	TArray<FPlaceStand> Places;

	UPROPERTY(Transient)
	TArray<AStandRefuge*> Stands;

	UPROPERTY(Transient)
	TArray<ASurvieRelais*> ListeRelais;

	TWeakObjectPtr<AActor> InvasionBranchee;
	float ProchaineGestion = 0.f;
	float DerniereHeureServeur = -1.f;
	float VagueLanceeA = -1.f;
	bool bVagueDemarree = false;

	// Chaque machine
	TSharedPtr<SSurvieEcran> Ecran;
	TSharedPtr<SModesSurvie> EcranModes;
	FString Message;
	float AgeMessage = 100.f;
	bool bModesProposes = false;
	float DepuisJeuLance = 0.f;
	bool bEToucheAvant = false;
	bool bF7Avant = false;
	TWeakObjectPtr<ASurvieRelais> CibleReleveeLocale;
	EEtatJoueurSurvie DernierEtatLocal = EEtatJoueurSurvie::Debout;
	float ProchaineDemandeAchat = 0.f;
};
