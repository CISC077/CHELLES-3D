// CHELLES 3D - Roue des modes : maintenir Tab pour choisir Soldat / Spider-Man / Motoneige / Automobile (UE 4.27)
#include "RoueModes.h"

#include "SRoueModes.h"

#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

#define CHEMIN_MOTONEIGE TEXT("/Game/DV_Snowmobile/Blueprints/BP_Snowmobile.BP_Snowmobile_C")
// Spider-Man sur le squelette du pack SuperPowers (cree par Tools\\SpiderManJoueur\\creer_spiderman_pouvoirs.py)
#define CHEMIN_COSTUME_SPIDERMAN TEXT("/Game/Personnages/SK_SpiderMan_UE4/SK_SpiderMan_Pouvoirs.SK_SpiderMan_Pouvoirs")

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

	// Dernier bouton : le cheval du pack HorseStarterKit (le cavalier fait partie du cheval)
	FModeRoue Cheval;
	Cheval.Nom = FText::FromString(TEXT("HORSE"));
	Cheval.Type = ETypeModeRoue::Vehicule;
	Cheval.Icone = EIconeRoue::FerACheval;
	Cheval.Couleur = FLinearColor(0.85f, 0.55f, 0.28f);
	Cheval.ClasseVehicule = FSoftClassPath(FString(TEXT("/Game/HorseStarterKit/Blueprint/BP_Rideable_DarkBrown_Horse.BP_Rideable_DarkBrown_Horse_C")));
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
		if (Modes[i].Type == ETypeModeRoue::Vehicule && Modes[i].ClasseVehicule.IsValid())
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
		// Pendant la conduite, F (descendre puis voler) passe aussi par la roue. E reste pour descendre.
		if (FObjectProperty* P = FindFProperty<FObjectProperty>(A->GetClass(), TEXT("EntreesConduite")))
		{
			RetirerTouche(Cast<UInputComponent>(P->GetObjectPropertyValue_InContainer(A)), EKeys::F);
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
	if (Mode.Costume.IsNull())
	{
		return;
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
		AActor* Controleur = ControleurVehicule(Index);
		if (!Controleur)
		{
			Message(FString::Printf(TEXT("%s : impossible de preparer la conduite."), *Mode.Nom.ToString()));
			break;
		}
		Appeler(Controleur, TEXT("Basculer")); // le pieton (ou Spider-Man) monte, le vehicule arrive a cote
		if (AppelerBool(Controleur, TEXT("EstSurMoto")))
		{
			MontrerHudPack(RacinePack(Mode.ClasseVehicule));
			MettreCostume(Mode);
		}
		else
		{
			Message(FString::Printf(TEXT("%s : le vehicule n'a pas pu venir (voir le journal)."), *Mode.Nom.ToString()));
		}
		break;
	}
	default:
		break;
	}

	RetirerAnciennesTouches();
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
