// CHELLES 3D - Sons des vehicules (UE 4.27)
#include "SonsVehicules.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const FName ParamRegime(TEXT("RPM"));
	const FName ParamVitesse(TEXT("Speed"));

	bool EstOui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("True"), ESearchCase::IgnoreCase) || V == TEXT("1");
	}

	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesSonsVehicules"), TEXT("SonsVehicules.txt"));
	}

	void Message(const FString& Texte)
	{
		UE_LOG(LogTemp, Log, TEXT("[Vehicules] %s"), *Texte);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor(255, 200, 90), FString(TEXT("[Sons vehicules] ")) + Texte);
		}
	}

	/** Lissage independant du nombre d'images par seconde. */
	float Vers(float Actuel, float Cible, float Dt, float Rapidite)
	{
		return FMath::Lerp(Actuel, Cible, 1.f - FMath::Exp(-Rapidite * Dt));
	}

	/** Appelle une fonction Blueprint sans parametre qui renvoie un float (ex : GetEngineRotationSpeed). */
	bool LireFloat(UObject* Objet, const TCHAR* Fonction, float& Resultat)
	{
		if (!Objet)
		{
			return false;
		}
		UFunction* F = Objet->FindFunction(FName(Fonction));
		if (!F || F->ParmsSize != sizeof(float) || F->NumParms != 1)
		{
			return false;
		}
		float Retour = 0.f;
		Objet->ProcessEvent(F, &Retour);
		Resultat = Retour;
		return true;
	}

	bool NomContient(const FString& Nom, const TCHAR* const* Mots, int32 Nombre)
	{
		for (int32 i = 0; i < Nombre; ++i)
		{
			if (Nom.Contains(Mots[i], ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	const TCHAR* MotsMoteur[] = { TEXT("engine"), TEXT("engeine"), TEXT("moteur"), TEXT("motor") };
	const TCHAR* MotsKlaxon[] = { TEXT("horn"), TEXT("klaxon") };
}

// =====================================================================================================
// Gestionnaire (un par machine)
// =====================================================================================================

ASonsVehicules::ASonsVehicules()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);
	SetCanBeDamaged(false);
}

void ASonsVehicules::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	ProchainKlaxonHasard = GetWorld()->GetTimeSeconds() + FMath::FRandRange(15.f, 30.f);
	UE_LOG(LogTemp, Log, TEXT("[Vehicules] Pret. Klaxon = touche %s. Console : Chelles.Vehicules Info"), *Reglages.ToucheKlaxon);
}

void ASonsVehicules::EndPlay(const EEndPlayReason::Type Raison)
{
	CouperTout();
	Super::EndPlay(Raison);
}

// ---------------------------------------------------------------- Reglages

