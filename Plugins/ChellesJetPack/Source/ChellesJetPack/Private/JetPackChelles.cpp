// CHELLES 3D - JetPack a ramasser (UE 4.27)
#include "JetPackChelles.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Noms de parametres de couleur souvent utilises, essayes si le script n'a rien trouve. */
	const TCHAR* ParametresCouleurParDefaut[] = {
		TEXT("Color"), TEXT("Colour"), TEXT("Tint"), TEXT("BaseColor"), TEXT("Base Color"), TEXT("Couleur"),
		TEXT("Mask Colour"), TEXT("Mask Color"), TEXT("Colour 1"), TEXT("Color 1"), TEXT("Colour1"), TEXT("Color1"),
		TEXT("Primary Colour"), TEXT("Primary Color"), TEXT("R Colour"), TEXT("R Color"), TEXT("Red Channel Colour")};

	bool ModeCompatible(const UCharacterMovementComponent* M)
	{
		return M && (M->MovementMode == MOVE_Walking || M->MovementMode == MOVE_NavWalking
			|| M->MovementMode == MOVE_Falling || M->MovementMode == MOVE_Flying);
	}

	FString Aide()
	{
		return TEXT("ESPACE : voler / monter   •   CTRL ou C : descendre   •   MAJ : turbo   •   N : lâcher");
	}
}

AJetPackChelles::AJetPackChelles()
{
	bReplicates = true;
	SetReplicatingMovement(false); // la place au sol passe par PosePosition, le dos par l'accroche
	bAlwaysRelevant = true;       // peu nombreux : tout le monde les voit partout
	NetUpdateFrequency = 20.f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	Zone->InitSphereRadius(110.f);
	Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Zone->SetCollisionObjectType(ECC_WorldDynamic);
	Zone->SetCollisionResponseToAllChannels(ECR_Ignore);
	Zone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Zone->SetGenerateOverlapEvents(true);
	Zone->SetCanEverAffectNavigation(false);
	RootComponent = Zone;

	Modele = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Modele"));
	Modele->SetupAttachment(Zone);
	Modele->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Modele->SetGenerateOverlapEvents(false);
	Modele->SetCanEverAffectNavigation(false);
	Modele->bReceivesDecals = false;
	Modele->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Lueur = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lueur"));
	Lueur->SetupAttachment(Zone);
	Lueur->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Lueur->SetIntensity(2500.f);
	Lueur->SetAttenuationRadius(350.f);
	Lueur->SetCastShadows(false);
}

void AJetPackChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AJetPackChelles, Porteur);
	DOREPLIFETIME(AJetPackChelles, bEnVol);
	DOREPLIFETIME(AJetPackChelles, bTurbo);
	DOREPLIFETIME(AJetPackChelles, PosePosition);
	DOREPLIFETIME(AJetPackChelles, PoseRotation);
}

