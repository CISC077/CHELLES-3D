// CHELLES 3D - Voitures de police (voir PoliceChelles.h).
#include "PoliceChelles.h"
#include "RelaisPolice.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/UnrealType.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "TireConfig.h"
#include "VehicleWheel.h"
#include "WheeledVehicleMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPolice, Log, All);

namespace
{
	const FName TAG_POSEE(TEXT("ChellesPolicePosee"));
	const TCHAR* CHEMIN_BP_POLICE = TEXT("/Game/SportVehiclesPack/Blueprints/Vehicles/BP_Police_Drivable.BP_Police_Drivable_C");
	const TCHAR* CHEMIN_BP_PACK = TEXT("/Game/SportVehiclesPack/Blueprints/Vehicles/BP_WheeledVehicle_Sportcar1_Police.BP_WheeledVehicle_Sportcar1_Police_C");
	constexpr float CmsVersKmh = 0.036f;

	UClass* TrouverClasse(const TCHAR* CheminScript, const TCHAR* Nom)
	{
		UClass* Classe = FindObject<UClass>(nullptr, CheminScript);
		if (!Classe)
		{
			Classe = FindObject<UClass>(ANY_PACKAGE, Nom);
		}
		return Classe;
	}

	bool EstOui(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		return T.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || T.Equals(TEXT("1")) || T.Equals(TEXT("True"), ESearchCase::IgnoreCase)
			|| T.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	FKey LireTouche(const FString& Nom, const FKey& Defaut)
	{
		const FKey Touche(*Nom.TrimStartAndEnd());
		return Touche.IsValid() ? Touche : Defaut;
	}

	UWheeledVehicleMovementComponent* Roulement(const AActor* Acteur)
	{
		return Acteur ? Acteur->FindComponentByClass<UWheeledVehicleMovementComponent>() : nullptr;
	}

	UPrimitiveComponent* Caisse(const AActor* Acteur)
	{
		return Acteur ? Cast<UPrimitiveComponent>(Acteur->GetRootComponent()) : nullptr;
	}

	bool AppelerBool(AActor* Acteur, const TCHAR* NomFonction)
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

	void Appeler(AActor* Acteur, const TCHAR* NomFonction)
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
}

// =====================================================================================================
// Construction, reglages
// =====================================================================================================

APoliceChelles::APoliceChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	SetActorHiddenInGame(true);
	bReplicates = false; // un par machine

	Classes = { TEXT("BP_Police_Drivable"), TEXT("BP_WheeledVehicle_Sportcar1_Police"), TEXT("BP_WheeledVehicle_Sportcar2_Police") };
	CheminClassePosee = CHEMIN_BP_POLICE;
	TouchePoser = EKeys::F8;
	ToucheRetirer = EKeys::F9;
	ToucheSirene = EKeys::L;
	ToucheMonter = EKeys::E;
}

FString APoliceChelles::DossierPlugin() const
{
	return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesPolice"));
}

void APoliceChelles::LireReglages()
{
	const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Police.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogPolice, Warning, TEXT("[Police] %s introuvable : reglages par defaut."), *Chemin);
		return;
	}
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

		if (Est(TEXT("Classes")))
		{
			TArray<FString> Noms;
			Valeur.ParseIntoArray(Noms, TEXT(","), true);
			Classes.Reset();
			for (const FString& Nom : Noms)
			{
				if (!Nom.TrimStartAndEnd().IsEmpty())
				{
					Classes.Add(Nom.TrimStartAndEnd());
				}
			}
		}
		else if (Est(TEXT("VoitureAPoser")))     { if (!Valeur.IsEmpty()) { CheminClassePosee = Valeur; ClassePosee = nullptr; } }
		else if (Est(TEXT("TouchePoser")))       { TouchePoser = LireTouche(Valeur, EKeys::F8); }
		else if (Est(TEXT("ToucheRetirer")))     { ToucheRetirer = LireTouche(Valeur, EKeys::F9); }
		else if (Est(TEXT("ToucheSirene")))      { ToucheSirene = LireTouche(Valeur, EKeys::L); }
		else if (Est(TEXT("ToucheMonter")))      { ToucheMonter = LireTouche(Valeur, EKeys::E); }
		else if (Est(TEXT("Administrateurs")))
		{
			Administrateurs.Reset();
			Valeur.ParseIntoArray(Administrateurs, TEXT(","), true);
			for (FString& Nom : Administrateurs)
			{
				Nom.TrimStartAndEndInline();
			}
		}
		else if (Est(TEXT("MaxVoitures")))       { MaxVoitures = FMath::Clamp(FMath::RoundToInt(Nombre), 1, 200); }
		else if (Est(TEXT("DistanceMonter")))    { DistanceMonter = FMath::Clamp(Nombre, 1.f, 20.f) * 100.f; }
		else if (Est(TEXT("SireneAuto")))        { bSireneAuto = EstOui(Valeur); bSireneSolo = bSireneAuto; }
		else if (Est(TEXT("DegagerRoute")))      { bDegager = EstOui(Valeur); }
		else if (Est(TEXT("VitesseDegager")))    { VitesseDegager = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("DistanceDegager")))   { DistanceDegager = FMath::Clamp(Nombre, 10.f, 300.f) * 100.f; }
		else if (Est(TEXT("LargeurDegager")))    { LargeurDegager = FMath::Clamp(Nombre, 1.f, 15.f) * 100.f; }
		else if (Est(TEXT("DureeRangement")))    { DureeRangement = FMath::Clamp(Nombre, 0.5f, 30.f); }
		else if (Est(TEXT("DistanceCamera")))    { DistanceCamera = FMath::Clamp(Nombre, 1.f, 30.f) * 100.f; }
		else if (Est(TEXT("HauteurCamera")))     { HauteurCamera = FMath::Clamp(Nombre, 0.f, 10.f) * 100.f; }
		else if (Est(TEXT("AideConduite")))      { bAide = EstOui(Valeur); }
		else if (Est(TEXT("Adherence")))         { Adherence = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("VirageMax")))         { VirageMaxDeg = FMath::Max(5.f, Nombre); }
		else if (Est(TEXT("RayonMini")))         { RayonMini = FMath::Max(1.f, Nombre); }
		else if (Est(TEXT("ForceVirage")))       { AccelLaterale = FMath::Max(1.f, Nombre); }
		else if (Est(TEXT("AntiRoulis")))        { AntiRoulis = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("Appui")))             { Appui = FMath::Clamp(Nombre, 0.f, 3.f); }
		else if (Est(TEXT("AbaisserCentre")))    { AbaisserCentre = FMath::Clamp(Nombre, -200.f, 200.f); }
		else if (Est(TEXT("Friction")))          { Friction = FMath::Clamp(Nombre, 0.1f, 10.f); }
		else if (Est(TEXT("Rigidite")))          { Rigidite = FMath::Clamp(Nombre, 0.1f, 10.f); }
		else if (Est(TEXT("Braquage")))          { Braquage = FMath::Clamp(Nombre, 0.f, 80.f); }
		else if (Est(TEXT("Amortissement")))     { Amortissement = FMath::Clamp(Nombre, 0.f, 5.f); }
	}
}

