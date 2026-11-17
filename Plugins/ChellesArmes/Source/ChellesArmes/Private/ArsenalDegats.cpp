// CHELLES 3D - Arsenal : degats, explosions et poussee (serveur)
#include "ArsenalDegats.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

namespace ChellesArsenalDegats
{
	bool PvpActif(UWorld* Monde)
	{
		if (!Monde || Monde->GetNetMode() == NM_Standalone)
		{
			return true;
		}
		bool bPvp = true; // celui qui heberge decide (menu > PVP)
		if (GConfig)
		{
			GConfig->GetBool(TEXT("ChellesMenu"), TEXT("Pvp"), bPvp, GGameUserSettingsIni);
		}
		return bPvp;
	}

	bool EstZombie(const AActor* Acteur)
	{
		static const FName Marque(TEXT("ChellesZombie"));
		return Acteur && Acteur->Tags.Contains(Marque);
	}

	bool PeutBlesser(const AActor* Cible, const APawn* Lanceur, bool bToucherJoueurs)
	{
		if (!Cible || Cible->IsPendingKill() || Cible == Lanceur)
		{
			return false;
		}
		if (Lanceur && Cible->GetOwner() == Lanceur)
		{
			return false; // l'arme du tireur...
		}
		static const FName Appat(TEXT("ChellesAppat"));
		if (Cible->Tags.Contains(Appat))
		{
			return false;
		}
		const APawn* Pion = Cast<APawn>(Cible);
		if (Pion && Pion->IsPlayerControlled() && !bToucherJoueurs)
		{
			return false;
		}
		return true;
	}

	static AController* Auteur(APawn* Lanceur)
	{
		return Lanceur ? Lanceur->GetController() : nullptr;
	}

	void Blesser(AActor* Cible, float Quantite, const FHitResult& Impact, const FVector& Direction, APawn* Lanceur, AActor* Cause)
	{
		if (!Cible || Quantite <= 0.f)
		{
			return;
		}
		FHitResult Precis = Impact;
		if (ACharacter* Perso = Cast<ACharacter>(Cible))
		{
			if (USkeletalMeshComponent* Corps = Perso->GetMesh())
			{
				FHitResult Os;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(ArsenalChelles), false);
				const FVector Depart = Impact.ImpactPoint - Direction * 80.f;
				const FVector Arrivee = Impact.ImpactPoint + Direction * 160.f;
				if (Corps->LineTraceComponent(Os, Depart, Arrivee, Params))
				{
					Precis = Os;
					Precis.Actor = Cible;
				}
			}
		}
		UGameplayStatics::ApplyPointDamage(Cible, Quantite, Direction, Precis, Auteur(Lanceur), Cause, UDamageType::StaticClass());
	}

	void Trancher(AActor* Cible, float Quantite, const FVector& Point, const FVector& Direction, APawn* Lanceur, AActor* Cause)
	{
		if (!Cible)
		{
			return;
		}
		FHitResult Faux(Cible, nullptr, Point, -Direction);
		Faux.ImpactPoint = Point;
		Faux.Location = Point;
		if (EstZombie(Cible))
		{
			// Le plugin Zombies tue un zombie touche a la tete
			Faux.BoneName = TEXT("head");
			if (const ACharacter* Perso = Cast<ACharacter>(Cible))
			{
				Faux.Component = Perso->GetMesh();
			}
			UGameplayStatics::ApplyPointDamage(Cible, FMath::Max(Quantite, 1.f), Direction, Faux, Auteur(Lanceur), Cause, UDamageType::StaticClass());
			return;
		}
		Blesser(Cible, Quantite, Faux, Direction, Lanceur, Cause);
	}

	void Explosion(UWorld* Monde, const FVector& Point, float Rayon, float Degats, float Poussee, APawn* Lanceur, AActor* Cause, bool bToucherJoueurs)
	{
		if (!Monde || Rayon <= 0.f)
		{
			return;
		}

		// 1) Personnages et PNJ
		TArray<APawn*> Touches;
		for (TActorIterator<APawn> It(Monde); It; ++It)
		{
			APawn* P = *It;
			if (!PeutBlesser(P, Lanceur, bToucherJoueurs))
			{
				continue;
			}
			if (FVector::Dist(P->GetActorLocation(), Point) <= Rayon)
			{
				Touches.Add(P);
			}
		}
		for (APawn* P : Touches)
		{
			if (!IsValid(P))
			{
				continue;
			}
			const FVector Vers = P->GetActorLocation() - Point;
			const float Distance = Vers.Size();
			const float Force = FMath::Clamp(1.f - Distance / Rayon, 0.25f, 1.f);
			FVector Direction = Vers.GetSafeNormal2D();
			if (Direction.IsNearlyZero())
			{
				Direction = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
			}

			// Projete en l'air, loin de l'explosion
			if (ACharacter* Perso = Cast<ACharacter>(P))
			{
				if (Poussee > 0.f)
				{
					Perso->LaunchCharacter(Direction * Poussee * Force + FVector(0.f, 0.f, Poussee * 0.6f * Force), true, true);
				}
			}

			if (EstZombie(P) && Distance <= Rayon * 0.6f)
			{
				Trancher(P, Degats * Force, P->GetActorLocation(), Direction, Lanceur, Cause);
			}
			else
			{
				FHitResult Faux(P, nullptr, P->GetActorLocation(), -Direction);
				Faux.ImpactPoint = P->GetActorLocation();
				Blesser(P, Degats * Force, Faux, Direction, Lanceur, Cause);
			}
		}

		// 2) Objets physiques (poubelles, cartons, voitures en physique...)
		TArray<FOverlapResult> Objets;
		FCollisionObjectQueryParams Types;
		Types.AddObjectTypesToQuery(ECC_PhysicsBody);
		Types.AddObjectTypesToQuery(ECC_WorldDynamic);
		Types.AddObjectTypesToQuery(ECC_Vehicle);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArsenalExplosion), false, Lanceur);
		Monde->OverlapMultiByObjectType(Objets, Point, FQuat::Identity, Types, FCollisionShape::MakeSphere(Rayon), Params);
		TSet<UPrimitiveComponent*> Vus;
		for (const FOverlapResult& O : Objets)
		{
			UPrimitiveComponent* C = O.GetComponent();
			if (C && !Vus.Contains(C) && C->IsSimulatingPhysics())
			{
				Vus.Add(C);
				C->AddRadialImpulse(Point, Rayon, Poussee * 1.2f, ERadialImpulseFalloff::RIF_Linear, true);
			}
		}
	}
}