void ASonsVehicules::LireReglages()
{
	Reglages = FReglagesSonsVehicules();
	FReglagesSonsVehicules& R = Reglages;
	R.Voiture.Chemin = TEXT("/Game/DriveableCar_basic/Sound/cues/standard/standard_mix_Cue");
	R.Voiture.Volume = 0.55f; R.Voiture.Ralenti = 150.f; R.Voiture.RegimeMax = 4500.f; R.Voiture.Rapport = 22.f;
	R.Bus.Chemin = TEXT("/Game/DV_Car_Horo/Sounds/Cues/SC_Car_Hatchback_Horo_Engine");
	R.Bus.Volume = 0.9f; R.Bus.Ralenti = 800.f; R.Bus.RegimeMax = 5000.f; R.Bus.Rapport = 14.f; R.Bus.Grave = 0.85f;
	R.Motoneige.Chemin = TEXT("/Game/DV_Snowmobile/Sounds/Cues/SC_Snowmobile_engine");
	R.Motoneige.Volume = 0.6f; R.Motoneige.Ralenti = 800.f; R.Motoneige.RegimeMax = 6500.f; R.Motoneige.Rapport = 30.f;

	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *CheminReglages()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vehicules] %s introuvable : reglages par defaut."), *CheminReglages());
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
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
		const float F = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));

		FString Groupe, Champ;
		if (Cle.Split(TEXT("."), &Groupe, &Champ))
		{
			FMoteurReglages* M = Groupe.Equals(TEXT("Voiture"), ESearchCase::IgnoreCase) ? &R.Voiture
				: Groupe.Equals(TEXT("Bus"), ESearchCase::IgnoreCase) ? &R.Bus
				: Groupe.Equals(TEXT("Motoneige"), ESearchCase::IgnoreCase) ? &R.Motoneige : nullptr;
			if (!M)
			{
				continue;
			}
			if (Champ.Equals(TEXT("Moteur"), ESearchCase::IgnoreCase)) { M->Chemin = Valeur; }
			else if (Champ.Equals(TEXT("Volume"), ESearchCase::IgnoreCase)) { M->Volume = FMath::Max(0.f, F); }
			else if (Champ.Equals(TEXT("Ralenti"), ESearchCase::IgnoreCase)) { M->Ralenti = FMath::Max(0.f, F); }
			else if (Champ.Equals(TEXT("RegimeMax"), ESearchCase::IgnoreCase)) { M->RegimeMax = FMath::Max(1.f, F); }
			else if (Champ.Equals(TEXT("Rapport"), ESearchCase::IgnoreCase)) { M->Rapport = FMath::Max(3.f, F); }
			else if (Champ.Equals(TEXT("Grave"), ESearchCase::IgnoreCase)) { M->Grave = FMath::Clamp(F, 0.3f, 3.f); }
			continue;
		}
		if (Cle == TEXT("Actif")) { R.bActif = EstOui(Valeur); }
		else if (Cle == TEXT("Volume")) { R.Volume = FMath::Max(0.f, F); }
		else if (Cle == TEXT("ToucheKlaxon")) { R.ToucheKlaxon = Valeur.ToUpper(); }
		else if (Cle == TEXT("DistanceMoteur")) { R.DistanceMoteur = FMath::Clamp(F, 5.f, 500.f) * 100.f; }
		else if (Cle == TEXT("MoteursMax")) { R.MoteursMax = FMath::Clamp(FMath::RoundToInt(F), 1, 32); }
		else if (Cle == TEXT("Klaxon")) { R.Klaxon = Valeur; }
		else if (Cle == TEXT("KlaxonVolume")) { R.KlaxonVolume = FMath::Max(0.f, F); }
		else if (Cle == TEXT("KlaxonBusGrave")) { R.KlaxonBusGrave = FMath::Clamp(F, 0.3f, 3.f); }
		else if (Cle == TEXT("KlaxonIA")) { R.bKlaxonIA = EstOui(Valeur); }
		else if (Cle == TEXT("KlaxonIAArret")) { R.KlaxonIAArret = FMath::Max(0.5f, F); }
		else if (Cle == TEXT("KlaxonIAChance")) { R.KlaxonIAChance = FMath::Clamp(F, 0.f, 100.f); }
		else if (Cle == TEXT("KlaxonIAHasard")) { R.KlaxonIAHasard = FMath::Max(0.f, F); }
		else if (Cle == TEXT("KlaxonBusDepart")) { R.bKlaxonBusDepart = EstOui(Valeur); }
	}
	if (R.ToucheKlaxon.IsEmpty())
	{
		R.ToucheKlaxon = TEXT("H");
	}
}

USoundBase* ASonsVehicules::ChargerSon(FMoteurReglages& M)
{
	if (M.bChargementTente)
	{
		return M.Son;
	}
	M.bChargementTente = true;
	FString Chemin = M.Chemin;
	Chemin.TrimStartAndEndInline();
	if (Chemin.IsEmpty() || Chemin.Equals(TEXT("Aucun"), ESearchCase::IgnoreCase))
	{
		return nullptr;
	}
	if (!Chemin.Contains(TEXT(".")))
	{
		FString Nom;
		Chemin.Split(TEXT("/"), nullptr, &Nom, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		Chemin += TEXT(".") + Nom;
	}
	M.Son = LoadObject<USoundBase>(nullptr, *Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (M.Son)
	{
		SonsGardes.AddUnique(M.Son);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vehicules] Son introuvable : %s (verifiez SonsVehicules.txt)"), *Chemin);
	}
	return M.Son;
}

USoundBase* ASonsVehicules::SonKlaxon()
{
	if (!bKlaxonTente)
	{
		bKlaxonTente = true;
		FMoteurReglages Tmp;
		Tmp.Chemin = Reglages.Klaxon;
		SonKlaxonCharge = ChargerSon(Tmp);
	}
	return SonKlaxonCharge;
}

FMoteurReglages& ASonsVehicules::ReglagesMoteur(EGenreVehicule Genre)
{
	switch (Genre)
	{
	case EGenreVehicule::Bus: return Reglages.Bus;
	case EGenreVehicule::Motoneige: return Reglages.Motoneige;
	default: return Reglages.Voiture;
	}
}

float ASonsVehicules::VolumeOptions() const
{
	float V = 1.f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("ChellesMenu"), TEXT("VolumeVehicules"), V, GGameUserSettingsIni);
	}
	return FMath::Clamp(V, 0.f, 1.f);
}

// ---------------------------------------------------------------- Recherche des vehicules

FVehiculeSuivi* ASonsVehicules::Trouver(AActor* Acteur)
{
	for (FVehiculeSuivi& V : Vehicules)
	{
		if (V.Acteur.Get() == Acteur)
		{
			return &V;
		}
	}
	return nullptr;
}

