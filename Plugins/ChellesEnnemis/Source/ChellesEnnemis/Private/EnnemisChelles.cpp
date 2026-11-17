// CHELLES 3D - Ennemis des dossiers FPS_Game/Blueprints/Bonus/AI (voir EnnemisChelles.h)
//
// On n'ecrit RIEN dans les assets du projet : tout est fait pendant le jeu.
//  - Le serveur (ou le jeu solo) trouve les Blueprints d'ennemis avec l'Asset Registry, pose
//    NombreParDossier ennemis de chaque dossier au niveau de la rue et remplace ceux qui meurent.
//  - Les personnages du dossier Other sont diriges par ce plugin (IA simple, comme les zombies de
//    l'invasion) : ils reperent, poursuivent et frappent le joueur vivant le plus proche.
//  - Chaque PC habille les ennemis qui ont encore le mannequin gris.
#include "EnnemisChelles.h"

#include <initializer_list>

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Animation/Skeleton.h"
#include "UObject/UnrealType.h"

#define CHEMIN_MONTAGE_ZOMBIE TEXT("/Game/FPS_Game/Character/TPP/AnimStarterPack/Zombie/ZombieAttackMontage.ZombieAttackMontage")
#define CHEMIN_SANG TEXT("/Game/FPS_Game/Particles/P_body_bullet_NoStop.P_body_bullet_NoStop")
// Ennemi FPS_Game de reference : ses reglages de collision sont copies sur les ennemis Other
#define CHEMIN_ENNEMI_MODELE TEXT("/Game/FPS_Game/Blueprints/Bonus/AI/AI_Soldier.AI_Soldier_C")

const FName AEnnemisChelles::MarqueEnnemiAuto(TEXT("ChellesEnnemiAuto"));

static const FName MarqueVu(TEXT("ChellesEnnemiVu"));
static const FName MarquePose(TEXT("ChellesEnnemiPose"));
static const FName NomTenue(TEXT("TenueEnnemiChelles"));
// Marques du plugin ChellesZombies : ses zombies sont deja habilles et diriges par lui
static const FName MarqueZombieInvasion(TEXT("ChellesZombie"));
static const FName MarqueZombieInvasionHabille(TEXT("ChellesZombieHabille"));
static const FName NomPeauInvasion(TEXT("PeauZombieChelles"));
static const FName MarqueJoueurHorsJeu(TEXT("ChellesHorsJeu"));

static const FName DossierCharger(TEXT("Charger"));
static const FName DossierOther(TEXT("Other"));
static const FName DossierPatrol(TEXT("Patrol"));
static const FName DossierSoldier(TEXT("Soldier"));
static const FName DossierZombie(TEXT("Zombie"));

namespace
{
	const TArray<FName>& LesDossiers()
	{
		static const TArray<FName> Liste = { DossierCharger, DossierOther, DossierPatrol, DossierSoldier, DossierZombie };
		return Liste;
	}

	// --------- Lecture / ecriture des variables des Blueprints, sans les modifier ---------

	FProperty* TrouverVariable(UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			for (TFieldIterator<FProperty> It(Objet->GetClass()); It; ++It)
			{
				if (It->GetName().Equals(Nom, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	bool LireNombre(UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(TrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? (float)P->GetFloatingPointPropertyValue(Adresse) : (float)P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	bool LireBooleen(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(TrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	void EcrireBooleen(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool Valeur)
	{
		if (FBoolProperty* P = CastField<FBoolProperty>(TrouverVariable(Objet, Noms)))
		{
			P->SetPropertyValue_InContainer(Objet, Valeur);
		}
	}

	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesEnnemis"), TEXT("Ennemis.txt"));
	}

	bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	/** "/Game/Dossier/Objet" -> "/Game/Dossier/Objet.Objet" (chemin complet d'un asset). */
	FString CheminComplet(const FString& Chemin)
	{
		if (Chemin.IsEmpty() || Chemin.Contains(TEXT(".")))
		{
			return Chemin;
		}
		return Chemin + TEXT(".") + FPaths::GetBaseFilename(Chemin);
	}

	/** Nom du Blueprint sans le "_C" de sa classe. */
	FString NomBlueprint(const UClass* Classe)
	{
		FString Nom = Classe ? Classe->GetName() : FString();
		Nom.RemoveFromEnd(TEXT("_C"));
		return Nom;
	}

	bool EstUnMannequin(const USkeletalMeshComponent* Corps)
	{
		return Corps && Corps->SkeletalMesh && Corps->SkeletalMesh->GetName().StartsWith(TEXT("SK_Mannequin"), ESearchCase::IgnoreCase);
	}

	bool AUneTenue(ACharacter* Ennemi)
	{
		TArray<USkeletalMeshComponent*> Composants;
		Ennemi->GetComponents<USkeletalMeshComponent>(Composants);
		for (USkeletalMeshComponent* C : Composants)
		{
			if (C && (C->GetFName() == NomTenue || C->GetFName() == NomPeauInvasion))
			{
				return true;
			}
		}
		return false;
	}

	bool MontageCompatible(UAnimMontage* Montage, ACharacter* Ennemi)
	{
		USkeletalMeshComponent* Corps = Ennemi ? Ennemi->GetMesh() : nullptr;
		return Montage && Corps && Corps->SkeletalMesh && Montage->GetSkeleton() && Montage->GetSkeleton() == Corps->SkeletalMesh->Skeleton;
	}

	bool NomDAttaque(const FString& Nom)
	{
		static const TCHAR* const Mots[] = { TEXT("attack"), TEXT("attaque"), TEXT("punch"), TEXT("melee"), TEXT("bite"), TEXT("strike"), TEXT("hit"), TEXT("swing") };
		for (const TCHAR* Mot : Mots)
		{
			if (Nom.Contains(Mot, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

AEnnemisChelles::AEnnemisChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 2.f;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AEnnemisChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEnnemisChelles, TenuesChoisies);
}

bool AEnnemisChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

// ===================================================================================
//  Demarrage et reglages
// ===================================================================================

void AEnnemisChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	if (!Reglages.bActif)
	{
		UE_LOG(LogTemp, Log, TEXT("[Ennemis] Desactive (Actif=Non dans Ennemis.txt)."));
		SetActorTickEnabled(false);
		return;
	}

	MontageZombie = LoadObject<UAnimMontage>(nullptr, CHEMIN_MONTAGE_ZOMBIE, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (GetNetMode() != NM_DedicatedServer && Reglages.bHabiller)
	{
		MeshBandit = LoadObject<USkeletalMesh>(nullptr, *CheminComplet(Reglages.MeshBandit), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (const FString& Chemin : Reglages.Personnes)
		{
			USkeletalMesh* Personne = LoadObject<USkeletalMesh>(nullptr, *CheminComplet(Chemin), nullptr, LOAD_NoWarn | LOAD_Quiet);
			MeshesPersonnes.Add(Personne); // nul si introuvable : on garde la place pour que les numeros restent les memes
			if (!Personne)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Ennemis] Personne introuvable : %s"), *Chemin);
			}
		}
		PeauZombie = LoadObject<USkeletalMesh>(nullptr, *CheminComplet(Reglages.PeauZombie), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!MeshBandit)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Ennemis] Tenue de bandit introuvable : %s"), *Reglages.MeshBandit);
		}
		if (!PeauZombie)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Ennemis] Peau de zombie introuvable : %s"), *Reglages.PeauZombie);
		}
	}

	if (EstServeur())
	{
		TrouverLesClasses();
		PoseA = GetWorld()->GetTimeSeconds() + 3.f; // le temps que la carte soit bien chargee
	}
}

void AEnnemisChelles::ChargerReglages()
{
	const FString Chemin = CheminReglages();
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[Ennemis] %s absent : reglages par defaut."), *Chemin);
	}


	bool bPersonnesLues = false;
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		const float Nombre = FCString::Atof(*Valeur);

		if (Cle.StartsWith(TEXT("Nombre."), ESearchCase::IgnoreCase))
		{
			const FString Dossier = Cle.Mid(7);
			for (const FName& D : LesDossiers())
			{
				if (D.ToString().Equals(Dossier, ESearchCase::IgnoreCase))
				{
					Reglages.NombreDuDossier.Add(D, FMath::Clamp(FCString::Atoi(*Valeur), 0, 200));
				}
			}
		}
		else if (Cle.StartsWith(TEXT("Tenue."), ESearchCase::IgnoreCase))
		{
			Reglages.Tenues.Add(Cle.Mid(6), Valeur);
		}
		else if (Cle == TEXT("Actif")) Reglages.bActif = EstOui(Valeur);
		else if (Cle == TEXT("NombreParDossier")) Reglages.NombreParDossier = FMath::Clamp(FCString::Atoi(*Valeur), 0, 200);
		else if (Cle == TEXT("ReapparitionSecondes")) Reglages.ReapparitionSecondes = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("EspacementMetres")) Reglages.EspacementMetres = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("LoinDesJoueursMetres")) Reglages.LoinDesJoueursMetres = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("EcartNiveauRueMetres")) Reglages.EcartNiveauRueMetres = FMath::Max(1.f, Nombre);
		else if (Cle == TEXT("DossierIA")) { Reglages.DossierIA = Valeur; Reglages.DossierIA.RemoveFromEnd(TEXT("/")); }
		else if (Cle == TEXT("DossierAuto")) { Reglages.DossierAuto = Valeur; Reglages.DossierAuto.RemoveFromEnd(TEXT("/")); }
		else if (Cle == TEXT("Exclus"))
		{
			TArray<FString> Noms;
			Valeur.ParseIntoArray(Noms, TEXT(","), true);
			for (FString& N : Noms)
			{
				N.TrimStartAndEndInline();
				Reglages.Exclus.AddUnique(N);
			}
		}
		else if (Cle == TEXT("DistanceDetectionMetres")) Reglages.DistanceDetectionMetres = FMath::Max(1.f, Nombre);
		else if (Cle == TEXT("DistanceAbandonMetres")) Reglages.DistanceAbandonMetres = FMath::Max(1.f, Nombre);
		else if (Cle == TEXT("DegatsPourcent")) Reglages.DegatsPourcent = FMath::Clamp(Nombre, 0.f, 100.f);
		else if (Cle == TEXT("TempsEntreCoups")) Reglages.TempsEntreCoups = FMath::Max(0.3f, Nombre);
		else if (Cle == TEXT("DegatsCorpsPourcent")) Reglages.DegatsCorpsPourcent = FMath::Clamp(Nombre, 1.f, 100.f);
		else if (Cle == TEXT("TeteTue")) Reglages.bTeteTue = EstOui(Valeur);
		else if (Cle == TEXT("VitesseCourse")) Reglages.VitesseCourse = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("Promenade")) Reglages.bPromenade = EstOui(Valeur);
		else if (Cle == TEXT("ZombiesTuables")) Reglages.bZombiesTuables = EstOui(Valeur);
		else if (Cle == TEXT("Habiller")) Reglages.bHabiller = EstOui(Valeur);
		else if (Cle == TEXT("RemplacerBandits")) Reglages.bRemplacerBandits = EstOui(Valeur);
		else if (Cle == TEXT("Personne"))
		{
			if (!bPersonnesLues)
			{
				Reglages.Personnes.Reset(); // la liste du fichier remplace celle par defaut
				bPersonnesLues = true;
			}
			Reglages.Personnes.Add(Valeur);
		}
		else if (Cle == TEXT("MeshBandit")) Reglages.MeshBandit = Valeur;
		else if (Cle == TEXT("PeauZombie")) Reglages.PeauZombie = Valeur;
		else if (Cle == TEXT("TenueParDefaut")) Reglages.TenueParDefaut = Valeur;
	}
	// La detection ne peut pas etre plus loin que l'abandon
	Reglages.DistanceAbandonMetres = FMath::Max(Reglages.DistanceAbandonMetres, Reglages.DistanceDetectionMetres + 5.f);
}