AJetPackChelles* AJetPackChelles::PortePar(const ACharacter* Perso)
{
	if (!Perso || !Perso->GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<AJetPackChelles> It(Perso->GetWorld()); It; ++It)
	{
		if (It->Porteur == Perso)
		{
			return *It;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Apparence
// ---------------------------------------------------------------------------

void AJetPackChelles::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AppliquerApparence(); // pour voir la couleur directement dans l'editeur
}

void AJetPackChelles::AppliquerApparence()
{
	if (ModeleJetPack && Modele->SkeletalMesh != ModeleJetPack)
	{
		Modele->SetSkeletalMesh(ModeleJetPack);
	}
	if (!Porteur)
	{
		Modele->SetRelativeScale3D(FVector(TailleAuSol));
	}
	Lueur->SetLightColor(Couleur);

	Modele->EmptyOverrideMaterials();
	const FLinearColor Sombre = Couleur * 0.35f;
	const int32 Nombre = Modele->GetNumMaterials();
	for (int32 i = 0; i < Nombre; ++i)
	{
		UMaterialInterface* Base = MateriauCouleur ? MateriauCouleur : Modele->GetMaterial(i);
		if (!Base)
		{
			continue;
		}
		UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(Base, this);
		if (!Dyn)
		{
			continue;
		}
		if (ParametresCouleur.Num() > 0)
		{
			for (const FName& Nom : ParametresCouleur)
			{
				Dyn->SetVectorParameterValue(Nom, Couleur);
			}
		}
		else
		{
			for (const TCHAR* Nom : ParametresCouleurParDefaut)
			{
				Dyn->SetVectorParameterValue(FName(Nom), Couleur);
			}
		}
		for (const FName& Nom : ParametresCouleurSombre)
		{
			Dyn->SetVectorParameterValue(Nom, Sombre);
		}
		Modele->SetMaterial(i, Dyn);
	}
}

void AJetPackChelles::CreerEffets()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (SonMoteur && !Moteur)
	{
		Moteur = NewObject<UAudioComponent>(this, TEXT("Moteur"));
		Moteur->bAutoActivate = false;
		Moteur->SetupAttachment(Modele);
		Moteur->SetSound(SonMoteur);
		Moteur->bOverrideAttenuation = true; // on entend le JetPack pres de lui, pas dans toute la ville
		Moteur->AttenuationOverrides.bAttenuate = true;
		Moteur->AttenuationOverrides.bSpatialize = true;
		Moteur->AttenuationOverrides.AttenuationShapeExtents = FVector(400.f);
		Moteur->AttenuationOverrides.FalloffDistance = 4500.f;
		Moteur->RegisterComponent();
	}
	if (!Flammes || Reacteurs.Num() > 0)
	{
		return;
	}

	// Les reacteurs = les "sockets" du modele, sinon ses petits os (comme le fait le pack)
	TArray<FName> Points;
	if (USkeletalMesh* SK = Modele->SkeletalMesh)
	{
		for (int32 i = 0; i < SK->NumSockets(); ++i)
		{
			if (USkeletalMeshSocket* S = SK->GetSocketByIndex(i))
			{
				Points.AddUnique(S->SocketName);
			}
		}
		if (Points.Num() == 0)
		{
			const int32 NbOs = Modele->GetNumBones();
			for (int32 i = 1; i < NbOs; ++i)
			{
				const FName Os = Modele->GetBoneName(i);
				const FString Texte = Os.ToString().ToLower();
				if (Texte.Contains(TEXT("exhaust")) || Texte.Contains(TEXT("jet")) || Texte.Contains(TEXT("thrust"))
					|| Texte.Contains(TEXT("nozzle")) || Texte.Contains(TEXT("fx")) || Texte.Contains(TEXT("flame"))
					|| Texte.Contains(TEXT("fire")))
				{
					Points.Add(Os);
				}
			}
		}
	}
	if (Points.Num() > 6)
	{
		Points.SetNum(6);
	}
	if (Points.Num() == 0)
	{
		Points.Add(NAME_None); // au moins une flamme sous le JetPack
	}
	for (const FName& Point : Points)
	{
		UParticleSystemComponent* P = NewObject<UParticleSystemComponent>(this);
		P->bAutoActivate = false;
		P->SetTemplate(Flammes);
		P->SetupAttachment(Modele, Point);
		if (Point.IsNone())
		{
			P->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
		}
		P->RegisterComponent();
		Reacteurs.Add(P);
	}
	UE_LOG(LogTemp, Log, TEXT("[JetPack] %s : %d reacteur(s)."), *GetName(), Reacteurs.Num());
}

void AJetPackChelles::MettreAJourEffets()
{
	const bool bActif = bEnVol && Porteur != nullptr;
	for (UParticleSystemComponent* P : Reacteurs)
	{
		if (!P)
		{
			continue;
		}
		if (bActif)
		{
			if (!P->IsActive())
			{
				P->ActivateSystem(true);
			}
			P->SetFloatParameter(TEXT("Scale"), bTurbo ? 1.6f : 1.f);
		}
		else if (P->IsActive())
		{
			P->DeactivateSystem();
		}
	}
	if (Moteur)
	{
		if (bActif && !Moteur->IsPlaying())
		{
			Moteur->Play();
		}
		else if (!bActif && Moteur->IsPlaying())
		{
			Moteur->FadeOut(0.4f, 0.f);
		}
		Moteur->SetPitchMultiplier(bTurbo ? 1.25f : 1.f);
	}
}

// ---------------------------------------------------------------------------
// Demarrage / fin
// ---------------------------------------------------------------------------

void AJetPackChelles::BeginPlay()
{
	Super::BeginPlay();
	Depart = GetActorTransform();
	bDepartConnu = true;
	if (HasAuthority())
	{
		PosePosition = Depart.GetLocation();
		PoseRotation = Depart.Rotator();
		bPoseConnue = true;
	}
	AppliquerApparence();
	CreerEffets();
	AppliquerPorte();
}

void AJetPackChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (PersoModifie.IsValid())
	{
		FinDuVol(PersoModifie.Get());
	}
	RetirerMessage();
	Super::EndPlay(Raison);
}

