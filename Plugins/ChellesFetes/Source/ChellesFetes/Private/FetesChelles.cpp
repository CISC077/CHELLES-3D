// CHELLES 3D - Evenements du calendrier (UE 4.27)
#include "FetesChelles.h"

#include "Animation/AnimationAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace
{
	const TCHAR* CheminLumiere = TEXT("/Game/Chelles/Fetes/M_Fetes_Lumiere.M_Fetes_Lumiere");
	const TCHAR* CheminFantome = TEXT("/Game/Chelles/Fetes/M_Fetes_Fantome.M_Fetes_Fantome");
	const TCHAR* CheminBase = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	FString Dossier()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesFetes"));
	}

	void Message(const FString& Texte, bool bEcran = false, const FColor& Teinte = FColor::Orange)
	{
		UE_LOG(LogTemp, Log, TEXT("[Fetes] %s"), *Texte);
		if (bEcran && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f, Teinte, Texte);
		}
	}

	bool Oui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("True"), ESearchCase::IgnoreCase) || V == TEXT("1");
	}

	float Nombre(const FString& V)
	{
		return FCString::Atof(*V.Replace(TEXT(","), TEXT(".")));
	}

	/** "31/10" -> 1031 */
	int32 LireDate(const FString& V, int32 Defaut)
	{
		FString J, M;
		if (!V.Split(TEXT("/"), &J, &M))
		{
			return Defaut;
		}
		const int32 Jour = FCString::Atoi(*J.TrimStartAndEnd());
		const int32 Mois = FCString::Atoi(*M.TrimStartAndEnd());
		if (Jour < 1 || Jour > 31 || Mois < 1 || Mois > 12)
		{
			return Defaut;
		}
		return Mois * 100 + Jour;
	}

	/** "0.03,0.12,0.75" -> couleur */
	bool LireCouleur(const FString& V, FLinearColor& Sortie)
	{
		TArray<FString> P;
		V.ParseIntoArray(P, TEXT(","), true);
		if (P.Num() < 3)
		{
			return false;
		}
		Sortie = FLinearColor(FCString::Atof(*P[0].TrimStartAndEnd()), FCString::Atof(*P[1].TrimStartAndEnd()),
			FCString::Atof(*P[2].TrimStartAndEnd()), 1.f);
		return true;
	}

	TArray<FString> LireListe(const FString& V)
	{
		TArray<FString> Morceaux, Sortie;
		V.ParseIntoArray(Morceaux, TEXT(";"), true);
		for (FString M : Morceaux)
		{
			M.TrimStartAndEndInline();
			if (!M.IsEmpty())
			{
				Sortie.Add(M);
			}
		}
		return Sortie;
	}

	/** "/Game/Dossier/Objet" -> "/Game/Dossier/Objet.Objet" */
	FString CheminComplet(const FString& Chemin)
	{
		if (Chemin.IsEmpty() || Chemin.Contains(TEXT(".")))
		{
			return Chemin;
		}
		return Chemin + TEXT(".") + FPaths::GetBaseFilename(Chemin);
	}

	template <typename T>
	T* Charger(const FString& Chemin)
	{
		if (Chemin.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<T>(nullptr, *CheminComplet(Chemin), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	bool DansPeriode(int32 Jour, int32 Debut, int32 Fin)
	{
		return Debut <= Fin ? (Jour >= Debut && Jour <= Fin) : (Jour >= Debut || Jour <= Fin);
	}

	FString TexteDate(int32 MMJJ)
	{
		return FString::Printf(TEXT("%02d/%02d"), MMJJ % 100, MMJJ / 100);
	}
}

AFetesChelles::AFetesChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

// ---------------------------------------------------------------- Reglages

void AFetesChelles::LireReglages()
{
	Reglages = FReglagesFetes();
	FReglagesFetes& R = Reglages;
	R.CouleursFeux = {
		FLinearColor(0.05f, 0.25f, 1.f), FLinearColor(1.f, 1.f, 1.f), FLinearColor(1.f, 0.05f, 0.05f),
		FLinearColor(1.f, 0.7f, 0.1f), FLinearColor(0.1f, 1.f, 0.25f), FLinearColor(0.75f, 0.15f, 1.f) };
	R.MaillagesDanseurs = { TEXT("/Game/ActorCore_Sample_Motions/Dummy_Female/Dummy_Female") };
	R.Danses = {
		TEXT("/Game/ActorCore_Sample_Motions/Pose_dance_catwalk_Female/Aerobic_Dance_Anim"),
		TEXT("/Game/ActorCore_Sample_Motions/Pose_dance_catwalk_Female/Air_dance_Anim"),
		TEXT("/Game/ActorCore_Sample_Motions/Pose_dance_catwalk_Female/Dance_graceful_Anim") };
	R.Musiques = {
		TEXT("/Game/WAV/WAV_Synthwave_Cyberpunk_Track_01"), TEXT("/Game/WAV/WAV_Synthwave_Cyberpunk_Track_02"),
		TEXT("/Game/WAV/WAV_Synthwave_Cyberpunk_Track_03"), TEXT("/Game/WAV/WAV_Synthwave_Cyberpunk_Track_04"),
		TEXT("/Game/WAV/WAV_Synthwave_Cyberpunk_Track_05") };

	TArray<FString> Lignes;
	const FString Chemin = FPaths::Combine(Dossier(), TEXT("Fetes.txt"));
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		Message(Chemin + TEXT(" introuvable : reglages par defaut."));
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		FString Cle, V;
		if (!Ligne.Split(TEXT("="), &Cle, &V))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		V.TrimStartAndEndInline();

		if (Cle == TEXT("Actif")) { R.bActif = Oui(V); }
		else if (Cle == TEXT("Mode")) { R.Mode = V; }
		else if (Cle == TEXT("Carte")) { R.Carte = V; }
		else if (Cle == TEXT("HalloweenDebut")) { R.HalloweenDebut = LireDate(V, R.HalloweenDebut); }
		else if (Cle == TEXT("HalloweenFin")) { R.HalloweenFin = LireDate(V, R.HalloweenFin); }
		else if (Cle == TEXT("JuilletDebut")) { R.JuilletDebut = LireDate(V, R.JuilletDebut); }
		else if (Cle == TEXT("JuilletFin")) { R.JuilletFin = LireDate(V, R.JuilletFin); }
		else if (Cle == TEXT("MusiqueDebut")) { R.MusiqueDebut = LireDate(V, R.MusiqueDebut); }
		else if (Cle == TEXT("MusiqueFin")) { R.MusiqueFin = LireDate(V, R.MusiqueFin); }
		else if (Cle == TEXT("Citrouilles")) { R.Citrouilles = FMath::Clamp(FCString::Atoi(*V), 0, 300); }
		else if (Cle == TEXT("CitrouillesRayon")) { R.CitrouillesRayon = FMath::Clamp(Nombre(V), 5.f, 3000.f); }
		else if (Cle == TEXT("TailleCitrouille")) { R.TailleCitrouille = FMath::Clamp(Nombre(V), 10.f, 500.f); }
		else if (Cle == TEXT("BougiesMax")) { R.BougiesMax = FMath::Clamp(FCString::Atoi(*V), 0, 100); }
		else if (Cle == TEXT("Fantomes")) { R.Fantomes = FMath::Clamp(FCString::Atoi(*V), 0, 60); }
		else if (Cle == TEXT("TetesCitrouille")) { R.TetesCitrouille = FMath::Clamp(FCString::Atoi(*V), 0, 100); }
		else if (Cle == TEXT("CitrouilleMaillage")) { R.CitrouilleMaillage = V; }
		else if (Cle == TEXT("AmbianceHalloween")) { R.AmbianceHalloween = V; }
		else if (Cle == TEXT("VolumeAmbiance")) { R.VolumeAmbiance = FMath::Clamp(Nombre(V), 0.f, 4.f); }
		else if (Cle == TEXT("LieuFeux")) { R.LieuFeux = V; }
		else if (Cle == TEXT("DistanceFeux")) { R.DistanceFeux = FMath::Clamp(Nombre(V), 10.f, 2000.f); }
		else if (Cle == TEXT("HauteurFeux")) { R.HauteurFeux = FMath::Clamp(Nombre(V), 10.f, 500.f); }
		else if (Cle == TEXT("PauseSpectacle")) { R.PauseSpectacle = FMath::Clamp(Nombre(V), 0.f, 3600.f); }
		else if (Cle == TEXT("DureeSpectacle")) { R.DureeSpectacle = FMath::Clamp(Nombre(V), 10.f, 3600.f); }
		else if (Cle == TEXT("EtincellesParBouquet")) { R.EtincellesParBouquet = FMath::Clamp(FCString::Atoi(*V), 10, 400); }
		else if (Cle == TEXT("TailleEtincelle")) { R.TailleEtincelle = FMath::Clamp(Nombre(V), 0.1f, 10.f); }
		else if (Cle == TEXT("ForceLumiere")) { R.ForceLumiere = FMath::Clamp(Nombre(V), 1.f, 1000.f); }
		else if (Cle == TEXT("VolumeFeux")) { R.VolumeFeux = FMath::Clamp(Nombre(V), 0.f, 4.f); }
		else if (Cle == TEXT("SonExplosion")) { R.SonExplosion = V; }
		else if (Cle == TEXT("SonDepart")) { R.SonDepart = V; }
		else if (Cle == TEXT("CouleursFeux"))
		{
			TArray<FLinearColor> Couleurs;
			for (const FString& C : LireListe(V))
			{
				FLinearColor Lue;
				if (LireCouleur(C, Lue))
				{
					Couleurs.Add(Lue);
				}
			}
			if (Couleurs.Num() > 0)
			{
				R.CouleursFeux = Couleurs;
			}
		}
		else if (Cle == TEXT("Danseurs")) { R.Danseurs = FMath::Clamp(FCString::Atoi(*V), 0, 60); }
		else if (Cle == TEXT("MaillagesDanseurs")) { R.MaillagesDanseurs = LireListe(V); }
		else if (Cle == TEXT("Danses")) { R.Danses = LireListe(V); }
		else if (Cle == TEXT("Musiques")) { R.Musiques = LireListe(V); }
		else if (Cle == TEXT("VolumeMusique")) { R.VolumeMusique = FMath::Clamp(Nombre(V), 0.f, 4.f); }
		else if (Cle == TEXT("PorteeMusique")) { R.PorteeMusique = FMath::Clamp(Nombre(V), 5.f, 1000.f); }
		else if (Cle == TEXT("RotationDanseurs")) { R.RotationDanseurs = Nombre(V); }
	}
}

FString AFetesChelles::NomFete(EFeteChelles Fete)
{
	switch (Fete)
	{
	case EFeteChelles::Halloween: return TEXT("Halloween");
	case EFeteChelles::QuatorzeJuillet: return TEXT("14 juillet");
	case EFeteChelles::FeteMusique: return TEXT("Fete de la musique");
	default: return TEXT("aucune");
	}
}

EFeteChelles AFetesChelles::FeteDuJour() const
{
	const FDateTime Maintenant = FDateTime::Now();
	const int32 Jour = Maintenant.GetMonth() * 100 + Maintenant.GetDay();
	if (DansPeriode(Jour, Reglages.HalloweenDebut, Reglages.HalloweenFin)) { return EFeteChelles::Halloween; }
	if (DansPeriode(Jour, Reglages.JuilletDebut, Reglages.JuilletFin)) { return EFeteChelles::QuatorzeJuillet; }
	if (DansPeriode(Jour, Reglages.MusiqueDebut, Reglages.MusiqueFin)) { return EFeteChelles::FeteMusique; }
	return EFeteChelles::Aucune;
}

EFeteChelles AFetesChelles::FeteVoulue() const
{
	if (ModeForce >= 0)
	{
		return static_cast<EFeteChelles>(ModeForce);
	}
	if (!Reglages.bActif)
	{
		return EFeteChelles::Aucune;
	}
	const FString& M = Reglages.Mode;
	if (M.Equals(TEXT("Halloween"), ESearchCase::IgnoreCase)) { return EFeteChelles::Halloween; }
	if (M.Contains(TEXT("Juillet"), ESearchCase::IgnoreCase)) { return EFeteChelles::QuatorzeJuillet; }
	if (M.Contains(TEXT("Musique"), ESearchCase::IgnoreCase)) { return EFeteChelles::FeteMusique; }
	if (M.Equals(TEXT("Non"), ESearchCase::IgnoreCase)) { return EFeteChelles::Aucune; }
	return FeteDuJour();
}

// ---------------------------------------------------------------- Demarrage / arret

void AFetesChelles::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	const FString NomCarte = GetWorld() ? GetWorld()->GetMapName() : FString();
	if (!Reglages.Carte.IsEmpty() && !Reglages.Carte.Equals(TEXT("Toutes"), ESearchCase::IgnoreCase) && !NomCarte.Contains(Reglages.Carte))
	{
		Message(FString::Printf(TEXT("Carte %s : pas de fete ici (Carte=%s dans Fetes.txt)."), *NomCarte, *Reglages.Carte));
		SetActorTickEnabled(false);
		return;
	}

	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylindre = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	MateriauBase = LoadObject<UMaterialInterface>(nullptr, CheminBase);
	MateriauLumiere = LoadObject<UMaterialInterface>(nullptr, CheminLumiere, nullptr, LOAD_NoWarn | LOAD_Quiet);
	MateriauFantome = LoadObject<UMaterialInterface>(nullptr, CheminFantome, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!MateriauLumiere)
	{
		Message(TEXT("Materiau lumineux /Game/Chelles/Fetes/M_Fetes_Lumiere absent : lancez Tools\\Fetes\\INSTALLER_FETES.bat (les feux seront moins brillants)."));
	}

	const EFeteChelles Fete = FeteVoulue();
	Message(FString::Printf(TEXT("Pret. Mode %s, fete du jour : %s. Halloween %s-%s, 14 juillet %s-%s, musique %s-%s. Console : Chelles.Fetes Info"),
		*Reglages.Mode, *NomFete(FeteDuJour()),
		*TexteDate(Reglages.HalloweenDebut), *TexteDate(Reglages.HalloweenFin),
		*TexteDate(Reglages.JuilletDebut), *TexteDate(Reglages.JuilletFin),
		*TexteDate(Reglages.MusiqueDebut), *TexteDate(Reglages.MusiqueFin)));
	Lancer(Fete);
}

void AFetesChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	// Fin de partie : le monde emporte tout seul ce qu'on a cree (et les passants leurs citrouilles).
	// On ne nettoie que si les fetes sont retirees en cours de jeu.
	if (Raison == EEndPlayReason::Destroyed)
	{
		Arreter();
	}
	Super::EndPlay(Raison);
}

void AFetesChelles::Lancer(EFeteChelles Fete)
{
	Arreter();
	FeteEnCours = Fete;
	Temps = 0.f;
	switch (Fete)
	{
	case EFeteChelles::Halloween:
		LancerHalloween();
		Message(TEXT("Joyeux HALLOWEEN a Chelles ! Citrouilles, fantomes... et attention aux passants !"), true, FColor::Orange);
		break;
	case EFeteChelles::QuatorzeJuillet:
		LancerJuillet();
		Message(TEXT("Bonne fete nationale ! Feu d'artifice du 14 JUILLET dans le ciel de Chelles"), true, FColor::Cyan);
		break;
	case EFeteChelles::FeteMusique:
		LancerMusique();
		Message(TEXT("FETE DE LA MUSIQUE : une scene est installee dans le quartier, suivez la musique !"), true, FColor::Magenta);
		break;
	default:
		break;
	}
}

void AFetesChelles::Arreter()
{
	for (TWeakObjectPtr<USceneComponent>& C : Coiffes)
	{
		if (C.IsValid())
		{
			TArray<USceneComponent*> Enfants;
			C->GetChildrenComponents(true, Enfants);
			for (USceneComponent* E : Enfants)
			{
				if (E)
				{
					E->DestroyComponent();
				}
			}
			C->DestroyComponent();
		}
	}
	Coiffes.Reset();
	PassantsVus.Reset();
	for (UActorComponent* C : Crees)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	Crees.Reset();
	Bougies.Reset();
	Fantomes.Reset();
	Ambiance = nullptr;
	Etincelles.Reset();
	Eclairs.Reset();
	ForceEclairs.Reset();
	Fusees.Reset();
	Points.Reset();
	BoumsEnAttente.Reset();
	FinSpectacle = 0.f;
	ProchainSpectacle = 0.f;
	Scene = nullptr;
	Projecteurs.Reset();
	Lumieres.Reset();
	Musique = nullptr;
	FeteEnCours = EFeteChelles::Aucune;
}

// ---------------------------------------------------------------- Outils

FVector AFetesChelles::Depart() const
{
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		return It->GetActorLocation();
	}
	if (const APawn* Joueur = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return Joueur->GetActorLocation();
	}
	return FVector::ZeroVector;
}

bool AFetesChelles::PointDeVue(FVector& Lieu, FRotator& Direction) const
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			Lieu = PC->PlayerCameraManager->GetCameraLocation();
			Direction = PC->PlayerCameraManager->GetCameraRotation();
			return true;
		}
		if (APawn* P = PC->GetPawn())
		{
			Lieu = P->GetActorLocation();
			Direction = P->GetActorRotation();
			return true;
		}
	}
	return false;
}

