// CHELLES 3D - Tourelle automatique : vise et tire toute seule sur les ennemis et les zombies proches
#include "TourelleChelles.h"

#include <initializer_list>

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

// Marques posees par les autres plugins Chelles
static const FName MarqueZombieInvasion(TEXT("ChellesZombie"));
static const FName MarqueZombieInvasionHabille(TEXT("ChellesZombieHabille"));
static const FName MarqueEnnemiAuto(TEXT("ChellesEnnemiAuto"));
static const FName MarqueEnnemiPose(TEXT("ChellesEnnemiPose"));
static const FName MarqueHorsJeu(TEXT("ChellesHorsJeu"));
static const FName MarqueTourelle(TEXT("ChellesTourelle"));

#define CHEMIN_MATIERE TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
#define CHEMIN_CUBE TEXT("/Engine/BasicShapes/Cube.Cube")
#define CHEMIN_CYLINDRE TEXT("/Engine/BasicShapes/Cylinder.Cylinder")

namespace
{
	const int32 FrequenceSon = 44100;

	FReglagesTourelle GReglages;
	bool GReglagesLus = false;

	bool EstOui(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		return T.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || T.Equals(TEXT("1")) || T.Equals(TEXT("True"), ESearchCase::IgnoreCase)
			|| T.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	bool LireCouleur(const FString& Texte, FLinearColor& Couleur)
	{
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(","), true);
		if (Morceaux.Num() < 3)
		{
			return false;
		}
		Couleur = FLinearColor::FromSRGBColor(FColor(
			(uint8)FMath::Clamp(FCString::Atoi(*Morceaux[0].TrimStartAndEnd()), 0, 255),
			(uint8)FMath::Clamp(FCString::Atoi(*Morceaux[1].TrimStartAndEnd()), 0, 255),
			(uint8)FMath::Clamp(FCString::Atoi(*Morceaux[2].TrimStartAndEnd()), 0, 255)));
		return true;
	}

