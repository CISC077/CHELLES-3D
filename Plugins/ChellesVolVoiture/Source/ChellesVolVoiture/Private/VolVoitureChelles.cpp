// CHELLES 3D - Vol de voiture facon GTA (voir VolVoitureChelles.h).
#include "VolVoitureChelles.h"
#include "FuyardChelles.h"
#include "RelaisVolVoiture.h"

#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "VehicleWheel.h"
#include "WheeledVehicleMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogVolVoiture, Log, All);

// Noms uniques (unity build : les plugins sont compiles par gros fichiers)
namespace VolVoitureOutils
{
	const FName TAG_NOTRE(TEXT("ChellesVolVoiture"));
	const FName TAG_POLICE(TEXT("ChellesVolPoursuite"));
	constexpr float CmsVersKmh = 0.036f;
	constexpr int32 CLE_AIDE = 89101;
	constexpr int32 CLE_ETOILES = 89102;
	constexpr int32 CLE_INFO = 89103;

	bool EstOuiVol(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		return T.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || T.Equals(TEXT("1")) || T.Equals(TEXT("True"), ESearchCase::IgnoreCase)
			|| T.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	FKey LireToucheVol(const FString& Nom, const FKey& Defaut)
	{
		const FKey Touche(*Nom.TrimStartAndEnd());
		return Touche.IsValid() ? Touche : Defaut;
	}

	UWheeledVehicleMovementComponent* RoulementVol(const AActor* Acteur)
	{
		return Acteur ? Acteur->FindComponentByClass<UWheeledVehicleMovementComponent>() : nullptr;
	}

	UPrimitiveComponent* CaisseVol(const AActor* Acteur)
	{
		return Acteur ? Cast<UPrimitiveComponent>(Acteur->GetRootComponent()) : nullptr;
	}

	/** "SM_Hatchback" -> "hatchback" ; "SK_Truck_Box" -> "truck_box" */
	FString NormaliserModele(const FString& Nom)
	{
		FString N = Nom.TrimStartAndEnd();
		if (N.StartsWith(TEXT("SM_"), ESearchCase::IgnoreCase) || N.StartsWith(TEXT("SK_"), ESearchCase::IgnoreCase))
		{
			N.RightChopInline(3);
		}
		return N.ToLower();
	}

	FString TexteEtoiles(int32 N, int32 Max)
	{
		FString T;
		for (int32 i = 0; i < Max; ++i)
		{
			T += (i < N) ? TEXT("* ") : TEXT(". ");
		}
		return T;
	}
}
using namespace VolVoitureOutils;

// =====================================================================================================
// Construction, reglages
// =====================================================================================================

AVolVoitureChelles::AVolVoitureChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetActorHiddenInGame(true);
	bReplicates = false; // un par machine

	ToucheVoler = EKeys::F;
	ToucheDescendre2 = EKeys::E;
	const FString VVP = TEXT("/Game/VehicleVarietyPack/Blueprints/");
	CheminsModeles.Add(TEXT("hatchback"), VVP + TEXT("Hatchback/BP_Hatchback.BP_Hatchback_C"));
	CheminsModeles.Add(TEXT("suv"), VVP + TEXT("SUV/BP_SUV.BP_SUV_C"));
	CheminsModeles.Add(TEXT("pickup"), VVP + TEXT("Pickup/BP_Pickup.BP_Pickup_C"));
	CheminsModeles.Add(TEXT("sportscar"), VVP + TEXT("SportsCar/BP_SportsCar.BP_SportsCar_C"));
	CheminsModeles.Add(TEXT("truck_box"), VVP + TEXT("BoxTruck/BP_BoxTruck.BP_BoxTruck_C"));
	CheminsSecours = {
		TEXT("/Game/SportVehiclesPack/Blueprints/Vehicles/BP_WheeledVehicle_Sportcar1.BP_WheeledVehicle_Sportcar1_C"),
		TEXT("/Game/SportVehiclesPack/Blueprints/Vehicles/BP_WheeledVehicle_Sportcar2.BP_WheeledVehicle_Sportcar2_C"),
		TEXT("/Game/Vehicles/VH_Buggy/Blueprint/VH_Buggy.VH_Buggy_C")
	};
	// 25/11 : autos garees = pack DV_Car_Horo (3 sur 5) et les 2 Bugatti de Rachid (1 sur 5 chacune)
	CheminsGarees.Add(TPair<FString, int32>(TEXT("/Game/DV_Car_Horo/Blueprints/BP_Car_Hatchback_Horo.BP_Car_Hatchback_Horo_C"), 3));
	CheminsGarees.Add(TPair<FString, int32>(TEXT("/Game/Chelles/Bugatti/BP_Bugatti_Chelles.BP_Bugatti_Chelles_C"), 1));
	CheminsGarees.Add(TPair<FString, int32>(TEXT("/Game/Chelles/Bugatti/BP_BugattiBrouillard_Chelles.BP_BugattiBrouillard_Chelles_C"), 1));
	OptionsHoro.Add(TEXT("bUse_Low_Rider_?"), true);
	OptionsHoro.Add(TEXT("bUse_Can_Be_Repaired_?"), true);
	OptionsHoro.Add(TEXT("bUse_Random_Licence_Plate_?"), false);
	OptionsHoro.Add(TEXT("bUse_Fuel_?"), false);
	OptionsHoro.Add(TEXT("bUse_Car_Locked_?"), false);
	OptionsHoro.Add(TEXT("bUse_Car_Alarm_Activated_?"), false);
	ToucheActions[EActionHoro::Phares] = EKeys::L;
	ToucheActions[EActionHoro::Saut] = EKeys::R;
	ToucheActions[EActionHoro::Reinitialiser] = EKeys::BackSpace;
	ToucheActions[EActionHoro::ClignotantGauche] = EKeys::G;
	ToucheActions[EActionHoro::ClignotantDroit] = EKeys::U;
	ToucheActions[EActionHoro::Warnings] = EKeys::V;
	const TCHAR* NomsCheminsRoues[4] = { TEXT("AVG"), TEXT("AVD"), TEXT("ARG"), TEXT("ARD") };
	const TCHAR* Prefixes[NB_HABILLAGES] = { TEXT("SM_Bugatti_"), TEXT("SM_BugattiBrouillard_") };
	for (int32 h = 0; h < NB_HABILLAGES; ++h)
	{
		const FString B = FString(TEXT("/Game/Chelles/Bugatti/")) + Prefixes[h];
		CheminsBugatti[h][0] = B + TEXT("Carrosserie.") + Prefixes[h] + TEXT("Carrosserie");
		for (int32 k = 0; k < 4; ++k)
		{
			CheminsBugatti[h][k + 1] = B + TEXT("Roue_") + NomsCheminsRoues[k] + TEXT(".") + Prefixes[h] + TEXT("Roue_") + NomsCheminsRoues[k];
		}
	}
	// 2e Bugatti (couleur Brouillard, 25/11) : decoupee par Claude, centres et rayons de ses propres roues
	{
		FHabillageBugatti& H = Habillages[1];
		H.Centres[0] = FVector(125.f, -95.37f, 35.75f);
		H.Centres[1] = FVector(125.f, 94.99f, 35.75f);
		H.Centres[2] = FVector(-141.75f, -93.11f, 38.f);
		H.Centres[3] = FVector(-141.75f, 93.46f, 38.f);
		H.RayonAvant = 34.75f;
		H.RayonArriere = 36.75f;
		H.PlaqueAvant = FVector(222.5f, 0.f, 48.f); // logo rouge de la calandre
	}
	CheminPolice = TEXT("/Game/SportVehiclesPack/Blueprints/Vehicles/BP_WheeledVehicle_Sportcar1_Police.BP_WheeledVehicle_Sportcar1_Police_C");
	CheminFuyard = TEXT("/Game/Hijacker/Mesh/SK_hijacker.SK_hijacker");
	CheminAnimFuyard = TEXT("/Game/Hijacker/Animations/ThirdPersonRun.ThirdPersonRun");
}

