#include "PassantsSpawner.h"
#include "PassantCharacter.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPassants, Log, All);

namespace
{
	/** Mots qui indiquent une animation qu'on ne veut pas pour un passant (arme, accroupi, course...). */
	const TCHAR* MotsExclus[] = {
		TEXT("rifle"), TEXT("pistol"), TEXT("gun"), TEXT("weapon"), TEXT("aim"), TEXT("crouch"),
		TEXT("run"), TEXT("jog"), TEXT("sprint"), TEXT("bwd"), TEXT("back"), TEXT("left"), TEXT("right"),
		TEXT("strafe"), TEXT("turn"), TEXT("start"), TEXT("stop"), TEXT("jump"), TEXT("fall"), TEXT("land"),
		TEXT("injur"), TEXT("zombie"), TEXT("drunk"), TEXT("sit"), TEXT("dead"), TEXT("death"), TEXT("hit"),
		TEXT("attack"), TEXT("punch"), TEXT("kick"), TEXT("swim"), TEXT("climb"), TEXT("_bs"), TEXT("blend")
	};

	/** Note une animation : > 0 si son nom correspond, plus c'est haut mieux c'est. */
	int32 NoterAnimation(const FString& Nom, bool bMarche)
	{
		const FString N = Nom.ToLower();
		const bool bCorrespond = bMarche
			? (N.Contains(TEXT("walk")) || N.Contains(TEXT("marche")) || N.Contains(TEXT("stroll")))
			: (N.Contains(TEXT("idle")) || N.Contains(TEXT("repos")) || N.Contains(TEXT("stand")) || N.Contains(TEXT("attente")));
		if (!bCorrespond)
		{
			return 0;
		}
		int32 Note = 100 - N.Len(); // les noms courts ("Walk", "Idle") sont souvent les animations de base
		for (const TCHAR* Mot : MotsExclus)
		{
			if (N.Contains(Mot))
			{
				Note -= 60;
			}
		}
		if (bMarche && (N.Contains(TEXT("fwd")) || N.Contains(TEXT("forward")))) { Note += 10; }
		return FMath::Max(Note, 1);
	}

	bool EstMannequinSansVetements(const USkeletalMesh* Mesh)
	{
		const FString N = Mesh->GetName().ToLower();
		return N.Contains(TEXT("mannequin")) || N.Contains(TEXT("ue4_man")) || N.Contains(TEXT("tutorialtpp"));
	}
}

APassantsSpawner::APassantsSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	Zone->InitSphereRadius(RayonDeZone);
	Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zone->SetGenerateOverlapEvents(false);
	Zone->SetCanEverAffectNavigation(false);
	Zone->SetHiddenInGame(true);
	Zone->ShapeColor = FColor(60, 220, 90);
	RootComponent = Zone;

#if WITH_EDITORONLY_DATA
	Icone = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icone"));
	if (Icone)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> Sprite(TEXT("/Engine/EditorResources/S_Pawn"));
		if (Sprite.Succeeded()) { Icone->SetSprite(Sprite.Get()); }
		Icone->SetupAttachment(Zone);
		Icone->SetRelativeScale3D(FVector(2.f));
	}
#endif
}

void APassantsSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Zone->SetSphereRadius(RayonDeZone);
}

void APassantsSpawner::Annoncer(const FString& Texte, const FColor& Couleur) const
{
	UE_LOG(LogPassants, Log, TEXT("%s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, Couleur, Texte);
	}
}

void APassantsSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!bActif)
	{
		return;
	}

	PreparerTypes();

	for (int32 i = 0; i < NombreDePassants; ++i)
	{
		if (APassantCharacter* Passant = FaireApparaitreUnPassant())
		{
			Passants.Add(Passant);
		}
	}

	Annoncer(FString::Printf(TEXT("Passants Chelles : %d passant(s) en promenade, %d personnage(s) different(s)."),
		Passants.Num(), Types.Num()), FColor::Green);

	// Si un passant tombe hors du monde, on le remplace.
	GetWorldTimerManager().SetTimer(MinuteurRemplacement, this, &APassantsSpawner::RemplacerLesDisparus, 5.f, true);
}

