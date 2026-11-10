// Petite IA de promenade : choisir un endroit, y marcher, faire une pause, recommencer.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "PassantAIController.generated.h"

UCLASS()
class CHELLESPASSANTS_API APassantAIController : public AAIController
{
	GENERATED_BODY()

public:
	APassantAIController();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** Choisit une nouvelle destination et commence a marcher. */
	void ChoisirDestination();

	/** Mode "trottoirs seulement" : choisit un point plus loin sur le meme trottoir, en ligne droite. */
	void ChoisirDestinationTrottoir();

	/** Essaie de trouver un trajet sur le NavMesh. Renvoie false s'il n'y a pas de NavMesh utilisable ici. */
	bool EssayerTrajetNavMesh(const FVector& Position, const FVector& Centre, float Rayon, bool& bNavMeshPresent);

	/** S'arrete un moment (regarder une vitrine...) puis repart. */
	void FairePause();

	/** Active le Tick seulement quand il sert (marche en ligne droite) : economise du temps CPU. */
	void PasserEnMarcheLibre(bool bActive);

	/** Mode de secours quand le niveau n'a pas de NavMesh : marche en ligne droite. */
	bool bModeLibre = false;
	bool bEnMarcheLibre = false;
	FVector CibleLibre = FVector::ZeroVector;
	FVector DernierePosition = FVector::ZeroVector;
	float TempsBloque = 0.f;

	/** En mode trottoir : vrai si le passant revient vers un trottoir (il a ete pousse hors du trottoir). */
	bool bRetourVersTrottoir = false;

	/** Echecs NavMesh consecutifs (le NavMesh peut encore etre en construction au lancement). */
	int32 EchecsNavMesh = 0;

	/** En mode de secours, nombre de trajets avant de reessayer le NavMesh. */
	int32 TrajetsAvantNouvelEssai = 0;

	/** Distance max d'un trajet (cm). */
	float LongueurTrajetMax = 3000.f;

	FTimerHandle MinuteurPause;
};
