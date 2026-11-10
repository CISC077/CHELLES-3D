#include "EvenementNoel.h"
#include "CadeauxSauvegarde.h"
#include "SauvegardeJoueur.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"

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
		// cadeaux : un peu apres le debut, quand tout le decor est en place
		GetWorldTimerManager().SetTimer(MinuterieCadeaux, this, &AEvenementNoel::EparpillerLesCadeaux, 1.f, false);
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
	GetWorldTimerManager().ClearTimer(MinuterieCadeaux);
	GetWorldTimerManager().ClearTimer(MinuterieRamassage);
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
		else if (Cle.Equals(TEXT("Cadeaux"), ESearchCase::IgnoreCase))
		{
			NombreCadeaux = FMath::Clamp(FCString::Atoi(*Valeur), 0, 1000);
		}
		else if (Cle.Equals(TEXT("TailleCadeau"), ESearchCase::IgnoreCase))
		{
			// "30-60" (cm) ou une seule valeur
			FString A, B;
			if (Valeur.Split(TEXT("-"), &A, &B))
			{
				TailleCadeauMin = FMath::Clamp(FCString::Atof(*A), 5.f, 300.f);
				TailleCadeauMax = FMath::Clamp(FCString::Atof(*B), TailleCadeauMin, 300.f);
			}
			else
			{
				TailleCadeauMin = TailleCadeauMax = FMath::Clamp(FCString::Atof(*Valeur), 5.f, 300.f);
			}
		}
		else if (Cle.Equals(TEXT("XPParCadeau"), ESearchCase::IgnoreCase))
		{
			XPParCadeau = FMath::Clamp(FCString::Atoi(*Valeur), 0, 1000);
		}
		else if (Cle.Equals(TEXT("DistanceRamassage"), ESearchCase::IgnoreCase))
		{
			DistanceRamassage = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 1000.f);
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