bool AFetesChelles::TrouverSol(const FVector& XY, FVector& Sol, bool bHerbeSeulement) const
{
	UWorld* Monde = GetWorld();
	FCollisionQueryParams Requete(TEXT("FetesSol"), false, this);
	const FVector Haut(XY.X, XY.Y, XY.Z + 30000.f);
	const FVector Bas(XY.X, XY.Y, XY.Z - 30000.f);
	for (int32 Essai = 0; Essai < 4; ++Essai)
	{
		FHitResult Impact;
		if (!Monde->LineTraceSingleByChannel(Impact, Haut, Bas, ECC_Visibility, Requete))
		{
			return false;
		}
		AActor* Touche = Impact.GetActor();
		// le dome du ciel (GoodSky) ne doit pas arreter le rayon
		if (Touche && Touche->GetClass()->GetName().Contains(TEXT("Sky")))
		{
			Requete.AddIgnoredActor(Touche);
			continue;
		}
		if (Touche && Touche->IsA<APawn>())
		{
			return false;
		}
		if (bHerbeSeulement && (!Touche || !Touche->GetClass()->GetName().Contains(TEXT("Landscape"))))
		{
			return false;
		}
		if (Impact.ImpactNormal.Z < 0.9f)
		{
			return false;
		}
		Sol = Impact.ImpactPoint;
		return true;
	}
	return false;
}

bool AFetesChelles::PlaceLibre(const FVector& Sol, float Rayon, float Hauteur) const
{
	FCollisionQueryParams Requete(TEXT("FetesPlace"), false, this);
	FCollisionObjectQueryParams Objets;
	Objets.AddObjectTypesToQuery(ECC_WorldStatic);
	Objets.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objets.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objets.AddObjectTypesToQuery(ECC_Vehicle);
	const FCollisionShape Forme = FCollisionShape::MakeCapsule(Rayon, FMath::Max(Rayon, Hauteur * 0.5f));
	return !GetWorld()->OverlapAnyTestByObjectType(Sol + FVector(0, 0, FMath::Max(Rayon, Hauteur * 0.5f) + 30.f),
		FQuat::Identity, Objets, Forme, Requete);
}

bool AFetesChelles::EstNuit() const
{
	float Heure = -1.f;
	// L'heure du ciel du plugin ChellesMeteo, s'il est la (heure forcee comprise)
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName() == TEXT("MeteoChelles"))
		{
			if (FFloatProperty* P = FindFProperty<FFloatProperty>(It->GetClass(), TEXT("HeureActuelle")))
			{
				Heure = P->GetPropertyValue_InContainer(*It);
			}
			break;
		}
	}
	if (Heure < 0.f)
	{
		const FDateTime Maintenant = FDateTime::Now();
		Heure = Maintenant.GetHour() + Maintenant.GetMinute() / 60.f;
	}
	return Heure >= 19.f || Heure < 7.f;
}

USceneComponent* AFetesChelles::NouvelleRacine(const FTransform& Place)
{
	USceneComponent* Racine = NewObject<USceneComponent>(this);
	Racine->SetMobility(EComponentMobility::Movable);
	Racine->SetupAttachment(RootComponent);
	Racine->RegisterComponent();
	Racine->SetWorldTransform(Place);
	Crees.Add(Racine);
	return Racine;
}

UStaticMeshComponent* AFetesChelles::Piece(USceneComponent* Parent, UStaticMesh* Maillage, UMaterialInterface* Materiau,
	const FVector& Position, const FRotator& Rotation, const FVector& Echelle, AActor* Proprietaire)
{
	if (!Maillage || !Parent)
	{
		return nullptr;
	}
	AActor* Proprio = Proprietaire ? Proprietaire : this;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Proprio);
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(Maillage);
	if (Materiau)
	{
		C->SetMaterial(0, Materiau);
	}
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetGenerateOverlapEvents(false);
	C->SetCastShadow(false);
	C->SetupAttachment(Parent);
	C->SetRelativeLocationAndRotation(Position, Rotation);
	C->SetRelativeScale3D(Echelle);
	C->RegisterComponent();
	if (Proprio == this)
	{
		Crees.Add(C);
	}
	return C;
}

UMaterialInterface* AFetesChelles::Couleur(const FLinearColor& Teinte, float Force, bool bLumineux)
{
	const FString Cle = FString::Printf(TEXT("%.3f_%.3f_%.3f_%.1f_%d"), Teinte.R, Teinte.G, Teinte.B, Force, bLumineux ? 1 : 0);
	if (UMaterialInterface** Deja = Materiaux.Find(Cle))
	{
		return *Deja;
	}
	UMaterialInstanceDynamic* Mid = nullptr;
	if (bLumineux && MateriauLumiere)
	{
		Mid = UMaterialInstanceDynamic::Create(MateriauLumiere, this);
		Mid->SetVectorParameterValue(TEXT("Couleur"), Teinte);
		Mid->SetScalarParameterValue(TEXT("Force"), Force);
	}
	else if (MateriauBase)
	{
		Mid = UMaterialInstanceDynamic::Create(MateriauBase, this);
		Mid->SetVectorParameterValue(TEXT("Color"), Teinte);
	}
	Materiaux.Add(Cle, Mid);
	return Mid;
}

UMaterialInterface* AFetesChelles::Fantome(float Opacite)
{
	const FString Cle = FString::Printf(TEXT("Fantome_%.2f"), Opacite);
	if (UMaterialInterface** Deja = Materiaux.Find(Cle))
	{
		return *Deja;
	}
	UMaterialInterface* Resultat = nullptr;
	if (MateriauFantome)
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(MateriauFantome, this);
		Mid->SetVectorParameterValue(TEXT("Couleur"), FLinearColor(0.75f, 0.9f, 1.f));
		Mid->SetScalarParameterValue(TEXT("Force"), 1.5f);
		Mid->SetScalarParameterValue(TEXT("Opacite"), Opacite);
		Resultat = Mid;
	}
	else
	{
		Resultat = Couleur(FLinearColor(0.9f, 0.95f, 1.f), 2.f, true);
	}
	Materiaux.Add(Cle, Resultat);
	return Resultat;
}

// ---------------------------------------------------------------- HALLOWEEN

USceneComponent* AFetesChelles::FabriquerCitrouille(USceneComponent* Parent, const FVector& Position, float Lacet, float T,
	bool bBougie, AActor* Proprietaire)
{
	AActor* Proprio = Proprietaire ? Proprietaire : this;
	USceneComponent* Racine = NewObject<USceneComponent>(Proprio);
	Racine->SetMobility(EComponentMobility::Movable);
	Racine->RegisterComponent();
	Racine->SetWorldLocationAndRotation(Position, FRotator(0.f, Lacet, 0.f));
	if (Parent)
	{
		Racine->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform, NAME_None);
	}
	if (Proprio == this)
	{
		Crees.Add(Racine);
	}

	const float s = T / 100.f;
	UStaticMesh* Perso = Reglages.CitrouilleMaillage.IsEmpty() ? nullptr : Charger<UStaticMesh>(Reglages.CitrouilleMaillage);
	if (Perso)
	{
		// citrouille d'un pack : on la met a la bonne taille
		const FBoxSphereBounds B = Perso->GetBounds();
		const float Echelle = B.BoxExtent.Z > 1.f ? (T * 0.5f) / B.BoxExtent.Z : 1.f;
		Piece(Racine, Perso, nullptr, FVector(0, 0, (B.BoxExtent.Z - B.Origin.Z) * Echelle), FRotator::ZeroRotator, FVector(Echelle), Proprio);
	}
	else
	{
		UMaterialInterface* Orange = Couleur(FLinearColor(1.f, 0.32f, 0.02f));
		UMaterialInterface* OrangeFonce = Couleur(FLinearColor(0.8f, 0.22f, 0.01f));
		UMaterialInterface* Tige = Couleur(FLinearColor(0.18f, 0.28f, 0.04f));
		UMaterialInterface* Flamme = Couleur(FLinearColor(1.f, 0.55f, 0.05f), 14.f, true);
		const float Z = 0.42f * T;
		Piece(Racine, Sphere, Orange, FVector(0, 0, Z), FRotator::ZeroRotator, FVector(1.15f * s, 1.15f * s, 0.85f * s), Proprio);
		Piece(Racine, Sphere, OrangeFonce, FVector(0, 0.2f * T, Z), FRotator::ZeroRotator, FVector(0.95f * s, 0.8f * s, 0.82f * s), Proprio);
		Piece(Racine, Sphere, OrangeFonce, FVector(0, -0.2f * T, Z), FRotator::ZeroRotator, FVector(0.95f * s, 0.8f * s, 0.82f * s), Proprio);
		Piece(Racine, Cylindre, Tige, FVector(0, 0, 0.88f * T), FRotator(12.f, 0, 0), FVector(0.09f * s, 0.09f * s, 0.2f * s), Proprio);
		// visage qui brille : deux yeux en losange et une bouche
		Piece(Racine, Cube, Flamme, FVector(0.53f * T, 0.17f * T, 0.52f * T), FRotator(0, 0, 45.f), FVector(0.1f * s, 0.1f * s, 0.1f * s), Proprio);
		Piece(Racine, Cube, Flamme, FVector(0.53f * T, -0.17f * T, 0.52f * T), FRotator(0, 0, 45.f), FVector(0.1f * s, 0.1f * s, 0.1f * s), Proprio);
		Piece(Racine, Cube, Flamme, FVector(0.53f * T, 0, 0.3f * T), FRotator::ZeroRotator, FVector(0.1f * s, 0.36f * s, 0.06f * s), Proprio);
	}

	if (bBougie)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(Racine);
		L->SetRelativeLocation(FVector(0.9f * T, 0, 0.45f * T));
		L->SetLightColor(FLinearColor(1.f, 0.45f, 0.08f));
		L->SetIntensity(1200.f);
		L->SetAttenuationRadius(FMath::Max(250.f, T * 5.f));
		L->SetCastShadows(false);
		L->RegisterComponent();
		Crees.Add(L);
		Bougies.Add(L);
	}
	return Racine;
}

