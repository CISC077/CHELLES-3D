// CHELLES 3D - Armes : projectile d'un tir special
#include "TirSpecialChelles.h"
#include "ArmesEffets.h"
#include "ArmesTypes.h"
#include "GivreChelles.h"

#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

using namespace ChellesArmes;

namespace
{
	FLinearColor CouleurTir(uint8 Type)
	{
		switch (Type)
		{
		case Laser:  return FLinearColor(1.f, 0.05f, 0.05f);
		case Feu:    return FLinearColor(1.f, 0.35f, 0.02f);
		case Glace:  return FLinearColor(0.55f, 0.85f, 1.f);
		case Foudre: return FLinearColor(1.f, 0.95f, 0.45f);
		default:     return FLinearColor::White;
		}
	}
}

ATirSpecialChelles::ATirSpecialChelles()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	InitialLifeSpan = 4.f;
	PrimaryActorTick.bCanEverTick = false;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(10.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	// Les balles normales de l'arme partent au meme moment : on les traverse (sinon explosion dans le canon)
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	// Canaux ajoutes par les packs (projectiles, interactions...) : ignores aussi
	for (int32 Canal = ECC_GameTraceChannel1; Canal <= ECC_GameTraceChannel18; ++Canal)
	{
		Sphere->SetCollisionResponseToChannel(ECollisionChannel(Canal), ECR_Ignore);
	}
	Sphere->SetGenerateOverlapEvents(false);
	Sphere->CanCharacterStepUpOn = ECB_No;
	RootComponent = Sphere;

	Forme = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Forme"));
	Forme->SetupAttachment(Sphere);
	Forme->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Forme->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Boule(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Boule.Succeeded())
	{
		Forme->SetStaticMesh(Boule.Object);
	}

	Lumiere = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lumiere"));
	Lumiere->SetupAttachment(Sphere);
	Lumiere->SetIntensity(6000.f);
	Lumiere->SetAttenuationRadius(500.f);
	Lumiere->SetCastShadows(false);

	Mouvement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Mouvement"));
	Mouvement->UpdatedComponent = Sphere;
	Mouvement->InitialSpeed = 6000.f;
	Mouvement->MaxSpeed = 100000.f;
	Mouvement->ProjectileGravityScale = 0.f;
	Mouvement->bRotationFollowsVelocity = true;
	Mouvement->bShouldBounce = false;
}

void ATirSpecialChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATirSpecialChelles, Type);
	DOREPLIFETIME(ATirSpecialChelles, Vitesse);
}

