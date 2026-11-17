// CHELLES 3D - Drone compagnon : un drone par joueur, qui le suit et tire sur les ennemis et zombies proches
#include "DroneChelles.h"

#include <initializer_list>

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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

#define CHEMIN_MATIERE TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")
#define CHEMIN_SPHERE TEXT("/Engine/BasicShapes/Sphere.Sphere")
#define CHEMIN_CUBE TEXT("/Engine/BasicShapes/Cube.Cube")
#define CHEMIN_CYLINDRE TEXT("/Engine/BasicShapes/Cylinder.Cylinder")

namespace
{
	const int32 FrequenceSon = 44100;

	// ------------------------------------------------------------------ Reglages (Drone.txt)

	FReglagesDrone GReglages;
	bool GReglagesLus = false;

	bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	/** "R,G,B" (0 a 255) -> couleur. */
	bool LireCouleur(const FString& Texte, FLinearColor& Couleur)
	{
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(","), true);
		if (Morceaux.Num() < 3)
		{
			return false;
		}
		const uint8 R = (uint8)FMath::Clamp(FCString::Atoi(*Morceaux[0].TrimStartAndEnd()), 0, 255);
		const uint8 G = (uint8)FMath::Clamp(FCString::Atoi(*Morceaux[1].TrimStartAndEnd()), 0, 255);
		const uint8 B = (uint8)FMath::Clamp(FCString::Atoi(*Morceaux[2].TrimStartAndEnd()), 0, 255);
		Couleur = FLinearColor(FColor(R, G, B));
		return true;
	}

	void LireReglages()
	{
		GReglages = FReglagesDrone();
		const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesDrone"), TEXT("Drone.txt"));
		TArray<FString> Lignes;
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Log, TEXT("[Drone] %s absent : reglages par defaut"), *Chemin);
			return;
		}
		bool bListeEnnemisVidee = false;
		bool bListeIgnoresVidee = false;
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
			FReglagesDrone& R = GReglages;
			auto Nombre = [&Valeur]() { return FCString::Atof(*Valeur.Replace(TEXT(","), TEXT("."))); };

			if (Cle.Equals(TEXT("Actif"), ESearchCase::IgnoreCase)) R.bActif = EstOui(Valeur);
			else if (Cle.Equals(TEXT("SortiAuDepart"), ESearchCase::IgnoreCase)) R.bSortiAuDepart = EstOui(Valeur);
			else if (Cle.Equals(TEXT("Touche"), ESearchCase::IgnoreCase))
			{
				const FKey Touche(*Valeur);
				if (Touche.IsValid())
				{
					R.Touche = Touche;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[Drone] Touche inconnue : %s (F10 gardee)"), *Valeur);
				}
			}
			else if (Cle.Equals(TEXT("PorteeMetres"), ESearchCase::IgnoreCase)) R.PorteeMetres = FMath::Clamp(Nombre(), 3.f, 200.f);
			else if (Cle.Equals(TEXT("TirsParSeconde"), ESearchCase::IgnoreCase)) R.TirsParSeconde = FMath::Clamp(Nombre(), 0.2f, 15.f);
			else if (Cle.Equals(TEXT("Degats"), ESearchCase::IgnoreCase)) R.Degats = FMath::Max(0.f, Nombre());
			else if (Cle.Equals(TEXT("ViserTete"), ESearchCase::IgnoreCase)) R.bViserTete = EstOui(Valeur);
			else if (Cle.Equals(TEXT("TirerEnVehicule"), ESearchCase::IgnoreCase)) R.bTirerEnVehicule = EstOui(Valeur);
			else if (Cle.Equals(TEXT("Hauteur"), ESearchCase::IgnoreCase)) R.Hauteur = Nombre();
			else if (Cle.Equals(TEXT("Cote"), ESearchCase::IgnoreCase)) R.Cote = Nombre();
			else if (Cle.Equals(TEXT("Arriere"), ESearchCase::IgnoreCase)) R.Arriere = Nombre();
			else if (Cle.Equals(TEXT("Souplesse"), ESearchCase::IgnoreCase)) R.Souplesse = FMath::Clamp(Nombre(), 0.5f, 30.f);
			else if (Cle.Equals(TEXT("Taille"), ESearchCase::IgnoreCase)) R.Taille = FMath::Clamp(Nombre(), 0.2f, 5.f);
			else if (Cle.Equals(TEXT("CouleurCorps"), ESearchCase::IgnoreCase)) LireCouleur(Valeur, R.CouleurCorps);
			else if (Cle.Equals(TEXT("CouleurBras"), ESearchCase::IgnoreCase)) LireCouleur(Valeur, R.CouleurBras);
			else if (Cle.Equals(TEXT("CouleurLaser"), ESearchCase::IgnoreCase)) LireCouleur(Valeur, R.CouleurLaser);
			else if (Cle.Equals(TEXT("VolumeTir"), ESearchCase::IgnoreCase)) R.VolumeTir = FMath::Clamp(Nombre(), 0.f, 4.f);
			else if (Cle.Equals(TEXT("VolumeMoteur"), ESearchCase::IgnoreCase)) R.VolumeMoteur = FMath::Clamp(Nombre(), 0.f, 4.f);
			else if (Cle.Equals(TEXT("ClasseEnnemie"), ESearchCase::IgnoreCase))
			{
				if (!bListeEnnemisVidee)
				{
					R.ClassesEnnemies.Reset();
					bListeEnnemisVidee = true;
				}
				if (!Valeur.IsEmpty()) R.ClassesEnnemies.AddUnique(Valeur);
			}
			else if (Cle.Equals(TEXT("ClasseIgnoree"), ESearchCase::IgnoreCase))
			{
				if (!bListeIgnoresVidee)
				{
					R.ClassesIgnorees.Reset();
					bListeIgnoresVidee = true;
				}
				if (!Valeur.IsEmpty()) R.ClassesIgnorees.AddUnique(Valeur);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Drone] Reglages lus : actif=%d touche=%s portee=%.0f m, %.1f tirs/s, degats %.0f"),
			GReglages.bActif ? 1 : 0, *GReglages.Touche.ToString(), GReglages.PorteeMetres, GReglages.TirsParSeconde, GReglages.Degats);
	}

	// ------------------------------------------------------------------ Variables des Blueprints (lecture seule)

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

