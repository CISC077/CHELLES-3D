// CHELLES 3D - Optimisation : neige autour du joueur, fleurs de pres, PNJ lointains caches (UE 4.27)
#include "OptimisationChelles.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName RaisonPause(TEXT("ChellesOptimisation"));
	const FName MarqueZombie(TEXT("ChellesZombie"));

	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesOptimisation"), TEXT("Optimisation.txt"));
	}

	bool EstOui(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd().ToLower();
		return T == TEXT("1") || T == TEXT("oui") || T == TEXT("true") || T == TEXT("vrai");
	}

	/** Noms de plantes posees une par une (pas en foliage) : seulement les petites sont raccourcies. */
	bool NomDePlante(const FString& NomBrut)
	{
		static const TCHAR* Mots[] = {
			TEXT("flower"), TEXT("fleur"), TEXT("plant"), TEXT("grass"), TEXT("herbe"), TEXT("bush"), TEXT("buisson"),
			TEXT("weed"), TEXT("clover"), TEXT("daisy"), TEXT("tulip"), TEXT("rose"), TEXT("fern"), TEXT("fougere"),
			TEXT("shrub"), TEXT("ivy"), TEXT("lierre"), TEXT("lavender"), TEXT("lavande") };
		const FString Nom = NomBrut.ToLower();
		for (const TCHAR* Mot : Mots)
		{
			if (Nom.Contains(Mot))
			{
				return true;
			}
		}
		return false;
	}
}

AOptimisationChelles::AOptimisationChelles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // chaque machine fait son optimisation, rien ne passe par le reseau
}

// ------------------------------------------------------------------------------------------------
//  Reglages
// ------------------------------------------------------------------------------------------------

void AOptimisationChelles::LireReglages()
{
	const FString Chemin = CheminReglages();
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[Optim] %s absent : reglages par defaut."), *Chemin);
		return;
	}
	for (const FString& Brute : Lignes)
	{
		const FString Ligne = Brute.TrimStartAndEnd();
		FString Cle, Valeur;
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle = Cle.TrimStartAndEnd();
		Valeur = Valeur.TrimStartAndEnd();
		const float Nombre = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));
		if (Cle == TEXT("Actif"))                      Reglages.bActif = EstOui(Valeur);
		else if (Cle == TEXT("PNJ"))                   Reglages.bPNJ = EstOui(Valeur);
		else if (Cle == TEXT("DistancePNJ"))           Reglages.DistancePNJ = FMath::Max(10.f, Nombre) * 100.f;
		else if (Cle == TEXT("Fleurs"))                Reglages.bFleurs = EstOui(Valeur);
		else if (Cle == TEXT("DistanceFleurs"))        Reglages.DistanceFleurs = FMath::Max(5.f, Nombre) * 100.f;
		else if (Cle == TEXT("TailleMaxFleursMetres")) Reglages.TailleMaxFleurs = FMath::Max(0.1f, Nombre) * 100.f;
		else if (Cle == TEXT("NeigeLointaine"))        Reglages.bNeigeLointaine = EstOui(Valeur);
		else if (Cle == TEXT("TailleNeigeMetres"))     Reglages.TailleNeige = FMath::Max(0.f, Nombre) * 100.f;
	}
	UE_LOG(LogTemp, Log, TEXT("[Optim] Reglages : actif %d, PNJ %d (%.0f m), fleurs %d (%.0f m, plantes < %.1f m), neige lointaine %d, zone de neige %.0f m."),
		Reglages.bActif, Reglages.bPNJ, Reglages.DistancePNJ / 100.f, Reglages.bFleurs, Reglages.DistanceFleurs / 100.f,
		Reglages.TailleMaxFleurs / 100.f, Reglages.bNeigeLointaine, Reglages.TailleNeige / 100.f);
}

// ------------------------------------------------------------------------------------------------
//  Neige : seulement la couche qui suit la camera
// ------------------------------------------------------------------------------------------------