// ===================================================================================
//  Trouver les ennemis des dossiers (Asset Registry : marche aussi pour un pack ajoute plus tard)
// ===================================================================================

FName AEnnemisChelles::DossierDe(const UClass* Classe) const
{
	const FString Auto = Reglages.DossierAuto + TEXT("/");
	const FString Racine = Reglages.DossierIA + TEXT("/");
	// On remonte les parents : AI_Soldier (a la racine de AI) -> BP_AI_CharSoldier (dans Soldier)
	for (const UClass* K = Classe; K; K = K->GetSuperClass())
	{
		const FString Paquet = K->GetOutermost()->GetName();
		if (Paquet.StartsWith(Auto, ESearchCase::IgnoreCase))
		{
			return DossierOther;
		}
		if (!Paquet.StartsWith(Racine, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FString Premier, Reste;
		if (Paquet.Mid(Racine.Len()).Split(TEXT("/"), &Premier, &Reste))
		{
			for (const FName& D : LesDossiers())
			{
				if (D.ToString().Equals(Premier, ESearchCase::IgnoreCase))
				{
					return D;
				}
			}
		}
	}
	return NAME_None;
}

void AEnnemisChelles::TrouverLesClasses()
{
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
#if WITH_EDITOR
	// Dans l'editeur : on relit le dossier pour voir aussi un Blueprint depose a l'instant
	TArray<FString> AScanner;
	AScanner.Add(Reglages.DossierIA);
	if (!Reglages.DossierAuto.StartsWith(Reglages.DossierIA))
	{
		AScanner.Add(Reglages.DossierAuto);
	}
	Registre.ScanPathsSynchronous(AScanner, true);
#endif

	TArray<FAssetData> Assets;
	Registre.GetAssetsByPath(FName(*Reglages.DossierIA), Assets, true);
	if (!Reglages.DossierAuto.StartsWith(Reglages.DossierIA))
	{
		Registre.GetAssetsByPath(FName(*Reglages.DossierAuto), Assets, true);
	}

	TMap<FName, TArray<UClass*>> ARacine;      // ex. AI_Soldier, AI_Genesis (les vrais ennemis du pack)
	TMap<FName, TArray<UClass*>> DansDossier;  // ex. BP_AI_CharSoldier (leurs parents)
	for (const FAssetData& A : Assets)
	{
		const FString Nom = A.AssetName.ToString();
		if (Reglages.Exclus.ContainsByPredicate([&Nom](const FString& E) { return Nom.Equals(E, ESearchCase::IgnoreCase); }))
		{
			continue; // vieille copie cassee du pack : on ne la charge jamais
		}
		const FString SorteAsset = A.AssetClass.ToString();
		FString CheminClasse;
		if (SorteAsset == TEXT("BlueprintGeneratedClass"))
		{
			CheminClasse = A.ObjectPath.ToString();
		}
		else if (SorteAsset.Contains(TEXT("Blueprint")))
		{
			// Ne pas charger ce qui n'est pas un personnage (animations, widgets, objets...)
			FString ParentNatif;
			if (A.GetTagValue(FName(TEXT("NativeParentClass")), ParentNatif) && !ParentNatif.Contains(TEXT("Character")))
			{
				continue;
			}
			CheminClasse = A.ObjectPath.ToString() + TEXT("_C");
		}
		else
		{
			continue;
		}

		UClass* Classe = LoadObject<UClass>(nullptr, *CheminClasse, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Classe || !Classe->IsChildOf(ACharacter::StaticClass())
			|| Classe->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
			|| Classe->GetName().StartsWith(TEXT("SKEL_")) || Classe->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		const FName Dossier = DossierDe(Classe);
		if (Dossier == NAME_None)
		{
			continue;
		}
		ClassesGardees.AddUnique(Classe);
		const FString CheminPaquet = A.PackagePath.ToString();
		if (Dossier == DossierOther || CheminPaquet.Equals(Reglages.DossierIA, ESearchCase::IgnoreCase))
		{
			ARacine.FindOrAdd(Dossier).AddUnique(Classe);
		}
		else
		{
			DansDossier.FindOrAdd(Dossier).AddUnique(Classe);
		}
	}

	// Pour chaque dossier : les ennemis "finis" de la racine de AI s'il y en a, sinon ceux du dossier
	for (const FName& D : LesDossiers())
	{
		TArray<UClass*>* Liste = ARacine.Find(D);
		if (!Liste || Liste->Num() == 0)
		{
			Liste = DansDossier.Find(D);
		}
		if (Liste && Liste->Num() > 0)
		{
			ClassesParDossier.Add(D, *Liste);
		}
		FString Noms;
		if (Liste)
		{
			for (UClass* C : *Liste)
			{
				Noms += (Noms.IsEmpty() ? TEXT("") : TEXT(", ")) + NomBlueprint(C);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Ennemis] Dossier %s : %s"), *D.ToString(), Noms.IsEmpty() ? TEXT("(aucun personnage)") : *Noms);
	}
}

// ===================================================================================
//  Eparpiller les ennemis dans la carte (serveur)
// ===================================================================================

void AEnnemisChelles::PreparerPlaces()
{
	bPlacesPretes = true;
	UWorld* W = GetWorld();

	// La zone de jeu = le volume du NavMesh de la carte (NavMesh_Chelles), sinon autour des departs
	FBox Zone(ForceInit);
	for (TActorIterator<ANavMeshBoundsVolume> It(W); It; ++It)
	{
		Zone += It->GetComponentsBoundingBox(true);
	}
	if (!Zone.IsValid)
	{
		for (TActorIterator<APlayerStart> It(W); It; ++It)
		{
			Zone += FBox::BuildAABB(It->GetActorLocation(), FVector(40000.f, 40000.f, 5000.f));
		}
	}
	if (!Zone.IsValid)
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			if (APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
			{
				Zone += FBox::BuildAABB(P->GetActorLocation(), FVector(40000.f, 40000.f, 5000.f));
			}
		}
	}
	if (!Zone.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ennemis] Impossible de trouver la zone de jeu : aucun ennemi pose."));
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesEnnemisSol), false);
	for (TActorIterator<APawn> It(W); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	TArray<FVector> Sols;
	const int32 Tirs = 2500;
	for (int32 i = 0; i < Tirs; ++i)
	{
		const float X = FMath::FRandRange(Zone.Min.X, Zone.Max.X);
		const float Y = FMath::FRandRange(Zone.Min.Y, Zone.Max.Y);
		FHitResult Sol;
		if (!W->LineTraceSingleByChannel(Sol, FVector(X, Y, Zone.Max.Z + 5000.f), FVector(X, Y, Zone.Min.Z - 5000.f), ECC_Visibility, Params))
		{
			continue;
		}
		if (Sol.ImpactNormal.Z < 0.75f)
		{
			continue;
		}
		const AActor* Touche = Sol.GetActor();
		const FString NomTouche = Touche ? Touche->GetName().ToLower() : FString();
		if (NomTouche.Contains(TEXT("sky")) || NomTouche.Contains(TEXT("water")) || NomTouche.Contains(TEXT("cloud")))
		{
			continue;
		}
		Sols.Add(Sol.ImpactPoint);
	}
	if (Sols.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ennemis] Aucun sol trouve dans la zone de jeu : aucun ennemi pose."));
		return;
	}

	// Niveau de la rue : la plupart des tirs tombent dans la rue, les toits sont plus hauts
	TArray<float> Hauteurs;
	for (const FVector& S : Sols)
	{
		Hauteurs.Add(S.Z);
	}
	Hauteurs.Sort();
	const float NiveauRue = Hauteurs[FMath::Clamp(FMath::FloorToInt(Hauteurs.Num() * 0.3f), 0, Hauteurs.Num() - 1)];
	const float Ecart = Reglages.EcartNiveauRueMetres * 100.f;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(W);
	int32 SurNavMesh = 0;
	TArray<FVector> HorsNavMesh;
	for (const FVector& S : Sols)
	{
		if (FMath::Abs(S.Z - NiveauRue) > Ecart)
		{
			continue;
		}
		FNavLocation SurNav;
		if (Nav && Nav->ProjectPointToNavigation(S, SurNav, FVector(150.f, 150.f, 250.f)))
		{
			Places.Add(SurNav.Location);
			++SurNavMesh;
		}
		else
		{
			HorsNavMesh.Add(S);
		}
	}
	// Pas de NavMesh construit : on garde les points de la rue tels quels
	if (Places.Num() == 0)
	{
		Places = HorsNavMesh;
	}
	UE_LOG(LogTemp, Log, TEXT("[Ennemis] Zone de jeu %.0f x %.0f m, niveau de la rue ~ %.1f m : %d places (%d sur le NavMesh)."),
		Zone.GetSize().X / 100.f, Zone.GetSize().Y / 100.f, NiveauRue / 100.f, Places.Num(), SurNavMesh);
}

bool AEnnemisChelles::ChoisirPlace(const ACharacter* Modele, bool bLoinDesJoueurs, FVector& Resultat)
{
	if (Places.Num() == 0)
	{
		return false;
	}
	UWorld* W = GetWorld();
	const UCapsuleComponent* Capsule = Modele ? Modele->GetCapsuleComponent() : nullptr;
	const float DemiHauteur = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	const float Rayon = Capsule ? Capsule->GetScaledCapsuleRadius() : 40.f;

	TArray<FVector> Deja;
	for (const FEnnemiPose& P : Poses)
	{
		if (P.Pion.IsValid())
		{
			Deja.Add(P.Pion->GetActorLocation());
		}
	}
	TArray<FVector> Joueurs;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Joueurs.Add(P->GetActorLocation());
		}
	}
	for (TActorIterator<APlayerStart> It(W); It; ++It)
	{
		Joueurs.Add(It->GetActorLocation());
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesEnnemisPlace), false);
	for (int32 Essai = 0; Essai < 80; ++Essai)
	{
		// Si la place manque, on se serre un peu
		const float Facteur = Essai < 40 ? 1.f : (Essai < 60 ? 0.5f : 0.2f);
		const float Espacement = Reglages.EspacementMetres * 100.f * Facteur;
		const float Loin = Reglages.LoinDesJoueursMetres * 100.f * Facteur;
		const FVector Sol = Places[FMath::RandRange(0, Places.Num() - 1)];

		bool bOk = true;
		for (const FVector& D : Deja)
		{
			if (FVector::Dist2D(D, Sol) < Espacement)
			{
				bOk = false;
				break;
			}
		}
		if (bOk && bLoinDesJoueurs)
		{
			for (const FVector& J : Joueurs)
			{
				if (FVector::Dist2D(J, Sol) < Loin)
				{
					bOk = false;
					break;
				}
			}
		}
		if (!bOk)
		{
			continue;
		}
		const FVector Centre = Sol + FVector(0.f, 0.f, DemiHauteur + 5.f);
		if (W->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Rayon, DemiHauteur), Params))
		{
			continue; // dans un mur, une voiture...
		}
		Resultat = Centre;
		return true;
	}
	return false;
}