const FReglagesDrone& ReglagesDrone()
{
	if (!GReglagesLus)
	{
		LireReglages();
		GReglagesLus = true;
	}
	return GReglages;
}

void RechargerReglagesDrone()
{
	LireReglages();
	GReglagesLus = true;
	GClassesEnnemies.Reset();
	GVariables.Reset();
}

// ====================================================================== Sons

USonDroneChelles::USonDroneChelles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(FrequenceSon);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Default;
}

int32 USonDroneChelles::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Sortie = reinterpret_cast<int16*>(OutAudio.GetData());
	const float DeuxPi = 2.f * PI;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = (float)(Echantillon % ((int64)FrequenceSon * 3600)) / (float)FrequenceSon;
		++Echantillon;
		float Valeur = 0.f;

		switch (Genre)
		{
		case 0:
		{
			// Helices : bourdonnement grave qui "bat" + souffle
			Phase += DeuxPi * 165.f / (float)FrequenceSon;
			Phase2 += DeuxPi * 331.f / (float)FrequenceSon;
			if (Phase > DeuxPi) Phase -= DeuxPi;
			if (Phase2 > DeuxPi) Phase2 -= DeuxPi;
			Graine = Graine * 1664525u + 1013904223u;
			const float Blanc = ((Graine >> 9) & 0xFFFF) / 32768.f - 1.f;
			Bruit += 0.08f * (Blanc - Bruit);
			const float Battement = 0.7f + 0.3f * FMath::Sin(DeuxPi * 26.f * T);
			const float Dent = FMath::Clamp(FMath::Sin(Phase) * 1.6f, -1.f, 1.f);
			Valeur = (0.30f * Dent + 0.12f * FMath::Sin(Phase2)) * Battement + 0.35f * Bruit;
			Valeur *= 0.5f * FMath::Clamp(T / 0.4f, 0.f, 1.f);
			break;
		}
		case 1:
		{
			// "Piou" : son aigu qui descend tres vite
			const float Duree = 0.2f;
			if (T < Duree)
			{
				const float Hauteur = 2200.f * FMath::Exp(-T * 16.f) + 320.f;
				Phase += DeuxPi * Hauteur / (float)FrequenceSon;
				if (Phase > DeuxPi) Phase -= DeuxPi;
				const float Carre = FMath::Clamp(FMath::Sin(Phase) * 2.5f, -1.f, 1.f);
				const float Enveloppe = FMath::Exp(-T * 13.f) * FMath::Clamp(T / 0.004f, 0.f, 1.f);
				Valeur = 0.55f * Carre * Enveloppe;
			}
			break;
		}
		case 2:
		case 3:
		{
			// Bip-bip : monte (drone sorti) ou descend (drone range)
			const bool bPremier = T < 0.09f;
			const float Hauteur = (Genre == 2) == bPremier ? 880.f : 1320.f;
			if (T < 0.19f && (T < 0.08f || T > 0.1f))
			{
				Phase += DeuxPi * Hauteur / (float)FrequenceSon;
				if (Phase > DeuxPi) Phase -= DeuxPi;
				Valeur = 0.35f * FMath::Sin(Phase);
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

// ====================================================================== Drone

ADroneChelles::ADroneChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // apres le deplacement du joueur : le drone le suit sans a-coups
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false); // chaque PC calcule la place du drone a cote de son joueur
	NetUpdateFrequency = 10.f;
	SetCanBeDamaged(false);

	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;
}

void ADroneChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADroneChelles, Maitre);
	DOREPLIFETIME(ADroneChelles, bSorti);
	DOREPLIFETIME(ADroneChelles, Cible);
}

