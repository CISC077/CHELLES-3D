#include "PassantsSpawner.h"
#include "PassantCharacter.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
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
		TEXT("attack"), TEXT("punch"), TEXT("kick"), TEXT("swim"), TEXT("climb"), TEXT("_bs"), TEXT("blend"),
		TEXT("_to_"), TEXT("transition"), TEXT("additive"), TEXT("thinking")
	};

	/** Animations a ne jamais utiliser (zombie, couche, defile, vol, fin de marche...). */
	const TCHAR* MotsInterdits[] = {
		TEXT("prone"), TEXT("zombie"), TEXT("crawl"), TEXT("injur"), TEXT("drunk"), TEXT("dead"),
		TEXT("death"), TEXT("dying"), TEXT("crouch"), TEXT("lying"), TEXT("sleep"),
		TEXT("catwalk"), TEXT("flight"), TEXT("fly"), TEXT("dance"), TEXT("_end"), TEXT("end_")
	};

	/** Personnages a ne pas utiliser comme passants (zombies, bras FPS, morceaux, corps nus, doublons). */
	bool EstPersonnageExclu(const USkeletalMesh* Mesh)
	{
		const FString Chemin = Mesh->GetPathName().ToLower();
		const TCHAR* Mots[] = {
			TEXT("zombie"), TEXT("monster"), TEXT("_arms"), TEXT("/arms/"), TEXT("_lod"), TEXT("/lods/"), TEXT("skeleton"),
			TEXT("/parts/"), TEXT("separated_mesh"), TEXT("bodyfull"), TEXT("nude"), TEXT("naked")
		};
		for (const TCHAR* Mot : Mots)
		{
			if (Chemin.Contains(Mot)) { return true; }
		}
		// Corps seul, version "base" sans tenue, version mobile en double, copies FPS / vol d'un autre personnage.
		const FString Nom = Mesh->GetName().ToLower();
		const TCHAR* Fins[] = { TEXT("_body"), TEXT("_base"), TEXT("_fps"), TEXT("_vol") };
		for (const TCHAR* Fin : Fins)
		{
			if (Nom.EndsWith(Fin)) { return true; }
		}
		return Nom.Contains(TEXT("_mobile"));
	}

	/** Taille du personnage d'apres ses os (du plus bas au plus haut, + le haut du crane). */
	float HauteurDuSquelette(const USkeletalMesh* Mesh)
	{
		const FReferenceSkeleton& Squelette = Mesh->GetRefSkeleton();
		const TArray<FTransform>& Pose = Squelette.GetRefBonePose();
		TArray<FTransform> Global;
		Global.SetNum(Pose.Num());
		float MinZ = BIG_NUMBER, MaxZ = -BIG_NUMBER;
		for (int32 i = 0; i < Pose.Num(); ++i)
		{
			const int32 Parent = Squelette.GetParentIndex(i);
			Global[i] = (Parent >= 0 && Parent < i) ? Pose[i] * Global[Parent] : Pose[i];
			const float Z = Global[i].GetLocation().Z;
			MinZ = FMath::Min(MinZ, Z);
			MaxZ = FMath::Max(MaxZ, Z);
		}
		return Pose.Num() > 1 ? (MaxZ - MinZ) + 15.f : 0.f;
	}

	/** Passants crees par image au lancement (au lieu de tous d'un coup : evite un gros a-coup). */
	constexpr int32 PassantsParLot = 6;

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
		for (const TCHAR* Mot : MotsInterdits)
		{
			if (N.Contains(Mot))
			{
				return 0; // allonge, zombie, defile, vol... : jamais pour un passant
			}
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
		if (N.Contains(TEXT("loop"))) { Note += 15; } // une boucle se repete sans a-coup
		return FMath::Max(Note, 1);
	}

	bool EstMannequinSansVetements(const USkeletalMesh* Mesh)
	{
		const FString N = Mesh->GetName().ToLower();
		return N.Contains(TEXT("mannequin")) || N.Contains(TEXT("ue4_man")) || N.Contains(TEXT("tutorialtpp"))
			|| N.Contains(TEXT("dummy"));
	}

	/** Chemin du squelette lu dans le registre d'assets, SANS charger l'asset (vide si inconnu). */
	FString SqueletteDepuisRegistre(const FAssetData& Donnee)
	{
		FString Valeur;
		if (Donnee.GetTagValue(TEXT("Skeleton"), Valeur) && !Valeur.IsEmpty() && Valeur != TEXT("None"))
		{
			return FPackageName::ExportTextPathToObjectPath(Valeur);
		}
		return FString();
	}

	struct FMeilleureAnim
	{
		int32 Note = 0;
		FAssetData Donnee;
	};
}