void AOptimisationChelles::PreparerNeige()
{
	UWorld* W = GetWorld();
	if (!Reglages.bActif || !W || W->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	int32 Coupees = 0;
	int32 Reglees = 0;
	// Les 2 couches posees par Tools\Neige\ajouter_neige.py : MI_Neige_Proche (suit la camera, 40 m)
	// et MI_Neige_Lointaine (300 m de large, 450 m de haut : la plus couteuse).
	for (TActorIterator<AStaticMeshActor> It(W); It; ++It)
	{
		UStaticMeshComponent* Comp = It->GetStaticMeshComponent();
		UMaterialInterface* Mat = Comp ? Comp->GetMaterial(0) : nullptr;
		if (!Mat)
		{
			continue;
		}
		const FString Nom = Mat->GetName();
		if (Nom.StartsWith(TEXT("MI_Neige_Lointaine")))
		{
			if (!Reglages.bNeigeLointaine)
			{
				// Seulement sur cette machine (le composant n'est pas replique)
				Comp->SetVisibility(false);
				Comp->SetHiddenInGame(true);
				++Coupees;
			}
		}
		else if (Nom.StartsWith(TEXT("MI_Neige_Proche")) && Reglages.TailleNeige > 0.f)
		{
			// Le nom garde "MI_Neige_" au debut : le plugin ChellesNeige (nuit / qualite) retrouve la couche.
			UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mat);
			if (!Mid)
			{
				const FName NomMid = MakeUniqueObjectName(Comp, UMaterialInstanceDynamic::StaticClass(), TEXT("MI_Neige_Proche_Optim"));
				Mid = UMaterialInstanceDynamic::Create(Mat, Comp, NomMid);
				if (!Mid)
				{
					continue;
				}
				Comp->SetMaterial(0, Mid);
			}
			const float Boite = Mid->K2_GetScalarParameterValue(TEXT("BoiteXY"));
			if (Boite <= 0.f || FMath::IsNearlyEqual(Boite, Reglages.TailleNeige, 1.f))
			{
				continue;
			}
			const float Rapport = Reglages.TailleNeige / Boite;
			const float Densite = Mid->K2_GetScalarParameterValue(TEXT("Densite"));
			const float LoinDebut = Mid->K2_GetScalarParameterValue(TEXT("LoinDebut"));
			const float LoinLong = Mid->K2_GetScalarParameterValue(TEXT("LoinLong"));
			Mid->SetScalarParameterValue(TEXT("BoiteXY"), Reglages.TailleNeige);
			// Meme nombre de flocons par metre cube qu'avant (le maillage a un nombre fixe de flocons)
			Mid->SetScalarParameterValue(TEXT("Densite"), FMath::Clamp(Densite * Rapport * Rapport, 0.f, 1.f));
			// Fondu au bord de la zone, a la nouvelle taille
			Mid->SetScalarParameterValue(TEXT("LoinDebut"), LoinDebut * Rapport);
			Mid->SetScalarParameterValue(TEXT("LoinLong"), LoinLong * Rapport);
			++Reglees;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Optim] Neige : %d couche(s) lointaine(s) coupee(s), %d couche(s) autour du joueur reglee(s) a %.0f m."),
		Coupees, Reglees, Reglages.TailleNeige / 100.f);
}

// ------------------------------------------------------------------------------------------------
//  Demarrage
// ------------------------------------------------------------------------------------------------

void AOptimisationChelles::BeginPlay()
{
	Super::BeginPlay();
	UWorld* W = GetWorld();
	if (!Reglages.bActif || !W)
	{
		UE_LOG(LogTemp, Log, TEXT("[Optim] Desactive (Actif=0 dans Optimisation.txt)."));
		return;
	}
	const bool bImage = W->GetNetMode() != NM_DedicatedServer;

	if (Reglages.bPNJ)
	{
		ReglerPassants();
		GetWorldTimerManager().SetTimer(MinuteurPNJ, this, &AOptimisationChelles::VerifierPNJ, 0.5f, true, 1.f);
	}
	if (Reglages.bFleurs && bImage)
	{
		PremierPassageFleurs();
		// L'herbe du Landscape est recreee au fur et a mesure que le joueur avance : on repasse toutes les 2 s
		GetWorldTimerManager().SetTimer(MinuteurHerbe, this, &AOptimisationChelles::VerifierHerbe, 2.f, true, 0.5f);
	}
}

void AOptimisationChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	GetWorldTimerManager().ClearTimer(MinuteurPNJ);
	GetWorldTimerManager().ClearTimer(MinuteurHerbe);
	for (TPair<TWeakObjectPtr<ACharacter>, FEtatPNJ>& Paire : PNJ)
	{
		if (ACharacter* Perso = Paire.Key.Get())
		{
			RendreTout(Perso, Paire.Value);
		}
	}
	PNJ.Reset();
	Super::EndPlay(Raison);
}

// ------------------------------------------------------------------------------------------------
//  PNJ
// ------------------------------------------------------------------------------------------------

void AOptimisationChelles::ReglerPassants()
{
	// Le plugin ChellesPassants met deja ses passants en pause loin du joueur (150 m par defaut) :
	// on lui donne la meme distance que les autres PNJ, sans toucher a ses fichiers.
	int32 Nb = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() != TEXT("PassantsSpawner"))
		{
			continue;
		}
		if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(It->GetClass(), TEXT("DistanceEndormir")))
		{
			Prop->SetPropertyValue_InContainer(*It, Reglages.DistancePNJ);
			++Nb;
		}
	}
	if (Nb > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Optim] %d generateur(s) de passants regle(s) a %.0f m."), Nb, Reglages.DistancePNJ / 100.f);
	}
}