bool ADroneChelles::EstServeur() const
{
	return GetLocalRole() == ROLE_Authority;
}

void ADroneChelles::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		Construire();
	}
	AppliquerVisibilite();
}

void ADroneChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Moteur)
	{
		Moteur->Stop();
	}
	Super::EndPlay(Raison);
}

UStaticMeshComponent* ADroneChelles::AjouterForme(USceneComponent* Parent, UStaticMesh* Forme, const FVector& Place, const FRotator& Rotation,
	const FVector& Echelle, const FLinearColor& Couleur, bool bOmbre)
{
	if (!Forme || !Parent)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetStaticMesh(Forme);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(bOmbre);
	C->SetupAttachment(Parent);
	C->SetRelativeLocation(Place);
	C->SetRelativeRotation(Rotation);
	C->SetRelativeScale3D(Echelle);
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, CHEMIN_MATIERE))
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, this);
		Mat->SetVectorParameterValue(TEXT("Color"), Couleur);
		C->SetMaterial(0, Mat);
	}
	C->RegisterComponent();
	return C;
}

void ADroneChelles::Construire()
{
	if (bConstruit)
	{
		return;
	}
	bConstruit = true;
	const FReglagesDrone& R = ReglagesDrone();

	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, CHEMIN_SPHERE);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CHEMIN_CUBE);
	FormeCylindre = LoadObject<UStaticMesh>(nullptr, CHEMIN_CYLINDRE);

	Corps = NewObject<USceneComponent>(this, TEXT("Corps"));
	Corps->SetupAttachment(Racine);
	Corps->SetRelativeScale3D(FVector(R.Taille));
	Corps->RegisterComponent();

	const FLinearColor Gris(0.12f, 0.12f, 0.13f);
	const FLinearColor Noir(0.02f, 0.02f, 0.02f);

	// Coque : soucoupe aplatie + dome de couleur
	AjouterForme(Corps, Sphere, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.42f, 0.34f, 0.15f), R.CouleurCorps, true);
	AjouterForme(Corps, Sphere, FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator, FVector(0.22f, 0.2f, 0.11f), R.CouleurBras);
	// Bras en X
	AjouterForme(Corps, Cube, FVector(0.f, 0.f, 2.f), FRotator(0.f, 45.f, 0.f), FVector(0.68f, 0.045f, 0.035f), Gris, true);
	AjouterForme(Corps, Cube, FVector(0.f, 0.f, 2.f), FRotator(0.f, -45.f, 0.f), FVector(0.68f, 0.045f, 0.035f), Gris, true);

	// 4 moteurs avec leurs helices + feux de position (rouge a gauche, vert a droite)
	const float Bout = 24.f;
	const FVector Coins[4] = { FVector(Bout, Bout, 3.f), FVector(Bout, -Bout, 3.f), FVector(-Bout, Bout, 3.f), FVector(-Bout, -Bout, 3.f) };
	for (int32 i = 0; i < 4; ++i)
	{
		AjouterForme(Corps, FormeCylindre, Coins[i], FRotator::ZeroRotator, FVector(0.07f, 0.07f, 0.08f), R.CouleurBras);
		USceneComponent* Moyeu = NewObject<USceneComponent>(this);
		Moyeu->SetupAttachment(Corps);
		Moyeu->SetRelativeLocation(Coins[i] + FVector(0.f, 0.f, 6.f));
		Moyeu->RegisterComponent();
		if (UStaticMeshComponent* Pale = AjouterForme(Moyeu, Cube, FVector::ZeroVector, FRotator(0.f, 30.f * i, 0.f), FVector(0.4f, 0.035f, 0.006f), Noir))
		{
			Helices.Add(Pale);
		}
		AjouterForme(Corps, Sphere, Coins[i] * 1.18f - FVector(0.f, 0.f, 3.f), FRotator::ZeroRotator, FVector(0.035f),
			Coins[i].Y > 0.f ? FLinearColor(0.f, 1.f, 0.1f) : FLinearColor(1.f, 0.f, 0.f));
	}

	// Tourelle sous le drone : elle tourne vers la cible
	Tourelle = NewObject<USceneComponent>(this, TEXT("Tourelle"));
	Tourelle->SetupAttachment(Corps);
	Tourelle->SetRelativeLocation(FVector(4.f, 0.f, -8.f));
	Tourelle->RegisterComponent();
	AjouterForme(Tourelle, Sphere, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.11f), Gris);
	AjouterForme(Tourelle, FormeCylindre, FVector(11.f, 0.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(0.045f, 0.045f, 0.2f), Noir);
	AjouterForme(Tourelle, Sphere, FVector(21.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(0.04f), R.CouleurLaser);

	// "Oeil" lumineux et eclair au bout du canon
	Oeil = NewObject<UPointLightComponent>(this, TEXT("Oeil"));
	Oeil->SetupAttachment(Tourelle);
	Oeil->SetRelativeLocation(FVector(22.f, 0.f, 0.f));
	Oeil->SetLightColor(R.CouleurLaser);
	Oeil->SetIntensity(1200.f);
	Oeil->SetAttenuationRadius(160.f);
	Oeil->SetCastShadows(false);
	Oeil->RegisterComponent();

	Eclair = NewObject<UPointLightComponent>(this, TEXT("Eclair"));
	Eclair->SetupAttachment(Tourelle);
	Eclair->SetRelativeLocation(FVector(26.f, 0.f, 0.f));
	Eclair->SetLightColor(R.CouleurLaser);
	Eclair->SetIntensity(0.f);
	Eclair->SetAttenuationRadius(450.f);
	Eclair->SetCastShadows(false);
	Eclair->RegisterComponent();

	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, CHEMIN_MATIERE))
	{
		MatLaser = UMaterialInstanceDynamic::Create(Base, this);
		MatLaser->SetVectorParameterValue(TEXT("Color"), R.CouleurLaser);
	}
	Etincelles = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FPS_Game/Particles/P_AssaultRifle_IH_SparksOnly.P_AssaultRifle_IH_SparksOnly"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Etincelles)
	{
		Etincelles = LoadObject<UParticleSystem>(nullptr, TEXT("/Game/FPS_Game/Particles/P_Sparks.P_Sparks"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	// Sons : on entend le drone pres de lui, pas dans toute la ville
	Attenuation = NewObject<USoundAttenuation>(this);
	Attenuation->Attenuation.bAttenuate = true;
	Attenuation->Attenuation.bSpatialize = true;
	Attenuation->Attenuation.AttenuationShapeExtents = FVector(250.f);
	Attenuation->Attenuation.FalloffDistance = 3500.f;

	if (R.VolumeMoteur > 0.f)
	{
		SonMoteur = NewObject<USonDroneChelles>(this);
		SonMoteur->Genre = 0;
		Moteur = NewObject<UAudioComponent>(this, TEXT("Moteur"));
		Moteur->bAutoActivate = false;
		Moteur->SetupAttachment(Racine);
		Moteur->SetSound(SonMoteur);
		Moteur->SetVolumeMultiplier(R.VolumeMoteur);
		Moteur->AttenuationSettings = Attenuation;
		Moteur->RegisterComponent();
	}
}

void ADroneChelles::OnRep_Sorti()
{
	bPlace = false;
	AppliquerVisibilite();
	JouerSon(bSorti ? 2 : 3, 0.5f);
}

void ADroneChelles::ServeurBasculer_Implementation()
{
	Basculer();
}

void ADroneChelles::Basculer()
{
	if (!EstServeur())
	{
		ServeurBasculer();
		return;
	}
	bSorti = !bSorti;
	Cible = nullptr;
	UE_LOG(LogTemp, Log, TEXT("[Drone] %s : drone %s"), *GetNameSafe(GetOwner()), bSorti ? TEXT("sorti") : TEXT("range"));
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRep_Sorti(); // sur le serveur, OnRep n'est pas appele tout seul
	}
}

void ADroneChelles::AppliquerVisibilite()
{
	const bool bVisible = bSorti && Maitre != nullptr;
	SetActorHiddenInGame(!bVisible);
	if (Moteur)
	{
		if (bVisible && !Moteur->IsPlaying())
		{
			Moteur->Play();
		}
		else if (!bVisible && Moteur->IsPlaying())
		{
			Moteur->Stop();
		}
	}
}

void ADroneChelles::MulticastSon_Implementation(uint8 Genre)
{
	JouerSon(Genre, Genre == 1 ? ReglagesDrone().VolumeTir : 0.5f);
}

void ADroneChelles::JouerSon(int32 Genre, float Volume)
{
	if (GetNetMode() == NM_DedicatedServer || Volume <= 0.f || !GetWorld())
	{
		return;
	}
	USonDroneChelles* Son = NewObject<USonDroneChelles>(this);
	Son->Genre = Genre;
	UAudioComponent* Lecteur = UGameplayStatics::SpawnSoundAtLocation(this, Son, GetActorLocation(), FRotator::ZeroRotator, Volume,
		Genre == 1 ? FMath::FRandRange(0.92f, 1.08f) : 1.f, 0.f, Attenuation, nullptr, true);
	if (!Lecteur)
	{
		return;
	}
	// Un son fabrique ne s'arrete pas tout seul : on le coupe a la fin
	TWeakObjectPtr<UAudioComponent> Faible(Lecteur);
	FTimerHandle Minuteur;
	GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
	{
		if (Faible.IsValid())
		{
			Faible->Stop();
		}
	}), 0.35f, false);
}