void AEvenementNoel::EparpillerLesCadeaux()
{
	if (NombreCadeaux <= 0)
	{
		return;
	}
	TArray<UStaticMesh*> Paquets;
	for (int32 Numero = 1; Numero <= 8; ++Numero)
	{
		const FString Chemin = FString::Printf(
			TEXT("/Game/Christmas_pack/Mesh/Decoration/SM_decoration_present_%d.SM_decoration_present_%d"), Numero, Numero);
		if (UStaticMesh* Paquet = LoadObject<UStaticMesh>(nullptr, *Chemin, nullptr, LOAD_NoWarn))
		{
			Paquets.Add(Paquet);
		}
	}
	if (Paquets.Num() == 0)
	{
		UE_LOG(LogNoel, Warning, TEXT("[Noel] aucun cadeau trouve dans /Game/Christmas_pack/Mesh/Decoration"));
		return;
	}

	// zone de la ville = le Landscape (sinon 400 m autour du centre)
	FBox Zone(ForceInit);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It && It->GetClass()->GetName().Contains(TEXT("Landscape")))
		{
			Zone += It->GetComponentsBoundingBox();
		}
	}
	if (!Zone.IsValid || Zone.GetSize().X < 1000.f)
	{
		Zone = FBox(FVector(-40000.f, -40000.f, -10000.f), FVector(40000.f, 40000.f, 10000.f));
	}

	// memes places pour tous les joueurs, nouvelles places chaque jour
	const FDateTime Jour = FDateTime::Now();
	FRandomStream Hasard(Jour.GetYear() * 1000 + Jour.GetDayOfYear());

	// cadeaux deja ramasses aujourd'hui par CE joueur : ils ne sont pas reposes
	const bool bJoueurIci = GetNetMode() != NM_DedicatedServer && DistanceRamassage > 0.f;
	if (bJoueurIci)
	{
		ChargerCadeaux(Jour.ToString(TEXT("%Y-%m-%d")));
	}
	int32 DejaRamasses = 0;

	FCollisionQueryParams Requete(TEXT("CadeauxNoel"), false, this);
	const float Haut = Zone.Max.Z + 50000.f;
	const float Bas = Zone.Min.Z - 50000.f;
	int32 Poses = 0;
	for (int32 Essai = 0; Essai < NombreCadeaux * 30 && Poses < NombreCadeaux; ++Essai)
	{
		const float X = Hasard.FRandRange(Zone.Min.X, Zone.Max.X);
		const float Y = Hasard.FRandRange(Zone.Min.Y, Zone.Max.Y);
		FHitResult Impact;
		if (!GetWorld()->LineTraceSingleByChannel(Impact, FVector(X, Y, Haut), FVector(X, Y, Bas), ECC_Visibility, Requete))
		{
			continue;
		}
		// seulement par terre (Landscape) ou sur un trottoir : pas sur les toits ni sur les routes
		const AActor* Touche = Impact.GetActor();
		const FString NomActeur = Touche ? Touche->GetName() + Touche->GetClass()->GetName() : FString();
		const FString NomMesh = Impact.GetComponent() ? Impact.GetComponent()->GetName() : FString();
		const UStaticMeshComponent* SurMesh = Cast<UStaticMeshComponent>(Impact.GetComponent());
		const FString NomForme = (SurMesh && SurMesh->GetStaticMesh()) ? SurMesh->GetStaticMesh()->GetName() : FString();
		const bool bSol = NomActeur.Contains(TEXT("Landscape"));
		const bool bTrottoir = NomActeur.Contains(TEXT("Trottoir")) || NomMesh.Contains(TEXT("Trottoir")) || NomForme.Contains(TEXT("Trottoir"));
		if ((!bSol && !bTrottoir) || Impact.ImpactNormal.Z < 0.85f)
		{
			continue;
		}

		UStaticMesh* Paquet = Paquets[Hasard.RandRange(0, Paquets.Num() - 1)];
		const FBox Boite = Paquet->GetBoundingBox();
		const float Largeur = FMath::Max(Boite.GetSize().X, Boite.GetSize().Y);
		if (Largeur < 0.1f)
		{
			continue;
		}
		const float Echelle = Hasard.FRandRange(TailleCadeauMin, TailleCadeauMax) / Largeur;
		const FRotator Orientation(0.f, Hasard.FRandRange(0.f, 360.f), 0.f);
		const FVector Bas3(Boite.GetCenter().X, Boite.GetCenter().Y, Boite.Min.Z);
		const FVector Position = Impact.ImpactPoint - Orientation.RotateVector(Bas3 * Echelle);

		// numero du cadeau : le meme pour tous les joueurs ce jour-la (meme tirage au hasard)
		const int32 Numero = Poses;
		if (Ramasses.Contains(Numero))
		{
			++Poses;
			++DejaRamasses;
			continue;
		}

		UStaticMeshComponent* Cadeau = NewObject<UStaticMeshComponent>(this);
		Cadeau->SetStaticMesh(Paquet);
		Cadeau->SetMobility(EComponentMobility::Movable);
		Cadeau->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cadeau->SetGenerateOverlapEvents(false);
		Cadeau->SetCanEverAffectNavigation(false);
		Cadeau->SetWorldTransform(FTransform(Orientation, Position, FVector(Echelle)));
		Cadeau->RegisterComponent();
		CadeauxPoses.Add(Numero, Cadeau);
		++Poses;
	}
	UE_LOG(LogNoel, Log, TEXT("[Noel] %d cadeau(x) eparpille(s) (zone %.0f x %.0f m), %d deja ramasse(s) aujourd'hui"),
		Poses - DejaRamasses, Zone.GetSize().X / 100.f, Zone.GetSize().Y / 100.f, DejaRamasses);

	if (bJoueurIci && CadeauxPoses.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(MinuterieRamassage, this, &AEvenementNoel::RegarderRamassage, 0.2f, true);
	}
}

// ---------------------------------------------------------------------------
// Cadeaux ramasses : sauvegardes sur le PC du joueur
// ---------------------------------------------------------------------------

void AEvenementNoel::ChargerCadeaux(const FString& Jour)
{
	JourCadeaux = Jour;
	NomSauvegarde = ChellesSauvegarde::Emplacement(this, TEXT("ChellesNoel"));
	Ramasses.Reset();
	TotalRamasses = 0;
	if (UCadeauxSauvegarde* S = ChellesSauvegarde::Lire<UCadeauxSauvegarde>(NomSauvegarde))
	{
		TotalRamasses = FMath::Max(0, S->Total);
		if (S->Jour == JourCadeaux)
		{
			Ramasses.Append(S->Ramasses);
		}
	}
	UE_LOG(LogNoel, Log, TEXT("[Noel] %s.sav : %d cadeau(x) ramasse(s) aujourd'hui, %d au total"),
		*NomSauvegarde, Ramasses.Num(), TotalRamasses);
}