ACharacter* AEnnemisChelles::PoserEnnemi(FName Dossier, bool bLoinDesJoueurs)
{
	const TArray<UClass*>* Classes = ClassesParDossier.Find(Dossier);
	if (!Classes || Classes->Num() == 0)
	{
		return nullptr;
	}
	UClass* Classe = (*Classes)[FMath::RandRange(0, Classes->Num() - 1)];
	const ACharacter* Modele = Cast<ACharacter>(Classe->GetDefaultObject());
	FVector Centre;
	if (!ChoisirPlace(Modele, bLoinDesJoueurs, Centre))
	{
		return nullptr;
	}

	const FTransform Ou(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Centre);
	ACharacter* E = GetWorld()->SpawnActorDeferred<ACharacter>(Classe, Ou, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!E)
	{
		return nullptr;
	}
	if (Dossier == DossierOther)
	{
		// Controleur IA simple : c'est ce plugin qui le dirige (le Blueprint d'un autre pack ne sait pas qui sont les joueurs)
		E->AIControllerClass = AAIController::StaticClass();
	}
	E->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	E->Tags.AddUnique(MarquePose);
	// Patrouilleur : des points de patrouille avant son BeginPlay (sinon TargetPointArray[0] est vide)
	if (RemplirPatrouille(E))
	{
		++PatrouillesRemplies;
	}
	UGameplayStatics::FinishSpawningActor(E, Ou);
	if (!IsValid(E))
	{
		return nullptr;
	}
	if (!E->GetController())
	{
		E->SpawnDefaultController();
	}
	if (Dossier == DossierOther)
	{
		PrendreEnMain(E);
	}

	FEnnemiPose Pose;
	Pose.Pion = E;
	Pose.Dossier = Dossier;
	Poses.Add(Pose);
	return E;
}