void AVolVoitureChelles::LireReglages()
{
	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesVolVoiture"), TEXT("VolVoiture.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogVolVoiture, Warning, TEXT("[Vol] %s introuvable : reglages par defaut."), *Chemin);
		return;
	}
	bool bSecoursLu = false;
	bool bGareesLu = false;
	auto LireVecteur = [](const FString& V, const FVector& Defaut)
	{
		TArray<FString> P;
		V.Replace(TEXT(";"), TEXT(" ")).ParseIntoArray(P, TEXT(" "), true);
		if (P.Num() < 3)
		{
			return Defaut;
		}
		return FVector(FCString::Atof(*P[0].Replace(TEXT(","), TEXT("."))), FCString::Atof(*P[1].Replace(TEXT(","), TEXT("."))),
			FCString::Atof(*P[2].Replace(TEXT(","), TEXT("."))));
	};
	const TCHAR* NomsTouches[EActionHoro::Nombre] = { TEXT("TouchePhares"), TEXT("ToucheSaut"), TEXT("ToucheReinitialiser"),
		TEXT("ToucheClignotantGauche"), TEXT("ToucheClignotantDroit"), TEXT("ToucheWarnings") };
	const TCHAR* NomsRoues[4] = { TEXT("AVG"), TEXT("AVD"), TEXT("ARG"), TEXT("ARD") };
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
		const float Nombre = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));
		auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };

		if (Cle.StartsWith(TEXT("Modele."), ESearchCase::IgnoreCase))
		{
			const FString Nom = NormaliserModele(Cle.RightChop(7));
			if (!Nom.IsEmpty())
			{
				CheminsModeles.Add(Nom, Valeur);
			}
		}
		else if (Est(TEXT("Secours")))
		{
			if (!bSecoursLu)
			{
				CheminsSecours.Reset();
				bSecoursLu = true;
			}
			TArray<FString> Noms;
			Valeur.ParseIntoArray(Noms, TEXT(","), true);
			for (const FString& N : Noms)
			{
				if (!N.TrimStartAndEnd().IsEmpty())
				{
					CheminsSecours.Add(N.TrimStartAndEnd());
				}
			}
		}
		else if (Est(TEXT("Garees")))
		{
			// Liste "chemin *poids, chemin *poids" (poids = combien de fois plus souvent)
			if (!bGareesLu)
			{
				CheminsGarees.Reset();
				bGareesLu = true;
			}
			TArray<FString> Noms;
			Valeur.ParseIntoArray(Noms, TEXT(","), true);
			for (FString N : Noms)
			{
				int32 Poids = 1;
				FString CheminGaree = N, Nombre2;
				if (N.Split(TEXT("*"), &CheminGaree, &Nombre2, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
				{
					Poids = FMath::Clamp(FCString::Atoi(*Nombre2.TrimStartAndEnd()), 0, 20);
				}
				CheminGaree.TrimStartAndEndInline();
				if (!CheminGaree.IsEmpty() && Poids > 0)
				{
					CheminsGarees.Add(TPair<FString, int32>(CheminGaree, Poids));
				}
			}
		}
		else if (Cle.StartsWith(TEXT("Option."), ESearchCase::IgnoreCase))
		{
			OptionsHoro.Add(Cle.RightChop(7).TrimStartAndEnd(), EstOuiVol(Valeur));
		}
		else if (Est(TEXT("Plaque")))
		{
			TextePlaque = Valeur.Left(16); // plaques du pack (les Bugatti ont leur cle Bugatti.Plaque)
		}
		else if (Est(TEXT("CameraDuPack")))       { bCameraPack = EstOuiVol(Valeur); }
		else if (Cle.StartsWith(TEXT("Bugatti."), ESearchCase::IgnoreCase) || Cle.StartsWith(TEXT("BugattiBrouillard."), ESearchCase::IgnoreCase))
		{
			// "Bugatti.X" = 1re Bugatti, "BugattiBrouillard.X" = 2e
			const int32 h = Cle.StartsWith(TEXT("BugattiBrouillard."), ESearchCase::IgnoreCase) ? 1 : 0;
			FString Sous;
			Cle.Split(TEXT("."), nullptr, &Sous);
			Sous.TrimStartAndEndInline();
			FHabillageBugatti& H = Habillages[h];
			auto EstS = [&Sous](const TCHAR* Nom) { return Sous.Equals(Nom, ESearchCase::IgnoreCase); };
			if (EstS(TEXT("Carrosserie")))       { CheminsBugatti[h][0] = Valeur; }
			else if (EstS(TEXT("RayonAvant")))   { H.RayonAvant = FMath::Clamp(Nombre, 5.f, 100.f); }
			else if (EstS(TEXT("RayonArriere"))) { H.RayonArriere = FMath::Clamp(Nombre, 5.f, 100.f); }
			else if (EstS(TEXT("Largeur")))      { H.EchelleLargeur = FMath::Clamp(Nombre, 0.5f, 2.f); }
			else if (EstS(TEXT("Hauteur")))      { H.EchelleHauteur = FMath::Clamp(Nombre, 0.5f, 2.f); }
			else if (EstS(TEXT("Decalage")))     { H.Decalage = LireVecteur(Valeur, FVector::ZeroVector); }
			else if (EstS(TEXT("Rotation")))     { H.Lacet = FMath::Clamp(Nombre, -360.f, 360.f); }
			else if (EstS(TEXT("Plaque")))       { H.TextePlaque = Valeur.Left(16); }
			else if (EstS(TEXT("PlaqueAvant")))  { H.PlaqueAvant = LireVecteur(Valeur, H.PlaqueAvant); }
			else if (EstS(TEXT("PlaqueArriere"))) { H.PlaqueArriere = LireVecteur(Valeur, H.PlaqueArriere); }
			else
			{
				for (int32 k = 0; k < 4; ++k)
				{
					if (EstS(*FString::Printf(TEXT("Roue%s"), NomsRoues[k])))   { CheminsBugatti[h][k + 1] = Valeur; }
					if (EstS(*FString::Printf(TEXT("Centre%s"), NomsRoues[k]))) { H.Centres[k] = LireVecteur(Valeur, H.Centres[k]); }
				}
			}
		}
		else if (Cle.StartsWith(TEXT("Touche"), ESearchCase::IgnoreCase) && !Est(TEXT("ToucheVoler")) && !Est(TEXT("ToucheDescendre")))
		{
			for (int32 k = 0; k < EActionHoro::Nombre; ++k)
			{
				if (Est(NomsTouches[k]))
				{
					ToucheActions[k] = LireToucheVol(Valeur, ToucheActions[k]);
				}
			}
		}
		else if (Est(TEXT("ToucheVoler")))        { ToucheVoler = LireToucheVol(Valeur, EKeys::F); }
		else if (Est(TEXT("ToucheDescendre")))    { ToucheDescendre2 = LireToucheVol(Valeur, EKeys::E); }
		else if (Est(TEXT("DistanceVol")))        { DistanceVol = FMath::Clamp(Nombre, 1.f, 15.f) * 100.f; }
		else if (Est(TEXT("VoiturePolice")))      { CheminPolice = Valeur; }
		else if (Est(TEXT("ConducteurEjecte")))   { bFuyard = EstOuiVol(Valeur); }
		else if (Est(TEXT("ModeleConducteur")))   { CheminFuyard = Valeur; }
		else if (Est(TEXT("CourseConducteur")))   { CheminAnimFuyard = Valeur; }
		else if (Est(TEXT("AutosGarees")))        { NombreGarees = FMath::Clamp(FMath::RoundToInt(Nombre), 0, 40); }
		else if (Est(TEXT("RayonGarees")))        { RayonGarees = FMath::Clamp(Nombre, 50.f, 1000.f) * 100.f; }
		else if (Est(TEXT("MaxVoitures")))        { MaxVoitures = FMath::Clamp(FMath::RoundToInt(Nombre), 2, 80); }
		else if (Est(TEXT("EcartTrottoir")))      { EcartTrottoir = FMath::Clamp(Nombre, 0.f, 10.f) * 100.f; }
		else if (Est(TEXT("Recherche")))          { bRecherche = EstOuiVol(Valeur); }
		else if (Est(TEXT("EtoilesParVol")))      { EtoilesParVol = FMath::Clamp(FMath::RoundToInt(Nombre), 0, 5); }
		else if (Est(TEXT("EtoilesMax")))         { EtoilesMax = FMath::Clamp(FMath::RoundToInt(Nombre), 1, 5); }
		else if (Est(TEXT("PoliceParEtoile")))    { PoliceParEtoile = FMath::Clamp(FMath::RoundToInt(Nombre), 0, 4); }
		else if (Est(TEXT("PoliceMax")))          { PoliceMax = FMath::Clamp(FMath::RoundToInt(Nombre), 0, 8); }
		else if (Est(TEXT("DureeEtoile")))        { DureeEtoile = FMath::Clamp(Nombre, 3.f, 300.f); }
		else if (Est(TEXT("DistanceVuePolice")))  { DistanceVuePolice = FMath::Clamp(Nombre, 20.f, 500.f) * 100.f; }
		else if (Est(TEXT("VitessePolice")))      { VitessePolice = FMath::Clamp(Nombre, 20.f, 250.f); }
		else if (Est(TEXT("DistanceCamera")))     { DistanceCamera = FMath::Clamp(Nombre, 1.f, 30.f) * 100.f; }
		else if (Est(TEXT("HauteurCamera")))      { HauteurCamera = FMath::Clamp(Nombre, 0.f, 10.f) * 100.f; }
		else if (Est(TEXT("AideConduite")))       { bAide = EstOuiVol(Valeur); }
		else if (Est(TEXT("Adherence")))          { Adherence = FMath::Clamp(Nombre, 0.f, 30.f); }
	}
}

void AVolVoitureChelles::ChargerClasses()
{
	bClassesChargees = true;
	ClassesModeles.Reset();
	ClassesSecours.Reset();
	int32 Trouvees = 0;
	for (const TPair<FString, FString>& Paire : CheminsModeles)
	{
		if (UClass* C = LoadClass<APawn>(nullptr, *Paire.Value))
		{
			ClassesModeles.Add(Paire.Key, C);
			++Trouvees;
		}
		else
		{
			UE_LOG(LogVolVoiture, Warning, TEXT("[Vol] Modele %s : Blueprint introuvable (%s)"), *Paire.Key, *Paire.Value);
		}
	}
	for (const FString& Chemin : CheminsSecours)
	{
		if (UClass* C = LoadClass<APawn>(nullptr, *Chemin))
		{
			ClassesSecours.AddUnique(C);
			++Trouvees;
		}
		else
		{
			UE_LOG(LogVolVoiture, Warning, TEXT("[Vol] Voiture de secours introuvable : %s"), *Chemin);
		}
	}
	ClassesGarees.Reset();
	for (const TPair<FString, int32>& Paire : CheminsGarees)
	{
		if (UClass* C = LoadClass<APawn>(nullptr, *Paire.Key))
		{
			for (int32 i = 0; i < Paire.Value; ++i)
			{
				ClassesGarees.Add(C);
			}
			++Trouvees;
		}
		else
		{
			UE_LOG(LogVolVoiture, Warning, TEXT("[Vol] Auto garee introuvable : %s (lancez Tools\\VoituresHoro\\INSTALLER_VOITURES_HORO.bat)"), *Paire.Key);
		}
	}
	// Bugatti de Rachid (meshes charges ici une fois, pour ne pas geler le jeu plus tard)
	ObjetsBugatti.Reset();
	for (int32 h = 0; h < NB_HABILLAGES; ++h)
	{
		FHabillageBugatti& H = Habillages[h];
		H.Carrosserie = nullptr;
		for (int32 k = 0; k < 4; ++k)
		{
			H.Roues[k] = nullptr;
		}
		if (GetNetMode() == NM_DedicatedServer)
		{
			continue;
		}
		for (int32 i = 0; i < 5; ++i)
		{
			UStaticMesh* M = CheminsBugatti[h][i].IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *CheminsBugatti[h][i]);
			if (M)
			{
				ObjetsBugatti.Add(M);
			}
			if (i == 0)
			{
				H.Carrosserie = M;
			}
			else
			{
				H.Roues[i - 1] = M;
			}
		}
		if (!H.Carrosserie)
		{
			UE_LOG(LogVolVoiture, Log, TEXT("[Vol] Bugatti %d pas encore importee (%s)."), h + 1, *CheminsBugatti[h][0]);
		}
	}
	ClassePolice = bRecherche && !CheminPolice.IsEmpty() ? LoadClass<APawn>(nullptr, *CheminPolice) : nullptr;
	if (bFuyard && GetNetMode() != NM_DedicatedServer)
	{
		MeshFuyard = LoadObject<USkeletalMesh>(nullptr, *CheminFuyard);
		AnimFuyard = LoadObject<UAnimationAsset>(nullptr, *CheminAnimFuyard);
	}
	UE_LOG(LogVolVoiture, Log, TEXT("[Vol] %d voitures conduites chargees, police : %s, conducteur ejecte : %s."), Trouvees,
		ClassePolice ? *ClassePolice->GetName() : TEXT("aucune"), MeshFuyard ? *MeshFuyard->GetName() : TEXT("aucun"));
}