void ADroneChelles::MulticastTir_Implementation(FVector_NetQuantize Depart, FVector_NetQuantize Fin)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!bConstruit)
	{
		Construire();
	}
	// Le rayon part du canon tel qu'on le voit sur ce PC
	const FVector Debut = Tourelle ? Tourelle->GetComponentTransform().TransformPosition(FVector(24.f, 0.f, 0.f)) : (FVector)Depart;
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
		Rayon->SetWorldScale3D(FVector(0.03f, 0.03f, Longueur / 100.f));
		if (MatLaser)
		{
			Rayon->SetMaterial(0, MatLaser);
		}
		Rayon->RegisterComponent();
		FRayon Nouveau;
		Nouveau.Forme = Rayon;
		Rayons.Add(Nouveau);
	}
	if (Eclair)
	{
		Eclair->SetIntensity(25000.f);
		AgeEclair = 0.f;
	}
	if (Etincelles)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Etincelles, (FVector)Fin, (-Trajet).Rotation(), FVector(0.6f), true);
	}
	JouerSon(1, ReglagesDrone().VolumeTir);
}

// ---------------------------------------------------------------------- Qui est un ennemi ?

bool ADroneChelles::EstEnnemi(AActor* Acteur)
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
	const FReglagesDrone& R = ReglagesDrone();
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
		// Tout ce qui vient du dossier des ennemis FPS_Game (Charger, Other, Patrol, Soldier, Zombie)
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
	UE_LOG(LogTemp, Log, TEXT("[Drone] Classe %s : %s"), *Classe->GetName(), bResultat ? TEXT("ENNEMI") : TEXT("ignoree"));
	return bResultat;
}