void ATirSpecialChelles::BeginPlay()
{
	Super::BeginPlay();

	// Meme vitesse sur toutes les machines (le reseau corrige ensuite la position)
	Mouvement->MaxSpeed = FMath::Max(Vitesse, 100.f);
	Mouvement->ProjectileGravityScale = (Type == Feu) ? 0.08f : 0.f;
	Mouvement->Velocity = GetActorForwardVector() * Vitesse;
	Mouvement->UpdateComponentVelocity();

	if (HasAuthority())
	{
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
		Sphere->OnComponentHit.AddDynamic(this, &ATirSpecialChelles::Touche);
	}
	else
	{
		// Chez les autres joueurs le projectile ne fait que suivre le serveur
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Allure selon le type
	const FLinearColor Couleur = CouleurTir(Type);
	Lumiere->SetLightColor(Couleur);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, this);
		Mat->SetVectorParameterValue(TEXT("Color"), Couleur);
		Forme->SetMaterial(0, Mat);
	}

	switch (Type)
	{
	case Laser:
		if (UStaticMesh* Cylindre = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			Forme->SetStaticMesh(Cylindre);
		}
		// Cylindre couche dans l'axe du tir : 3 cm d'epaisseur, 1,6 m de long
		Forme->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Forme->SetRelativeScale3D(FVector(0.03f, 0.03f, 1.6f));
		Lumiere->SetIntensity(9000.f);
		Lumiere->SetAttenuationRadius(400.f);
		if (USoundBase* Son = ChellesArmesEffets::SonLaser())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Son, GetActorLocation(), 0.5f, 1.3f);
		}
		break;

	case Feu:
		Forme->SetRelativeScale3D(FVector(0.32f));
		Lumiere->SetIntensity(9000.f);
		Lumiere->SetAttenuationRadius(700.f);
		if (UParticleSystem* Flammes = ChellesArmesEffets::Flammes())
		{
			UGameplayStatics::SpawnEmitterAttached(Flammes, Sphere, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				FVector(0.35f), EAttachLocation::KeepRelativeOffset, true);
		}
		break;

	case Glace:
		// Eclat de glace allonge
		Forme->SetRelativeScale3D(FVector(0.35f, 0.12f, 0.12f));
		Lumiere->SetIntensity(5000.f);
		if (USoundBase* Son = ChellesArmesEffets::SonLaser())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Son, GetActorLocation(), 0.45f, 0.55f);
		}
		break;

	case Foudre:
		Forme->SetRelativeScale3D(FVector(0.5f, 0.08f, 0.08f));
		Lumiere->SetIntensity(14000.f);
		Lumiere->SetAttenuationRadius(800.f);
		if (UParticleSystem* Eclairs = ChellesArmesEffets::Etincelles())
		{
			UGameplayStatics::SpawnEmitterAttached(Eclairs, Sphere, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				FVector(0.5f), EAttachLocation::KeepRelativeOffset, true);
		}
		if (USoundBase* Son = ChellesArmesEffets::SonLaser())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Son, GetActorLocation(), 0.5f, 2.f);
		}
		break;

	default:
		break;
	}
}

bool ATirSpecialChelles::PeutBlesser(AActor* Cible) const
{
	if (!Cible || Cible == this || Cible == GetInstigator() || Cible->IsPendingKill())
	{
		return false;
	}
	if (Cible->GetOwner() && Cible->GetOwner() == GetInstigator())
	{
		return false; // l'arme du tireur, etc.
	}
	const APawn* Pion = Cast<APawn>(Cible);
	if (Pion && Pion->IsPlayerControlled() && !bToucherJoueurs)
	{
		return false;
	}
	return true;
}

void ATirSpecialChelles::Blesser(AActor* Cible, float Quantite, const FHitResult& Impact, const FVector& Direction)
{
	if (!PeutBlesser(Cible) || Quantite <= 0.f)
	{
		return;
	}
	// On cherche l'os touche (tete, corps...) : le plugin Zombies decide tete = mort, corps = 33 %
	FHitResult Precis = Impact;
	if (ACharacter* Perso = Cast<ACharacter>(Cible))
	{
		if (USkeletalMeshComponent* Corps = Perso->GetMesh())
		{
			FHitResult Os;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TirSpecialChelles), false);
			const FVector Depart = Impact.ImpactPoint - Direction * 80.f;
			const FVector Arrivee = Impact.ImpactPoint + Direction * 160.f;
			if (Corps->LineTraceComponent(Os, Depart, Arrivee, Params))
			{
				Precis = Os;
				Precis.Actor = Cible;
			}
		}
	}
	APawn* Lanceur = GetInstigator();
	UGameplayStatics::ApplyPointDamage(Cible, Quantite, Direction, Precis, Lanceur ? Lanceur->GetController() : nullptr, this,
		UDamageType::StaticClass());
}

void ATirSpecialChelles::Geler(APawn* Cible)
{
	if (Cible && PeutBlesser(Cible))
	{
		AGivreChelles::Geler(GetWorld(), Cible, DureeGel, GetInstigator());
	}
}

void ATirSpecialChelles::Touche(UPrimitiveComponent* MonComposant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact)
{
	if (HasAuthority())
	{
		Exploser(Autre, Impact);
	}
}