UClass* AVolVoitureChelles::ClassePourModele(const FString& Modele)
{
	const FString N = NormaliserModele(Modele);
	if (UClass** Exacte = ClassesModeles.Find(N))
	{
		return *Exacte;
	}
	for (const TPair<FString, UClass*>& Paire : ClassesModeles)
	{
		if (Paire.Value && !Paire.Key.IsEmpty() && N.Contains(Paire.Key))
		{
			return Paire.Value;
		}
	}
	TArray<UClass*> Toutes = ClassesSecours;
	if (Toutes.Num() == 0)
	{
		ClassesModeles.GenerateValueArray(Toutes);
	}
	return Toutes.Num() > 0 ? Toutes[FMath::RandRange(0, Toutes.Num() - 1)] : nullptr;
}

void AVolVoitureChelles::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	ChargerClasses();
	UE_LOG(LogVolVoiture, Log, TEXT("[Vol] Pret (%s). Touche %s = voler / monter / descendre."),
		GetNetMode() == NM_Standalone ? TEXT("solo") : (EstServeur() ? TEXT("serveur") : TEXT("client")), *ToucheVoler.ToString());
}

void AVolVoitureChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerCamera();
	Super::EndPlay(Raison);
}

// =====================================================================================================
// Reseau
// =====================================================================================================

bool AVolVoitureChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

APlayerController* AVolVoitureChelles::PCLocal() const
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

ARelaisVolVoiture* AVolVoitureChelles::MonRelais() const
{
	APlayerController* PC = PCLocal();
	if (!PC)
	{
		return nullptr;
	}
	for (TActorIterator<ARelaisVolVoiture> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

void AVolVoitureChelles::GererRelaisServeur()
{
	UWorld* Monde = GetWorld();
	TArray<APlayerController*> DejaServis;
	TArray<ARelaisVolVoiture*> Relais;
	for (TActorIterator<ARelaisVolVoiture> It(Monde); It; ++It)
	{
		Relais.Add(*It);
	}
	for (ARelaisVolVoiture* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			// Joueur parti : son pieton cache disparait avec son relais
			if (IsValid(R->PietonCache))
			{
				R->PietonCache->Destroy();
			}
			R->Destroy();
			continue;
		}
		DejaServis.Add(PC);
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
		Monde->SpawnActor<ARelaisVolVoiture>(ARelaisVolVoiture::StaticClass(), FTransform::Identity, Params);
	}
}

void AVolVoitureChelles::Message(const FString& Texte, float Duree, int32 Cle) const
{
	UE_LOG(LogVolVoiture, Log, TEXT("[Vol] %s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(Cle, Duree, FColor(255, 190, 60), Texte);
	}
}

void AVolVoitureChelles::Avis(APlayerController* PC, const FString& Texte, float Duree) const
{
	if (!PC)
	{
		return;
	}
	if (PC->IsLocalController())
	{
		Message(Texte, Duree);
		return;
	}
	for (TActorIterator<ARelaisVolVoiture> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			It->ClientAvis(Texte, Duree);
			return;
		}
	}
}

// =====================================================================================================
// Voitures
// =====================================================================================================

UClass* AVolVoitureChelles::ClasseCirculation()
{
	if (!ClasseCirc)
	{
		ClasseCirc = FindObject<UClass>(nullptr, TEXT("/Script/ChellesCirculation.VehiculeCirculation"));
	}
	return ClasseCirc;
}

AActor* AVolVoitureChelles::VoitureCirculationProche(const APawn* Pieton, float Rayon)
{
	UClass* Classe = ClasseCirculation();
	if (!Classe || !Pieton)
	{
		return nullptr;
	}
	AActor* Meilleure = nullptr;
	float MeilleureDistance = FMath::Square(Rayon + 220.f); // + demi-longueur de la voiture
	const FVector Ici = Pieton->GetActorLocation();
	for (TActorIterator<AActor> It(GetWorld(), Classe); It; ++It)
	{
		AActor* V = *It;
		if (!IsValid(V) || V->IsHidden())
		{
			continue;
		}
		const float D = FVector::DistSquared(V->GetActorLocation(), Ici);
		if (D < MeilleureDistance)
		{
			MeilleureDistance = D;
			Meilleure = V;
		}
	}
	return Meilleure;
}

FString AVolVoitureChelles::ModeleDe(const AActor* VoitureCirculation)
{
	if (!VoitureCirculation)
	{
		return FString();
	}
	TArray<UStaticMeshComponent*> Statiques;
	VoitureCirculation->GetComponents<UStaticMeshComponent>(Statiques);
	for (const UStaticMeshComponent* C : Statiques)
	{
		if (C && C->GetStaticMesh() && C->IsVisible() && !C->GetStaticMesh()->GetName().StartsWith(TEXT("Cube")))
		{
			return C->GetStaticMesh()->GetName();
		}
	}
	TArray<USkeletalMeshComponent*> Squelettes;
	VoitureCirculation->GetComponents<USkeletalMeshComponent>(Squelettes);
	for (const USkeletalMeshComponent* C : Squelettes)
	{
		if (C && C->SkeletalMesh)
		{
			return C->SkeletalMesh->GetName();
		}
	}
	return FString();
}

bool AVolVoitureChelles::EstNotreVoiture(const AActor* Acteur) const
{
	if (!IsValid(Acteur) || !RoulementVol(Acteur))
	{
		return false;
	}
	if (Acteur->ActorHasTag(TAG_NOTRE))
	{
		return true; // serveur
	}
	// Les tags ne sont pas copies par le reseau : chez les clients on reconnait la classe
	UClass* Classe = Acteur->GetClass();
	if (ClassePolice && Classe->IsChildOf(ClassePolice))
	{
		return false;
	}
	for (const TPair<FString, UClass*>& Paire : ClassesModeles)
	{
		if (Paire.Value && Classe->IsChildOf(Paire.Value))
		{
			return true;
		}
	}
	for (const UClass* C : ClassesSecours)
	{
		if (C && Classe->IsChildOf(C))
		{
			return true;
		}
	}
	for (const UClass* C : ClassesGarees)
	{
		if (C && Classe->IsChildOf(C))
		{
			return true;
		}
	}
	return false;
}

bool AVolVoitureChelles::EstConduiteParJoueur(const APawn* Voiture) const
{
	if (!IsValid(Voiture))
	{
		return false;
	}
	if (Voiture->GetPlayerState() != nullptr)
	{
		return true;
	}
	const AController* C = Voiture->GetController();
	return C && C->IsPlayerController();
}

APawn* AVolVoitureChelles::NotreVoitureLibreProche(const APawn* Pieton, float Rayon) const
{
	if (!Pieton)
	{
		return nullptr;
	}
	APawn* Meilleure = nullptr;
	float MeilleureDistance = FMath::Square(Rayon + 220.f);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* V = *It;
		if (V == Pieton || V->IsHidden() || EstConduiteParJoueur(V) || !EstNotreVoiture(V))
		{
			continue;
		}
		const float D = FVector::DistSquared(V->GetActorLocation(), Pieton->GetActorLocation());
		if (D < MeilleureDistance)
		{
			MeilleureDistance = D;
			Meilleure = V;
		}
	}
	return Meilleure;
}