void AFetesChelles::FabriquerFantome(const FVector& Centre, FRandomStream& Hasard)
{
	const float G = Hasard.FRandRange(110.f, 170.f);
	const float s = G / 100.f;
	USceneComponent* Racine = NouvelleRacine(FTransform(Centre));
	UMaterialInterface* Drap = Fantome(0.55f);
	UMaterialInterface* Noir = Couleur(FLinearColor(0.01f, 0.01f, 0.01f));
	Piece(Racine, Sphere, Drap, FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.9f * s, 0.9f * s, 1.f * s));
	Piece(Racine, Cone, Drap, FVector(0, 0, -0.6f * G), FRotator(0, 0, 180.f), FVector(0.9f * s, 0.9f * s, 1.f * s));
	Piece(Racine, Sphere, Noir, FVector(0.4f * G, 0.14f * G, 0.1f * G), FRotator::ZeroRotator, FVector(0.12f * s));
	Piece(Racine, Sphere, Noir, FVector(0.4f * G, -0.14f * G, 0.1f * G), FRotator::ZeroRotator, FVector(0.12f * s));
	Piece(Racine, Sphere, Noir, FVector(0.42f * G, 0, -0.12f * G), FRotator::ZeroRotator, FVector(0.08f * s, 0.12f * s, 0.16f * s));

	FFantomeFete F;
	F.Racine = Racine;
	F.Centre = Centre;
	F.Rayon = Hasard.FRandRange(300.f, 1200.f);
	F.Vitesse = Hasard.FRandRange(0.15f, 0.4f) * (Hasard.FRand() < 0.5f ? -1.f : 1.f);
	F.Phase = Hasard.FRandRange(0.f, 2.f * PI);
	F.Hauteur = Centre.Z;
	Fantomes.Add(F);
}

void AFetesChelles::LancerHalloween()
{
	const FVector Centre = Depart();
	FRandomStream Hasard(77500 + 1031);
	const float Rayon = Reglages.CitrouillesRayon * 100.f;
	const float T = Reglages.TailleCitrouille;

	// Citrouilles posees par petits groupes sur l'herbe, memes places pour tous les joueurs
	TArray<FVector> Poses;
	int32 Posees = 0;
	for (int32 Essai = 0; Essai < Reglages.Citrouilles * 40 && Posees < Reglages.Citrouilles; ++Essai)
	{
		const float Angle = Hasard.FRandRange(0.f, 2.f * PI);
		const float Dist = Rayon * FMath::Sqrt(Hasard.FRand());
		const FVector XY = Centre + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		FVector Sol;
		if (!TrouverSol(XY, Sol, true))
		{
			continue;
		}
		bool bTropPres = false;
		for (const FVector& Autre : Poses)
		{
			if (FVector::Dist2D(Autre, Sol) < 600.f)
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres || !PlaceLibre(Sol, T * 1.5f, T))
		{
			continue;
		}
		Poses.Add(Sol);
		// le visage regarde vers le depart (la ou passent les joueurs)
		const float VersDepart = (Centre - Sol).Rotation().Yaw;
		const int32 Groupe = FMath::Min(Hasard.RandRange(1, 3), Reglages.Citrouilles - Posees);
		for (int32 i = 0; i < Groupe; ++i)
		{
			const FVector Decalage = i == 0 ? FVector::ZeroVector
				: FRotator(0.f, VersDepart + (i == 1 ? 70.f : -70.f), 0.f).Vector() * T * 1.3f;
			FVector SolGroupe = Sol + Decalage;
			TrouverSol(SolGroupe, SolGroupe, false);
			const float Taille = T * (i == 0 ? 1.f : Hasard.FRandRange(0.55f, 0.8f));
			const bool bBougie = i == 0 && Bougies.Num() < Reglages.BougiesMax;
			FabriquerCitrouille(nullptr, SolGroupe, VersDepart + Hasard.FRandRange(-25.f, 25.f), Taille, bBougie);
			++Posees;
		}
	}

	// Fantomes qui tournent au-dessus des rues
	for (int32 i = 0; i < Reglages.Fantomes; ++i)
	{
		const float Angle = Hasard.FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::Min(Rayon, 8000.f) * FMath::Sqrt(Hasard.FRand());
		FVector Sol = Centre + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		if (!TrouverSol(Sol, Sol, false))
		{
			Sol = Centre + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		}
		FabriquerFantome(Sol + FVector(0, 0, Hasard.FRandRange(300.f, 650.f)), Hasard);
	}

	// Ambiance sonore inquietante (la nuit)
	if (USoundBase* Son = Charger<USoundBase>(Reglages.AmbianceHalloween))
	{
		Ambiance = NewObject<UAudioComponent>(this);
		Ambiance->SetupAttachment(RootComponent);
		Ambiance->bAllowSpatialization = false;
		Ambiance->bAutoActivate = false;
		Ambiance->SetSound(Son);
		Ambiance->SetVolumeMultiplier(Reglages.VolumeAmbiance);
		Ambiance->RegisterComponent();
		Crees.Add(Ambiance);
	}

	ProchainCoiffage = 1.f;
	Message(FString::Printf(TEXT("Halloween : %d citrouille(s) (%d bougie(s)), %d fantome(s)."), Posees, Bougies.Num(), Fantomes.Num()));
}

void AFetesChelles::CoifferPassants()
{
	if (Reglages.TetesCitrouille <= 0)
	{
		return;
	}
	static const FName Tetes[] = {
		TEXT("head"), TEXT("Head"), TEXT("HEAD"), TEXT("head_01"), TEXT("Bip01-Head"), TEXT("Bip001-Head"),
		TEXT("Bip01_Head"), TEXT("Bip001_Head"), TEXT("mixamorig:Head"), TEXT("CC_Base_Head"), TEXT("J_Bip_C_Head") };

	FVector Vue;
	FRotator Direction;
	const bool bVue = PointDeVue(Vue, Direction);
	int32 Nouveaux = 0;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Passant = *It;
		if (!Passant || Passant->IsPlayerControlled() || PassantsVus.Contains(Passant->GetFName()))
		{
			continue;
		}
		if (bVue && FVector::DistSquared(Vue, Passant->GetActorLocation()) > FMath::Square(12000.f))
		{
			continue; // trop loin pour l'instant, on verra plus tard
		}
		PassantsVus.Add(Passant->GetFName());
		if (static_cast<int32>(GetTypeHash(Passant->GetName()) % 100u) >= Reglages.TetesCitrouille)
		{
			continue;
		}
		USkeletalMeshComponent* Corps = Passant->GetMesh();
		if (!Corps || !Corps->SkeletalMesh)
		{
			continue;
		}
		FName Os = NAME_None;
		for (const FName& Nom : Tetes)
		{
			if (Corps->GetBoneIndex(Nom) != INDEX_NONE)
			{
				Os = Nom;
				break;
			}
		}
		if (Os.IsNone())
		{
			continue;
		}
		const float T = 40.f * FMath::Clamp(Passant->GetActorScale3D().Z, 0.5f, 3.f);
		const FVector Tete = Corps->GetSocketLocation(Os);
		USceneComponent* C = FabriquerCitrouille(nullptr, Tete - FVector(0, 0, T * 0.25f), Passant->GetActorRotation().Yaw, T, false, Passant);
		if (C)
		{
			C->AttachToComponent(Corps, FAttachmentTransformRules::KeepWorldTransform, Os);
			Coiffes.Add(C);
			++Nouveaux;
		}
		if (Nouveaux >= 6)
		{
			break; // un peu a chaque fois pour ne pas ralentir le jeu
		}
	}
}

