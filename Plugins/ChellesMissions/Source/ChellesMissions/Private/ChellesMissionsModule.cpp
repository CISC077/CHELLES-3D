// CHELLES 3D - Module du plugin Missions : ajoute le gestionnaire des missions au lancement du jeu
// et les commandes de la console (touche \u00B2).
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#include "MissionsChelles.h"

namespace
{
	AMissionsChelles* TrouverMissions(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AMissionsChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Chelles.Mission 1 / 2 / 3 : lance une mission (1 tramway, 2 Mairie, 3 chasse). Sans numero : ecran MISSIONS. */
	void ConsoleMission(const TArray<FString>& Arguments, UWorld* Monde)
	{
		AMissionsChelles* M = TrouverMissions(Monde);
		if (!M)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Missions] Pas de missions dans cette partie (plugin ChellesMissions, Actif=Oui dans Missions.txt ?)."));
			return;
		}
		if (Arguments.Num() == 0)
		{
			M->OuvrirMenu();
			return;
		}
		const FString Erreur = M->Demarrer((uint8)FMath::Clamp(FCString::Atoi(*Arguments[0]), 0, 255));
		if (!Erreur.IsEmpty())
		{
			M->MessageLocal(Erreur);
		}
	}

	/** Chelles.MissionFin : abandonne la mission en cours. */
	void ConsoleMissionFin(UWorld* Monde)
	{
		if (AMissionsChelles* M = TrouverMissions(Monde))
		{
			M->Abandonner();
		}
	}

	/** Chelles.MissionIci Mairie : la Mairie a defendre = vos pieds. */
	void ConsoleMissionIci(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (AMissionsChelles* M = TrouverMissions(Monde))
		{
			M->CommandeMissionIci(Arguments);
		}
	}

	/** Chelles.Missions : etat dans le journal (Output Log). */
	void ConsoleMissions(UWorld* Monde)
	{
		if (AMissionsChelles* M = TrouverMissions(Monde))
		{
			M->DecrireEtat();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Missions] Pas de missions dans cette partie."));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCmdMission(TEXT("Chelles.Mission"),
		TEXT("Lance une mission : Chelles.Mission 1 (tramway) / 2 (Mairie) / 3 (chasse). Sans numero : ecran MISSIONS."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleMission));

	FAutoConsoleCommandWithWorld GCmdMissionFin(TEXT("Chelles.MissionFin"),
		TEXT("Abandonne la mission en cours."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleMissionFin));

	FAutoConsoleCommandWithWorldAndArgs GCmdMissionIci(TEXT("Chelles.MissionIci"),
		TEXT("Enregistre la place de la Mairie a vos pieds : Chelles.MissionIci Mairie"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleMissionIci));

	FAutoConsoleCommandWithWorld GCmdMissions(TEXT("Chelles.Missions"),
		TEXT("Affiche l'etat des missions dans le journal."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleMissions));
}

class FChellesMissionsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesMissionsModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* MondeDuJeu = Infos.World;
		if (!MondeDuJeu || !MondeDuJeu->IsGameWorld())
		{
			return;
		}
		// En multijoueur, seul le serveur cree le gestionnaire : il arrive chez les joueurs par le reseau
		if (MondeDuJeu->GetNetMode() == NM_Client || TrouverMissions(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<AMissionsChelles>(AMissionsChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesMissionsModule, ChellesMissions)
