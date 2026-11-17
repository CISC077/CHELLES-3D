// CHELLES 3D - Arsenal : projectiles (blaster, bazooka, grenade)
#include "ProjectileArsenal.h"
#include "AppatChelles.h"
#include "ArmesEffets.h"
#include "ArsenalDegats.h"
#include "ArsenalRelais.h"
#include "ArsenalTypes.h"
#include "ArsenalVisuel.h"
#include "SonArsenalChelles.h"

#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"

using namespace ChellesArsenal;

AProjectileArsenal::AProjectileArsenal()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	InitialLifeSpan = 40.f; // securite : le serveur detruit le projectile bien avant
	PrimaryActorTick.bCanEverTick = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(8.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	// Canaux ajoutes par les packs (projectiles, interactions...) : ignores
	for (int32 Canal = ECC_GameTraceChannel1; Canal <= ECC_GameTraceChannel18; ++Canal)
	{
		Sphere->SetCollisionResponseToChannel(ECollisionChannel(Canal), ECR_Ignore);
	}
	Sphere->SetGenerateOverlapEvents(false);
	Sphere->CanCharacterStepUpOn = ECB_No;
	RootComponent = Sphere;

	Lumiere = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lumiere"));
	Lumiere->SetupAttachment(Sphere);
	Lumiere->SetIntensity(5000.f);
	Lumiere->SetAttenuationRadius(400.f);
	Lumiere->SetCastShadows(false);

	Mouvement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Mouvement"));
	Mouvement->UpdatedComponent = Sphere;
	Mouvement->InitialSpeed = 6000.f;
	Mouvement->MaxSpeed = 100000.f;
	Mouvement->ProjectileGravityScale = 0.f;
	Mouvement->bRotationFollowsVelocity = true;
	Mouvement->bShouldBounce = false;
}

void AProjectileArsenal::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectileArsenal, Arme);
	DOREPLIFETIME(AProjectileArsenal, Vitesse);
	DOREPLIFETIME(AProjectileArsenal, Couleur);
	DOREPLIFETIME(AProjectileArsenal, bArmee);
	DOREPLIFETIME(AProjectileArsenal, Attente);
}

void AProjectileArsenal::BeginPlay()
{
	Super::BeginPlay();

	// Mouvement selon l'arme
	Mouvement->MaxSpeed = FMath::Max(Vitesse, 100.f);
	if (Arme == Grenade)
	{
		Sphere->SetSphereRadius(7.f);
		Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // elle passe entre les jambes des zombies
		Mouvement->ProjectileGravityScale = 1.f;
		Mouvement->bShouldBounce = true;
		Mouvement->Bounciness = 0.35f;
		Mouvement->Friction = 0.6f;
		Mouvement->bRotationFollowsVelocity = false;
	}
	else if (Arme == Bazooka)
	{
		Sphere->SetSphereRadius(12.f);
	}
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
		if (Arme == Grenade)
		{
			Mouvement->OnProjectileStop.AddDynamic(this, &AProjectileArsenal::Arrete);
		}
		else
		{
			Sphere->OnComponentHit.AddDynamic(this, &AProjectileArsenal::Touche);
		}
	}
	else if (Arme != Grenade)
	{
		// Chez les autres joueurs le trait / la fusee ne fait que suivre le serveur
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		Allure();
	}
}