void APassantsSpawner::PreparerTypes()
{
	Types.Reset();

	// 1) Personnages choisis a la main dans le panneau Details.
	for (USkeletalMesh* Mesh : Personnages)
	{
		if (!Mesh) { continue; }
		FTypePassant Type;
		Type.Apparence = Mesh;
		Type.Marche = AnimationMarche;
		Type.Repos = AnimationRepos;
		Types.Add(Type);
	}

	// 2) Sinon, recherche automatique dans le dossier Content.
	if (Types.Num() == 0 && bChercherPersonnagesAutomatiquement)
	{
		ChercherPersonnagesDansContent();
	}

	if (Types.Num() == 0)
	{
		Annoncer(TEXT("Passants Chelles : aucun personnage anime trouve, des silhouettes colorees sont utilisees. "
			"Remplissez 'Personnages' dans le panneau Details du Passants Spawner."), FColor::Orange);
	}
}

void APassantsSpawner::ChercherPersonnagesDansContent()
{
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// --- Les animations de marche et d'attente, rangees par squelette ---
	TMap<const USkeleton*, TPair<int32, UAnimSequence*>> MeilleureMarche;
	TMap<const USkeleton*, TPair<int32, UAnimSequence*>> MeilleurRepos;
	{
		FARFilter Filtre;
		Filtre.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		Filtre.PackagePaths.Add(FName(*DossierDeRecherche));
		Filtre.bRecursivePaths = true;
		TArray<FAssetData> Anims;
		Registre.GetAssets(Filtre, Anims);

		for (const FAssetData& Donnee : Anims)
		{
			const FString Nom = Donnee.AssetName.ToString();
			const int32 NoteMarche = NoterAnimation(Nom, true);
			const int32 NoteRepos = NoterAnimation(Nom, false);
			if (NoteMarche == 0 && NoteRepos == 0)
			{
				continue; // on ne charge que les animations dont le nom est prometteur
			}
			UAnimSequence* Anim = Cast<UAnimSequence>(Donnee.GetAsset());
			const USkeleton* Squelette = Anim ? Anim->GetSkeleton() : nullptr;
			if (!Squelette) { continue; }

			TMap<const USkeleton*, TPair<int32, UAnimSequence*>>& Table = (NoteMarche >= NoteRepos) ? MeilleureMarche : MeilleurRepos;
			const int32 Note = FMath::Max(NoteMarche, NoteRepos);
			TPair<int32, UAnimSequence*>* Actuelle = Table.Find(Squelette);
			if (!Actuelle || Actuelle->Key < Note)
			{
				Table.Add(Squelette, TPair<int32, UAnimSequence*>(Note, Anim));
			}
		}
	}

	// --- Les personnages complets (taille humaine) qui ont une animation de marche ---
	TArray<FTypePassant> Habilles;
	TArray<FTypePassant> Mannequins;
	{
		FARFilter Filtre;
		Filtre.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());
		Filtre.PackagePaths.Add(FName(*DossierDeRecherche));
		Filtre.bRecursivePaths = true;
		TArray<FAssetData> Meshes;
		Registre.GetAssets(Filtre, Meshes);

		for (const FAssetData& Donnee : Meshes)
		{
			USkeletalMesh* Mesh = Cast<USkeletalMesh>(Donnee.GetAsset());
			const USkeleton* Squelette = Mesh ? Mesh->GetSkeleton() : nullptr;
			const TPair<int32, UAnimSequence*>* Marche = Squelette ? MeilleureMarche.Find(Squelette) : nullptr;
			if (!Marche)
			{
				continue; // pas de squelette ou pas d'animation de marche : il resterait fige
			}

			// Ecarte les morceaux (tete, cheveux, pantalon seul...) et les objets : on veut 1,40 m a 2,20 m.
			const float Hauteur = Mesh->GetBounds().BoxExtent.Z * 2.f;
			if (Hauteur < 140.f || Hauteur > 220.f)
			{
				UE_LOG(LogPassants, Log, TEXT("Ignore %s (hauteur %.0f cm)"), *Mesh->GetPathName(), Hauteur);
				continue;
			}

			FTypePassant Type;
			Type.Apparence = Mesh;
			Type.Marche = Marche->Value;
			const TPair<int32, UAnimSequence*>* Repos = MeilleurRepos.Find(Squelette);
			Type.Repos = Repos ? Repos->Value : nullptr;

			(EstMannequinSansVetements(Mesh) ? Mannequins : Habilles).Add(Type);
			UE_LOG(LogPassants, Log, TEXT("Personnage trouve : %s (marche : %s, attente : %s)"),
				*Mesh->GetPathName(), *Type.Marche->GetName(), Type.Repos ? *Type.Repos->GetName() : TEXT("aucune"));
		}
	}

	// On prefere les personnages habilles ; le mannequin gris seulement s'il n'y a rien d'autre.
	Types = Habilles;
	if (Types.Num() == 0 || bGarderMannequinSansVetements)
	{
		Types.Append(Mannequins);
	}
}