// ---------------------------------------------------------------------------
// Sur le dos / au sol (chaque machine)
// ---------------------------------------------------------------------------

void AJetPackChelles::OnRep_Porteur()
{
	AppliquerPorte();
}

void AJetPackChelles::OnRep_Pose()
{
	bPoseConnue = true;
	if (!Porteur)
	{
		AppliquerPorte();
	}
}

void AJetPackChelles::OnRep_EnVol()
{
	if (Porteur && Porteur->IsLocallyControlled())
	{
		bVolLocal = bEnVol;
		if (!bEnVol)
		{
			FinDuVol(Porteur);
		}
	}
}

void AJetPackChelles::AppliquerPorte()
{
	// l'ancien porteur retrouve sa vitesse normale
	if (PersoModifie.IsValid() && PersoModifie.Get() != Porteur)
	{
		FinDuVol(PersoModifie.Get());
	}

	if (Porteur)
	{
		Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lueur->SetVisibility(false);
		Modele->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Modele->SetRelativeScale3D(FVector::OneVector);

		USkeletalMeshComponent* Corps = Porteur->GetMesh();
		const FAttachmentTransformRules Regles(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
		if (Corps && Corps->DoesSocketExist(OsDuDos))
		{
			AttachToComponent(Corps, Regles, OsDuDos);
			Zone->SetRelativeLocationAndRotation(DecalageDos, RotationDos);
		}
		else
		{
			// personnage sans os "spine_03" : on l'accroche au dos de la capsule
			AttachToComponent(Porteur->GetRootComponent(), Regles, NAME_None);
			Zone->SetRelativeLocationAndRotation(FVector(-35.f, 0.f, 25.f), FRotator(0.f, 0.f, 0.f));
		}
		SetActorScale3D(FVector::OneVector);
		// en vue a la 1re personne le corps est cache au joueur : on cache aussi son JetPack (pas aux autres)
		Modele->SetOwnerNoSee(Corps && Corps->bOwnerNoSee);
	}
	else
	{
		if (GetAttachParentActor() || RootComponent->GetAttachParent())
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		SetActorScale3D(FVector::OneVector);
		if (bPoseConnue)
		{
			SetActorLocationAndRotation(PosePosition, PoseRotation);
		}
		Zone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Lueur->SetVisibility(true);
		Modele->SetOwnerNoSee(false);
		Modele->SetRelativeScale3D(FVector(TailleAuSol));
	}

	// messages pour le joueur de cette machine
	const bool bMoi = Porteur && Porteur->IsLocallyControlled() && GetNetMode() != NM_DedicatedServer;
	if (bMoi && !bEtaitMoi)
	{
		bVolLocal = false;
		bTurboLocal = false;
		MessagePermanent = FString::Printf(TEXT("JETPACK %s\n%s"), *NomCouleur, *Aide());
		Afficher(FString::Printf(TEXT("Tu as ramassé le JETPACK %s !\n%s"), *NomCouleur, *Aide()), 6.f);
	}
	else if (!bMoi && bEtaitMoi)
	{
		bVolLocal = false;
		MessagePermanent.Empty();
		Afficher(FString::Printf(TEXT("JetPack %s lâché."), *NomCouleur), 3.f);
	}
	bEtaitMoi = bMoi;
}

// ---------------------------------------------------------------------------
// Chaque image
// ---------------------------------------------------------------------------

void AJetPackChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Horloge += DeltaSeconds;

	if (HasAuthority())
	{
		if (Porteur)
		{
			ServeurSurveillePorteur(DeltaSeconds);
		}
		else
		{
			ServeurCherche(DeltaSeconds);
		}
	}

	if (Porteur)
	{
		if (Porteur->IsLocallyControlled())
		{
			CommandesLocales(DeltaSeconds);
		}
		if (HasAuthority() || Porteur->IsLocallyControlled())
		{
			ImposerVol(DeltaSeconds);
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (!Porteur)
		{
			// au sol : il tourne et flotte doucement pour se faire remarquer
			Modele->SetRelativeLocationAndRotation(
				FVector(0.f, 0.f, 45.f + FMath::Sin(Horloge * 2.f) * 10.f),
				FRotator(0.f, FMath::Fmod(Horloge * 60.f, 360.f), 0.f));
		}
		MettreAJourEffets();
		MettreAJourMessage(DeltaSeconds);
	}
}

// ---------------------------------------------------------------------------
// Serveur : ramassage, surveillance du porteur, retour au depart
// ---------------------------------------------------------------------------

void AJetPackChelles::ServeurCherche(float Dt)
{
	if (FinRetour > 0.f && Horloge >= FinRetour)
	{
		RevenirAuDepart();
	}
	if (Horloge < ProchaineRecherche)
	{
		return;
	}
	ProchaineRecherche = Horloge + 0.2f;

	TArray<AActor*> Proches;
	Zone->GetOverlappingActors(Proches, ACharacter::StaticClass());
	for (AActor* A : Proches)
	{
		ACharacter* Perso = Cast<ACharacter>(A);
		if (!Perso || Perso->IsPendingKill() || !Perso->IsPlayerControlled() || !Perso->GetCharacterMovement())
		{
			continue;
		}
		if (Perso == DernierPorteur.Get() && Horloge < FinInterditDernier)
		{
			continue;
		}
		if (PortePar(Perso))
		{
			continue; // un seul JetPack a la fois
		}
		Prendre(Perso);
		return;
	}
}

void AJetPackChelles::Prendre(ACharacter* Perso)
{
	Porteur = Perso;
	SetOwner(Perso); // le joueur peut ainsi envoyer ses commandes au serveur
	PCDuPorteur = Cast<APlayerController>(Perso->GetController());
	bEnVol = false;
	bTurbo = false;
	FinRetour = -1.f;
	SansPion = 0.f;
	DernierePosition = Perso->GetActorLocation();
	AppliquerPorte();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[JetPack] %s ramasse par %s."), *NomCouleur, *Perso->GetName());
}

void AJetPackChelles::ServeurSurveillePorteur(float Dt)
{
	if (!IsValid(Porteur))
	{
		Porteur = nullptr; // personnage detruit
		Lacher(false);
		return;
	}
	DernierePosition = Porteur->GetActorLocation();

	if (AController* C = Porteur->GetController())
	{
		SansPion = 0.f;
		if (APlayerController* PC = Cast<APlayerController>(C))
		{
			PCDuPorteur = PC;
		}
		return;
	}

	APlayerController* PC = PCDuPorteur.Get();
	if (!PC)
	{
		Lacher(false); // joueur parti
		return;
	}
	APawn* Pion = PC->GetPawn();
	if (ACharacter* Nouveau = Cast<ACharacter>(Pion))
	{
		if (Nouveau != Porteur)
		{
			if (Porteur->IsHidden() && !PortePar(Nouveau))
			{
				// changement de personnage avec la roue (Tab) : le JetPack suit le joueur
				ACharacter* Ancien = Porteur;
				bEnVol = false;
				FinDuVol(Ancien);
				Prendre(Nouveau);
			}
			else
			{
				Lacher(false); // mort puis reapparu ailleurs : le JetPack reste sur place
			}
		}
		return;
	}
	if (Pion)
	{
		SansPion = 0.f; // dans un vehicule : il garde son JetPack
		return;
	}
	SansPion += Dt;
	if (SansPion > 3.f)
	{
		Lacher(false); // mort, en attente de reapparition
	}
}

void AJetPackChelles::Lacher(bool bRetourDepart)
{
	ACharacter* Ancien = IsValid(Porteur) ? Porteur : nullptr;
	if (Ancien)
	{
		FinDuVol(Ancien);
		DernierePosition = Ancien->GetActorLocation();
	}
	DernierPorteur = Ancien;
	FinInterditDernier = Horloge + 3.f;

	FVector Sol = DernierePosition;
	if (UWorld* W = GetWorld())
	{
		FCollisionQueryParams Params(TEXT("JetPackSol"), false, this);
		if (Ancien)
		{
			Params.AddIgnoredActor(Ancien);
		}
		FHitResult Impact;
		if (W->LineTraceSingleByChannel(Impact, DernierePosition + FVector(0.f, 0.f, 50.f),
			DernierePosition - FVector(0.f, 0.f, 100000.f), ECC_Visibility, Params))
		{
			Sol = Impact.ImpactPoint;
		}
	}

	Porteur = nullptr;
	bEnVol = false;
	bTurbo = false;
	SetOwner(nullptr);
	if (bRetourDepart && bDepartConnu)
	{
		PosePosition = Depart.GetLocation();
		PoseRotation = Depart.Rotator();
		FinRetour = -1.f;
	}
	else
	{
		PosePosition = Sol;
		PoseRotation = FRotator(0.f, Ancien ? Ancien->GetActorRotation().Yaw : 0.f, 0.f);
		FinRetour = Horloge + DelaiRetour;
	}
	bPoseConnue = true;
	AppliquerPorte();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[JetPack] %s lache."), *NomCouleur);
}

void AJetPackChelles::RevenirAuDepart()
{
	FinRetour = -1.f;
	if (!bDepartConnu)
	{
		return;
	}
	PosePosition = Depart.GetLocation();
	PoseRotation = Depart.Rotator();
	bPoseConnue = true;
	AppliquerPorte();
	ForceNetUpdate();
}

void AJetPackChelles::ServeurVoler_Implementation(bool bVoler)
{
	if (!Porteur)
	{
		return;
	}
	if (bVoler && !ModeCompatible(Porteur->GetCharacterMovement()))
	{
		bEnVol = false; // en vehicule, nage, vol de super-heros... : pas de JetPack
		ForceNetUpdate();
		return;
	}
	bEnVol = bVoler;
	if (!bVoler)
	{
		bTurbo = false;
		FinDuVol(Porteur);
	}
	ForceNetUpdate();
}

void AJetPackChelles::ServeurTurbo_Implementation(bool bActif)
{
	bTurbo = bActif && Porteur != nullptr;
}

void AJetPackChelles::ServeurLacher_Implementation()
{
	if (Porteur)
	{
		Lacher(false);
	}
}

// ---------------------------------------------------------------------------
// Vol (serveur + joueur qui le porte, pour que les deux soient d'accord)
// ---------------------------------------------------------------------------

void AJetPackChelles::ImposerVol(float Dt)
{
	ACharacter* Perso = Porteur;
	UCharacterMovementComponent* M = Perso ? Perso->GetCharacterMovement() : nullptr;
	if (!M)
	{
		return;
	}
	const bool bLocal = Perso->IsLocallyControlled();
	const bool bVeutVoler = bLocal ? bVolLocal : bEnVol;
	const bool bTurboVoulu = bLocal ? bTurboLocal : bTurbo;

	if (!bVeutVoler)
	{
		if (PersoModifie.Get() == Perso)
		{
			FinDuVol(Perso);
		}
		return;
	}

	if (PersoModifie.Get() != Perso)
	{
		PersoModifie = Perso;
		VitesseOrigine = M->MaxFlySpeed;
		FreinOrigine = M->BrakingDecelerationFlying;
	}
	if (M->MovementMode == MOVE_Walking || M->MovementMode == MOVE_NavWalking || M->MovementMode == MOVE_Falling)
	{
		M->SetMovementMode(MOVE_Flying);
	}
	if (M->MovementMode != MOVE_Flying)
	{
		return; // un autre mode (vehicule, vol de super-heros...) a la priorite
	}
	M->MaxFlySpeed = bTurboVoulu ? VitesseTurbo : VitesseVol;
	M->BrakingDecelerationFlying = 2200.f;

	// plafond : on ne monte pas indefiniment
	if (bDepartConnu && Perso->GetActorLocation().Z > Depart.GetLocation().Z + PlafondVol && M->Velocity.Z > 0.f)
	{
		M->Velocity.Z = 0.f;
	}
}

void AJetPackChelles::FinDuVol(ACharacter* Perso)
{
	if (!Perso)
	{
		return;
	}
	UCharacterMovementComponent* M = Perso->GetCharacterMovement();
	if (M && PersoModifie.Get() == Perso)
	{
		if (M->MovementMode == MOVE_Flying)
		{
			M->SetMovementMode(MOVE_Falling); // il retombe et atterrit normalement
		}
		if (VitesseOrigine >= 0.f)
		{
			M->MaxFlySpeed = VitesseOrigine;
			M->BrakingDecelerationFlying = FreinOrigine;
		}
	}
	if (PersoModifie.Get() == Perso)
	{
		PersoModifie = nullptr;
		VitesseOrigine = -1.f;
		FreinOrigine = -1.f;
	}
}

// ---------------------------------------------------------------------------
// Joueur qui porte le JetPack : clavier / manette
// ---------------------------------------------------------------------------

APlayerController* AJetPackChelles::PCLocal() const
{
	return Porteur ? Cast<APlayerController>(Porteur->GetController()) : nullptr;
}

bool AJetPackChelles::ProcheDuSol(const ACharacter* Perso, float Distance) const
{
	UWorld* W = GetWorld();
	if (!W || !Perso)
	{
		return false;
	}
	const float DemiHauteur = Perso->GetCapsuleComponent() ? Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const FVector Pieds = Perso->GetActorLocation() - FVector(0.f, 0.f, DemiHauteur);
	FCollisionQueryParams Params(TEXT("JetPackAtterrir"), false, Perso);
	Params.AddIgnoredActor(this);
	FHitResult Impact;
	return W->LineTraceSingleByChannel(Impact, Pieds + FVector(0.f, 0.f, 5.f), Pieds - FVector(0.f, 0.f, Distance), ECC_Visibility, Params);
}

void AJetPackChelles::CommandesLocales(float Dt)
{
	APlayerController* PC = PCLocal();
	UCharacterMovementComponent* M = Porteur->GetCharacterMovement();
	if (!PC || !M)
	{
		return;
	}
	if (ToucheLacher.IsValid() && PC->WasInputKeyJustPressed(ToucheLacher))
	{
		bVolLocal = false;
		FinDuVol(Porteur);
		ServeurLacher();
		return;
	}

	const bool bMonter = PC->IsInputKeyDown(ToucheMonter) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
	const bool bDescendre = PC->IsInputKeyDown(ToucheDescendre) || PC->IsInputKeyDown(ToucheDescendre2)
		|| PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Right);
	const bool bTurboVoulu = bVolLocal && (PC->IsInputKeyDown(ToucheTurbo) || PC->IsInputKeyDown(EKeys::Gamepad_LeftThumbstick));
	if (bTurboVoulu != bTurboLocal)
	{
		bTurboLocal = bTurboVoulu;
		ServeurTurbo(bTurboVoulu);
	}

	if (!bVolLocal)
	{
		const bool bAppui = PC->WasInputKeyJustPressed(ToucheMonter) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom);
		if (bAppui && ModeCompatible(M))
		{
			bVolLocal = true;
			AttenteDecollage = 0.4f;
			ServeurVoler(true);
		}
		return;
	}

	if (!ModeCompatible(M))
	{
		bVolLocal = false; // monte dans un vehicule, nage... : le JetPack s'arrete
		ServeurVoler(false);
		return;
	}

	const float Vertical = (bMonter ? 1.f : 0.f) - (bDescendre ? 1.f : 0.f);
	if (Vertical != 0.f)
	{
		Porteur->AddMovementInput(FVector::UpVector, Vertical);
	}

	AttenteDecollage -= Dt;
	if (AttenteDecollage <= 0.f && bDescendre && !bMonter && ProcheDuSol(Porteur, 70.f))
	{
		// atterrissage
		bVolLocal = false;
		bTurboLocal = false;
		FinDuVol(Porteur);
		ServeurVoler(false);
	}
}

