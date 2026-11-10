// CHELLES 3D - XP et pouvoirs : gagner de l'XP en jouant, debloquer et utiliser des pouvoirs (UE 4.27)
#include "PouvoirsChelles.h"
#include "PouvoirsRelais.h"
#include "PouvoirsTypes.h"
#include "BouleDeFeuChelles.h"
#include "XPSauvegarde.h"
#include "SauvegardeJoueur.h"
#include "SPouvoirsHud.h"

#include "AIController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BrainComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundBase.h"
#include "Modules/ModuleManager.h"

using namespace ChellesPouvoirs;

namespace
{
	const TCHAR* EmplacementXP = TEXT("ChellesXP");

	FString TempsDeJeuAvant(int32 XPVoulue, int32 XPActuelle)
	{
		const int32 Minutes = FMath::Max(1, FMath::DivideAndRoundUp(XPVoulue - XPActuelle, XPParMinute));
		if (Minutes >= 60)
		{
			return FString::Printf(TEXT("%d h %02d"), Minutes / 60, Minutes % 60);
		}
		return FString::Printf(TEXT("%d min"), Minutes);
	}

	/** Le pion et tout ce qui y est accroche (armes, sacs...), sur 3 niveaux. */
	void ActeursDuPion(AActor* Racine, TArray<AActor*>& Sortie, int32 Profondeur = 0)
	{
		if (!Racine || Sortie.Contains(Racine) || Profondeur > 3)
		{
			return;
		}
		Sortie.Add(Racine);
		TArray<AActor*> Attaches;
		Racine->GetAttachedActors(Attaches);
		for (AActor* A : Attaches)
		{
			ActeursDuPion(A, Sortie, Profondeur + 1);
		}
	}
}

APouvoirsChelles::APouvoirsChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // pour cacher la barre d'XP quand le menu met en pause
	bReplicates = false; // un par machine : les choses partagees passent par APouvoirsRelais
	SetActorHiddenInGame(true);
	ToucheUtiliser = EKeys::J;
	ToucheChanger = EKeys::Y;
}

APlayerController* APouvoirsChelles::GetPC() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

bool APouvoirsChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

APouvoirsRelais* APouvoirsChelles::MonRelais() const
{
	APlayerController* PC = GetPC();
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

void APouvoirsChelles::Message(const FString& Texte)
{
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] %s"), *Texte);
	Info = Texte;
	AgeInfo = 0.f;
}

// ---------------------------------------------------------------------------
// Demarrage / fin
// ---------------------------------------------------------------------------

void APouvoirsChelles::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		Charger();
	}
	LierClavier();
	CreerHud();
}

void APouvoirsChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (bCharge)
	{
		Sauvegarder();
	}
	RetirerHud();
	Super::EndPlay(Raison);
}

void APouvoirsChelles::LierClavier()
{
	if (bLieClavier)
	{
		return;
	}
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	EnableInput(PC);
	if (!InputComponent)
	{
		return;
	}
	if (ToucheUtiliser.IsValid())
	{
		FInputKeyBinding& B = InputComponent->BindKey(ToucheUtiliser, IE_Pressed, this, &APouvoirsChelles::Utiliser);
		B.bConsumeInput = false;
	}
	if (ToucheChanger.IsValid())
	{
		FInputKeyBinding& B = InputComponent->BindKey(ToucheChanger, IE_Pressed, this, &APouvoirsChelles::Changer);
		B.bConsumeInput = false;
	}
	bLieClavier = true;
}

// ---------------------------------------------------------------------------
// XP du joueur de cette machine
// ---------------------------------------------------------------------------

void APouvoirsChelles::Charger()
{
	NomSauvegarde = ChellesSauvegarde::Emplacement(this, EmplacementXP);
	if (UXPSauvegarde* S = ChellesSauvegarde::Lire<UXPSauvegarde>(NomSauvegarde))
	{
		XPLocale = FMath::Max(0, S->XP);
		SecondesJouees = FMath::Max(0.0, S->SecondesJouees);
		Selection = FMath::Clamp(S->Selection, 0, int32(NombrePouvoirs) - 1);
	}
	NiveauAnnonce = NiveauPourXP(XPLocale);
	bCharge = true;
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] XP chargee depuis %s.sav : %d (niveau %d, %.0f min de jeu)."),
		*NomSauvegarde, XPLocale, NiveauAnnonce, SecondesJouees / 60.0);
	if (XPLocale > 0)
	{
		Message(FString::Printf(TEXT("Progression retrouv\u00E9e : niveau %d, %d XP"), NiveauAnnonce, XPLocale));
	}
}