void AEnnemisChelles::PoserTout()
{
	if (Places.Num() == 0)
	{
		return;
	}
	FString Bilan;
	int32 Total = 0;
	PatrouillesRemplies = 0;
	for (const FName& D : LesDossiers())
	{
		const int32 Voulus = Reglages.NombrePour(D);
		int32 Poses_ = 0;
		if (ClassesParDossier.Contains(D))
		{
			for (int32 i = 0; i < Voulus; ++i)
			{
				if (PoserEnnemi(D, true))
				{
					++Poses_;
				}
			}
		}
		Total += Poses_;
		Bilan += FString::Printf(TEXT("%s%s %d/%d"), Bilan.IsEmpty() ? TEXT("") : TEXT(", "), *D.ToString(), Poses_, Voulus);
	}
	UE_LOG(LogTemp, Log, TEXT("[Ennemis] %d ennemis eparpilles dans la carte (%s). Console : Chelles.Ennemis"), Total, *Bilan);
	if (PatrouillesRemplies > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Ennemis] %d patrouilleurs sans points : points de patrouille ajoutes"), PatrouillesRemplies);
	}
}

bool AEnnemisChelles::RemplirPatrouille(AActor* Acteur)
{
	if (!IsValid(Acteur))
	{
		return false;
	}
	UWorld* W = Acteur->GetWorld();
	if (!W)
	{
		return false;
	}
	static const FName NomTableau(TEXT("TargetPointArray"));
	FArrayProperty* Tableau = FindFProperty<FArrayProperty>(Acteur->GetClass(), NomTableau);
	if (!Tableau)
	{
		return false; // pas un patrouilleur
	}
	FObjectProperty* Interieur = CastField<FObjectProperty>(Tableau->Inner);
	if (!Interieur || !Interieur->PropertyClass)
	{
		return false;
	}
	if (!ATargetPoint::StaticClass()->IsChildOf(Interieur->PropertyClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ennemis] %s : TargetPointArray attend des %s, pas des TargetPoint : rien ajoute."),
			*Acteur->GetName(), *Interieur->PropertyClass->GetName());
		return false;
	}

	FScriptArrayHelper Aide(Tableau, Tableau->ContainerPtrToValuePtr<void>(Acteur));
	for (int32 i = 0; i < Aide.Num(); ++i)
	{
		if (IsValid(Interieur->GetObjectPropertyValue(Aide.GetRawPtr(i))))
		{
			return false; // deja au moins un point : on ne touche a rien
		}
	}

	// 3 points autour du patrouilleur : sur le NavMesh si possible, sinon a 6-10 m dans 3 directions
	const FVector Origine = Acteur->GetActorLocation();
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(W);
	const float AngleDepart = FMath::FRandRange(0.f, 360.f);
	TArray<FVector> Points;
	for (int32 i = 0; i < 3; ++i)
	{
		FNavLocation SurNav;
		if (Nav && Nav->GetRandomReachablePointInRadius(Origine, 1200.f, SurNav)
			&& FVector::Dist2D(SurNav.Location, Origine) > 200.f)
		{
			Points.Add(SurNav.Location + FVector(0.f, 0.f, 50.f));
			continue;
		}
		const float Angle = FMath::DegreesToRadians(AngleDepart + 120.f * i);
		const float Distance = FMath::FRandRange(600.f, 1000.f);
		Points.Add(Origine + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.f));
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TArray<ATargetPoint*> Nouveaux;
	for (const FVector& P : Points)
	{
		if (ATargetPoint* Point = W->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), FTransform(P), Params))
		{
			Nouveaux.Add(Point);
		}
	}
	if (Nouveaux.Num() == 0)
	{
		return false;
	}
	Aide.EmptyValues();
	const int32 Premier = Aide.AddValues(Nouveaux.Num());
	for (int32 i = 0; i < Nouveaux.Num(); ++i)
	{
		Interieur->SetObjectPropertyValue(Aide.GetRawPtr(Premier + i), Nouveaux[i]);
	}
	return true;
}

void AEnnemisChelles::AjouterUneVague()
{
	if (!bPlacesPretes)
	{
		PreparerPlaces();
	}
	bPoseFaite = true;
	PoserTout();
}

void AEnnemisChelles::SurveillerLesPoses(float Maintenant)
{
	TArray<FName> ARemplacer;
	for (int32 i = Poses.Num() - 1; i >= 0; --i)
	{
		FEnnemiPose& P = Poses[i];
		if (!EstMort(P.Pion.Get()))
		{
			continue;
		}
		if (P.MortDepuis < 0.f)
		{
			P.MortDepuis = Maintenant;
		}
		if (Reglages.ReapparitionSecondes > 0.f && Maintenant - P.MortDepuis >= Reglages.ReapparitionSecondes)
		{
			ARemplacer.Add(P.Dossier);
			Poses.RemoveAt(i);
		}
		else if (Reglages.ReapparitionSecondes <= 0.f && !P.Pion.IsValid())
		{
			Poses.RemoveAt(i);
		}
	}
	for (const FName& D : ARemplacer)
	{
		PoserEnnemi(D, true); // un nouveau, ailleurs, loin des joueurs
	}
}

// ===================================================================================
//  Tick
// ===================================================================================

void AEnnemisChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Maintenant = GetWorld()->GetTimeSeconds();

	if (Maintenant >= ProchainScan)
	{
		ProchainScan = Maintenant + 1.f;
		ScannerLesPersonnages(Maintenant);
	}

	if (!EstServeur())
	{
		return;
	}

	if (!bPoseFaite && Maintenant >= PoseA)
	{
		bPoseFaite = true;
		PreparerPlaces();
		PoserTout();
	}

	if (Maintenant >= ProchaineSurveillance)
	{
		ProchaineSurveillance = Maintenant + 2.f;
		SurveillerLesPoses(Maintenant);
	}

	// Zombies suivis disparus : on les oublie
	for (auto It = ZombiesSuivis.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// Ennemis du dossier Other : le plugin les dirige
	TArray<TWeakObjectPtr<ACharacter>> Liste;
	Autos.GetKeys(Liste);
	for (const TWeakObjectPtr<ACharacter>& Cle : Liste)
	{
		ACharacter* E = Cle.Get();
		if (!IsValid(E))
		{
			Autos.Remove(Cle);
			continue;
		}
		FEnnemiAuto* Etat = Autos.Find(Cle);
		if (Etat && !Etat->bMort)
		{
			Piloter(E, *Etat, Maintenant, DeltaSeconds);
		}
	}
}

// ===================================================================================
//  Les joueurs
// ===================================================================================

void AEnnemisChelles::JoueursVivants(TArray<APawn*>& Resultat) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (Pion && EstVivant(Pion))
		{
			Resultat.Add(Pion);
		}
	}
}