void APoliceChelles::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	UE_LOG(LogPolice, Log, TEXT("[Police] Pret (%s). Voitures reconnues : %s. Administrateur : %s poser, %s retirer."),
		EstServeur() ? (EstMultijoueur() ? TEXT("serveur") : TEXT("solo")) : TEXT("client"),
		*FString::Join(Classes, TEXT(", ")), *TouchePoser.ToString(), *ToucheRetirer.ToString());
}

void APoliceChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerCamera();
	if (bEmprunt && MotoneigeEmpruntee.IsValid() && ClasseMotoneige)
	{
		if (FObjectProperty* PropMoto = FindFProperty<FObjectProperty>(ClasseMotoneige, TEXT("Moto")))
		{
			PropMoto->SetObjectPropertyValue_InContainer(MotoneigeEmpruntee.Get(), MotoAvantEmprunt.Get());
		}
	}
	RemettreRoues();
	Super::EndPlay(Raison);
}

// =====================================================================================================
// Reseau
// =====================================================================================================

bool APoliceChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

bool APoliceChelles::EstMultijoueur() const
{
	return GetNetMode() != NM_Standalone;
}

APlayerController* APoliceChelles::PCLocal() const
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

ARelaisPolice* APoliceChelles::MonRelais() const
{
	APlayerController* PC = PCLocal();
	if (!PC || !GetWorld())
	{
		return nullptr;
	}
	for (TActorIterator<ARelaisPolice> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

bool APoliceChelles::EstAdministrateur(APlayerController* PC) const
{
	if (!PC)
	{
		return false;
	}
	if (!EstMultijoueur() || PC->IsLocalController())
	{
		return true; // solo, ou l'hote de la partie
	}
	const FString Nom = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	if (Administrateurs.Num() > 0)
	{
		for (const FString& Admin : Administrateurs)
		{
			if (!Admin.IsEmpty() && Nom.Equals(Admin, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
	// Serveur dedie (VPS) sans liste : le premier joueur connecte est l'administrateur.
	if (GetNetMode() == NM_DedicatedServer)
	{
		const AGameStateBase* Etat = GetWorld() ? GetWorld()->GetGameState() : nullptr;
		if (Etat)
		{
			for (APlayerState* Joueur : Etat->PlayerArray)
			{
				if (Joueur && !Joueur->IsInactive())
				{
					return Joueur == PC->PlayerState;
				}
			}
		}
	}
	return false;
}

void APoliceChelles::GererRelaisServeur()
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	TArray<APlayerController*> DejaServis;
	TArray<ARelaisPolice*> Relais;
	for (TActorIterator<ARelaisPolice> It(Monde); It; ++It)
	{
		Relais.Add(*It);
	}
	for (ARelaisPolice* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			// Joueur parti : son soldat cache disparait avec son relais
			if (IsValid(R->PietonCache))
			{
				R->PietonCache->Destroy();
			}
			R->Destroy();
			continue;
		}
		DejaServis.Add(PC);
		const bool bAdmin = EstAdministrateur(PC);
		APawn* Pion = PC->GetPawn();
		APawn* Conduite = (Pion && EstPolice(Pion)) ? Pion : nullptr;
		if (R->bAdmin != bAdmin || R->Voiture != Conduite)
		{
			R->bAdmin = bAdmin;
			R->Voiture = Conduite;
			R->ForceNetUpdate();
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
		if (ARelaisPolice* Nouveau = Monde->SpawnActor<ARelaisPolice>(ARelaisPolice::StaticClass(), FTransform::Identity, Params))
		{
			Nouveau->bSirene = bSireneAuto;
			Nouveau->bAdmin = EstAdministrateur(PC);
			UE_LOG(LogPolice, Log, TEXT("[Police] Relais pret pour %s (administrateur : %s)."), *PC->GetName(), Nouveau->bAdmin ? TEXT("oui") : TEXT("non"));
		}
	}
}

void APoliceChelles::Message(const FString& Texte, float Duree, int32 Cle) const
{
	UE_LOG(LogPolice, Log, TEXT("[Police] %s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(Cle, Duree, FColor(110, 170, 255), Texte);
	}
}

void APoliceChelles::Avis(APlayerController* PC, const FString& Texte, float Duree) const
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
	for (TActorIterator<ARelaisPolice> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			It->ClientAvis(Texte, Duree);
			return;
		}
	}
}

// =====================================================================================================
// Voitures connues
// =====================================================================================================

bool APoliceChelles::EstPolice(const AActor* Acteur) const
{
	if (!IsValid(Acteur) || !Roulement(Acteur))
	{
		return false;
	}
	for (const UClass* Classe = Acteur->GetClass(); Classe; Classe = Classe->GetSuperClass())
	{
		const FString Nom = Classe->GetName();
		for (const FString& Cherche : Classes)
		{
			if (Nom.StartsWith(Cherche, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	return false;
}

void APoliceChelles::ChercherVoitures()
{
	Suivies.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !P.IsValid(); });
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Voiture = *It;
		if (!Voiture || Suivies.Contains(Voiture) || !EstPolice(Voiture))
		{
			continue;
		}
		Suivies.Add(Voiture);
		// Une voiture a physique ne doit pas etre tournee par la souris (sinon elle se tord / saute)
		Voiture->bUseControllerRotationYaw = false;
		Voiture->bUseControllerRotationPitch = false;
		Voiture->bUseControllerRotationRoll = false;
		if (bAide && !Preparees.Contains(Voiture))
		{
			PreparerVoiture(Voiture);
			Preparees.Add(Voiture);
		}
		UE_LOG(LogPolice, Log, TEXT("[Police] Voiture de police reconnue : %s (%s)%s"), *Voiture->GetName(), *Voiture->GetClass()->GetName(),
			EstPosee(Voiture) ? TEXT(" posee par l'administrateur") : TEXT(""));
	}
}

TArray<APawn*> APoliceChelles::Voitures() const
{
	TArray<APawn*> Liste;
	for (const TWeakObjectPtr<APawn>& P : Suivies)
	{
		if (P.IsValid())
		{
			Liste.Add(P.Get());
		}
	}
	return Liste;
}

bool APoliceChelles::EstConduite(const APawn* Voiture) const
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

bool APoliceChelles::EstPosee(const AActor* Voiture) const
{
	return Voiture && Voiture->ActorHasTag(TAG_POSEE);
}

// =====================================================================================================
// Pose par l'administrateur (serveur)
// =====================================================================================================

UClass* APoliceChelles::ClasseAPoser()
{
	if (ClassePosee)
	{
		return ClassePosee;
	}
	ClassePosee = LoadClass<APawn>(nullptr, *CheminClassePosee);
	if (!ClassePosee)
	{
		UE_LOG(LogPolice, Warning, TEXT("[Police] %s introuvable (lancez Tools\\Police\\INSTALLER_POLICE.bat) : j'utilise la voiture du pack."),
			*CheminClassePosee);
		ClassePosee = LoadClass<APawn>(nullptr, CHEMIN_BP_PACK);
	}
	return ClassePosee;
}

APawn* APoliceChelles::PoserVoiture(const FVector& Lieu, float Yaw, bool bSauver)
{
	UWorld* Monde = GetWorld();
	if (!Monde || !EstServeur())
	{
		return nullptr;
	}
	if (PlacesPosees.Num() >= MaxVoitures)
	{
		UE_LOG(LogPolice, Warning, TEXT("[Police] Deja %d voitures posees (MaxVoitures dans Police.txt)."), PlacesPosees.Num());
		return nullptr;
	}
	UClass* Classe = ClasseAPoser();
	if (!Classe)
	{
		UE_LOG(LogPolice, Error, TEXT("[Police] Aucune voiture de police trouvee : le pack SportVehiclesPack est-il dans Content ?"));
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Rot(0.f, Yaw, 0.f);
	APawn* Voiture = Monde->SpawnActor<APawn>(Classe, Lieu + FVector(0.f, 0.f, 60.f), Rot, Params);
	if (!Voiture)
	{
		UE_LOG(LogPolice, Warning, TEXT("[Police] La voiture n'a pas pu apparaitre en %s."), *Lieu.ToString());
		return nullptr;
	}
	Voiture->Tags.AddUnique(TAG_POSEE);
	Voiture->SetReplicates(true);
	Voiture->SetReplicateMovement(true);
	ArreterVoiture(Voiture);
	Posees.Add(Voiture);
	PlacesPosees.Add(FTransform(Rot, Lieu));
	if (bSauver)
	{
		SauverPositions();
	}
	UE_LOG(LogPolice, Log, TEXT("[Police] Voiture posee en (%.0f, %.0f, %.0f), cap %.0f (%d en tout)."), Lieu.X, Lieu.Y, Lieu.Z, Yaw, PlacesPosees.Num());
	return Voiture;
}

bool APoliceChelles::RetirerVoitureProche(const FVector& Point, FString& OutRaison)
{
	int32 Meilleur = INDEX_NONE;
	float MeilleureDistance = FMath::Square(800.f);
	for (int32 i = 0; i < PlacesPosees.Num(); ++i)
	{
		const FVector Ici = (Posees.IsValidIndex(i) && Posees[i].IsValid()) ? Posees[i]->GetActorLocation() : PlacesPosees[i].GetLocation();
		const float D = FVector::DistSquared(Ici, Point);
		if (D < MeilleureDistance)
		{
			MeilleureDistance = D;
			Meilleur = i;
		}
	}
	if (Meilleur == INDEX_NONE)
	{
		OutRaison = FString::Printf(TEXT("Vise une voiture de police posee (a moins de 8 m) puis %s."), *ToucheRetirer.ToString());
		return false;
	}
	APawn* Voiture = Posees.IsValidIndex(Meilleur) ? Posees[Meilleur].Get() : nullptr;
	if (Voiture && EstConduite(Voiture))
	{
		OutRaison = TEXT("Quelqu'un conduit cette voiture : attends qu'il descende.");
		return false;
	}
	if (Voiture)
	{
		Voiture->Destroy();
	}
	if (Posees.IsValidIndex(Meilleur))
	{
		Posees.RemoveAt(Meilleur);
	}
	PlacesPosees.RemoveAt(Meilleur);
	SauverPositions();
	return true;
}

void APoliceChelles::ChargerPositions()
{
	const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Police_Positions.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		return; // aucune voiture posee pour l'instant
	}
	int32 Nombre = 0;
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
		{
			continue;
		}
		TArray<FString> Morceaux;
		Ligne.ParseIntoArray(Morceaux, TEXT(";"), true);
		if (Morceaux.Num() < 4)
		{
			continue;
		}
		const FVector Lieu(FCString::Atof(*Morceaux[0]), FCString::Atof(*Morceaux[1]), FCString::Atof(*Morceaux[2]));
		if (PoserVoiture(Lieu, FCString::Atof(*Morceaux[3]), false))
		{
			++Nombre;
		}
	}
	UE_LOG(LogPolice, Log, TEXT("[Police] %d voiture(s) de police remise(s) a leur place (Police_Positions.txt)."), Nombre);
}

void APoliceChelles::SauverPositions() const
{
	TArray<FString> Lignes;
	Lignes.Add(TEXT("# CHELLES 3D - voitures de police posees par l'administrateur (F8 poser, F9 retirer en jeu)."));
	Lignes.Add(TEXT("# Une voiture par ligne : X;Y;Z;Cap. Supprimez une ligne pour retirer une voiture."));
	for (const FTransform& Place : PlacesPosees)
	{
		const FVector L = Place.GetLocation();
		Lignes.Add(FString::Printf(TEXT("%.0f;%.0f;%.0f;%.0f"), L.X, L.Y, L.Z, Place.Rotator().Yaw));
	}
	const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Police_Positions.txt"));
	if (!FFileHelper::SaveStringArrayToFile(Lignes, *Chemin))
	{
		UE_LOG(LogPolice, Warning, TEXT("[Police] Impossible d'ecrire %s"), *Chemin);
	}
}

bool APoliceChelles::ViserLeSol(APlayerController* PC, FVector& OutPoint, float& OutYaw) const
{
	if (!PC || !GetWorld())
	{
		return false;
	}
	FVector Oeil;
	FRotator Vue;
	PC->GetPlayerViewPoint(Oeil, Vue);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PoliceViser), false, PC->GetPawn());
	FHitResult Impact;
	if (!GetWorld()->LineTraceSingleByChannel(Impact, Oeil, Oeil + Vue.Vector() * 10000.f, ECC_Visibility, Params))
	{
		return false;
	}
	OutPoint = Impact.ImpactPoint;
	OutYaw = Vue.Yaw;
	return true;
}

// =====================================================================================================
// Monter / descendre
// =====================================================================================================

APawn* APoliceChelles::VoitureLibreProche(const APawn* Pieton, float Rayon) const
{
	if (!Pieton)
	{
		return nullptr;
	}
	APawn* Meilleure = nullptr;
	float MeilleureDistance = FMath::Square(Rayon + 200.f); // + demi-longueur de la voiture
	for (APawn* Voiture : Voitures())
	{
		if (Voiture == Pieton || EstConduite(Voiture) || Voiture->IsHidden())
		{
			continue;
		}
		const float D = FVector::DistSquared(Voiture->GetActorLocation(), Pieton->GetActorLocation());
		if (D < MeilleureDistance)
		{
			MeilleureDistance = D;
			Meilleure = Voiture;
		}
	}
	return Meilleure;
}

AActor* APoliceChelles::ControleurMotoneigePolice(bool bCreer)
{
	if (!ClasseMotoneige)
	{
		ClasseMotoneige = TrouverClasse(TEXT("/Script/ChellesMotoneige.Motoneige"), TEXT("Motoneige"));
	}
	UClass* Police = ClasseAPoser();
	FClassProperty* PropClasse = ClasseMotoneige ? FindFProperty<FClassProperty>(ClasseMotoneige, TEXT("ClasseMoto")) : nullptr;
	if (!ClasseMotoneige || !Police || !PropClasse)
	{
		return nullptr;
	}
	// Celui de la case POLICE de la roue TAB s'il existe deja (meme classe de vehicule)
	for (TActorIterator<AActor> It(GetWorld(), ClasseMotoneige); It; ++It)
	{
		if (PropClasse->GetObjectPropertyValue_InContainer(*It) == Police)
		{
			return *It;
		}
	}
	if (!bCreer)
	{
		return nullptr;
	}
	// Sinon on en cree un, sans touche ; la roue le reprendra pour sa case POLICE.
	const FTransform Lieu = FTransform::Identity;
	AActor* Nouveau = GetWorld()->SpawnActorDeferred<AActor>(ClasseMotoneige, Lieu, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Nouveau)
	{
		return nullptr;
	}
	PropClasse->SetObjectPropertyValue_InContainer(Nouveau, Police);
	if (FStructProperty* PropTouche = FindFProperty<FStructProperty>(ClasseMotoneige, TEXT("Touche")))
	{
		if (PropTouche->Struct && PropTouche->Struct->GetFName() == FName(TEXT("Key")))
		{
			*PropTouche->ContainerPtrToValuePtr<FKey>(Nouveau) = EKeys::Invalid;
		}
	}
	Nouveau->FinishSpawning(Lieu);
	UE_LOG(LogPolice, Log, TEXT("[Police] Conduite prete (acteur %s)."), *Nouveau->GetName());
	return Nouveau;
}

void APoliceChelles::MonterSolo(APawn* Voiture)
{
	// En solo on passe par le plugin ChellesMotoneige (comme la case POLICE de la roue) :
	// il cache le soldat, donne la voiture au joueur, et E fait descendre.
	AActor* Conduite = ControleurMotoneigePolice(true);
	FObjectProperty* PropMoto = ClasseMotoneige ? FindFProperty<FObjectProperty>(ClasseMotoneige, TEXT("Moto")) : nullptr;
	if (!Conduite || !PropMoto)
	{
		Message(TEXT("Impossible de monter : le plugin ChellesMotoneige n'est pas compile."));
		return;
	}
	if (AppelerBool(Conduite, TEXT("EstSurMoto")))
	{
		return;
	}
	APawn* Avant = Cast<APawn>(PropMoto->GetObjectPropertyValue_InContainer(Conduite));
	if (Avant != Voiture)
	{
		MotoAvantEmprunt = Avant;
		MotoneigeEmpruntee = Conduite;
		bEmprunt = true;
		PropMoto->SetObjectPropertyValue_InContainer(Conduite, Voiture);
	}
	Appeler(Conduite, TEXT("Basculer"));
	if (!AppelerBool(Conduite, TEXT("EstSurMoto")))
	{
		Message(TEXT("Impossible de monter dans la voiture de police (voir le journal)."));
	}
}

void APoliceChelles::SuivreMonteeSolo()
{
	if (!bEmprunt)
	{
		return;
	}
	AActor* Conduite = MotoneigeEmpruntee.Get();
	if (!Conduite || !ClasseMotoneige)
	{
		bEmprunt = false;
		return;
	}
	if (AppelerBool(Conduite, TEXT("EstSurMoto")))
	{
		return;
	}
	// Descendu : la case POLICE de la roue retrouve sa propre voiture (la voiture posee reste a sa place)
	if (FObjectProperty* PropMoto = FindFProperty<FObjectProperty>(ClasseMotoneige, TEXT("Moto")))
	{
		PropMoto->SetObjectPropertyValue_InContainer(Conduite, MotoAvantEmprunt.Get());
	}
	bEmprunt = false;
	MotoAvantEmprunt = nullptr;
}

FVector APoliceChelles::PlaceADescendre(const APawn* Voiture, const APawn* Pieton)
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
		Base - Avant * 380.f + Hauteur,
		Base + Avant * 380.f + Hauteur
	};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PoliceDescente), false);
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