	void LireReglages()
	{
		GReglages = FReglagesTourelle();
		const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesTourelle"), TEXT("Tourelle.txt"));
		TArray<FString> Lignes;
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s absent : reglages par defaut"), *Chemin);
			return;
		}
		bool bEnnemisVides = false;
		bool bIgnoresVides = false;
		bool bMenusVides = false;
		FReglagesTourelle& R = GReglages;
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
			auto Nombre = [&Valeur]() { return FCString::Atof(*Valeur.Replace(TEXT(","), TEXT("."))); };
			auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };

			if (Est(TEXT("Actif"))) R.bActif = EstOui(Valeur);
			else if (Est(TEXT("PorteeMetres"))) R.PorteeMetres = FMath::Clamp(Nombre(), 3.f, 300.f);
			else if (Est(TEXT("TirsParSeconde"))) R.TirsParSeconde = FMath::Clamp(Nombre(), 0.2f, 20.f);
			else if (Est(TEXT("Degats"))) R.Degats = FMath::Max(0.f, Nombre());
			else if (Est(TEXT("ViserTete"))) R.bViserTete = EstOui(Valeur);
			else if (Est(TEXT("VitesseRotation"))) R.VitesseRotation = FMath::Clamp(Nombre(), 0.5f, 40.f);
			else if (Est(TEXT("CouleurTraceur"))) LireCouleur(Valeur, R.CouleurTraceur);
			else if (Est(TEXT("VolumeTir"))) R.VolumeTir = FMath::Clamp(Nombre(), 0.f, 4.f);
			else if (Est(TEXT("MinutesAvantCadeau"))) R.MinutesAvantCadeau = FMath::Max(0.f, Nombre());
			else if (Est(TEXT("Compter"))) R.bTempsTotal = !Valeur.Equals(TEXT("Partie"), ESearchCase::IgnoreCase);
			else if (Est(TEXT("TourellesParJoueur"))) R.TourellesParJoueur = FMath::Clamp(FCString::Atoi(*Valeur), 1, 20);
			else if (Est(TEXT("ClassePosee"))) R.ClassePosee = Valeur;
			else if (Est(TEXT("DistancePose"))) R.DistancePose = FMath::Clamp(Nombre(), 80.f, 2000.f);
			else if (Est(TEXT("FermerInventaire"))) R.bFermerInventaire = EstOui(Valeur);
			else if (Est(TEXT("PlaceCase"))) R.PlaceCase = Valeur;
			else if (Est(TEXT("ToucheRapide")))
			{
				R.ToucheRapide = Valeur.IsEmpty() ? FKey() : FKey(*Valeur);
				if (!Valeur.IsEmpty() && !R.ToucheRapide.IsValid())
				{
					UE_LOG(LogTemp, Warning, TEXT("[Tourelle] Touche inconnue : %s (aucune touche rapide)"), *Valeur);
					R.ToucheRapide = FKey();
				}
			}
			else if (Est(TEXT("MenuInventaire")))
			{
				if (!bMenusVides) { R.MenusInventaire.Reset(); bMenusVides = true; }
				if (!Valeur.IsEmpty()) R.MenusInventaire.AddUnique(Valeur);
			}
			else if (Est(TEXT("ClasseEnnemie")))
			{
				if (!bEnnemisVides) { R.ClassesEnnemies.Reset(); bEnnemisVides = true; }
				if (!Valeur.IsEmpty()) R.ClassesEnnemies.AddUnique(Valeur);
			}
			else if (Est(TEXT("ClasseIgnoree")))
			{
				if (!bIgnoresVides) { R.ClassesIgnorees.Reset(); bIgnoresVides = true; }
				if (!Valeur.IsEmpty()) R.ClassesIgnorees.AddUnique(Valeur);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Tourelle] Reglages lus : actif=%d portee=%.0f m, %.1f tirs/s, degats %.0f, cadeau apres %.0f min (%s)"),
			R.bActif ? 1 : 0, R.PorteeMetres, R.TirsParSeconde, R.Degats, R.MinutesAvantCadeau, R.bTempsTotal ? TEXT("temps total") : TEXT("dans la partie"));
	}

	// ------------------------------------------------------------------ Variables des Blueprints ennemis (lecture seule)

	struct FVariablesEnnemi
	{
		FBoolProperty* Mort = nullptr;
		FNumericProperty* Vie = nullptr;
	};

	TMap<TWeakObjectPtr<UClass>, FVariablesEnnemi> GVariables;
	TMap<TWeakObjectPtr<UClass>, bool> GClassesEnnemies;

	FProperty* TrouverVariable(UClass* Classe, std::initializer_list<const TCHAR*> Noms)
	{
		for (const TCHAR* Nom : Noms)
		{
			for (TFieldIterator<FProperty> It(Classe); It; ++It)
			{
				if (It->GetName().Equals(Nom, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	const FVariablesEnnemi& VariablesDe(UClass* Classe)
	{
		if (const FVariablesEnnemi* Deja = GVariables.Find(Classe))
		{
			return *Deja;
		}
		FVariablesEnnemi V;
		V.Mort = CastField<FBoolProperty>(TrouverVariable(Classe, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead"), TEXT("bDead") }));
		V.Vie = CastField<FNumericProperty>(TrouverVariable(Classe, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth") }));
		return GVariables.Add(Classe, V);
	}
}

const FReglagesTourelle& ReglagesTourelle()
{
	if (!GReglagesLus)
	{
		LireReglages();
		GReglagesLus = true;
	}
	return GReglages;
}

void RechargerReglagesTourelle()
{
	LireReglages();
	GReglagesLus = true;
	GClassesEnnemies.Reset();
	GVariables.Reset();
}

// ====================================================================== Sons

USonTourelleChelles::USonTourelleChelles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(FrequenceSon);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Default;
}

int32 USonTourelleChelles::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Sortie = reinterpret_cast<int16*>(OutAudio.GetData());
	const float DeuxPi = 2.f * PI;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = (float)Echantillon / (float)FrequenceSon;
		++Echantillon;
		float Valeur = 0.f;
		switch (Genre)
		{
		case 0:
		{
			// Coup de feu : claquement de bruit + coup sourd grave
			if (T < 0.25f)
			{
				Graine = Graine * 1664525u + 1013904223u;
				const float Blanc = ((Graine >> 9) & 0xFFFF) / 32768.f - 1.f;
				Bruit += 0.45f * (Blanc - Bruit);
				const float Hauteur = 70.f + 160.f * FMath::Exp(-T * 40.f);
				Phase += DeuxPi * Hauteur / (float)FrequenceSon;
				if (Phase > DeuxPi) Phase -= DeuxPi;
				const float Claque = Bruit * FMath::Exp(-T * 38.f);
				const float Sourd = FMath::Sin(Phase) * FMath::Exp(-T * 22.f);
				Valeur = (0.75f * Claque + 0.65f * Sourd) * FMath::Clamp(T / 0.0015f, 0.f, 1.f);
			}
			break;
		}
		case 1:
		case 2:
		{
			// Bip-bip : monte (tourelle posee) ou descend (ramassee)
			const bool bPremier = T < 0.09f;
			const float Hauteur = (Genre == 1) == bPremier ? 660.f : 990.f;
			if (T < 0.19f && (T < 0.08f || T > 0.1f))
			{
				Phase += DeuxPi * Hauteur / (float)FrequenceSon;
				if (Phase > DeuxPi) Phase -= DeuxPi;
				Valeur = 0.35f * FMath::Sin(Phase);
			}
			break;
		}
		case 3:
		{
			// Petit jingle de cadeau : 4 notes qui montent
			static const float Notes[] = { 523.25f, 659.25f, 783.99f, 1046.5f };
			const int32 N = (int32)(T / 0.13f);
			if (N < 4)
			{
				const float Local = T - N * 0.13f;
				Phase += DeuxPi * Notes[N] / (float)FrequenceSon;
				if (Phase > DeuxPi) Phase -= DeuxPi;
				Valeur = 0.32f * FMath::Sin(Phase) * FMath::Exp(-Local * (N == 3 ? 5.f : 14.f));
			}
			break;
		}
		default:
			break;
		}
		Sortie[i] = (int16)FMath::Clamp((int32)(Valeur * 32767.f), -32767, 32767);
	}
	return NumSamples;
}

// ====================================================================== Tourelle

ATourelleChelles::ATourelleChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
	Tags.Add(MarqueTourelle);

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;

	Pied = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pied"));
	Pied->SetupAttachment(Racine);
	Pied->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Racine);

	Canon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Canon"));
	Canon->SetupAttachment(Pivot);
	Canon->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	Squelette = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Squelette"));
	Squelette->SetupAttachment(Racine);
	Squelette->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Eclair = CreateDefaultSubobject<UPointLightComponent>(TEXT("Eclair"));
	Eclair->SetupAttachment(Pivot);
	Eclair->SetIntensity(0.f);
	Eclair->SetAttenuationRadius(500.f);
	Eclair->SetLightColor(FLinearColor(1.f, 0.6f, 0.2f));
	Eclair->SetCastShadows(false);
}

void ATourelleChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATourelleChelles, Cible);
}

void ATourelleChelles::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AppliquerModele(); // pour la voir tout de suite dans l'editeur
}