void AFetesChelles::AnimerHalloween(float DeltaSeconds)
{
	const bool bNuit = EstNuit();

	// Bougies qui vacillent
	for (int32 i = 0; i < Bougies.Num(); ++i)
	{
		if (UPointLightComponent* L = Bougies[i])
		{
			const float Vacille = 0.7f + 0.2f * FMath::Sin(Temps * 11.f + i * 1.7f) + 0.1f * FMath::Sin(Temps * 23.f + i);
			L->SetIntensity((bNuit ? 1500.f : 300.f) * Vacille);
		}
	}

	// Fantomes qui tournent et ondulent
	for (FFantomeFete& F : Fantomes)
	{
		if (!F.Racine)
		{
			continue;
		}
		const float A = F.Phase + Temps * F.Vitesse;
		const FVector P = F.Centre + FVector(FMath::Cos(A) * F.Rayon, FMath::Sin(A) * F.Rayon, 60.f * FMath::Sin(Temps * 1.6f + F.Phase));
		const float Lacet = FMath::RadiansToDegrees(A) + (F.Vitesse > 0.f ? 90.f : -90.f);
		F.Racine->SetWorldLocationAndRotation(P, FRotator(8.f * FMath::Sin(Temps * 2.f + F.Phase), Lacet, 0.f));
	}

	// Tetes de citrouille sur les passants (les nouveaux arrivent au fil du temps)
	if (Temps >= ProchainCoiffage)
	{
		ProchainCoiffage = Temps + 3.f;
		CoifferPassants();
		if (Ambiance)
		{
			if (bNuit && !Ambiance->IsPlaying())
			{
				Ambiance->FadeIn(4.f, Reglages.VolumeAmbiance);
			}
			else if (!bNuit && Ambiance->IsPlaying())
			{
				Ambiance->FadeOut(4.f, 0.f);
			}
		}
	}
}

// ---------------------------------------------------------------- 14 JUILLET

void AFetesChelles::PreparerFeux()
{
	if (Etincelles.Num() > 0)
	{
		return;
	}
	// une couleur par groupe d'etincelles + l'or des trainees
	TArray<FLinearColor> Teintes = Reglages.CouleursFeux;
	Teintes.Add(FLinearColor(1.f, 0.6f, 0.15f));
	for (const FLinearColor& Teinte : Teintes)
	{
		UInstancedStaticMeshComponent* Ism = NewObject<UInstancedStaticMeshComponent>(this);
		Ism->SetMobility(EComponentMobility::Movable);
		Ism->SetStaticMesh(Sphere);
		Ism->SetMaterial(0, Couleur(Teinte, Reglages.ForceLumiere, true));
		Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ism->SetGenerateOverlapEvents(false);
		Ism->SetCastShadow(false);
		Ism->SetupAttachment(RootComponent);
		Ism->RegisterComponent();
		Ism->SetWorldTransform(FTransform::Identity);
		Crees.Add(Ism);
		Etincelles.Add(Ism);
	}
	// lumieres qui eclairent le quartier a chaque explosion
	for (int32 i = 0; i < 4; ++i)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(RootComponent);
		L->bUseInverseSquaredFalloff = false;
		L->LightFalloffExponent = 2.f;
		L->SetIntensity(0.f);
		L->SetAttenuationRadius(25000.f);
		L->SetCastShadows(false);
		L->RegisterComponent();
		Crees.Add(L);
		Eclairs.Add(L);
		ForceEclairs.Add(0.f);
	}
	SonBoum = Charger<USoundBase>(Reglages.SonExplosion);
	SonFusee = Charger<USoundBase>(Reglages.SonDepart);
	PorteeFeux = NewObject<USoundAttenuation>(this);
	PorteeFeux->Attenuation.bAttenuate = true;
	PorteeFeux->Attenuation.bSpatialize = true;
	PorteeFeux->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	PorteeFeux->Attenuation.AttenuationShapeExtents = FVector(5000.f, 0.f, 0.f);
	PorteeFeux->Attenuation.FalloffDistance = 60000.f;
}

void AFetesChelles::LancerJuillet()
{
	PreparerFeux();
	ProchainSpectacle = 8.f; // premier spectacle quelques secondes apres le debut de la partie
}

void AFetesChelles::DemarrerSpectacle(float Duree)
{
	FVector Vue;
	FRotator Direction;
	const bool bVue = PointDeVue(Vue, Direction);
	TArray<FString> P;
	Reglages.LieuFeux.ParseIntoArray(P, TEXT(","), true);
	if (P.Num() >= 3)
	{
		CentreFeux = FVector(Nombre(P[0]), Nombre(P[1]), Nombre(P[2]));
	}
	else if (Reglages.LieuFeux.Equals(TEXT("Depart"), ESearchCase::IgnoreCase) || !bVue)
	{
		CentreFeux = Depart();
	}
	else
	{
		// devant le joueur, la ou il regarde
		const FVector Devant = FRotator(0.f, Direction.Yaw, 0.f).Vector();
		CentreFeux = Vue + Devant * Reglages.DistanceFeux * 100.f;
	}
	FVector Sol;
	if (TrouverSol(CentreFeux, Sol, false))
	{
		CentreFeux = Sol;
	}
	else if (bVue)
	{
		CentreFeux.Z = Vue.Z - 200.f;
	}
	FinSpectacle = Temps + Duree;
	ProchaineFusee = Temps + 0.5f;
	Message(TEXT("Le feu d'artifice commence ! Levez les yeux"), true, FColor::Cyan);
}

void AFetesChelles::TirerFusee()
{
	const int32 NbCouleurs = Reglages.CouleursFeux.Num();
	FFuseeFete F;
	const FVector Base = CentreFeux + FVector(FMath::FRandRange(-2500.f, 2500.f), FMath::FRandRange(-2500.f, 2500.f), 0.f);
	const FVector Cible = Base + FVector(FMath::FRandRange(-1500.f, 1500.f), FMath::FRandRange(-1500.f, 1500.f),
		Reglages.HauteurFeux * 100.f * FMath::FRandRange(0.8f, 1.2f));
	F.Position = Base;
	F.Duree = FMath::FRandRange(1.6f, 2.3f);
	F.Vitesse = (Cible - Base) / F.Duree;
	F.Couleur = FMath::RandRange(0, FMath::Max(0, NbCouleurs - 1));
	F.Forme = FMath::RandRange(0, 3);
	Fusees.Add(F);
	if (SonFusee)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SonFusee, Base, Reglages.VolumeFeux * 0.5f, FMath::FRandRange(0.9f, 1.2f), 0.f, PorteeFeux);
	}
}

void AFetesChelles::Eclater(const FFuseeFete& Fusee)
{
	const int32 NbCouleurs = Reglages.CouleursFeux.Num();
	const int32 Or = NbCouleurs; // dernier groupe = or
	const int32 N = Reglages.EtincellesParBouquet;
	const float V = FMath::FRandRange(1900.f, 2600.f);
	// bleu blanc rouge de temps en temps
	const bool bTricolore = NbCouleurs >= 3 && FMath::FRand() < 0.25f;
	const FVector Axe = FMath::VRand();
	const FVector A1 = FVector::CrossProduct(Axe, FVector(0.3f, 0.5f, 0.8f)).GetSafeNormal();
	const FVector A2 = FVector::CrossProduct(Axe, A1);

	const int32 Total = Fusee.Forme == 2 ? N / 2 : (Fusee.Forme == 3 ? N + N / 2 : N);
	for (int32 i = 0; i < Total; ++i)
	{
		FEtincelleFete E;
		E.Position = Fusee.Position;
		E.Couleur = bTricolore ? (i % 3) : Fusee.Couleur;
		E.Vie = FMath::FRandRange(1.5f, 2.4f);
		E.Taille = FMath::FRandRange(0.8f, 1.2f);
		switch (Fusee.Forme)
		{
		case 1: // anneau
		{
			const float Ang = 2.f * PI * i / Total;
			E.Vitesse = (A1 * FMath::Cos(Ang) + A2 * FMath::Sin(Ang)) * V;
			break;
		}
		case 2: // palmier dore qui retombe
			E.Vitesse = FMath::VRand() * V * 0.8f + FVector(0, 0, 400.f);
			E.Couleur = Or;
			E.Vie *= 1.5f;
			E.Taille *= 1.4f;
			break;
		case 3: // double : coeur d'une autre couleur
			E.Vitesse = FMath::VRand() * V * (i < N ? 1.f : 0.5f);
			if (i >= N && NbCouleurs > 1)
			{
				E.Couleur = (Fusee.Couleur + 1 + FMath::RandRange(0, NbCouleurs - 2)) % NbCouleurs;
			}
			break;
		default: // boule
			E.Vitesse = FMath::VRand() * V * FMath::FRandRange(0.85f, 1.f);
			break;
		}
		Points.Add(E);
	}

	// eclair de lumiere sur le quartier
	if (Eclairs.Num() > 0)
	{
		const int32 i = ProchainEclair++ % Eclairs.Num();
		if (UPointLightComponent* L = Eclairs[i])
		{
			L->SetWorldLocation(Fusee.Position);
			L->SetLightColor(Fusee.Forme == 2 ? FLinearColor(1.f, 0.6f, 0.15f) : Reglages.CouleursFeux[FMath::Clamp(Fusee.Couleur, 0, NbCouleurs - 1)]);
			ForceEclairs[i] = 40.f;
		}
	}

	// le "BOUM" arrive avec un peu de retard (le son va moins vite que la lumiere)
	FVector Vue;
	FRotator Dir;
	const float Distance = PointDeVue(Vue, Dir) ? FVector::Dist(Vue, Fusee.Position) : 0.f;
	BoumsEnAttente.Add(TPair<float, FVector>(Temps + FMath::Min(Distance / 34300.f, 2.f), Fusee.Position));
}