void AProjectileArsenal::Allure()
{
	const FLinearColor C = FLinearColor(Couleur);
	USceneComponent* Racine = Sphere;
	const FVector Ici = GetActorLocation();
	using namespace ChellesArsenalVisuel;

	switch (Arme)
	{
	case Blaster:
		// Trait lumineux de 90 cm (coeur blanc + gaine de couleur)
		Pieces.Add(Piece(Racine, Cylindre, FVector(-35.f, 0.f, 0.f), FVector(90.f, 4.f, 4.f), C, true));
		Pieces.Add(Piece(Racine, Cylindre, FVector(-35.f, 0.f, 0.f), FVector(80.f, 1.6f, 1.6f), FLinearColor::White, true));
		Lumiere->SetLightColor(C);
		Lumiere->SetIntensity(7000.f);
		Lumiere->SetAttenuationRadius(350.f);
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::Piou, Ici, 0.9f, FMath::FRandRange(0.94f, 1.06f));
		break;

	case Bazooka:
		Pieces.Add(Piece(Racine, Cylindre, FVector(-10.f, 0.f, 0.f), FVector(60.f, 11.f, 11.f), FLinearColor(0.75f, 0.75f, 0.7f), false));
		Pieces.Add(Piece(Racine, Cone, FVector(28.f, 0.f, 0.f), FVector(16.f, 11.f, 11.f), FLinearColor(0.8f, 0.05f, 0.03f), false));
		// Ailerons
		Pieces.Add(Piece(Racine, Cube, FVector(-36.f, 0.f, 0.f), FVector(10.f, 26.f, 1.5f), FLinearColor(0.15f, 0.15f, 0.15f), false));
		Pieces.Add(Piece(Racine, Cube, FVector(-36.f, 0.f, 0.f), FVector(10.f, 1.5f, 26.f), FLinearColor(0.15f, 0.15f, 0.15f), false));
		// Flamme du moteur
		Pieces.Add(Piece(Racine, Cone, FVector(-48.f, 0.f, 0.f), FVector(22.f, 9.f, 9.f), FLinearColor(1.f, 0.45f, 0.05f), true, FRotator(0.f, 180.f, 0.f)));
		if (UParticleSystem* Flammes = ChellesArmesEffets::Flammes())
		{
			UGameplayStatics::SpawnEmitterAttached(Flammes, Sphere, NAME_None, FVector(-45.f, 0.f, 0.f), FRotator::ZeroRotator,
				FVector(0.25f), EAttachLocation::KeepRelativeOffset, true);
		}
		Lumiere->SetLightColor(FLinearColor(1.f, 0.5f, 0.15f));
		Lumiere->SetIntensity(9000.f);
		Lumiere->SetAttenuationRadius(600.f);
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::Lancement, Ici, 1.f);
		SonVol = USonArsenalChelles::Jouer(this, ChellesArsenalSons::Reacteur, Ici, 0.7f, 1.f, Sphere);
		break;

	case Grenade:
		Pieces.Add(Piece(Racine, ChellesArsenalVisuel::Sphere, FVector::ZeroVector, FVector(13.f), FLinearColor(0.08f, 0.18f, 0.06f), false));
		Pieces.Add(Piece(Racine, Cylindre, FVector::ZeroVector, FVector(13.6f, 13.6f, 3.f), C, true, FRotator::ZeroRotator, false));
		Pieces.Add(Piece(Racine, Cylindre, FVector(0.f, 0.f, 8.f), FVector(2.f, 2.f, 3.f), FLinearColor(0.5f, 0.5f, 0.5f), false, FRotator::ZeroRotator, false));
		Lumiere->SetLightColor(C);
		Lumiere->SetIntensity(0.f);
		Lumiere->SetAttenuationRadius(500.f);
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::Lancer, Ici, 0.7f);
		break;

	default:
		break;
	}
	Pieces.Remove(nullptr);
	LumiereNormale = Lumiere->Intensity;
}

void AProjectileArsenal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}

	// Eclair de lumiere de l'explosion qui s'eteint
	if (Flash > 0.f)
	{
		Flash = FMath::Max(0.f, Flash - DeltaSeconds);
		Lumiere->SetIntensity(80000.f * Flash / 0.35f);
	}
	if (bFini)
	{
		return;
	}

	// ---- Serveur ----
	if (HasAuthority())
	{
		if (Arme == Bazooka)
		{
			AArsenalRelais* R = Relais.Get();
			if (R && Virage > 0.f && W->GetTimeSeconds() - R->HeureVise < 0.6f)
			{
				// La fusee tourne vers le point vise par le joueur, sans depasser Virage degres par seconde
				const FVector Dir = Mouvement->Velocity.GetSafeNormal();
				const FVector Voulu = (R->PointVise - GetActorLocation()).GetSafeNormal();
				if (!Dir.IsNearlyZero() && !Voulu.IsNearlyZero())
				{
					const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Dir, Voulu), -1.f, 1.f));
					const float Max = FMath::DegreesToRadians(Virage) * DeltaSeconds;
					FVector Nouvelle = Voulu;
					if (Angle > Max && Angle > KINDA_SMALL_NUMBER)
					{
						const FQuat Tour = FQuat::FindBetweenNormals(Dir, Voulu);
						Nouvelle = FQuat::Slerp(FQuat::Identity, Tour, Max / Angle).RotateVector(Dir);
					}
					Mouvement->Velocity = Nouvelle.GetSafeNormal() * Vitesse;
				}
			}
			if (Age > DureeVie)
			{
				Exploser(GetActorLocation());
				return;
			}
		}
		else if (Arme == Blaster && Age > 3.f)
		{
			Finir();
			return;
		}
		else if (Arme == Grenade)
		{
			if (!bArmee && Age > 2.5f)
			{
				Armer(); // pas encore posee (roule encore...) : elle s'arme quand meme
			}
			if (bArmee && AgeArmee >= Attente)
			{
				Exploser(GetActorLocation() + FVector(0.f, 0.f, 20.f));
				return;
			}
		}
	}
	if (bArmee)
	{
		AgeArmee += DeltaSeconds;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// ---- Effets sur chaque machine ----
	if (Arme == Bazooka && Age >= ProchaineFumee)
	{
		// Traine de fumee : petites bouffees laissees derriere la fusee
		ProchaineFumee = Age + 0.045f;
		ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Vapeur(), GetActorLocation() - GetActorForwardVector() * 50.f, 0.45f, 0.9f);
	}
	if (Arme == Grenade && bArmee)
	{
		// Bips de plus en plus rapides + lumiere qui clignote
		const float Avance = FMath::Clamp(AgeArmee / FMath::Max(Attente, 0.1f), 0.f, 1.f);
		if (AgeArmee >= ProchainBip)
		{
			ProchainBip = AgeArmee + FMath::Lerp(0.7f, 0.12f, Avance);
			USonArsenalChelles::Jouer(this, ChellesArsenalSons::Bip, GetActorLocation(), 0.8f, 1.f + Avance * 0.4f);
			Lumiere->SetIntensity(20000.f);
		}
		else
		{
			Lumiere->SetIntensity(FMath::Max(0.f, Lumiere->Intensity - DeltaSeconds * 120000.f));
		}
	}
}