void ATourelleChelles::BeginPlay()
{
	Super::BeginPlay();
	AppliquerModele();
	bOsMesure = false;

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, CHEMIN_MATIERE))
		{
			MatTraceur = UMaterialInstanceDynamic::Create(Base, this);
			MatTraceur->SetVectorParameterValue(TEXT("Color"), ReglagesTourelle().CouleurTraceur * 4.f);
		}
		FormeCylindre = LoadObject<UStaticMesh>(nullptr, CHEMIN_CYLINDRE);
		Etincelles = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FPS_Game/Particles/P_AssaultRifle_IH_SparksOnly.P_AssaultRifle_IH_SparksOnly"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Etincelles)
		{
			Etincelles = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FPS_Game/Particles/P_Sparks.P_Sparks"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		FlammeCanon = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FPS_Game/Particles/P_AssaultRifle_MF.P_AssaultRifle_MF"), nullptr, LOAD_NoWarn | LOAD_Quiet);

		Attenuation = NewObject<USoundAttenuation>(this);
		Attenuation->Attenuation.bAttenuate = true;
		Attenuation->Attenuation.bSpatialize = true;
		Attenuation->Attenuation.AttenuationShapeExtents = FVector(300.f);
		Attenuation->Attenuation.FalloffDistance = 5000.f;
	}
	if (HasAuthority())
	{
		// petit decalage pour que toutes les tourelles ne cherchent pas en meme temps
		ProchainChoix = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.f, 0.4f);
	}
	UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s prete (%s)"), *GetName(), *Resume());
}

