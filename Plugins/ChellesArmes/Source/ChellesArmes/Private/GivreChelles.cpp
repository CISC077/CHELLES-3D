// CHELLES 3D - Armes : gel d'une cible (tir GLACIAL)
#include "GivreChelles.h"
#include "ArmesEffets.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

AGivreChelles::AGivreChelles()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AGivreChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGivreChelles, Cible);
	DOREPLIFETIME(AGivreChelles, Duree);
}

void AGivreChelles::Geler(UWorld* Monde, APawn* NouvelleCible, float NouvelleDuree, APawn* Auteur)
{
	if (!Monde || !NouvelleCible || NouvelleDuree <= 0.f)
	{
		return;
	}
	// Un seul gel a la fois par cible : le nouveau remplace l'ancien (la duree repart de zero)
	for (TActorIterator<AGivreChelles> It(Monde); It; ++It)
	{
		if (It->Cible == NouvelleCible)
		{
			It->Destroy();
		}
	}
	FActorSpawnParameters Params;
	Params.Instigator = Auteur;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AGivreChelles* G = Monde->SpawnActor<AGivreChelles>(AGivreChelles::StaticClass(), NouvelleCible->GetActorTransform(), Params);
	if (G)
	{
		G->Cible = NouvelleCible;
		G->Duree = NouvelleDuree;
		G->FinishSpawning(NouvelleCible->GetActorTransform());
	}
}

void AGivreChelles::BeginPlay()
{
	Super::BeginPlay();
	bPret = true;
	if (HasAuthority())
	{
		SetLifeSpan(FMath::Max(0.1f, Duree));
	}
	Commencer();
}

void AGivreChelles::OnRep_Cible()
{
	Commencer();
}

void AGivreChelles::Commencer()
{
	if (!Cible || Gele.IsValid() || !bPret)
	{
		return;
	}
	Gele = Cible;
	AttachToActor(Cible, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	Cible->CustomTimeDilation = 0.03f; // l'animation se fige

	if (HasAuthority())
	{
		if (AAIController* IA = Cast<AAIController>(Cible->GetController()))
		{
			IA->StopMovement();
			if (UBrainComponent* Cerveau = IA->GetBrainComponent())
			{
				Cerveau->PauseLogic(TEXT("Givre Chelles"));
			}
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		Lueur = NewObject<UPointLightComponent>(this, TEXT("Lueur"));
		Lueur->SetupAttachment(RootComponent);
		Lueur->SetIntensity(6000.f);
		Lueur->SetLightColor(FLinearColor(0.4f, 0.75f, 1.f));
		Lueur->SetAttenuationRadius(300.f);
		Lueur->SetCastShadows(false);
		Lueur->RegisterComponent();

		if (UParticleSystem* Modele = ChellesArmesEffets::Vapeur())
		{
			Brume = UGameplayStatics::SpawnEmitterAttached(Modele, RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
				FVector(0.6f), EAttachLocation::KeepRelativeOffset, true);
		}
	}
	Tick(0.f);
}

void AGivreChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APawn* P = Gele.Get();
	if (!P)
	{
		return;
	}
	P->CustomTimeDilation = 0.03f;
	ACharacter* Perso = Cast<ACharacter>(P);
	if (Perso && (HasAuthority() || Perso->IsLocallyControlled()))
	{
		UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement();
		if (Mouvement && Mouvement->MovementMode != MOVE_None)
		{
			Mouvement->StopMovementImmediately();
			Mouvement->DisableMovement();
		}
	}
}

void AGivreChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	Finir();
	Super::EndPlay(Raison);
}

void AGivreChelles::Finir()
{
	APawn* P = Gele.Get();
	Gele.Reset();
	if (Brume)
	{
		Brume->DeactivateSystem();
		Brume = nullptr;
	}
	if (!P)
	{
		return;
	}
	P->CustomTimeDilation = 1.f;
	ACharacter* Perso = Cast<ACharacter>(P);
	if (Perso && (HasAuthority() || Perso->IsLocallyControlled()))
	{
		UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement();
		if (Mouvement && Mouvement->MovementMode == MOVE_None)
		{
			Mouvement->SetMovementMode(MOVE_Falling);
		}
	}
	if (HasAuthority())
	{
		if (AAIController* IA = Cast<AAIController>(P->GetController()))
		{
			if (UBrainComponent* Cerveau = IA->GetBrainComponent())
			{
				Cerveau->ResumeLogic(TEXT("Givre Chelles"));
			}
		}
	}
}
