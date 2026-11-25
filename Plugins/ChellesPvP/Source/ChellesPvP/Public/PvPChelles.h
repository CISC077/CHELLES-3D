// CHELLES 3D - PvP : le gestionnaire des combats entre joueurs (ajoute tout seul au lancement du jeu)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "PvPRelais.h"
#include "PvPChelles.generated.h"

class AController;
class APawn;
class APlayerController;
class UDamageType;
class UPrimitiveComponent;
class SWidget;

UENUM()
enum class EEtatManchePvP : uint8
{
	Attente,   // moins de 2 joueurs : pas de manche
	EnCours,   // le PvP est ouvert
	Scores     // fin de manche : treve + tableau des scores
};

/**
 * Un seul par niveau. Le serveur (ou le jeu solo) le cree, il arrive chez les autres joueurs par le reseau.
 * - Serveur : compte les coups entre joueurs (sans laisser FPS_Game tuer), met a terre au bout de
 *   CoupsAvantATerre coups, reanime quand un kit de survie tombe a cote, acheve a la fin du delai,
 *   enchaine les manches de DureeMancheMinutes.
 * - Chaque machine : bandeau PvP (temps, coups, kits), ecran "A TERRE", touche du kit, tableau des scores.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESPVP_API APvPChelles : public AActor
{
	GENERATED_BODY()

public:
	APvPChelles();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool EstServeur() const;
	float Maintenant() const;

	// ---- Serveur ----
	void JeterKit(APvPRelais* Lanceur);
	void AvisATous(const FString& Texte);
	void NouvelleManche();
	void ChargerReglages();

	/**
	 * Pour les autres plugins (par leur nom, ex. ChellesAccessoires quand un coffre donne un kit) :
	 * ajoute des kits de survie au joueur de ce pion. Serveur seulement.
	 */
	UFUNCTION()
	void AjouterKitsSurvie(APawn* Joueur, int32 Nombre);

	/** Pour les autres plugins : ce pion est-il a terre (PvP) ? */
	UFUNCTION()
	bool EstATerre(APawn* Joueur) const;

	// ---- Console (touche ²) ----
	void CommandeEtat();
	void CommandeKits(int32 Nombre);
	void CommandeMoiATerre();

	// ---- Ecoute des degats des personnages (serveur) ----
	UFUNCTION()
	void QuandDegatsPrecis(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

	UFUNCTION()
	void QuandDegatsZone(AActor* DamagedActor, float Damage, const UDamageType* DamageType, FVector Origin, FHitResult HitInfo, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void QuandDegats(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	// ---- Etat connu de tous ----
	/** PvP permis : PvP.txt Actif=Oui, menu PVP de l'hote, et pas de mode Survie (Dernier Refuge) en cours. */
	UPROPERTY(Replicated)
	bool bActif = false;

	UPROPERTY(Replicated)
	EEtatManchePvP Manche = EEtatManchePvP::Attente;

	/** Heure (horloge du serveur) de fin de la manche ou de l'ecran des scores. 0 = manche sans limite. */
	UPROPERTY(Replicated)
	float FinManche = 0.f;

	UPROPERTY(Replicated)
	int32 NumeroManche = 0;

	UPROPERTY(Replicated)
	int32 CoupsMax = 5;

	UPROPERTY(Replicated)
	float DureeATerreRepl = 30.f;

	/** "FIN DE LA MANCHE : Rachid gagne avec 4 éliminations". */
	UPROPERTY(Replicated)
	FString TitreScores;

	/** Touche pour jeter un kit (lue dans PvP.txt par chaque machine). */
	FKey ToucheKit = EKeys::Y;

	void Relais(TArray<APvPRelais*>& Resultat) const;
	APvPRelais* MonRelais() const;
	APvPRelais* RelaisDe(const AController* Controleur) const;
	APvPRelais* RelaisDuPion(const APawn* Pion) const;

	/** Message court affiche sur l'ecran de cette machine. */
	void MessageLocal(const FString& Texte);

	// Textes du bandeau (chaque machine)
	FText TexteBandeau() const;
	FText TexteCentre() const;
	FText TexteInvite() const;
	FText TexteMessage() const;
	FText TexteScores() const;
	bool MontrerBandeau() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// Serveur
	void TickServeur(float Heure);
	void GererRelais();
	void Ecouter(APvPRelais* R, APawn* P);
	bool CalculerActif() const;
	bool EstCoupPvP(AActor* Victime, AController* Auteur, AActor* Cause, APvPRelais*& RVictime, AController*& AuteurReel) const;
	void CompterCoup(APvPRelais* R, AController* Auteur);
	void MettreATerre(APvPRelais* R);
	void Reanimer(APvPRelais* R, APvPRelais* Sauveur, bool bParKit);
	void Achever(APvPRelais* R);
	void RemettreDebout(APvPRelais* R, bool bToutOublier);
	void FinDeManche();
	void VerifierKits();
	APvPRelais* CibleDuLancer(APawn* Lanceur, const FVector& Oeil, const FVector& Direction) const;

	// Chaque machine
	void TickLocal(float DeltaSeconds);
	void CreerEcran();
	void RetirerEcran();
	void AppliquerEtatLocal(APvPRelais* R);
	APvPRelais* AmiATerreProche(APawn* Pion, float& Distance) const;
	void LireToucheLocale();

	// Reglages (serveur)
	bool bActifFichier = true;
	float DureeManche = 600.f;
	float DureeScores = 12.f;
	float DureeATerre = 30.f;
	float RecuperationCoup = 20.f;
	float PourcentVieReanimation = 50.f;
	int32 KitsDepart = 2;
	int32 KitsMax = 5;
	float RayonKit = 250.f;
	float ForceLancer = 1100.f;
	float PorteeAide = 2500.f;
	float DureeVieKit = 30.f;
	float AttenteEntreKits = 1.5f;
	bool bKitsAChaqueManche = true;
	int32 JoueursMin = 2;
	FString ModeleKit;

	UPROPERTY(Transient)
	TArray<APvPRelais*> ListeRelais;

	float ProchaineGestion = 0.f;
	float ProchaineVerifKits = 0.f;

	// Chaque machine
	TSharedPtr<SWidget> Ecran;
	FString Message;
	float AgeMessage = 100.f;
	EEtatJoueurPvP DernierEtatLocal = EEtatJoueurPvP::Debout;
	TWeakObjectPtr<APawn> DernierPionLocal;
	float ProchaineDemandeKit = 0.f;
	FString InviteLocale;
};