void ATourelleChelles::AppliquerModele()
{
	const float E = FMath::Max(0.05f, Echelle);
	const FRotator Sens(0.f, RotationModele, 0.f);
	const bool bSquelette = ModeleSquelette != nullptr;
	const bool bAucunModele = !bSquelette && !ModelePied && !ModeleCanon;

	Pivot->SetRelativeLocation(PivotCanon * E);
	Eclair->SetRelativeLocation(BoutDuCanonLocal * E);

	Squelette->SetSkeletalMesh(ModeleSquelette);
	Squelette->SetVisibility(bSquelette);
	Squelette->SetRelativeRotation(Sens);
	Squelette->SetRelativeScale3D(FVector(E));

	if (bAucunModele)
	{
		// Pas encore de modele : une tourelle en formes simples (pied + canon) pour que ca marche quand meme
		ConstruireFormesSimples();
		return;
	}
	Pied->SetStaticMesh(bSquelette ? nullptr : ModelePied);
	Pied->SetRelativeLocation(FVector::ZeroVector);
	Pied->SetRelativeRotation(Sens);
	Pied->SetRelativeScale3D(FVector(E));

	Canon->SetStaticMesh(bSquelette ? nullptr : ModeleCanon);
	// Le canon est dessine comme s'il etait pose a l'origine de la tourelle : on compense la hauteur du pivot
	Canon->SetRelativeLocation(-PivotCanon * E);
	Canon->SetRelativeRotation(Sens);
	Canon->SetRelativeScale3D(FVector(E));
	// Enleve les couleurs des formes simples (si elles avaient ete posees avant) : couleurs d'origine du modele
	for (UStaticMeshComponent* Morceau : { Pied, Canon })
	{
		for (int32 i = 0; i < Morceau->GetNumOverrideMaterials(); ++i)
		{
			UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(Morceau->OverrideMaterials[i]);
			if (Dyn && Dyn->GetOuter() == this)
			{
				Morceau->SetMaterial(i, nullptr);
			}
		}
	}
}

void ATourelleChelles::ConstruireFormesSimples()
{
	UStaticMesh* Cylindre = LoadObject<UStaticMesh>(nullptr, CHEMIN_CYLINDRE);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CHEMIN_CUBE);
	const float E = FMath::Max(0.05f, Echelle);
	const float Haut = PivotCanon.Z;

	// Pied : colonne de la hauteur du pivot (les formes de base font 100 cm et sont centrees)
	Pied->SetStaticMesh(Cylindre);
	Pied->SetRelativeLocation(FVector(0.f, 0.f, Haut * 0.5f * E));
	Pied->SetRelativeRotation(FRotator::ZeroRotator);
	Pied->SetRelativeScale3D(FVector(0.5f, 0.5f, Haut / 100.f) * E);

	// Canon : une boite allongee vers l'avant, posee sur le pivot
	Canon->SetStaticMesh(Cube);
	Canon->SetRelativeLocation(FVector(BoutDuCanonLocal.X * 0.45f, 0.f, 0.f) * E);
	Canon->SetRelativeRotation(FRotator::ZeroRotator);
	Canon->SetRelativeScale3D(FVector(FMath::Max(0.4f, BoutDuCanonLocal.X / 100.f), 0.35f, 0.35f) * E);

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, CHEMIN_MATIERE))
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			UMaterialInstanceDynamic* MatGris = UMaterialInstanceDynamic::Create(Base, this);
			MatGris->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.05f, 0.05f, 0.06f));
			UMaterialInstanceDynamic* Rouge = UMaterialInstanceDynamic::Create(Base, this);
			Rouge->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.03f, 0.02f));
			Pied->SetMaterial(0, MatGris);
			Canon->SetMaterial(0, Rouge);
		}
	}
}