void ASonsVehicules::Ajouter(AActor* Acteur, EGenreVehicule Genre)
{
	if (!Acteur || Acteur->IsPendingKill())
	{
		return;
	}
	if (FVehiculeSuivi* Existant = Trouver(Acteur))
	{
		Existant->bVu = true;
		return;
	}
	FVehiculeSuivi V;
	V.Acteur = Acteur;
	V.Genre = Genre;
	V.bVu = true;
	V.Variation = (Genre == EGenreVehicule::Voiture) ? FMath::FRandRange(0.92f, 1.08f) : 1.f;
	V.ProchainKlaxon = GetWorld()->GetTimeSeconds() + FMath::FRandRange(5.f, 20.f);

	TInlineComponentArray<UActorComponent*> Composants;
	Acteur->GetComponents(Composants);
	for (UActorComponent* C : Composants)
	{
		if (UAudioComponent* A = Cast<UAudioComponent>(C))
		{
			const FString Nom = A->GetName() + TEXT(" ") + (A->Sound ? A->Sound->GetName() : FString());
			if (NomContient(Nom, MotsMoteur, UE_ARRAY_COUNT(MotsMoteur)))
			{
				V.MoteursPack.Add(A);
				VolumesPack.Add(A, FVector2D(A->VolumeMultiplier, A->VolumeMultiplier));
			}
			else if (NomContient(Nom, MotsKlaxon, UE_ARRAY_COUNT(MotsKlaxon)))
			{
				V.bKlaxonPack = true;
				VolumesPack.Add(A, FVector2D(A->VolumeMultiplier, A->VolumeMultiplier));
			}
		}
		else if (C && C->GetClass()->GetName().Contains(TEXT("VehicleMovement")))
		{
			V.Roues = C;
		}
	}
	Vehicules.Add(V);
	UE_LOG(LogTemp, Verbose, TEXT("[Vehicules] Suivi : %s"), *Acteur->GetName());
}

