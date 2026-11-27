// CHELLES 3D - Caisses de munitions dispersees sur la carte (25/11/2026, agent Armements) - UE 4.27
#include "CaisseMunitions.h"
#include "ArsenalChelles.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

// ---------------------------------------------------------------------------
//  Reglages
// ---------------------------------------------------------------------------
const FReglagesCaisses& FReglagesCaisses::Lire(bool bRelire)
{
	static FReglagesCaisses R;
	static bool bLu = false;
	if (bLu && !bRelire)
	{
		return R;
	}
	bLu = true;
	R = FReglagesCaisses();
	FString Contenu;
	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesArmes"), TEXT("Arsenal.txt"));
	if (!FFileHelper::LoadFileToString(Contenu, *Chemin))
	{
		return R;
	}
	TArray<FString> Lignes;
	Contenu.ParseIntoArrayLines(Lignes);
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		FString Cle, Valeur;
		if (Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		const bool bOui = Valeur.StartsWith(TEXT("O"), ESearchCase::IgnoreCase) || Valeur == TEXT("1");
		if (Cle.Equals(TEXT("Caisses"), ESearchCase::IgnoreCase))                    { R.bActif = bOui; }
		else if (Cle.Equals(TEXT("Caisses.Nombre"), ESearchCase::IgnoreCase))        { R.Nombre = FMath::Clamp(FCString::Atoi(*Valeur), 0, 200); }
		else if (Cle.Equals(TEXT("Caisses.Attente"), ESearchCase::IgnoreCase))       { R.Attente = FMath::Max(5.f, FCString::Atof(*Valeur)); }
		else if (Cle.Equals(TEXT("Caisses.Rayon"), ESearchCase::IgnoreCase))         { R.Rayon = FMath::Max(2000.f, FCString::Atof(*Valeur) * 100.f); }
		else if (Cle.Equals(TEXT("Caisses.Ecart"), ESearchCase::IgnoreCase))         { R.Ecart = FMath::Max(0.f, FCString::Atof(*Valeur) * 100.f); }
		else if (Cle.Equals(TEXT("Caisses.Flammes"), ESearchCase::IgnoreCase))       { R.Flammes = FMath::Max(0, FCString::Atoi(*Valeur)); }
		else if (Cle.Equals(TEXT("Caisses.Mitrailleuse"), ESearchCase::IgnoreCase))  { R.Mitrailleuse = FMath::Max(0, FCString::Atoi(*Valeur)); }
		else if (Cle.Equals(TEXT("Caisses.Modele"), ESearchCase::IgnoreCase))        { R.Modele = Valeur; }
	}
	return R;
}

// ---------------------------------------------------------------------------
//  Petits outils de lecture / ecriture par nom (aucun Blueprint modifie)
// ---------------------------------------------------------------------------
namespace
{
	FNumericProperty* Nombre(UObject* Objet, const TCHAR* Nom)
	{
		return Objet ? FindFProperty<FNumericProperty>(Objet->GetClass(), FName(Nom)) : nullptr;
	}

	double LireNombre(UObject* Objet, FNumericProperty* P)
	{
		void* Valeur = P->ContainerPtrToValuePtr<void>(Objet);
		return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Valeur) : (double)P->GetSignedIntPropertyValue(Valeur);
	}

	void EcrireNombre(UObject* Objet, FNumericProperty* P, double V)
	{
		void* Valeur = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Valeur, V);
		}
		else
		{
			P->SetIntPropertyValue(Valeur, (int64)V);
		}
	}

	/** Met une valeur au maximum (Max lu dans NomMax, sinon +Secours). true si quelque chose a change. */
	bool Remplir(UObject* Arme, const TCHAR* Nom, const TCHAR* NomMax, double Secours)
	{
		FNumericProperty* P = Nombre(Arme, Nom);
		if (!P)
		{
			return false;
		}
		FNumericProperty* Max = Nombre(Arme, NomMax);
		const double Plein = Max ? LireNombre(Arme, Max) : 0.0;
		const double Avant = LireNombre(Arme, P);
		const double Apres = Plein > 0.0 ? FMath::Max(Avant, Plein) : Avant + Secours;
		if (Apres <= Avant)
		{
			return false;
		}
		EcrireNombre(Arme, P, Apres);
		return true;
	}

	/** 27/11/2026 : chargeur ET reserve au maximum (avant : seulement la reserve, deja pleine => "0 arme remplie"). */
	bool RemplirUneArme(UObject* Arme)
	{
		if (!Arme || Nombre(Arme, TEXT("AmmoInReserve")) == nullptr)
		{
			return false;
		}
		const bool bReserve = Remplir(Arme, TEXT("AmmoInReserve"), TEXT("AmmoInReserve_Max"), 60.0);
		const bool bChargeur = Remplir(Arme, TEXT("AmmoCurrent"), TEXT("AmmoMax"), 0.0);
		if ((bReserve || bChargeur) && Cast<AActor>(Arme))
		{
			Cast<AActor>(Arme)->ForceNetUpdate();
		}
		return bReserve || bChargeur;
	}

	void AjouterArsenal(APawn* Pion, const TCHAR* Propriete, int32 Quantite)
	{
		APlayerController* PC = Pion ? Cast<APlayerController>(Pion->GetController()) : nullptr;
		UWorld* W = Pion ? Pion->GetWorld() : nullptr;
		if (!PC || !W || Quantite <= 0)
		{
			return;
		}
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->GetOwner() == PC && It->GetClass()->GetName() == TEXT("ArsenalRelais"))
			{
				if (FIntProperty* P = FindFProperty<FIntProperty>(It->GetClass(), FName(Propriete)))
				{
					P->SetPropertyValue_InContainer(*It, P->GetPropertyValue_InContainer(*It) + Quantite);
					It->ForceNetUpdate();
				}
				return;
			}
		}
	}

	bool EstJoueur(const APawn* Pion)
	{
		return Pion && Pion->GetPlayerState() && !Pion->GetPlayerState()->IsABot();
	}
}

