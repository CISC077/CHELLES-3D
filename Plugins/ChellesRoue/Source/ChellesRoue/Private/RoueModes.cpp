// CHELLES 3D - Roue des modes : maintenir Tab pour choisir Soldat / Spider-Man / Motoneige / Automobile (UE 4.27)
#include "RoueModes.h"

#include "SRoueModes.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#define CHEMIN_MOTONEIGE TEXT("/Game/DV_Snowmobile/Blueprints/BP_Snowmobile.BP_Snowmobile_C")
// Spider-Man sur le squelette du pack SuperPowers (cree par Tools\\SpiderManJoueur\\creer_spiderman_pouvoirs.py)
#define CHEMIN_COSTUME_SPIDERMAN TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Pouvoirs.SK_SpiderMan_Pouvoirs")
// Spider-Man sur le squelette du cavalier HorseStarterKit (meme script)
// Spider-Man sur le squelette du pack DynamicCombatSystem (meme script)
#define CHEMIN_COSTUME_COMBAT TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Combat.SK_SpiderMan_Combat")
#define CHEMIN_COSTUME_CAVALIER TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Cavalier.SK_SpiderMan_Cavalier")

// ---------------------------------------------------------------------------
// Appels par reflexion vers ChellesBasculeVol et ChellesMotoneige (aucune dependance)
// ---------------------------------------------------------------------------

static bool AppelerBool(AActor* Acteur, const TCHAR* NomFonction)
{
	if (!IsValid(Acteur))
	{
		return false;
	}
	UFunction* Fonction = Acteur->FindFunction(FName(NomFonction));
	if (!Fonction)
	{
		return false;
	}
	const int32 Taille = FMath::Max<int32>(Fonction->ParmsSize, 1);
	uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
	FMemory::Memzero(Parametres, Taille);
	Acteur->ProcessEvent(Fonction, Parametres);
	if (FBoolProperty* Retour = FindFProperty<FBoolProperty>(Fonction, TEXT("ReturnValue")))
	{
		return Retour->GetPropertyValue_InContainer(Parametres);
	}
	return false;
}

static void Appeler(AActor* Acteur, const TCHAR* NomFonction)
{
	if (!IsValid(Acteur))
	{
		return;
	}
	if (UFunction* Fonction = Acteur->FindFunction(FName(NomFonction)))
	{
		const int32 Taille = FMath::Max<int32>(Fonction->ParmsSize, 1);
		uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
		FMemory::Memzero(Parametres, Taille);
		Acteur->ProcessEvent(Fonction, Parametres);
	}
}

static FKey LireTouche(AActor* Acteur)
{
	if (IsValid(Acteur))
	{
		if (FStructProperty* P = FindFProperty<FStructProperty>(Acteur->GetClass(), TEXT("Touche")))
		{
			if (P->Struct && P->Struct->GetFName() == FName(TEXT("Key")))
			{
				return *P->ContainerPtrToValuePtr<FKey>(Acteur);
			}
		}
	}
	return EKeys::Invalid;
}

static void RetirerTouche(UInputComponent* Entrees, const FKey& Touche)
{
	if (Entrees && Touche.IsValid())
	{
		Entrees->KeyBindings.RemoveAll([&Touche](const FInputKeyBinding& Liaison)
		{
			return Liaison.Chord.Key == Touche;
		});
	}
}

static UClass* TrouverClasse(const TCHAR* CheminScript, const TCHAR* Nom)
{
	UClass* Classe = FindObject<UClass>(nullptr, CheminScript);
	if (!Classe)
	{
		Classe = FindObject<UClass>(ANY_PACKAGE, Nom);
	}
	return Classe;
}

// ---------------------------------------------------------------------------

