// CHELLES 3D - Roue des modes : maintenir Tab pour choisir Soldat / Spider-Man / Motoneige / Automobile (UE 4.27)
#include "RoueModes.h"
#include "RoueRelais.h"

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
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

#define CHEMIN_MOTONEIGE TEXT("/Game/DV_Snowmobile/Blueprints/BP_Snowmobile.BP_Snowmobile_C")
// Spider-Man volant (pack SuperheroFlight) : en solo c'est ChellesBasculeVol qui le gere, en multijoueur la roue
#define CHEMIN_SPIDERMAN_VOL TEXT("/Game/SuperheroFlight/Blueprints/Characters/BP_SpiderMan_Vol.BP_SpiderMan_Vol_C")
// Spider-Man sur le squelette du pack SuperPowers (cree par Tools\\SpiderManJoueur\\creer_spiderman_pouvoirs.py)
#define CHEMIN_COSTUME_SPIDERMAN TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Pouvoirs.SK_SpiderMan_Pouvoirs")
// Spider-Man sur le squelette du cavalier HorseStarterKit (meme script)
// Spider-Man sur le squelette du pack DynamicCombatSystem (meme script)
#define CHEMIN_COSTUME_COMBAT TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Combat.SK_SpiderMan_Combat")
#define CHEMIN_COSTUME_CAVALIER TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Cavalier.SK_SpiderMan_Cavalier")
// Corps du volant (squelette SuperheroFlight) : Spider-Male d'origine, et Spider-Femelle
// (Spider-Femelle cree par Tools\SpiderFemelle\creer_spider_femelle.py, 2026-11-11)
#define CHEMIN_COSTUME_SPIDER_MALE_VOL TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Vol.SK_SpiderMan_Vol")
// Spider-Male sur le squelette du soldat FPS_Game (Tools\SpiderManJoueur\importer_spiderman.py) : soldat habille en Spider-Male
#define CHEMIN_COSTUME_SPIDER_MALE_SOLDAT TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_UE4.SK_SpiderMan_UE4")
#define CHEMIN_COSTUME_SPIDER_FEMELLE_VOL TEXT("/Game/Personnages/SK_SpiderFemelle/SK_SpiderFemelle_Vol.SK_SpiderFemelle_Vol")
// PANTERA (2026-11-11) : tenue noire, masque noir, yeux blancs. Cree par Tools\Pantera\creer_pantera.py
#define CHEMIN_COSTUME_PANTERA_VOL TEXT("/Game/Personnages/SK_Pantera/SK_Pantera_Vol.SK_Pantera_Vol")
#define CHEMIN_YEUX_PANTERA TEXT("/Game/Personnages/SK_Pantera/M_Pantera_Yeux.M_Pantera_Yeux")

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
	Vol.Nom = FText::FromString(TEXT("Spider-Male")); // nom affiche (renomme le 2026-11-11, demande MERIDJA)
	Vol.Type = ETypeModeRoue::Vol;
	Vol.Icone = EIconeRoue::Toile;
	Vol.Couleur = FLinearColor(0.95f, 0.18f, 0.22f);
	Vol.ClasseVehicule = FSoftClassPath(FString(CHEMIN_SPIDERMAN_VOL)); // utilise en multijoueur seulement
	Vol.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDER_MALE_VOL)); // remet son corps apres Spider-Femelle
	// 2026-11-11 (demande MERIDJA) : Spider-Male garde le vol ET a les fonctions du soldat.
	// Au sol = le soldat FPS_Game (armes, tir, visee, recharge, vie) en costume Spider-Male ; Espace en l'air = s'envoler.
	Vol.bFonctionsSoldat = true;
	Vol.CostumeSoldat = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDER_MALE_SOLDAT));
	Modes.Add(Vol);

	// Spider-Femelle (2026-11-11) : le meme volant, avec un corps feminin rouge et noir
	FModeRoue VolFemelle;
	VolFemelle.Nom = FText::FromString(TEXT("Spider-Femelle"));
	VolFemelle.Type = ETypeModeRoue::Vol;
	VolFemelle.Icone = EIconeRoue::Toile;
	VolFemelle.Couleur = FLinearColor(1.f, 0.3f, 0.65f);
	VolFemelle.ClasseVehicule = FSoftClassPath(FString(CHEMIN_SPIDERMAN_VOL)); // utilise en multijoueur seulement
	VolFemelle.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDER_FEMELLE_VOL));
	Modes.Add(VolFemelle);

	// PANTERA (2026-11-11, demande MERIDJA) : heros original tout en noir, masque noir et yeux blancs.
	// Meme volant que Spider-Male, mais 20 secondes de vol maximum avant d'atterrir.
	// Armes : clic gauche = tir special depuis les mains (plugin ChellesArmes), O = changer de tir.
	FModeRoue Pantera;
	Pantera.Nom = FText::FromString(TEXT("PANTERA"));
	Pantera.Type = ETypeModeRoue::Vol;
	Pantera.Icone = EIconeRoue::Griffes;
	Pantera.Couleur = FLinearColor(0.72f, 0.55f, 1.f);
	Pantera.ClasseVehicule = FSoftClassPath(FString(CHEMIN_SPIDERMAN_VOL)); // utilise en multijoueur seulement
	Pantera.Costume = FSoftObjectPath(FString(CHEMIN_COSTUME_PANTERA_VOL));
	Pantera.DureeVolMax = 20.f;
	Pantera.bYeuxMasque = true;
	Modes.Add(Pantera);

	FModeRoue Moto;
	Moto.Nom = FText::FromString(TEXT("Motoneige"));
	Moto.Type = ETypeModeRoue::Vehicule;
	Moto.Icone = EIconeRoue::Flocon;
	Moto.Couleur = FLinearColor(0.35f, 0.8f, 1.f);
	Moto.ClasseVehicule = FSoftClassPath(FString(CHEMIN_MOTONEIGE));
	Modes.Add(Moto);

	// SUV du pack DriveableCar_basic (remis sur la roue le 2026-11-09, seule voiture)
	FModeRoue Suv;
	Suv.Nom = FText::FromString(TEXT("SUV"));
	Suv.Type = ETypeModeRoue::Vehicule;
	Suv.Icone = EIconeRoue::Voiture;
	Suv.Couleur = FLinearColor(0.2f, 0.85f, 0.6f);
	Suv.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/DriveableCar_basic/Blueprints/Vehicles/SUV_BP_child.SUV_BP_child_C")));
	Suv.LimiteCirculation = 10; // peu de voitures, une par rue : on roule sans accident (demande 2026-11-10)
	Modes.Add(Suv);

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
		if (Modes[i].Type != ETypeModeRoue::Libre && Modes[i].ClasseVehicule.IsValid())
		{
			ClassesVehicules[i] = Modes[i].ClasseVehicule.TryLoadClass<APawn>();
			if (!ClassesVehicules[i])
			{
				UE_LOG(LogTemp, Warning, TEXT("[Roue] Vehicule introuvable pour \"%s\" : %s"),
					*Modes[i].Nom.ToString(), *Modes[i].ClasseVehicule.ToString());
			}
		}
	}

	PreparerCasesSoldatVolant();
	ChercherPlugins();
	LierClavier();
}

