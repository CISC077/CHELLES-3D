// CHELLES 3D - Protection du tireur (27/11/2026, agent Armements) - UE 4.27
#include "ProtectionTireur.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	/** Le personnage qui a tire : l'instigateur, sinon le proprietaire (arme -> personnage). */
	APawn* Tireur(AActor* Projectile)
	{
		if (APawn* P = Projectile->GetInstigator())
		{
			return P;
		}
		AActor* A = Projectile->GetOwner();
		for (int32 i = 0; A && i < 4; ++i)
		{
			if (APawn* P = Cast<APawn>(A))
			{
				return P;
			}
			if (AController* C = Cast<AController>(A))
			{
				return C->GetPawn();
			}
			A = A->GetOwner();
		}
		return nullptr;
	}
}

AProtectionTireur::AProtectionTireur()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	SetReplicates(false);
}

void AProtectionTireur::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* W = GetWorld())
	{
		Abonnement = W->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AProtectionTireur::ActeurApparu));
	}
	UE_LOG(LogTemp, Log, TEXT("[Protection] Pret : les projectiles ignorent leur tireur."));
}

void AProtectionTireur::EndPlay(const EEndPlayReason::Type Raison)
{
	if (UWorld* W = GetWorld())
	{
		W->RemoveOnActorSpawnedHandler(Abonnement);
	}
	Super::EndPlay(Raison);
}

void AProtectionTireur::ActeurApparu(AActor* Acteur)
{
	if (!Acteur || !Acteur->GetClass()->GetName().Contains(TEXT("Projectile")))
	{
		return;
	}
	APawn* Pion = Tireur(Acteur);
	if (!Pion)
	{
		return;
	}
	TArray<AActor*> AIgnorer;
	AIgnorer.Add(Pion);
	Pion->GetAttachedActors(AIgnorer, false); // armes, baton du skin...
	TArray<UPrimitiveComponent*> Formes;
	Acteur->GetComponents(Formes);
	for (UPrimitiveComponent* F : Formes)
	{
		for (AActor* A : AIgnorer)
		{
			if (A && A != Acteur)
			{
				F->IgnoreActorWhenMoving(A, true);
			}
		}
	}
	if (++Proteges <= 3)
	{
		UE_LOG(LogTemp, Log, TEXT("[Protection] %s ignore son tireur %s."), *Acteur->GetClass()->GetName(), *Pion->GetName());
	}
}

void AProtectionTireur::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (Pion == PionSuivi.Get())
	{
		return;
	}
	if (PionSuivi.IsValid())
	{
		PionSuivi->OnTakeAnyDamage.RemoveDynamic(this, &AProtectionTireur::DegatsRecus);
	}
	PionSuivi = Pion;
	if (Pion)
	{
		Pion->OnTakeAnyDamage.AddUniqueDynamic(this, &AProtectionTireur::DegatsRecus);
	}
}

void AProtectionTireur::DegatsRecus(AActor* Touche, float Degats, const UDamageType* Type, AController* Auteur, AActor* Cause)
{
	const APawn* Coupable = Auteur ? Auteur->GetPawn() : nullptr;
	const bool bLuiMeme = Coupable && Coupable == Touche;
	UE_LOG(LogTemp, Log, TEXT("[Protection] Joueur touche : %.0f degats de %s (auteur %s)%s"), Degats,
		Cause ? *Cause->GetClass()->GetName() : TEXT("?"), Coupable ? *Coupable->GetName() : TEXT("?"),
		bLuiMeme ? TEXT("  <-- SES PROPRES DEGATS") : TEXT(""));
}
