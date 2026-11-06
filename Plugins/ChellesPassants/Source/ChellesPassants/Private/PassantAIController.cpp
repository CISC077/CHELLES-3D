#include "PassantAIController.h"
#include "PassantCharacter.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	/** Nombre d'echecs NavMesh consecutifs avant de passer en marche libre. */
	constexpr int32 EchecsAvantModeLibre = 3;
	/** En marche libre, on retente le NavMesh tous les N trajets (il a peut-etre fini d'etre construit). */
	constexpr int32 TrajetsLibresEntreEssais = 4;

	/** Mode trottoir : nombre de directions essayees autour du passant. */
	constexpr int32 DirectionsTrottoir = 16;
	/** Mode trottoir : pas de verification le long d'une direction (cm). */
	constexpr float PasTrottoir = 75.f;
	/** Mode trottoir : longueur minimale d'un trajet (cm). */
	constexpr float TrajetTrottoirMin = 300.f;
	/** Mode trottoir : distance a laquelle on regarde devant soi en marchant (cm). */
	constexpr float RegardDevant = 60.f;
}

APassantAIController::APassantAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	// Le Tick ne sert qu'a la marche en ligne droite : il est coupe par defaut.
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAttachToPawn = true;
}

void APassantAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	// Petit decalage au hasard pour que tout le monde ne parte pas en meme temps
	// (laisse aussi au NavMesh le temps de se construire au lancement).
	GetWorldTimerManager().SetTimer(MinuteurPause, this, &APassantAIController::ChoisirDestination,
		FMath::FRandRange(0.5f, 3.f), false);
}

void APassantAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(MinuteurPause);
	PasserEnMarcheLibre(false);
	Super::OnUnPossess();
}

void APassantAIController::PasserEnMarcheLibre(bool bActive)
{
	bEnMarcheLibre = bActive;
	SetActorTickEnabled(bActive);
}

void APassantAIController::FairePause()
{
	PasserEnMarcheLibre(false);
	float PauseMin = 2.f, PauseMax = 8.f;
	if (const APassantCharacter* Passant = Cast<APassantCharacter>(GetPawn()))
	{
		PauseMin = Passant->PauseMin;
		PauseMax = Passant->PauseMax;
	}
	GetWorldTimerManager().SetTimer(MinuteurPause, this, &APassantAIController::ChoisirDestination,
		FMath::Max(0.1f, FMath::FRandRange(PauseMin, PauseMax)), false);
}

bool APassantAIController::EssayerTrajetNavMesh(const FVector& Position, const FVector& Centre, float Rayon, bool& bNavMeshPresent)
{
	bNavMeshPresent = false;
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		return false;
	}

	FNavLocation Depart;
	if (!Nav->ProjectPointToNavigation(Position, Depart, FVector(200.f, 200.f, 300.f)))
	{
		return false; // pas de NavMesh sous ce passant
	}
	bNavMeshPresent = true;

	// Plusieurs essais pour trouver un point sur les trottoirs/rues, dans la zone.
	for (int32 Essai = 0; Essai < 8; ++Essai)
	{
		FNavLocation Point;
		if (!Nav->GetRandomReachablePointInRadius(Depart.Location, LongueurTrajetMax, Point))
		{
			break;
		}
		if (FVector::Dist2D(Point.Location, Centre) <= Rayon && FVector::Dist2D(Point.Location, Position) > 400.f)
		{
			if (MoveToLocation(Point.Location, 80.f, true, true, true, false) == EPathFollowingRequestResult::RequestSuccessful)
			{
				return true;
			}
		}
	}
	// Sorti de la zone (ou aucun bon point) : on revient vers le centre.
	return MoveToLocation(Centre, 300.f, true, true, true, false) == EPathFollowingRequestResult::RequestSuccessful;
}