bool ADroneChelles::EstVivant(AActor* Acteur)
{
	ACharacter* Perso = Cast<ACharacter>(Acteur);
	if (!IsValid(Perso) || Perso->IsPendingKillPending() || Perso->Tags.Contains(MarqueHorsJeu))
	{
		return false;
	}
	if (Perso->GetMesh() && Perso->GetMesh()->IsSimulatingPhysics())
	{
		return false; // tombe par terre (ragdoll)
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

bool ADroneChelles::MaitreDisponible() const
{
	if (!IsValid(Maitre) || Maitre->Tags.Contains(MarqueHorsJeu))
	{
		return false;
	}
	if (ACharacter* Perso = Cast<ACharacter>(Maitre))
	{
		const FVariablesEnnemi& V = VariablesDe(Perso->GetClass());
		if (V.Mort && V.Mort->GetPropertyValue_InContainer(Perso))
		{
			return false;
		}
	}
	else if (!ReglagesDrone().bTirerEnVehicule)
	{
		return false; // le joueur conduit
	}
	return true;
}

void ADroneChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;

	if (IsHidden() == (bSorti && Maitre != nullptr))
	{
		AppliquerVisibilite(); // le joueur vient d'arriver / de changer de personnage
	}
	TickTouches();
	if (!Maitre || !bSorti)
	{
		bPlace = false;
		if (EstServeur())
		{
			Cible = nullptr;
		}
		return;
	}
	Suivre(DeltaSeconds, !bPlace);
	bPlace = true;
	if (EstServeur())
	{
		TickServeur(GetWorld()->GetTimeSeconds());
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		Animer(DeltaSeconds);
	}
}

void ADroneChelles::TickTouches()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(ReglagesDrone().Touche))
	{
		Basculer();
	}
}