void ARoueModes::EndPlay(const EEndPlayReason::Type Raison)
{
	Fermer();
	RetablirTouches();
	FinSoldatVolant();
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

			// Spider-Male soldat : Espace en l'air = s'envoler (le saut du soldat n'est pas bloque)
			for (const FKey& K : { EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom })
			{
				FInputKeyBinding& Saut = InputComponent->BindKey(K, IE_Pressed, this, &ARoueModes::ToucheSaut);
				Saut.bConsumeInput = false;
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
	if (EstMultijoueur())
	{
		// En multijoueur, le serveur fait apparaitre directement le personnage de chaque case
		if (Modes[Index].Type == ETypeModeRoue::APied)
		{
			return true;
		}
		return Modes[Index].Type != ETypeModeRoue::Libre && ClassesVehicules.IsValidIndex(Index) && ClassesVehicules[Index] != nullptr;
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
	if (EstMultijoueur())
	{
		const ARoueRelais* Relais = MonRelais();
		if (Relais && Modes.IsValidIndex(Relais->Mode))
		{
			return Relais->Mode;
		}
		return IndexDuType(ETypeModeRoue::APied);
	}
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
	// Spider-Male soldat : a pied (armes) comme en vol, c'est toujours sa case
	if (Modes.IsValidIndex(ModeSoldatVol))
	{
		return ModeSoldatVol;
	}
	const ETypeModeRoue Cherche = EstEnVol() ? ETypeModeRoue::Vol : ETypeModeRoue::APied;
	if (Cherche == ETypeModeRoue::Vol)
	{
		// Plusieurs cases de vol (Spider-Male, Spider-Femelle) : on reconnait la case a son corps
		const ACharacter* Perso = Cast<ACharacter>(Pion);
		const USkeletalMesh* Corps = (Perso && Perso->GetMesh()) ? Perso->GetMesh()->SkeletalMesh : nullptr;
		if (Corps)
		{
			const FSoftObjectPath CheminCorps(Corps);
			for (int32 i = 0; i < Modes.Num(); ++i)
			{
				if (Modes[i].Type == ETypeModeRoue::Vol && !Modes[i].Costume.IsNull() && Modes[i].Costume == CheminCorps)
				{
					return i;
				}
			}
		}
	}
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
	HabillerPion(PC ? PC->GetPawn() : nullptr, Mode, true);
}

void ARoueModes::HabillerPion(APawn* Pion, const FModeRoue& Mode, bool bAvertir)
{
	if (!IsValid(Pion) || Mode.Costume.IsNull())
	{
		return;
	}
	if (!Mode.ComposantCostume.IsNone())
	{
		HabillerComposant(Pion, Mode, true); // cavalier du cheval
		return;
	}
	ACharacter* Perso = Cast<ACharacter>(Pion);
	USkeletalMeshComponent* Corps = Perso ? Perso->GetMesh() : nullptr;
	if (!Corps)
	{
		return;
	}
	USkeletalMesh* Costume = Cast<USkeletalMesh>(Mode.Costume.TryLoad());
	if (!Costume)
	{
		if (bAvertir)
		{
			if (Mode.Costume.ToString().Contains(TEXT("Pantera")))
			{
				Message(TEXT("Costume PANTERA introuvable : lance Tools\\Pantera\\creer_pantera.py dans Unreal (File > Execute Python Script)."));
			}
			else if (Mode.Costume.ToString().Contains(TEXT("SpiderFemelle")))
			{
				Message(TEXT("Costume Spider-Femelle introuvable : lance Tools\\SpiderFemelle\\creer_spider_femelle.py dans Unreal."));
			}
			else
			{
				Message(TEXT("Costume Spider-Male introuvable : lance Tools\\SpiderManJoueur\\creer_spiderman_pouvoirs.py dans Unreal."));
			}
		}
		return;
	}
	USkeletalMesh* Actuel = Corps->SkeletalMesh;
	if (Actuel == Costume)
	{
		MettreYeuxMasque(Corps, Mode.bYeuxMasque);
		return;
	}
	if (Actuel && Actuel->GetSkeleton() != Costume->GetSkeleton())
	{
		// Autre squelette : les animations du pack ne marcheraient plus, on garde le perso d'origine
		if (bAvertir)
		{
			Message(FString::Printf(TEXT("Costume %s : squelette different de celui du personnage, costume non applique."), *Mode.Nom.ToString()));
		}
		return;
	}
	Corps->SetSkeletalMesh(Costume);
	Corps->EmptyOverrideMaterials();
	MettreYeuxMasque(Corps, Mode.bYeuxMasque); // PANTERA : yeux blancs ; autres costumes : yeux retires
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
	if (bOuverte || MontureActive != INDEX_NONE || EstMultijoueur())
	{
		return; // deja en selle : E sert a descendre (plugin Motoneige). En multijoueur on monte directement.
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

void ARoueModes::ChiffreAzerty(FKey ToucheAzerty, bool bAppui)
{
	APlayerController* PC = GetPC();
	if (!PC || bOuverte)
	{
		return;
	}
	FKey Chiffre;
	if (ToucheAzerty == EKeys::Ampersand) Chiffre = EKeys::One;
	else if (ToucheAzerty == EKeys::E_AccentAigu) Chiffre = EKeys::Two;
	else if (ToucheAzerty == EKeys::Quote) Chiffre = EKeys::Three;
	else if (ToucheAzerty == EKeys::Apostrophe) Chiffre = EKeys::Four;
	else if (ToucheAzerty == EKeys::LeftParantheses) Chiffre = EKeys::Five;
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
	if (GetNetMode() == NM_Client)
	{
		return; // le serveur decide de la position
	}
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
	if (EstMultijoueur())
	{
		if (ARoueRelais* Relais = MonRelais())
		{
			Relais->ServeurChoisir(Index); // le serveur fait le changement pour ce joueur
		}
		else
		{
			Message(TEXT("Roue : connexion au serveur en cours, reessaie dans une seconde."));
		}
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
			Message(FString::Printf(TEXT("%s indisponible : l'acteur Bascule Vol n'est pas dans le niveau."), *Mode.Nom.ToString()));
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

	// Quitter Spider-Male soldat : le soldat reprend son corps (et son HABILLAGE)
	const bool bSoldatVolant = Mode.Type == ETypeModeRoue::Vol && Mode.bFonctionsSoldat;
	if (!bSoldatVolant && ModeSoldatVol != INDEX_NONE)
	{
		FinSoldatVolant();
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
		if (bSoldatVolant)
		{
			// Spider-Male : soldat au sol (armes du soldat), volant en l'air
			ModeSoldatVol = Index;
			bDemarrerVol = false;
			TempsPose = 0.f;
			if (EstEnVol())
			{
				MettreCostume(Mode); // deja en l'air : il garde le vol, il reprendra les armes en se posant
			}
			else if (APlayerController* PC = GetPC())
			{
				HabillerSoldat(PC->GetPawn(), true);
			}
			Message(FString::Printf(TEXT("%s : armes du soldat au sol. Saute puis rappuie sur Espace en l'air pour t'envoler ; pose-toi pour reprendre tes armes."), *Mode.Nom.ToString()));
			break;
		}
		if (!EstEnVol())
		{
			Appeler(Bascule, TEXT("Basculer"));
		}
		if (EstEnVol())
		{
			MettreCostume(Mode); // Spider-Male ou Spider-Femelle : meme volant, corps different
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
	if (Ralenti < 0.999f && !EstMultijoueur()) // en multijoueur on ne ralentit pas le temps des autres
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
		if (Ralenti < 0.999f && !EstMultijoueur())
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
		if (EstMultijoueur())
		{
			if (HasAuthority())
			{
				GererRelaisServeur();
			}
			SuivreJoueursReseau();
		}
		ReglerCirculation();
	}
	RetirerAnciennesTouches();
	SuivreMontures();
	if (EstMultijoueur())
	{
		// HUD des packs sur cette machine : on suit le mode que le serveur nous a donne
		const int32 Vu = ModeActuel();
		if (Vu != ModeVuReseau)
		{
			if (Modes.IsValidIndex(ModeVuReseau))
			{
				CacherHudPack(RacinePack(Modes[ModeVuReseau].ClasseVehicule));
			}
			if (Modes.IsValidIndex(Vu))
			{
				MontrerHudPack(RacinePack(Modes[Vu].ClasseVehicule));
			}
			ModeVuReseau = Vu;
			SortCourant = 1;
		}
	}
	if (PietonCache.IsValid() && !PersoJoue.IsValid())
	{
		SortirPersonnage(); // le perso du pack a disparu : on rend le soldat
	}
	EmpecherChute();
	LimiterVol(DeltaSeconds);
	SuivreSoldatVolant(DeltaSeconds);
	DonnerGuideCommandes();
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

// ---------------------------------------------------------------------------
// Multijoueur : la roue de chaque joueur passe par son relais, le serveur change le personnage
// ---------------------------------------------------------------------------

bool ARoueModes::EstMultijoueur() const
{
	return GetNetMode() != NM_Standalone;
}

ARoueRelais* ARoueModes::MonRelais() const
{
	APlayerController* PC = GetPC();
	UWorld* Monde = GetWorld();
	if (!PC || !Monde)
	{
		return nullptr;
	}
	for (TActorIterator<ARoueRelais> It(Monde); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

int32 ARoueModes::IndexDuType(ETypeModeRoue Type) const
{
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if (Modes[i].Type == Type)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

static void CacherPionReseau(APawn* Pion, bool bCacher)
{
	if (!Pion)
	{
		return;
	}
	Pion->SetActorHiddenInGame(bCacher);
	Pion->SetActorEnableCollision(!bCacher);
	TArray<AActor*> Attaches;
	Pion->GetAttachedActors(Attaches);
	for (AActor* A : Attaches)
	{
		if (A)
		{
			A->SetActorHiddenInGame(bCacher);
		}
	}
	if (ACharacter* Perso = Cast<ACharacter>(Pion))
	{
		if (bCacher)
		{
			Perso->GetCharacterMovement()->StopMovementImmediately();
			Perso->GetCharacterMovement()->DisableMovement();
		}
		else
		{
			Perso->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		}
	}
}

static void DetruireAvecAttaches(APawn* Pion)
{
	if (!IsValid(Pion))
	{
		return;
	}
	TArray<AActor*> Attaches;
	Pion->GetAttachedActors(Attaches);
	for (AActor* A : Attaches)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	Pion->Destroy();
}

void ARoueModes::ChoisirModeReseau(ARoueRelais* Relais, int32 Index)
{
	if (!HasAuthority() || !IsValid(Relais) || !Modes.IsValidIndex(Index))
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Ancien = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Ancien)
	{
		Relais->ClientAvis(TEXT("Attends d'avoir un personnage avant de changer de mode."));
		return;
	}
	const FModeRoue& Mode = Modes[Index];
	const int32 IndexSoldat = IndexDuType(ETypeModeRoue::APied);
	const int32 Avant = Modes.IsValidIndex(Relais->Mode) ? Relais->Mode : IndexSoldat;
	if (Avant == Index)
	{
		return;
	}
	if (Mode.Type == ETypeModeRoue::Libre)
	{
		Relais->ClientAvis(TEXT("Case libre : aucune fonction pour l'instant."));
		return;
	}

	// Le soldat de depart : celui qu'on joue maintenant si on est en mode soldat, sinon celui qui est cache
	const bool bDepuisSoldat = (Avant == IndexSoldat);
	APawn* Soldat = bDepuisSoldat ? Ancien : (IsValid(Relais->PionCache) ? Relais->PionCache : nullptr);
	if (bDepuisSoldat && !Relais->ClasseSoldat)
	{
		Relais->ClasseSoldat = Ancien->GetClass();
	}

	const FVector Lieu = Ancien->GetActorLocation();
	const FRotator Rot(0.f, PC->GetControlRotation().Yaw, 0.f);
	APawn* Nouveau = nullptr;

	if (Mode.Type == ETypeModeRoue::APied && Soldat && Soldat != Ancien)
	{
		// Retour au soldat cache : il reapparait la ou on est
		Soldat->SetActorLocationAndRotation(Lieu + FVector(0.f, 0.f, 20.f), Rot, false, nullptr, ETeleportType::TeleportPhysics);
		CacherPionReseau(Soldat, false);
		Nouveau = Soldat;
	}
	else
	{
		UClass* Classe = nullptr;
		if (Mode.Type == ETypeModeRoue::APied)
		{
			Classe = Relais->ClasseSoldat;
			if (!Classe)
			{
				AGameModeBase* Jeu = GetWorld()->GetAuthGameMode();
				Classe = Jeu ? Jeu->GetDefaultPawnClassForController(PC) : nullptr;
			}
		}
		else if (ClassesVehicules.IsValidIndex(Index))
		{
			Classe = ClassesVehicules[Index];
		}
		if (!Classe)
		{
			Relais->ClientAvis(FString::Printf(TEXT("%s indisponible : %s introuvable."), *Mode.Nom.ToString(), *Mode.ClasseVehicule.ToString()));
			return;
		}
		Ancien->SetActorEnableCollision(false); // pour apparaitre a la meme place
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Nouveau = GetWorld()->SpawnActor<APawn>(Classe, Lieu, Rot, Params);
		if (!Nouveau)
		{
			Ancien->SetActorEnableCollision(true);
			Relais->ClientAvis(FString::Printf(TEXT("%s : le personnage n'a pas pu apparaitre."), *Mode.Nom.ToString()));
			return;
		}
	}

	PC->Possess(Nouveau);
	PC->SetControlRotation(Rot);
	PC->ClientSetRotation(Rot);

	// L'ancien personnage : le soldat est cache (on le garde avec sa vie et ses armes), les autres disparaissent
	if (Ancien == Soldat)
	{
		CacherPionReseau(Ancien, true);
		Relais->PionCache = Ancien;
	}
	else
	{
		DetruireAvecAttaches(Ancien);
		if (Nouveau == Soldat || Mode.Type == ETypeModeRoue::APied)
		{
			if (IsValid(Soldat) && Soldat != Nouveau)
			{
				DetruireAvecAttaches(Soldat);
			}
			Relais->PionCache = nullptr;
		}
	}

	Relais->Mode = Index;
	Relais->PionActif = Nouveau;
	Relais->PionHabille = Mode.Costume.IsNull() ? nullptr : Nouveau;
	HabillerPion(Relais->PionHabille, Mode, false);
	Relais->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Roue] %s joue maintenant \"%s\" (%s)."), *PC->GetName(), *Mode.Nom.ToString(), *Nouveau->GetName());
}

void ARoueModes::GererRelaisServeur()
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	TArray<APlayerController*> DejaServis;
	TArray<ARoueRelais*> Relais;
	for (TActorIterator<ARoueRelais> It(Monde); It; ++It)
	{
		Relais.Add(*It);
	}
	for (ARoueRelais* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			// Joueur parti : on range son soldat cache et son relais
			DetruireAvecAttaches(R->PionCache);
			R->Destroy();
			continue;
		}
		DejaServis.Add(PC);

		// Le jeu a donne un autre personnage au joueur (mort puis reapparition) : retour au mode soldat
		APawn* Actuel = PC->GetPawn();
		if (Actuel && Actuel != R->PionActif)
		{
			const bool bRienAChanger = (R->PionActif == nullptr && R->Mode == INDEX_NONE && R->PionCache == nullptr);
			if (!bRienAChanger)
			{
				DetruireAvecAttaches(R->PionCache);
				R->PionCache = nullptr;
				R->PionHabille = nullptr;
				R->Mode = INDEX_NONE;
				R->ClasseSoldat = nullptr;
				R->ForceNetUpdate();
			}
			R->PionActif = Actuel;
		}
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || DejaServis.Contains(PC))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ARoueRelais* Nouveau = Monde->SpawnActor<ARoueRelais>(ARoueRelais::StaticClass(), FTransform::Identity, Params))
		{
			Nouveau->PionActif = PC->GetPawn();
			UE_LOG(LogTemp, Log, TEXT("[Roue] Roue prete pour le joueur %s."), *PC->GetName());
		}
	}
}

void ARoueModes::SuivreJoueursReseau()
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	TArray<TWeakObjectPtr<APawn>> Caches;
	for (TActorIterator<ARoueRelais> It(Monde); It; ++It)
	{
		ARoueRelais* R = *It;
		// Costume Spider-Man des autres joueurs (et du notre) sur cette machine
		if (IsValid(R->PionHabille) && Modes.IsValidIndex(R->Mode))
		{
			HabillerPion(R->PionHabille, Modes[R->Mode], false);
		}
		// Soldat cache : pas de collision ici non plus (la collision n'est pas copiee par le reseau)
		if (IsValid(R->PionCache))
		{
			R->PionCache->SetActorEnableCollision(false);
			Caches.Add(R->PionCache);
		}
	}
	for (const TWeakObjectPtr<APawn>& Ancien : CollisionsCoupees)
	{
		if (Ancien.IsValid() && !Caches.Contains(Ancien))
		{
			Ancien->SetActorEnableCollision(true); // soldat revenu en jeu
		}
	}
	CollisionsCoupees = Caches;
}

// ---------------------------------------------------------------------------
// Circulation (plugin ChellesCirculation, par reflexion) : moins de voitures dans certains modes (SUV)
// ---------------------------------------------------------------------------

void ARoueModes::ReglerCirculation()
{
	// La limite la plus basse parmi les modes joues. En multijoueur on regarde tous les joueurs :
	// deux joueurs en SUV = la meme limite qu'un seul (elle ne s'additionne pas).
	int32 Limite = 0;
	auto Prendre = [this, &Limite](int32 Index)
	{
		if (Modes.IsValidIndex(Index) && Modes[Index].LimiteCirculation > 0)
		{
			Limite = (Limite > 0) ? FMath::Min(Limite, Modes[Index].LimiteCirculation) : Modes[Index].LimiteCirculation;
		}
	};
	if (EstMultijoueur())
	{
		for (TActorIterator<ARoueRelais> It(GetWorld()); It; ++It)
		{
			Prendre(It->Mode);
		}
	}
	else
	{
		Prendre(ModeActuel());
	}
	if (Limite == LimiteCirculationAppliquee)
	{
		return;
	}
	if (!ClasseCirculation)
	{
		ClasseCirculation = TrouverClasse(TEXT("/Script/ChellesCirculation.CirculationSpawner"), TEXT("CirculationSpawner"));
		if (!ClasseCirculation)
		{
			return; // plugin circulation absent
		}
	}
	FIntProperty* PropLimite = FindFProperty<FIntProperty>(ClasseCirculation, TEXT("LimiteEnJeu"));
	FBoolProperty* PropUneParRue = FindFProperty<FBoolProperty>(ClasseCirculation, TEXT("bUneVoitureParRue"));
	if (!PropLimite)
	{
		static bool bDejaDit = false;
		if (!bDejaDit)
		{
			bDejaDit = true;
			UE_LOG(LogTemp, Warning, TEXT("[Roue] Circulation : recompiler ChellesCirculation pour limiter les voitures en SUV."));
		}
		LimiteCirculationAppliquee = Limite;
		return;
	}
	for (TActorIterator<AActor> It(GetWorld(), ClasseCirculation); It; ++It)
	{
		PropLimite->SetPropertyValue_InContainer(*It, Limite);
		if (PropUneParRue)
		{
			PropUneParRue->SetPropertyValue_InContainer(*It, Limite > 0);
		}
	}
	LimiteCirculationAppliquee = Limite;
	UE_LOG(LogTemp, Log, TEXT("[Roue] Circulation : %s."), Limite > 0
		? *FString::Printf(TEXT("%d voitures maximum, une par rue"), Limite) : TEXT("normale"));
}

// ---------------------------------------------------------------------------
// PANTERA (2026-11-11) : choisir un mode par son nom, yeux blancs du masque, limite de vol
// ---------------------------------------------------------------------------

bool ARoueModes::ChoisirModeParNom(const FString& NomMode)
{
	for (int32 i = 0; i < Modes.Num(); ++i)
	{
		if (Modes[i].Nom.ToString().Equals(NomMode, ESearchCase::IgnoreCase))
		{
			if (ModeActuel() != i)
			{
				ChoisirMode(i);
			}
			return ModeActuel() == i;
		}
	}
	UE_LOG(LogTemp, Warning, TEXT("[Roue] Aucun mode ne s'appelle \"%s\"."), *NomMode);
	return false;
}

namespace RouePantera
{
	static const FName TagYeux(TEXT("ChellesYeuxMasque"));

	/**
	 * Reglage des yeux, en cm, dans le repere du corps au repos (face vers +Y, haut = +Z).
	 * Valeurs mesurees sur le mannequin du pack SuperheroFlight : tete a Z 165, visage a Y 8 / 9 au niveau des yeux.
	 * Modifiables en jeu (console) : Chelles.PanteraYeux Avant Haut Ecart Taille Inclinaison Ouverture
	 * Enregistres dans GameUserSettings.ini, section [ChellesRoue], cle YeuxPantera.
	 */
	struct FReglageYeux
	{
		float Avant = 8.1f;        // vers l'avant du visage
		float Haut = 171.f;        // hauteur depuis les pieds
		float Ecart = 3.6f;        // distance entre le milieu du visage et le centre d'un oeil
		float Taille = 1.f;        // multiplie la taille (oeil = 3,2 x 1,4 cm)
		float Inclinaison = 15.f;  // degres : coin exterieur releve (regard de felin)
		float Ouverture = 35.f;    // degres : l'oeil suit l'arrondi de la tete

		FString VersTexte() const
		{
			return FString::Printf(TEXT("%g,%g,%g,%g,%g,%g"), Avant, Haut, Ecart, Taille, Inclinaison, Ouverture);
		}
		void DepuisTexte(const FString& Texte)
		{
			TArray<FString> Morceaux;
			Texte.ParseIntoArray(Morceaux, TEXT(","), true);
			float* Champs[] = { &Avant, &Haut, &Ecart, &Taille, &Inclinaison, &Ouverture };
			for (int32 i = 0; i < Morceaux.Num() && i < 6; ++i)
			{
				*Champs[i] = FCString::Atof(*Morceaux[i].TrimStartAndEnd());
			}
			Taille = FMath::Clamp(Taille, 0.2f, 5.f);
		}
	};

	static FReglageYeux& Reglage()
	{
		static FReglageYeux R;
		static bool bLu = false;
		if (!bLu && GConfig)
		{
			bLu = true;
			FString Texte;
			if (GConfig->GetString(TEXT("ChellesRoue"), TEXT("YeuxPantera"), Texte, GGameUserSettingsIni))
			{
				R.DepuisTexte(Texte);
			}
		}
		return R;
	}

	static void YeuxDe(USkeletalMeshComponent* Corps, TArray<USceneComponent*>& Yeux)
	{
		Yeux.Reset();
		if (!Corps)
		{
			return;
		}
		for (USceneComponent* Enfant : Corps->GetAttachChildren())
		{
			if (IsValid(Enfant) && Enfant->ComponentHasTag(TagYeux))
			{
				Yeux.Add(Enfant);
			}
		}
	}

	/** Les yeux suivent le corps : caches s'il est cache (vol bascule, effet verre...). */
	static void SuivreVisibilite(USkeletalMeshComponent* Corps, const TArray<USceneComponent*>& Yeux)
	{
		const bool bCache = !Corps->IsVisible() || Corps->bHiddenInGame;
		for (USceneComponent* Oeil : Yeux)
		{
			if (Oeil->bHiddenInGame != bCache)
			{
				Oeil->SetHiddenInGame(bCache);
			}
		}
	}
}

void ARoueModes::MettreYeuxMasque(USkeletalMeshComponent* Corps, bool bAvecYeux)
{
	using namespace RouePantera;
	if (!IsValid(Corps) || !Corps->GetOwner())
	{
		return;
	}
	TArray<USceneComponent*> Yeux;
	YeuxDe(Corps, Yeux);
	if (bAvecYeux && Yeux.Num() == 2)
	{
		SuivreVisibilite(Corps, Yeux);
		return; // deja poses
	}
	for (USceneComponent* Oeil : Yeux)
	{
		Oeil->DestroyComponent();
	}
	if (!bAvecYeux || !Corps->SkeletalMesh)
	{
		return;
	}

	// Position de l'os de la tete au repos (repere du corps)
	const FReferenceSkeleton& Squelette = Corps->SkeletalMesh->GetRefSkeleton();
	const int32 Tete = Squelette.FindBoneIndex(TEXT("head"));
	if (Tete == INDEX_NONE)
	{
		return;
	}
	FTransform TeteRepos = FTransform::Identity;
	for (int32 Os = Tete; Os != INDEX_NONE; Os = Squelette.GetParentIndex(Os))
	{
		TeteRepos = TeteRepos * Squelette.GetRefBonePose()[Os];
	}

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Sphere)
	{
		return;
	}
	UMaterialInterface* Blanc = LoadObject<UMaterialInterface>(nullptr, CHEMIN_YEUX_PANTERA);
	if (!Blanc)
	{
		// Script pas encore lance : materiau de base de l'Engine, en blanc
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(Base, Corps->GetOwner());
			Dyn->SetVectorParameterValue(TEXT("Color"), FLinearColor::White);
			Blanc = Dyn;
		}
	}

	const FReglageYeux& R = Reglage();
	for (const float Cote : { -1.f, 1.f })
	{
		// Un oeil en amande : sphere aplatie (3,2 x 1,4 cm, 1,4 cm d'epaisseur), tournee vers l'exterieur
		const FVector Lieu(Cote * R.Ecart, R.Avant, R.Haut);
		const FRotator Sens(Cote * R.Inclinaison, -Cote * R.Ouverture, 0.f);
		const FVector Echelle = FVector(0.032f, 0.014f, 0.014f) * R.Taille;
		const FTransform OeilRepos(Sens, Lieu, Echelle);

		UStaticMeshComponent* Oeil = NewObject<UStaticMeshComponent>(Corps->GetOwner(), NAME_None, RF_Transient);
		Oeil->ComponentTags.Add(TagYeux);
		Oeil->SetStaticMesh(Sphere);
		if (Blanc)
		{
			Oeil->SetMaterial(0, Blanc);
		}
		Oeil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Oeil->SetGenerateOverlapEvents(false);
		Oeil->SetCastShadow(false);
		Oeil->SetupAttachment(Corps, TEXT("head"));
		Oeil->SetRelativeTransform(OeilRepos.GetRelativeTransform(TeteRepos));
		Oeil->RegisterComponent();
	}
	YeuxDe(Corps, Yeux);
	SuivreVisibilite(Corps, Yeux);
}