FString ATourelleChelles::Resume() const
{
	return FString::Printf(TEXT("%s : modele %s, portee %.0f m, cible %s, tireur %s"), *GetName(),
		ModeleSquelette ? *ModeleSquelette->GetName() : (ModeleCanon ? *ModeleCanon->GetName() : TEXT("formes simples")),
		Portee() / 100.f, *GetNameSafe(Cible), *GetNameSafe(Tireur.Get()));
}

float ATourelleChelles::Portee() const
{
	return (PorteeMetres > 0.f ? PorteeMetres : ReglagesTourelle().PorteeMetres) * 100.f;
}

FVector ATourelleChelles::BoutDuCanon() const
{
	return Pivot->GetComponentTransform().TransformPositionNoScale(BoutDuCanonLocal * FMath::Max(0.05f, Echelle));
}

// ---------------------------------------------------------------------- Qui est un ennemi ?

bool ATourelleChelles::EstEnnemi(AActor* Acteur)
{
	ACharacter* Perso = Cast<ACharacter>(Acteur);
	if (!Perso || Perso->IsPlayerControlled())
	{
		return false;
	}
	if (AController* C = Perso->GetController())
	{
		if (C->IsA(APlayerController::StaticClass()))
		{
			return false;
		}
	}
	if (Perso->Tags.Contains(MarqueZombieInvasion) || Perso->Tags.Contains(MarqueZombieInvasionHabille)
		|| Perso->Tags.Contains(MarqueEnnemiAuto) || Perso->Tags.Contains(MarqueEnnemiPose))
	{
		return true;
	}
	UClass* Classe = Perso->GetClass();
	if (const bool* Deja = GClassesEnnemies.Find(Classe))
	{
		return *Deja;
	}
	const FReglagesTourelle& R = ReglagesTourelle();
	bool bEnnemi = false;
	bool bIgnore = false;
	for (UClass* C = Classe; C && C != ACharacter::StaticClass(); C = C->GetSuperClass())
	{
		const FString Nom = C->GetName();
		for (const FString& Morceau : R.ClassesIgnorees)
		{
			if (Nom.Contains(Morceau, ESearchCase::IgnoreCase))
			{
				bIgnore = true;
			}
		}
		if (C->GetPathName().Contains(TEXT("/Blueprints/Bonus/AI/"), ESearchCase::IgnoreCase) || Nom.StartsWith(TEXT("AI_")))
		{
			bEnnemi = true;
		}
		for (const FString& Morceau : R.ClassesEnnemies)
		{
			if (Nom.Contains(Morceau, ESearchCase::IgnoreCase))
			{
				bEnnemi = true;
			}
		}
	}
	const bool bResultat = bEnnemi && !bIgnore;
	GClassesEnnemies.Add(Classe, bResultat);
	UE_LOG(LogTemp, Log, TEXT("[Tourelle] Classe %s : %s"), *Classe->GetName(), bResultat ? TEXT("ENNEMI") : TEXT("ignoree"));
	return bResultat;
}