void APoliceChelles::CacherPion(APawn* Pion, bool bCacher)
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

void APoliceChelles::ArreterVoiture(APawn* Voiture)
{
	if (!IsValid(Voiture))
	{
		return;
	}
	if (UWheeledVehicleMovementComponent* Mouvement = Roulement(Voiture))
	{
		Mouvement->SetThrottleInput(0.f);
		Mouvement->SetSteeringInput(0.f);
		Mouvement->SetHandbrakeInput(true);
		// Sans conducteur, le vehicule garde la derniere commande recue : on la remet a zero aussi.
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
	if (UPrimitiveComponent* Corps = Caisse(Voiture))
	{
		if (Corps->IsSimulatingPhysics())
		{
			Corps->SetPhysicsLinearVelocity(Corps->GetPhysicsLinearVelocity() * 0.2f);
		}
	}
}

void APoliceChelles::MonterServeur(ARelaisPolice* Relais, APawn* Voiture)
{
	APlayerController* PC = Relais ? Cast<APlayerController>(Relais->GetOwner()) : nullptr;
	APawn* Pieton = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pieton || !IsValid(Voiture) || !EstPolice(Voiture) || EstPolice(Pieton))
	{
		return;
	}
	if (EstConduite(Voiture))
	{
		Avis(PC, TEXT("Quelqu'un conduit deja cette voiture de police."));
		return;
	}
	if (FVector::Dist(Pieton->GetActorLocation(), Voiture->GetActorLocation()) > DistanceMonter + 500.f)
	{
		Avis(PC, TEXT("Approche-toi de la voiture de police."));
		return;
	}
	// Seulement depuis le soldat : les autres modes de la roue (Spider-Male, cheval...) ont leur propre personnage.
	AGameModeBase* Jeu = GetWorld()->GetAuthGameMode();
	UClass* Soldat = Jeu ? Jeu->GetDefaultPawnClassForController(PC) : nullptr;
	if (!Cast<ACharacter>(Pieton) || (Soldat && !Pieton->IsA(Soldat)))
	{
		Avis(PC, TEXT("Reviens en Soldat (roue TAB) pour monter dans une voiture de police."));
		return;
	}
	if (AController* Ancien = Voiture->GetController())
	{
		Ancien->UnPossess();
	}
	CacherPion(Pieton, true);
	Relais->PietonCache = Pieton;
	Relais->VoitureMontee = Voiture;
	PC->Possess(Voiture);
	const FRotator Rot(-12.f, Voiture->GetActorRotation().Yaw, 0.f);
	PC->SetControlRotation(Rot);
	PC->ClientSetRotation(Rot);
	if (UWheeledVehicleMovementComponent* M = Roulement(Voiture))
	{
		M->SetHandbrakeInput(false);
	}
	Relais->Voiture = Voiture;
	Relais->ForceNetUpdate();
	UE_LOG(LogPolice, Log, TEXT("[Police] %s monte dans %s."), *PC->GetName(), *Voiture->GetName());
}