void ARoueModes::ReglerYeux(UWorld* Monde, const TArray<FString>& Arguments)
{
	using namespace RouePantera;
	FReglageYeux& R = Reglage();
	if (Arguments.Num() > 0)
	{
		if (Arguments[0].Equals(TEXT("Defaut"), ESearchCase::IgnoreCase))
		{
			R = FReglageYeux();
		}
		else
		{
			R.DepuisTexte(FString::Join(Arguments, TEXT(",")));
		}
		if (GConfig)
		{
			GConfig->SetString(TEXT("ChellesRoue"), TEXT("YeuxPantera"), *R.VersTexte(), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
	const FString Texte = FString::Printf(TEXT("Yeux PANTERA : Avant %g  Haut %g  Ecart %g  Taille %g  Inclinaison %g  Ouverture %g   (Chelles.PanteraYeux Defaut = reglage d'origine)"),
		R.Avant, R.Haut, R.Ecart, R.Taille, R.Inclinaison, R.Ouverture);
	UE_LOG(LogTemp, Warning, TEXT("[Roue] %s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, Texte);
	}
	if (!Monde)
	{
		return;
	}
	// On refait les yeux de tous les personnages qui en ont
	TArray<USkeletalMeshComponent*> AvecYeux;
	for (TObjectIterator<USkeletalMeshComponent> It; It; ++It)
	{
		USkeletalMeshComponent* Corps = *It;
		TArray<USceneComponent*> Yeux;
		if (IsValid(Corps) && Corps->GetWorld() == Monde)
		{
			YeuxDe(Corps, Yeux);
		}
		if (Yeux.Num() > 0)
		{
			AvecYeux.Add(Corps);
		}
	}
	for (USkeletalMeshComponent* Corps : AvecYeux)
	{
		TArray<USceneComponent*> Yeux;
		YeuxDe(Corps, Yeux);
		for (USceneComponent* Oeil : Yeux)
		{
			Oeil->DestroyComponent();
		}
		MettreYeuxMasque(Corps, true);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GCommandeYeuxPantera(
	TEXT("Chelles.PanteraYeux"),
	TEXT("Yeux blancs de PANTERA. Chelles.PanteraYeux Avant Haut Ecart Taille Inclinaison Ouverture (cm et degres). Ex : Chelles.PanteraYeux 8.5 171 3.6 1 15 35. Chelles.PanteraYeux Defaut = reglage d'origine."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Arguments, UWorld* Monde)
	{
		ARoueModes::ReglerYeux(Monde, Arguments);
	}));

/** Composant de vol du pack SuperheroFlight (BP_SuperheroFlightComponent) sur ce personnage. */
static UActorComponent* ComposantVol(APawn* Pion)
{
	if (!Pion)
	{
		return nullptr;
	}
	for (UActorComponent* C : Pion->GetComponents())
	{
		if (C && C->GetClass()->GetName().Contains(TEXT("SuperheroFlightComponent")))
		{
			return C;
		}
	}
	return nullptr;
}

/** Le personnage vole-t-il ? Variable IsFlying du pack, sinon le mode de deplacement "Flying". */
static bool EnTrainDeVoler(ACharacter* Perso, UActorComponent* Vol)
{
	if (Vol)
	{
		if (FBoolProperty* P = FindFProperty<FBoolProperty>(Vol->GetClass(), TEXT("IsFlying")))
		{
			if (P->GetPropertyValue_InContainer(Vol))
			{
				return true;
			}
		}
	}
	const UCharacterMovementComponent* Mvt = Perso ? Perso->GetCharacterMovement() : nullptr;
	return Mvt && Mvt->MovementMode == MOVE_Flying;
}

/** Atterrissage force : on demande au pack d'arreter le vol, puis on fait retomber le personnage. */
static void ForcerAtterrissage(ACharacter* Perso, UActorComponent* Vol)
{
	if (Vol)
	{
		if (UFunction* Fonction = Vol->FindFunction(TEXT("StopFlight")))
		{
			const int32 Taille = FMath::Max<int32>(Fonction->ParmsSize, 1);
			uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
			FMemory::Memzero(Parametres, Taille);
			Vol->ProcessEvent(Fonction, Parametres);
		}
	}
	UCharacterMovementComponent* Mvt = Perso ? Perso->GetCharacterMovement() : nullptr;
	if (Mvt && Mvt->MovementMode == MOVE_Flying)
	{
		Mvt->SetMovementMode(MOVE_Falling);
	}
}

void ARoueModes::LimiterVol(float DeltaSeconds)
{
	APlayerController* PC = GetPC();
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;

	// Yeux du masque : ils suivent la visibilite du corps (effet verre, bascule de vol...)
	if (Perso && Perso->GetMesh())
	{
		TArray<USceneComponent*> Yeux;
		RouePantera::YeuxDe(Perso->GetMesh(), Yeux);
		if (Yeux.Num() > 0)
		{
			RouePantera::SuivreVisibilite(Perso->GetMesh(), Yeux);
		}
	}

	const int32 Mode = Perso ? ModeActuel() : INDEX_NONE;
	const float Maximum = (Modes.IsValidIndex(Mode) && Modes[Mode].Type == ETypeModeRoue::Vol) ? Modes[Mode].DureeVolMax : 0.f;
	if (!Perso || Maximum <= 0.f || Perso != PionVolSuivi.Get())
	{
		TempsVol = 0.f;
		bAtterrissageForce = false;
		DernierAvisVol = -1;
		PionVolSuivi = (Maximum > 0.f) ? Perso : nullptr;
		if (Maximum <= 0.f)
		{
			return;
		}
	}

	UActorComponent* Vol = ComposantVol(Perso);
	const UCharacterMovementComponent* Mvt = Perso->GetCharacterMovement();
	const FString& Nom = Modes[Mode].Nom.ToString();
	if (EnTrainDeVoler(Perso, Vol))
	{
		if (bAtterrissageForce)
		{
			// Il repart sans avoir touche le sol : on le refait tomber
			ForcerAtterrissage(Perso, Vol);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(42771, 2.f, FColor::Orange, FString::Printf(TEXT("%s : pose-toi au sol pour pouvoir revoler."), *Nom));
			}
			return;
		}
		if (TempsVol <= 0.f && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(42770, 3.f, FColor::Cyan, FString::Printf(TEXT("%s : %d secondes de vol maximum"), *Nom, FMath::RoundToInt(Maximum)));
		}
		TempsVol += DeltaSeconds;
		const int32 Reste = FMath::CeilToInt(Maximum - TempsVol);
		if (Reste <= 5 && Reste > 0 && Reste != DernierAvisVol)
		{
			DernierAvisVol = Reste;
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(42770, 1.2f, FColor::Yellow, FString::Printf(TEXT("%s : encore %d s de vol"), *Nom, Reste));
			}
		}
		if (TempsVol >= Maximum)
		{
			ForcerAtterrissage(Perso, Vol);
			bAtterrissageForce = true;
			Message(FString::Printf(TEXT("%s : %d secondes de vol, atterrissage ! Touche le sol pour revoler."), *Nom, FMath::RoundToInt(Maximum)));
		}
	}
	else if (Mvt && Mvt->IsMovingOnGround())
	{
		// Pose au sol : le compteur repart a zero
		TempsVol = 0.f;
		bAtterrissageForce = false;
		DernierAvisVol = -1;
	}
}

// ---------------------------------------------------------------------------
// Spider-Male avec les fonctions du soldat (2026-11-11, demande MERIDJA)
// Au sol : le soldat FPS_Game (armes, tir, visee, recharge, ramassage, vie) habille en Spider-Male.
// Sauter puis rappuyer sur Espace en l'air : on passe au volant (pack SuperheroFlight) qui decolle.
// En se posant : retour au soldat, avec ses armes. Solo seulement (en multijoueur la case reste le volant).
// ---------------------------------------------------------------------------

/** Tag pose sur le soldat costume : le menu HABILLAGE (plugin ChellesMenu) ne l'habille pas pendant ce temps. */
static const FName TagSansHabillage(TEXT("ChellesSansHabillage"));

/** Lance le vol du pack (comme sauter puis rappuyer sur Espace avec le volant). */
static void DemarrerVolPack(UActorComponent* Vol)
{
	if (!Vol)
	{
		return;
	}
	if (UFunction* Fonction = Vol->FindFunction(TEXT("StartFlight")))
	{
		const int32 Taille = FMath::Max<int32>(Fonction->ParmsSize, 1);
		uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
		FMemory::Memzero(Parametres, Taille);
		Vol->ProcessEvent(Fonction, Parametres);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Roue] StartFlight introuvable sur %s : rappuie sur Espace pour voler."), *GetNameSafe(Vol));
	}
}

/** Pion range dans l'acteur Bascule Vol (PionPied ou PionVol), lu par reflexion. */
static APawn* PionDeBascule(AActor* Bascule, const TCHAR* Nom)
{
	if (!IsValid(Bascule))
	{
		return nullptr;
	}
	FObjectProperty* P = FindFProperty<FObjectProperty>(Bascule->GetClass(), Nom);
	APawn* Pion = P ? Cast<APawn>(P->GetObjectPropertyValue_InContainer(Bascule)) : nullptr;
	return IsValid(Pion) ? Pion : nullptr;
}

void ARoueModes::PreparerCasesSoldatVolant()
{
	// Roue enregistree dans un niveau avec ses anciennes cases : la case Spider-Male recoit quand meme les armes
	const FSoftObjectPath CorpsSpiderMale(FString(CHEMIN_COSTUME_SPIDER_MALE_VOL));
	for (FModeRoue& M : Modes)
	{
		if (M.Type == ETypeModeRoue::Vol && M.CostumeSoldat.IsNull() && M.Costume == CorpsSpiderMale)
		{
			M.bFonctionsSoldat = true;
			M.CostumeSoldat = FSoftObjectPath(FString(CHEMIN_COSTUME_SPIDER_MALE_SOLDAT));
		}
	}
}

void ARoueModes::HabillerSoldat(APawn* Soldat, bool bCostume)
{
	ACharacter* Perso = Cast<ACharacter>(Soldat);
	if (!IsValid(Perso))
	{
		return;
	}

	if (!bCostume)
	{
		// On rend au soldat le corps qu'il avait, puis le menu HABILLAGE le rhabille
		if (SoldatCostume.Get() == Perso)
		{
			for (const FRoueCorpsAvant& Avant : CorpsAvant)
			{
				USkeletalMeshComponent* C = Avant.Composant.Get();
				if (!C)
				{
					continue;
				}
				C->SetSkeletalMesh(Avant.Mesh.Get());
				C->EmptyOverrideMaterials();
				for (int32 i = 0; i < Avant.Materiaux.Num(); ++i)
				{
					if (Avant.Materiaux[i].IsValid())
					{
						C->SetMaterial(i, Avant.Materiaux[i].Get());
					}
				}
			}
			CorpsAvant.Reset();
			SoldatCostume = nullptr;
		}
		Perso->Tags.Remove(TagSansHabillage);
		return;
	}

	if (!Modes.IsValidIndex(ModeSoldatVol))
	{
		return;
	}
	const FModeRoue& Mode = Modes[ModeSoldatVol];
	USkeletalMesh* Costume = Cast<USkeletalMesh>(Mode.CostumeSoldat.ResolveObject());
	if (!Costume)
	{
		Costume = Cast<USkeletalMesh>(Mode.CostumeSoldat.TryLoad());
	}
	if (!Costume)
	{
		if (!bAvisCostumeSoldat)
		{
			bAvisCostumeSoldat = true;
			Message(FString::Printf(TEXT("%s : corps introuvable (%s). Les armes marchent, mais le soldat garde son apparence."),
				*Mode.Nom.ToString(), *Mode.CostumeSoldat.ToString()));
		}
		return;
	}

	if (SoldatCostume.Get() != Perso)
	{
		// Premier passage, ou nouveau soldat (reapparu apres la mort)
		if (SoldatCostume.IsValid())
		{
			HabillerSoldat(SoldatCostume.Get(), false);
		}
		CorpsAvant.Reset();
		SoldatCostume = Perso;

		// Le corps (vu par les autres et en 3e personne) et les jambes vues en 1re personne
		TArray<USkeletalMeshComponent*> Cibles;
		if (Perso->GetMesh())
		{
			Cibles.Add(Perso->GetMesh());
		}
		TArray<USkeletalMeshComponent*> Composants;
		Perso->GetComponents<USkeletalMeshComponent>(Composants);
		for (USkeletalMeshComponent* C : Composants)
		{
			if (C && C != Perso->GetMesh() && C->GetName().Contains(TEXT("Legs")) && !C->ComponentHasTag(FName(TEXT("ChellesJambes"))))
			{
				Cibles.Add(C);
			}
		}
		for (USkeletalMeshComponent* C : Cibles)
		{
			if (!C->SkeletalMesh || C->SkeletalMesh->GetSkeleton() != Costume->GetSkeleton())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Roue] %s : %s a un autre squelette (%s), costume non mis sur ce morceau."),
					*Mode.Nom.ToString(), *C->GetName(), C->SkeletalMesh ? *GetNameSafe(C->SkeletalMesh->GetSkeleton()) : TEXT("aucun"));
				continue;
			}
			FRoueCorpsAvant Avant;
			Avant.Composant = C;
			Avant.Mesh = C->SkeletalMesh;
			for (UMaterialInterface* M : C->OverrideMaterials)
			{
				Avant.Materiaux.Add(M);
			}
			CorpsAvant.Add(MoveTemp(Avant));
		}
		Perso->Tags.AddUnique(TagSansHabillage);
		UE_LOG(LogTemp, Log, TEXT("[Roue] \"%s\" : soldat %s habille (%d morceaux), armes du soldat au sol."),
			*Mode.Nom.ToString(), *Perso->GetName(), CorpsAvant.Num());
	}

	// (Re)met le costume si quelqu'un l'a change
	for (const FRoueCorpsAvant& Avant : CorpsAvant)
	{
		USkeletalMeshComponent* C = Avant.Composant.Get();
		if (C && C->SkeletalMesh != Costume)
		{
			C->SetSkeletalMesh(Costume);
			C->EmptyOverrideMaterials();
		}
	}
}