APawn* AVolVoitureChelles::FaireApparaitre(UClass* Classe, const FVector& Lieu, float Yaw, bool bGaree)
{
	UWorld* Monde = GetWorld();
	if (!Classe || !Monde)
	{
		return nullptr;
	}
	// Pose sur le sol (pas sur un pieton ni sur une autre voiture)
	FVector Pose = Lieu;
	FHitResult Sol;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VolVoitureSol), false, this);
	FCollisionObjectQueryParams Objets;
	Objets.AddObjectTypesToQuery(ECC_WorldStatic);
	if (Monde->LineTraceSingleByObjectType(Sol, Lieu + FVector(0.f, 0.f, 300.f), Lieu - FVector(0.f, 0.f, 800.f), Objets, Params))
	{
		Pose = Sol.ImpactPoint;
	}
	else if (bGaree)
	{
		return nullptr; // pas de sol sous la place : on ne gare pas dans le vide
	}
	Pose.Z += 60.f;

	// 25/11/2026 (agent Terrain) : place deja prise (trottoir, arbre, autre voiture) ? On ne tente pas :
	// avant, Unreal essayait quand meme, echouait et ecrivait 2 avertissements a chaque fois.
	if (bGaree)
	{
		FCollisionQueryParams Place(SCENE_QUERY_STAT(VolVoiturePlace), false, this);
		const FVector Centre = Pose + FVector(0.f, 0.f, 70.f);
		if (Monde->OverlapBlockingTestByChannel(Centre, FRotator(0.f, Yaw, 0.f).Quaternion(), ECC_Vehicle,
			FCollisionShape::MakeBox(FVector(240.f, 105.f, 60.f)), Place))
		{
			return nullptr;
		}
	}

	FActorSpawnParameters Apparition;
	Apparition.SpawnCollisionHandlingOverride = bGaree ? ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding
		: ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FTransform Place(FRotator(0.f, Yaw, 0.f), Pose);
	APawn* V = Monde->SpawnActorDeferred<APawn>(Classe, Place, nullptr, nullptr, Apparition.SpawnCollisionHandlingOverride);
	if (!V)
	{
		return nullptr;
	}
	// 25/11 : voiture DV_Car_Horo : ses options (saut, reparation, plaque fixe, pas de panne d'essence...) avant son BeginPlay
	if (HoroChelles::EstHoro(V))
	{
		HoroChelles::ReglerOptions(V, OptionsHoro);
	}
	V->FinishSpawning(Place);
	if (!IsValid(V))
	{
		return nullptr;
	}
	// Une voiture a physique ne doit pas etre tournee par la souris (sinon elle se tord / saute)
	V->bUseControllerRotationYaw = false;
	V->bUseControllerRotationPitch = false;
	V->bUseControllerRotationRoll = false;
	V->Tags.AddUnique(TAG_NOTRE);
	if (bGaree)
	{
		ArreterVoiture(V);
	}
	NosVoitures.Add(V);

	// Trop de voitures : on retire la plus lointaine qui n'est pas conduite et qui est hors de vue
	NosVoitures.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !P.IsValid(); });
	if (NosVoitures.Num() > MaxVoitures)
	{
		APawn* PlusLoin = nullptr;
		float DistMax = 0.f;
		for (const TWeakObjectPtr<APawn>& P : NosVoitures)
		{
			APawn* Autre = P.Get();
			if (!Autre || Autre == V || EstConduiteParJoueur(Autre) || !LoinDesJoueurs(Autre->GetActorLocation(), 3000.f, true))
			{
				continue;
			}
			float DMin = BIG_NUMBER;
			for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
			{
				if (APawn* J = It->Get() ? It->Get()->GetPawn() : nullptr)
				{
					DMin = FMath::Min(DMin, FVector::Dist(J->GetActorLocation(), Autre->GetActorLocation()));
				}
			}
			if (DMin > DistMax)
			{
				DistMax = DMin;
				PlusLoin = Autre;
			}
		}
		if (PlusLoin)
		{
			PlusLoin->Destroy();
		}
	}
	return V;
}

void AVolVoitureChelles::RetirerCirculationPres(const FVector& Lieu, float Rayon)
{
	UClass* Classe = ClasseCirculation();
	if (!Classe)
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld(), Classe); It; ++It)
	{
		if (IsValid(*It) && FVector::DistSquared(It->GetActorLocation(), Lieu) < FMath::Square(Rayon))
		{
			It->Destroy();
		}
	}
}

void AVolVoitureChelles::EjecterConducteur(const FVector& Lieu, float Yaw)
{
	if (!bFuyard || !MeshFuyard || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Il sort cote conducteur (a gauche) et s'enfuit en courant vers le trottoir, un peu en arriere
	const FRotator Rot(0.f, Yaw, 0.f);
	const FVector Gauche = -FRotationMatrix(Rot).GetUnitAxis(EAxis::Y);
	const FVector Depart = Lieu + Gauche * 150.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AFuyardChelles* F = GetWorld()->SpawnActor<AFuyardChelles>(AFuyardChelles::StaticClass(),
		FTransform(FRotator(0.f, Yaw - 120.f, 0.f), Depart), Params))
	{
		F->Lancer(MeshFuyard, AnimFuyard);
	}
}

// =====================================================================================================
// Serveur : vol, monter, descendre
// =====================================================================================================

void AVolVoitureChelles::VolerServeur(ARelaisVolVoiture* R, const FVector& Lieu, float Yaw, const FString& Modele)
{
	APlayerController* PC = R ? Cast<APlayerController>(R->GetOwner()) : nullptr;
	APawn* Pieton = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Cast<ACharacter>(Pieton) || Pieton->IsHidden() || IsValid(R->Voiture))
	{
		return;
	}
	if (FVector::Dist(Pieton->GetActorLocation(), Lieu) > DistanceVol + 800.f)
	{
		Avis(PC, TEXT("Approche-toi de la voiture."));
		return;
	}
	UClass* Classe = ClassePourModele(Modele);
	if (!Classe)
	{
		Avis(PC, TEXT("Aucune voiture conduite trouvee : voir Plugins\\ChellesVolVoiture\\VolVoiture.txt"));
		return;
	}
	// La circulation est locale a chaque machine : on retire aussi la copie du serveur
	if (GetNetMode() != NM_DedicatedServer)
	{
		RetirerCirculationPres(Lieu, 350.f);
		if (!PC->IsLocalController())
		{
			EjecterConducteur(Lieu, Yaw);
		}
	}
	APawn* V = FaireApparaitre(Classe, Lieu, Yaw, false);
	if (!V)
	{
		Avis(PC, TEXT("Impossible de voler cette voiture ici (voir le journal [Vol])."));
		return;
	}
	UE_LOG(LogVolVoiture, Log, TEXT("[Vol] %s vole une %s (modele %s)."), *PC->GetName(), *Classe->GetName(), *Modele);
	MonterServeur(R, V, true);
}

void AVolVoitureChelles::MonterServeur(ARelaisVolVoiture* R, APawn* V, bool bAvecEtoile)
{
	APlayerController* PC = R ? Cast<APlayerController>(R->GetOwner()) : nullptr;
	APawn* Pieton = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Cast<ACharacter>(Pieton) || Pieton->IsHidden() || !IsValid(V) || !EstNotreVoiture(V) || IsValid(R->Voiture))
	{
		return;
	}
	if (EstConduiteParJoueur(V))
	{
		Avis(PC, TEXT("Quelqu'un conduit deja cette voiture."));
		return;
	}
	if (FVector::Dist(Pieton->GetActorLocation(), V->GetActorLocation()) > DistanceVol + 800.f)
	{
		Avis(PC, TEXT("Approche-toi de la voiture."));
		return;
	}
	if (AController* Ancien = V->GetController())
	{
		Ancien->UnPossess();
	}
	CacherPion(Pieton, true);
	R->PietonCache = Pieton;
	R->Voiture = V;
	PC->Possess(V);
	const FRotator Rot(-12.f, V->GetActorRotation().Yaw, 0.f);
	PC->SetControlRotation(Rot);
	PC->ClientSetRotation(Rot);
	if (UWheeledVehicleMovementComponent* M = RoulementVol(V))
	{
		M->SetHandbrakeInput(false);
	}
	R->ForceNetUpdate();

	if (bAvecEtoile)
	{
		AjouterEtoiles(R, EtoilesParVol);
	}
	else
	{
		// Voiture garee : seulement si une voiture de police est tout pres
		for (const FPoursuivantVol& P : Poursuivants)
		{
			if (IsValid(P.Voiture) && FVector::Dist(P.Voiture->GetActorLocation(), V->GetActorLocation()) < 5000.f)
			{
				AjouterEtoiles(R, 1);
				break;
			}
		}
	}
}

void AVolVoitureChelles::DescendreServeur(ARelaisVolVoiture* R)
{
	APlayerController* PC = R ? Cast<APlayerController>(R->GetOwner()) : nullptr;
	if (!PC)
	{
		return;
	}
	APawn* V = R->Voiture;
	APawn* Pieton = R->PietonCache;
	if (!IsValid(Pieton))
	{
		Avis(PC, TEXT("Plus de personnage a pied : utilise la roue TAB > Soldat."));
		return;
	}
	if (IsValid(V) && PC->GetPawn() == V)
	{
		const float Yaw = V->GetActorRotation().Yaw;
		Pieton->SetActorLocationAndRotation(PlaceADescendre(V, Pieton), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		CacherPion(Pieton, false);
		PC->Possess(Pieton);
		PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
		PC->ClientSetRotation(FRotator(0.f, Yaw, 0.f));
		ArreterVoiture(V);
	}
	R->PietonCache = nullptr;
	R->Voiture = nullptr;
	R->ForceNetUpdate();
}

void AVolVoitureChelles::RendreLePietonALaRoue(APlayerController* PC, APawn* Voiture, APawn* Pieton)
{
	// La roue TAB a change de mode pendant qu'on conduisait : elle a pu garder la voiture comme "personnage cache".
	// On lui rend le vrai pieton (toutes ses proprietes qui pointaient sur la voiture), et la voiture reste garee.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->GetClass()->GetName().StartsWith(TEXT("Roue")))
		{
			continue;
		}
		if (A->GetOwner() && A->GetOwner() != PC)
		{
			continue;
		}
		for (TFieldIterator<FObjectPropertyBase> P(A->GetClass()); P; ++P)
		{
			if (P->ArrayDim == 1 && P->GetObjectPropertyValue_InContainer(A) == Voiture && IsValid(Pieton))
			{
				P->SetObjectPropertyValue_InContainer(A, Pieton);
				UE_LOG(LogVolVoiture, Log, TEXT("[Vol] Roue : %s.%s rendu au pieton."), *A->GetName(), *P->GetName());
			}
		}
		A->ForceNetUpdate();
	}
	CacherPion(Voiture, false);
	ArreterVoiture(Voiture);
}