ARoueModes::ARoueModes()
{
	PrimaryActorTick.bCanEverTick = true;
	Touche = EKeys::Tab;
	InputPriority = 20; // passe avant le personnage et la motoneige
	SetActorHiddenInGame(true);

	USceneComponent* Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	FModeRoue Soldat;
	Soldat.Nom = FText::FromString(TEXT("Soldat"));
	Soldat.Type = ETypeModeRoue::APied;
	Soldat.Icone = EIconeRoue::Viseur;
	Soldat.Couleur = FLinearColor(0.55f, 0.78f, 0.25f);
	Modes.Add(Soldat);

	FModeRoue Vol;
	Vol.Nom = FText::FromString(TEXT("Spider-Man"));
	Vol.Type = ETypeModeRoue::Vol;
	Vol.Icone = EIconeRoue::Toile;
	Vol.Couleur = FLinearColor(0.95f, 0.18f, 0.22f);
	Modes.Add(Vol);

	FModeRoue Moto;
	Moto.Nom = FText::FromString(TEXT("Motoneige"));
	Moto.Type = ETypeModeRoue::Vehicule;
	Moto.Icone = EIconeRoue::Flocon;
	Moto.Couleur = FLinearColor(0.35f, 0.8f, 1.f);
	Moto.ClasseVehicule = FSoftClassPath(FString(CHEMIN_MOTONEIGE));
	Modes.Add(Moto);

	// Deux personnages du pack SuperPowers (a la place des voitures, 2026-11-08)
	FModeRoue Flash;
	Flash.Nom = FText::FromString(TEXT("FLASH ECLAIR"));
	Flash.Type = ETypeModeRoue::Vehicule; // personnage du pack, pris en main comme un vehicule
	Flash.Icone = EIconeRoue::Eclair;
	Flash.Couleur = FLinearColor(1.f, 0.85f, 0.1f);
	Flash.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/SuperPowers/Powers/Speedster/BP_Speedster.BP_Speedster_C")));
	Flash.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDERMAN));
	Modes.Add(Flash);

	FModeRoue Flamme;
	Flamme.Nom = FText::FromString(TEXT("FLAMME FIRE"));
	Flamme.Type = ETypeModeRoue::Vehicule;
	Flamme.Icone = EIconeRoue::Flamme;
	Flamme.Couleur = FLinearColor(1.f, 0.36f, 0.08f);
	Flamme.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/SuperPowers/Powers/Flame/BP_Flame.BP_Flame_C")));
	Flamme.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDERMAN));
	Modes.Add(Flamme);

	FModeRoue Pouvoirs;
	Pouvoirs.Nom = FText::FromString(TEXT("SuperPowers"));
	Pouvoirs.Type = ETypeModeRoue::Vehicule; // personnage du pack, pris en main comme un vehicule
	Pouvoirs.Icone = EIconeRoue::Etoile;
	Pouvoirs.Couleur = FLinearColor(0.75f, 0.35f, 1.f);
	Pouvoirs.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/SuperPowers/Powers/Super/BP_Super.BP_Super_C")));
	Pouvoirs.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDERMAN));
	Modes.Add(Pouvoirs);

	// Pack DynamicCombatSystem (+ MagicModule) : combat a mains nues, sorts, inventaire
	FModeRoue Combat;
	Combat.Nom = FText::FromString(TEXT("DYNAMIC COMBAT"));
	// Pris en main directement (sans le plugin Motoneige, qui donnait le soldat cache aux variables du perso :
	// les sorts se jouaient alors sur le soldat, journal 2026-11-08 "Montage ... for the wrong Skeleton").
	Combat.Type = ETypeModeRoue::Personnage;
	Combat.Icone = EIconeRoue::Epees;
	Combat.Couleur = FLinearColor(0.95f, 0.25f, 0.3f);
	Combat.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/DynamicCombatSystem/Blueprints/BP_CombatCharacter.BP_CombatCharacter_C")));
	Combat.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_COMBAT));
	Combat.bDescendreAvecE = false; // E = ramasser dans ce pack
	// Touches du pack (voir sa fenetre K) : Espace = roulade, Maj+Espace = saut ; R = mode combat (pas le ralenti)
	// Tout le pack sur peu de touches (demande MERIDJA) :
	//   clic gauche = attaque / sort, clic droit = mode combat (sortir / annuler le sort),
	//   molette (ou + / -) = sort suivant / precedent, F2 roulade, F3 saut, F4 potion (outil), F5 ramasser, F6 cible.
	// On coupe toutes les autres actions posees sur la souris (armes du soldat, pouvoirs SuperPowers...) :
	// sinon un autre acteur peut prendre le clic avant le perso du pack.
	Combat.ActionsCoupees = {
		TEXT("Jump"), TEXT("SlowMotion"), TEXT("Zoom"),
		TEXT("Fire"), TEXT("ADS"), TEXT("Aim"), TEXT("NextWeapon"), TEXT("PreviousWeapon"), TEXT("SwitchWeapon"),
		TEXT("Disperse"), TEXT("Freefall"), TEXT("FireProjectile"), TEXT("FireProjectile2Hands"),
		TEXT("ThrowLightning"), TEXT("Freeze"),
		TEXT("TargetLeft"), TEXT("TargetRight"), TEXT("SwitchMainHandItemUp"), TEXT("SwitchMainHandItemDown")
	};
	Combat.ActionsAjoutees = {
		FInputActionKeyMapping(TEXT("ToggleCombat"), EKeys::RightMouseButton),
		FInputActionKeyMapping(TEXT("Roll"), EKeys::F2),
		FInputActionKeyMapping(TEXT("Jump"), EKeys::F3),
		FInputActionKeyMapping(TEXT("Jump"), EKeys::SpaceBar, true),
		FInputActionKeyMapping(TEXT("UseToolItem"), EKeys::F4),
		FInputActionKeyMapping(TEXT("Interact"), EKeys::F5),
		FInputActionKeyMapping(TEXT("ToggleTargeting"), EKeys::F6)
	};
	Combat.bSortsPlusMoins = true;
	Modes.Add(Combat);

	// Dernier bouton : le cheval du pack HorseStarterKit (le cavalier fait partie du cheval)
	FModeRoue Cheval;
	Cheval.Nom = FText::FromString(TEXT("HORSE"));
	Cheval.Type = ETypeModeRoue::Vehicule;
	Cheval.Icone = EIconeRoue::FerACheval;
	Cheval.Couleur = FLinearColor(0.85f, 0.55f, 0.28f);
	Cheval.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/HorseStarterKit/Blueprint/BP_Rideable_DarkBrown_Horse.BP_Rideable_DarkBrown_Horse_C")));
	Cheval.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_CAVALIER));
	Cheval.ComposantCostume = TEXT("Horse_Rider");
	Cheval.bMonterAvecE = true; // le cheval arrive, on monte avec E
	Modes.Add(Cheval);

	// Pour ajouter une case libre plus tard : Type = ETypeModeRoue::Libre, Icone = EIconeRoue::Question.
}

APlayerController* ARoueModes::GetPC() const
{
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

void ARoueModes::BeginPlay()
{
	Super::BeginPlay();

	Controleurs.SetNum(Modes.Num());
	ClassesVehicules.SetNum(Modes.Num());
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if ((Modes[i].Type == ETypeModeRoue::Vehicule || Modes[i].Type == ETypeModeRoue::Personnage) && Modes[i].ClasseVehicule.IsValid())
		{
			ClassesVehicules[i] = Modes[i].ClasseVehicule.TryLoadClass<APawn>();
			if (!ClassesVehicules[i])
			{
				UE_LOG(LogTemp, Warning, TEXT("[Roue] Vehicule introuvable pour \"%s\" : %s"),
					*Modes[i].Nom.ToString(), *Modes[i].ClasseVehicule.ToString());
			}
		}
	}

	ChercherPlugins();
	LierClavier();
}

void ARoueModes::EndPlay(const EEndPlayReason::Type Raison)
{
	Fermer();
	RetablirTouches();
	Super::EndPlay(Raison);
}

void ARoueModes::LierClavier()
{
	if (bLieClavier)
	{
		return;
	}
	if (APlayerController* PC = GetPC())
	{
		EnableInput(PC);
		if (InputComponent && Touche.IsValid())
		{
			InputComponent->BindKey(Touche, IE_Pressed, this, &ARoueModes::Ouvrir);
			InputComponent->BindKey(Touche, IE_Released, this, &ARoueModes::Relacher);
			// E pres du cheval : monter (ne bloque pas la touche E pour le reste du jeu)
			FInputKeyBinding& Monter = InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ARoueModes::ToucheMonter);
			Monter.bConsumeInput = false;

			// Clavier AZERTY : la rangee & e " ' ( ne donne pas 1 2 3 4 5 a Unreal. On les traduit
			// pour les packs qui ecoutent les chiffres (sorts de DYNAMIC COMBAT, armes...).
			// + et - : sorts du pack (DYNAMIC COMBAT). Pave numerique et clavier principal.
			for (const FKey& K : { EKeys::Add, EKeys::Equals })
			{
				FInputKeyBinding& B = InputComponent->BindKey(K, IE_Pressed, this, &ARoueModes::SortPlus);
				B.bConsumeInput = false;
			}
			for (const FKey& K : { EKeys::Subtract, EKeys::Hyphen })
			{
				FInputKeyBinding& B = InputComponent->BindKey(K, IE_Pressed, this, &ARoueModes::SortMoins);
				B.bConsumeInput = false;
			}
			// Molette de la souris : choisir le sort / l'objet (seulement dans les modes bSortsPlusMoins)
			{
				FInputKeyBinding& Haut = InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ARoueModes::SortPlus);
				Haut.bConsumeInput = false;
				FInputKeyBinding& Bas = InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ARoueModes::SortMoins);
				Bas.bConsumeInput = false;
			}

			const FKey Azerty[] = { EKeys::Ampersand, EKeys::E_AccentAigu, EKeys::Quote, EKeys::Apostrophe, EKeys::LeftParantheses };
			for (const FKey& K : Azerty)
			{
				FInputKeyBinding Appui(FInputChord(K), IE_Pressed);
				Appui.bConsumeInput = false;
				Appui.KeyDelegate.GetDelegateWithKeyForManualSet().BindUObject(this, &ARoueModes::ChiffreAzerty, true);
				InputComponent->KeyBindings.Add(Appui);
				FInputKeyBinding Lache(FInputChord(K), IE_Released);
				Lache.bConsumeInput = false;
				Lache.KeyDelegate.GetDelegateWithKeyForManualSet().BindUObject(this, &ARoueModes::ChiffreAzerty, false);
				InputComponent->KeyBindings.Add(Lache);
			}
			bLieClavier = true;
			UE_LOG(LogTemp, Log, TEXT("[Roue] Maintenir %s pour choisir le mode."), *Touche.ToString());
		}
	}
}