void ARoueModes::FinSoldatVolant()
{
	if (SoldatCostume.IsValid())
	{
		HabillerSoldat(SoldatCostume.Get(), false);
	}
	CorpsAvant.Reset();
	SoldatCostume = nullptr;
	ModeSoldatVol = INDEX_NONE;
	bDemarrerVol = false;
	TempsPose = 0.f;
}

void ARoueModes::ToucheSaut()
{
	if (!Modes.IsValidIndex(ModeSoldatVol) || bOuverte || EstMultijoueur() || !IsValid(Bascule) || EstEnVol())
	{
		return;
	}
	APlayerController* PC = GetPC();
	ACharacter* Soldat = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	const UCharacterMovementComponent* Mvt = Soldat ? Soldat->GetCharacterMovement() : nullptr;
	if (!Mvt || !Mvt->IsFalling() || ModeActuel() != ModeSoldatVol)
	{
		return; // au sol : c'est le saut normal du soldat
	}
	// En l'air (apres un saut) : on s'envole
	Appeler(Bascule, TEXT("Basculer"));
	if (!EstEnVol())
	{
		Message(FString::Printf(TEXT("%s : impossible de s'envoler (voir le journal, lignes [BasculeVol])."), *Modes[ModeSoldatVol].Nom.ToString()));
		return;
	}
	MettreCostume(Modes[ModeSoldatVol]);
	bDemarrerVol = true;
	TempsPose = 0.f;
}