void AFetesChelles::AnimerFeux(float DeltaSeconds)
{
	if (Etincelles.Num() == 0)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	// Programme : un spectacle, une pause, un spectacle...
	if (FeteEnCours == EFeteChelles::QuatorzeJuillet && Temps >= ProchainSpectacle && Temps >= FinSpectacle)
	{
		DemarrerSpectacle(Reglages.DureeSpectacle);
		ProchainSpectacle = FinSpectacle + Reglages.PauseSpectacle;
	}
	if (Temps < FinSpectacle && Temps >= ProchaineFusee)
	{
		const bool bBouquetFinal = FinSpectacle - Temps < 10.f;
		TirerFusee();
		if (bBouquetFinal && FMath::FRand() < 0.5f)
		{
			TirerFusee();
		}
		ProchaineFusee = Temps + (bBouquetFinal ? FMath::FRandRange(0.12f, 0.3f) : FMath::FRandRange(0.6f, 1.6f));
	}

	// Fusees qui montent (avec une trainee doree)
	const int32 Or = Etincelles.Num() - 1;
	for (int32 i = Fusees.Num() - 1; i >= 0; --i)
	{
		FFuseeFete& F = Fusees[i];
		F.Age += Dt;
		F.Position += F.Vitesse * Dt;
		F.Vitesse.Z -= 150.f * Dt;
		FEtincelleFete Trace;
		Trace.Position = F.Position;
		Trace.Vitesse = FMath::VRand() * 60.f;
		Trace.Vie = 0.45f;
		Trace.Couleur = Or;
		Trace.Taille = 0.6f;
		Points.Add(Trace);
		if (F.Age >= F.Duree)
		{
			Eclater(F);
			Fusees.RemoveAtSwap(i);
		}
	}

	// Etincelles : freinage de l'air + gravite
	const float Frein = FMath::Exp(-1.3f * Dt);
	TArray<TArray<FTransform>> ParCouleur;
	ParCouleur.SetNum(Etincelles.Num());
	for (int32 i = Points.Num() - 1; i >= 0; --i)
	{
		FEtincelleFete& E = Points[i];
		E.Age += Dt;
		if (E.Age >= E.Vie)
		{
			Points.RemoveAtSwap(i);
			continue;
		}
		E.Vitesse *= Frein;
		E.Vitesse.Z -= 380.f * Dt;
		E.Position += E.Vitesse * Dt;
		const float Reste = 1.f - E.Age / E.Vie;
		const float Scintille = Reste < 0.35f ? (0.6f + 0.4f * FMath::Sin(E.Age * 40.f + i)) : 1.f;
		const float Taille = Reglages.TailleEtincelle * E.Taille * FMath::Sqrt(Reste) * Scintille;
		ParCouleur[FMath::Clamp(E.Couleur, 0, Etincelles.Num() - 1)].Add(FTransform(FRotator::ZeroRotator, E.Position, FVector(Taille)));
	}
	for (int32 c = 0; c < Etincelles.Num(); ++c)
	{
		UInstancedStaticMeshComponent* Ism = Etincelles[c];
		if (!Ism)
		{
			continue;
		}
		const TArray<FTransform>& Liste = ParCouleur[c];
		if (Ism->GetInstanceCount() != Liste.Num())
		{
			Ism->ClearInstances();
			if (Liste.Num() > 0)
			{
				Ism->AddInstances(Liste, false);
				Ism->UpdateBounds();
				Ism->MarkRenderStateDirty();
			}
			continue;
		}
		if (Liste.Num() == 0)
		{
			continue;
		}
		Ism->BatchUpdateInstancesTransforms(0, Liste, true, false, true);
		Ism->UpdateBounds();
		Ism->MarkRenderStateDirty();
	}

	// Eclairs qui s'eteignent
	for (int32 i = 0; i < Eclairs.Num(); ++i)
	{
		ForceEclairs[i] *= FMath::Exp(-4.f * Dt);
		if (Eclairs[i])
		{
			Eclairs[i]->SetIntensity(ForceEclairs[i] < 0.05f ? 0.f : ForceEclairs[i]);
		}
	}

	// Les BOUM
	for (int32 i = BoumsEnAttente.Num() - 1; i >= 0; --i)
	{
		if (Temps >= BoumsEnAttente[i].Key)
		{
			if (SonBoum)
			{
				UGameplayStatics::PlaySoundAtLocation(this, SonBoum, BoumsEnAttente[i].Value, Reglages.VolumeFeux,
					FMath::FRandRange(0.8f, 1.15f), 0.f, PorteeFeux);
			}
			BoumsEnAttente.RemoveAtSwap(i);
		}
	}
}

// ---------------------------------------------------------------- FETE DE LA MUSIQUE

bool AFetesChelles::PlaceScene(FVector& Sol, float& Lacet) const
{
	const FVector Centre = Depart();
	// 1) place enregistree avec "Chelles.Fetes PoserScene"
	TArray<FString> Lignes;
	if (FFileHelper::LoadFileToStringArray(Lignes, *FPaths::Combine(Dossier(), TEXT("Scene_Position.txt"))))
	{
		for (const FString& Ligne : Lignes)
		{
			TArray<FString> P;
			Ligne.ParseIntoArray(P, TEXT(","), true);
			if (P.Num() >= 3 && !Ligne.StartsWith(TEXT("#")))
			{
				Sol = FVector(Nombre(P[0]), Nombre(P[1]), Nombre(P[2]));
				Lacet = P.Num() >= 4 ? Nombre(P[3]) : 0.f;
				return true;
			}
		}
	}
	// 2) une place plate et libre sur l'herbe, pas trop loin du depart (la meme pour tous les joueurs)
	FRandomStream Hasard(77500 + 621);
	for (int32 Essai = 0; Essai < 400; ++Essai)
	{
		const float Angle = Hasard.FRandRange(0.f, 2.f * PI);
		const float Dist = Hasard.FRandRange(2000.f, 12000.f);
		const FVector XY = Centre + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		FVector Ici;
		if (!TrouverSol(XY, Ici, true) || !PlaceLibre(Ici, 750.f, 600.f))
		{
			continue;
		}
		// la piste de danse devant la scene doit etre libre aussi
		const float VersDepart = (Centre - Ici).Rotation().Yaw;
		const FVector Piste = Ici + FRotator(0.f, VersDepart, 0.f).Vector() * 900.f;
		FVector SolPiste;
		if (!TrouverSol(Piste, SolPiste, false) || FMath::Abs(SolPiste.Z - Ici.Z) > 150.f || !PlaceLibre(SolPiste, 600.f, 200.f))
		{
			continue;
		}
		Sol = Ici;
		Lacet = VersDepart;
		return true;
	}
	return false;
}