void ADroneChelles::Suivre(float DeltaSeconds, bool bImmediat)
{
	const FReglagesDrone& R = ReglagesDrone();
	const FRotator Direction(0.f, Maitre->GetActorRotation().Yaw, 0.f);
	const FVector Flottement(FMath::Sin(Temps * 1.3f) * 10.f, FMath::Cos(Temps * 0.9f) * 8.f, FMath::Sin(Temps * 2.1f) * 9.f);
	const FVector Voulu = Maitre->GetActorLocation() + Direction.RotateVector(FVector(-R.Arriere, R.Cote, R.Hauteur)) + Flottement;

	const FVector Avant = GetActorLocation();
	FVector Nouveau = Voulu;
	if (!bImmediat && FVector::DistSquared(Avant, Voulu) < FMath::Square(3000.f))
	{
		Nouveau = FMath::VInterpTo(Avant, Voulu, DeltaSeconds, R.Souplesse);
	}
	Vitesse = DeltaSeconds > 0.f && !bImmediat ? (Nouveau - Avant) / DeltaSeconds : FVector::ZeroVector;

	// Il regarde sa cible, sinon dans la meme direction que son joueur
	float Cap = Direction.Yaw;
	if (IsValid(Cible))
	{
		Cap = (Cible->GetActorLocation() - Nouveau).Rotation().Yaw;
	}
	const FRotator Actuelle = GetActorRotation();
	const float NouveauCap = bImmediat ? Cap : FMath::RInterpTo(FRotator(0.f, Actuelle.Yaw, 0.f), FRotator(0.f, Cap, 0.f), DeltaSeconds, 5.f).Yaw;

	// Il penche dans le sens ou il avance, comme un vrai drone
	const FVector Locale = FRotator(0.f, NouveauCap, 0.f).UnrotateVector(Vitesse);
	const float Tangage = FMath::Clamp(-Locale.X * 0.025f, -22.f, 22.f);
	const float Roulis = FMath::Clamp(Locale.Y * 0.025f, -22.f, 22.f);
	const FRotator Penche = bImmediat ? FRotator(0.f, NouveauCap, 0.f)
		: FMath::RInterpTo(Actuelle, FRotator(Tangage, NouveauCap, Roulis), DeltaSeconds, 6.f);

	SetActorLocationAndRotation(Nouveau, FRotator(Penche.Pitch, NouveauCap, Penche.Roll));
}