// ---------------------------------------------------------------------------
// Liens avec les autres plugins
// ---------------------------------------------------------------------------

void ARoueModes::ChercherPlugins()
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	if (!ClasseMotoneige)
	{
		ClasseMotoneige = TrouverClasse(TEXT("/Script/ChellesMotoneige.Motoneige"), TEXT("Motoneige"));
	}
	if (!IsValid(Bascule))
	{
		Bascule = nullptr;
		if (UClass* ClasseBascule = TrouverClasse(TEXT("/Script/ChellesBasculeVol.BasculeVol"), TEXT("BasculeVol")))
		{
			for (TActorIterator<AActor> It(Monde, ClasseBascule); It; ++It)
			{
				Bascule = *It;
				break;
			}
		}
	}
	ActeursMotoneige.Reset();
	if (ClasseMotoneige)
	{
		for (TActorIterator<AActor> It(Monde, ClasseMotoneige); It; ++It)
		{
			ActeursMotoneige.Add(*It);
		}
	}
}

void ARoueModes::RetirerAnciennesTouches()
{
	if (!bRetirerTouchesFetM)
	{
		return;
	}
	if (IsValid(Bascule))
	{
		RetirerTouche(Bascule->InputComponent, LireTouche(Bascule));
	}
	for (const TWeakObjectPtr<AActor>& Ptr : ActeursMotoneige)
	{
		AActor* A = Ptr.Get();
		if (!A)
		{
			continue;
		}
		RetirerTouche(A->InputComponent, LireTouche(A));
		// Pendant la conduite, F (descendre puis voler) passe aussi par la roue. E reste pour descendre,
		// sauf pour les personnages de pack qui se servent de E (bDescendreAvecE = faux).
		if (FObjectProperty* P = FindFProperty<FObjectProperty>(A->GetClass(), TEXT("EntreesConduite")))
		{
			UInputComponent* Conduite = Cast<UInputComponent>(P->GetObjectPropertyValue_InContainer(A));
			RetirerTouche(Conduite, EKeys::F);
			const int32 Index = Controleurs.IndexOfByKey(A);
			if (Modes.IsValidIndex(Index) && !Modes[Index].bDescendreAvecE)
			{
				RetirerTouche(Conduite, EKeys::E);
			}
		}
	}
}

bool ARoueModes::EstEnVol() const
{
	return IsValid(Bascule) && AppelerBool(Bascule, TEXT("EstEnVol"));
}

AActor* ARoueModes::ControleurVehicule(int32 Index)
{
	if (!Modes.IsValidIndex(Index) || Modes[Index].Type != ETypeModeRoue::Vehicule || !ClasseMotoneige)
	{
		return nullptr;
	}
	if (Controleurs.Num() < Modes.Num())
	{
		Controleurs.SetNum(Modes.Num());
		ClassesVehicules.SetNum(Modes.Num());
	}
	if (IsValid(Controleurs[Index]))
	{
		return Controleurs[Index];
	}
	UClass* Vehicule = ClassesVehicules[Index];
	if (!Vehicule)
	{
		Vehicule = Modes[Index].ClasseVehicule.TryLoadClass<APawn>();
		ClassesVehicules[Index] = Vehicule;
	}
	FClassProperty* PropClasse = FindFProperty<FClassProperty>(ClasseMotoneige, TEXT("ClasseMoto"));
	if (!Vehicule || !PropClasse)
	{
		return nullptr;
	}

	// Un acteur Motoneige qui conduit deja ce vehicule (celui ajoute par le plugin Motoneige pour BP_Snowmobile).
	for (TActorIterator<AActor> It(GetWorld(), ClasseMotoneige); It; ++It)
	{
		AActor* A = *It;
		if (Controleurs.Contains(A))
		{
			continue;
		}
		UClass* SaClasse = Cast<UClass>(PropClasse->GetObjectPropertyValue_InContainer(A));
		if (SaClasse == Vehicule || (!SaClasse && Vehicule->GetPathName() == CHEMIN_MOTONEIGE))
		{
			Controleurs[Index] = A;
			return A;
		}
	}

	// Sinon on en cree un pour ce vehicule, sans touche (tout passe par la roue).
	const FTransform Lieu = FTransform::Identity;
	AActor* Nouveau = GetWorld()->SpawnActorDeferred<AActor>(ClasseMotoneige, Lieu, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Nouveau)
	{
		return nullptr;
	}
	PropClasse->SetObjectPropertyValue_InContainer(Nouveau, Vehicule);
	if (FStructProperty* PropTouche = FindFProperty<FStructProperty>(ClasseMotoneige, TEXT("Touche")))
	{
		if (PropTouche->Struct && PropTouche->Struct->GetFName() == FName(TEXT("Key")))
		{
			*PropTouche->ContainerPtrToValuePtr<FKey>(Nouveau) = EKeys::Invalid;
		}
	}
	Nouveau->FinishSpawning(Lieu);
	Controleurs[Index] = Nouveau;
	ActeursMotoneige.Add(Nouveau);
	UE_LOG(LogTemp, Log, TEXT("[Roue] Conduite prete pour \"%s\" (%s)."), *Modes[Index].Nom.ToString(), *Vehicule->GetName());
	return Nouveau;
}