void AVolVoitureChelles::SuivreConducteurs()
{
	for (TActorIterator<ARelaisVolVoiture> It(GetWorld()); It; ++It)
	{
		ARelaisVolVoiture* R = *It;
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC || (!R->Voiture && !R->PietonCache))
		{
			continue;
		}
		APawn* V = R->Voiture;
		APawn* Pieton = R->PietonCache;
		APawn* Actuel = PC->GetPawn();
		if (IsValid(V) && Actuel == V)
		{
			// Le pieton cache suit la voiture (il redescend au bon endroit, et reste pres du joueur pour le reseau)
			if (IsValid(Pieton))
			{
				Pieton->SetActorLocation(V->GetActorLocation() + FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			continue;
		}
		if (Actuel == nullptr && IsValid(Pieton))
		{
			// Voiture detruite : le joueur reprend son pieton
			if (IsValid(V))
			{
				Pieton->SetActorLocation(PlaceADescendre(V, Pieton), false, nullptr, ETeleportType::TeleportPhysics);
			}
			CacherPion(Pieton, false);
			PC->Possess(Pieton);
		}
		else if (Actuel && Actuel != Pieton && IsValid(V))
		{
			RendreLePietonALaRoue(PC, V, Pieton);
		}
		R->Voiture = nullptr;
		R->PietonCache = nullptr;
		R->ForceNetUpdate();
	}
}

// =====================================================================================================
// Serveur : autos garees au bord des rues (loin des yeux, retirees quand on s'eloigne)
// =====================================================================================================

bool AVolVoitureChelles::LoinDesJoueurs(const FVector& Point, float Distance, bool bHorsDeVue) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* J = PC ? PC->GetPawn() : nullptr;
		if (!J)
		{
			continue;
		}
		const FVector Vers = Point - J->GetActorLocation();
		const float D = Vers.Size();
		if (D < Distance)
		{
			if (!bHorsDeVue || D < 2500.f)
			{
				return false;
			}
			const FVector Regard = PC->GetControlRotation().Vector();
			if (FVector::DotProduct(Regard, Vers / FMath::Max(D, 1.f)) > 0.2f)
			{
				return false; // devant le joueur : il le verrait
			}
		}
	}
	return true;
}

void AVolVoitureChelles::GererVoituresGarees()
{
	UWorld* Monde = GetWorld();
	NosVoitures.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !P.IsValid(); });

	// 1) Retirer celles qui sont tres loin de tout le monde et hors de vue (fluidite)
	for (const TWeakObjectPtr<APawn>& P : NosVoitures)
	{
		APawn* V = P.Get();
		if (V && !EstConduiteParJoueur(V) && LoinDesJoueurs(V->GetActorLocation(), RayonGarees * 1.4f, true))
		{
			V->Destroy();
		}
	}
	NosVoitures.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !P.IsValid() || P->IsPendingKill(); });

	// Les voitures garees s'endorment (la physique ne coute presque plus rien)
	for (const TWeakObjectPtr<APawn>& P : NosVoitures)
	{
		APawn* V = P.Get();
		UPrimitiveComponent* Corps = CaisseVol(V);
		if (V && Corps && !EstConduiteParJoueur(V) && Corps->IsSimulatingPhysics() && Corps->GetPhysicsLinearVelocity().SizeSquared() < 400.f)
		{
			Corps->PutAllRigidBodiesToSleep();
		}
	}

	if (NombreGarees <= 0 || NosVoitures.Num() >= MaxVoitures)
	{
		return;
	}

	// 2) Compter les voitures libres autour de chaque joueur, et en garer de nouvelles le long de la circulation
	UClass* Circ = ClasseCirculation();
	if (!Circ)
	{
		return;
	}
	TArray<UClass*> Toutes = ClassesGarees;
	if (Toutes.Num() == 0)
	{
		Toutes = ClassesSecours;
		for (const TPair<FString, UClass*>& Paire : ClassesModeles)
		{
			Toutes.AddUnique(Paire.Value);
		}
	}
	Toutes.Remove(nullptr);
	if (Toutes.Num() == 0)
	{
		return;
	}
	int32 Ajoutees = 0;
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It && Ajoutees < 2; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* J = PC ? PC->GetPawn() : nullptr;
		if (!J)
		{
			continue;
		}
		const FVector Ici = J->GetActorLocation();
		int32 Autour = 0;
		for (const TWeakObjectPtr<APawn>& P : NosVoitures)
		{
			if (P.IsValid() && !EstConduiteParJoueur(P.Get()) && FVector::DistSquared(P->GetActorLocation(), Ici) < FMath::Square(RayonGarees))
			{
				++Autour;
			}
		}
		if (Autour >= NombreGarees)
		{
			continue;
		}
		// Places possibles : au bord de la route d'une voiture de la circulation, entre 40 m et le rayon, hors de vue
		TArray<AActor*> Candidates;
		for (TActorIterator<AActor> C(Monde, Circ); C; ++C)
		{
			if (!IsValid(*C) || C->IsHidden())
			{
				continue;
			}
			const float D = FVector::Dist(C->GetActorLocation(), Ici);
			if (D > 4000.f && D < RayonGarees * 0.85f && LoinDesJoueurs(C->GetActorLocation(), 12000.f, true))
			{
				Candidates.Add(*C);
			}
		}
		for (int32 Essai = 0; Essai < 3 && Candidates.Num() > 0 && Ajoutees < 2; ++Essai)
		{
			AActor* C = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
			Candidates.Remove(C);
			const float Yaw = C->GetActorRotation().Yaw;
			// Garee contre le trottoir, a droite de la voie, en arriere de la voiture (elle est deja passee)
			const FVector Lieu = C->GetActorLocation() + C->GetActorRightVector() * EcartTrottoir - C->GetActorForwardVector() * 900.f;
			bool bLibre = true;
			for (const TWeakObjectPtr<APawn>& P : NosVoitures)
			{
				if (P.IsValid() && FVector::DistSquared(P->GetActorLocation(), Lieu) < FMath::Square(1500.f))
				{
					bLibre = false;
					break;
				}
			}
			if (!bLibre)
			{
				continue;
			}
			UClass* Classe = Toutes[FMath::RandRange(0, Toutes.Num() - 1)];
			if (FaireApparaitre(Classe, Lieu, Yaw, true))
			{
				++Ajoutees;
			}
		}
	}
}

// =====================================================================================================
// Serveur : niveau RECHERCHE et police
// =====================================================================================================

void AVolVoitureChelles::AjouterEtoiles(ARelaisVolVoiture* R, int32 Nombre)
{
	if (!bRecherche || !R || Nombre <= 0)
	{
		return;
	}
	const int32 Avant = R->Etoiles;
	R->Etoiles = FMath::Clamp(R->Etoiles + Nombre, 0, EtoilesMax);
	R->TempsSansPolice = 0.f;
	R->ForceNetUpdate();
	if (R->Etoiles > Avant)
	{
		Avis(Cast<APlayerController>(R->GetOwner()), FString::Printf(TEXT("RECHERCHE : %d etoile%s ! La police arrive."), R->Etoiles, R->Etoiles > 1 ? TEXT("s") : TEXT("")), 4.f);
	}
}

bool AVolVoitureChelles::TrouverPlaceHorsDeVue(const FVector& Centre, float DistMin, float DistMax, FVector& OutLieu, float& OutYaw)
{
	// D'abord sur une route : une voiture de la circulation hors de vue (elle est remplacee par la police)
	if (UClass* Circ = ClasseCirculation())
	{
		for (TActorIterator<AActor> C(GetWorld(), Circ); C; ++C)
		{
			if (!IsValid(*C) || C->IsHidden())
			{
				continue;
			}
			const float D = FVector::Dist(C->GetActorLocation(), Centre);
			if (D > DistMin && D < DistMax && LoinDesJoueurs(C->GetActorLocation(), DistMax, true))
			{
				OutLieu = C->GetActorLocation();
				OutYaw = (Centre - OutLieu).Rotation().Yaw;
				C->Destroy();
				return true;
			}
		}
	}
	// Sinon : un point du sol dans une direction au hasard, hors de vue
	for (int32 Essai = 0; Essai < 8; ++Essai)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float D = FMath::FRandRange(DistMin, DistMax);
		const FVector Point = Centre + FVector(FMath::Cos(Angle) * D, FMath::Sin(Angle) * D, 0.f);
		if (!LoinDesJoueurs(Point, DistMax, true))
		{
			continue;
		}
		FHitResult Sol;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VolVoiturePolice), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Sol, Point + FVector(0.f, 0.f, 3000.f), Point - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params)
			&& FMath::Abs(Sol.ImpactPoint.Z - Centre.Z) < 800.f && Sol.ImpactNormal.Z > 0.85f)
		{
			OutLieu = Sol.ImpactPoint;
			OutYaw = (Centre - OutLieu).Rotation().Yaw;
			return true;
		}
	}
	return false;
}

void AVolVoitureChelles::EnvoyerPolice(ARelaisVolVoiture* R)
{
	APlayerController* PC = Cast<APlayerController>(R->GetOwner());
	APawn* J = PC ? PC->GetPawn() : nullptr;
	if (!J || !ClassePolice)
	{
		return;
	}
	FVector Lieu;
	float Yaw = 0.f;
	if (!TrouverPlaceHorsDeVue(J->GetActorLocation(), 6000.f, 13000.f, Lieu, Yaw))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* V = GetWorld()->SpawnActor<APawn>(ClassePolice, FTransform(FRotator(0.f, Yaw, 0.f), Lieu + FVector(0.f, 0.f, 80.f)), Params);
	if (!V)
	{
		return;
	}
	V->bUseControllerRotationYaw = false;
	V->bUseControllerRotationPitch = false;
	V->bUseControllerRotationRoll = false;
	V->Tags.AddUnique(TAG_POLICE);
	// Un "pilote" ordinateur : sans lui la voiture n'ecoute pas les commandes
	V->SpawnDefaultController();
	if (!V->GetController())
	{
		if (UClass* ClasseIA = LoadClass<AController>(nullptr, TEXT("/Script/AIModule.AIController")))
		{
			FActorSpawnParameters ParamsIA;
			ParamsIA.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AController* IA = GetWorld()->SpawnActor<AController>(ClasseIA, V->GetActorTransform(), ParamsIA))
			{
				IA->Possess(V);
			}
		}
	}
	FPoursuivantVol P;
	P.Voiture = V;
	P.Cible = R;
	Poursuivants.Add(P);
	UE_LOG(LogVolVoiture, Log, TEXT("[Vol] Police envoyee contre %s (%d en poursuite)."), *PC->GetName(), Poursuivants.Num());
}