void APoliceChelles::DescendreServeur(ARelaisPolice* Relais)
{
	APlayerController* PC = Relais ? Cast<APlayerController>(Relais->GetOwner()) : nullptr;
	if (!PC)
	{
		return;
	}
	APawn* Voiture = Relais->VoitureMontee;
	APawn* Pieton = Relais->PietonCache;
	if (!IsValid(Pieton))
	{
		Avis(PC, TEXT("Plus de personnage a pied : utilise la roue TAB > Soldat."));
		return;
	}
	if (IsValid(Voiture) && PC->GetPawn() == Voiture)
	{
		const float Yaw = Voiture->GetActorRotation().Yaw;
		Pieton->SetActorLocationAndRotation(PlaceADescendre(Voiture, Pieton), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		CacherPion(Pieton, false);
		PC->Possess(Pieton);
		PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
		PC->ClientSetRotation(FRotator(0.f, Yaw, 0.f));
		ArreterVoiture(Voiture);
	}
	Relais->PietonCache = nullptr;
	Relais->VoitureMontee = nullptr;
	Relais->Voiture = nullptr;
	Relais->ForceNetUpdate();
}

void APoliceChelles::SuivreMonteesServeur()
{
	UClass* ClasseRelaisRoue = TrouverClasse(TEXT("/Script/ChellesRoue.RoueRelais"), TEXT("RoueRelais"));
	for (TActorIterator<ARelaisPolice> It(GetWorld()); It; ++It)
	{
		ARelaisPolice* R = *It;
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC || !R->VoitureMontee)
		{
			continue;
		}
		APawn* Voiture = R->VoitureMontee;
		APawn* Pieton = R->PietonCache;
		APawn* Actuel = PC->GetPawn();
		if (IsValid(Voiture) && Actuel == Voiture)
		{
			// Le soldat cache suit la voiture (il redescend au bon endroit, et reste pres du joueur pour le reseau)
			if (IsValid(Pieton))
			{
				Pieton->SetActorLocation(Voiture->GetActorLocation() + FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
			}
			continue;
		}
		if (Actuel == nullptr && IsValid(Pieton))
		{
			// Voiture detruite : le joueur reprend son soldat
			CacherPion(Pieton, false);
			PC->Possess(Pieton);
		}
		else if (Actuel && IsValid(Voiture) && ClasseRelaisRoue)
		{
			// La roue TAB a change de mode pendant qu'on conduisait : elle a cache la voiture en la prenant pour le soldat.
			// On lui rend le vrai soldat, et la voiture de police reapparait garee.
			for (TActorIterator<AActor> RoueIt(GetWorld(), ClasseRelaisRoue); RoueIt; ++RoueIt)
			{
				if (RoueIt->GetOwner() != PC)
				{
					continue;
				}
				if (FObjectProperty* PropCache = FindFProperty<FObjectProperty>(ClasseRelaisRoue, TEXT("PionCache")))
				{
					if (PropCache->GetObjectPropertyValue_InContainer(*RoueIt) == Voiture && IsValid(Pieton))
					{
						PropCache->SetObjectPropertyValue_InContainer(*RoueIt, Pieton);
						CacherPion(Voiture, false);
						ArreterVoiture(Voiture);
						RoueIt->ForceNetUpdate();
						Pieton = nullptr; // garde par la roue maintenant
					}
				}
			}
		}
		R->VoitureMontee = nullptr;
		R->PietonCache = IsValid(Pieton) && Actuel != Pieton && Actuel == nullptr ? Pieton : nullptr;
		R->ForceNetUpdate();
	}
}

// =====================================================================================================
// Conduite du joueur local : camera, commandes, sirene, et touches de l'administrateur
// =====================================================================================================

void APoliceChelles::InstallerCamera(APawn* Voiture)
{
	RetirerCamera();
	if (!IsValid(Voiture) || !Voiture->GetRootComponent())
	{
		return;
	}
	USpringArmComponent* Bras = NewObject<USpringArmComponent>(Voiture, TEXT("BrasCameraPolice"));
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

	UCameraComponent* Cam = NewObject<UCameraComponent>(Voiture, TEXT("CameraPolice"));
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
	}
}