bool AEnnemisChelles::EstVivant(APawn* Joueur)
{
	if (!IsValid(Joueur) || Joueur->Tags.Contains(MarqueJoueurHorsJeu))
	{
		return false;
	}
	bool bMort = false;
	if (LireBooleen(Joueur, { TEXT("IsDead"), TEXT("isDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
	{
		return false;
	}
	float Vie = 1.f;
	if (LireNombre(Joueur, { TEXT("HealthCurrent"), TEXT("Health") }, Vie) && Vie <= 0.f)
	{
		return false;
	}
	return true;
}

float AEnnemisChelles::VieMax(APawn* Joueur)
{
	float Max = 100.f;
	if (LireNombre(Joueur, { TEXT("HealthMax"), TEXT("MaxHealth") }, Max) && Max > 0.f)
	{
		return Max;
	}
	return 100.f;
}

bool AEnnemisChelles::EstMort(ACharacter* Ennemi) const
{
	if (!IsValid(Ennemi) || Ennemi->IsPendingKillPending())
	{
		return true;
	}
	if (const FEnnemiAuto* Etat = Autos.Find(Ennemi))
	{
		return Etat->bMort;
	}
	if (const FEnnemiAuto* Etat = ZombiesSuivis.Find(Ennemi))
	{
		if (Etat->bMort)
		{
			return true;
		}
	}
	if (Ennemi->GetGameTimeSinceCreation() < 5.f)
	{
		return false; // sa vie n'est peut-etre pas encore remplie par son Blueprint
	}
	bool bMort = false;
	if (LireBooleen(Ennemi, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead"), TEXT("bDead") }, bMort) && bMort)
	{
		return true;
	}
	float Vie = 1.f;
	if (LireNombre(Ennemi, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth") }, Vie) && Vie <= 0.f)
	{
		return true;
	}
	return false;
}

// ===================================================================================
//  Ennemis du dossier Other : le plugin les dirige (serveur)
// ===================================================================================

void AEnnemisChelles::PrendreEnMain(ACharacter* E)
{
	if (!IsValid(E) || Autos.Contains(E))
	{
		return;
	}
	AController* Ancien = E->GetController();
	if (Ancien && Ancien->IsA(APlayerController::StaticClass()))
	{
		return; // c'est un joueur : on n'y touche pas
	}
	E->Tags.AddUnique(MarqueEnnemiAuto);
	E->SetReplicates(true);
	E->SetReplicateMovement(true);

	// Son propre cerveau (s'il en a un) est remplace par un controleur simple que le plugin guide
	if (!Ancien || Ancien->GetClass() != AAIController::StaticClass())
	{
		if (Ancien)
		{
			Ancien->UnPossess();
			Ancien->Destroy();
		}
		E->AIControllerClass = AAIController::StaticClass();
		E->SpawnDefaultController();
	}

	FEnnemiAuto Etat;
	if (UCharacterMovementComponent* Mouvement = E->GetCharacterMovement())
	{
		Etat.VitesseMarche = FMath::Max(150.f, Mouvement->MaxWalkSpeed);
		Mouvement->bOrientRotationToMovement = true;
		Mouvement->bUseControllerDesiredRotation = false;
		Mouvement->RotationRate = FRotator(0.f, 360.f, 0.f);
		Mouvement->JumpZVelocity = FMath::Max(Mouvement->JumpZVelocity, 520.f);
		if (Mouvement->MovementMode == MOVE_None)
		{
			Mouvement->SetMovementMode(MOVE_Walking);
		}
	}
	E->bUseControllerRotationYaw = false;
	if (USkeletalMeshComponent* Corps = E->GetMesh())
	{
		Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	E->SetCanBeDamaged(true);
	E->OnTakeAnyDamage.AddUniqueDynamic(this, &AEnnemisChelles::ToucheQuelconque);
	E->OnTakePointDamage.AddUniqueDynamic(this, &AEnnemisChelles::TouchePrecis);

	Etat.Vie = 100.f;
	Etat.DernierePosition = E->GetActorLocation();
	Etat.PointPromenade = E->GetActorLocation();
	Etat.CoteContournement = FMath::RandBool() ? 1.f : -1.f;
	Etat.ProchainChoixCible = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.f, 0.5f);
	Autos.Add(E, Etat);
	UE_LOG(LogTemp, Log, TEXT("[Ennemis] %s (dossier Other) devient un ennemi des joueurs."), *NomBlueprint(E->GetClass()));
}

void AEnnemisChelles::Piloter(ACharacter* E, FEnnemiAuto& Etat, float Maintenant, float DeltaSeconds)
{
	const float Detection = Reglages.DistanceDetectionMetres * 100.f;
	const float Abandon = Reglages.DistanceAbandonMetres * 100.f;
	UCharacterMovementComponent* Mouvement = E->GetCharacterMovement();

	// 1) Le joueur vivant le plus proche, s'il est assez pres (deux fois par seconde)
	if (Maintenant >= Etat.ProchainChoixCible || (Etat.Cible.IsValid() && !EstVivant(Etat.Cible.Get())))
	{
		Etat.ProchainChoixCible = Maintenant + 0.5f;
		TArray<APawn*> Joueurs;
		JoueursVivants(Joueurs);
		APawn* Meilleur = nullptr;
		float MeilleureDist = TNumericLimits<float>::Max();
		for (APawn* J : Joueurs)
		{
			const float D = FVector::Dist(J->GetActorLocation(), E->GetActorLocation());
			if (D < MeilleureDist)
			{
				MeilleureDist = D;
				Meilleur = J;
			}
		}
		const bool bDejaEnChasse = Etat.Cible.IsValid();
		if (Meilleur && MeilleureDist <= (bDejaEnChasse ? Abandon : Detection))
		{
			Etat.Cible = Meilleur;
		}
		else if (!bDejaEnChasse || !EstVivant(Etat.Cible.Get())
			|| FVector::Dist(Etat.Cible->GetActorLocation(), E->GetActorLocation()) > Abandon)
		{
			Etat.Cible = nullptr;
		}
	}

	// 2) Coup en cours : les degats tombent au milieu de l'animation
	const float RayonEnnemi = E->GetCapsuleComponent() ? E->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f;
	const float Portee = 110.f + RayonEnnemi;
	if (Etat.CoupPrevuA > 0.f && Maintenant >= Etat.CoupPrevuA)
	{
		Etat.CoupPrevuA = -1.f;
		APawn* Victime = Etat.CibleDuCoup.Get();
		if (EstVivant(Victime))
		{
			const FVector Ecart = Victime->GetActorLocation() - E->GetActorLocation();
			if (Ecart.Size2D() < Portee + 90.f && FMath::Abs(Ecart.Z) < 220.f)
			{
				const float Degats = VieMax(Victime) * Reglages.DegatsPourcent / 100.f;
				UGameplayStatics::ApplyDamage(Victime, Degats, E->GetController(), E, UDamageType::StaticClass());
				if (!IsValid(E) || Etat.bMort)
				{
					return;
				}
			}
		}
	}

	APawn* Cible = Etat.Cible.Get();

	// 3) Personne en vue : il se promene doucement
	if (!Cible)
	{
		if (!Reglages.bPromenade)
		{
			return;
		}
		if (Mouvement)
		{
			Mouvement->MaxWalkSpeed = Etat.VitesseMarche * 0.5f;
		}
		const FVector Vers = (Etat.PointPromenade - E->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (Maintenant >= Etat.ProchainePromenade || Vers.Size() < 80.f)
		{
			if (Maintenant >= Etat.ProchainePromenade)
			{
				const float Angle = FMath::FRandRange(0.f, 2.f * PI);
				const float Dist = FMath::FRandRange(300.f, 1200.f);
				Etat.PointPromenade = E->GetActorLocation() + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
				Etat.ProchainePromenade = Maintenant + FMath::FRandRange(6.f, 12.f);
			}
			return; // arrive : il attend un peu
		}
		E->AddMovementInput(Vers.GetSafeNormal(), 1.f);
		return;
	}

	FVector Vers = Cible->GetActorLocation() - E->GetActorLocation();
	const float Hauteur = Vers.Z;
	Vers.Z = 0.f;
	const float Distance = Vers.Size();
	const FVector Direction = Vers.GetSafeNormal();

	// 4) A portee : il frappe
	if (Distance < Portee && FMath::Abs(Hauteur) < 200.f)
	{
		E->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
		if (Maintenant >= Etat.ProchaineAttaque)
		{
			Etat.ProchaineAttaque = Maintenant + Reglages.TempsEntreCoups;
			Etat.CoupPrevuA = Maintenant + FMath::Min(0.55f, Reglages.TempsEntreCoups * 0.5f);
			Etat.CibleDuCoup = Cible;
			MulticastAttaque(E);
		}
		Etat.Bloque = 0;
		Etat.DernierePosition = E->GetActorLocation();
		return;
	}

	// 5) Sinon il court vers le joueur (et contourne s'il est coince)
	if (Mouvement)
	{
		Mouvement->MaxWalkSpeed = Reglages.VitesseCourse > 0.f ? Reglages.VitesseCourse : Etat.VitesseMarche;
	}
	FVector Marche = Direction;
	if (Maintenant < Etat.ContournerJusqua)
	{
		Marche = Direction.RotateAngleAxis(70.f * Etat.CoteContournement, FVector::UpVector);
	}
	E->AddMovementInput(Marche, 1.f);

	if (Maintenant >= Etat.ProchainControle)
	{
		Etat.ProchainControle = Maintenant + 1.f;
		const float Parcouru = FVector::Dist2D(E->GetActorLocation(), Etat.DernierePosition);
		Etat.DernierePosition = E->GetActorLocation();
		if (Parcouru < 40.f)
		{
			++Etat.Bloque;
			E->Jump();
			Etat.ContournerJusqua = Maintenant + 1.5f;
			if (Etat.Bloque % 3 == 0)
			{
				Etat.CoteContournement = -Etat.CoteContournement;
			}
		}
		else
		{
			Etat.Bloque = 0;
		}
	}
}

UAnimMontage* AEnnemisChelles::MontageAttaquePour(ACharacter* E)
{
	if (!IsValid(E))
	{
		return nullptr;
	}
	if (const TWeakObjectPtr<UAnimMontage>* Connu = MontagesParClasse.Find(E->GetClass()))
	{
		return Connu->Get();
	}

	// On cherche une animation d'attaque dans les variables du Blueprint (montage, ou liste de montages)
	UAnimMontage* Trouve = nullptr;
	UAnimMontage* Secours = nullptr;
	for (TFieldIterator<FProperty> It(E->GetClass()); It && !Trouve; ++It)
	{
		TArray<UAnimMontage*> Candidats;
		if (FObjectProperty* P = CastField<FObjectProperty>(*It))
		{
			if (P->PropertyClass && P->PropertyClass->IsChildOf(UAnimMontage::StaticClass()))
			{
				Candidats.Add(Cast<UAnimMontage>(P->GetObjectPropertyValue_InContainer(E)));
			}
		}
		else if (FArrayProperty* Tableau = CastField<FArrayProperty>(*It))
		{
			FObjectProperty* Interieur = CastField<FObjectProperty>(Tableau->Inner);
			if (Interieur && Interieur->PropertyClass && Interieur->PropertyClass->IsChildOf(UAnimMontage::StaticClass()))
			{
				FScriptArrayHelper Aide(Tableau, Tableau->ContainerPtrToValuePtr<void>(E));
				for (int32 i = 0; i < Aide.Num(); ++i)
				{
					Candidats.Add(Cast<UAnimMontage>(Interieur->GetObjectPropertyValue(Aide.GetRawPtr(i))));
				}
			}
		}
		for (UAnimMontage* M : Candidats)
		{
			if (!MontageCompatible(M, E))
			{
				continue;
			}
			if (NomDAttaque(It->GetName()) || NomDAttaque(M->GetName()))
			{
				Trouve = M;
				break;
			}
			if (!Secours && !M->GetName().Contains(TEXT("death"), ESearchCase::IgnoreCase) && !M->GetName().Contains(TEXT("die"), ESearchCase::IgnoreCase))
			{
				Secours = M;
			}
		}
	}
	if (!Trouve)
	{
		Trouve = Secours;
	}
	// Secours : l'attaque de zombie de FPS_Game, si le personnage a les memes os
	if (!Trouve && MontageCompatible(MontageZombie, E))
	{
		Trouve = MontageZombie;
	}
	MontagesParClasse.Add(E->GetClass(), Trouve);
	return Trouve;
}

void AEnnemisChelles::MulticastAttaque_Implementation(ACharacter* E)
{
	if (!IsValid(E) || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UAnimMontage* Montage = MontageAttaquePour(E);
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (Montage && Corps)
	{
		if (UAnimInstance* Anim = Corps->GetAnimInstance())
		{
			Anim->Montage_Play(Montage, 1.f);
		}
	}
}

// ===================================================================================
//  Degats recus par les ennemis du dossier Other (serveur)
// ===================================================================================

void AEnnemisChelles::TouchePrecis(AActor* Touche, float Degats, AController* Auteur, FVector PointImpact, UPrimitiveComponent* Composant,
	FName Os, FVector Direction, const UDamageType* TypeDegats, AActor* Causeur)
{
	ACharacter* E = Cast<ACharacter>(Touche);
	FEnnemiAuto* Etat = E ? EtatDe(E) : nullptr;
	if (!Etat || Etat->bMort)
	{
		return;
	}
	const FString NomOs = Os.ToString();
	bool bTete = NomOs.Contains(TEXT("head"), ESearchCase::IgnoreCase) || NomOs.Contains(TEXT("neck"), ESearchCase::IgnoreCase)
		|| NomOs.Contains(TEXT("tete"), ESearchCase::IgnoreCase);
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (!bTete && Corps && Corps->DoesSocketExist(TEXT("head")))
	{
		const FVector Tete = Corps->GetSocketLocation(TEXT("head"));
		bTete = PointImpact.Z >= Tete.Z - 8.f && FVector::Dist2D(PointImpact, Tete) < 35.f;
	}
	Etat->ImageCoupPrecis = GFrameCounter;
	Etat->bCoupTete = bTete;
	Etat->DirectionCoup = Direction;
}

void AEnnemisChelles::ToucheQuelconque(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur)
{
	ACharacter* E = Cast<ACharacter>(Touche);
	FEnnemiAuto* Etat = E ? EtatDe(E) : nullptr;
	if (!Etat || Etat->bMort || Degats <= 0.f)
	{
		return;
	}
	if (ZombiesSuivis.Contains(E))
	{
		// Son Blueprint l'a peut-etre deja fait mourir : on ne le tue pas deux fois
		bool bDejaMort = false;
		if (LireBooleen(E, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bDejaMort) && bDejaMort)
		{
			Etat->bMort = true;
			return;
		}
	}
	// Qui a tire ?
	AController* Tireur = Auteur;
	if (!Tireur && Causeur)
	{
		Tireur = Causeur->GetInstigatorController();
		if (!Tireur)
		{
			if (APawn* PionCauseur = Cast<APawn>(Causeur->GetOwner()))
			{
				Tireur = PionCauseur->GetController();
			}
		}
	}
	// Les ennemis ne se blessent pas entre eux
	if ((Causeur && Causeur->Tags.Contains(MarqueEnnemiAuto)) || (Tireur && !Tireur->IsPlayerController()))
	{
		return;
	}
	// Touche par un joueur : il le prend en chasse, meme de loin
	if (Tireur && EstVivant(Tireur->GetPawn()))
	{
		Etat->Cible = Tireur->GetPawn();
		Etat->ProchainChoixCible = GetWorld()->GetTimeSeconds() + 3.f;
	}

	const bool bPrecis = Etat->ImageCoupPrecis == GFrameCounter;
	if (bPrecis && Etat->bCoupTete && Reglages.bTeteTue)
	{
		Tuer(E, Etat->DirectionCoup);
		return;
	}
	// Une balle retire DegatsCorpsPourcent ; un petit degat continu (feu...) retire seulement sa valeur
	Etat->Vie -= Degats >= 5.f ? Reglages.DegatsCorpsPourcent : Degats;
	if (Etat->Vie <= 0.5f)
	{
		Tuer(E, bPrecis ? Etat->DirectionCoup : FVector::ZeroVector);
	}
}

FEnnemiAuto* AEnnemisChelles::EtatDe(ACharacter* E)
{
	if (FEnnemiAuto* Etat = Autos.Find(E))
	{
		return Etat;
	}
	return ZombiesSuivis.Find(E);
}

void AEnnemisChelles::SuivreZombie(ACharacter* E)
{
	if (!Reglages.bZombiesTuables || !IsValid(E) || Autos.Contains(E) || ZombiesSuivis.Contains(E))
	{
		return;
	}
	// Les zombies de l'invasion sont deja geres par le plugin ChellesZombies
	if (E->Tags.Contains(MarqueZombieInvasion) || E->Tags.Contains(MarqueZombieInvasionHabille)
		|| E->GetClass()->GetName() == TEXT("BP_AI_CharZombie_C"))
	{
		return;
	}
	AController* Controleur = E->GetController();
	if (Controleur && Controleur->IsA(APlayerController::StaticClass()))
	{
		return; // c'est un joueur
	}
	E->SetCanBeDamaged(true);
	E->OnTakeAnyDamage.AddUniqueDynamic(this, &AEnnemisChelles::ToucheQuelconque);
	E->OnTakePointDamage.AddUniqueDynamic(this, &AEnnemisChelles::TouchePrecis);
	FEnnemiAuto Etat;
	Etat.Vie = 100.f;
	ZombiesSuivis.Add(E, Etat);
	UE_LOG(LogTemp, Log, TEXT("[Ennemis] Zombie %s : le plugin compte ses balles (tete = mort, corps = %.0f %%)."),
		*NomBlueprint(E->GetClass()), Reglages.DegatsCorpsPourcent);
}

void AEnnemisChelles::Tuer(ACharacter* E, const FVector& Direction)
{
	FEnnemiAuto* Etat = EtatDe(E);
	if (!Etat || Etat->bMort)
	{
		return;
	}
	Etat->bMort = true;
	Etat->Vie = 0.f;

	// On previent aussi son Blueprint (pour ses animations), sans toucher a son fichier
	EcrireBooleen(E, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, true);
	if (UCharacterMovementComponent* Mouvement = E->GetCharacterMovement())
	{
		Mouvement->StopMovementImmediately();
		Mouvement->DisableMovement();
	}
	E->OnTakeAnyDamage.RemoveDynamic(this, &AEnnemisChelles::ToucheQuelconque);
	E->OnTakePointDamage.RemoveDynamic(this, &AEnnemisChelles::TouchePrecis);
	E->DetachFromControllerPendingDestroy();
	E->SetLifeSpan(10.f);
	MulticastMort(E, Direction);
}

void AEnnemisChelles::MulticastMort_Implementation(ACharacter* E, FVector Direction)
{
	if (!IsValid(E))
	{
		return;
	}
	if (UCapsuleComponent* Capsule = E->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (!Corps)
	{
		return;
	}
	if (UAnimInstance* Anim = Corps->GetAnimInstance())
	{
		Anim->StopAllMontages(0.1f);
	}
	if (Corps->GetPhysicsAsset())
	{
		// Chute en poupee de chiffon
		Corps->SetCollisionProfileName(TEXT("Ragdoll"));
		Corps->SetSimulatePhysics(true);
		Corps->WakeAllRigidBodies();
		if (!Direction.IsNearlyZero())
		{
			Corps->AddImpulse(Direction.GetSafeNormal() * 350.f, NAME_None, true);
		}
	}
	else
	{
		// Pas de squelette physique : il disparait dans une giclee
		if (GetNetMode() != NM_DedicatedServer)
		{
			if (UParticleSystem* Sang = LoadObject<UParticleSystem>(nullptr, CHEMIN_SANG, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Sang, E->GetActorLocation(), FRotator::ZeroRotator, FVector(2.f));
			}
		}
		E->SetActorHiddenInGame(true);
	}
}

// ===================================================================================
//  Chaque seconde : nouveaux personnages (poses a la main, apparus, arrives par le reseau)
// ===================================================================================

void AEnnemisChelles::ScannerLesPersonnages(float Maintenant)
{
	const bool bAffiche = GetNetMode() != NM_DedicatedServer;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* E = *It;
		if (!IsValid(E) || E->Tags.Contains(MarqueVu))
		{
			continue;
		}
		const FName Dossier = DossierDe(E->GetClass());
		if (Dossier == NAME_None)
		{
			E->Tags.AddUnique(MarqueVu); // pas un ennemi des dossiers AI (joueur, passant...)
			continue;
		}
		// On laisse a son Blueprint (et au plugin ChellesZombies) le temps de s'installer
		const float Attente = Dossier == DossierZombie ? 3.f : 1.f;
		if (E->GetGameTimeSinceCreation() < Attente)
		{
			continue;
		}
		E->Tags.AddUnique(MarqueVu);

		if (Dossier == DossierOther)
		{
			// Les balles et les tirs speciaux doivent le toucher comme un ennemi FPS_Game
			if (!bCollisionsModeleLues)
			{
				bCollisionsModeleLues = true;
				ReponsesCorpsModele = FCollisionResponseContainer(ECR_Block);
				ReponsesCapsuleModele = FCollisionResponseContainer(ECR_Block);
				if (UClass* Modele = LoadClass<ACharacter>(nullptr, CHEMIN_ENNEMI_MODELE, nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					const ACharacter* CDO = Cast<ACharacter>(Modele->GetDefaultObject());
					if (CDO && CDO->GetMesh())
					{
						ReponsesCorpsModele = CDO->GetMesh()->GetCollisionResponseToChannels();
					}
					if (CDO && CDO->GetCapsuleComponent())
					{
						ReponsesCapsuleModele = CDO->GetCapsuleComponent()->GetCollisionResponseToChannels();
					}
				}
			}
			USkeletalMeshComponent* Corps = E->GetMesh();
			UCapsuleComponent* Capsule = E->GetCapsuleComponent();
			if (Corps && Corps->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				Corps->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			for (int32 Canal = ECC_GameTraceChannel1; Canal <= ECC_GameTraceChannel18; ++Canal)
			{
				const ECollisionChannel C = (ECollisionChannel)Canal;
				if (Corps)
				{
					Corps->SetCollisionResponseToChannel(C, ReponsesCorpsModele.GetResponse(C));
				}
				if (Capsule)
				{
					Capsule->SetCollisionResponseToChannel(C, ReponsesCapsuleModele.GetResponse(C));
				}
			}
			if (Corps)
			{
				Corps->SetCollisionResponseToChannel(ECC_Visibility, ReponsesCorpsModele.GetResponse(ECC_Visibility));
			}
			if (EstServeur())
			{
				PrendreEnMain(E);
			}
		}

		if (Dossier == DossierZombie && EstServeur())
		{
			SuivreZombie(E);
		}

		if (Reglages.bHabiller)
		{
			// Le serveur choisit la tenue (meme sur un serveur sans ecran : les joueurs la recoivent)
			bool bConnu = false;
			const int32 Choix = ChoixDeTenue(E, bConnu);
			if (bAffiche)
			{
				if (bConnu || Dossier == DossierZombie)
				{
					Habiller(E, Dossier, Choix);
				}
				else
				{
					EnAttenteDeTenue.Add(E);
				}
			}
		}
	}

	// Ennemis qui attendaient le choix du serveur (multijoueur)
	for (int32 i = EnAttenteDeTenue.Num() - 1; i >= 0; --i)
	{
		ACharacter* E = EnAttenteDeTenue[i].Get();
		if (!IsValid(E))
		{
			EnAttenteDeTenue.RemoveAt(i);
			continue;
		}
		bool bConnu = false;
		const int32 Choix = ChoixDeTenue(E, bConnu);
		if (bConnu || E->GetGameTimeSinceCreation() > 8.f)
		{
			Habiller(E, DossierDe(E->GetClass()), Choix);
			EnAttenteDeTenue.RemoveAt(i);
		}
	}
	if (EstServeur())
	{
		TenuesChoisies.RemoveAll([](const FTenueChoisie& T) { return !IsValid(T.Pion); });
	}
}

int32 AEnnemisChelles::ChoixDeTenue(ACharacter* E, bool& bConnu)
{
	const int32 Nombre = FMath::Max(1, Reglages.Personnes.Num());
	for (const FTenueChoisie& T : TenuesChoisies)
	{
		if (T.Pion == E)
		{
			bConnu = true;
			return T.Choix % Nombre;
		}
	}
	// Un ennemi pose dans la carte a le meme nom chez tous les joueurs : son nom suffit
	const int32 ParLeNom = (int32)(GetTypeHash(E->GetFName().ToString()) % (uint32)Nombre);
	if (E->IsNetStartupActor())
	{
		bConnu = true;
		return ParLeNom;
	}
	if (EstServeur())
	{
		FTenueChoisie T;
		T.Pion = E;
		T.Choix = (uint8)FMath::RandRange(0, Nombre - 1);
		TenuesChoisies.Add(T);
		ForceNetUpdate();
		bConnu = true;
		return T.Choix;
	}
	bConnu = false;
	return ParLeNom;
}

// ===================================================================================
//  Tenues : les ennemis encore en mannequin gris recoivent de vrais vetements
// ===================================================================================

void AEnnemisChelles::Habiller(ACharacter* E, FName Dossier, int32 Choix)
{
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (!Corps || !Corps->SkeletalMesh || AUneTenue(E))
	{
		return; // deja habille (par son Blueprint ou le plugin ChellesZombies)
	}
	if (E->Tags.Contains(MarqueZombieInvasion) || E->Tags.Contains(MarqueZombieInvasionHabille)
		|| E->GetClass()->GetName() == TEXT("BP_AI_CharZombie_C"))
	{
		return; // zombie de l'invasion : le plugin ChellesZombies l'habille lui-meme
	}

	USkeletalMesh* Tenue = nullptr;
	if (Dossier == DossierZombie)
	{
		// Zombie : vrai zombie DecayedZombieB (seulement s'il a encore le mannequin gris)
		if (!EstUnMannequin(Corps))
		{
			return;
		}
		Tenue = PeauZombie;
	}
	else
	{
		// Les autres : un vrai humain habille (Scanned3DPeoplePack), a la place du mannequin gris
		// ou du bandit de couleur pose par Tools\Ennemis
		const bool bBandit = Reglages.bRemplacerBandits && Corps->SkeletalMesh->GetName().StartsWith(TEXT("SK_Bandit"), ESearchCase::IgnoreCase);
		if (!EstUnMannequin(Corps) && !bBandit)
		{
			return; // il a deja sa propre apparence (pack d'un autre auteur, squelette...)
		}
		for (int32 Essai = 0; Essai < MeshesPersonnes.Num() && !Tenue; ++Essai)
		{
			Tenue = MeshesPersonnes[(Choix + Essai) % MeshesPersonnes.Num()];
		}
	}
	if (!Tenue)
	{
		return;
	}

	// La tenue a les memes os que le mannequin : elle suit exactement ses animations
	Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	USkeletalMeshComponent* Habit = NewObject<USkeletalMeshComponent>(E, NomTenue);
	Habit->SetSkeletalMesh(Tenue);
	Habit->SetupAttachment(Corps);
	Habit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Habit->SetGenerateOverlapEvents(false);
	Habit->RegisterComponent();
	Habit->AttachToComponent(Corps, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Habit->SetMasterPoseComponent(Corps);
	E->AddInstanceComponent(Habit);
	// Le corps d'origine devient invisible mais garde ses collisions : les balles touchent ses os (tete...)
	Corps->SetHiddenInGame(true, false);
	Corps->SetCastShadow(false);
}

// ===================================================================================
//  Console : Chelles.Ennemis
// ===================================================================================

void AEnnemisChelles::Resume() const
{
	TArray<FString> Lignes;
	for (const FName& D : LesDossiers())
	{
		const TArray<UClass*>* Classes = ClassesParDossier.Find(D);
		int32 Vivants = 0;
		for (const FEnnemiPose& P : Poses)
		{
			if (P.Dossier == D && !EstMort(P.Pion.Get()))
			{
				++Vivants;
			}
		}
		FString Noms;
		if (Classes)
		{
			for (UClass* C : *Classes)
			{
				Noms += (Noms.IsEmpty() ? TEXT("") : TEXT(", ")) + NomBlueprint(C);
			}
		}
		Lignes.Add(FString::Printf(TEXT("%s : %d en vie  (%s)"), *D.ToString(), Vivants, Noms.IsEmpty() ? TEXT("aucun personnage dans ce dossier") : *Noms));
	}
	Lignes.Add(FString::Printf(TEXT("Ennemis du dossier Other diriges : %d - places possibles : %d"), Autos.Num(), Places.Num()));
	for (const FString& L : Lignes)
	{
		UE_LOG(LogTemp, Log, TEXT("[Ennemis] %s"), *L);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Orange, L);
		}
	}
}