void ASonsVehicules::Rechercher()
{
	UWorld* Monde = GetWorld();
	for (FVehiculeSuivi& V : Vehicules)
	{
		V.bVu = false;
	}

	// Voitures de la circulation (plugin ChellesCirculation, reconnu par le nom de sa classe)
	if (!ClasseCirculation.IsValid())
	{
		ClasseCirculation = FindObject<UClass>(ANY_PACKAGE, TEXT("VehiculeCirculation"));
	}
	if (UClass* Classe = ClasseCirculation.Get())
	{
		for (TActorIterator<AActor> It(Monde, Classe); It; ++It)
		{
			Ajouter(*It, EGenreVehicule::Voiture);
		}
	}

	// Bus du pack Bus_sity (deplace par le plugin ChellesBus)
	if (UClass* ClasseBus = FindObject<UClass>(ANY_PACKAGE, TEXT("BP_Bus_sity_C")))
	{
		for (TActorIterator<AActor> It(Monde, ClasseBus); It; ++It)
		{
			Ajouter(*It, EGenreVehicule::Bus);
		}
	}

	// Tous les vehicules a roues (motoneige, voiture de la roue TAB, voitures des packs...)
	for (TActorIterator<APawn> It(Monde); It; ++It)
	{
		APawn* P = *It;
		if (Trouver(P))
		{
			Trouver(P)->bVu = true;
			continue;
		}
		const FString NomClasse = P->GetClass()->GetName();
		if (NomClasse.Contains(TEXT("Bus_sity")))
		{
			Ajouter(P, EGenreVehicule::Bus);
			continue;
		}
		TInlineComponentArray<UActorComponent*> Composants;
		P->GetComponents(Composants);
		for (UActorComponent* C : Composants)
		{
			if (C && C->GetClass()->GetName().Contains(TEXT("WheeledVehicleMovement")))
			{
				Ajouter(P, NomClasse.Contains(TEXT("Snowmobile")) ? EGenreVehicule::Motoneige : EGenreVehicule::Conduite);
				break;
			}
		}
	}

	// Oublie les vehicules disparus
	for (int32 i = Vehicules.Num() - 1; i >= 0; --i)
	{
		FVehiculeSuivi& V = Vehicules[i];
		if (!V.Acteur.IsValid() || !V.bVu)
		{
			if (UAudioComponent* M = V.Moteur.Get())
			{
				M->Stop();
				M->DestroyComponent();
			}
			if (UAudioComponent* K = V.KlaxonEnCours.Get())
			{
				K->Stop();
			}
			Vehicules.RemoveAtSwap(i);
		}
	}
	for (auto It = VolumesPack.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

// ---------------------------------------------------------------- Etat

bool ASonsVehicules::EstConduitParUnJoueur(const AActor* Acteur) const
{
	const APawn* P = Cast<APawn>(Acteur);
	// PlayerState est replique : marche aussi pour les vehicules des autres joueurs en multi
	return P && (P->GetPlayerState() != nullptr || P->IsPlayerControlled());
}

APlayerController* ASonsVehicules::JoueurLocal() const
{
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

AActor* ASonsVehicules::VehiculeDuJoueur() const
{
	APlayerController* PC = JoueurLocal();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P)
	{
		return nullptr;
	}
	for (const FVehiculeSuivi& V : Vehicules)
	{
		if (V.Acteur.Get() == P && V.Genre != EGenreVehicule::Voiture)
		{
			return P;
		}
	}
	return nullptr;
}

bool ASonsVehicules::MoteurPackActif(const FVehiculeSuivi& V) const
{
	if (V.Genre == EGenreVehicule::Bus)
	{
		return false; // le bus est deplace sans physique : son moteur de pack ne suivrait pas la vitesse
	}
	for (const TWeakObjectPtr<UAudioComponent>& A : V.MoteursPack)
	{
		if (A.IsValid() && A->IsPlaying())
		{
			return true;
		}
	}
	return false;
}

float ASonsVehicules::RegimeCible(const FVehiculeSuivi& V, const FMoteurReglages& M) const
{
	const float Plage = FMath::Max(1.f, M.RegimeMax - M.Ralenti);
	const float Charge = FMath::Clamp(V.Acceleration / 300.f, -1.f, 1.f);

	// Vrai regime du moteur du pack (vehicule a roues PhysX), si disponible
	if (UActorComponent* Roues = V.Roues.Get())
	{
		float Regime = 0.f, RegimeMaxi = 0.f;
		if (LireFloat(Roues, TEXT("GetEngineRotationSpeed"), Regime)
			&& LireFloat(Roues, TEXT("GetEngineMaxRotationSpeed"), RegimeMaxi)
			&& RegimeMaxi > 100.f && (Regime > 1.f || V.Vitesse < 100.f))
		{
			return M.Ralenti + Plage * FMath::Clamp(Regime / RegimeMaxi, 0.f, 1.05f);
		}
	}

	// Sinon : moteur imaginaire avec une boite de vitesses
	const float Kmh = V.Vitesse * 0.036f;
	float Base = 0.f;
	if (Kmh < 3.f)
	{
		Base = 0.f;
	}
	else
	{
		const int32 Rapport = FMath::Min(FMath::FloorToInt(Kmh / M.Rapport), 4);
		const float Dans = (Kmh - Rapport * M.Rapport) / (Rapport == 4 ? M.Rapport * 2.5f : M.Rapport);
		const float F = FMath::Clamp(Dans, 0.f, 1.f);
		Base = (Rapport == 0) ? (0.15f + 0.6f * F) : (0.35f + 0.5f * F);
	}
	if (Charge > 0.f)
	{
		Base += 0.18f * Charge;   // on accelere : le moteur monte
	}
	else
	{
		Base -= 0.08f * -Charge;  // on freine / on lache : il redescend
	}
	return M.Ralenti + Plage * FMath::Clamp(Base, -0.05f, 1.05f);
}

// ---------------------------------------------------------------- Boucle

void ASonsVehicules::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	const float Maintenant = Monde->GetTimeSeconds();
	const bool bServeur = GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer;

	if (bServeur && Maintenant >= ProchainRelais)
	{
		ProchainRelais = Maintenant + 2.f;
		CreerLesRelais();
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return; // un serveur sans ecran n'entend rien
	}

	if (Maintenant >= ProchaineLectureOptions)
	{
		ProchaineLectureOptions = Maintenant + 0.5f;
		VolumeOptionsCache = VolumeOptions();
		// Le reglage du menu s'applique aussi aux sons de moteur / klaxon des packs (motoneige, voitures)
		for (auto& Paire : VolumesPack)
		{
			if (UAudioComponent* A = Paire.Key.Get())
			{
				FVector2D& Vol = Paire.Value;
				if (!FMath::IsNearlyEqual(A->VolumeMultiplier, Vol.Y, 0.005f))
				{
					Vol.X = A->VolumeMultiplier; // le Blueprint du pack a change son volume : on part de sa valeur
				}
				const float Voulu = Vol.X * ((bCoupe || !Reglages.bActif) ? 1.f : VolumeOptionsCache);
				if (!FMath::IsNearlyEqual(A->VolumeMultiplier, Voulu, 0.005f))
				{
					A->SetVolumeMultiplier(Voulu);
				}
				Vol.Y = A->VolumeMultiplier;
			}
		}
	}

	if (Maintenant >= ProchaineRecherche)
	{
		ProchaineRecherche = Maintenant + 1.f;
		Rechercher();
		KlaxonsIA(Maintenant);
	}

	// Point d'ecoute : camera du joueur
	FVector Ecoute = FVector::ZeroVector;
	APlayerController* PC = JoueurLocal();
	if (PC && PC->PlayerCameraManager)
	{
		Ecoute = PC->PlayerCameraManager->GetCameraLocation();
	}
	else if (PC && PC->GetPawn())
	{
		Ecoute = PC->GetPawn()->GetActorLocation();
	}
	AActor* MonVehicule = VehiculeDuJoueur();

	// Les moteurs les plus proches sont entendus (limite MoteursMax)
	TArray<FVehiculeSuivi*, TInlineAllocator<64>> Candidats;
	for (FVehiculeSuivi& V : Vehicules)
	{
		AActor* A = V.Acteur.Get();
		V.bEcoute = false;
		if (!A)
		{
			continue;
		}
		V.Distance = FVector::Dist(A->GetActorLocation(), Ecoute);
		const bool bActifIci = V.Genre == EGenreVehicule::Voiture || V.Genre == EGenreVehicule::Bus
			|| EstConduitParUnJoueur(A) || V.Vitesse > 80.f;
		if (A == MonVehicule || (bActifIci && !A->IsHidden() && V.Distance < Reglages.DistanceMoteur))
		{
			Candidats.Add(&V);
		}
	}
	Candidats.Sort([MonVehicule](const FVehiculeSuivi& X, const FVehiculeSuivi& Y)
	{
		const bool bX = X.Acteur.Get() == MonVehicule;
		const bool bY = Y.Acteur.Get() == MonVehicule;
		return bX != bY ? bX : X.Distance < Y.Distance;
	});
	for (int32 i = 0; i < Candidats.Num() && i < Reglages.MoteursMax; ++i)
	{
		Candidats[i]->bEcoute = true;
	}

	const float VolumeGlobal = (bCoupe || !Reglages.bActif) ? 0.f : Reglages.Volume * VolumeOptionsCache;
	for (FVehiculeSuivi& V : Vehicules)
	{
		MettreAJourVehicule(V, DeltaSeconds, Maintenant, Ecoute, VolumeGlobal);
		MettreAJourKlaxon(V, Maintenant);
	}
	KlaxonJoueur(Maintenant);
}

void ASonsVehicules::MettreAJourVehicule(FVehiculeSuivi& V, float Dt, float Maintenant, const FVector& Ecoute, float VolumeGlobal)
{
	AActor* A = V.Acteur.Get();
	if (!A || Dt <= 0.f)
	{
		return;
	}

	// Vitesse et acceleration
	const FVector Position = A->GetActorLocation();
	float Instant = V.Vitesse;
	if (V.bPositionConnue)
	{
		const float Deplacement = FVector::Dist(Position, V.DernierePosition);
		if (Deplacement > 2000.f)
		{
			// Teleportation (voiture recyclee par la circulation) : on repart de zero
			V.Vitesse = 0.f;
			V.Acceleration = 0.f;
			Instant = 0.f;
		}
		else
		{
			Instant = Deplacement / Dt;
		}
	}
	const float Physique = A->GetVelocity().Size();
	if (Physique > 1.f && V.Genre != EGenreVehicule::Voiture)
	{
		Instant = Physique;
	}
	V.DernierePosition = Position;
	V.bPositionConnue = true;
	const float Ancienne = V.Vitesse;
	V.Vitesse = Vers(V.Vitesse, Instant, Dt, 6.f);
	V.Acceleration = Vers(V.Acceleration, (V.Vitesse - Ancienne) / Dt, Dt, 3.f);

	// Arrets (klaxons de l'IA, bus qui repart)
	if (V.Vitesse < 50.f)
	{
		V.TempsArret += Dt;
	}
	else
	{
		if (V.Genre == EGenreVehicule::Bus && Reglages.bKlaxonBusDepart && V.TempsArret > 4.f && V.Distance < 6000.f)
		{
			Klaxonner(A, 0.35f, 2); // tut-tut : le bus repart
		}
		V.TempsArret = 0.f;
	}

	FMoteurReglages& M = ReglagesMoteur(V.Genre);

	// Bus : son moteur de pack ne suit pas la vitesse, on le coupe
	if (V.Genre == EGenreVehicule::Bus)
	{
		for (TWeakObjectPtr<UAudioComponent>& P : V.MoteursPack)
		{
			if (P.IsValid() && P->IsPlaying())
			{
				P->Stop();
			}
		}
	}

	const bool bJoue = VolumeGlobal > 0.f && V.bEcoute && !A->IsHidden() && !MoteurPackActif(V);
	V.Regime = Vers(V.Regime <= 0.f ? M.Ralenti : V.Regime, RegimeCible(V, M), Dt, 5.f);
	const float Charge = FMath::Clamp(V.Acceleration / 300.f, 0.f, 1.f);
	float Cible = bJoue ? M.Volume * VolumeGlobal * (0.8f + 0.2f * Charge) : 0.f;
	if (V.Genre == EGenreVehicule::Voiture && V.Vitesse < 80.f)
	{
		Cible *= 0.8f; // ralenti plus discret
	}
	V.VolumeActuel = FMath::FInterpConstantTo(V.VolumeActuel, Cible, Dt, 1.5f);

	UAudioComponent* Moteur = V.Moteur.Get();
	if (V.VolumeActuel <= 0.005f)
	{
		if (Moteur && Moteur->IsPlaying())
		{
			Moteur->Stop();
		}
		return;
	}
	if (!Moteur)
	{
		USoundBase* Son = ChargerSon(M);
		if (!Son || !A->GetRootComponent())
		{
			return;
		}
		Moteur = NewObject<UAudioComponent>(A, MakeUniqueObjectName(A, UAudioComponent::StaticClass(), TEXT("MoteurChelles")));
		Moteur->bAutoActivate = false;
		Moteur->bAutoDestroy = false;
		Moteur->bAllowSpatialization = true;
		Moteur->bOverrideAttenuation = true;
		Moteur->AttenuationOverrides.bAttenuate = true;
		Moteur->AttenuationOverrides.bSpatialize = true;
		Moteur->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		Moteur->AttenuationOverrides.AttenuationShapeExtents = FVector(500.f, 0.f, 0.f);
		Moteur->AttenuationOverrides.FalloffDistance = FMath::Max(500.f, Reglages.DistanceMoteur - 500.f);
		Moteur->SetSound(Son);
		Moteur->SetupAttachment(A->GetRootComponent());
		Moteur->SetRelativeLocation(FVector(100.f, 0.f, 60.f));
		Moteur->RegisterComponent();
		V.Moteur = Moteur;
	}
	Moteur->SetFloatParameter(ParamRegime, V.Regime);
	Moteur->SetFloatParameter(ParamVitesse, V.Vitesse * 0.036f);
	Moteur->SetPitchMultiplier(V.Variation * M.Grave);
	Moteur->SetVolumeMultiplier(V.VolumeActuel);
	if (!Moteur->IsPlaying())
	{
		Moteur->Play(FMath::FRandRange(0.f, 2.f)); // depart decale : les voitures ne ronronnent pas en choeur
	}
}

// ---------------------------------------------------------------- Klaxon

void ASonsVehicules::Klaxonner(AActor* Vehicule, float Duree, int32 Coups)
{
	UWorld* Monde = GetWorld();
	if (!Vehicule || !Monde || !Vehicule->GetRootComponent())
	{
		return;
	}
	FVehiculeSuivi* V = Trouver(Vehicule);
	if (!V)
	{
		Ajouter(Vehicule, EGenreVehicule::Conduite);
		V = Trouver(Vehicule);
	}
	USoundBase* Son = SonKlaxon();
	const float Volume = (bCoupe || !Reglages.bActif) ? 0.f : Reglages.KlaxonVolume * Reglages.Volume * VolumeOptionsCache;
	if (!V || !Son || Volume < 0.01f)
	{
		return;
	}
	if (UAudioComponent* Ancien = V->KlaxonEnCours.Get())
	{
		Ancien->Stop();
	}
	float Hauteur = (V->Genre == EGenreVehicule::Bus) ? Reglages.KlaxonBusGrave : 1.f;
	if (V->Genre == EGenreVehicule::Voiture)
	{
		Hauteur *= V->Variation;
	}
	UAudioComponent* K = UGameplayStatics::SpawnSoundAttached(Son, Vehicule->GetRootComponent(), NAME_None,
		FVector(150.f, 0.f, 80.f), EAttachLocation::KeepRelativeOffset, true, Volume, Hauteur, 0.f, nullptr, nullptr, true);
	V->KlaxonEnCours = K;
	const float Maintenant = Monde->GetTimeSeconds();
	// Duree <= 0 : tenu par le joueur (securite : 8 s maximum)
	V->FinKlaxon = Duree > 0.f ? Maintenant + Duree : Maintenant + 8.f;
	V->CoupsRestants = FMath::Max(0, Coups - 1);
	V->ProchainCoup = 0.f;
}

void ASonsVehicules::ArreterKlaxon(AActor* Vehicule)
{
	if (FVehiculeSuivi* V = Trouver(Vehicule))
	{
		if (UAudioComponent* K = V->KlaxonEnCours.Get())
		{
			K->Stop();
		}
		V->KlaxonEnCours = nullptr;
		V->CoupsRestants = 0;
	}
}

void ASonsVehicules::MettreAJourKlaxon(FVehiculeSuivi& V, float Maintenant)
{
	UAudioComponent* K = V.KlaxonEnCours.Get();
	if (K && Maintenant >= V.FinKlaxon)
	{
		K->Stop();
		V.KlaxonEnCours = nullptr;
		K = nullptr;
	}
	if (!K && V.CoupsRestants > 0 && V.ProchainCoup <= 0.f)
	{
		V.ProchainCoup = Maintenant + 0.12f; // petite pause entre deux coups : tut... tut
	}
	if (!K && V.CoupsRestants > 0 && Maintenant >= V.ProchainCoup)
	{
		const int32 Reste = V.CoupsRestants;
		Klaxonner(V.Acteur.Get(), 0.22f, 1);
		V.CoupsRestants = Reste - 1;
	}
}

void ASonsVehicules::KlaxonJoueur(float Maintenant)
{
	APlayerController* PC = JoueurLocal();
	AActor* Vehicule = VehiculeDuJoueur();
	FVehiculeSuivi* V = Vehicule ? Trouver(Vehicule) : nullptr;
	// La motoneige a deja son klaxon (action Horn = H) : on ne le double pas
	const bool bPeut = PC && V && !V->bKlaxonPack && !bCoupe && Reglages.bActif;
	const bool bAppuye = bPeut && PC->IsInputKeyDown(FKey(FName(*Reglages.ToucheKlaxon)));

	auto Envoyer = [this, PC](AActor* Cible, bool bOn)
	{
		if (!Cible)
		{
			return;
		}
		if (bOn)
		{
			Klaxonner(Cible, 0.f, 1);
		}
		else
		{
			ArreterKlaxon(Cible);
		}
		if (GetNetMode() != NM_Standalone && PC)
		{
			for (TActorIterator<ARelaisKlaxonChelles> It(GetWorld()); It; ++It)
			{
				if (It->GetOwner() == PC)
				{
					It->ServeurKlaxon(Cible, bOn);
					break;
				}
			}
		}
	};

	if (bAppuye && !bKlaxonJoueurTenu)
	{
		bKlaxonJoueurTenu = true;
		VehiculeKlaxonJoueur = Vehicule;
		Envoyer(Vehicule, true);
	}
	else if (bKlaxonJoueurTenu && (!bAppuye || VehiculeKlaxonJoueur.Get() != Vehicule))
	{
		bKlaxonJoueurTenu = false;
		Envoyer(VehiculeKlaxonJoueur.Get(), false);
		VehiculeKlaxonJoueur = nullptr;
	}
	else if (bKlaxonJoueurTenu && V)
	{
		V->FinKlaxon = Maintenant + 8.f; // reste allume tant qu'on appuie
	}
}

void ASonsVehicules::KlaxonsIA(float Maintenant)
{
	if (!Reglages.bKlaxonIA || bCoupe || !Reglages.bActif)
	{
		return;
	}
	TArray<FVehiculeSuivi*, TInlineAllocator<32>> Proches;
	for (FVehiculeSuivi& V : Vehicules)
	{
		AActor* A = V.Acteur.Get();
		if (V.Genre != EGenreVehicule::Voiture || !A || A->IsHidden() || V.Distance > 6000.f || V.KlaxonEnCours.IsValid())
		{
			continue;
		}
		Proches.Add(&V);
		// Voiture bloquee (bouchon, pieton ou joueur devant) : elle s'impatiente
		if (V.TempsArret >= Reglages.KlaxonIAArret && Maintenant >= V.ProchainKlaxon && Maintenant >= ProchainKlaxonGlobal
			&& FMath::FRand() * 100.f < Reglages.KlaxonIAChance)
		{
			Klaxonner(A, FMath::FRandRange(0.25f, 0.8f), FMath::RandRange(1, 2));
			V.ProchainKlaxon = Maintenant + FMath::FRandRange(10.f, 20.f);
			ProchainKlaxonGlobal = Maintenant + 2.5f;
		}
	}
	// Petit coup de klaxon au hasard dans la circulation
	if (Reglages.KlaxonIAHasard > 0.f && Maintenant >= ProchainKlaxonHasard)
	{
		ProchainKlaxonHasard = Maintenant + Reglages.KlaxonIAHasard * FMath::FRandRange(0.5f, 1.5f);
		if (Proches.Num() > 0 && Maintenant >= ProchainKlaxonGlobal)
		{
			FVehiculeSuivi* V = Proches[FMath::RandRange(0, Proches.Num() - 1)];
			Klaxonner(V->Acteur.Get(), FMath::FRandRange(0.18f, 0.4f), FMath::RandRange(1, 2));
			ProchainKlaxonGlobal = Maintenant + 2.5f;
		}
	}
}

// ---------------------------------------------------------------- Divers

void ASonsVehicules::CreerLesRelais()
{
	TSet<AActor*> DejaServis;
	for (TActorIterator<ARelaisKlaxonChelles> It(GetWorld()); It; ++It)
	{
		DejaServis.Add(It->GetOwner());
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !DejaServis.Contains(PC))
		{
			FActorSpawnParameters Params;
			Params.Owner = PC;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<ARelaisKlaxonChelles>(ARelaisKlaxonChelles::StaticClass(), FTransform::Identity, Params);
		}
	}
}