void ADroneChelles::Animer(float DeltaSeconds)
{
	if (!bConstruit)
	{
		return;
	}
	// Helices
	RotationHelices = FMath::Fmod(RotationHelices + DeltaSeconds * 2400.f, 360.f);
	for (int32 i = 0; i < Helices.Num(); ++i)
	{
		if (Helices[i])
		{
			const float Sens = (i == 0 || i == 3) ? 1.f : -1.f;
			Helices[i]->SetRelativeRotation(FRotator(0.f, Sens * RotationHelices + 30.f * i, 0.f));
		}
	}
	// Canon vers la cible
	if (Tourelle)
	{
		FRotator Visee = GetActorRotation();
		Visee.Roll = 0.f;
		if (IsValid(Cible))
		{
			Visee = (Cible->GetActorLocation() + FVector(0.f, 0.f, 25.f) - Tourelle->GetComponentLocation()).Rotation();
		}
		else
		{
			Visee.Pitch = -10.f;
		}
		Tourelle->SetWorldRotation(FMath::RInterpTo(Tourelle->GetComponentRotation(), Visee, DeltaSeconds, 12.f));
	}
	// Oeil : plus vif quand il a une cible
	if (Oeil)
	{
		const float Pouls = IsValid(Cible) ? 2600.f + 1400.f * FMath::Sin(Temps * 14.f) : 1000.f + 250.f * FMath::Sin(Temps * 3.f);
		Oeil->SetIntensity(Pouls);
	}
	if (Eclair && AgeEclair < 1.f)
	{
		AgeEclair += DeltaSeconds;
		Eclair->SetIntensity(FMath::Max(0.f, 25000.f * (1.f - AgeEclair / 0.08f)));
	}
	// Rayons laser : ils s'amincissent puis disparaissent
	for (int32 i = Rayons.Num() - 1; i >= 0; --i)
	{
		FRayon& Rayon = Rayons[i];
		Rayon.Age += DeltaSeconds;
		UStaticMeshComponent* Forme = Rayon.Forme.Get();
		if (!Forme || Rayon.Age > 0.12f)
		{
			if (Forme)
			{
				Forme->DestroyComponent();
			}
			Rayons.RemoveAtSwap(i);
			continue;
		}
		const float Epaisseur = 0.03f * (1.f - Rayon.Age / 0.12f) + 0.004f;
		const FVector Echelle = Forme->GetComponentScale();
		Forme->SetWorldScale3D(FVector(Epaisseur, Epaisseur, Echelle.Z));
	}
}

FVector ADroneChelles::BoutDuCanon() const
{
	if (Tourelle)
	{
		return Tourelle->GetComponentTransform().TransformPosition(FVector(24.f, 0.f, 0.f));
	}
	return GetActorLocation() + GetActorForwardVector() * 25.f * ReglagesDrone().Taille - FVector(0.f, 0.f, 8.f);
}

bool ADroneChelles::PointDeVisee(AActor* Ennemi, FVector& Point, FName& Os) const
{
	ACharacter* Perso = Cast<ACharacter>(Ennemi);
	if (!Perso)
	{
		return false;
	}
	const bool bTete = ReglagesDrone().bViserTete;
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
	// Pas d'os connu : le milieu du torse d'apres la capsule
	const float Demi = Perso->GetCapsuleComponent() ? Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	Point = Perso->GetActorLocation() + FVector(0.f, 0.f, Demi * (bTete ? 0.8f : 0.3f));
	Os = NAME_None;
	return true;
}

bool ADroneChelles::EstVisible(AActor* Ennemi, const FVector& Point) const
{
	const FVector Depart = BoutDuCanon();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VueDroneChelles), false, this);
	if (Maitre)
	{
		Params.AddIgnoredActor(Maitre);
		TArray<AActor*> Portes;
		Maitre->GetAttachedActors(Portes);
		Params.AddIgnoredActors(Portes);
	}
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
	// Un autre ennemi devant : on peut tirer quand meme (il sera vise au prochain choix)
	if (Touche && Touche != Maitre && EstEnnemi(Touche))
	{
		return true;
	}
	return Impact.Distance >= (Point - Depart).Size() - 50.f;
}

AActor* ADroneChelles::ChoisirCible() const
{
	const FReglagesDrone& R = ReglagesDrone();
	const float Portee = R.PorteeMetres * 100.f;
	const FVector Centre = Maitre->GetActorLocation();

	struct FCandidat
	{
		ACharacter* Perso;
		float Distance;
	};
	TArray<FCandidat> Candidats;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Perso = *It;
		if (Perso == Maitre)
		{
			continue;
		}
		const float Distance = FVector::Dist(Perso->GetActorLocation(), Centre);
		if (Distance > Portee || !EstEnnemi(Perso) || !EstVivant(Perso))
		{
			continue;
		}
		// On garde la cible actuelle tant qu'on peut (moins de va-et-vient du canon)
		Candidats.Add(FCandidat{ Perso, Perso == Cible ? Distance * 0.6f : Distance });
	}
	Candidats.Sort([](const FCandidat& A, const FCandidat& B) { return A.Distance < B.Distance; });

	// Le plus proche que le drone voit vraiment (pas a travers les murs)
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

