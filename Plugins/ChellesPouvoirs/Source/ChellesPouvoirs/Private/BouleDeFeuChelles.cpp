// CHELLES 3D - Pouvoir BOULE DE FEU : le projectile
#include "BouleDeFeuChelles.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// Effets du Starter Content deja present dans le projet (si absents : la boule reste visible sans eux)
#define CHEMIN_FLAMMES TEXT("/Game/StarterContent/Particles/P_Fire.P_Fire")
#define CHEMIN_EXPLOSION TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion")
#define CHEMIN_SON_EXPLOSION TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue")
#define CHEMIN_SON_LANCER TEXT("/Game/StarterContent/Audio/Fire01_Cue.Fire01_Cue")

ABouleDeFeuChelles::ABouleDeFeuChelles()
{
	bReplicates = true;
	SetReplicateMovement(true);
	InitialLifeSpan = 5.f;
	PrimaryActorTick.bCanEverTick = false;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(18.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Sphere->SetGenerateOverlapEvents(false);
	Sphere->SetNotifyRigidBodyCollision(true);
	Sphere->CanCharacterStepUpOn = ECB_No;
	RootComponent = Sphere;

	Boule = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boule"));
	Boule->SetupAttachment(Sphere);
	Boule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Boule->SetCastShadow(false);
	Boule->SetRelativeScale3D(FVector(0.4f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Forme(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Forme.Succeeded())
	{
		Boule->SetStaticMesh(Forme.Object);
	}

	Lumiere = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lumiere"));
	Lumiere->SetupAttachment(Sphere);
	Lumiere->SetIntensity(9000.f);
	Lumiere->SetLightColor(FLinearColor(1.f, 0.42f, 0.08f));
	Lumiere->SetAttenuationRadius(700.f);
	Lumiere->SetCastShadows(false);

	Mouvement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Mouvement"));
	Mouvement->UpdatedComponent = Sphere;
	Mouvement->InitialSpeed = 2800.f;
	Mouvement->MaxSpeed = 2800.f;
	Mouvement->ProjectileGravityScale = 0.08f;
	Mouvement->bRotationFollowsVelocity = true;
	Mouvement->bShouldBounce = false;
}

void ABouleDeFeuChelles::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* Lanceur = GetInstigator())
	{
		Sphere->IgnoreActorWhenMoving(Lanceur, true);
		TArray<AActor*> Attaches;
		Lanceur->GetAttachedActors(Attaches);
		for (AActor* A : Attaches)
		{
			Sphere->IgnoreActorWhenMoving(A, true);
		}
	}
	Sphere->OnComponentHit.AddDynamic(this, &ABouleDeFeuChelles::Touche);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Boule orange (materiau de base d'Unreal, colore)
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Couleur = UMaterialInstanceDynamic::Create(Base, this);
		Couleur->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.35f, 0.02f));
		Boule->SetMaterial(0, Couleur);
	}
	if (UParticleSystem* Flammes = LoadObject<UParticleSystem>(nullptr, CHEMIN_FLAMMES))
	{
		UGameplayStatics::SpawnEmitterAttached(Flammes, Sphere, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			FVector(0.35f), EAttachLocation::KeepRelativeOffset, true);
	}
	if (USoundBase* Son = LoadObject<USoundBase>(nullptr, CHEMIN_SON_LANCER))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Son, GetActorLocation(), 0.6f, 1.6f);
	}
}

void ABouleDeFeuChelles::Touche(UPrimitiveComponent* MonComposant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact)
{
	if (HasAuthority())
	{
		Exploser();
	}
}

void ABouleDeFeuChelles::Exploser()
{
	if (bExplosee)
	{
		return;
	}
	bExplosee = true;

	const FVector Centre = GetActorLocation();
	TArray<AActor*> Epargnes;
	APawn* Lanceur = GetInstigator();
	if (Lanceur)
	{
		Epargnes.Add(Lanceur);
	}
	if (!bToucherJoueurs)
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			if (It->IsPlayerControlled())
			{
				Epargnes.AddUnique(*It);
			}
		}
	}
	UGameplayStatics::ApplyRadialDamage(this, Degats, Centre, Rayon, UDamageType::StaticClass(), Epargnes, this,
		Lanceur ? Lanceur->GetController() : nullptr, false, ECC_Visibility);

	// Souffle : pousse les objets physiques (poubelles, cartons, voitures...)
	TArray<FOverlapResult> Proches;
	FCollisionObjectQueryParams Types;
	Types.AddObjectTypesToQuery(ECC_PhysicsBody);
	Types.AddObjectTypesToQuery(ECC_WorldDynamic);
	Types.AddObjectTypesToQuery(ECC_Vehicle);
	GetWorld()->OverlapMultiByObjectType(Proches, Centre, FQuat::Identity, Types, FCollisionShape::MakeSphere(Rayon));
	for (const FOverlapResult& R : Proches)
	{
		UPrimitiveComponent* C = R.GetComponent();
		if (C && C->IsSimulatingPhysics())
		{
			C->AddRadialImpulse(Centre, Rayon, 900.f, ERadialImpulseFalloff::RIF_Linear, true);
		}
	}

	Destroy(); // l'explosion s'affiche chez tous dans Destroyed()
}

void ABouleDeFeuChelles::Destroyed()
{
	Super::Destroyed();

	UWorld* W = GetWorld();
	if (!W || W->bIsTearingDown || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UParticleSystem* Explosion = LoadObject<UParticleSystem>(nullptr, CHEMIN_EXPLOSION))
	{
		UGameplayStatics::SpawnEmitterAtLocation(W, Explosion, GetActorLocation(), FRotator::ZeroRotator, FVector(0.6f));
	}
	if (USoundBase* Son = LoadObject<USoundBase>(nullptr, CHEMIN_SON_EXPLOSION))
	{
		UGameplayStatics::PlaySoundAtLocation(W, Son, GetActorLocation(), 0.8f);
	}
}
