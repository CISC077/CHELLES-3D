#include "PassantAIController.h"
#include "PassantCharacter.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

APassantAIController::APassantAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bAttachToPawn = true;
}

void APassantAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// Petit decalage au hasard pour que tout le monde ne parte pas en meme temps.
	GetWorldTimerManager().SetTimer(MinuteurPause, this, &APassantAIController::ChoisirDestination,
		FMath::FRandRange(0.2f, 3.f), false);
}

void APassantAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(MinuteurPause);
	Super::OnUnPossess();
}

void APassantAIController::FairePause()
{
	bEnMarcheLibre = false;
	float PauseMin = 2.f, PauseMax = 8.f;
	if (const APassantCharacter* Passant = Cast<APassantCharacter>(GetPawn()))
	{
		PauseMin = Passant->PauseMin;
		PauseMax = Passant->PauseMax;
	}
	GetWorldTimerManager().SetTimer(MinuteurPause, this, &APassantAIController::ChoisirDestination,
		FMath::Max(0.1f, FMath::FRandRange(PauseMin, PauseMax)), false);
}

void APassantAIController::ChoisirDestination()
{
	const APassantCharacter* Passant = Cast<APassantCharacter>(GetPawn());
	if (!Passant)
	{
		return;
	}

	const FVector Position = Passant->GetActorLocation();
	const FVector Centre = Passant->CentreZone;
	const float Rayon = Passant->RayonZone;

	if (!bModeLibre)
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			// Plusieurs essais pour trouver un point sur les trottoirs/rues, dans la zone.
			bool bNavMeshPresent = false;
			FNavLocation Depart;
			const bool bSurNavMesh = Nav->ProjectPointToNavigation(Position, Depart, FVector(200.f, 200.f, 300.f));
			for (int32 Essai = 0; Essai < 8; ++Essai)
			{
				FNavLocation Point;
				if (!bSurNavMesh || !Nav->GetRandomReachablePointInRadius(Depart.Location, LongueurTrajetMax, Point))
				{
					break; // pas de NavMesh sous ce passant
				}
				bNavMeshPresent = true;
				if (FVector::Dist2D(Point.Location, Centre) <= Rayon && FVector::Dist2D(Point.Location, Position) > 400.f)
				{
					if (MoveToLocation(Point.Location, 80.f, true, true, true, false) == EPathFollowingRequestResult::RequestSuccessful)
					{
						return;
					}
				}
			}
			// Sorti de la zone (ou aucun bon point) : on revient vers le centre.
			if (bNavMeshPresent && MoveToLocation(Centre, 300.f, true, true, true, false) == EPathFollowingRequestResult::RequestSuccessful)
			{
				return;
			}
			if (bNavMeshPresent)
			{
				FairePause(); // reessaiera plus tard
				return;
			}
		}
		// Aucun chemin possible : on passe en mode de secours (sans NavMesh).
		bModeLibre = true;
	}

	// Mode de secours : un point au hasard dans la zone, marche en ligne droite.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Distance = FMath::FRandRange(500.f, LongueurTrajetMax);
	FVector Cible = Position + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
	if (FVector::Dist2D(Cible, Centre) > Rayon)
	{
		Cible = FVector(Centre.X, Centre.Y, Position.Z);
	}
	CibleLibre = Cible;
	DernierePosition = Position;
	TempsBloque = 0.f;
	bEnMarcheLibre = true;
}

void APassantAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	FairePause();
}

void APassantAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bModeLibre || !bEnMarcheLibre)
	{
		return;
	}
	APawn* Passant = GetPawn();
	if (!Passant)
	{
		return;
	}

	const FVector Position = Passant->GetActorLocation();
	FVector Direction = CibleLibre - Position;
	Direction.Z = 0.f;

	if (Direction.Size() < 100.f)
	{
		FairePause(); // arrive
		return;
	}

	Passant->AddMovementInput(Direction.GetSafeNormal(), 1.f);

	// Bloque contre un mur ? On verifie toutes les demi-secondes, puis on change de direction.
	TempsBloque += DeltaSeconds;
	if (TempsBloque >= 0.5f)
	{
		const bool bBloque = FVector::Dist2D(Position, DernierePosition) < 15.f;
		TempsBloque = 0.f;
		DernierePosition = Position;
		if (bBloque)
		{
			FairePause();
		}
	}
}