void ADroneChelles::TickServeur(float Maintenant)
{
	const FReglagesDrone& R = ReglagesDrone();
	if (!MaitreDisponible())
	{
		Cible = nullptr;
		return;
	}
	if (Maintenant >= ProchainChoix || (Cible && !EstVivant(Cible)))
	{
		ProchainChoix = Maintenant + 0.3f;
		AActor* Nouvelle = ChoisirCible();
		if (Nouvelle != Cible)
		{
			Cible = Nouvelle;
			// petit temps de visee avant le premier tir sur une nouvelle cible
			ProchainTir = FMath::Max(ProchainTir, Maintenant + 0.25f);
		}
	}
	if (!Cible || Maintenant < ProchainTir || R.Degats <= 0.f)
	{
		return;
	}
	FVector Point;
	FName Os;
	if (!EstVivant(Cible) || !PointDeVisee(Cible, Point, Os) || !EstVisible(Cible, Point)
		|| FVector::Dist(Point, Maitre->GetActorLocation()) > R.PorteeMetres * 100.f + 100.f)
	{
		Cible = nullptr;
		ProchainChoix = 0.f;
		return;
	}
	ProchainTir = Maintenant + 1.f / R.TirsParSeconde;

	const FVector Depart = BoutDuCanon();
	const FVector Direction = (Point - Depart).GetSafeNormal();
	ACharacter* Perso = Cast<ACharacter>(Cible);
	FHitResult Coup(Cible, Perso ? (UPrimitiveComponent*)Perso->GetMesh() : nullptr, Point, -Direction);
	Coup.bBlockingHit = true;
	Coup.BoneName = Os;
	Coup.TraceStart = Depart;
	Coup.TraceEnd = Point;
	Coup.Distance = FVector::Dist(Depart, Point);

	// Le tir compte comme celui du joueur (points, ennemis qui le prennent en chasse...)
	AController* Tireur = Maitre->GetController();
	if (!Tireur)
	{
		Tireur = Cast<AController>(GetOwner());
	}
	UGameplayStatics::ApplyPointDamage(Cible, R.Degats, Direction, Coup, Tireur, this, UDamageType::StaticClass());
	MulticastTir(Depart, Point);
}

FString ADroneChelles::Resume() const
{
	return FString::Printf(TEXT("Drone de %s : %s, suit %s, cible %s"), *GetNameSafe(GetOwner()), bSorti ? TEXT("sorti") : TEXT("range"),
		*GetNameSafe(Maitre), *GetNameSafe(Cible));
}

// ====================================================================== Gestionnaire (serveur)

ADronesChelles::ADronesChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

ADroneChelles* ADronesChelles::DroneDe(APlayerController* Joueur) const
{
	const TWeakObjectPtr<ADroneChelles>* Trouve = Drones.Find(Joueur);
	return Trouve ? Trouve->Get() : nullptr;
}

void ADronesChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Joueur = It->Get();
		if (!Joueur)
		{
			continue;
		}
		ADroneChelles* Drone = DroneDe(Joueur);
		APawn* Pion = Joueur->GetPawn();
		if (!Drone)
		{
			if (!Pion)
			{
				continue; // pas encore de personnage (menu, chargement)
			}
			FActorSpawnParameters Params;
			Params.Owner = Joueur;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Drone = Monde->SpawnActor<ADroneChelles>(ADroneChelles::StaticClass(), FTransform(Pion->GetActorLocation()), Params);
			if (!Drone)
			{
				continue;
			}
			Drone->bSorti = ReglagesDrone().bSortiAuDepart;
			Drones.Add(Joueur, Drone);
			UE_LOG(LogTemp, Log, TEXT("[Drone] Drone pose pour %s"), *Joueur->GetName());
		}
		if (Drone->Maitre != Pion)
		{
			Drone->Maitre = Pion;
			Drone->Cible = nullptr;
		}
	}
	// Joueurs partis : leur drone s'en va aussi
	for (auto It = Drones.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			if (It->Value.IsValid())
			{
				It->Value->Destroy();
			}
			It.RemoveCurrent();
		}
	}
}

void ADronesChelles::Resume() const
{
	UE_LOG(LogTemp, Log, TEXT("[Drone] %d drone(s)"), Drones.Num());
	for (const auto& Paire : Drones)
	{
		if (Paire.Value.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("[Drone]   %s"), *Paire.Value->Resume());
		}
	}
}