void AVolVoitureChelles::ConduirePoursuivant(FPoursuivantVol& P, float Dt)
{
	APawn* V = P.Voiture;
	APlayerController* PC = P.Cible ? Cast<APlayerController>(P.Cible->GetOwner()) : nullptr;
	APawn* Cible = PC ? PC->GetPawn() : nullptr;
	UWheeledVehicleMovementComponent* M = RoulementVol(V);
	if (!M || !Cible || EstConduiteParJoueur(V))
	{
		return;
	}
	P.Age += Dt;
	// Sirene et gyrophares (le plugin ChellesPolice les eteint une fois en decouvrant la voiture : on rallume apres)
	if (!P.bSirene && P.Age > 1.5f)
	{
		P.bSirene = true;
		for (const TCHAR* Nom : { TEXT("ActivateLights"), TEXT("ActivateSiren") })
		{
			if (UFunction* F = V->FindFunction(FName(Nom)))
			{
				const int32 Taille = FMath::Max<int32>(F->ParmsSize, 1);
				uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
				FMemory::Memzero(Parametres, Taille);
				for (TFieldIterator<FProperty> It(F); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
				{
					if (FBoolProperty* B = CastField<FBoolProperty>(*It))
					{
						B->SetPropertyValue_InContainer(Parametres, true);
					}
				}
				V->ProcessEvent(F, Parametres);
			}
		}
	}

	const FVector Vers = Cible->GetActorLocation() - V->GetActorLocation();
	const float Dist = Vers.Size2D();
	const FVector Local = V->GetActorTransform().InverseTransformVectorNoScale(Vers);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const float Vitesse = M->GetForwardSpeed(); // cm/s

	float Volant = FMath::Clamp(Angle / 35.f, -1.f, 1.f);
	// Vitesse voulue : vite de loin, ralentit en arrivant (s'arrete a 6 m)
	const float Voulue = FMath::Clamp((Dist - 600.f) * 0.6f, 0.f, VitessePolice / CmsVersKmh);
	float Gaz = Vitesse < Voulue ? 1.f : (Vitesse > Voulue + 250.f ? -1.f : 0.f);
	if (FMath::Abs(Angle) > 90.f && Dist < 2000.f)
	{
		Gaz = FMath::Min(Gaz, 0.5f);
	}

	// Coincee contre un mur : marche arriere en braquant de l'autre cote
	if (Gaz > 0.5f && FMath::Abs(Vitesse) < 150.f)
	{
		P.TempsBloque += Dt;
		if (P.TempsBloque > 2.5f)
		{
			P.MarcheArriere = 1.6f;
			P.TempsBloque = 0.f;
		}
	}
	else
	{
		P.TempsBloque = 0.f;
	}
	if (P.MarcheArriere > 0.f)
	{
		P.MarcheArriere -= Dt;
		Gaz = -1.f;
		Volant = -Volant;
	}

	// Sur le toit ou sur le cote : on la remet sur ses roues
	if (V->GetActorUpVector().Z < 0.4f)
	{
		P.TempsRenverse += Dt;
		if (P.TempsRenverse > 3.f)
		{
			P.TempsRenverse = 0.f;
			V->SetActorLocationAndRotation(V->GetActorLocation() + FVector(0.f, 0.f, 200.f), FRotator(0.f, V->GetActorRotation().Yaw, 0.f),
				false, nullptr, ETeleportType::TeleportPhysics);
			if (UPrimitiveComponent* Corps = CaisseVol(V))
			{
				Corps->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Corps->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			}
		}
	}
	else
	{
		P.TempsRenverse = 0.f;
	}

	M->SetThrottleInput(Gaz);
	M->SetSteeringInput(Volant);
	M->SetHandbrakeInput(false);
}

void AVolVoitureChelles::GererRecherche(float Dt)
{
	// Poursuivants : retirer ceux qui n'ont plus de raison d'etre
	for (int32 i = Poursuivants.Num() - 1; i >= 0; --i)
	{
		FPoursuivantVol& P = Poursuivants[i];
		APlayerController* PC = P.Cible ? Cast<APlayerController>(P.Cible->GetOwner()) : nullptr;
		APawn* J = PC ? PC->GetPawn() : nullptr;
		const bool bVoitureOk = IsValid(P.Voiture);
		const bool bSansCible = !IsValid(P.Cible) || P.Cible->Etoiles <= 0 || !J;
		const float Dist = (bVoitureOk && J) ? FVector::Dist(J->GetActorLocation(), P.Voiture->GetActorLocation()) : BIG_NUMBER;
		if (!bVoitureOk || EstConduiteParJoueur(P.Voiture))
		{
			// Detruite, ou un joueur l'a prise (plugin ChellesPolice, touche E) : elle n'est plus a nous
			if (bVoitureOk && P.Voiture->GetController() && !P.Voiture->GetController()->IsPlayerController())
			{
				P.Voiture->GetController()->Destroy();
			}
			Poursuivants.RemoveAt(i);
			continue;
		}
		if ((bSansCible && (Dist > 4000.f || P.Age > 60.f)) || Dist > 30000.f)
		{
			if (AController* C = P.Voiture->GetController())
			{
				C->UnPossess();
				C->Destroy();
			}
			P.Voiture->Destroy();
			Poursuivants.RemoveAt(i);
			continue;
		}
		if (bSansCible)
		{
			// Plus recherche : la police s'arrete et repart plus tard
			if (UWheeledVehicleMovementComponent* M = RoulementVol(P.Voiture))
			{
				M->SetThrottleInput(0.f);
				M->SetHandbrakeInput(true);
			}
			P.Age += Dt;
			continue;
		}
		ConduirePoursuivant(P, Dt);
	}

	// Chaque joueur recherche
	for (TActorIterator<ARelaisVolVoiture> It(GetWorld()); It; ++It)
	{
		ARelaisVolVoiture* R = *It;
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		APawn* J = PC ? PC->GetPawn() : nullptr;
		if (!J || R->Etoiles <= 0)
		{
			R->TempsSansPolice = 0.f;
			R->TempsCoince = 0.f;
			continue;
		}
		int32 Miennes = 0;
		float PlusProche = BIG_NUMBER;
		for (const FPoursuivantVol& P : Poursuivants)
		{
			if (P.Cible == R && IsValid(P.Voiture))
			{
				++Miennes;
				PlusProche = FMath::Min(PlusProche, FVector::Dist(P.Voiture->GetActorLocation(), J->GetActorLocation()));
			}
		}
		// Renforts : une voiture par etoile (avec un maximum), une a la fois
		const int32 Voulues = FMath::Min(R->Etoiles * PoliceParEtoile, PoliceMax);
		R->ProchainRenfort -= Dt;
		if (Miennes < Voulues && R->ProchainRenfort <= 0.f)
		{
			R->ProchainRenfort = 6.f;
			EnvoyerPolice(R);
		}
		// Loin de la police : les etoiles baissent une par une
		if (PlusProche > DistanceVuePolice)
		{
			R->TempsSansPolice += Dt;
			if (R->TempsSansPolice > DureeEtoile)
			{
				R->TempsSansPolice = 0.f;
				R->Etoiles = FMath::Max(0, R->Etoiles - 1);
				R->ForceNetUpdate();
				Avis(PC, R->Etoiles > 0 ? FString::Printf(TEXT("Tu semes la police : %d etoile%s."), R->Etoiles, R->Etoiles > 1 ? TEXT("s") : TEXT(""))
					: FString(TEXT("Tu as seme la police. Plus recherche !")), 4.f);
			}
		}
		else
		{
			R->TempsSansPolice = 0.f;
		}
		// ARRETE : immobile a moins de 7 m d'une voiture de police pendant 4 s
		const float VitesseJoueur = J->GetVelocity().Size();
		if (PlusProche < 700.f && VitesseJoueur < 150.f)
		{
			R->TempsCoince += Dt;
			if (R->TempsCoince > 4.f)
			{
				R->TempsCoince = 0.f;
				R->Etoiles = 0;
				R->ForceNetUpdate();
				if (IsValid(R->Voiture) && PC->GetPawn() == R->Voiture)
				{
					DescendreServeur(R);
				}
				Avis(PC, TEXT("ARRETE PAR LA POLICE ! Tes etoiles sont effacees."), 5.f);
			}
		}
		else
		{
			R->TempsCoince = 0.f;
		}
	}
}

// =====================================================================================================
// Joueur local : touches, camera, conduite, etoiles
// =====================================================================================================

void AVolVoitureChelles::InstallerCamera(APawn* Voiture)
{
	RetirerCamera();
	if (!IsValid(Voiture) || !Voiture->GetRootComponent())
	{
		return;
	}
	// Les cameras du Blueprint de la voiture sont eteintes le temps de la conduite (la notre suit la souris)
	TArray<UCameraComponent*> Existantes;
	Voiture->GetComponents<UCameraComponent>(Existantes);
	for (UCameraComponent* C : Existantes)
	{
		if (C && C->IsActive())
		{
			C->Deactivate();
			CamerasEteintes.Add(C);
		}
	}

	USpringArmComponent* Bras = NewObject<USpringArmComponent>(Voiture, TEXT("BrasCameraVol"));
	Bras->SetupAttachment(Voiture->GetRootComponent());
	Bras->SetRelativeLocation(FVector(0.f, 0.f, HauteurCamera));
	Bras->TargetArmLength = DistanceCamera;
	Bras->bUsePawnControlRotation = true;
	Bras->bInheritRoll = false;
	Bras->bEnableCameraLag = true;
	Bras->CameraLagSpeed = 12.f;
	Bras->bDoCollisionTest = true;
	Bras->ProbeSize = 12.f;
	Bras->RegisterComponent();

	UCameraComponent* Cam = NewObject<UCameraComponent>(Voiture, TEXT("CameraVol"));
	Cam->SetupAttachment(Bras, USpringArmComponent::SocketName);
	Cam->bUsePawnControlRotation = false;
	Cam->FieldOfView = 90.f;
	Cam->RegisterComponent();
	Cam->Activate(true);

	VoitureCamera = Voiture;
	BrasCamera = Bras;
	Camera = Cam;
	TempsSansSouris = 0.f;
	if (APlayerController* PC = PCLocal())
	{
		PC->SetControlRotation(FRotator(-12.f, Voiture->GetActorRotation().Yaw, 0.f));
		if (PC->GetViewTarget() != Voiture)
		{
			PC->SetViewTarget(Voiture);
		}
		// Nos commandes passent apres celles du Blueprint de la voiture, et avant le moteur de la voiture
		AddTickPrerequisiteActor(PC);
		if (UWheeledVehicleMovementComponent* M = RoulementVol(Voiture))
		{
			M->AddTickPrerequisiteActor(this);
		}
	}
}

void AVolVoitureChelles::RetirerCamera()
{
	if (Camera.IsValid())
	{
		Camera->DestroyComponent();
	}
	if (BrasCamera.IsValid())
	{
		BrasCamera->DestroyComponent();
	}
	for (const TWeakObjectPtr<UCameraComponent>& C : CamerasEteintes)
	{
		if (C.IsValid())
		{
			C->Activate();
		}
	}
	CamerasEteintes.Reset();
	Camera = nullptr;
	BrasCamera = nullptr;
	VoitureCamera = nullptr;
}

void AVolVoitureChelles::AfficherEtoiles()
{
	ARelaisVolVoiture* R = MonRelais();
	const int32 N = R ? R->Etoiles : 0;
	if (N > 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(CLE_ETOILES, 0.6f, FColor(255, 215, 0),
			FString::Printf(TEXT("RECHERCHE   %s"), *TexteEtoiles(N, EtoilesMax)), true, FVector2D(1.6f, 1.6f));
	}
	if (N != EtoilesAffichees)
	{
		EtoilesAffichees = N;
	}
}

void AVolVoitureChelles::JoueurLocal(float Dt)
{
	APlayerController* PC = PCLocal();
	ARelaisVolVoiture* R = MonRelais();
	if (!PC || !R)
	{
		return;
	}
	AfficherEtoiles();
	APawn* Pion = PC->GetPawn();
	const bool bEnVoiture = Pion && R->Voiture == Pion && RoulementVol(Pion);

	// ---- A pied : voler (circulation) ou monter (voiture garee / deja volee)
	if (!bEnVoiture)
	{
		if (VoitureCamera.IsValid() || Camera.IsValid())
		{
			RetirerCamera();
		}
		if (bEtaitEnHoro)
		{
			// On vient de descendre d'une Horo : ses compteurs (vitesse, ordinateur de bord) quittent l'ecran
			bEtaitEnHoro = false;
			HoroChelles::RetirerAffichagesDuPack(GetWorld());
		}
		bAideAffichee = false;
		ACharacter* Perso = Cast<ACharacter>(Pion);
		if (!Perso || Perso->IsHidden())
		{
			return;
		}
		APawn* Garee = NotreVoitureLibreProche(Perso, DistanceVol);
		AActor* Circulation = Garee ? nullptr : VoitureCirculationProche(Perso, DistanceVol);
		if (!Garee && !Circulation)
		{
			return;
		}
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(CLE_AIDE, 0.3f, FColor(255, 190, 60), FString::Printf(TEXT("%s : %s"), *ToucheVoler.ToString(),
				Garee ? TEXT("voler cette voiture garee") : TEXT("VOLER CETTE VOITURE (le conducteur sera ejecte)")));
		}
		if (!PC->WasInputKeyJustPressed(ToucheVoler))
		{
			return;
		}
		if (Garee)
		{
			R->ServeurMonter(Garee);
			return;
		}
		const FVector Lieu = Circulation->GetActorLocation();
		const float Yaw = Circulation->GetActorRotation().Yaw;
		const FString Modele = ModeleDe(Circulation);
		// La circulation est locale : la voiture disparait ici tout de suite, le conducteur s'enfuit
		Circulation->Destroy();
		EjecterConducteur(Lieu, Yaw);
		R->ServeurVolerCirculation(Lieu, Yaw, Modele);
		return;
	}

	// ---- En voiture volee
	if (VoitureCamera.Get() != Pion)
	{
		if (CameraDuPack(Pion))
		{
			// 25/11 : voiture DV_Car_Horo : on garde ses cameras (exterieure / cockpit, touche du pack)
			RetirerCamera();
			VoitureCamera = Pion;
			bCamPackActive = true;
			if (PC->GetViewTarget() != Pion)
			{
				PC->SetViewTarget(Pion);
			}
		}
		else
		{
			bCamPackActive = false;
			InstallerCamera(Pion);
		}
	}
	if (!bAideAffichee)
	{
		bAideAffichee = true;
		Message(AideConduite(Pion), 10.f, CLE_INFO);
	}
	if (HoroChelles::EstHoro(Pion))
	{
		bEtaitEnHoro = true;
		TouchesHoro(PC, R, Pion);
	}
	if (UWheeledVehicleMovementComponent* M = RoulementVol(Pion))
	{
		auto Bas = [PC](const FKey& Touche) { return PC->IsInputKeyDown(Touche); };
		float Gaz = ((Bas(EKeys::Z) || Bas(EKeys::W) || Bas(EKeys::Up)) ? 1.f : 0.f) - ((Bas(EKeys::S) || Bas(EKeys::Down)) ? 1.f : 0.f);
		Gaz += PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) - PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
		float Volant = ((Bas(EKeys::D) || Bas(EKeys::Right)) ? 1.f : 0.f) - ((Bas(EKeys::Q) || Bas(EKeys::A) || Bas(EKeys::Left)) ? 1.f : 0.f);
		Volant += PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		M->SetThrottleInput(FMath::Clamp(Gaz, -1.f, 1.f));
		M->SetSteeringInput(FMath::Clamp(Volant, -1.f, 1.f));
		M->SetHandbrakeInput(Bas(EKeys::SpaceBar) || Bas(EKeys::Gamepad_FaceButton_Bottom));
	}

	// Souris / stick droit : regarder autour ; sans souris, la camera revient derriere la voiture
	// (camera du pack Horo : c'est le Blueprint de la voiture qui la tourne avec la souris)
	float DX = 0.f, DY = 0.f;
	if (bCamPackActive)
	{
		if (PC->WasInputKeyJustPressed(ToucheVoler) || PC->WasInputKeyJustPressed(ToucheDescendre2))
		{
			R->ServeurDescendre();
		}
		return;
	}
	PC->GetInputMouseDelta(DX, DY);
	DX += PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX) * 2.f;
	DY += PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY) * 2.f;
	if (FMath::Abs(DX) + FMath::Abs(DY) > 0.01f)
	{
		PC->AddYawInput(DX);
		PC->AddPitchInput(-DY);
		TempsSansSouris = 0.f;
	}
	else
	{
		TempsSansSouris += Dt;
		if (TempsSansSouris > 2.f)
		{
			const FRotator Actuelle = PC->GetControlRotation();
			const FRotator Derriere(-12.f, Pion->GetActorRotation().Yaw, 0.f);
			PC->SetControlRotation(FMath::RInterpTo(Actuelle, Derriere, Dt, 2.5f));
		}
	}

	if (PC->WasInputKeyJustPressed(ToucheVoler) || PC->WasInputKeyJustPressed(ToucheDescendre2))
	{
		R->ServeurDescendre();
	}
}

