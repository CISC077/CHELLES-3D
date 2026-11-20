// CHELLES 3D - Butin : ping et explosions
#include "PingButin.h"
#include "ButinTypes.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

using namespace ChellesButin;

namespace
{
	void ButinColorerForme(UStaticMeshComponent* C, const FLinearColor& Couleur)
	{
		if (!C)
		{
			return;
		}
		UMaterialInterface* Forme = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (UMaterialInstanceDynamic* M = C->CreateDynamicMaterialInstance(0, Forme ? Forme : C->GetMaterial(0)))
		{
			M->SetVectorParameterValue(TEXT("Color"), Couleur);
		}
	}
}

APingButin::APingButin()
{
	bReplicates = true;
	bAlwaysRelevant = true; // un ping se voit de loin
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 8.f;

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylindre(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Colonne = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Colonne"));
	Colonne->SetupAttachment(Racine);
	Point = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Point"));
	Point->SetupAttachment(Racine);
	for (UStaticMeshComponent* C : { Colonne, Point })
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->CastShadow = false;
	}
	if (Cylindre.Succeeded())
	{
		Colonne->SetStaticMesh(Cylindre.Object);
		Colonne->SetRelativeScale3D(FVector(0.05f, 0.05f, 5.f));
		Colonne->SetRelativeLocation(FVector(0.f, 0.f, 250.f));
	}
	if (Sphere.Succeeded())
	{
		Point->SetStaticMesh(Sphere.Object);
		Point->SetRelativeScale3D(FVector(0.35f));
		Point->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	}

	Texte = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Texte"));
	Texte->SetupAttachment(Racine);
	Texte->SetRelativeLocation(FVector(0.f, 0.f, 560.f));
	Texte->SetHorizontalAlignment(EHTA_Center);
	Texte->SetVerticalAlignment(EVRTA_TextBottom);
	Texte->SetWorldSize(40.f);
	Texte->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Lumiere = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lumiere"));
	Lumiere->SetupAttachment(Racine);
	Lumiere->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Lumiere->SetIntensity(2500.f);
	Lumiere->SetAttenuationRadius(350.f);
	Lumiere->SetCastShadows(false);
	Lumiere->SetMobility(EComponentMobility::Movable);
}

void APingButin::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APingButin, Genre);
	DOREPLIFETIME(APingButin, Auteur);
}

void APingButin::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FLinearColor Couleur = CouleurPing(Genre);
	ButinColorerForme(Colonne, Couleur);
	ButinColorerForme(Point, Couleur);
	Lumiere->SetLightColor(Couleur);
	Texte->SetTextRenderColor(Couleur.ToFColor(true));
	const FString Ligne = Auteur.IsEmpty() ? FString(TextePing(Genre)) : FString::Printf(TEXT("%s\n%s"), TextePing(Genre), *Auteur);
	Texte->SetText(FText::FromString(Ligne));

	FReglagesButin R;
	ChargerReglagesButin(R);
	if (USoundBase* Son = LoadObject<USoundBase>(nullptr, *R.SonPing, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(this, Son, Genre == PingEnnemi || Genre == PingFrappe ? 0.9f : 0.6f,
			Genre == PingEnnemi ? 1.3f : 1.f);
	}
}

void APingButin::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Le texte regarde toujours la camera et grossit avec la distance (lisible de loin)
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Ici = Texte->GetComponentLocation();
		FRotator Vers = (Camera - Ici).Rotation();
		Vers.Pitch = 0.f;
		Vers.Roll = 0.f;
		Texte->SetWorldRotation(Vers);
		const float Distance = FVector::Dist(Camera, Ici);
		Texte->SetWorldSize(FMath::Clamp(Distance * 0.025f, 25.f, 600.f));
	}
	// petit battement de la boule
	Point->SetRelativeScale3D(FVector(0.35f * (1.f + 0.2f * FMath::Sin(Temps * 8.f))));
}

// ---------------------------------------------------------------------------------------------------

AEffetButin::AEffetButin()
{
	bReplicates = true;
	bAlwaysRelevant = false;
	NetCullDistanceSquared = FMath::Square(20000.f);
	SetReplicatingMovement(false);
	InitialLifeSpan = 2.f;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AEffetButin::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UWorld* W = GetWorld();
	const FVector Ici = GetActorLocation();
	UParticleSystem* Feu = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FPS_Game/Particles/P_Explosion.P_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Feu)
	{
		Feu = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Feu && W)
	{
		UGameplayStatics::SpawnEmitterAtLocation(W, Feu, FTransform(FRotator::ZeroRotator, Ici, FVector(2.f)));
	}
	USoundBase* Boum = LoadObject<USoundBase>(nullptr, TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Boum)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Boum, Ici, 1.f, FMath::FRandRange(0.85f, 1.1f));
	}
}