bool APassantsSpawner::TrouverPointDApparition(FVector& OutPoint) const
{
	const FVector Centre = GetActorLocation();
	const float DemiHauteur = GetDefault<APassantCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// De preference sur le NavMesh (rues et trottoirs ou l'on peut marcher).
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Point;
		if (Nav->GetRandomPointInNavigableRadius(Centre, RayonDeZone, Point))
		{
			OutPoint = Point.Location + FVector(0.f, 0.f, DemiHauteur + 5.f);
			return true;
		}
	}

	// Sinon : on cherche le sol sous un point au hasard, a peu pres a la hauteur du Spawner
	// (pour ne pas faire apparaitre quelqu'un sur un toit).
	for (int32 Essai = 0; Essai < 10; ++Essai)
	{
		const FVector2D Hasard = FMath::RandPointInCircle(RayonDeZone);
		const FVector Haut = Centre + FVector(Hasard.X, Hasard.Y, 600.f);
		const FVector Bas = Centre + FVector(Hasard.X, Hasard.Y, -1500.f);
		FHitResult Impact;
		FCollisionQueryParams Params(TEXT("PassantSol"), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Impact, Haut, Bas, ECC_Visibility, Params)
			&& Impact.ImpactNormal.Z > 0.8f
			&& FMath::Abs(Impact.ImpactPoint.Z - Centre.Z) < 400.f)
		{
			OutPoint = Impact.ImpactPoint + FVector(0.f, 0.f, DemiHauteur + 5.f);
			return true;
		}
	}
	return false;
}

APassantCharacter* APassantsSpawner::FaireApparaitreUnPassant()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	for (int32 Essai = 0; Essai < 6; ++Essai)
	{
		FVector Point;
		if (!TrouverPointDApparition(Point))
		{
			continue;
		}
		const FRotator Orientation(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
		APassantCharacter* Passant = GetWorld()->SpawnActor<APassantCharacter>(APassantCharacter::StaticClass(), Point, Orientation, Params);
		if (!Passant)
		{
			continue;
		}

		const FTypePassant* Type = Types.Num() > 0 ? &Types[FMath::RandRange(0, Types.Num() - 1)] : nullptr;
		Passant->Configurer(
			Type ? Type->Apparence : nullptr,
			AnimationBlueprint,
			Type ? Type->Marche : nullptr,
			Type ? Type->Repos : nullptr,
			FMath::FRandRange(VitesseMin, FMath::Max(VitesseMin, VitesseMax)),
			GetActorLocation(), RayonDeZone, PauseMin, PauseMax);
		return Passant;
	}
	return nullptr;
}

void APassantsSpawner::RemplacerLesDisparus()
{
	Passants.RemoveAll([](const APassantCharacter* P) { return !IsValid(P); });
	// Un ou deux par passage, pour ne pas faire "pop" tout le monde d'un coup.
	for (int32 i = 0; i < 2 && Passants.Num() < NombreDePassants; ++i)
	{
		if (APassantCharacter* Passant = FaireApparaitreUnPassant())
		{
			Passants.Add(Passant);
		}
		else
		{
			break;
		}
	}
}