void ARoueModes::SuivreSoldatVolant(float DeltaSeconds)
{
	if (!Modes.IsValidIndex(ModeSoldatVol))
	{
		return;
	}
	if (EstMultijoueur())
	{
		FinSoldatVolant();
		return;
	}
	APlayerController* PC = GetPC();
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Perso || ModeActuel() != ModeSoldatVol)
	{
		return;
	}

	if (!EstEnVol())
	{
		// A pied : c'est le soldat (aussi apres une mort, le nouveau soldat est rhabille)
		HabillerSoldat(Perso, true);
		bDemarrerVol = false;
		TempsPose = 0.f;
		return;
	}

	UActorComponent* Vol = ComposantVol(Perso);
	if (bDemarrerVol)
	{
		bDemarrerVol = false;
		DemarrerVolPack(Vol);
		return;
	}

	// Pose au sol avec le volant : on reprend le soldat et ses armes
	const UCharacterMovementComponent* Mvt = Perso->GetCharacterMovement();
	if (!EnTrainDeVoler(Perso, Vol) && Mvt && Mvt->IsMovingOnGround())
	{
		TempsPose += DeltaSeconds;
		if (TempsPose >= 0.35f)
		{
			TempsPose = 0.f;
			if (!PionDeBascule(Bascule, TEXT("PionPied")))
			{
				return; // plus de soldat (mort) : on reste avec le volant
			}
			Appeler(Bascule, TEXT("Basculer"));
			if (!EstEnVol() && PC->GetPawn())
			{
				HabillerSoldat(PC->GetPawn(), true);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(42772, 2.f, FColor::Green,
						FString::Printf(TEXT("%s : armes en main"), *Modes[ModeSoldatVol].Nom.ToString()));
				}
			}
		}
	}
	else
	{
		TempsPose = 0.f;
	}
}

