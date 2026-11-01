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

	/** S'arrete un moment (regarder une vitrine...) puis repart. */
	void FairePause();

	/** Mode de secours quand le niveau n'a pas de NavMesh : marche en ligne droite. */
	bool bModeLibre = false;
	bool bEnMarcheLibre = false;
	FVector CibleLibre = FVector::ZeroVector;
	FVector DernierePosition = FVector::ZeroVector;
	float TempsBloque = 0.f;

	/** Distance max d'un trajet (cm). */
	float LongueurTrajetMax = 3000.f;

	FTimerHandle MinuteurPause;
};