void APassantAIController::ChoisirDestinationTrottoir()
{
	const APassantCharacter* Passant = Cast<APassantCharacter>(GetPawn());
	if (!Passant)
	{
		return;
	}

	const UWorld* Monde = GetWorld();
	const FVector Position = Passant->GetActorLocation();
	const float Pieds = Passant->HauteurDesPieds();
	const FString& MotCle = Passant->MotCleTrottoir;
	const FVector Centre = Passant->CentreZone;
	const float Rayon = Passant->RayonZone;

	DernierePosition = Position;
	TempsBloque = 0.f;

	// Pousse hors du trottoir (par un autre passant) ? On cherche le trottoir le plus proche et on y retourne.
	if (!APassantCharacter::EstUnTrottoir(Monde, Position, Pieds, MotCle, Passant))
	{
		for (float Distance = 50.f; Distance <= 400.f; Distance += 50.f)
		{
			for (int32 i = 0; i < DirectionsTrottoir; ++i)
			{
				const float Angle = 2.f * PI * i / DirectionsTrottoir;
				const FVector Point = Position + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
				if (APassantCharacter::EstUnTrottoir(Monde, Point, Pieds, MotCle, Passant))
				{
					CibleLibre = Point;
					bRetourVersTrottoir = true;
					PasserEnMarcheLibre(true);
					return;
				}
			}
		}
		FairePause(); // pas de trottoir tout pres : il attend sur place
		return;
	}
	bRetourVersTrottoir = false;

	// Pour chaque direction, jusqu'ou peut-on marcher en restant sur le trottoir (et dans la zone) ?
	const float Decalage = FMath::FRandRange(0.f, 2.f * PI);
	TArray<TPair<FVector, float>> Choix; // direction, distance possible
	for (int32 i = 0; i < DirectionsTrottoir; ++i)
	{
		const float Angle = Decalage + 2.f * PI * i / DirectionsTrottoir;
		const FVector Direction(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		float Possible = 0.f;
		float HauteurSol = Pieds;
		for (float Distance = PasTrottoir; Distance <= LongueurTrajetMax; Distance += PasTrottoir)
		{
			const FVector Point = Position + Direction * Distance;
			FVector Sol;
			if (FVector::Dist2D(Point, Centre) > Rayon
				|| !APassantCharacter::EstUnTrottoir(Monde, Point, HauteurSol, MotCle, Passant, &Sol))
			{
				break;
			}
			HauteurSol = Sol.Z; // suit les pentes douces de la rue
			Possible = Distance;
		}
		if (Possible >= TrajetTrottoirMin)
		{
			Choix.Add(TPair<FVector, float>(Direction, Possible));
		}
	}

	if (Choix.Num() == 0)
	{
		FairePause(); // bout de trottoir trop court ici : il attend un peu
		return;
	}

	// Les grandes lignes droites (le long du trottoir) sont preferees aux petites traversees.
	float Total = 0.f;
	for (const TPair<FVector, float>& C : Choix) { Total += C.Value; }
	float Tirage = FMath::FRandRange(0.f, Total);
	const TPair<FVector, float>* Retenu = &Choix.Last();
	for (const TPair<FVector, float>& C : Choix)
	{
		Tirage -= C.Value;
		if (Tirage <= 0.f) { Retenu = &C; break; }
	}

	const float Longueur = FMath::Max(TrajetTrottoirMin, FMath::FRandRange(0.5f, 1.f) * Retenu->Value - 50.f);
	CibleLibre = Position + Retenu->Key * Longueur;
	PasserEnMarcheLibre(true);
}

void APassantAIController::ChoisirDestination()
{
	const APassantCharacter* Passant = Cast<APassantCharacter>(GetPawn());
	if (!Passant)
	{
		return;
	}

	if (Passant->bTrottoirsSeulement)
	{
		ChoisirDestinationTrottoir();
		return;
	}

	const FVector Position = Passant->GetActorLocation();
	const FVector Centre = Passant->CentreZone;
	const float Rayon = Passant->RayonZone;

	// En marche libre, on retente regulierement le NavMesh : avant, un NavMesh pas encore
	// construit au lancement faisait passer le passant en ligne droite pour toujours.
	const bool bEssayerNavMesh = !bModeLibre || (--TrajetsAvantNouvelEssai <= 0);
	if (bEssayerNavMesh)
	{
		bool bNavMeshPresent = false;
		if (EssayerTrajetNavMesh(Position, Centre, Rayon, bNavMeshPresent))
		{
			bModeLibre = false;
			EchecsNavMesh = 0;
			return;
		}
		if (bNavMeshPresent && !bModeLibre)
		{
			FairePause(); // NavMesh present mais aucun chemin pour l'instant : on reessaiera
			return;
		}
		if (!bModeLibre && ++EchecsNavMesh < EchecsAvantModeLibre)
		{
			FairePause();
			return;
		}
		bModeLibre = true;
		TrajetsAvantNouvelEssai = TrajetsLibresEntreEssais;
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
	PasserEnMarcheLibre(true);
}

void APassantAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	FairePause();
}

void APassantAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnMarcheLibre)
	{
		return;
	}
	APassantCharacter* Passant = Cast<APassantCharacter>(GetPawn());
	if (!Passant)
	{
		return;
	}

	const FVector Position = Passant->GetActorLocation();
	FVector Direction = CibleLibre - Position;
	Direction.Z = 0.f;

	if (Direction.SizeSquared() < FMath::Square(bRetourVersTrottoir ? 30.f : 100.f))
	{
		bRetourVersTrottoir = false;
		FairePause(); // arrive
		return;
	}
	Direction.Normalize();

	// Mode trottoir : on regarde juste devant ses pieds ; si ce n'est plus un trottoir
	// (route, herbe, jumper...), on s'arrete avant d'y mettre le pied.
	if (Passant->bTrottoirsSeulement && !bRetourVersTrottoir
		&& !APassantCharacter::EstUnTrottoir(GetWorld(), Position + Direction * RegardDevant,
			Passant->HauteurDesPieds(), Passant->MotCleTrottoir, Passant))
	{
		Passant->GetMovementComponent()->StopMovementImmediately();
		FairePause();
		return;
	}

	Passant->AddMovementInput(Direction, 1.f);

	// Bloque contre un mur ? On verifie toutes les demi-secondes, puis on change de direction.
	TempsBloque += DeltaSeconds;
	if (TempsBloque >= 0.5f)
	{
		const bool bBloque = FVector::DistSquared2D(Position, DernierePosition) < FMath::Square(15.f);
		TempsBloque = 0.f;
		DernierePosition = Position;
		if (bBloque)
		{
			bRetourVersTrottoir = false;
			FairePause();
		}
	}
}