void APouvoirsChelles::Sauvegarder()
{
	if (!bCharge)
	{
		return; // jamais ecraser la sauvegarde avant de l'avoir lue
	}
	UXPSauvegarde* S = Cast<UXPSauvegarde>(UGameplayStatics::CreateSaveGameObject(UXPSauvegarde::StaticClass()));
	if (!S)
	{
		return;
	}
	S->XP = XPLocale;
	S->SecondesJouees = SecondesJouees;
	S->Selection = Selection;
	ChellesSauvegarde::Ecrire(S, NomSauvegarde);
}

bool APouvoirsChelles::EnJeu() const
{
	APlayerController* PC = GetPC();
	if (!PC || !PC->GetPawn() || PC->IsPaused())
	{
		return false;
	}
	// Souris visible = menu, chat ou options ouverts : on ne compte pas
	return !PC->bShowMouseCursor;
}

void APouvoirsChelles::CompterTemps(float Secondes)
{
	const int32 HeuresAvant = int32(SecondesJouees / 3600.0);
	SecondesJouees += Secondes;
	SecondesMinute += Secondes;
	while (SecondesMinute >= 60.f)
	{
		SecondesMinute -= 60.f;
		DonnerXP(XPParMinute, true);
	}
	if (int32(SecondesJouees / 3600.0) > HeuresAvant)
	{
		DonnerXP(BonusParHeure, true);
		Message(FString::Printf(TEXT("+%d XP : bonus pour une heure de jeu !"), BonusParHeure));
	}
}

