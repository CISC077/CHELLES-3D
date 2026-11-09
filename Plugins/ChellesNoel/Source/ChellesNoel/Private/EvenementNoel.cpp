#include "EvenementNoel.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNoel, Log, All);

namespace
{
	const FName TagBonnet(TEXT("ChellesNoelBonnet"));

	/** "01/12" -> jour 1, mois 12. */
	void LireJourMois(const FString& Texte, int32& Jour, int32& Mois)
	{
		FString J, M;
		if (Texte.Split(TEXT("/"), &J, &M))
		{
			const int32 NouveauJour = FCString::Atoi(*J.TrimStartAndEnd());
			const int32 NouveauMois = FCString::Atoi(*M.TrimStartAndEnd());
			if (NouveauJour >= 1 && NouveauJour <= 31 && NouveauMois >= 1 && NouveauMois <= 12)
			{
				Jour = NouveauJour;
				Mois = NouveauMois;
			}
		}
	}

	/** Os de la tete : "head", "Head", "mixamorig:Head", "CC_Base_Head", "Bip01-Head"... */
	FName TrouverOsTete(const USkeletalMeshComponent* Mesh)
	{
		FName Meilleur = NAME_None;
		for (int32 i = 0; i < Mesh->GetNumBones(); ++i)
		{
			const FName Os = Mesh->GetBoneName(i);
			const FString Bas = Os.ToString().ToLower();
			if (Bas == TEXT("head"))
			{
				return Os;
			}
			if (Meilleur.IsNone() && Bas.EndsWith(TEXT("head")) && !Bas.Contains(TEXT("end")) && !Bas.Contains(TEXT("top")))
			{
				Meilleur = Os;
			}
		}
		return Meilleur;
	}
}

AEvenementNoel::AEvenementNoel()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AEvenementNoel::EstDansLaPeriode(const FDateTime& Date, int32 JDebut, int32 MDebut, int32 JFin, int32 MFin)
{
	const int32 Aujourdhui = Date.GetMonth() * 100 + Date.GetDay();
	const int32 Debut = MDebut * 100 + JDebut;
	const int32 Fin = MFin * 100 + JFin;
	if (Debut <= Fin)
	{
		return Aujourdhui >= Debut && Aujourdhui <= Fin;
	}
	// la periode passe le nouvel an (ex. 01/12 -> 01/01)
	return Aujourdhui >= Debut || Aujourdhui <= Fin;
}

void AEvenementNoel::BeginPlay()
{
	Super::BeginPlay();

	LireReglages();
	const FDateTime Maintenant = FDateTime::Now();
	if (Mode.Equals(TEXT("Oui"), ESearchCase::IgnoreCase))
	{
		bNoelActif = true;
	}
	else if (Mode.Equals(TEXT("Non"), ESearchCase::IgnoreCase))
	{
		bNoelActif = false;
	}
	else
	{
		bNoelActif = EstDansLaPeriode(Maintenant, JourDebut, MoisDebut, JourFin, MoisFin);
	}
	UE_LOG(LogNoel, Log, TEXT("[Noel] date du PC %02d/%02d/%04d, mode %s, periode %02d/%02d -> %02d/%02d : Noel %s"),
		Maintenant.GetDay(), Maintenant.GetMonth(), Maintenant.GetYear(), *Mode,
		JourDebut, MoisDebut, JourFin, MoisFin, bNoelActif ? TEXT("ACTIF") : TEXT("inactif"));

	AppliquerSaison();
	// une 2e fois un peu plus tard : certains acteurs (empreintes) remettent leur Tick en route a leur BeginPlay
	GetWorldTimerManager().SetTimer(MinuterieSaison, this, &AEvenementNoel::AppliquerSaison, 0.5f, false);

	if (bNoelActif)
	{
		for (int32 Numero : NumerosBonnets)
		{
			const FString Chemin = FString::Printf(
				TEXT("/Game/Christmas_pack/Mesh/Decoration/SM_decoration_hat_%d.SM_decoration_hat_%d"), Numero, Numero);
			if (UStaticMesh* Bonnet = LoadObject<UStaticMesh>(nullptr, *Chemin, nullptr, LOAD_NoWarn))
			{
				Bonnets.Add(Bonnet);
			}
			else
			{
				UE_LOG(LogNoel, Warning, TEXT("[Noel] bonnet introuvable : %s"), *Chemin);
			}
		}
		UE_LOG(LogNoel, Log, TEXT("[Noel] %d bonnet(s) charge(s)"), Bonnets.Num());
		if (Bonnets.Num() > 0)
		{
			// les passants apparaissent petit a petit : on regarde regulierement
			GetWorldTimerManager().SetTimer(Minuterie, this, &AEvenementNoel::HabillerLesPassants, 1.5f, true, 1.f);
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor(255, 60, 60),
				TEXT("Joyeux Noel a Chelles ! Evenement du 1er decembre au 1er janvier"));
		}
	}
}