void APoliceChelles::RetirerCamera()
{
	if (Camera.IsValid())
	{
		Camera->DestroyComponent();
	}
	if (BrasCamera.IsValid())
	{
		BrasCamera->DestroyComponent();
	}
	Camera = nullptr;
	BrasCamera = nullptr;
	VoitureCamera = nullptr;
}

void APoliceChelles::ToucherSirene()
{
	bool bNouveau = true;
	if (EstMultijoueur())
	{
		ARelaisPolice* R = MonRelais();
		if (!R)
		{
			return;
		}
		bNouveau = !R->bSirene;
		R->ServeurSirene(bNouveau);
	}
	else
	{
		bSireneSolo = !bSireneSolo;
		bNouveau = bSireneSolo;
	}
	Message(bNouveau ? TEXT("Sirene et gyrophares ALLUMES") : TEXT("Sirene et gyrophares eteints"), 2.f, 88002);
}

void APoliceChelles::ConduireLocal(float Dt)
{
	APlayerController* PC = PCLocal();
	if (!PC)
	{
		return;
	}
	APawn* Pion = PC->GetPawn();
	const bool bEnPolice = Pion && EstPolice(Pion);

	// ---- Touches de l'administrateur (a pied ou en voiture)
	const bool bMulti = EstMultijoueur();
	ARelaisPolice* Relais = bMulti ? MonRelais() : nullptr;
	const bool bAdmin = bMulti ? (Relais && Relais->bAdmin) : true;
	if (bAdmin && !bAvisAdmin && Pion)
	{
		bAvisAdmin = true;
		Message(FString::Printf(TEXT("Administrateur : %s = poser une voiture de police la ou tu vises, %s = retirer celle visee."),
			*TouchePoser.ToString(), *ToucheRetirer.ToString()), 8.f);
	}
	if (PC->WasInputKeyJustPressed(TouchePoser) || PC->WasInputKeyJustPressed(ToucheRetirer))
	{
		const bool bPoser = PC->WasInputKeyJustPressed(TouchePoser);
		FVector Point;
		float Yaw = 0.f;
		if (!bAdmin)
		{
			Message(TEXT("Seul l'administrateur du jeu (l'hote) peut poser ou retirer des voitures de police."));
		}
		else if (!ViserLeSol(PC, Point, Yaw))
		{
			Message(FString::Printf(TEXT("Vise la route (a moins de 100 m) puis %s."), *(bPoser ? TouchePoser : ToucheRetirer).ToString()));
		}
		else if (bMulti)
		{
			if (Relais)
			{
				if (bPoser) { Relais->ServeurPoser(Point, Yaw); } else { Relais->ServeurRetirer(Point); }
			}
		}
		else if (bPoser)
		{
			Message(PoserVoiture(Point, Yaw, true)
				? FString::Printf(TEXT("Voiture de police posee (%d sur la carte). Elle sera la a chaque partie."), PlacesPosees.Num())
				: FString(TEXT("Impossible de poser une voiture de police ici (voir le journal [Police]).")));
		}
		else
		{
			FString Raison;
			Message(RetirerVoitureProche(Point, Raison)
				? FString::Printf(TEXT("Voiture de police retiree (%d sur la carte)."), PlacesPosees.Num()) : Raison);
		}
	}

	// ---- A pied : monter avec E
	if (!bEnPolice)
	{
		if (VoitureCamera.IsValid() || Camera.IsValid())
		{
			RetirerCamera();
		}
		bAideAffichee = false;
		if (Pion && Cast<ACharacter>(Pion) && !Pion->IsHidden())
		{
			if (APawn* Proche = VoitureLibreProche(Pion, DistanceMonter))
			{
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(88001, 0.3f, FColor(110, 170, 255),
						FString::Printf(TEXT("%s : monter dans la voiture de police"), *ToucheMonter.ToString()));
				}
				if (PC->WasInputKeyJustPressed(ToucheMonter))
				{
					if (bMulti)
					{
						if (Relais) { Relais->ServeurMonter(Proche); }
					}
					else
					{
						MonterSolo(Proche);
					}
				}
			}
		}
		return;
	}

	// ---- En voiture de police
	if (VoitureCamera.Get() != Pion)
	{
		InstallerCamera(Pion);
	}
	if (!bAideAffichee)
	{
		bAideAffichee = true;
		Message(FString::Printf(TEXT("POLICE : Z/S avancer/reculer, Q/D tourner, Espace frein a main, souris regarder, %s sirene, H klaxon, %s descendre."),
			*ToucheSirene.ToString(), *ToucheMonter.ToString()), 8.f);
	}

	if (UWheeledVehicleMovementComponent* M = Roulement(Pion))
	{
		auto Bas = [PC](const FKey& Touche) { return PC->IsInputKeyDown(Touche); };
		float Gaz = ((Bas(EKeys::Z) || Bas(EKeys::Up)) ? 1.f : 0.f) - ((Bas(EKeys::S) || Bas(EKeys::Down)) ? 1.f : 0.f);
		Gaz += PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis) - PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
		float Volant = ((Bas(EKeys::D) || Bas(EKeys::Right)) ? 1.f : 0.f) - ((Bas(EKeys::Q) || Bas(EKeys::Left)) ? 1.f : 0.f);
		Volant += PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		M->SetThrottleInput(FMath::Clamp(Gaz, -1.f, 1.f));
		M->SetSteeringInput(FMath::Clamp(Volant, -1.f, 1.f));
		M->SetHandbrakeInput(Bas(EKeys::SpaceBar) || Bas(EKeys::Gamepad_FaceButton_Bottom));
	}

	// Souris / stick droit : regarder autour ; sans souris, la camera revient derriere la voiture
	float DX = 0.f, DY = 0.f;
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

	if (PC->WasInputKeyJustPressed(ToucheSirene))
	{
		ToucherSirene();
	}

	// Descendre : en solo c'est le plugin Motoneige (touche E) ; en multijoueur on demande au serveur
	if (bMulti && PC->WasInputKeyJustPressed(ToucheMonter) && Relais)
	{
		if (Relais->PietonCache)
		{
			Relais->ServeurDescendre();
		}
		else
		{
			Message(TEXT("Pour descendre de la voiture de la roue : roue TAB > Soldat."));
		}
	}
}