void APouvoirsChelles::DonnerXP(int32 Quantite, bool bTempsDeJeu)
{
	XPLocale = FMath::Max(0, XPLocale + Quantite);
	const int32 Niveau = NiveauPourXP(XPLocale);
	if (Niveau > NiveauAnnonce)
	{
		Banniere = FString::Printf(TEXT("NIVEAU %d : %s"), Niveau, Paliers()[Niveau - 1].Titre);
		SousBanniere.Empty();
		for (int32 i = 0; i < NombrePouvoirs; ++i)
		{
			if (Pouvoir(i).Niveau == Niveau)
			{
				SousBanniere = FString::Printf(TEXT("NOUVEAU POUVOIR : %s  -  touche J"), Pouvoir(i).Nom);
				Selection = i;
			}
		}
		AgeBanniere = 0.f;
		if (USoundBase* Son = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuAccept_Stereo_Cue.MenuAccept_Stereo_Cue")))
		{
			UGameplayStatics::PlaySound2D(this, Son);
		}
		UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Niveau %d atteint (%d XP)."), Niveau, XPLocale);
	}
	NiveauAnnonce = Niveau;
	Sauvegarder();
}

void APouvoirsChelles::GagnerXPBonus(int32 Quantite, const FString& Raison)
{
	if (!bCharge || Quantite == 0)
	{
		return;
	}
	DonnerXP(Quantite, false); // sauvegarde tout de suite ; le serveur recoit la nouvelle XP au Tick
	if (!Raison.IsEmpty())
	{
		Message(FString::Printf(TEXT("+%d XP : %s"), Quantite, *Raison));
	}
}

void APouvoirsChelles::AjouterXPTest(int32 Quantite)
{
	DonnerXP(Quantite, false);
	Message(FString::Printf(TEXT("Test : %+d XP (total %d)"), Quantite, XPLocale));
	if (APouvoirsRelais* R = MonRelais())
	{
		R->ServeurXP(XPLocale);
	}
}

// ---------------------------------------------------------------------------
// Touches J et Y
// ---------------------------------------------------------------------------

void APouvoirsChelles::Changer()
{
	if (!EnJeu())
	{
		return;
	}
	for (int32 Pas = 1; Pas <= NombrePouvoirs; ++Pas)
	{
		const int32 Suivant = (Selection + Pas) % NombrePouvoirs;
		if (EstDebloque(Suivant, XPLocale))
		{
			Selection = Suivant;
			Message(FString::Printf(TEXT("%s : %s"), Pouvoir(Selection).Nom, Pouvoir(Selection).Aide));
			Sauvegarder(); // le pouvoir choisi est retrouve a la prochaine partie
			return;
		}
	}
	Message(FString::Printf(TEXT("Aucun pouvoir pour l'instant : %s s'ouvre dans %s de jeu."),
		Pouvoir(0).Nom, *TempsDeJeuAvant(Paliers()[Pouvoir(0).Niveau - 1].XP, XPLocale)));
}

void APouvoirsChelles::Utiliser()
{
	if (!EnJeu())
	{
		return;
	}
	APlayerController* PC = GetPC();
	APouvoirsRelais* R = MonRelais();
	if (!PC || !R)
	{
		Message(TEXT("Les pouvoirs se pr\u00E9parent, r\u00E9essaie dans une seconde."));
		return;
	}
	const FInfoPouvoir& P = Pouvoir(Selection);
	if (!EstDebloque(Selection, XPLocale))
	{
		Message(FString::Printf(TEXT("%s s'ouvre au niveau %d, dans %s de jeu."), P.Nom, P.Niveau,
			*TempsDeJeuAvant(Paliers()[P.Niveau - 1].XP, XPLocale)));
		return;
	}
	if (R->EffetActif(Selection) && Selection != Cameleon)
	{
		return;
	}
	const float Attente = R->RestantRecharge(Selection);
	if (Attente > 0.f)
	{
		Message(FString::Printf(TEXT("%s se recharge : encore %d s"), P.Nom, FMath::CeilToInt(Attente)));
		return;
	}
	if (R->XP != XPLocale)
	{
		R->ServeurXP(XPLocale); // arrive au serveur avant la demande de pouvoir
	}
	FVector Origine;
	FRotator Visee;
	PC->GetPlayerViewPoint(Origine, Visee);
	R->ServeurPouvoir(uint8(Selection), Origine, Visee);
}

// ---------------------------------------------------------------------------
// Serveur
// ---------------------------------------------------------------------------

bool APouvoirsChelles::PvpActif() const
{
	if (GetNetMode() == NM_Standalone)
	{
		return true; // en solo il n'y a que des passants et des ennemis
	}
	bool bPvp = true; // reglage PVP du menu (celui qui heberge decide)
	if (GConfig)
	{
		GConfig->GetBool(TEXT("ChellesMenu"), TEXT("Pvp"), bPvp, GGameUserSettingsIni);
	}
	return bPvp;
}

void APouvoirsChelles::GererRelais()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	TArray<APlayerController*> Servis;
	TArray<APouvoirsRelais*> Relais;
	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		Relais.Add(*It);
	}
	for (APouvoirsRelais* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			R->Destroy(); // joueur parti
			continue;
		}
		Servis.Add(PC);
		APawn* Actuel = PC->GetPawn();
		if (R->Pion != Actuel)
		{
			R->Pion = Actuel;
			R->ForceNetUpdate();
		}
		if (R->CibleGel && !R->EffetActif(Gel))
		{
			R->CibleGel = nullptr;
		}
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || Servis.Contains(PC))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APouvoirsRelais* Nouveau = W->SpawnActor<APouvoirsRelais>(APouvoirsRelais::StaticClass(), FTransform::Identity, Params))
		{
			Nouveau->Pion = PC->GetPawn();
			UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Pouvoirs prets pour le joueur %s."), *PC->GetName());
		}
	}
}

APawn* APouvoirsChelles::ViserCible(APawn* Lanceur, const FVector& Origine, const FVector& Direction) const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	FCollisionQueryParams Requete(SCENE_QUERY_STAT(GelChelles), false, Lanceur);
	TArray<AActor*> AIgnorer;
	ActeursDuPion(Lanceur, AIgnorer);
	Requete.AddIgnoredActors(AIgnorer);
	FHitResult Impact;
	const FVector Fin = Origine + Direction * 3000.f; // 30 m
	if (!W->SweepSingleByChannel(Impact, Origine, Fin, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(45.f), Requete))
	{
		return nullptr;
	}
	AActor* Touche = Impact.GetActor();
	for (int32 i = 0; Touche && i < 4; ++i)
	{
		if (APawn* Cible = Cast<APawn>(Touche))
		{
			return Cible;
		}
		Touche = Touche->GetAttachParentActor() ? Touche->GetAttachParentActor() : Touche->GetOwner();
	}
	return nullptr;
}

