// CHELLES 3D - Gestionnaire des agents de police (un par machine, cree au lancement du jeu).
// Il lit le reseau routier du plugin Chelles Circulation (par reflexion : aucun lien de compilation),
// trouve les ronds-points et les grands carrefours, y pose un agent pres du joueur et lui dit
// vers quelle voiture se tourner et quel geste faire.
// Ne modifie aucun niveau, aucun Blueprint et aucun autre plugin.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AgentPolicier.h"
#include "PolicierChelles.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UAnimSequenceBase;

/** Endroit surveille par un agent : rond-point ou grand carrefour. */
USTRUCT()
struct FPostePolicier
{
	GENERATED_BODY()

	/** Centre du carrefour ou du rond-point. */
	UPROPERTY() FVector Centre = FVector::ZeroVector;
	/** Ou se tient l'agent (au centre du rond-point, au coin du trottoir pour un carrefour). */
	UPROPERTY() FVector Position = FVector::ZeroVector;
	/** Rayon de la zone surveillee (cm). */
	UPROPERTY() float Rayon = 1500.f;
	UPROPERTY() bool bRondPoint = false;
	UPROPERTY() int32 Branches = 0;
	UPROPERTY() AAgentPolicier* Agent = nullptr;

	float ProchainChangement = 0.f;
};

UCLASS(NotPlaceable, Transient, meta = (DisplayName = "Policiers (Chelles)"))
class CHELLESPOLICIER_API APolicierChelles : public AActor
{
	GENERATED_BODY()

public:
	APolicierChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Policier [Info|Liste|Montrer|Relire]. */
	void Commande(const TArray<FString>& Arguments);

protected:
	// ---- Reglages (Policier.txt)
	bool bActif = true;
	FString CheminModele = TEXT("/Game/Personnages/POLICES");
	float Taille = 180.f;
	bool bRondsPoints = true;
	bool bGrandsCarrefours = true;
	int32 BranchesMinimum = 4;
	int32 BranchesPrincipales = 1;
	float VitesseRuePrincipaleKmh = 45.f;
	float EcartCarrefour = 750.f;
	float DistanceApparition = 25000.f;
	int32 AgentsMaximum = 12;
	FString DossierAnimations = TEXT("/Game/Personnages/Mondes/Policier_Animations");
	FString CheminAnimRepos;
	FString CheminAnimPassez;
	FString CheminAnimStop;
	bool bRotationAuto = true;
	float CorrectionRotation = 0.f;
	FRotator Redressement = FRotator::ZeroRotator;

	UPROPERTY(Transient) USkeletalMesh* Modele = nullptr;
	/** Modele fige (Static Mesh) si le modele n'a pas de squelette. */
	UPROPERTY(Transient) UStaticMesh* ModeleFixe = nullptr;
	UPROPERTY(Transient) UAnimSequenceBase* AnimRepos = nullptr;
	UPROPERTY(Transient) UAnimSequenceBase* AnimPassez = nullptr;
	UPROPERTY(Transient) UAnimSequenceBase* AnimStop = nullptr;

	/** Le Circulation Spawner du niveau (plugin ChellesCirculation). */
	UPROPERTY(Transient) AActor* Circulation = nullptr;

	UPROPERTY(Transient) TArray<FPostePolicier> Postes;

	bool bPostesCalcules = false;
	float PremierReseauVu = -1.f;
	float ProchaineRecherche = 0.f;
	float ProchaineApparition = 0.f;
	float ProchainOrdre = 0.f;
	float DerniereLectureVoitures = -1.f;
	int32 NombreRondsPoints = 0;
	int32 NombreCarrefours = 0;
	FString DernierMessage;

	/** Voitures de la circulation : position et vitesse estimee. */
	struct FVoitureVue
	{
		FVector Position = FVector::ZeroVector;
		FVector Vitesse = FVector::ZeroVector;
	};
	TArray<FVoitureVue> Voitures;
	TMap<TWeakObjectPtr<AActor>, FVector> PositionsPrecedentes;

	void LireReglages();
	void ChargerModeleEtAnimations();
	AActor* TrouverCirculation() const;
	bool CalculerPostes();
	bool TrouverSol(const FVector& Point, float& OutZ) const;
	bool PlaceLibre(const FVector& Centre, const FVector& Point) const;
	void MettreAJourAgents(const FVector& PositionJoueur);
	void LireVoitures(float Maintenant);
	void DonnerLesOrdres(float Maintenant);
	bool PositionJoueur(FVector& OutPosition) const;
	void EcrireRapport() const;
	void Annoncer(const FString& Texte, const FColor& Couleur);
	void RetirerAgents();
	FString DescriptionMode() const;
};