int32 ACaisseMunitions::RemplirArmes(APawn* Pion)
{
	if (!Pion)
	{
		return 0;
	}
	TSet<UObject*> Armes;
	// Inventaire du soldat FPS_Game (UT_Inventory) et arme en main
	for (TFieldIterator<FProperty> It(Pion->GetClass()); It; ++It)
	{
		const FString Nom = It->GetName();
		if (FArrayProperty* Tableau = CastField<FArrayProperty>(*It))
		{
			if (!(Nom.Contains(TEXT("Inventory")) || Nom.Contains(TEXT("Weapons"))))
			{
				continue;
			}
			if (FObjectPropertyBase* Element = CastField<FObjectPropertyBase>(Tableau->Inner))
			{
				FScriptArrayHelper Aide(Tableau, Tableau->ContainerPtrToValuePtr<void>(Pion));
				for (int32 i = 0; i < Aide.Num(); ++i)
				{
					if (UObject* O = Element->GetObjectPropertyValue(Aide.GetRawPtr(i)))
					{
						Armes.Add(O);
					}
				}
			}
		}
		else if (FObjectPropertyBase* Objet = CastField<FObjectPropertyBase>(*It))
		{
			if (Nom.Contains(TEXT("Weapon")))
			{
				if (UObject* O = Objet->GetObjectPropertyValue_InContainer(Pion))
				{
					Armes.Add(O);
				}
			}
		}
	}
	// Armes accrochees au personnage
	TArray<AActor*> Accroches;
	Pion->GetAttachedActors(Accroches);
	for (AActor* A : Accroches)
	{
		Armes.Add(A);
	}
	int32 N = 0;
	for (UObject* Arme : Armes)
	{
		if (RemplirUneArme(Arme))
		{
			++N;
		}
	}
	return N;
}

// ---------------------------------------------------------------------------
//  Une caisse
// ---------------------------------------------------------------------------
ACaisseMunitions::ACaisseMunitions()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetCullDistanceSquared = FMath::Square(20000.f); // vue a 200 m

	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	Zone->InitSphereRadius(110.f);
	Zone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Zone->SetGenerateOverlapEvents(true);
	RootComponent = Zone;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Bois(TEXT("/Game/StarterContent/Materials/M_Wood_Pine.M_Wood_Pine"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Jaune(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Caisse = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Caisse"));
	Caisse->SetupAttachment(Zone);
	Caisse->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Caisse->SetCollisionResponseToAllChannels(ECR_Ignore);
	Caisse->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Caisse->SetGenerateOverlapEvents(false);
	Caisse->SetCastShadow(false);
	Caisse->SetRelativeLocation(FVector(0.f, 0.f, -60.f));
	Caisse->SetRelativeScale3D(FVector(0.9f, 0.6f, 0.5f));
	if (Cube.Succeeded())
	{
		Caisse->SetStaticMesh(Cube.Object);
	}
	if (Bois.Succeeded())
	{
		Caisse->SetMaterial(0, Bois.Object);
	}

	Bande = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bande"));
	Bande->SetupAttachment(Caisse);
	Bande->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bande->SetCastShadow(false);
	Bande->SetRelativeScale3D(FVector(1.02f, 1.02f, 0.25f));
	if (Cube.Succeeded())
	{
		Bande->SetStaticMesh(Cube.Object);
	}
	if (Jaune.Succeeded())
	{
		Bande->SetMaterial(0, Jaune.Object);
	}

	Texte = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Texte"));
	Texte->SetupAttachment(Zone);
	Texte->SetRelativeLocation(FVector(0.f, -32.f, -40.f));
	Texte->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Texte->SetHorizontalAlignment(EHTA_Center);
	Texte->SetVerticalAlignment(EVRTA_TextCenter);
	Texte->SetWorldSize(14.f);
	Texte->SetTextRenderColor(FColor(255, 205, 40));
	Texte->SetText(FText::FromString(TEXT("MUNITIONS")));

	Texte2 = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Texte2"));
	Texte2->SetupAttachment(Zone);
	Texte2->SetRelativeLocation(FVector(0.f, 32.f, -40.f));
	Texte2->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
	Texte2->SetHorizontalAlignment(EHTA_Center);
	Texte2->SetVerticalAlignment(EVRTA_TextCenter);
	Texte2->SetWorldSize(14.f);
	Texte2->SetTextRenderColor(FColor(255, 205, 40));
	Texte2->SetText(FText::FromString(TEXT("MUNITIONS")));
}

void ACaisseMunitions::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACaisseMunitions, bVide);
}