const TArray<FString>& APouvoirsChelles::ApparencesPour(USkeletalMeshComponent* Corps)
{
	static const TArray<FString> Vide;
	if (!Corps || !Corps->SkeletalMesh || !Corps->SkeletalMesh->GetSkeleton())
	{
		return Vide;
	}
	const FString Squelette = Corps->SkeletalMesh->GetSkeleton()->GetPathName();
	if (const TArray<FString>* Deja = ApparencesParSquelette.Find(Squelette))
	{
		return *Deja;
	}
	TArray<FString>& Liste = ApparencesParSquelette.Add(Squelette);
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Meshes;
	Registre.GetAssetsByClass(USkeletalMesh::StaticClass()->GetFName(), Meshes, true);
	const TCHAR* Exclus[] = { TEXT("Arms"), TEXT("FP_"), TEXT("FPP"), TEXT("Hand"), TEXT("Weapon"), TEXT("Head"), TEXT("Hair"), TEXT("Shoe"), TEXT("Hat"), TEXT("Glove"), TEXT("Upper"), TEXT("Lower"), TEXT("Legs"), TEXT("Apercu") };
	for (const FAssetData& A : Meshes)
	{
		FString SonSquelette;
		if (!A.GetTagValue(FName(TEXT("Skeleton")), SonSquelette) || !SonSquelette.Contains(Squelette))
		{
			continue;
		}
		const FString Nom = A.AssetName.ToString();
		bool bExclu = false;
		for (const TCHAR* Mot : Exclus)
		{
			if (Nom.Contains(Mot))
			{
				bExclu = true;
				break;
			}
		}
		if (!bExclu)
		{
			Liste.Add(A.ObjectPath.ToString());
		}
	}
	Liste.Sort();
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] CAMELEON : %d apparences compatibles avec %s."), Liste.Num(), *Squelette);
	return Liste;
}

bool APouvoirsChelles::ServeurCameleon(APouvoirsRelais* Relais, APawn* Pion)
{
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (!Perso || !Perso->GetMesh())
	{
		Relais->ClientAvis(TEXT("CAM\u00C9L\u00C9ON marche seulement \u00E0 pied."));
		return false;
	}
	const TArray<FString>& Liste = ApparencesPour(Perso->GetMesh());
	const FString Origine = Perso->GetMesh()->SkeletalMesh ? Perso->GetMesh()->SkeletalMesh->GetPathName() : FString();
	if (Liste.Num() == 0 || (Liste.Num() == 1 && Liste[0] == Origine))
	{
		Relais->ClientAvis(TEXT("Aucune autre apparence compatible trouv\u00E9e pour ce personnage."));
		return false;
	}
	// Deguise -> on redevient soi-meme. Sinon -> une apparence au hasard (differente de l'origine).
	if (!Relais->Deguisement.IsEmpty())
	{
		Relais->IndexDeguisement = 0;
		Relais->Deguisement.Empty();
		Relais->ClientAvis(TEXT("CAM\u00C9L\u00C9ON : tu reprends ton apparence."));
		return true;
	}
	int32 Choix = FMath::RandRange(0, Liste.Num() - 1);
	if (Liste[Choix] == Origine)
	{
		Choix = (Choix + 1) % Liste.Num();
	}
	Relais->IndexDeguisement = Choix + 1;
	Relais->Deguisement = Liste[Choix];
	Relais->ClientAvis(FString::Printf(TEXT("CAM\u00C9L\u00C9ON : te voil\u00E0 en %s  (J = redevenir toi)"),
		*FPackageName::ObjectPathToObjectName(Relais->Deguisement)));
	return true;
}