void ASonsVehicules::CouperTout()
{
	for (FVehiculeSuivi& V : Vehicules)
	{
		if (UAudioComponent* M = V.Moteur.Get())
		{
			M->Stop();
		}
		if (UAudioComponent* K = V.KlaxonEnCours.Get())
		{
			K->Stop();
		}
		V.VolumeActuel = 0.f;
	}
	for (auto& Paire : VolumesPack)
	{
		if (UAudioComponent* A = Paire.Key.Get())
		{
			if (FMath::IsNearlyEqual(A->VolumeMultiplier, Paire.Value.Y, 0.005f))
			{
				A->SetVolumeMultiplier(Paire.Value.X);
			}
			Paire.Value.Y = A->VolumeMultiplier;
		}
	}
}

void ASonsVehicules::Commande(const TArray<FString>& Arguments)
{
	const FString A = Arguments.Num() > 0 ? Arguments[0] : FString();
	if (A == TEXT("0"))
	{
		bCoupe = true;
		CouperTout();
		Message(TEXT("coupes"));
		return;
	}
	if (A == TEXT("1") || (A.IsEmpty() && bCoupe))
	{
		bCoupe = false;
		Message(TEXT("remis"));
		return;
	}
	if (A.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		CouperTout();
		for (FVehiculeSuivi& V : Vehicules)
		{
			if (UAudioComponent* M = V.Moteur.Get())
			{
				M->DestroyComponent();
			}
		}
		Vehicules.Reset();
		SonsGardes.Reset();
		SonKlaxonCharge = nullptr;
		bKlaxonTente = false;
		LireReglages();
		Message(TEXT("SonsVehicules.txt relu"));
		return;
	}
	if (A.Equals(TEXT("Klaxon"), ESearchCase::IgnoreCase))
	{
		FVehiculeSuivi* Proche = nullptr;
		for (FVehiculeSuivi& V : Vehicules)
		{
			if (V.Acteur.IsValid() && (!Proche || V.Distance < Proche->Distance))
			{
				Proche = &V;
			}
		}
		if (Proche)
		{
			Klaxonner(Proche->Acteur.Get(), 0.6f, 2);
			Message(FString::Printf(TEXT("klaxon de %s (%d m)"), *Proche->Acteur->GetName(), FMath::RoundToInt(Proche->Distance / 100.f)));
		}
		else
		{
			Message(TEXT("aucun vehicule trouve"));
		}
		return;
	}

	int32 Voitures = 0, Entendus = 0;
	FString Liste;
	for (const FVehiculeSuivi& V : Vehicules)
	{
		Voitures += V.Genre == EGenreVehicule::Voiture ? 1 : 0;
		if (V.bEcoute && V.Acteur.IsValid())
		{
			++Entendus;
			const bool bPack = MoteurPackActif(V);
			Liste += FString::Printf(TEXT("\n  %s : %d m, %d km/h, %d tr/min%s"), *V.Acteur->GetClass()->GetName(),
				FMath::RoundToInt(V.Distance / 100.f), FMath::RoundToInt(V.Vitesse * 0.036f), FMath::RoundToInt(V.Regime),
				bPack ? TEXT(" (son du pack)") : TEXT(""));
		}
	}
	Message(FString::Printf(TEXT("%s | %d vehicules (%d de la circulation), %d entendus | volume menu %d %% | klaxon = %s%s"),
		bCoupe ? TEXT("COUPES") : TEXT("actifs"), Vehicules.Num(), Voitures, Entendus,
		FMath::RoundToInt(VolumeOptionsCache * 100.f), *Reglages.ToucheKlaxon, *Liste));
}

// =====================================================================================================
// Relais (multijoueur)
// =====================================================================================================

ARelaisKlaxonChelles::ARelaisKlaxonChelles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 1.f;
	SetHidden(true);
}

bool ARelaisKlaxonChelles::ServeurKlaxon_Validate(AActor* Vehicule, bool bAppuye)
{
	return true;
}

void ARelaisKlaxonChelles::ServeurKlaxon_Implementation(AActor* Vehicule, bool bAppuye)
{
	if (Vehicule)
	{
		TousKlaxon(Vehicule, bAppuye);
	}
}

void ARelaisKlaxonChelles::TousKlaxon_Implementation(AActor* Vehicule, bool bAppuye)
{
	UWorld* Monde = GetWorld();
	if (!Monde || !Vehicule || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Le joueur qui klaxonne l'entend deja chez lui
	const APawn* P = Cast<APawn>(Vehicule);
	if (P && P->IsLocallyControlled())
	{
		return;
	}
	for (TActorIterator<ASonsVehicules> It(Monde); It; ++It)
	{
		if (bAppuye)
		{
			It->Klaxonner(Vehicule, 0.f, 1);
		}
		else
		{
			It->ArreterKlaxon(Vehicule);
		}
		return;
	}
}