bool ATourelleChelles::EstVivant(AActor* Acteur)
{
	ACharacter* Perso = Cast<ACharacter>(Acteur);
	if (!IsValid(Perso) || Perso->IsPendingKillPending() || Perso->Tags.Contains(MarqueHorsJeu))
	{
		return false;
	}
	if (Perso->GetMesh() && Perso->GetMesh()->IsSimulatingPhysics())
	{
		return false; // tombe par terre
	}
	if (Perso->GetCapsuleComponent() && Perso->GetCapsuleComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return false;
	}
	const FVariablesEnnemi& V = VariablesDe(Perso->GetClass());
	if (V.Mort && V.Mort->GetPropertyValue_InContainer(Perso))
	{
		return false;
	}
	if (V.Vie)
	{
		const void* Adresse = V.Vie->ContainerPtrToValuePtr<void>(Perso);
		const float Vie = V.Vie->IsFloatingPoint() ? (float)V.Vie->GetFloatingPointPropertyValue(Adresse) : (float)V.Vie->GetSignedIntPropertyValue(Adresse);
		if (Vie <= 0.f)
		{
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------- Chaque image

void ATourelleChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;
	if (!ReglagesTourelle().bActif)
	{
		return;
	}
	Viser(DeltaSeconds);
	if (HasAuthority())
	{
		TickServeur(GetWorld()->GetTimeSeconds());
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (AgeEclair < 1.f)
	{
		AgeEclair += DeltaSeconds;
		Eclair->SetIntensity(FMath::Max(0.f, 30000.f * (1.f - AgeEclair / 0.06f)));
	}
	for (int32 i = Traceurs.Num() - 1; i >= 0; --i)
	{
		FTraceur& T = Traceurs[i];
		T.Age += DeltaSeconds;
		UStaticMeshComponent* Forme = T.Forme.Get();
		if (!Forme || T.Age > 0.08f)
		{
			if (Forme)
			{
				Forme->DestroyComponent();
			}
			Traceurs.RemoveAtSwap(i);
		}
	}
}

void ATourelleChelles::Viser(float DeltaSeconds)
{
	float CapVoulu;
	float HausseVoulue = 0.f;
	if (IsValid(Cible))
	{
		const FVector Vers = Cible->GetActorLocation() + FVector(0.f, 0.f, 30.f) - Pivot->GetComponentLocation();
		const FRotator Locale = GetActorTransform().InverseTransformVectorNoScale(Vers).Rotation();
		CapVoulu = Locale.Yaw;
		HausseVoulue = FMath::Clamp(Locale.Pitch, -35.f, 50.f);
	}
	else
	{
		// Pas de cible : elle surveille en tournant doucement
		CapVoulu = 40.f * FMath::Sin(Temps * 0.35f);
	}
	const float Vitesse = ReglagesTourelle().VitesseRotation;
	const FRotator Nouveau = FMath::RInterpTo(FRotator(Hausse, Cap, 0.f), FRotator(HausseVoulue, CapVoulu, 0.f), DeltaSeconds, Vitesse);
	Cap = FRotator::NormalizeAxis(Nouveau.Yaw);
	Hausse = Nouveau.Pitch;

	// Un canon separe peut lever le nez ; un modele d'un seul morceau tourne seulement sur lui-meme
	const bool bLever = bViserEnHauteur && ModelePied && ModeleCanon && !ModeleSquelette;
	const float HausseAppliquee = bLever ? Hausse : 0.f;

	if (ModeleSquelette && Squelette->SkeletalMesh)
	{
		Pivot->SetRelativeRotation(FRotator(0.f, Cap, 0.f));
		if (OsQuiTourne.IsNone() || Squelette->GetBoneIndex(OsQuiTourne) == INDEX_NONE)
		{
			Squelette->SetRelativeRotation(FRotator(0.f, RotationModele + Cap, 0.f));
		}
		else
		{
			if (!bOsMesure)
			{
				RotationOsDepart = Squelette->GetBoneRotationByName(OsQuiTourne, EBoneSpaces::ComponentSpace).Quaternion();
				bOsMesure = true;
			}
			const FQuat Tour = FRotator(0.f, Cap, 0.f).Quaternion();
			Squelette->SetBoneRotationByName(OsQuiTourne, (Tour * RotationOsDepart).Rotator(), EBoneSpaces::ComponentSpace);
		}
		return;
	}
	Pivot->SetRelativeRotation(FRotator(HausseAppliquee, Cap, 0.f));
}

bool ATourelleChelles::PointDeVisee(AActor* Ennemi, FVector& Point, FName& Os) const
{
	ACharacter* Perso = Cast<ACharacter>(Ennemi);
	if (!Perso)
	{
		return false;
	}
	const bool bTete = ReglagesTourelle().bViserTete;
	USkeletalMeshComponent* Mesh = Perso->GetMesh();
	const FName Choix[] = { bTete ? FName(TEXT("head")) : FName(TEXT("spine_03")), FName(TEXT("spine_02")), FName(TEXT("pelvis")) };
	for (const FName& Nom : Choix)
	{
		if (Mesh && Mesh->DoesSocketExist(Nom))
		{
			Point = Mesh->GetSocketLocation(Nom);
			Os = Nom;
			return true;
		}
	}
	const float Demi = Perso->GetCapsuleComponent() ? Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	Point = Perso->GetActorLocation() + FVector(0.f, 0.f, Demi * (bTete ? 0.8f : 0.3f));
	Os = NAME_None;
	return true;
}

bool ATourelleChelles::EstVisible(AActor* Ennemi, const FVector& Point) const
{
	// On regarde depuis l'axe du canon (le bout du canon peut etre dans un mur ou une haie)
	const FVector Depart = Pivot->GetComponentLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VueTourelleChelles), false, this);
	TArray<AActor*> Autres;
	for (TActorIterator<ATourelleChelles> It(GetWorld()); It; ++It)
	{
		Autres.Add(*It); // les tourelles ne se genent pas entre elles
	}
	Params.AddIgnoredActors(Autres);
	FHitResult Impact;
	if (!GetWorld()->LineTraceSingleByChannel(Impact, Depart, Point, ECC_Visibility, Params))
	{
		return true;
	}
	AActor* Touche = Impact.GetActor();
	if (Touche == Ennemi || (Touche && (Touche->GetAttachParentActor() == Ennemi || Touche->GetOwner() == Ennemi)))
	{
		return true;
	}
	if (Touche && EstEnnemi(Touche))
	{
		return true; // un autre ennemi devant : il prendra la balle
	}
	return Impact.Distance >= (Point - Depart).Size() - 50.f;
}

AActor* ATourelleChelles::ChoisirCible() const
{
	const float LaPortee = Portee();
	const FVector Centre = Pivot->GetComponentLocation();
	struct FCandidat
	{
		ACharacter* Perso;
		float Distance;
	};
	TArray<FCandidat> Candidats;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Perso = *It;
		const float Distance = FVector::Dist(Perso->GetActorLocation(), Centre);
		if (Distance > LaPortee || !EstEnnemi(Perso) || !EstVivant(Perso))
		{
			continue;
		}
		Candidats.Add(FCandidat{ Perso, Perso == Cible ? Distance * 0.6f : Distance });
	}
	Candidats.Sort([](const FCandidat& A, const FCandidat& B) { return A.Distance < B.Distance; });
	for (int32 i = 0; i < Candidats.Num() && i < 6; ++i)
	{
		FVector Point;
		FName Os;
		if (PointDeVisee(Candidats[i].Perso, Point, Os) && EstVisible(Candidats[i].Perso, Point))
		{
			return Candidats[i].Perso;
		}
	}
	return nullptr;
}