void APouvoirsChelles::ExecuterServeur(APouvoirsRelais* Relais, int32 Quel, FVector Origine, FRotator Visee)
{
	if (!Relais || Quel < 0 || Quel >= NombrePouvoirs)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	const FInfoPouvoir& P = Pouvoir(Quel);
	if (!EstDebloque(Quel, Relais->XP))
	{
		Relais->ClientAvis(FString::Printf(TEXT("%s pas encore d\u00E9bloqu\u00E9 (niveau %d)."), P.Nom, P.Niveau));
		return;
	}
	if (Relais->RestantRecharge(Quel) > 0.f)
	{
		return;
	}
	// Point de visee trop loin du personnage : on prend sa tete (anti-triche simple)
	if (FVector::Dist(Origine, Pion->GetActorLocation()) > 1000.f)
	{
		Origine = Pion->GetPawnViewLocation();
		Visee = PC->GetControlRotation();
	}
	const FVector Direction = Visee.Vector();
	const float Maintenant = Relais->Maintenant();

	switch (Quel)
	{
	case Envol:
		if (!Cast<ACharacter>(Pion))
		{
			Relais->ClientAvis(TEXT("Descends du v\u00E9hicule pour t'envoler."));
			return;
		}
		break;

	case BouleDeFeu:
	{
		FActorSpawnParameters Params;
		Params.Owner = Pion;
		Params.Instigator = Pion;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABouleDeFeuChelles* Boule = GetWorld()->SpawnActor<ABouleDeFeuChelles>(ABouleDeFeuChelles::StaticClass(),
			Origine + Direction * 110.f, Visee, Params);
		if (!Boule)
		{
			return;
		}
		Boule->bToucherJoueurs = PvpActif();
		break;
	}

	case Verre:
		break;

	case Cameleon:
		if (!ServeurCameleon(Relais, Pion))
		{
			return;
		}
		break;

	case Gel:
	{
		APawn* Cible = ViserCible(Pion, Origine, Direction);
		if (!Cible)
		{
			Relais->ClientAvis(TEXT("GEL : vise un joueur ou un passant (30 m maximum)."));
			return;
		}
		if (Cible->IsPlayerControlled() && !PvpActif())
		{
			Relais->ClientAvis(TEXT("GEL : le PVP est coup\u00E9 dans cette partie, impossible de geler un joueur."));
			return;
		}
		Relais->CibleGel = Cible;
		if (APlayerController* Victime = Cast<APlayerController>(Cible->GetController()))
		{
			for (TActorIterator<APouvoirsRelais> It(GetWorld()); It; ++It)
			{
				if (It->GetOwner() == Victime)
				{
					It->ClientAvis(FString::Printf(TEXT("%s t'a gel\u00E9 !"),
						PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("Un joueur")));
				}
			}
		}
		break;
	}

	default:
		return;
	}

	Relais->FinEffet[Quel] = Maintenant + P.Duree;
	Relais->FinRecharge[Quel] = Maintenant + P.Duree + P.Recharge;
	Relais->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] %s utilise %s."), *PC->GetName(), P.Nom);
}

// ---------------------------------------------------------------------------
// Effets vus sur cette machine (pour tous les joueurs)
// ---------------------------------------------------------------------------

bool APouvoirsChelles::AppliquerApparence(APawn* Pion, const FString& Chemin)
{
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (!Perso || Chemin.IsEmpty())
	{
		return false;
	}
	USkeletalMeshComponent* Corps = Perso->GetMesh();
	if (!Corps || !Corps->SkeletalMesh)
	{
		return false;
	}
	FDeguisementChelles* D = Deguises.Find(Pion);
	if (!D || D->Chemin != Chemin)
	{
		USkeletalMesh* Mesh = FindObject<USkeletalMesh>(nullptr, *Chemin);
		if (!Mesh)
		{
			Mesh = LoadObject<USkeletalMesh>(nullptr, *Chemin);
			if (Mesh)
			{
				MeshesCharges.AddUnique(Mesh);
			}
		}
		if (!Mesh || Mesh->GetSkeleton() != Corps->SkeletalMesh->GetSkeleton())
		{
			return false;
		}
		if (!D)
		{
			USkeletalMeshComponent* Copie = NewObject<USkeletalMeshComponent>(Perso, MakeUniqueObjectName(Perso, USkeletalMeshComponent::StaticClass(), TEXT("ChellesCameleon")));
			Copie->SetupAttachment(Corps);
			Copie->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copie->SetOwnerNoSee(Corps->bOwnerNoSee);
			Copie->SetOnlyOwnerSee(Corps->bOnlyOwnerSee);
			Copie->SetSkeletalMesh(Mesh);
			Copie->RegisterComponent();
			Copie->SetMasterPoseComponent(Corps);

			FDeguisementChelles Nouveau;
			Nouveau.Copie = Copie;
			Nouveau.Corps = Corps;
			Nouveau.AncienTickAnim = uint8(Corps->VisibilityBasedAnimTickOption);
			Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			D = &Deguises.Add(Pion, MoveTemp(Nouveau));
		}
		else if (D->Copie.IsValid())
		{
			D->Copie->SetSkeletalMesh(Mesh, false);
		}
		D->Chemin = Chemin;
	}

	// L'ancienne apparence (corps + morceaux d'habillage qui le suivent) reste cachee
	TArray<USkinnedMeshComponent*> Peaux;
	Perso->GetComponents<USkinnedMeshComponent>(Peaux);
	for (USkinnedMeshComponent* S : Peaux)
	{
		if (!S || S == D->Copie.Get() || S->bOnlyOwnerSee)
		{
			continue;
		}
		const bool bAncienne = (S == Corps) || (S->MasterPoseComponent.Get() == Corps);
		if (bAncienne && !S->bHiddenInGame)
		{
			S->SetHiddenInGame(true);
			D->MorceauxCaches.AddUnique(S);
		}
	}
	return true;
}