void AOptimisationChelles::VerifierPNJ()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const ENetMode Mode = W->GetNetMode();
	const bool bAutorite = Mode != NM_Client;          // solo ou serveur : decide des pauses
	const bool bImage = Mode != NM_DedicatedServer;   // une camera a afficher

	// Ou regardent les joueurs ? Serveur : tous les joueurs. Joueur en multi : seulement sa propre camera.
	TArray<FVector> Points;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		FVector Oeil;
		FRotator Vue;
		PC->GetPlayerViewPoint(Oeil, Vue);
		Points.Add(Oeil);
		if (APawn* Pion = PC->GetPawn())
		{
			Points.Add(Pion->GetActorLocation());
		}
	}
	if (Points.Num() == 0)
	{
		return; // menu, chargement : on ne change rien
	}

	const float D = Reglages.DistancePNJ;
	for (TActorIterator<ACharacter> It(W); It; ++It)
	{
		ACharacter* Perso = *It;
		if (!IsValid(Perso))
		{
			continue;
		}
		// Un joueur (soldat, Spider-Man, cheval monte, joueur distant...) n'est jamais touche
		const bool bJoueur = Perso->IsPlayerControlled() || Perso->GetPlayerState() != nullptr;
		if (bJoueur)
		{
			if (FEtatPNJ* Ancien = PNJ.Find(Perso))
			{
				RendreTout(Perso, *Ancien);
				PNJ.Remove(Perso);
			}
			continue;
		}

		FEtatPNJ& Etat = PNJ.FindOrAdd(Perso);
		if (bImage)
		{
			AppliquerDistanceAffichage(Perso, Etat); // invisible au-dela de D, pour chaque camera
		}

		float Plus = TNumericLimits<float>::Max();
		for (const FVector& P : Points)
		{
			Plus = FMath::Min(Plus, FVector::DistSquared(P, Perso->GetActorLocation()));
		}
		// Endormi un peu apres la limite d'affichage (105 %), reveille des qu'il peut etre vu (100 %)
		const float Seuil = Etat.bLoin ? D : D * 1.05f;
		const bool bLoin = Plus > Seuil * Seuil;
		if (bLoin && !Etat.bLoin)
		{
			Endormir(Perso, Etat, bAutorite);
		}
		else if (!bLoin && Etat.bLoin)
		{
			Reveiller(Perso, Etat);
		}
	}

	for (auto It = PNJ.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void AOptimisationChelles::AppliquerDistanceAffichage(ACharacter* Perso, FEtatPNJ& Etat)
{
	AjouterComposants(Perso, Etat);
	// Arme ou accessoire accroche au PNJ (ex. fusil des bandits)
	TArray<AActor*> Accroches;
	Perso->GetAttachedActors(Accroches);
	for (AActor* A : Accroches)
	{
		const APawn* Pion = Cast<APawn>(A);
		if (!IsValid(A) || (Pion && (Pion->IsPlayerControlled() || Pion->GetPlayerState())))
		{
			continue;
		}
		AjouterComposants(A, Etat);
	}
}

void AOptimisationChelles::AjouterComposants(AActor* Acteur, FEtatPNJ& Etat)
{
	TInlineComponentArray<UPrimitiveComponent*> Composants(Acteur);
	const float D = Reglages.DistancePNJ;
	for (UPrimitiveComponent* C : Composants)
	{
		if (!C)
		{
			continue;
		}
		bool bDeja = false;
		for (const FDistanceOrigine& O : Etat.Distances)
		{
			if (O.Composant.Get() == C)
			{
				bDeja = true;
				break;
			}
		}
		if (bDeja)
		{
			continue;
		}
		FDistanceOrigine O;
		O.Composant = C;
		O.Origine = C->LDMaxDrawDistance;
		Etat.Distances.Add(O);
		const float Nouvelle = O.Origine > 0.f ? FMath::Min(O.Origine, D) : D;
		C->SetCullDistance(Nouvelle);
	}
}

void AOptimisationChelles::Endormir(ACharacter* Perso, FEtatPNJ& Etat, bool bAutorite)
{
	Etat.bLoin = true;

	// Animation : plus calculee (chaque machine pour elle-meme)
	if (USkeletalMeshComponent* Mesh = Perso->GetMesh())
	{
		Etat.bTickMesh = Mesh->IsComponentTickEnabled();
		Mesh->SetComponentTickEnabled(false);
	}

	if (!bAutorite)
	{
		return; // en multi, seul le serveur met en pause le deplacement et l'IA
	}
	// Les zombies doivent pouvoir rejoindre les joueurs ; les passants ont deja leur propre pause.
	if (Perso->Tags.Contains(MarqueZombie) || Perso->GetClass()->GetName().Contains(TEXT("Passant")))
	{
		return;
	}
	Etat.bGele = true;
	if (UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement())
	{
		Etat.bTickMouvement = Mouvement->IsComponentTickEnabled();
		Mouvement->StopMovementImmediately();
		Mouvement->SetComponentTickEnabled(false);
	}
	Etat.bTickActeur = Perso->IsActorTickEnabled();
	Perso->SetActorTickEnabled(false);
	Etat.bLogiquePausee = false;
	if (AAIController* IA = Cast<AAIController>(Perso->GetController()))
	{
		UBrainComponent* Cerveau = IA->GetBrainComponent();
		if (Cerveau && Cerveau->IsRunning())
		{
			Cerveau->PauseLogic(RaisonPause.ToString());
			Etat.bLogiquePausee = true;
		}
	}
}

void AOptimisationChelles::Reveiller(ACharacter* Perso, FEtatPNJ& Etat)
{
	Etat.bLoin = false;
	if (USkeletalMeshComponent* Mesh = Perso->GetMesh())
	{
		Mesh->SetComponentTickEnabled(Etat.bTickMesh);
	}
	if (!Etat.bGele)
	{
		return;
	}
	Etat.bGele = false;
	if (UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement())
	{
		Mouvement->SetComponentTickEnabled(Etat.bTickMouvement);
	}
	Perso->SetActorTickEnabled(Etat.bTickActeur);
	if (Etat.bLogiquePausee)
	{
		Etat.bLogiquePausee = false;
		if (AAIController* IA = Cast<AAIController>(Perso->GetController()))
		{
			if (UBrainComponent* Cerveau = IA->GetBrainComponent())
			{
				Cerveau->ResumeLogic(RaisonPause.ToString());
			}
		}
	}
}

void AOptimisationChelles::RendreTout(ACharacter* Perso, FEtatPNJ& Etat)
{
	if (Etat.bLoin)
	{
		Reveiller(Perso, Etat);
	}
	for (const FDistanceOrigine& O : Etat.Distances)
	{
		if (UPrimitiveComponent* C = O.Composant.Get())
		{
			C->SetCullDistance(O.Origine);
		}
	}
	Etat.Distances.Reset();
}

// ------------------------------------------------------------------------------------------------
//  Fleurs, herbe, petites plantes
// ------------------------------------------------------------------------------------------------

bool AOptimisationChelles::RaccourcirInstances(UInstancedStaticMeshComponent* Comp, bool bVerifierTaille)
{
	UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return false;
	}
	if (bVerifierTaille)
	{
		const float Hauteur = Mesh->GetBounds().BoxExtent.Z * 2.f * FMath::Abs(Comp->GetComponentScale().Z);
		if (Hauteur > Reglages.TailleMaxFleurs)
		{
			return false; // arbre, grand buisson : on ne change rien
		}
	}
	const int32 Fin = FMath::RoundToInt(Reglages.DistanceFleurs);
	const int32 Debut = FMath::RoundToInt(Reglages.DistanceFleurs * 0.75f);
	if (Comp->InstanceEndCullDistance > 0 && Comp->InstanceEndCullDistance <= Fin)
	{
		return false; // deja plus court
	}
	Comp->SetCullDistances(Debut, Fin);
	return true;
}