bool ARoueModes::EstDisponible(int32 Index) const
{
	if (!Modes.IsValidIndex(Index))
	{
		return false;
	}
	switch (Modes[Index].Type)
	{
	case ETypeModeRoue::APied:
		return true;
	case ETypeModeRoue::Vol:
		return IsValid(Bascule);
	case ETypeModeRoue::Vehicule:
		return ClasseMotoneige != nullptr && ClassesVehicules.IsValidIndex(Index) && ClassesVehicules[Index] != nullptr;
	case ETypeModeRoue::Personnage:
		return ClassesVehicules.IsValidIndex(Index) && ClassesVehicules[Index] != nullptr;
	case ETypeModeRoue::Libre:
		return false;
	}
	return false;
}

int32 ARoueModes::ModeActuel() const
{
	const APlayerController* PC = GetPC();
	const APawn* Pion = PC ? PC->GetPawn() : nullptr;
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if (Modes[i].Type == ETypeModeRoue::Personnage)
		{
			if (Pion && Pion == PersoJoue.Get())
			{
				return i;
			}
			continue;
		}
		if (Modes[i].Type != ETypeModeRoue::Vehicule)
		{
			continue;
		}
		if (Controleurs.IsValidIndex(i) && IsValid(Controleurs[i]) && AppelerBool(Controleurs[i], TEXT("EstSurMoto")))
		{
			return i;
		}
		if (Pion && ClassesVehicules.IsValidIndex(i) && ClassesVehicules[i] && Pion->IsA(ClassesVehicules[i]))
		{
			return i;
		}
	}
	const ETypeModeRoue Cherche = EstEnVol() ? ETypeModeRoue::Vol : ETypeModeRoue::APied;
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if (Modes[i].Type == Cherche)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

// ---------------------------------------------------------------------------
// Changer de mode
// ---------------------------------------------------------------------------

void ARoueModes::MettreCostume(const FModeRoue& Mode)
{
	if (Mode.Costume.IsNull() || !Mode.ComposantCostume.IsNone())
	{
		return; // costume sur un composant (cavalier) : gere par SuivreMontures
	}
	APlayerController* PC = GetPC();
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	USkeletalMeshComponent* Corps = Perso ? Perso->GetMesh() : nullptr;
	if (!Corps)
	{
		return;
	}
	USkeletalMesh* Costume = Cast<USkeletalMesh>(Mode.Costume.TryLoad());
	if (!Costume)
	{
		Message(TEXT("Costume Spider-Man introuvable : lance Tools\\SpiderManJoueur\\creer_spiderman_pouvoirs.py dans Unreal."));
		return;
	}
	USkeletalMesh* Actuel = Corps->SkeletalMesh;
	if (Actuel == Costume)
	{
		return;
	}
	if (Actuel && Actuel->GetSkeleton() != Costume->GetSkeleton())
	{
		// Autre squelette : les animations du pack ne marcheraient plus, on garde le perso d'origine
		Message(TEXT("Costume Spider-Man : squelette different de celui du personnage, costume non applique."));
		return;
	}
	Corps->SetSkeletalMesh(Costume);
	Corps->EmptyOverrideMaterials();
}

APawn* ARoueModes::VehiculeDe(AActor* Controleur) const
{
	if (!IsValid(Controleur))
	{
		return nullptr;
	}
	FObjectProperty* P = FindFProperty<FObjectProperty>(Controleur->GetClass(), TEXT("Moto"));
	APawn* Pion = P ? Cast<APawn>(P->GetObjectPropertyValue_InContainer(Controleur)) : nullptr;
	return IsValid(Pion) ? Pion : nullptr;
}

void ARoueModes::HabillerComposant(AActor* Acteur, const FModeRoue& Mode, bool bVisible)
{
	if (!IsValid(Acteur) || Mode.ComposantCostume.IsNone())
	{
		return;
	}
	TArray<USkeletalMeshComponent*> Corps;
	Acteur->GetComponents(Corps);
	for (USkeletalMeshComponent* C : Corps)
	{
		if (!C || C->GetFName() != Mode.ComposantCostume)
		{
			continue;
		}
		if (bVisible && !Mode.Costume.IsNull())
		{
			USkeletalMesh* Costume = Cast<USkeletalMesh>(Mode.Costume.TryLoad());
			if (!Costume)
			{
				static bool bDejaDit = false;
				if (!bDejaDit)
				{
					bDejaDit = true;
					Message(TEXT("Costume du cavalier introuvable : relance Tools\\SpiderManJoueur\\creer_spiderman_pouvoirs.py dans Unreal."));
				}
			}
			else if (C->SkeletalMesh != Costume)
			{
				if (C->SkeletalMesh && C->SkeletalMesh->GetSkeleton() != Costume->GetSkeleton())
				{
					Message(TEXT("Costume du cavalier : squelette different, costume non applique."));
				}
				else
				{
					C->SetSkeletalMesh(Costume);
					C->EmptyOverrideMaterials();
				}
			}
		}
		if (C->bHiddenInGame == bVisible)
		{
			C->SetHiddenInGame(!bVisible);
		}
	}
}