void APouvoirsChelles::RetirerApparence(APawn* Pion)
{
	FDeguisementChelles* D = Deguises.Find(Pion);
	if (!D)
	{
		return;
	}
	if (D->Copie.IsValid())
	{
		D->Copie->DestroyComponent();
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : D->MorceauxCaches)
	{
		if (C.IsValid())
		{
			C->SetHiddenInGame(false);
		}
	}
	if (D->Corps.IsValid())
	{
		D->Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption(D->AncienTickAnim);
	}
	Deguises.Remove(Pion);
}

void APouvoirsChelles::Degeler(APawn* Pion)
{
	TWeakObjectPtr<UPointLightComponent>* Lumiere = Geles.Find(Pion);
	if (Lumiere && Lumiere->IsValid())
	{
		(*Lumiere)->DestroyComponent();
	}
	Geles.Remove(Pion);
	if (!Pion)
	{
		return;
	}
	Pion->CustomTimeDilation = 1.f;
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (Perso && (EstServeur() || Perso->IsLocallyControlled()))
	{
		UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement();
		if (Mouvement && Mouvement->MovementMode == MOVE_None)
		{
			Mouvement->SetMovementMode(MOVE_Falling);
		}
	}
	if (EstServeur())
	{
		if (AAIController* IA = Cast<AAIController>(Pion->GetController()))
		{
			if (UBrainComponent* Cerveau = IA->GetBrainComponent())
			{
				Cerveau->ResumeLogic(TEXT("Gel Chelles"));
			}
		}
	}
}