void AEvenementNoel::EndPlay(const EEndPlayReason::Type Raison)
{
	GetWorldTimerManager().ClearTimer(Minuterie);
	GetWorldTimerManager().ClearTimer(MinuterieSaison);
	Super::EndPlay(Raison);
}

void AEvenementNoel::LireReglages()
{
	const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesNoel"), TEXT("Noel.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
	{
		UE_LOG(LogNoel, Warning, TEXT("[Noel] %s introuvable : reglages par defaut (Auto, 01/12 -> 01/01)"), *Fichier);
		return;
	}
	for (FString Ligne : Lignes)
	{
		int32 Commentaire;
		if (Ligne.FindChar(TCHAR(';'), Commentaire))
		{
			Ligne = Ligne.Left(Commentaire);
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle = Cle.TrimStartAndEnd();
		Valeur = Valeur.TrimStartAndEnd();
		if (Cle.Equals(TEXT("Mode"), ESearchCase::IgnoreCase))
		{
			Mode = Valeur;
		}
		else if (Cle.Equals(TEXT("Debut"), ESearchCase::IgnoreCase))
		{
			LireJourMois(Valeur, JourDebut, MoisDebut);
		}
		else if (Cle.Equals(TEXT("Fin"), ESearchCase::IgnoreCase))
		{
			LireJourMois(Valeur, JourFin, MoisFin);
		}
		else if (Cle.Equals(TEXT("Bonnets"), ESearchCase::IgnoreCase))
		{
			TArray<FString> Morceaux;
			Valeur.ParseIntoArray(Morceaux, TEXT(","), true);
			TArray<int32> Numeros;
			for (const FString& Morceau : Morceaux)
			{
				const int32 Numero = FCString::Atoi(*Morceau.TrimStartAndEnd());
				if (Numero >= 1 && Numero <= 9)
				{
					Numeros.Add(Numero);
				}
			}
			if (Numeros.Num() > 0)
			{
				NumerosBonnets = Numeros;
			}
		}
		else if (Cle.Equals(TEXT("TailleBonnet"), ESearchCase::IgnoreCase))
		{
			TailleBonnet = FMath::Clamp(FCString::Atof(*Valeur), 5.f, 100.f);
		}
		else if (Cle.Equals(TEXT("Enfoncement"), ESearchCase::IgnoreCase))
		{
			Enfoncement = FMath::Clamp(FCString::Atof(*Valeur), -20.f, 30.f);
		}
		else if (Cle.Equals(TEXT("TournerBonnet"), ESearchCase::IgnoreCase))
		{
			TournerBonnet = FCString::Atof(*Valeur);
		}
		else if (Cle.Equals(TEXT("PencherBonnet"), ESearchCase::IgnoreCase))
		{
			PencherBonnet = FCString::Atof(*Valeur);
		}
	}
}

void AEvenementNoel::AppliquerSaison()
{
	// 1. sol blanc ou normal
	if (UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(CollectionNoel.TryLoad()))
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, ParametreSaison, bNoelActif ? 1.f : 0.f);
	}
	else
	{
		UE_LOG(LogNoel, Warning, TEXT("[Noel] %s introuvable : lancez Tools/Noel/INSTALLER_NOEL.bat (sol non change)"),
			*CollectionNoel.ToString());
	}

	// 2. flocons et empreintes : visibles et actifs seulement pendant Noel
	int32 NbActeurs = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Acteur = *It;
		if (Acteur && Acteur != this && Acteur->ActorHasTag(TagSaison))
		{
			Acteur->SetActorHiddenInGame(!bNoelActif);
			Acteur->SetActorTickEnabled(bNoelActif);
			++NbActeurs;
		}
	}
	UE_LOG(LogNoel, Log, TEXT("[Noel] %d acteur(s) de neige %s"), NbActeurs, bNoelActif ? TEXT("montres") : TEXT("caches"));
}

