// CHELLES 3D - Conducteur ejecte qui s'enfuit (voir FuyardChelles.h).
#include "FuyardChelles.h"

#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

AFuyardChelles::AFuyardChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	Corps = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Corps"));
	Corps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Corps->SetGenerateOverlapEvents(false);
	Corps->bCastDynamicShadow = true;
	Corps->SetCanEverAffectNavigation(false);
	Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	RootComponent = Corps;
}

void AFuyardChelles::Lancer(USkeletalMesh* Mesh, UAnimationAsset* Course)
{
	if (!Mesh)
	{
		Destroy();
		return;
	}
	Corps->SetSkeletalMesh(Mesh);
	// Les personnages UE4 regardent vers +Y : on les tourne pour qu'ils courent devant eux.
	Corps->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	if (Course)
	{
		Corps->PlayAnimation(Course, true);
	}
	SetLifeSpan(5.f);
}

void AFuyardChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FVector Avant = GetActorForwardVector();
	FVector Lieu = GetActorLocation() + Avant * Vitesse * DeltaSeconds;
	// Reste colle au sol (un seul rayon par image, 5 secondes au plus)
	FHitResult Sol;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FuyardSol), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Sol, Lieu + FVector(0.f, 0.f, 120.f), Lieu - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		Lieu.Z = Sol.ImpactPoint.Z;
	}
	SetActorLocation(Lieu);
}