void ARoueModes::AppelerVehicule(int32 Index)
{
	const FModeRoue& Mode = Modes[Index];
	AActor* Controleur = ControleurVehicule(Index);
	APlayerController* PC = GetPC();
	APawn* Pieton = PC ? PC->GetPawn() : nullptr;
	FClassProperty* PropClasse = Controleur ? FindFProperty<FClassProperty>(Controleur->GetClass(), TEXT("ClasseMoto")) : nullptr;
	FObjectProperty* PropMoto = Controleur ? FindFProperty<FObjectProperty>(Controleur->GetClass(), TEXT("Moto")) : nullptr;
	UClass* Classe = PropClasse ? Cast<UClass>(PropClasse->GetObjectPropertyValue_InContainer(Controleur)) : nullptr;
	if (!Controleur || !Pieton || !PropMoto || !Classe)
	{
		Message(FString::Printf(TEXT("%s : impossible de le faire venir."), *Mode.Nom.ToString()));
		return;
	}

	// 3,5 m devant le joueur, pose au sol, tourne dans le meme sens que lui
	const float Yaw = PC->GetControlRotation().Yaw;
	const FVector Devant = FRotator(0.f, Yaw, 0.f).Vector();
	FVector Lieu = Pieton->GetActorLocation() + Devant * 350.f;
	FHitResult Sol;
	FCollisionQueryParams Params(TEXT("RoueSol"), false, Pieton);
	if (GetWorld()->LineTraceSingleByChannel(Sol, Lieu + FVector(0.f, 0.f, 500.f), Lieu - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
	{
		Lieu = Sol.ImpactPoint;
	}
	Lieu.Z += 120.f;
	const FRotator Rot(0.f, Yaw + 90.f, 0.f); // de profil : on monte par le cote gauche

	APawn* Monture = VehiculeDe(Controleur);
	if (Monture)
	{
		Monture->SetActorLocationAndRotation(Lieu, Rot, false, nullptr, ETeleportType::ResetPhysics);
	}
	else
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Monture = GetWorld()->SpawnActor<APawn>(Classe, Lieu, Rot, P);
		if (!Monture)
		{
			Message(FString::Printf(TEXT("%s : impossible de le faire venir."), *Mode.Nom.ToString()));
			return;
		}
		PropMoto->SetObjectPropertyValue_InContainer(Controleur, Monture);
	}
	HabillerComposant(Monture, Mode, false); // pas de cavalier tant qu'on n'est pas monte
	Message(FString::Printf(TEXT("%s est la : approche-toi et appuie sur E pour monter."), *Mode.Nom.ToString()));
}

void ARoueModes::ToucheMonter()
{
	if (bOuverte || MontureActive != INDEX_NONE)
	{
		return; // deja en selle : E sert a descendre (plugin Motoneige)
	}
	APlayerController* PC = GetPC();
	APawn* Pieton = PC ? PC->GetPawn() : nullptr;
	if (!Pieton)
	{
		return;
	}
	const int32 Avant = ModeActuel();
	if (Modes.IsValidIndex(Avant) && (Modes[Avant].Type == ETypeModeRoue::Vehicule || Modes[Avant].Type == ETypeModeRoue::Personnage))
	{
		return; // dans un autre vehicule / perso de pack
	}
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if (!Modes[i].bMonterAvecE || !Controleurs.IsValidIndex(i))
		{
			continue;
		}
		AActor* Controleur = Controleurs[i];
		APawn* Monture = VehiculeDe(Controleur);
		if (!Monture || FVector::Dist(Monture->GetActorLocation(), Pieton->GetActorLocation()) > 450.f)
		{
			continue;
		}
		Appeler(Controleur, TEXT("Basculer"));
		if (AppelerBool(Controleur, TEXT("EstSurMoto")))
		{
			MontrerHudPack(RacinePack(Modes[i].ClasseVehicule));
			HabillerComposant(Monture, Modes[i], true);
			JouerMontee(Monture, Modes[i]);
			MontureActive = i;
		}
		return;
	}
}

static void MettreBoolAnim(USkeletalMeshComponent* C, const TCHAR* Nom, bool bValeur)
{
	UAnimInstance* Anim = C ? C->GetAnimInstance() : nullptr;
	if (FBoolProperty* P = Anim ? FindFProperty<FBoolProperty>(Anim->GetClass(), FName(Nom)) : nullptr)
	{
		P->SetPropertyValue_InContainer(Anim, bValeur);
	}
}

void ARoueModes::JouerMontee(APawn* Monture, const FModeRoue& Mode)
{
	// Animation de montee du pack (cheval + cavalier), si elle existe
	TArray<USkeletalMeshComponent*> Corps;
	Monture->GetComponents(Corps);
	for (USkeletalMeshComponent* C : Corps)
	{
		MettreBoolAnim(C, TEXT("PlayMountLeft"), true);
	}
	MontureAnimee = Monture;
	GetWorldTimerManager().SetTimer(MinuteurMontee, this, &ARoueModes::FinMontee, 3.f, false);
}

void ARoueModes::FinMontee()
{
	if (APawn* Monture = MontureAnimee.Get())
	{
		TArray<USkeletalMeshComponent*> Corps;
		Monture->GetComponents(Corps);
		for (USkeletalMeshComponent* C : Corps)
		{
			MettreBoolAnim(C, TEXT("PlayMountLeft"), false);
		}
	}
	MontureAnimee.Reset();
}

void ARoueModes::SuivreMontures()
{
	int32 Active = INDEX_NONE;
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if (!Modes[i].bMonterAvecE || !Controleurs.IsValidIndex(i) || !IsValid(Controleurs[i]))
		{
			continue;
		}
		APawn* Monture = VehiculeDe(Controleurs[i]);
		if (!Monture)
		{
			continue;
		}
		const bool bSur = AppelerBool(Controleurs[i], TEXT("EstSurMoto"));
		HabillerComposant(Monture, Modes[i], bSur);
		if (bSur)
		{
			Active = i;
		}
	}
	MontureActive = Active;
}

void ARoueModes::ChiffreAzerty(FKey Touche, bool bAppui)
{
	APlayerController* PC = GetPC();
	if (!PC || bOuverte)
	{
		return;
	}
	FKey Chiffre;
	if (Touche == EKeys::Ampersand) Chiffre = EKeys::One;
	else if (Touche == EKeys::E_AccentAigu) Chiffre = EKeys::Two;
	else if (Touche == EKeys::Quote) Chiffre = EKeys::Three;
	else if (Touche == EKeys::Apostrophe) Chiffre = EKeys::Four;
	else if (Touche == EKeys::LeftParantheses) Chiffre = EKeys::Five;
	else return;
	// Au tick suivant : on ne touche pas aux touches pendant que Unreal les traite
	TWeakObjectPtr<APlayerController> PCFaible = PC;
	GetWorldTimerManager().SetTimerForNextTick([PCFaible, Chiffre, bAppui]()
	{
		if (APlayerController* P = PCFaible.Get())
		{
			P->InputKey(Chiffre, bAppui ? IE_Pressed : IE_Released, bAppui ? 1.f : 0.f, false);
		}
	});
}

void ARoueModes::EmpecherChute()
{
	APlayerController* PC = GetPC();
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	UCharacterMovementComponent* Mouvement = Perso ? Perso->GetCharacterMovement() : nullptr;
	if (!Mouvement)
	{
		return; // vehicule a roues : pas concerne
	}
	if (PionSurveille.Get() != Perso)
	{
		PionSurveille = Perso;
		bSolConnu = false;
	}
	const FVector Ici = Perso->GetActorLocation();
	if (Mouvement->IsMovingOnGround())
	{
		DernierSol = Ici;
		bSolConnu = true;
		return;
	}
	// En vol (Spider-Man, pouvoirs) on peut monter haut : on ne surveille que la chute loin SOUS le dernier sol
	if (bSolConnu && Ici.Z < DernierSol.Z - 2500.f && Mouvement->Velocity.Z < 0.f)
	{
		Perso->SetActorLocation(DernierSol + FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::ResetPhysics);
		Mouvement->Velocity = FVector::ZeroVector;
		Mouvement->SetMovementMode(MOVE_Falling);
		Message(TEXT("Tu etais passe sous la carte : retour au dernier endroit sur."));
	}
}