void ATirSpecialChelles::Exploser(AActor* Autre, const FHitResult& Impact)
{
	if (bFini)
	{
		return;
	}
	bFini = true;

	const FVector Direction = Mouvement->Velocity.IsNearlyZero() ? GetActorForwardVector() : Mouvement->Velocity.GetSafeNormal();
	const FVector Point = Impact.bBlockingHit ? FVector(Impact.ImpactPoint) : GetActorLocation();
	TArray<FVector_NetQuantize> PointsFoudre;

	// Coup direct
	Blesser(Autre, Degats, Impact, Direction);

	// Objets physiques (poubelles, cartons...) : petite poussee
	UPrimitiveComponent* Objet = Impact.GetComponent();
	if (Objet && Objet->IsSimulatingPhysics())
	{
		Objet->AddImpulseAtLocation(Direction * (Type == Feu ? 60000.f : 15000.f), Point);
	}

	APawn* Lanceur = GetInstigator();
	switch (Type)
	{
	case Feu:
		if (DegatsZone > 0.f && Rayon > 0.f)
		{
			TArray<AActor*> Epargnes;
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
			UGameplayStatics::ApplyRadialDamage(this, DegatsZone, Point, Rayon, UDamageType::StaticClass(), Epargnes, this,
				Lanceur ? Lanceur->GetController() : nullptr, false, ECC_Visibility);
		}
		break;

	case Glace:
		Geler(Cast<APawn>(Autre));
		break;

	case Foudre:
		if (Rebonds > 0 && Rayon > 0.f)
		{
			// L'eclair saute sur les ennemis / passants les plus proches
			TArray<TPair<float, APawn*>> Voisins;
			for (TActorIterator<APawn> It(GetWorld()); It; ++It)
			{
				APawn* P = *It;
				if (P == Autre || !PeutBlesser(P))
				{
					continue;
				}
				const float D = FVector::Dist(P->GetActorLocation(), Point);
				if (D <= Rayon)
				{
					Voisins.Add(TPair<float, APawn*>(D, P));
				}
			}
			Voisins.Sort([](const TPair<float, APawn*>& A, const TPair<float, APawn*>& B) { return A.Key < B.Key; });
			for (int32 i = 0; i < Voisins.Num() && i < Rebonds; ++i)
			{
				APawn* P = Voisins[i].Value;
				const FVector Vers = (P->GetActorLocation() - Point).GetSafeNormal();
				FHitResult Faux(P, nullptr, P->GetActorLocation(), -Vers);
				Faux.ImpactPoint = P->GetActorLocation();
				Blesser(P, Degats * 0.6f, Faux, Vers);
				PointsFoudre.Add(P->GetActorLocation());
			}
		}
		break;

	default:
		break;
	}

	MulticastImpact(Point, PointsFoudre);

	// On laisse le temps a l'effet d'arriver chez tous, puis le projectile disparait
	Mouvement->StopMovementImmediately();
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	SetLifeSpan(0.3f);
}

void ATirSpecialChelles::MulticastImpact_Implementation(FVector_NetQuantize Point, const TArray<FVector_NetQuantize>& Points)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	SetActorHiddenInGame(true);
	UWorld* W = GetWorld();
	switch (Type)
	{
	case Laser:
		ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::ImpactBalle(), Point, 1.f, 0.4f);
		break;
	case Feu:
		if (UParticleSystem* Boum = ChellesArmesEffets::Explosion())
		{
			UGameplayStatics::SpawnEmitterAtLocation(W, Boum, Point, FRotator::ZeroRotator, FVector(0.6f));
		}
		if (USoundBase* Son = ChellesArmesEffets::SonExplosion())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Son, Point, 0.7f);
		}
		break;
	case Glace:
		ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Vapeur(), Point, 0.8f, 0.8f);
		break;
	case Foudre:
		ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Etincelles(), Point, 1.f, 0.5f);
		for (const FVector_NetQuantize& P : Points)
		{
			ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Etincelles(), P, 1.f, 0.5f);
		}
		break;
	default:
		break;
	}
}