void ATourelleChelles::TickServeur(float Maintenant)
{
	const FReglagesTourelle& R = ReglagesTourelle();
	if (Maintenant >= ProchainChoix || (Cible && !EstVivant(Cible)))
	{
		ProchainChoix = Maintenant + 0.3f;
		AActor* Nouvelle = ChoisirCible();
		if (Nouvelle != Cible)
		{
			Cible = Nouvelle;
			ProchainTir = FMath::Max(ProchainTir, Maintenant + 0.35f); // le temps de tourner
		}
	}
	const float LesDegats = Degats > 0.f ? Degats : R.Degats;
	const float Cadence = TirsParSeconde > 0.f ? TirsParSeconde : R.TirsParSeconde;
	if (!Cible || Maintenant < ProchainTir || LesDegats <= 0.f)
	{
		return;
	}
	FVector Point;
	FName Os;
	if (!EstVivant(Cible) || !PointDeVisee(Cible, Point, Os) || !EstVisible(Cible, Point)
		|| FVector::Dist(Point, Pivot->GetComponentLocation()) > Portee() + 100.f)
	{
		Cible = nullptr;
		ProchainChoix = 0.f;
		return;
	}
	// On attend que le canon soit a peu pres tourne vers la cible
	const FVector VersCible = GetActorTransform().InverseTransformVectorNoScale(Point - Pivot->GetComponentLocation());
	if (FMath::Abs(FRotator::NormalizeAxis(VersCible.Rotation().Yaw - Cap)) > 15.f)
	{
		return;
	}
	ProchainTir = Maintenant + 1.f / Cadence;

	const FVector Depart = BoutDuCanon();
	const FVector Direction = (Point - Depart).GetSafeNormal();
	ACharacter* Perso = Cast<ACharacter>(Cible);
	FHitResult Coup(Cible, Perso ? (UPrimitiveComponent*)Perso->GetMesh() : nullptr, Point, -Direction);
	Coup.bBlockingHit = true;
	Coup.BoneName = Os;
	Coup.TraceStart = Depart;
	Coup.TraceEnd = Point;
	Coup.Distance = FVector::Dist(Depart, Point);

	// Le tir compte comme celui du joueur qui l'a posee ; tourelle de la carte = joueur le plus proche
	// (certains ennemis FPS_Game se retournent vers celui qui tire : il en faut un)
	AController* QuiTire = Tireur.Get();
	if (!QuiTire)
	{
		float Meilleure = TNumericLimits<float>::Max();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC && PC->GetPawn())
			{
				const float D = FVector::DistSquared(PC->GetPawn()->GetActorLocation(), GetActorLocation());
				if (D < Meilleure)
				{
					Meilleure = D;
					QuiTire = PC;
				}
			}
		}
	}
	UGameplayStatics::ApplyPointDamage(Cible, LesDegats, Direction, Coup, QuiTire, this, UDamageType::StaticClass());
	MulticastTir(Point);
}