// ---------------------------------------------------------------------------
// Message a l'ecran (joueur de cette machine)
// ---------------------------------------------------------------------------

void AJetPackChelles::Afficher(const FString& Texte, float Duree)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!Bandeau.IsValid())
	{
		UWorld* W = GetWorld();
		UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr;
		if (!Vue)
		{
			return;
		}
		Bandeau = SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(0.f, 0.f, 0.f, 150.f))
			.Visibility(EVisibility::HitTestInvisible)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
				.BorderBackgroundColor(FLinearColor(0.01f, 0.02f, 0.06f, 0.72f))
				.Padding(FMargin(18.f, 8.f))
				[
					SAssignNew(TexteBandeau, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
					.ColorAndOpacity(FLinearColor::White)
					.Justification(ETextJustify::Center)
				]
			];
		Vue->AddViewportWidgetContent(Bandeau.ToSharedRef(), 6);
	}
	TexteBandeau->SetText(FText::FromString(Texte));
	TexteBandeau->SetColorAndOpacity(FSlateColor(FLinearColor::LerpUsingHSV(Couleur, FLinearColor::White, 0.55f)));
	Bandeau->SetVisibility(EVisibility::HitTestInvisible);
	FinMessage = Horloge + Duree;
}

void AJetPackChelles::MettreAJourMessage(float Dt)
{
	if (!Bandeau.IsValid() || Horloge < FinMessage)
	{
		return;
	}
	if (MessagePermanent.IsEmpty())
	{
		Bandeau->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		const FString Texte = MessagePermanent;
		TexteBandeau->SetText(FText::FromString(Texte));
		Bandeau->SetVisibility(EVisibility::HitTestInvisible);
	}
}

void AJetPackChelles::RetirerMessage()
{
	if (!Bandeau.IsValid())
	{
		return;
	}
	UWorld* W = GetWorld();
	if (UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Bandeau.ToSharedRef());
	}
	Bandeau.Reset();
	TexteBandeau.Reset();
}