void ACaisseMunitions::BeginPlay()
{
	Super::BeginPlay();
	const FReglagesCaisses& R = FReglagesCaisses::Lire();
	if (!R.Modele.IsEmpty())
	{
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *R.Modele))
		{
			Caisse->SetStaticMesh(M);
			Caisse->SetRelativeScale3D(FVector(1.f));
			Caisse->EmptyOverrideMaterials();
			Bande->SetVisibility(false);
		}
	}
	if (UMaterialInstanceDynamic* Couleur = Bande->CreateAndSetMaterialInstanceDynamic(0))
	{
		Couleur->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.65f, 0.02f)); // bande jaune
	}
	Zone->OnComponentBeginOverlap.AddDynamic(this, &ACaisseMunitions::AuContact);
	MontrerEtat();
}

void ACaisseMunitions::AuContact(UPrimitiveComponent* Composant, AActor* Autre, UPrimitiveComponent* AutreComposant, int32 Index, bool bBalayage, const FHitResult& Impact)
{
	APawn* Pion = Cast<APawn>(Autre);
	if (bVide || !EstJoueur(Pion))
	{
		return;
	}
	const FReglagesCaisses& R = FReglagesCaisses::Lire();
	if (HasAuthority())
	{
		// Serveur / hote / solo : il decide pour tout le monde
		const int32 N = RemplirArmes(Pion);
		AjouterArsenal(Pion, TEXT("ChargeFlammes"), R.Flammes);
		AjouterArsenal(Pion, TEXT("ChargeMitrailleuse"), R.Mitrailleuse);
		bVide = true;
		MontrerEtat();
		ForceNetUpdate();
		GetWorldTimerManager().SetTimer(Minuteur, this, &ACaisseMunitions::Revenir, R.Attente, false);
		UE_LOG(LogTemp, Log, TEXT("[Caisses] %s : %s prend des munitions (%d arme(s) remplie(s))."), *GetName(), *Pion->GetName(), N);
		if (Pion->IsLocallyControlled())
		{
			Aviser(TEXT("MUNITIONS PLEINES !"));
		}
	}
	else if (Pion->IsLocallyControlled())
	{
		// Client : on remplit aussi la copie locale des armes pour que le compteur suive tout de suite
		RemplirArmes(Pion);
		Aviser(TEXT("MUNITIONS PLEINES !"));
	}
}

void ACaisseMunitions::Revenir()
{
	bVide = false;
	MontrerEtat();
	ForceNetUpdate();
	// Un joueur deja dessus : il la reprend
	TArray<AActor*> Dessus;
	Zone->GetOverlappingActors(Dessus, APawn::StaticClass());
	for (AActor* A : Dessus)
	{
		if (!bVide)
		{
			AuContact(Zone, A, nullptr, 0, false, FHitResult());
		}
	}
}

void ACaisseMunitions::OnRep_Vide()
{
	MontrerEtat();
}

void ACaisseMunitions::MontrerEtat()
{
	const bool bVisible = !bVide;
	Caisse->SetVisibility(bVisible, true);
	Texte->SetVisibility(bVisible);
	Texte2->SetVisibility(bVisible);
	if (bVisible && !FReglagesCaisses::Lire().Modele.IsEmpty())
	{
		Bande->SetVisibility(false);
	}
}

void ACaisseMunitions::Aviser(const FString& Message) const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	for (TActorIterator<AArsenalChelles> It(W); It; ++It)
	{
		It->AvisLocal(Message);
		return;
	}
}