void AFetesChelles::FabriquerScene(const FVector& Sol, float Lacet)
{
	Scene = NouvelleRacine(FTransform(FRotator(0.f, Lacet, 0.f), Sol));
	UMaterialInterface* Gris = Couleur(FLinearColor(0.08f, 0.08f, 0.09f));
	UMaterialInterface* Metal = Couleur(FLinearColor(0.45f, 0.45f, 0.5f));
	UMaterialInterface* Noir = Couleur(FLinearColor(0.01f, 0.01f, 0.01f));

	// Estrade, mur du fond, poteaux, portique
	Piece(Scene, Cube, Gris, FVector(0, 0, 30.f), FRotator::ZeroRotator, FVector(5.f, 9.f, 0.6f));
	Piece(Scene, Cube, Noir, FVector(-230.f, 0, 330.f), FRotator::ZeroRotator, FVector(0.4f, 9.f, 6.f));
	Piece(Scene, Cylindre, Metal, FVector(-200.f, 440.f, 330.f), FRotator::ZeroRotator, FVector(0.25f, 0.25f, 6.6f));
	Piece(Scene, Cylindre, Metal, FVector(-200.f, -440.f, 330.f), FRotator::ZeroRotator, FVector(0.25f, 0.25f, 6.6f));
	Piece(Scene, Cylindre, Metal, FVector(220.f, 440.f, 330.f), FRotator::ZeroRotator, FVector(0.25f, 0.25f, 6.6f));
	Piece(Scene, Cylindre, Metal, FVector(220.f, -440.f, 330.f), FRotator::ZeroRotator, FVector(0.25f, 0.25f, 6.6f));
	Piece(Scene, Cube, Metal, FVector(10.f, 0, 660.f), FRotator::ZeroRotator, FVector(4.6f, 9.2f, 0.25f));
	// Enceintes
	Piece(Scene, Cube, Noir, FVector(150.f, 560.f, 160.f), FRotator::ZeroRotator, FVector(1.2f, 1.2f, 2.6f));
	Piece(Scene, Cube, Noir, FVector(150.f, -560.f, 160.f), FRotator::ZeroRotator, FVector(1.2f, 1.2f, 2.6f));
	Piece(Scene, Cylindre, Metal, FVector(211.f, 560.f, 200.f), FRotator(90.f, 0, 0), FVector(0.8f, 0.8f, 0.02f));
	Piece(Scene, Cylindre, Metal, FVector(211.f, -560.f, 200.f), FRotator(90.f, 0, 0), FVector(0.8f, 0.8f, 0.02f));
	// Platine du DJ
	Piece(Scene, Cube, Metal, FVector(-60.f, 0, 110.f), FRotator::ZeroRotator, FVector(0.9f, 2.2f, 1.f));

	// Ecran du fond + bandeau lumineux devant l'estrade (couleurs qui changent)
	auto Ecran = [this](const FVector& Position, const FVector& Echelle)
	{
		UMaterialInstanceDynamic* Mid = nullptr;
		if (MateriauLumiere)
		{
			Mid = UMaterialInstanceDynamic::Create(MateriauLumiere, this);
			Mid->SetScalarParameterValue(TEXT("Force"), 6.f);
		}
		else if (MateriauBase)
		{
			Mid = UMaterialInstanceDynamic::Create(MateriauBase, this);
		}
		Piece(Scene, Cube, Mid, Position, FRotator::ZeroRotator, Echelle);
		if (Mid)
		{
			Lumieres.Add(Mid);
		}
	};
	Ecran(FVector(-208.f, 0, 340.f), FVector(0.05f, 8.f, 4.6f));
	Ecran(FVector(252.f, 0, 30.f), FVector(0.05f, 9.f, 0.4f));

	// Projecteurs colores sur le portique, braques sur la piste
	const float Ys[] = { -360.f, -120.f, 120.f, 360.f };
	for (float Y : Ys)
	{
		USpotLightComponent* Spot = NewObject<USpotLightComponent>(this);
		Spot->SetMobility(EComponentMobility::Movable);
		Spot->SetupAttachment(Scene);
		Spot->SetRelativeLocationAndRotation(FVector(150.f, Y, 640.f), FRotator(-45.f, 0.f, 0.f));
		Spot->SetIntensity(60000.f);
		Spot->SetAttenuationRadius(3500.f);
		Spot->SetInnerConeAngle(10.f);
		Spot->SetOuterConeAngle(22.f);
		Spot->SetCastShadows(false);
		Spot->RegisterComponent();
		Crees.Add(Spot);
		Projecteurs.Add(Spot);
	}

	// Danseurs sur la piste devant la scene
	TArray<USkeletalMesh*> Corps;
	for (const FString& C : Reglages.MaillagesDanseurs)
	{
		if (USkeletalMesh* M = Charger<USkeletalMesh>(C))
		{
			Corps.Add(M);
		}
	}
	TArray<UAnimationAsset*> Pas;
	for (const FString& C : Reglages.Danses)
	{
		if (UAnimationAsset* A = Charger<UAnimationAsset>(C))
		{
			Pas.Add(A);
		}
	}
	int32 Danseurs = 0;
	FRandomStream Hasard(77500 + 2106);
	const FTransform Repere = Scene->GetComponentTransform();
	TArray<FVector> Places;
	for (int32 Essai = 0; Essai < Reglages.Danseurs * 20 && Danseurs < Reglages.Danseurs && Corps.Num() > 0 && Pas.Num() > 0; ++Essai)
	{
		const FVector Local(Hasard.FRandRange(450.f, 1300.f), Hasard.FRandRange(-550.f, 550.f), 0.f);
		bool bTropPres = false;
		for (const FVector& Autre : Places)
		{
			if (FVector::Dist2D(Autre, Local) < 140.f)
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}
		FVector Pied = Repere.TransformPosition(Local);
		if (!TrouverSol(Pied, Pied, false) || FMath::Abs(Pied.Z - Sol.Z) > 200.f)
		{
			continue;
		}
		USkeletalMesh* M = Corps[Hasard.RandRange(0, Corps.Num() - 1)];
		// une danse faite pour ce squelette
		TArray<UAnimationAsset*> Possibles;
		for (UAnimationAsset* A : Pas)
		{
			if (A && A->GetSkeleton() == M->GetSkeleton())
			{
				Possibles.Add(A);
			}
		}
		if (Possibles.Num() == 0)
		{
			continue;
		}
		Places.Add(Local);
		UAnimationAsset* Danse = Possibles[Hasard.RandRange(0, Possibles.Num() - 1)];
		USkeletalMeshComponent* D = NewObject<USkeletalMeshComponent>(this);
		D->SetMobility(EComponentMobility::Movable);
		D->SetupAttachment(RootComponent);
		D->SetSkeletalMesh(M);
		D->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		D->SetGenerateOverlapEvents(false);
		D->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		D->RegisterComponent();
		// face a la scene
		const float Regard = Lacet + 180.f + Hasard.FRandRange(-25.f, 25.f) + Reglages.RotationDanseurs;
		D->SetWorldLocationAndRotation(Pied, FRotator(0.f, Regard, 0.f));
		D->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		D->PlayAnimation(Danse, true);
		D->SetPlayRate(Hasard.FRandRange(0.9f, 1.1f));
		D->SetPosition(Hasard.FRand() * FMath::Max(0.f, Danse->GetMaxCurrentTime()), false);
		Crees.Add(D);
		++Danseurs;
	}
	if (Corps.Num() == 0 || Pas.Num() == 0)
	{
		Message(TEXT("Danseurs : personnage ou danses introuvables (voir MaillagesDanseurs / Danses dans Fetes.txt)."));
	}

	// La musique, qui s'entend autour de la scene
	Musique = NewObject<UAudioComponent>(this);
	Musique->SetupAttachment(Scene);
	Musique->SetRelativeLocation(FVector(100.f, 0.f, 250.f));
	Musique->bAutoActivate = false;
	Musique->bAllowSpatialization = true;
	Musique->bOverrideAttenuation = true;
	Musique->AttenuationOverrides.bAttenuate = true;
	Musique->AttenuationOverrides.bSpatialize = true;
	Musique->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	Musique->AttenuationOverrides.AttenuationShapeExtents = FVector(1500.f, 0.f, 0.f);
	Musique->AttenuationOverrides.FalloffDistance = Reglages.PorteeMusique * 100.f;
	Musique->SetVolumeMultiplier(Reglages.VolumeMusique);
	Musique->RegisterComponent();
	Musique->OnAudioFinished.AddDynamic(this, &AFetesChelles::MorceauFini);
	Crees.Add(Musique);
	Morceau = 0;
	JouerMorceau();

	Message(FString::Printf(TEXT("Fete de la musique : scene en %.0f, %.0f, %.0f (lacet %.0f), %d danseur(s)."), Sol.X, Sol.Y, Sol.Z, Lacet, Danseurs));
}

