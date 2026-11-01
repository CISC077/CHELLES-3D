#include "PassantCharacter.h"
#include "PassantAIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APassantCharacter::APassantCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Le passant est pilote par sa propre petite IA, jamais par le joueur.
	AIControllerClass = APassantAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	// Il tourne dans le sens de sa marche, comme un vrai pieton.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Mouvement = GetCharacterMovement();
	Mouvement->bOrientRotationToMovement = true;
	Mouvement->RotationRate = FRotator(0.f, 240.f, 0.f);
	Mouvement->MaxWalkSpeed = 130.f;
	Mouvement->MaxAcceleration = 600.f;
	Mouvement->BrakingDecelerationWalking = 600.f;
	Mouvement->bUseRVOAvoidance = true;          // les passants s'evitent entre eux
	Mouvement->AvoidanceConsiderationRadius = 250.f;
	Mouvement->bCanWalkOffLedges = false;        // ne saute pas des trottoirs hauts / toits
	Mouvement->NavAgentProps.bCanJump = false;

	// Placement standard d'un personnage Unreal dans sa capsule.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -88.f), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Silhouette de secours (corps + tete) si aucun personnage n'est trouve.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylindre(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	SilhouetteCorps = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SilhouetteCorps"));
	SilhouetteCorps->SetupAttachment(GetCapsuleComponent());
	SilhouetteCorps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SilhouetteCorps->SetRelativeLocation(FVector(0.f, 0.f, -13.f));
	SilhouetteCorps->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.5f));
	if (Cylindre.Succeeded()) { SilhouetteCorps->SetStaticMesh(Cylindre.Object); }

	SilhouetteTete = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SilhouetteTete"));
	SilhouetteTete->SetupAttachment(GetCapsuleComponent());
	SilhouetteTete->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SilhouetteTete->SetRelativeLocation(FVector(0.f, 0.f, 75.f));
	SilhouetteTete->SetRelativeScale3D(FVector(0.25f));
	if (Sphere.Succeeded()) { SilhouetteTete->SetStaticMesh(Sphere.Object); }
}

void APassantCharacter::Configurer(USkeletalMesh* Apparence, TSubclassOf<UAnimInstance> AnimBlueprint,
	UAnimSequence* Marche, UAnimSequence* Repos,
	float Vitesse, const FVector& Centre, float Rayon, float InPauseMin, float InPauseMax)
{
	CentreZone = Centre;
	RayonZone = Rayon;
	PauseMin = InPauseMin;
	PauseMax = FMath::Max(InPauseMin, InPauseMax);
	GetCharacterMovement()->MaxWalkSpeed = Vitesse;

	if (Apparence)
	{
		GetMesh()->SetSkeletalMesh(Apparence);
		SilhouetteCorps->SetVisibility(false);
		SilhouetteTete->SetVisibility(false);

		if (AnimBlueprint)
		{
			// Animation Blueprint fourni : c'est lui qui gere marche / repos.
			GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			GetMesh()->SetAnimInstanceClass(AnimBlueprint);
		}
		else
		{
			AnimMarche = Marche;
			AnimRepos = Repos;
			EtatAnim = 0;
		}
	}
	else
	{
		GetMesh()->SetVisibility(false);
		ColorerVetements(SilhouetteCorps);
	}
}

void APassantCharacter::ColorerVetements(UMeshComponent* Composant)
{
	// Couleur de "vetement" au hasard pour la silhouette de secours.
	static const FLinearColor Couleurs[] = {
		FLinearColor(0.05f, 0.08f, 0.25f), FLinearColor(0.35f, 0.05f, 0.05f), FLinearColor(0.15f, 0.15f, 0.15f),
		FLinearColor(0.05f, 0.25f, 0.10f), FLinearColor(0.45f, 0.35f, 0.20f), FLinearColor(0.60f, 0.60f, 0.62f),
		FLinearColor(0.30f, 0.10f, 0.30f), FLinearColor(0.70f, 0.45f, 0.05f)
	};
	if (UMaterialInstanceDynamic* Mat = Composant->CreateAndSetMaterialInstanceDynamic(0))
	{
		Mat->SetVectorParameterValue(TEXT("Color"), Couleurs[FMath::RandRange(0, UE_ARRAY_COUNT(Couleurs) - 1)]);
	}
}

void APassantCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Sans Animation Blueprint : on alterne nous-memes entre marche et repos.
	if (!AnimMarche && !AnimRepos)
	{
		return;
	}

	const float Vitesse = GetVelocity().Size2D();
	const int32 NouvelEtat = (Vitesse > 15.f && AnimMarche) ? 2 : (AnimRepos ? 1 : 2);

	if (NouvelEtat != EtatAnim)
	{
		EtatAnim = NouvelEtat;
		GetMesh()->PlayAnimation(EtatAnim == 2 ? AnimMarche : AnimRepos, true);
	}

	if (EtatAnim == 2)
	{
		// Les pieds suivent la vitesse reelle (pas d'effet "patinage").
		GetMesh()->SetPlayRate(FMath::Clamp(Vitesse / VitesseReferenceMarche, 0.f, 1.6f));
	}
	else
	{
		GetMesh()->SetPlayRate(1.f);
	}
}