// =====================================================================================================
// Outils
// =====================================================================================================

FVector AVolVoitureChelles::PlaceADescendre(const APawn* Voiture, const APawn* Pieton)
{
	float Rayon = 42.f;
	float DemiHauteur = 96.f;
	if (const ACharacter* C = Cast<ACharacter>(Pieton))
	{
		if (C->GetCapsuleComponent())
		{
			C->GetCapsuleComponent()->GetScaledCapsuleSize(Rayon, DemiHauteur);
		}
	}
	const FVector Base = Voiture->GetActorLocation();
	const FVector Droite = Voiture->GetActorRightVector();
	const FVector Avant = Voiture->GetActorForwardVector();
	const FVector Hauteur(0.f, 0.f, DemiHauteur + 30.f);
	const FVector Essais[] = {
		Base - Droite * 230.f + Hauteur, // cote conducteur (gauche)
		Base + Droite * 230.f + Hauteur,
		Base - Avant * 420.f + Hauteur,
		Base + Avant * 420.f + Hauteur
	};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VolVoitureDescente), false);
	Params.AddIgnoredActor(Voiture);
	Params.AddIgnoredActor(Pieton);
	const FCollisionShape Forme = FCollisionShape::MakeCapsule(Rayon, DemiHauteur);
	for (const FVector& Essai : Essais)
	{
		if (!Voiture->GetWorld()->OverlapBlockingTestByChannel(Essai, FQuat::Identity, ECC_Pawn, Forme, Params))
		{
			return Essai;
		}
	}
	return Base + FVector(0.f, 0.f, DemiHauteur + 250.f);
}

void AVolVoitureChelles::CacherPion(APawn* Pion, bool bCacher)
{
	if (!IsValid(Pion))
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
		if (UCharacterMovementComponent* M = Perso->GetCharacterMovement())
		{
			if (bCacher)
			{
				M->StopMovementImmediately();
				M->DisableMovement();
			}
			else
			{
				M->SetMovementMode(MOVE_Falling);
			}
		}
	}
}