// =====================================================================================================
// Sirene et gyrophares (sur chaque machine)
// =====================================================================================================

bool APoliceChelles::SireneVoulue(const APawn* Voiture) const
{
	if (!EstConduite(Voiture))
	{
		return false;
	}
	if (!EstMultijoueur())
	{
		return bSireneSolo;
	}
	for (TActorIterator<ARelaisPolice> It(GetWorld()); It; ++It)
	{
		if (It->Voiture == Voiture)
		{
			return It->bSirene;
		}
	}
	return bSireneAuto;
}

void APoliceChelles::AppelerFonctionPack(AActor* Acteur, const TCHAR* Nom, bool bValeur)
{
	UFunction* Fonction = IsValid(Acteur) ? Acteur->FindFunction(FName(Nom)) : nullptr;
	if (!Fonction)
	{
		return;
	}
	const int32 Taille = FMath::Max<int32>(Fonction->ParmsSize, 1);
	uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
	FMemory::Memzero(Parametres, Taille);
	for (TFieldIterator<FProperty> It(Fonction); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm))
		{
			continue;
		}
		if (FBoolProperty* B = CastField<FBoolProperty>(*It))
		{
			B->SetPropertyValue_InContainer(Parametres, bValeur);
		}
		else if (FFloatProperty* F = CastField<FFloatProperty>(*It))
		{
			F->SetPropertyValue_InContainer(Parametres, bValeur ? 1.f : 0.f);
		}
	}
	Acteur->ProcessEvent(Fonction, Parametres);
}