void ATourelleChelles::MulticastTir_Implementation(FVector_NetQuantize Fin)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Debut = BoutDuCanon();
	const FVector Trajet = (FVector)Fin - Debut;
	const float Longueur = Trajet.Size();
	if (FormeCylindre && Longueur > 1.f)
	{
		UStaticMeshComponent* Rayon = NewObject<UStaticMeshComponent>(this);
		Rayon->SetStaticMesh(FormeCylindre);
		Rayon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Rayon->SetCastShadow(false);
		Rayon->SetAbsolute(true, true, true);
		Rayon->SetupAttachment(Racine);
		Rayon->SetWorldLocationAndRotation(Debut + Trajet * 0.5f, FRotationMatrix::MakeFromZ(Trajet / Longueur).Rotator());
		Rayon->SetWorldScale3D(FVector(0.025f, 0.025f, Longueur / 100.f));
		if (MatTraceur)
		{
			Rayon->SetMaterial(0, MatTraceur);
		}
		Rayon->RegisterComponent();
		FTraceur Nouveau;
		Nouveau.Forme = Rayon;
		Traceurs.Add(Nouveau);
	}
	Eclair->SetIntensity(30000.f);
	AgeEclair = 0.f;
	if (FlammeCanon)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FlammeCanon, Debut, Trajet.Rotation(), FVector(1.f), true);
	}
	if (Etincelles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Etincelles, (FVector)Fin, (-Trajet).Rotation(), FVector(0.6f), true);
	}
	JouerSon(0, ReglagesTourelle().VolumeTir);
}

void ATourelleChelles::JouerSon(int32 Genre, float Volume)
{
	if (GetNetMode() == NM_DedicatedServer || Volume <= 0.f || !GetWorld())
	{
		return;
	}
	USonTourelleChelles* Son = NewObject<USonTourelleChelles>(this);
	Son->Genre = Genre;
	UAudioComponent* Lecteur = UGameplayStatics::SpawnSoundAtLocation(this, Son, BoutDuCanon(), FRotator::ZeroRotator, Volume,
		FMath::FRandRange(0.93f, 1.07f), 0.f, Attenuation, nullptr, true);
	if (!Lecteur)
	{
		return;
	}
	TWeakObjectPtr<UAudioComponent> Faible(Lecteur);
	FTimerHandle Minuteur;
	GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
	{
		if (Faible.IsValid())
		{
			Faible->Stop();
		}
	}), 0.3f, false);
}