// ---------------------------------------------------------------------------
//  Pose des caisses (serveur)
// ---------------------------------------------------------------------------
ACaissesMunitionsGestion::ACaissesMunitionsGestion()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f;
	SetReplicates(false);
}

void ACaissesMunitionsGestion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W || bPosees || W->GetTimeSeconds() < ProchainEssai)
	{
		return;
	}
	bPosees = true;
	if (Poser() == 0 && FReglagesCaisses::Lire().bActif && ++Essais < 12)
	{
		bPosees = false; // NavMesh pas encore pret : nouvel essai
		ProchainEssai = W->GetTimeSeconds() + 5.f;
	}
}

int32 ACaissesMunitionsGestion::Poser()
{
	UWorld* W = GetWorld();
	const FReglagesCaisses& R = FReglagesCaisses::Lire();
	if (!W || !R.bActif || R.Nombre <= 0)
	{
		return 0;
	}
	int32 Deja = 0;
	for (TActorIterator<ACaisseMunitions> It(W); It; ++It)
	{
		++Deja;
	}
	if (Deja > 0)
	{
		return Deja;
	}

	// Centre : les points de depart des joueurs (sinon le premier joueur)
	FVector Centre = FVector::ZeroVector;
	int32 NbCentres = 0;
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("PlayerStart")))
		{
			Centre += It->GetActorLocation();
			++NbCentres;
		}
	}
	if (NbCentres > 0)
	{
		Centre /= NbCentres;
	}
	else if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (PC->GetPawn())
		{
			Centre = PC->GetPawn()->GetActorLocation();
			NbCentres = 1;
		}
	}
	if (NbCentres == 0)
	{
		return 0;
	}

	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CaissesMunitions), false);
	TArray<FVector> Poses;
	const int32 Tentatives = R.Nombre * 40;
	for (int32 t = 0; t < Tentatives && Poses.Num() < R.Nombre; ++t)
	{
		FVector Lieu;
		bool bOk = false;
		if (Nav)
		{
			FNavLocation Point;
			if (Nav->GetRandomPointInNavigableRadius(Centre, R.Rayon, Point))
			{
				Lieu = Point.Location;
				bOk = true;
			}
		}
		if (!bOk)
		{
			// Sans NavMesh : point au hasard puis le sol en dessous
			const FVector2D Hasard = FMath::RandPointInCircle(R.Rayon);
			const FVector Haut = Centre + FVector(Hasard.X, Hasard.Y, 5000.f);
			FHitResult Sol;
			if (W->LineTraceSingleByChannel(Sol, Haut, Haut - FVector(0.f, 0.f, 12000.f), ECC_Visibility, Params) && Sol.ImpactNormal.Z > 0.8f)
			{
				Lieu = Sol.ImpactPoint;
				bOk = true;
			}
		}
		if (!bOk)
		{
			continue;
		}
		// Recaler sur le vrai sol, refuser les pentes et les toits plats isoles
		FHitResult Sol;
		if (!W->LineTraceSingleByChannel(Sol, Lieu + FVector(0.f, 0.f, 200.f), Lieu - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params) || Sol.ImpactNormal.Z < 0.8f)
		{
			continue;
		}
		Lieu = Sol.ImpactPoint;
		bool bTropPres = false;
		for (const FVector& Autre : Poses)
		{
			if (FVector::DistSquared2D(Autre, Lieu) < FMath::Square(R.Ecart))
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FRotator Rotation(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
		if (W->SpawnActor<ACaisseMunitions>(ACaisseMunitions::StaticClass(), Lieu + FVector(0.f, 0.f, 90.f), Rotation, Spawn))
		{
			Poses.Add(Lieu);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Caisses] %d caisse(s) de munitions posee(s) (NavMesh %s, rayon %.0f m)."), Poses.Num(), Nav ? TEXT("oui") : TEXT("non"), R.Rayon / 100.f);
	return Poses.Num();
}

void ACaissesMunitionsGestion::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Nombre");
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase) || Quoi.Equals(TEXT("Reposer"), ESearchCase::IgnoreCase))
	{
		FReglagesCaisses::Lire(true);
		if (Quoi.Equals(TEXT("Reposer"), ESearchCase::IgnoreCase))
		{
			for (TActorIterator<ACaisseMunitions> It(W); It; ++It)
			{
				It->Destroy();
			}
			Poser();
		}
		return;
	}
	int32 N = 0;
	for (TActorIterator<ACaisseMunitions> It(W); It; ++It)
	{
		++N;
	}
	UE_LOG(LogTemp, Log, TEXT("[Caisses] %d caisse(s) dans la carte."), N);
}