void AEvenementNoel::HabillerLesPassants()
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Perso = *It;
		if (!Perso || Perso->IsPlayerControlled() || Perso->ActorHasTag(TagBonnet))
		{
			continue;
		}
		if (!Perso->GetClass()->GetName().Contains(MotCleClasse))
		{
			continue;
		}
		if (PoserBonnet(Perso))
		{
			++NbBonnets;
			if (NbBonnets == 1 || NbBonnets % 10 == 0)
			{
				UE_LOG(LogNoel, Log, TEXT("[Noel] %d passant(s) avec un bonnet"), NbBonnets);
			}
		}
	}
}

bool AEvenementNoel::PoserBonnet(ACharacter* Passant)
{
	USkeletalMeshComponent* Corps = Passant->GetMesh();
	if (!Corps || !Corps->SkeletalMesh || Bonnets.Num() == 0)
	{
		return false; // pas encore d'apparence : on reessaiera
	}

	// deja en tenue de Noel (Christmas_Girl...) : rien a ajouter
	if (Corps->SkeletalMesh->GetPathName().Contains(MotCleDejaHabille))
	{
		Passant->Tags.AddUnique(TagBonnet);
		return false;
	}

	const FName OsTete = TrouverOsTete(Corps);

	// taille du passant (1,0 = 1m80)
	float Hauteur = 180.f;
	if (const UCapsuleComponent* Capsule = Passant->GetCapsuleComponent())
	{
		Hauteur = Capsule->GetScaledCapsuleHalfHeight() * 2.f;
	}
	const float Proportion = FMath::Clamp(Hauteur / 180.f, 0.5f, 2.f);

	// haut de la tete : le haut du personnage, verifie avec la position de l'os de la tete
	const FVector Tete = OsTete.IsNone() ? Passant->GetActorLocation() + FVector(0.f, 0.f, Hauteur * 0.4f)
		: Corps->GetSocketLocation(OsTete);
	const float HautCorps = Corps->Bounds.Origin.Z + Corps->Bounds.BoxExtent.Z;
	float HautTete = Tete.Z + 18.f * Proportion;
	if (!OsTete.IsNone() && HautCorps > Tete.Z + 6.f * Proportion && HautCorps < Tete.Z + 30.f * Proportion)
	{
		HautTete = HautCorps;
	}
	const FVector Pose(Tete.X, Tete.Y, HautTete - Enfoncement * Proportion);

	// bonnet choisi au hasard (toujours le meme pour un passant donne)
	UStaticMesh* Bonnet = Bonnets[int32(GetTypeHash(Passant->GetName()) % uint32(Bonnets.Num()))];
	const FBox Boite = Bonnet->GetBoundingBox();
	const FVector Taille = Boite.GetSize();
	const float Largeur = FMath::Max(Taille.X, Taille.Y);
	if (Largeur < 0.1f)
	{
		Passant->Tags.AddUnique(TagBonnet);
		return false;
	}
	const float Echelle = TailleBonnet * Proportion / Largeur;

	const FRotator Orientation(PencherBonnet, Passant->GetActorRotation().Yaw + TournerBonnet, 0.f);
	const FVector BasDuBonnet(Boite.GetCenter().X, Boite.GetCenter().Y, Boite.Min.Z);
	const FVector Position = Pose - Orientation.RotateVector(BasDuBonnet * Echelle);

	UStaticMeshComponent* Composant = NewObject<UStaticMeshComponent>(Passant, TEXT("BonnetDeNoel"));
	Composant->SetStaticMesh(Bonnet);
	Composant->SetMobility(EComponentMobility::Movable);
	Composant->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Composant->SetGenerateOverlapEvents(false);
	Composant->SetCanEverAffectNavigation(false);
	Composant->CanCharacterStepUpOn = ECB_No;
	Composant->RegisterComponent();
	Composant->SetWorldTransform(FTransform(Orientation, Position, FVector(Echelle)));
	Composant->AttachToComponent(Corps, FAttachmentTransformRules::KeepWorldTransform, OsTete);

	Passant->Tags.AddUnique(TagBonnet);
	return true;
}