void AProjectileArsenal::Touche(UPrimitiveComponent* MonComposant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact)
{
	if (!HasAuthority() || bFini)
	{
		return;
	}
	const FVector Point = Impact.bBlockingHit ? FVector(Impact.ImpactPoint) : GetActorLocation();
	if (Arme == Bazooka)
	{
		Exploser(Point);
		return;
	}

	// Blaster : coup precis (tete / corps pour les zombies)
	const FVector Direction = Mouvement->Velocity.IsNearlyZero() ? GetActorForwardVector() : Mouvement->Velocity.GetSafeNormal();
	if (ChellesArsenalDegats::PeutBlesser(Autre, GetInstigator(), bToucherJoueurs))
	{
		ChellesArsenalDegats::Blesser(Autre, Degats, Impact, Direction, GetInstigator(), this);
	}
	UPrimitiveComponent* Objet = Impact.GetComponent();
	if (Objet && Objet->IsSimulatingPhysics())
	{
		Objet->AddImpulseAtLocation(Direction * 12000.f, Point);
	}
	MulticastImpact(Point);
	Finir();
}

void AProjectileArsenal::Arrete(const FHitResult& Impact)
{
	if (HasAuthority() && !bArmee)
	{
		Armer();
	}
}

void AProjectileArsenal::Armer()
{
	if (bArmee || bFini)
	{
		return;
	}
	bArmee = true;
	AgeArmee = 0.f;
	// L'appat que les zombies poursuivent (plugin ChellesZombies)
	UWorld* W = GetWorld();
	if (W && RayonAppat > 0.f)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		if (AAppatChelles* A = W->SpawnActor<AAppatChelles>(AAppatChelles::StaticClass(), GetActorTransform(), Params))
		{
			A->RayonAppat = RayonAppat;
			A->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
			Appat = A;
		}
	}
	OnRep_Armee();
}

void AProjectileArsenal::OnRep_Armee()
{
	if (bArmee)
	{
		ProchainBip = 0.f;
	}
}

void AProjectileArsenal::Exploser(const FVector& Point)
{
	if (bFini)
	{
		return;
	}
	if (Appat.IsValid())
	{
		Appat->Destroy();
	}
	ChellesArsenalDegats::Explosion(GetWorld(), Point, Rayon, Degats, Poussee, GetInstigator(), this, bToucherJoueurs);
	MulticastExplosion(Point, Rayon);
	Finir();
}

void AProjectileArsenal::Finir()
{
	if (bFini)
	{
		return;
	}
	bFini = true;
	Mouvement->StopMovementImmediately();
	Mouvement->ProjectileGravityScale = 0.f;
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(0.6f); // le temps que l'effet arrive chez tout le monde
}

void AProjectileArsenal::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Appat.IsValid())
	{
		Appat->Destroy();
	}
	if (SonVol)
	{
		SonVol->Stop();
	}
	Super::EndPlay(Raison);
}

void AProjectileArsenal::MulticastImpact_Implementation(FVector_NetQuantize Point)
{
	bFini = true;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (UStaticMeshComponent* P : Pieces)
	{
		if (P)
		{
			P->SetVisibility(false);
		}
	}
	Lumiere->SetIntensity(0.f);
	ChellesArmesEffets::Bouffee(GetWorld(), ChellesArmesEffets::ImpactBalle(), Point, 1.f, 0.4f);
	USonArsenalChelles::Jouer(this, ChellesArsenalSons::Impact, Point, 0.6f, FMath::FRandRange(0.9f, 1.2f));
}

void AProjectileArsenal::MulticastExplosion_Implementation(FVector_NetQuantize Point, float Taille)
{
	bFini = true;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (SonVol)
	{
		SonVol->Stop();
		SonVol = nullptr;
	}
	for (UStaticMeshComponent* P : Pieces)
	{
		if (P)
		{
			P->SetVisibility(false);
		}
	}
	SetActorLocation(Point);
	UWorld* W = GetWorld();
	const float Echelle = FMath::Clamp(Taille / 500.f, 0.5f, 4.f);
	if (UParticleSystem* Boum = ChellesArmesEffets::Explosion())
	{
		UGameplayStatics::SpawnEmitterAtLocation(W, Boum, Point, FRotator::ZeroRotator, FVector(Echelle));
	}
	ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Vapeur(), Point, 1.5f * Echelle, 1.5f);
	ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Flammes(), Point, 0.8f * Echelle, 1.f);
	if (USoundBase* Son = ChellesArmesEffets::SonExplosion())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Son, Point, 1.f);
	}
	else
	{
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::Boum, Point, 1.2f);
	}
	// Eclair de lumiere orange
	Lumiere->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Lumiere->SetAttenuationRadius(FMath::Max(800.f, Taille * 2.5f));
	Flash = 0.35f;
}