void APouvoirsChelles::AppliquerEffets(float DeltaSeconds)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const bool bServeur = EstServeur();
	APlayerController* MonPC = GetPC();

	TSet<APawn*> DoiventVoler;
	TSet<APawn*> DoiventGeler;
	TSet<UPrimitiveComponent*> ACacher;
	TSet<APawn*> Deguisables;
	float Heure = 0.f;

	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		APouvoirsRelais* R = *It;
		Heure = R->Maintenant();
		APawn* Pion = R->Pion;
		if (bServeur)
		{
			if (APlayerController* PC = Cast<APlayerController>(R->GetOwner()))
			{
				Pion = PC->GetPawn(); // le serveur connait le pion a jour
			}
		}

		if (R->EffetActif(Gel))
		{
			if (APawn* Cible = Cast<APawn>(R->CibleGel))
			{
				DoiventGeler.Add(Cible);
			}
		}
		if (!Pion)
		{
			continue;
		}
		if (R->EffetActif(Envol) && Cast<ACharacter>(Pion) && (bServeur || Pion->IsLocallyControlled()))
		{
			DoiventVoler.Add(Pion);
		}
		if (R->EffetActif(Verre) && !Pion->IsLocallyControlled())
		{
			TArray<AActor*> Acteurs;
			ActeursDuPion(Pion, Acteurs);
			for (AActor* A : Acteurs)
			{
				TArray<UPrimitiveComponent*> Morceaux;
				A->GetComponents<UPrimitiveComponent>(Morceaux);
				for (UPrimitiveComponent* C : Morceaux)
				{
					ACacher.Add(C);
				}
			}
		}
		if (AppliquerApparence(Pion, R->Deguisement))
		{
			Deguisables.Add(Pion);
		}
	}

	// --- ENVOL ---
	for (APawn* Pion : DoiventVoler)
	{
		if (DoiventGeler.Contains(Pion))
		{
			continue; // le gel gagne
		}
		ACharacter* Perso = Cast<ACharacter>(Pion);
		UCharacterMovementComponent* Mouvement = Perso ? Perso->GetCharacterMovement() : nullptr;
		if (!Mouvement)
		{
			continue;
		}
		if (Mouvement->MovementMode != MOVE_Flying)
		{
			Mouvement->SetMovementMode(MOVE_Flying);
			Mouvement->MaxFlySpeed = FMath::Max(Mouvement->MaxFlySpeed, 1400.f);
			Mouvement->BrakingDecelerationFlying = FMath::Max(Mouvement->BrakingDecelerationFlying, 1500.f);
		}
		Volants.Add(Pion);
		if (MonPC && Perso->IsLocallyControlled())
		{
			float Vertical = 0.f;
			if (MonPC->IsInputKeyDown(EKeys::SpaceBar) || MonPC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom))
			{
				Vertical += 1.f;
			}
			if (MonPC->IsInputKeyDown(EKeys::LeftControl) || MonPC->IsInputKeyDown(EKeys::C) || MonPC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Right))
			{
				Vertical -= 1.f;
			}
			if (Vertical != 0.f)
			{
				Perso->AddMovementInput(FVector::UpVector, Vertical);
			}
		}
	}
	for (auto ItV = Volants.CreateIterator(); ItV; ++ItV)
	{
		APawn* Pion = ItV->Get();
		if (Pion && DoiventVoler.Contains(Pion) && !DoiventGeler.Contains(Pion))
		{
			continue;
		}
		if (ACharacter* Perso = Cast<ACharacter>(Pion))
		{
			if (Perso->GetCharacterMovement() && Perso->GetCharacterMovement()->MovementMode == MOVE_Flying)
			{
				Perso->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		ItV.RemoveCurrent();
	}

	// --- EFFET VERRE : invisible, avec un reflet bref toutes les 1,7 s ---
	const bool bReflet = FMath::Fmod(Heure, 1.7f) < 0.08f;
	for (UPrimitiveComponent* C : ACacher)
	{
		const TWeakObjectPtr<UPrimitiveComponent> Faible(C);
		if (!MorceauxCaches.Contains(Faible))
		{
			if (C->bHiddenInGame)
			{
				continue; // deja cache par le jeu : on n'y touche pas
			}
			MorceauxCaches.Add(Faible);
		}
		const bool bDejaCache = C->bHiddenInGame;
		if (bDejaCache == bReflet)
		{
			C->SetHiddenInGame(!bReflet);
		}
	}
	for (auto ItC = MorceauxCaches.CreateIterator(); ItC; ++ItC)
	{
		UPrimitiveComponent* C = ItC->Get();
		if (C && ACacher.Contains(C))
		{
			continue;
		}
		if (C)
		{
			C->SetHiddenInGame(false);
		}
		ItC.RemoveCurrent();
	}

	// --- GEL ---
	for (APawn* Cible : DoiventGeler)
	{
		if (!Geles.Contains(Cible))
		{
			Cible->CustomTimeDilation = 0.02f; // l'animation se fige
			UPointLightComponent* Lumiere = NewObject<UPointLightComponent>(Cible, MakeUniqueObjectName(Cible, UPointLightComponent::StaticClass(), TEXT("ChellesGel")));
			Lumiere->SetupAttachment(Cible->GetRootComponent());
			Lumiere->SetIntensity(5000.f);
			Lumiere->SetLightColor(FLinearColor(0.45f, 0.75f, 1.f));
			Lumiere->SetAttenuationRadius(320.f);
			Lumiere->SetCastShadows(false);
			Lumiere->RegisterComponent();
			Geles.Add(Cible, Lumiere);
			if (bServeur)
			{
				if (AAIController* IA = Cast<AAIController>(Cible->GetController()))
				{
					IA->StopMovement();
					if (UBrainComponent* Cerveau = IA->GetBrainComponent())
					{
						Cerveau->PauseLogic(TEXT("Gel Chelles"));
					}
				}
			}
		}
		ACharacter* Perso = Cast<ACharacter>(Cible);
		if (Perso && (bServeur || Perso->IsLocallyControlled()))
		{
			UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement();
			if (Mouvement && Mouvement->MovementMode != MOVE_None)
			{
				Mouvement->StopMovementImmediately();
				Mouvement->DisableMovement();
			}
		}
	}
	TArray<TWeakObjectPtr<APawn>> AncienGeles;
	Geles.GetKeys(AncienGeles);
	for (const TWeakObjectPtr<APawn>& Faible : AncienGeles)
	{
		APawn* Pion = Faible.Get();
		if (!Pion || !DoiventGeler.Contains(Pion))
		{
			if (Pion)
			{
				Degeler(Pion);
			}
			else
			{
				Geles.Remove(Faible);
			}
		}
	}

	// --- CAMELEON : rendre l'apparence d'origine a ceux qui ne sont plus deguises ---
	TArray<TWeakObjectPtr<APawn>> AnciensDeguises;
	Deguises.GetKeys(AnciensDeguises);
	for (const TWeakObjectPtr<APawn>& Faible : AnciensDeguises)
	{
		APawn* Pion = Faible.Get();
		if (!Pion)
		{
			Deguises.Remove(Faible);
		}
		else if (!Deguisables.Contains(Pion))
		{
			RetirerApparence(Pion);
		}
	}
}

// ---------------------------------------------------------------------------
// Affichage
// ---------------------------------------------------------------------------

void APouvoirsChelles::CreerHud()
{
	if (Hud.IsValid() || !bAfficher || GetNetMode() == NM_DedicatedServer || !GetPC())
	{
		return;
	}
	UWorld* W = GetWorld();
	UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr;
	if (!Vue)
	{
		return;
	}
	Hud = SNew(SPouvoirsHud);
	Vue->AddViewportWidgetContent(Hud.ToSharedRef(), 4);
}

void APouvoirsChelles::RetirerHud()
{
	if (!Hud.IsValid())
	{
		return;
	}
	UWorld* W = GetWorld();
	if (UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Hud.ToSharedRef());
	}
	Hud.Reset();
}