void ARoueModes::RetablirTouches()
{
	APlayerController* PC = GetPC();
	UPlayerInput* Entrees = PC ? PC->PlayerInput : nullptr;
	if (Entrees)
	{
		for (const FInputActionKeyMapping& M : TouchesAjoutees)
		{
			Entrees->RemoveActionMapping(M);
		}
		for (const FInputActionKeyMapping& M : TouchesRetirees)
		{
			Entrees->AddActionMapping(M);
		}
		Entrees->ForceRebuildingKeyMaps(false);
	}
	TouchesAjoutees.Reset();
	TouchesRetirees.Reset();
	ModeTouches = -1;
}

void ARoueModes::AppliquerTouchesDuMode()
{
	APlayerController* PC = GetPC();
	UPlayerInput* Entrees = PC ? PC->PlayerInput : nullptr;
	if (!Entrees)
	{
		return;
	}
	const int32 Mode = ModeActuel();
	if (Mode == ModeTouches)
	{
		return;
	}
	RetablirTouches();
	if (!Modes.IsValidIndex(Mode) || (Modes[Mode].ActionsCoupees.Num() == 0 && Modes[Mode].ActionsAjoutees.Num() == 0))
	{
		ModeTouches = Mode;
		return;
	}
	for (const FInputActionKeyMapping& M : Entrees->ActionMappings)
	{
		if (Modes[Mode].ActionsCoupees.Contains(M.ActionName))
		{
			TouchesRetirees.Add(M);
		}
	}
	for (const FInputActionKeyMapping& M : TouchesRetirees)
	{
		Entrees->RemoveActionMapping(M);
	}
	for (const FInputActionKeyMapping& M : Modes[Mode].ActionsAjoutees)
	{
		Entrees->AddActionMapping(M);
		TouchesAjoutees.Add(M);
	}
	Entrees->ForceRebuildingKeyMaps(false);
	ModeTouches = Mode;
}