void AEvenementNoel::SauvegarderCadeaux()
{
	UCadeauxSauvegarde* S = Cast<UCadeauxSauvegarde>(UGameplayStatics::CreateSaveGameObject(UCadeauxSauvegarde::StaticClass()));
	if (!S || NomSauvegarde.IsEmpty())
	{
		return;
	}
	S->Jour = JourCadeaux;
	S->Ramasses = Ramasses.Array();
	S->Ramasses.Sort();
	S->Total = TotalRamasses;
	ChellesSauvegarde::Ecrire(S, NomSauvegarde);
}

void AEvenementNoel::RegarderRamassage()
{
	const UWorld* W = GetWorld();
	if (!W || W->IsPaused())
	{
		return;
	}
	const APawn* Pion = nullptr;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			Pion = PC->GetPawn();
			break;
		}
	}
	if (!Pion)
	{
		return;
	}
	const FVector Ici = Pion->GetActorLocation();
	const float Distance2 = DistanceRamassage * DistanceRamassage;
	for (auto It = CadeauxPoses.CreateIterator(); It; ++It)
	{
		UStaticMeshComponent* Cadeau = It.Value().Get();
		if (!Cadeau)
		{
			It.RemoveCurrent();
			continue;
		}
		const FVector Ou = Cadeau->GetComponentLocation();
		if (FVector::DistSquared2D(Ici, Ou) <= Distance2 && FMath::Abs(Ici.Z - Ou.Z) <= 250.f)
		{
			const int32 Numero = It.Key();
			It.RemoveCurrent();
			Ramasser(Numero, Cadeau);
		}
	}
	if (CadeauxPoses.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(MinuterieRamassage);
	}
}

void AEvenementNoel::Ramasser(int32 Numero, UStaticMeshComponent* Cadeau)
{
	if (Cadeau)
	{
		Cadeau->DestroyComponent();
	}
	Ramasses.Add(Numero);
	++TotalRamasses;
	SauvegarderCadeaux();

	if (USoundBase* Son = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuAccept_Stereo_Cue.MenuAccept_Stereo_Cue"), nullptr, LOAD_NoWarn))
	{
		UGameplayStatics::PlaySound2D(this, Son);
	}

	const FString Texte = FString::Printf(TEXT("cadeau de No\u00EBl ! (%d aujourd'hui, %d au total)"), Ramasses.Num(), TotalRamasses);
	UE_LOG(LogNoel, Log, TEXT("[Noel] cadeau %d ramasse (%d aujourd'hui, %d au total)"), Numero, Ramasses.Num(), TotalRamasses);
	if (!DonnerXP(XPParCadeau, Texte) && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor(255, 215, 80), TEXT("Tu as trouv\u00E9 un ") + Texte);
	}
}

bool AEvenementNoel::DonnerXP(int32 Quantite, const FString& Raison)
{
	if (Quantite <= 0)
	{
		return false;
	}
	// plugin ChellesPouvoirs : appel par le nom, pas de dependance de compilation entre plugins
	UClass* Classe = FindObject<UClass>(ANY_PACKAGE, TEXT("PouvoirsChelles"));
	if (!Classe)
	{
		return false;
	}
	for (TActorIterator<AActor> It(GetWorld(), Classe); It; ++It)
	{
		UFunction* Fonction = It->FindFunction(TEXT("GagnerXPBonus"));
		if (!Fonction)
		{
			return false;
		}
		struct FParametres
		{
			int32 Quantite;
			FString Raison;
		};
		FParametres Parametres{ Quantite, Raison };
		if (Fonction->ParmsSize != sizeof(FParametres))
		{
			UE_LOG(LogNoel, Warning, TEXT("[Noel] GagnerXPBonus a change dans ChellesPouvoirs : XP du cadeau non donnee"));
			return false;
		}
		It->ProcessEvent(Fonction, &Parametres);
		return true;
	}
	return false;
}