void AFetesChelles::JouerMorceau()
{
	if (!Musique || Reglages.Musiques.Num() == 0)
	{
		return;
	}
	for (int32 Essai = 0; Essai < Reglages.Musiques.Num(); ++Essai)
	{
		const FString& Chemin = Reglages.Musiques[(Morceau + Essai) % Reglages.Musiques.Num()];
		if (USoundBase* Son = Charger<USoundBase>(Chemin))
		{
			Morceau = (Morceau + Essai) % Reglages.Musiques.Num();
			Musique->SetSound(Son);
			Musique->Play();
			Message(TEXT("Musique : ") + FPaths::GetBaseFilename(Chemin));
			return;
		}
	}
	Message(TEXT("Aucune musique trouvee (voir Musiques= dans Fetes.txt)."));
}

void AFetesChelles::MorceauFini()
{
	if (FeteEnCours != EFeteChelles::FeteMusique)
	{
		return;
	}
	++Morceau;
	JouerMorceau();
}

void AFetesChelles::LancerMusique()
{
	FVector Sol;
	float Lacet = 0.f;
	if (!PlaceScene(Sol, Lacet))
	{
		// pas de place trouvee : devant le point de depart
		const FVector Centre = Depart();
		float Yaw = 0.f;
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			Yaw = It->GetActorRotation().Yaw;
			break;
		}
		Sol = Centre + FRotator(0.f, Yaw, 0.f).Vector() * 2000.f;
		if (!TrouverSol(Sol, Sol, false))
		{
			Sol = Centre;
		}
		Lacet = Yaw + 180.f;
		Message(TEXT("Pas de grande place libre trouvee : scene posee devant le depart. Placez-la vous-meme avec : Chelles.Fetes PoserScene"));
	}
	FabriquerScene(Sol, Lacet);
}

void AFetesChelles::AnimerMusique(float DeltaSeconds)
{
	// couleurs qui tournent au rythme
	const float Battement = 0.5f + 0.5f * FMath::Sin(Temps * 2.f * PI * 2.f); // 120 BPM
	for (int32 i = 0; i < Lumieres.Num(); ++i)
	{
		if (UMaterialInstanceDynamic* Mid = Lumieres[i])
		{
			const FLinearColor Teinte = FLinearColor::MakeFromHSV8(static_cast<uint8>(FMath::Fmod(Temps * 40.f + i * 90.f, 255.f)), 255, 255);
			Mid->SetVectorParameterValue(MateriauLumiere ? TEXT("Couleur") : TEXT("Color"), Teinte);
			if (MateriauLumiere)
			{
				Mid->SetScalarParameterValue(TEXT("Force"), 3.f + 6.f * Battement);
			}
		}
	}
	const bool bNuit = EstNuit();
	for (int32 i = 0; i < Projecteurs.Num(); ++i)
	{
		if (USpotLightComponent* Spot = Projecteurs[i])
		{
			const float Lacet = 35.f * FMath::Sin(Temps * (0.7f + 0.2f * i) + i * 1.3f);
			const float Tangage = -45.f + 12.f * FMath::Sin(Temps * (0.5f + 0.15f * i) + i);
			Spot->SetRelativeRotation(FRotator(Tangage, Lacet, 0.f));
			Spot->SetLightColor(FLinearColor::MakeFromHSV8(static_cast<uint8>(FMath::Fmod(Temps * 25.f + i * 64.f, 255.f)), 230, 255));
			Spot->SetIntensity((bNuit ? 60000.f : 15000.f) * (0.6f + 0.4f * Battement));
		}
	}
}

void AFetesChelles::PoserSceneDevantJoueur()
{
	FVector Vue;
	FRotator Direction;
	if (!PointDeVue(Vue, Direction))
	{
		return;
	}
	const FVector Devant = FRotator(0.f, Direction.Yaw, 0.f).Vector();
	FVector Sol = Vue + Devant * 1500.f;
	if (!TrouverSol(Sol, Sol, false))
	{
		Message(TEXT("Pas de sol devant vous : regardez vers une place degagee."), true);
		return;
	}
	const float Lacet = Direction.Yaw + 180.f; // la scene regarde le joueur
	const FString Texte = FString::Printf(TEXT("# Place de la scene (fete de la musique) : X,Y,Z,Lacet. Supprimez ce fichier pour la place automatique.\n%.0f,%.0f,%.0f,%.0f\n"),
		Sol.X, Sol.Y, Sol.Z, Lacet);
	FFileHelper::SaveStringToFile(Texte, *FPaths::Combine(Dossier(), TEXT("Scene_Position.txt")));
	Message(TEXT("Scene enregistree ici (Plugins\\ChellesFetes\\Scene_Position.txt)."), true);
	if (FeteEnCours == EFeteChelles::FeteMusique)
	{
		Lancer(EFeteChelles::FeteMusique);
	}
}

void AFetesChelles::RetirerSceneEnregistree()
{
	IFileManager::Get().Delete(*FPaths::Combine(Dossier(), TEXT("Scene_Position.txt")));
	Message(TEXT("Place de la scene oubliee : elle sera choisie automatiquement."), true);
	if (FeteEnCours == EFeteChelles::FeteMusique)
	{
		Lancer(EFeteChelles::FeteMusique);
	}
}

// ---------------------------------------------------------------- Tick / console

void AFetesChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;

	// Mode Auto : on regarde la date de temps en temps (partie lancee le 31/10 a 23h59...)
	if (ModeForce < 0 && FMath::Fmod(Temps, 60.f) < DeltaSeconds)
	{
		const EFeteChelles Voulue = FeteVoulue();
		if (Voulue != FeteEnCours)
		{
			Lancer(Voulue);
			return;
		}
	}

	switch (FeteEnCours)
	{
	case EFeteChelles::Halloween:
		AnimerHalloween(DeltaSeconds);
		break;
	case EFeteChelles::FeteMusique:
		AnimerMusique(DeltaSeconds);
		break;
	default:
		break;
	}
	// les feux peuvent aussi etre lances a la main (Chelles.Fetes Feux) pendant une autre fete
	AnimerFeux(DeltaSeconds);
}

void AFetesChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Action = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
	if (!Sphere)
	{
		Message(TEXT("Pas de fete sur cette carte (reglage Carte= dans Fetes.txt)."), true);
		return;
	}
	if (Action.Equals(TEXT("Halloween"), ESearchCase::IgnoreCase))
	{
		ModeForce = static_cast<int32>(EFeteChelles::Halloween);
		Lancer(EFeteChelles::Halloween);
	}
	else if (Action.Contains(TEXT("Juillet"), ESearchCase::IgnoreCase) || Action == TEXT("14"))
	{
		ModeForce = static_cast<int32>(EFeteChelles::QuatorzeJuillet);
		Lancer(EFeteChelles::QuatorzeJuillet);
	}
	else if (Action.Contains(TEXT("Musique"), ESearchCase::IgnoreCase))
	{
		ModeForce = static_cast<int32>(EFeteChelles::FeteMusique);
		Lancer(EFeteChelles::FeteMusique);
	}
	else if (Action.Equals(TEXT("Non"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))
	{
		ModeForce = static_cast<int32>(EFeteChelles::Aucune);
		Lancer(EFeteChelles::Aucune);
		Message(TEXT("Fetes arretees."), true);
	}
	else if (Action.Equals(TEXT("Auto"), ESearchCase::IgnoreCase))
	{
		ModeForce = -1;
		Lancer(FeteVoulue());
		Message(TEXT("Fete selon Fetes.txt / la date : ") + NomFete(FeteEnCours), true);
	}
	else if (Action.Equals(TEXT("Feux"), ESearchCase::IgnoreCase))
	{
		PreparerFeux();
		DemarrerSpectacle(Arguments.Num() > 1 ? FMath::Max(5.f, Nombre(Arguments[1])) : 30.f);
	}
	else if (Action.Equals(TEXT("PoserScene"), ESearchCase::IgnoreCase))
	{
		PoserSceneDevantJoueur();
	}
	else if (Action.Equals(TEXT("RetirerScene"), ESearchCase::IgnoreCase))
	{
		RetirerSceneEnregistree();
	}
	else if (Action.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		LireReglages();
		Lancer(FeteVoulue());
		Message(TEXT("Fetes.txt relu. Fete : ") + NomFete(FeteEnCours), true);
	}
	else
	{
		Message(FString::Printf(TEXT("Fete en cours : %s (date du jour : %s). %d citrouille(s) lumineuse(s), %d fantome(s), %d tete(s) de citrouille, %d etincelle(s), %d projecteur(s)."),
			*NomFete(FeteEnCours), *NomFete(FeteDuJour()), Bougies.Num(), Fantomes.Num(), Coiffes.Num(), Points.Num(), Projecteurs.Num()), true);
		Message(TEXT("Chelles.Fetes Halloween | Juillet | Musique | Non | Auto | Feux | PoserScene | RetirerScene | Relire"), true);
	}
}