void AOptimisationChelles::PremierPassageFleurs()
{
	const float D = Reglages.DistanceFleurs;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A))
		{
			continue;
		}
		const FString Classe = A->GetClass()->GetName();
		if (Classe.Contains(TEXT("InstancedFoliageActor")))
		{
			// Foliage peint (fleurs, herbe, buissons, arbres...) : seulement les petites plantes
			TInlineComponentArray<UInstancedStaticMeshComponent*> Comps(A);
			for (UInstancedStaticMeshComponent* C : Comps)
			{
				if (RaccourcirInstances(C, true))
				{
					++NbFoliage;
				}
			}
			continue;
		}
		if (Classe.Contains(TEXT("Landscape")))
		{
			Landscapes.Add(A); // herbe automatique du Landscape : voir VerifierHerbe
			continue;
		}
		// Plantes posees une par une (Static Mesh avec "flower", "plant", "grass"... dans le nom)
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps)
		{
			if (!C || C->IsA<UInstancedStaticMeshComponent>())
			{
				continue;
			}
			UStaticMesh* Mesh = C->GetStaticMesh();
			if (!Mesh || !NomDePlante(Mesh->GetName()))
			{
				continue;
			}
			const float Hauteur = Mesh->GetBounds().BoxExtent.Z * 2.f * FMath::Abs(C->GetComponentScale().Z);
			if (Hauteur > Reglages.TailleMaxFleurs)
			{
				continue;
			}
			if (C->LDMaxDrawDistance > 0.f && C->LDMaxDrawDistance <= D)
			{
				continue;
			}
			C->SetCullDistance(D);
			++NbPlantes;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Optim] Fleurs : %d groupe(s) de foliage et %d plante(s) affiches a %.0f m maximum, %d Landscape(s) surveille(s)."),
		NbFoliage, NbPlantes, D / 100.f, Landscapes.Num());
}

void AOptimisationChelles::VerifierHerbe()
{
	int32 Nouveaux = 0;
	for (const TWeakObjectPtr<AActor>& Faible : Landscapes)
	{
		AActor* A = Faible.Get();
		if (!A)
		{
			continue;
		}
		TInlineComponentArray<UInstancedStaticMeshComponent*> Comps(A);
		for (UInstancedStaticMeshComponent* C : Comps)
		{
			if (!C || HerbeTraitee.Contains(C))
			{
				continue;
			}
			HerbeTraitee.Add(C);
			if (RaccourcirInstances(C, false))
			{
				++Nouveaux;
			}
		}
	}
	if (HerbeTraitee.Num() > 4000)
	{
		for (auto It = HerbeTraitee.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	if (Nouveaux > 0)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Optim] Herbe : %d nouveau(x) groupe(s) raccourci(s)."), Nouveaux);
	}
}