void AVolVoitureChelles::ArreterVoiture(APawn* Voiture)
{
	if (!IsValid(Voiture))
	{
		return;
	}
	if (UWheeledVehicleMovementComponent* Mouvement = RoulementVol(Voiture))
	{
		Mouvement->SetThrottleInput(0.f);
		Mouvement->SetSteeringInput(0.f);
		Mouvement->SetHandbrakeInput(true);
		// Sans conducteur, le vehicule lit son etat "copie par le reseau" : on le remet a zero aussi.
		if (FStructProperty* Etat = FindFProperty<FStructProperty>(UWheeledVehicleMovementComponent::StaticClass(), TEXT("ReplicatedState")))
		{
			void* Valeurs = Etat->ContainerPtrToValuePtr<void>(Mouvement);
			auto Regler = [Etat, Valeurs](const TCHAR* Champ, float Valeur)
			{
				if (FFloatProperty* P = FindFProperty<FFloatProperty>(Etat->Struct, Champ))
				{
					P->SetPropertyValue_InContainer(Valeurs, Valeur);
				}
			};
			Regler(TEXT("ThrottleInput"), 0.f);
			Regler(TEXT("SteeringInput"), 0.f);
			Regler(TEXT("BrakeInput"), 1.f);
			Regler(TEXT("HandbrakeInput"), 1.f);
		}
	}
	if (UPrimitiveComponent* Corps = CaisseVol(Voiture))
	{
		if (Corps->IsSimulatingPhysics())
		{
			Corps->SetPhysicsLinearVelocity(Corps->GetPhysicsLinearVelocity() * 0.2f);
		}
	}
}

void AVolVoitureChelles::Aider(APawn* Voiture, float Dt) const
{
	// Aide legere : la voiture volee glisse moins sur le cote dans les virages (comme les voitures de police)
	UWheeledVehicleMovementComponent* Mouvement = RoulementVol(Voiture);
	UPrimitiveComponent* Corps = CaisseVol(Voiture);
	if (!Mouvement || !Corps || !Corps->IsSimulatingPhysics() || Dt <= 0.f || Adherence <= 0.f)
	{
		return;
	}
	int32 AuSol = 0;
	for (const UVehicleWheel* Roue : Mouvement->Wheels)
	{
		if (Roue && !Roue->IsInAir())
		{
			++AuSol;
		}
	}
	if (AuSol < 2)
	{
		return;
	}
	const FVector Droite = Corps->GetComponentTransform().GetUnitAxis(EAxis::Y);
	FVector Vitesse = Corps->GetPhysicsLinearVelocity();
	const float Lateral = FVector::DotProduct(Vitesse, Droite);
	Vitesse -= Droite * Lateral * (1.f - FMath::Exp(-Adherence * Dt));
	Corps->SetPhysicsLinearVelocity(Vitesse);
}

// =====================================================================================================
// Tick, console
// =====================================================================================================

void AVolVoitureChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	if (EstServeur())
	{
		ProchainRelais -= DeltaSeconds;
		if (ProchainRelais <= 0.f)
		{
			ProchainRelais = 0.5f;
			GererRelaisServeur();
		}
		SuivreConducteurs();
		ProchainesGarees -= DeltaSeconds;
		if (ProchainesGarees <= 0.f)
		{
			ProchainesGarees = 4.f;
			GererVoituresGarees();
		}
		if (bRecherche)
		{
			GererRecherche(Dt);
		}
	}

	// Pietons caches des joueurs en voiture : pas de collision ici non plus (la collision n'est pas copiee)
	for (TActorIterator<ARelaisVolVoiture> It(GetWorld()); It; ++It)
	{
		if (IsValid(It->PietonCache))
		{
			It->PietonCache->SetActorEnableCollision(false);
		}
		if (bAide && IsValid(It->Voiture) && EstConduiteParJoueur(It->Voiture))
		{
			Aider(It->Voiture, Dt);
		}
	}

	// 25/11 : voitures DV_Car_Horo : options, plaques CHELLES-3D, Bugatti (chaque seconde, sur chaque machine)
	ProchainHabillage -= DeltaSeconds;
	if (ProchainHabillage <= 0.f)
	{
		ProchainHabillage = 1.f;
		HabillerVoituresHoro();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		JoueurLocal(Dt);
	}
}

// =====================================================================================================
// 25/11/2026 : voitures du pack DV_Car_Horo garees au bord des rues (et Bugatti de Rachid)
// =====================================================================================================

void AVolVoitureChelles::HabillerVoituresHoro()
{
	const bool bAffichage = GetNetMode() != NM_DedicatedServer;
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* V = *It;
		if (!IsValid(V) || !HoroChelles::EstHoro(V) || !EstNotreVoiture(V))
		{
			continue;
		}
		HoroChelles::ReglerOptions(V, OptionsHoro);
		HoroChelles::CouperDeclencheursDuPack(V);
		if (!bAffichage)
		{
			continue;
		}
		HoroChelles::EcrirePlaques(V, TextePlaque);
		const FString NomClasse = V->GetClass()->GetName();
		if (NomClasse.Contains(TEXT("Bugatti")) && !HoroChelles::EstHabilleeBugatti(V))
		{
			const FHabillageBugatti& H = Habillages[NomClasse.Contains(TEXT("Brouillard")) ? 1 : 0];
			if (H.EstPret())
			{
				HoroChelles::HabillerBugatti(V, H);
			}
		}
	}
}

bool AVolVoitureChelles::CameraDuPack(const APawn* Voiture) const
{
	if (!bCameraPack || !HoroChelles::EstHoro(Voiture))
	{
		return false;
	}
	TArray<UCameraComponent*> Cameras;
	Voiture->GetComponents<UCameraComponent>(Cameras);
	return Cameras.Num() > 0;
}

FString AVolVoitureChelles::AideConduite(const APawn* Voiture) const
{
	FString Texte = FString::Printf(TEXT("VOITURE : Z/S avancer/reculer, Q/D tourner, Espace frein a main, H klaxon, %s descendre."),
		*ToucheVoler.ToString());
	if (HoroChelles::EstHoro(Voiture))
	{
		Texte += FString::Printf(TEXT("\n%s phares, %s saut americain, %s reinitialiser, %s / %s clignotants, %s warnings, C camera, Inser ordinateur de bord."),
			*ToucheActions[EActionHoro::Phares].GetDisplayName().ToString(), *ToucheActions[EActionHoro::Saut].GetDisplayName().ToString(),
			*ToucheActions[EActionHoro::Reinitialiser].GetDisplayName().ToString(), *ToucheActions[EActionHoro::ClignotantGauche].GetDisplayName().ToString(),
			*ToucheActions[EActionHoro::ClignotantDroit].GetDisplayName().ToString(), *ToucheActions[EActionHoro::Warnings].GetDisplayName().ToString());
	}
	return Texte;
}

void AVolVoitureChelles::TouchesHoro(APlayerController* PC, ARelaisVolVoiture* R, APawn* Voiture)
{
	// Le pack sort de la voiture avec "Use" (E) a sa facon, et a son propre klaxon : on garde nos versions
	// (descendre = plugin, klaxon H = ChellesSonsVehicules, entendu par tout le monde).
	// Les actions jouees par le plugin chez tout le monde (phares, saut...) sont aussi retirees des commandes
	// du pack, sinon le conducteur les jouerait deux fois (touches ajoutees dans DefaultInput.ini).
	static TArray<FName> Retirees;
	if (Retirees.Num() == 0)
	{
		Retirees.Add(FName(TEXT("Use")));
		Retirees.Add(FName(TEXT("Horn")));
		for (int32 k = 0; k < EActionHoro::Nombre; ++k)
		{
			Retirees.Add(HoroChelles::NomAction((EActionHoro::Type)k));
		}
	}
	HoroChelles::RetirerLiaisons(Voiture, Retirees);
	for (int32 k = 0; k < EActionHoro::Nombre; ++k)
	{
		if (ToucheActions[k].IsValid() && PC->WasInputKeyJustPressed(ToucheActions[k]))
		{
			R->ServeurActionVoiture((uint8)k);
		}
	}
}

void AVolVoitureChelles::ActionVoitureServeur(ARelaisVolVoiture* R, uint8 Action)
{
	APlayerController* PC = R ? Cast<APlayerController>(R->GetOwner()) : nullptr;
	APawn* V = PC ? PC->GetPawn() : nullptr;
	if (!V || V != R->Voiture || !HoroChelles::EstHoro(V) || Action >= EActionHoro::Nombre)
	{
		return;
	}
	if (Action == EActionHoro::Reinitialiser)
	{
		HoroChelles::RemettreSurRoues(V);
	}
	R->MulticastActionVoiture(V, Action);
}

void AVolVoitureChelles::ActionVoitureLocale(APawn* Voiture, uint8 Action)
{
	if (!IsValid(Voiture) || Action >= EActionHoro::Nombre)
	{
		return;
	}
	HoroChelles::ReglerOptions(Voiture, OptionsHoro);
	if (!HoroChelles::LancerActionPack(Voiture, HoroChelles::NomAction((EActionHoro::Type)Action)))
	{
		UE_LOG(LogVolVoiture, Log, TEXT("[Vol] Action %d : rien dans %s."), (int32)Action, *Voiture->GetClass()->GetName());
	}
}

void AVolVoitureChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Action = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
	if (Action.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		LireReglages();
		ChargerClasses();
		Message(TEXT("VolVoiture.txt relu."));
		return;
	}
	if (Action.Equals(TEXT("Etoiles"), ESearchCase::IgnoreCase) && EstServeur())
	{
		const int32 N = Arguments.Num() > 1 ? FCString::Atoi(*Arguments[1]) : 1;
		if (APlayerController* PC = PCLocal())
		{
			for (TActorIterator<ARelaisVolVoiture> It(GetWorld()); It; ++It)
			{
				if (It->GetOwner() == PC)
				{
					It->Etoiles = 0;
					AjouterEtoiles(*It, N);
					if (N <= 0)
					{
						Message(TEXT("Plus recherche."));
					}
				}
			}
		}
		return;
	}
	if (Action.Equals(TEXT("Garer"), ESearchCase::IgnoreCase) && EstServeur())
	{
		GererVoituresGarees();
		GererVoituresGarees();
		Message(FString::Printf(TEXT("%d autos du plugin sur la carte."), NosVoitures.Num()));
		return;
	}
	Message(FString::Printf(TEXT("Vol de voiture : %d modeles, %d de secours, police %s, %d autos sur la carte, %d en poursuite. Touche %s."),
		ClassesModeles.Num(), ClassesSecours.Num(), ClassePolice ? TEXT("prete") : TEXT("absente"), NosVoitures.Num(), Poursuivants.Num(),
		*ToucheVoler.ToString()), 8.f);
}