APassantsSpawner::APassantsSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// Imports de batiments (des milliers de faux Skeletal Mesh) et personnage a assembler en morceaux.
	DossiersIgnores = {
		TEXT("/Game/Chelles/BATFBX"), TEXT("/Game/Props/Bat07"), TEXT("/Game/Chelles/Buildings"),
		TEXT("/Game/AdvancedFemaleCustomization")
	};

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

	// Apparition etalee : quelques passants par image plutot que 40 d'un coup.
	PassantsRestantsAFaireApparaitre = NombreDePassants;
	GetWorldTimerManager().SetTimer(MinuteurApparition, this, &APassantsSpawner::FaireApparaitreUnLot, 0.05f, true, 0.f);
	if (bEndormirLoinDuJoueur)
	{
		GetWorldTimerManager().SetTimer(MinuteurEndormir, this, &APassantsSpawner::EndormirLesPassantsLointains, 0.5f, true, 1.f);
	}
}

void APassantsSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MinuteurApparition);
	GetWorldTimerManager().ClearTimer(MinuteurRemplacement);
	GetWorldTimerManager().ClearTimer(MinuteurEndormir);
	Super::EndPlay(EndPlayReason);
}

void APassantsSpawner::FaireApparaitreUnLot()
{
	for (int32 i = 0; i < PassantsParLot && PassantsRestantsAFaireApparaitre > 0; ++i, --PassantsRestantsAFaireApparaitre)
	{
		if (APassantCharacter* Passant = FaireApparaitreUnPassant())
		{
			Passants.Add(Passant);
		}
	}
	if (PassantsRestantsAFaireApparaitre > 0)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MinuteurApparition);
	Annoncer(FString::Printf(TEXT("Passants Chelles : %d passant(s) en promenade, %d personnage(s) different(s)%s."),
		Passants.Num(), Types.Num(), bTrottoirsSeulement ? TEXT(", sur les trottoirs") : TEXT("")), FColor::Green);
	if (bTrottoirsSeulement && Passants.Num() == 0)
	{
		Annoncer(FString::Printf(TEXT("Passants Chelles : aucun trottoir trouve autour de %s (acteurs dont le nom contient '%s')."),
			*GetName(), *MotCleTrottoir), FColor::Orange);
	}

	// Si un passant tombe hors du monde, on le remplace.
	GetWorldTimerManager().SetTimer(MinuteurRemplacement, this, &APassantsSpawner::RemplacerLesDisparus, 5.f, true);
}

void APassantsSpawner::PreparerTypes()
{
	Types.Reset();

	// 1) Liste complete (personnage + ses animations), remplie a la main ou par le bouton.
	for (const FTypePassant& Type : TypesDePersonnages)
	{
		if (Type.Apparence)
		{
			Types.Add(Type);
		}
	}

	// 2) Ancienne liste : personnages partageant les memes animations.
	if (Types.Num() == 0)
	{
		for (USkeletalMesh* Mesh : Personnages)
		{
			if (!Mesh) { continue; }
			FTypePassant Type;
			Type.Apparence = Mesh;
			Type.Marche = AnimationMarche;
			Type.Repos = AnimationRepos;
			Types.Add(Type);
		}
	}

	// 3) Sinon, recherche automatique dans le dossier Content.
	if (Types.Num() == 0 && bChercherPersonnagesAutomatiquement)
	{
		ChercherPersonnagesDansContent(Types);
	}

	if (Types.Num() == 0)
	{
		Annoncer(TEXT("Passants Chelles : aucun personnage anime trouve, des silhouettes colorees sont utilisees. "
			"Cliquez sur 'Remplir la liste des personnages' dans le panneau Details du Passants Spawner."), FColor::Orange);
	}
}

