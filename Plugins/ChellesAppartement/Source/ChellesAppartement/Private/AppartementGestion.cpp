// CHELLES 3D - Gestion des appartements (serveur ou partie solo) : un par joueur + exploits des autres plugins
#include "AppartementChelles.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

namespace
{
	bool GestLireEntier(const UObject* Objet, const TCHAR* Nom, int32& Sortie)
	{
		if (!Objet)
		{
			return false;
		}
		FProperty* P = Objet->GetClass()->FindPropertyByName(FName(Nom));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (const FNumericProperty* N = CastField<FNumericProperty>(P))
		{
			Sortie = N->IsFloatingPoint() ? int32(N->GetFloatingPointPropertyValue(Adresse)) : int32(N->GetSignedIntPropertyValue(Adresse));
			return true;
		}
		if (const FEnumProperty* E = CastField<FEnumProperty>(P))
		{
			Sortie = int32(E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Adresse));
			return true;
		}
		return false;
	}
}

AAppartementGestion::AAppartementGestion()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false; // n'existe que sur le serveur
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AAppartementGestion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Monde = GetWorld();
	if (!Monde || Monde->GetTimeSeconds() < ProchainTour)
	{
		return;
	}
	ProchainTour = Monde->GetTimeSeconds() + 0.5f;
	Joueurs();
	Ecouter();
}

AAppartementJoueur* AAppartementGestion::AppartementDe(const AController* Joueur) const
{
	const APlayerController* PC = Cast<APlayerController>(Joueur);
	if (!PC)
	{
		return nullptr;
	}
	for (const auto& Paire : Appartements)
	{
		if (Paire.Key.Get() == PC)
		{
			return Paire.Value.Get();
		}
	}
	return nullptr;
}

void AAppartementGestion::Joueurs()
{
	UWorld* Monde = GetWorld();
	// Joueurs partis : leur appartement disparait
	TSet<int32> Pris;
	for (auto It = Appartements.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() || !It->Value.IsValid())
		{
			if (AAppartementJoueur* Ancien = It->Value.Get())
			{
				Ancien->Destroy();
			}
			It.RemoveCurrent();
			continue;
		}
		Pris.Add(It->Value->Numero);
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || AppartementDe(PC))
		{
			continue;
		}
		int32 Numero = 0;
		while (Pris.Contains(Numero))
		{
			++Numero;
		}
		const FReglagesAppartement& R = ReglagesAppartement();
		const FTransform Lieu(FRotator::ZeroRotator, R.Position + FVector(0.f, Numero * R.Ecart, 0.f));
		AAppartementJoueur* A = Monde->SpawnActorDeferred<AAppartementJoueur>(AAppartementJoueur::StaticClass(), Lieu, PC, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!A)
		{
			continue;
		}
		A->Numero = Numero;
		A->FinishSpawning(Lieu);
		Pris.Add(Numero);
		Appartements.Add(PC, A);
		UE_LOG(LogTemp, Log, TEXT("[Appartement] Appartement %d pret pour %s."), Numero, *PC->GetName());
	}
}

AActor* AAppartementGestion::Trouver(const TCHAR* NomClasse, TWeakObjectPtr<AActor>& Cache, float& Prochain)
{
	if (Cache.IsValid())
	{
		return Cache.Get();
	}
	UWorld* Monde = GetWorld();
	if (Monde->GetTimeSeconds() < Prochain)
	{
		return nullptr;
	}
	Prochain = Monde->GetTimeSeconds() + 5.f;
	// Par le nom de la classe : pas de lien entre plugins (un plugin absent ne bloque rien)
	UClass* Classe = FindObject<UClass>(ANY_PACKAGE, NomClasse);
	if (!Classe || !Classe->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(Monde, Classe); It; ++It)
	{
		Cache = *It;
		return *It;
	}
	return nullptr;
}

void AAppartementGestion::Ecouter()
{
	// ---- Zombies tues (ChellesZombies : evenement OnZombieTue) ----
	if (AActor* Inv = Trouver(TEXT("InvasionZombies"), Invasion, ProchainInvasion))
	{
		if (InvasionBranchee.Get() != Inv)
		{
			InvasionBranchee = Inv;
			if (FMulticastDelegateProperty* P = FindFProperty<FMulticastDelegateProperty>(Inv->GetClass(), TEXT("OnZombieTue")))
			{
				FScriptDelegate Delegue;
				Delegue.BindUFunction(this, TEXT("QuandZombieTue"));
				P->AddDelegate(Delegue, Inv);
				UE_LOG(LogTemp, Log, TEXT("[Appartement] Branche sur ChellesZombies (trophee CHASSEUR DE ZOMBIES)."));
			}
		}
	}

	// ---- Missions reussies (ChellesMissions : Phase = 3) ----
	if (AActor* M = Trouver(TEXT("MissionsChelles"), Missions, ProchainMissions))
	{
		int32 Phase = 0;
		if (GestLireEntier(M, TEXT("Phase"), Phase))
		{
			if (Phase == 3 && PhaseAvant != 3)
			{
				for (const auto& Paire : Appartements)
				{
					if (AAppartementJoueur* A = Paire.Value.Get())
					{
						A->ServeurCompter(ChellesAppart::CompteMission, 1);
					}
				}
			}
			PhaseAvant = Phase;
		}
	}

	// ---- Vagues du Refuge (ChellesSurvie : Vague) ----
	if (AActor* S = Trouver(TEXT("SurvieChelles"), Survie, ProchainSurvie))
	{
		int32 Mode = 0;
		int32 V = 0;
		GestLireEntier(S, TEXT("Mode"), Mode);
		if (Mode != 0 && GestLireEntier(S, TEXT("Vague"), V) && V > 0 && V != VagueAvant)
		{
			for (const auto& Paire : Appartements)
			{
				if (AAppartementJoueur* A = Paire.Value.Get())
				{
					A->ServeurCompter(ChellesAppart::CompteVague, V);
				}
			}
		}
		VagueAvant = V;
	}
}

void AAppartementGestion::QuandZombieTue(ACharacter* Zombie, AController* Tueur, bool bTete)
{
	if (AAppartementJoueur* A = AppartementDe(Tueur))
	{
		A->ServeurCompter(ChellesAppart::CompteZombie, 1);
	}
}