void APoliceChelles::MettreAJourSirenes()
{
	for (APawn* Voiture : Voitures())
	{
		const bool bVoulu = SireneVoulue(Voiture);
		const bool* Etat = SirenesAllumees.Find(Voiture);
		if (Etat && *Etat == bVoulu)
		{
			continue;
		}
		if (bVoulu)
		{
			AppelerFonctionPack(Voiture, TEXT("ActivateLights"), true);
			AppelerFonctionPack(Voiture, TEXT("ActivateSiren"), true);
		}
		else
		{
			AppelerFonctionPack(Voiture, TEXT("StopPolice"), false);
		}
		SirenesAllumees.Add(Voiture, bVoulu);
	}
	// Voitures disparues
	for (auto It = SirenesAllumees.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

// =====================================================================================================
// Route degagee devant la police (la circulation est locale a chaque machine)
// =====================================================================================================

void APoliceChelles::DegagerLaRoute()
{
	if (!bDegager)
	{
		return;
	}
	if (!ClasseCirculation)
	{
		ClasseCirculation = TrouverClasse(TEXT("/Script/ChellesCirculation.CirculationSpawner"), TEXT("CirculationSpawner"));
		if (!ClasseCirculation)
		{
			return; // pas de circulation dans ce projet
		}
	}
	UFunction* Fonction = ClasseCirculation->FindFunctionByName(TEXT("DegagerDevant"));
	if (!Fonction)
	{
		static bool bDejaDit = false;
		if (!bDejaDit)
		{
			bDejaDit = true;
			UE_LOG(LogPolice, Warning, TEXT("[Police] Recompilez ChellesCirculation (Tools\\Police\\INSTALLER_POLICE.bat) pour que la circulation se range devant la police."));
		}
		return;
	}
	FStructProperty* PPosition = FindFProperty<FStructProperty>(Fonction, TEXT("Position"));
	FStructProperty* PDirection = FindFProperty<FStructProperty>(Fonction, TEXT("Direction"));
	FFloatProperty* PLongueur = FindFProperty<FFloatProperty>(Fonction, TEXT("Longueur"));
	FFloatProperty* PLargeur = FindFProperty<FFloatProperty>(Fonction, TEXT("Largeur"));
	FFloatProperty* PDuree = FindFProperty<FFloatProperty>(Fonction, TEXT("Duree"));
	if (!PPosition || !PDirection || !PLongueur || !PLargeur || !PDuree)
	{
		return;
	}

	TArray<TPair<FVector, FVector>> Roulent;
	for (APawn* Voiture : Voitures())
	{
		if (!EstConduite(Voiture))
		{
			continue;
		}
		const UPrimitiveComponent* Corps = Caisse(Voiture);
		FVector Vitesse = Corps ? Corps->GetComponentVelocity() : Voiture->GetVelocity();
		Vitesse.Z = 0.f;
		if (Vitesse.Size() * CmsVersKmh < VitesseDegager)
		{
			continue;
		}
		Roulent.Emplace(Voiture->GetActorLocation(), Vitesse.GetSafeNormal());
	}
	if (Roulent.Num() == 0)
	{
		return;
	}

	const int32 Taille = FMath::Max<int32>(Fonction->ParmsSize, 1);
	uint8* Parametres = (uint8*)FMemory_Alloca(Taille);
	for (TActorIterator<AActor> It(GetWorld(), ClasseCirculation); It; ++It)
	{
		for (const TPair<FVector, FVector>& Police : Roulent)
		{
			FMemory::Memzero(Parametres, Taille);
			*PPosition->ContainerPtrToValuePtr<FVector>(Parametres) = Police.Key;
			*PDirection->ContainerPtrToValuePtr<FVector>(Parametres) = Police.Value;
			PLongueur->SetPropertyValue_InContainer(Parametres, DistanceDegager);
			PLargeur->SetPropertyValue_InContainer(Parametres, LargeurDegager);
			PDuree->SetPropertyValue_InContainer(Parametres, DureeRangement);
			It->ProcessEvent(Fonction, Parametres);
		}
	}
}

// =====================================================================================================
// Aide a la conduite (reprise de l'ancien plugin ChellesConduite du SUV)
// =====================================================================================================

void APoliceChelles::RegleRoue(UClass* ClasseRoue, bool bAvant)
{
	if (!ClasseRoue)
	{
		return;
	}
	for (const FRouePoliceOrigine& O : Origines)
	{
		if (O.Classe == ClasseRoue)
		{
			return; // deja reglee
		}
	}
	UVehicleWheel* Roue = ClasseRoue->GetDefaultObject<UVehicleWheel>();
	if (!Roue)
	{
		return;
	}
	FRouePoliceOrigine O;
	O.Classe = ClasseRoue;
	O.Pneu = Roue->TireConfig;
	O.LatStiffValue = Roue->LatStiffValue;
	O.LongStiffValue = Roue->LongStiffValue;
	O.SteerAngle = Roue->SteerAngle;
	O.SuspensionDampingRatio = Roue->SuspensionDampingRatio;
	Origines.Add(O);

	// Pneu plus accrocheur (nouveau pneu en memoire, celui du pack n'est pas touche)
	UTireConfig* Pneu = NewObject<UTireConfig>(this);
	const float Base = Roue->TireConfig ? Roue->TireConfig->GetFrictionScale() : 1.f;
	Pneu->SetFrictionScale(Base * Friction);
	Pneus.Add(Pneu);
	Roue->TireConfig = Pneu;
	Roue->LatStiffValue = O.LatStiffValue * Rigidite;
	Roue->LongStiffValue = O.LongStiffValue * FMath::Max(1.f, Rigidite * 0.8f);
	if (bAvant && Braquage > 0.f && Roue->SteerAngle > Braquage)
	{
		Roue->SteerAngle = Braquage;
	}
	if (Amortissement > 0.f)
	{
		Roue->SuspensionDampingRatio = FMath::Max(Roue->SuspensionDampingRatio, Amortissement);
	}
}

void APoliceChelles::RemettreRoues()
{
	for (const FRouePoliceOrigine& O : Origines)
	{
		if (UVehicleWheel* Roue = O.Classe ? O.Classe->GetDefaultObject<UVehicleWheel>() : nullptr)
		{
			Roue->TireConfig = Cast<UTireConfig>(O.Pneu);
			Roue->LatStiffValue = O.LatStiffValue;
			Roue->LongStiffValue = O.LongStiffValue;
			Roue->SteerAngle = O.SteerAngle;
			Roue->SuspensionDampingRatio = O.SuspensionDampingRatio;
		}
	}
	Origines.Reset();
}

void APoliceChelles::PreparerVoiture(APawn* Voiture)
{
	UWheeledVehicleMovementComponent* Mouvement = Roulement(Voiture);
	if (!Mouvement)
	{
		return;
	}
	for (const FWheelSetup& Reglage : Mouvement->WheelSetups)
	{
		UClass* Classe = Reglage.WheelClass;
		const UVehicleWheel* Defaut = Classe ? Classe->GetDefaultObject<UVehicleWheel>() : nullptr;
		const bool bAvant = Defaut && !Reglage.bDisableSteering && Defaut->SteerAngle > 0.1f;
		RegleRoue(Classe, bAvant);
	}
	if (UPrimitiveComponent* Corps = Caisse(Voiture))
	{
		if (FBodyInstance* Corps0 = Corps->GetBodyInstance())
		{
			Corps0->COMNudge.Z += AbaisserCentre;
			Corps0->UpdateMassProperties();
		}
	}
	Mouvement->RecreatePhysicsState();
}

void APoliceChelles::Aider(APawn* Voiture, float Dt)
{
	UWheeledVehicleMovementComponent* Mouvement = Roulement(Voiture);
	UPrimitiveComponent* Corps = Caisse(Voiture);
	if (!Mouvement || !Corps || !Corps->IsSimulatingPhysics() || Dt <= 0.f)
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
	const FTransform T = Corps->GetComponentTransform();
	const FVector Avant = T.GetUnitAxis(EAxis::X);
	const FVector Droite = T.GetUnitAxis(EAxis::Y);
	const FVector Haut = T.GetUnitAxis(EAxis::Z);
	FVector Vitesse = Corps->GetPhysicsLinearVelocity();
	FVector Rotation = Corps->GetPhysicsAngularVelocityInRadians();

	// a) Plus de glisse sur le cote
	const float Lateral = FVector::DotProduct(Vitesse, Droite);
	Vitesse -= Droite * Lateral * (1.f - FMath::Exp(-Adherence * Dt));

	// b) Rotation limitee : pas de tete-a-queue
	const float VitM = FMath::Abs(FVector::DotProduct(Vitesse, Avant)) / 100.f;
	float LacetMax = FMath::DegreesToRadians(VirageMaxDeg);
	if (VitM > 0.5f)
	{
		LacetMax = FMath::Min(LacetMax, FMath::Max(VitM / RayonMini, FMath::DegreesToRadians(25.f)));
		LacetMax = FMath::Min(LacetMax, FMath::Max(AccelLaterale / VitM, FMath::DegreesToRadians(12.f)));
	}
	const float Lacet = FVector::DotProduct(Rotation, Haut);
	if (FMath::Abs(Lacet) > LacetMax)
	{
		Rotation -= Haut * (Lacet - FMath::Sign(Lacet) * LacetMax);
	}

	// c) Anti-roulis
	const FVector Bascule = Rotation - Haut * FVector::DotProduct(Rotation, Haut);
	Rotation -= Bascule * (1.f - FMath::Exp(-AntiRoulis * Dt));

	Corps->SetPhysicsLinearVelocity(Vitesse);
	Corps->SetPhysicsAngularVelocityInRadians(Rotation);

	// d) Plaquee au sol quand elle va vite
	if (Appui > 0.f)
	{
		const float Part = FMath::Clamp(VitM / 20.f, 0.f, 1.f);
		Corps->AddForce(-Haut * 980.f * Appui * Part, NAME_None, true);
	}
}

// =====================================================================================================
// Tick
// =====================================================================================================

void APoliceChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	ProchaineRecherche -= DeltaSeconds;
	if (ProchaineRecherche <= 0.f)
	{
		ProchaineRecherche = 0.5f;
		ChercherVoitures();
	}

	if (EstServeur())
	{
		if (!bPositionsChargees)
		{
			AttenteChargement -= DeltaSeconds;
			if (AttenteChargement <= 0.f)
			{
				bPositionsChargees = true;
				ChargerPositions();
			}
		}
		if (EstMultijoueur())
		{
			ProchainRelais -= DeltaSeconds;
			if (ProchainRelais <= 0.f)
			{
				ProchainRelais = 0.5f;
				GererRelaisServeur();
			}
			SuivreMonteesServeur();
		}
	}

	if (EstMultijoueur())
	{
		// Soldats caches des joueurs en voiture : pas de collision ici non plus (la collision n'est pas copiee)
		for (TActorIterator<ARelaisPolice> It(GetWorld()); It; ++It)
		{
			if (IsValid(It->PietonCache))
			{
				It->PietonCache->SetActorEnableCollision(false);
			}
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		ConduireLocal(Dt);
	}
	SuivreMonteeSolo();
	MettreAJourSirenes();

	ProchainDegagement -= DeltaSeconds;
	if (ProchainDegagement <= 0.f)
	{
		ProchainDegagement = 0.2f;
		DegagerLaRoute();
	}

	if (bAide)
	{
		for (APawn* Voiture : Voitures())
		{
			Aider(Voiture, Dt);
		}
	}
}

void APoliceChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Mot = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Info"));
	if (Mot.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		LireReglages();
		Message(TEXT("Police.txt relu (camera, touches, route degagee : tout de suite ; pneus : au prochain Play)."));
	}
	else if (Mot.Equals(TEXT("Liste"), ESearchCase::IgnoreCase))
	{
		for (int32 i = 0; i < PlacesPosees.Num(); ++i)
		{
			const FVector L = PlacesPosees[i].GetLocation();
			UE_LOG(LogPolice, Log, TEXT("[Police] %d : (%.0f, %.0f, %.0f) cap %.0f %s"), i + 1, L.X, L.Y, L.Z, PlacesPosees[i].Rotator().Yaw,
				(Posees.IsValidIndex(i) && Posees[i].IsValid()) ? TEXT("") : TEXT("(detruite)"));
		}
		Message(FString::Printf(TEXT("%d voiture(s) de police posee(s) (liste dans le journal)."), PlacesPosees.Num()));
	}
	else
	{
		int32 Conduites = 0;
		for (APawn* V : Voitures())
		{
			Conduites += EstConduite(V) ? 1 : 0;
		}
		Message(FString::Printf(TEXT("Police : %d voiture(s) dans la carte, %d conduite(s), %d posee(s). Route degagee %s, aide a la conduite %s."),
			Voitures().Num(), Conduites, PlacesPosees.Num(), bDegager ? TEXT("oui") : TEXT("non"), bAide ? TEXT("oui") : TEXT("non")), 8.f);
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