// ---------------------------------------------------------------------------
// Guide des commandes du pack SuperheroFlight (2026-11-11)
// BP_Player cree son widget WBP_ControlsGuide dans son BeginPlay, seulement s'il est deja
// controle par le joueur. Or la roue et ChellesBasculeVol font apparaitre Spider-Man PUIS le
// donnent au joueur (Possess) : au BeginPlay personne ne le controle, la variable reste vide,
// et chaque appui sur E / Retour arriere / F1 / F2 donne "Accessed None ... WBP_ControlsGuide".
// On cree donc le widget manquant et on le range dans la variable, SANS l'afficher a l'ecran :
// rien ne change a l'image, les erreurs disparaissent. Aucun Blueprint n'est modifie.
// ---------------------------------------------------------------------------

void ARoueModes::DonnerGuideCommandes()
{
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (Pion != PionGuide.Get())
	{
		PionGuide = Pion;
		bGuideVerifie = false;
	}
	if (bGuideVerifie || !IsValid(Pion) || !PC->IsLocalController())
	{
		return;
	}
	bGuideVerifie = true; // une seule verification par personnage

	FObjectProperty* Variable = FindFProperty<FObjectProperty>(Pion->GetClass(), TEXT("WBP_ControlsGuide"));
	if (!Variable || !Variable->PropertyClass || !Variable->PropertyClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return; // pas un personnage du pack SuperheroFlight
	}
	if (Variable->GetObjectPropertyValue_InContainer(Pion))
	{
		return; // le Blueprint l'a deja cree lui-meme
	}
	UUserWidget* Guide = CreateWidget<UUserWidget>(PC, Variable->PropertyClass);
	if (Guide)
	{
		Variable->SetObjectPropertyValue_InContainer(Pion, Guide);
		UE_LOG(LogTemp, Log, TEXT("[Roue] Guide des commandes donne a %s (non affiche)"), *Pion->GetName());
	}
}