void APouvoirsChelles::MajHud()
{
	if (!Hud.IsValid())
	{
		return;
	}
	FEtatPouvoirsHud& E = Hud->Etat;
	APlayerController* PC = GetPC();
	E.bVisible = bAfficher && PC && PC->GetPawn() && !PC->IsPaused();
	E.XP = XPLocale;
	E.Niveau = NiveauPourXP(XPLocale);
	E.Selection = Selection;
	E.Banniere = Banniere;
	E.SousBanniere = SousBanniere;
	E.AgeBanniere = AgeBanniere;
	E.Info = Info;
	E.AgeInfo = AgeInfo;
	E.MoiVerre = 0.f;
	E.MoiGele = 0.f;
	APouvoirsRelais* Mien = MonRelais();
	for (int32 i = 0; i < NombrePouvoirs; ++i)
	{
		E.Recharge[i] = Mien ? Mien->RestantRecharge(i) : 0.f;
		E.Effet[i] = (Mien && i != Cameleon) ? Mien->RestantEffet(i) : 0.f;
	}
	if (Mien)
	{
		E.MoiVerre = Mien->RestantEffet(Verre);
	}
	APawn* MonPion = PC ? PC->GetPawn() : nullptr;
	if (MonPion)
	{
		for (TActorIterator<APouvoirsRelais> It(GetWorld()); It; ++It)
		{
			if (It->CibleGel == MonPion)
			{
				E.MoiGele = FMath::Max(E.MoiGele, It->RestantEffet(Gel));
			}
		}
	}
}

void APouvoirsChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Reel = float(FApp::GetDeltaTime());
	AgeBanniere += Reel;
	AgeInfo += Reel;

	if (!bLieClavier)
	{
		LierClavier();
	}
	if (!Hud.IsValid())
	{
		CreerHud();
	}

	const float Horloge = W->GetRealTimeSeconds();
	if (EstServeur() && Horloge >= ProchaineGestion)
	{
		ProchaineGestion = Horloge + 0.5f;
		GererRelais();
	}

	if (bCharge && GetPC())
	{
		if (EnJeu())
		{
			CompterTemps(Reel);
		}
		APouvoirsRelais* R = MonRelais();
		if (R && R->XP != XPLocale && Horloge >= ProchainEnvoi)
		{
			ProchainEnvoi = Horloge + 1.f;
			R->ServeurXP(XPLocale);
		}
	}

	if (!W->IsPaused())
	{
		AppliquerEffets(DeltaSeconds);
	}
	MajHud();
}