void APassantsSpawner::RemplirLaListeDesPersonnages()
{
	TArray<FTypePassant> Trouves;
	ChercherPersonnagesDansContent(Trouves);
	if (Trouves.Num() == 0)
	{
		Annoncer(TEXT("Passants Chelles : aucun personnage avec une animation de marche trouve dans ") + DossierDeRecherche, FColor::Orange);
		return;
	}
	Modify(); // pour Annuler (Ctrl+Z) et pour que le niveau soit marque "a enregistrer"
	TypesDePersonnages = Trouves;
	Annoncer(FString::Printf(TEXT("Passants Chelles : %d personnage(s) ajoute(s) a la liste. Enregistrez le niveau."), Trouves.Num()), FColor::Green);
}

bool APassantsSpawner::EstDansUnDossierIgnore(const FString& Chemin) const
{
	for (const FString& Dossier : DossiersIgnores)
	{
		if (!Dossier.IsEmpty() && Chemin.StartsWith(Dossier, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

void APassantsSpawner::ChercherPersonnagesDansContent(TArray<FTypePassant>& OutTypes) const
{
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// --- Les animations de marche et d'attente, rangees par squelette ---
	// Le squelette est lu dans le registre : on ne charge pas toutes les animations du projet.
	TMap<FString, FMeilleureAnim> MeilleureMarche;
	TMap<FString, FMeilleureAnim> MeilleurRepos;
	{
		FARFilter Filtre;
		Filtre.ClassNames.Add(UAnimSequence::StaticClass()->GetFName());
		Filtre.PackagePaths.Add(FName(*DossierDeRecherche));
		Filtre.bRecursivePaths = true;
		TArray<FAssetData> Anims;
		Registre.GetAssets(Filtre, Anims);

		for (const FAssetData& Donnee : Anims)
		{
			if (EstDansUnDossierIgnore(Donnee.PackagePath.ToString()))
			{
				continue;
			}
			const FString Nom = Donnee.AssetName.ToString();
			const int32 NoteMarche = NoterAnimation(Nom, true);
			const int32 NoteRepos = NoterAnimation(Nom, false);
			if (NoteMarche == 0 && NoteRepos == 0)
			{
				continue; // on ne regarde que les animations dont le nom est prometteur
			}

			FString Squelette = SqueletteDepuisRegistre(Donnee);
			if (Squelette.IsEmpty())
			{
				// Ancien asset sans l'information : il faut le charger.
				const UAnimSequence* Anim = Cast<UAnimSequence>(Donnee.GetAsset());
				const USkeleton* Skel = Anim ? Anim->GetSkeleton() : nullptr;
				if (!Skel) { continue; }
				Squelette = Skel->GetPathName();
			}

			TMap<FString, FMeilleureAnim>& Table = (NoteMarche >= NoteRepos) ? MeilleureMarche : MeilleurRepos;
			const int32 Note = FMath::Max(NoteMarche, NoteRepos);
			FMeilleureAnim& Actuelle = Table.FindOrAdd(Squelette);
			if (Actuelle.Note < Note)
			{
				Actuelle.Note = Note;
				Actuelle.Donnee = Donnee;
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
			if (EstDansUnDossierIgnore(Donnee.PackagePath.ToString()))
			{
				continue;
			}

			// Squelette sans animation de marche : inutile de charger le mesh (il resterait fige).
			const FString SqueletteRegistre = SqueletteDepuisRegistre(Donnee);
			if (!SqueletteRegistre.IsEmpty() && !MeilleureMarche.Contains(SqueletteRegistre))
			{
				continue;
			}

			USkeletalMesh* Mesh = Cast<USkeletalMesh>(Donnee.GetAsset());
			const USkeleton* Squelette = Mesh ? Mesh->GetSkeleton() : nullptr;
			if (!Squelette) { continue; }
			const FString CheminSquelette = Squelette->GetPathName();
			const FMeilleureAnim* Marche = MeilleureMarche.Find(CheminSquelette);
			if (!Marche) { continue; }

			if (EstPersonnageExclu(Mesh))
			{
				UE_LOG(LogPassants, Log, TEXT("Ignore %s (zombie, bras FPS, morceau, corps seul ou doublon)"), *Mesh->GetPathName());
				continue;
			}

			// Ecarte les morceaux (tete, cheveux, pantalon seul...) et les objets : on veut 1,30 m a 2,30 m.
			// La boite d'Unreal a une grosse marge (~2,8 m pour un humain de 1,80 m) : la taille reelle est
			// mesuree sur le squelette (pieds -> tete). La boite sert seulement a ecarter les morceaux
			// (veste, chaussures...) qui partagent le squelette du corps entier.
			const float HauteurBoite = Mesh->GetImportedBounds().BoxExtent.Z * 2.f;
			const float Hauteur = HauteurDuSquelette(Mesh);
			if (HauteurBoite < 140.f || Hauteur < 130.f || Hauteur > 230.f)
			{
				UE_LOG(LogPassants, Log, TEXT("Ignore %s (hauteur %.0f cm, boite %.0f cm)"), *Mesh->GetPathName(), Hauteur, HauteurBoite);
				continue;
			}

			FTypePassant Type;
			Type.Apparence = Mesh;
			Type.Marche = Cast<UAnimSequence>(Marche->Donnee.GetAsset());
			if (!Type.Marche) { continue; }
			const FMeilleureAnim* Repos = MeilleurRepos.Find(CheminSquelette);
			Type.Repos = Repos ? Cast<UAnimSequence>(Repos->Donnee.GetAsset()) : nullptr;

			(EstMannequinSansVetements(Mesh) ? Mannequins : Habilles).Add(Type);
			UE_LOG(LogPassants, Log, TEXT("Personnage trouve : %s (marche : %s, attente : %s)"),
				*Mesh->GetPathName(), *Type.Marche->GetName(), Type.Repos ? *Type.Repos->GetName() : TEXT("aucune"));
		}
	}

	// On prefere les personnages habilles ; les mannequins gris seulement s'il n'y a rien d'autre.
	OutTypes = Habilles;
	if (OutTypes.Num() == 0 || bGarderMannequinSansVetements)
	{
		OutTypes.Append(Mannequins);
	}
}

bool APassantsSpawner::TrouverPointSurTrottoir(FVector& OutPoint) const
{
	const FVector Centre = GetActorLocation();
	const float DemiHauteur = GetDefault<APassantCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Les trottoirs ne couvrent qu'une petite partie du sol : on essaie beaucoup de points au hasard.
	// La trace part de 6 m au-dessus du Spawner (sous les toits) et ne regarde que le decor.
	FCollisionObjectQueryParams Objets;
	Objets.AddObjectTypesToQuery(ECC_WorldStatic);
	Objets.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(TEXT("PassantApparition"), true, this);
	for (int32 Essai = 0; Essai < 80; ++Essai)
	{
		const FVector2D Hasard = FMath::RandPointInCircle(RayonDeZone);
		const FVector Haut = Centre + FVector(Hasard.X, Hasard.Y, 600.f);
		const FVector Bas = Centre + FVector(Hasard.X, Hasard.Y, -1500.f);
		FHitResult Impact;
		if (!GetWorld()->LineTraceSingleByObjectType(Impact, Haut, Bas, Objets, Params))
		{
			continue;
		}
		// Meme verification que pendant la marche (nom du trottoir, sol horizontal).
		if (APassantCharacter::EstUnTrottoir(GetWorld(), Impact.ImpactPoint, Impact.ImpactPoint.Z, MotCleTrottoir, this))
		{
			OutPoint = Impact.ImpactPoint + FVector(0.f, 0.f, DemiHauteur + 5.f);
			return true;
		}
	}
	return false;
}

bool APassantsSpawner::TrouverPointDApparition(FVector& OutPoint) const
{
	if (bTrottoirsSeulement)
	{
		return TrouverPointSurTrottoir(OutPoint);
	}

	const FVector Centre = GetActorLocation();
	const float DemiHauteur = GetDefault<APassantCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// De preference sur le NavMesh, et seulement aux endroits que l'on peut rejoindre a pied
	// depuis le Spawner (pose dans la rue) : jamais sur un toit.
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Depart;
		FNavLocation Point;
		if (Nav->ProjectPointToNavigation(Centre, Depart, FVector(500.f, 500.f, 1000.f))
			&& Nav->GetRandomReachablePointInRadius(Depart.Location, RayonDeZone, Point))
		{
			OutPoint = Point.Location + FVector(0.f, 0.f, DemiHauteur + 5.f);
			return true;
		}
	}

	// Sinon : on cherche le sol sous un point au hasard, a peu pres a la hauteur du Spawner
	// (pour ne pas faire apparaitre quelqu'un sur un toit).
	FCollisionQueryParams Params(TEXT("PassantSol"), false, this);
	for (int32 Essai = 0; Essai < 10; ++Essai)
	{
		const FVector2D Hasard = FMath::RandPointInCircle(RayonDeZone);
		const FVector Haut = Centre + FVector(Hasard.X, Hasard.Y, 600.f);
		const FVector Bas = Centre + FVector(Hasard.X, Hasard.Y, -1500.f);
		FHitResult Impact;
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

		Passant->bTrottoirsSeulement = bTrottoirsSeulement;
		Passant->MotCleTrottoir = MotCleTrottoir;

		const FTypePassant* Type = Types.Num() > 0 ? &Types[FMath::RandRange(0, Types.Num() - 1)] : nullptr;
		Passant->Configurer(
			Type ? Type->Apparence : nullptr,
			AnimationBlueprint,
			Type ? Type->Marche : nullptr,
			Type ? Type->Repos : nullptr,
			FMath::FRandRange(VitesseMin, FMath::Max(VitesseMin, VitesseMax)),
			GetActorLocation(), RayonDeZone, PauseMin, PauseMax, DistanceAffichage);
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

void APassantsSpawner::EndormirLesPassantsLointains()
{
	static const FName MarqueEndormi(TEXT("ChellesPassantEndormi"));

	// Ou sont les joueurs ? (en solo : un seul ; sur le serveur multijoueur : tous)
	TArray<FVector> Joueurs;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		FVector Oeil;
		FRotator Vue;
		PC->GetPlayerViewPoint(Oeil, Vue);
		Joueurs.Add(Oeil);
	}
	if (Joueurs.Num() == 0)
	{
		return; // menu, chargement... : on ne change rien
	}

	const float Distance = DistanceEndormir > 0.f ? DistanceEndormir : (DistanceAffichage > 0.f ? DistanceAffichage : 15000.f);
	const float DistanceReveil = Distance * 0.9f; // un peu plus pres, pour ne pas clignoter a la limite

	for (APassantCharacter* P : Passants)
	{
		if (!IsValid(P))
		{
			continue;
		}
		float Plus = TNumericLimits<float>::Max();
		for (const FVector& J : Joueurs)
		{
			Plus = FMath::Min(Plus, FVector::DistSquared2D(J, P->GetActorLocation()));
		}
		const bool bEndormi = P->Tags.Contains(MarqueEndormi);
		const bool bLoin = Plus > FMath::Square(bEndormi ? DistanceReveil : Distance);
		if (bLoin == bEndormi)
		{
			continue;
		}
		// Pause : le deplacement ne calcule plus rien et le temps du passant (et de son IA) s'arrete.
		if (UCharacterMovementComponent* Mouvement = P->GetCharacterMovement())
		{
			Mouvement->SetComponentTickEnabled(!bLoin);
			if (bLoin)
			{
				Mouvement->StopMovementImmediately();
			}
		}
		P->CustomTimeDilation = bLoin ? 0.f : 1.f;
		if (AController* IA = P->GetController())
		{
			IA->CustomTimeDilation = bLoin ? 0.f : 1.f;
		}
		if (bLoin)
		{
			P->Tags.AddUnique(MarqueEndormi);
		}
		else
		{
			P->Tags.Remove(MarqueEndormi);
		}
	}
}