void ARoueModes::SortSuivant(int32 Sens)
{
	const int32 Mode = ModeActuel();
	APlayerController* PC = GetPC();
	if (!PC || bOuverte || !Modes.IsValidIndex(Mode) || !Modes[Mode].bSortsPlusMoins)
	{
		return;
	}
	SortCourant = (SortCourant - 1 + Sens + 5) % 5 + 1;
	static const FKey Chiffres[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	const FKey Chiffre = Chiffres[SortCourant - 1];
	TWeakObjectPtr<APlayerController> PCFaible = PC;
	GetWorldTimerManager().SetTimerForNextTick([PCFaible, Chiffre]()
	{
		if (APlayerController* P = PCFaible.Get())
		{
			P->InputKey(Chiffre, IE_Pressed, 1.f, false);
			P->InputKey(Chiffre, IE_Released, 0.f, false);
		}
	});
	Message(FString::Printf(TEXT("Sort %d"), SortCourant));
}

void ARoueModes::RendreSesReferences(APawn* Perso, APawn* Ancien)
{
	// Le plugin Motoneige donne le pieton aux variables "Character/Pawn/Player" du vehicule (utile pour une
	// vraie motoneige). Pour un personnage de pack, ces variables doivent designer le personnage lui-meme.
	if (!IsValid(Perso) || !IsValid(Ancien) || Perso == Ancien)
	{
		return;
	}
	auto Corriger = [Perso, Ancien](UObject* Objet)
	{
		if (!Objet)
		{
			return;
		}
		for (TFieldIterator<FObjectProperty> It(Objet->GetClass()); It; ++It)
		{
			FObjectProperty* Prop = *It;
			if (Prop->GetObjectPropertyValue_InContainer(Objet) == Ancien && Perso->IsA(Prop->PropertyClass))
			{
				Prop->SetObjectPropertyValue_InContainer(Objet, Perso);
				UE_LOG(LogTemp, Log, TEXT("[Roue] %s.%s remis sur %s"), *Objet->GetName(), *Prop->GetName(), *Perso->GetName());
			}
		}
	};
	Corriger(Perso);
	TInlineComponentArray<UActorComponent*> Composants;
	Perso->GetComponents(Composants);
	for (UActorComponent* C : Composants)
	{
		Corriger(C);
	}
}

void ARoueModes::ChoisirMode(int32 Index)
{
	if (!Modes.IsValidIndex(Index))
	{
		return;
	}
	ChercherPlugins();
	const FModeRoue& Mode = Modes[Index];
	if (!EstDisponible(Index))
	{
		if (Mode.Type == ETypeModeRoue::Libre)
		{
			Message(TEXT("Case libre : aucune fonction pour l'instant."));
		}
		else if (Mode.Type == ETypeModeRoue::Vol)
		{
			Message(TEXT("Spider-Man indisponible : l'acteur Bascule Vol n'est pas dans le niveau."));
		}
		else if (!ClasseMotoneige)
		{
			Message(TEXT("Vehicule indisponible : le plugin ChellesMotoneige n'est pas compile."));
		}
		else
		{
			Message(FString::Printf(TEXT("%s indisponible : %s introuvable."), *Mode.Nom.ToString(), *Mode.ClasseVehicule.ToString()));
		}
		return;
	}

	const int32 Avant = ModeActuel();
	if (Avant == Index)
	{
		return;
	}

	// 1. Descendre du vehicule actuel (sinon il serait pris pour le pieton)
	if (Modes.IsValidIndex(Avant) && Modes[Avant].Type == ETypeModeRoue::Vehicule)
	{
		if (AActor* Ancien = ControleurVehicule(Avant))
		{
			if (AppelerBool(Ancien, TEXT("EstSurMoto")))
			{
				Appeler(Ancien, TEXT("Basculer"));
			}
			if (AppelerBool(Ancien, TEXT("EstSurMoto")))
			{
				Message(TEXT("Impossible de descendre du vehicule."));
				return;
			}
		}
		CacherHudPack(RacinePack(Modes[Avant].ClasseVehicule));
	}
	else if (Modes.IsValidIndex(Avant) && Modes[Avant].Type == ETypeModeRoue::Personnage)
	{
		SortirPersonnage();
		CacherHudPack(RacinePack(Modes[Avant].ClasseVehicule));
	}

	// 2. Nouveau mode
	switch (Mode.Type)
	{
	case ETypeModeRoue::APied:
		if (EstEnVol())
		{
			Appeler(Bascule, TEXT("Basculer"));
		}
		break;

	case ETypeModeRoue::Vol:
		if (!EstEnVol())
		{
			Appeler(Bascule, TEXT("Basculer"));
		}
		break;

	case ETypeModeRoue::Vehicule:
	{
		if (Mode.bMonterAvecE)
		{
			AppelerVehicule(Index);
			break;
		}
		AActor* Controleur = ControleurVehicule(Index);
		if (!Controleur)
		{
			Message(FString::Printf(TEXT("%s : impossible de preparer la conduite."), *Mode.Nom.ToString()));
			break;
		}
		APlayerController* PCAvant = GetPC();
		APawn* PionAvant = PCAvant ? PCAvant->GetPawn() : nullptr;
		Appeler(Controleur, TEXT("Basculer")); // le pieton (ou Spider-Man) monte, le vehicule arrive a cote
		if (AppelerBool(Controleur, TEXT("EstSurMoto")))
		{
			MontrerHudPack(RacinePack(Mode.ClasseVehicule));
			MettreCostume(Mode);
			if (!Mode.Costume.IsNull() && Mode.ComposantCostume.IsNone() && PCAvant)
			{
				RendreSesReferences(PCAvant->GetPawn(), PionAvant); // personnages de pack (pouvoirs, combat)
			}
			SortCourant = 1;
		}
		else
		{
			Message(FString::Printf(TEXT("%s : le vehicule n'a pas pu venir (voir le journal)."), *Mode.Nom.ToString()));
		}
		break;
	}
	case ETypeModeRoue::Personnage:
		if (EntrerPersonnage(Index))
		{
			MontrerHudPack(RacinePack(Mode.ClasseVehicule));
			MettreCostume(Mode);
			SortCourant = 1;
			if (APlayerController* PC = GetPC())
			{
				ACharacter* Perso = Cast<ACharacter>(PC->GetPawn());
				USkeletalMesh* Corps = (Perso && Perso->GetMesh()) ? Perso->GetMesh()->SkeletalMesh : nullptr;
				UE_LOG(LogTemp, Log, TEXT("[Roue] \"%s\" : on joue %s (corps %s, squelette %s)."), *Mode.Nom.ToString(),
					*GetNameSafe(Perso), *GetNameSafe(Corps), Corps ? *GetNameSafe(Corps->GetSkeleton()) : TEXT("aucun"));
			}
		}
		break;

	default:
		break;
	}

	RetirerAnciennesTouches();
}

// ---------------------------------------------------------------------------
// Personnage de pack pris en main directement (DYNAMIC COMBAT)
// ---------------------------------------------------------------------------

static void CacherActeur(AActor* A, bool bCacher)
{
	if (!A)
	{
		return;
	}
	A->SetActorHiddenInGame(bCacher);
	A->SetActorEnableCollision(!bCacher);
	TArray<AActor*> Attaches;
	A->GetAttachedActors(Attaches, true);
	for (AActor* B : Attaches)
	{
		if (B)
		{
			B->SetActorHiddenInGame(bCacher);
		}
	}
}

bool ARoueModes::EntrerPersonnage(int32 Index)
{
	const FModeRoue& Mode = Modes[Index];
	if (EstEnVol())
	{
		Appeler(Bascule, TEXT("Basculer")); // on se pose d'abord
	}
	APlayerController* PC = GetPC();
	APawn* Pieton = PC ? PC->GetPawn() : nullptr;
	UClass* Classe = ClassesVehicules.IsValidIndex(Index) ? ClassesVehicules[Index] : nullptr;
	if (!Pieton || !Classe)
	{
		Message(FString::Printf(TEXT("%s : impossible de prendre le personnage."), *Mode.Nom.ToString()));
		return false;
	}

	const FVector Lieu = Pieton->GetActorLocation();
	const FRotator Rot(0.f, PC->GetControlRotation().Yaw, 0.f);
	CacherActeur(Pieton, true); // avant d'apparaitre : pas de collision avec le soldat

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* Perso = GetWorld()->SpawnActor<APawn>(Classe, Lieu, Rot, P);
	if (!Perso)
	{
		CacherActeur(Pieton, false);
		Message(FString::Printf(TEXT("%s : le personnage n'a pas pu apparaitre."), *Mode.Nom.ToString()));
		return false;
	}

	if (ACharacter* Soldat = Cast<ACharacter>(Pieton))
	{
		Soldat->GetCharacterMovement()->StopMovementImmediately();
		Soldat->GetCharacterMovement()->DisableMovement();
	}
	PC->Possess(Perso);
	PC->SetControlRotation(Rot);
	PersoJoue = Perso;
	PietonCache = Pieton;
	return true;
}

void ARoueModes::SortirPersonnage()
{
	APawn* Perso = PersoJoue.Get();
	APawn* Pieton = PietonCache.Get();
	APlayerController* PC = GetPC();
	if (Pieton)
	{
		if (Perso)
		{
			Pieton->SetActorLocation(Perso->GetActorLocation() + FVector(0.f, 0.f, 20.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		CacherActeur(Pieton, false);
		if (ACharacter* Soldat = Cast<ACharacter>(Pieton))
		{
			Soldat->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
		if (PC)
		{
			PC->Possess(Pieton);
		}
	}
	if (Perso)
	{
		TArray<AActor*> Attaches;
		Perso->GetAttachedActors(Attaches, true);
		for (AActor* A : Attaches)
		{
			if (A)
			{
				A->Destroy();
			}
		}
		Perso->Destroy();
	}
	PersoJoue.Reset();
	PietonCache.Reset();
}

// ---------------------------------------------------------------------------
// HUD des packs de vehicules (ex. compteur) : cache quand on descend
// ---------------------------------------------------------------------------

FString ARoueModes::RacinePack(const FSoftClassPath& Chemin)
{
	TArray<FString> Morceaux;
	Chemin.GetLongPackageName().ParseIntoArray(Morceaux, TEXT("/"), true);
	if (Morceaux.Num() >= 2)
	{
		return FString::Printf(TEXT("/%s/%s/"), *Morceaux[0], *Morceaux[1]);
	}
	return FString();
}

void ARoueModes::CacherHudPack(const FString& Racine)
{
	if (Racine.IsEmpty() || Racine == TEXT("/Game/"))
	{
		return;
	}
	TArray<TWeakObjectPtr<UUserWidget>>& Liste = HudCaches.FindOrAdd(Racine);
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (!IsValid(W) || W->GetWorld() != GetWorld() || !W->IsInViewport())
		{
			continue;
		}
		if (W->GetClass()->GetPathName().StartsWith(Racine) && W->GetVisibility() != ESlateVisibility::Collapsed)
		{
			W->SetVisibility(ESlateVisibility::Collapsed);
			Liste.Add(W);
		}
	}
}

void ARoueModes::MontrerHudPack(const FString& Racine)
{
	if (TArray<TWeakObjectPtr<UUserWidget>>* Liste = HudCaches.Find(Racine))
	{
		for (const TWeakObjectPtr<UUserWidget>& W : *Liste)
		{
			if (W.IsValid())
			{
				W->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
		HudCaches.Remove(Racine);
	}
}

void ARoueModes::Message(const FString& Texte) const
{
	UE_LOG(LogTemp, Warning, TEXT("[Roue] %s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Orange, Texte);
	}
}

// ---------------------------------------------------------------------------
// Ouvrir / fermer la roue
// ---------------------------------------------------------------------------

void ARoueModes::Ouvrir()
{
	APlayerController* PC = GetPC();
	UWorld* Monde = GetWorld();
	if (bOuverte || !PC || !Monde || PC->IsPaused() || Modes.Num() == 0)
	{
		return;
	}
	UGameViewportClient* Vue = Monde->GetGameViewport();
	if (!Vue)
	{
		return;
	}
	ChercherPlugins();

	Widget = SNew(SRoueModes);
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		FCaseRoue Case;
		Case.Nom = Modes[i].Nom;
		Case.Icone = Modes[i].Icone;
		Case.Couleur = Modes[i].Couleur;
		Case.bDisponible = EstDisponible(i);
		Case.bLibre = (Modes[i].Type == ETypeModeRoue::Libre);
		Widget->Cases.Add(Case);
	}
	Widget->Actuel = ModeActuel();
	Vue->AddViewportWidgetContent(Widget.ToSharedRef(), 100);

	if (!EntreesRoue)
	{
		EntreesRoue = NewObject<UInputComponent>(this, TEXT("EntreesRoue"));
		EntreesRoue->Priority = InputPriority + 1;
		EntreesRoue->RegisterComponent();
		EntreesRoue->BindAxisKey(EKeys::MouseX, this, &ARoueModes::SourisX);
		EntreesRoue->BindAxisKey(EKeys::MouseY, this, &ARoueModes::SourisY);
		EntreesRoue->BindAxisKey(EKeys::Gamepad_RightX, this, &ARoueModes::ManetteX);
		EntreesRoue->BindAxisKey(EKeys::Gamepad_RightY, this, &ARoueModes::ManetteY);
		EntreesRoue->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARoueModes::Valider);
		EntreesRoue->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARoueModes::Annuler);
	}
	PC->PushInputComponent(EntreesRoue);
	PC->SetIgnoreLookInput(true);
	bIgnoreVue = true;

	AncienRalenti = UGameplayStatics::GetGlobalTimeDilation(Monde);
	if (Ralenti < 0.999f)
	{
		UGameplayStatics::SetGlobalTimeDilation(Monde, FMath::Clamp(Ralenti, 0.05f, 1.f));
	}

	Curseur = FVector2D::ZeroVector;
	Manette = FVector2D::ZeroVector;
	Survol = INDEX_NONE;
	Apparition = 0.f;
	bOuverte = true;
	EnvoyerALaRoue();
}

void ARoueModes::Relacher()
{
	if (bOuverte)
	{
		Valider();
	}
}

void ARoueModes::Valider()
{
	if (!bOuverte)
	{
		return;
	}
	const int32 Choix = Survol;
	Fermer();
	if (Choix != INDEX_NONE)
	{
		ChoisirMode(Choix);
	}
}

void ARoueModes::Annuler()
{
	Fermer();
}

void ARoueModes::Fermer()
{
	if (!bOuverte)
	{
		return;
	}
	bOuverte = false;

	APlayerController* PC = GetPC();
	if (PC && EntreesRoue)
	{
		PC->PopInputComponent(EntreesRoue);
	}
	if (PC && bIgnoreVue)
	{
		PC->SetIgnoreLookInput(false);
	}
	bIgnoreVue = false;

	if (UWorld* Monde = GetWorld())
	{
		if (Ralenti < 0.999f)
		{
			UGameplayStatics::SetGlobalTimeDilation(Monde, AncienRalenti > 0.f ? AncienRalenti : 1.f);
		}
		if (Widget.IsValid())
		{
			if (UGameViewportClient* Vue = Monde->GetGameViewport())
			{
				Vue->RemoveViewportWidgetContent(Widget.ToSharedRef());
			}
		}
	}
	Widget.Reset();
}

void ARoueModes::SourisX(float Valeur)
{
	if (bOuverte)
	{
		Curseur.X += Valeur * Sensibilite * 0.01f;
	}
}

void ARoueModes::SourisY(float Valeur)
{
	if (bOuverte)
	{
		Curseur.Y -= Valeur * Sensibilite * 0.01f; // l'ecran descend vers le bas
	}
}

void ARoueModes::ManetteX(float Valeur)
{
	Manette.X = Valeur;
}

void ARoueModes::ManetteY(float Valeur)
{
	Manette.Y = -Valeur;
}

void ARoueModes::MettreAJourSurvol()
{
	if (Manette.SizeSquared() > 0.25f * 0.25f)
	{
		Curseur = Manette.GetSafeNormal();
	}
	if (Curseur.SizeSquared() > 1.f)
	{
		Curseur = Curseur.GetSafeNormal();
	}
	const int32 N = Modes.Num();
	if (N == 0 || Curseur.SizeSquared() < 0.3f * 0.3f)
	{
		return; // pas assez bouge : on garde le choix precedent
	}
	const float Part = 2.f * PI / N;
	float Angle = FMath::Atan2(Curseur.Y, Curseur.X) + PI / 2.f; // 0 = en haut, sens horaire
	if (Angle < 0.f)
	{
		Angle += 2.f * PI;
	}
	Survol = FMath::RoundToInt(Angle / Part) % N;
}

void ARoueModes::EnvoyerALaRoue()
{
	if (Widget.IsValid())
	{
		Widget->Survol = Survol;
		Widget->Curseur = Curseur;
		Widget->Apparition = Apparition;
	}
}

void ARoueModes::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bLieClavier)
	{
		LierClavier();
	}

	// Toutes les demi-secondes (temps reel) : on retrouve les acteurs des autres plugins
	const float Maintenant = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	if (Maintenant >= ProchaineRecherche)
	{
		ProchaineRecherche = Maintenant + 0.5f;
		ChercherPlugins();
	}
	RetirerAnciennesTouches();
	SuivreMontures();
	if (PietonCache.IsValid() && !PersoJoue.IsValid())
	{
		SortirPersonnage(); // le perso du pack a disparu : on rend le soldat
	}
	EmpecherChute();
	if (!bOuverte)
	{
		AppliquerTouchesDuMode();
	}

	if (bOuverte)
	{
		APlayerController* PC = GetPC();
		if (!PC || PC->IsPaused())
		{
			Annuler();
			return;
		}
		Apparition = FMath::Min(1.f, Apparition + float(FApp::GetDeltaTime()) / 0.12f);
		MettreAJourSurvol();
		EnvoyerALaRoue();
	}
}
