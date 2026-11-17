// CHELLES 3D - Module du plugin Train : ajoute la ligne de train au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "TrainChelles.h"

namespace
{
	ATrainChelles* TrouverTrain(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<ATrainChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Console du jeu : Chelles.Train = etat de la ligne dans le journal (Output Log). */
	void CommandeTrain(UWorld* Monde)
	{
		if (ATrainChelles* Train = TrouverTrain(Monde))
		{
			Train->DecrireEtat();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Train] Pas de train dans cette partie (plugin ChellesTrain, Actif=Oui dans Train.txt ?)."));
		}
	}

	/** Console : Chelles.TrainVoir = vous emmene a cote du prochain arret du train (pour tester). */
	void CommandeTrainVoir(UWorld* Monde)
	{
		ATrainChelles* Train = TrouverTrain(Monde);
		APlayerController* PC = Monde ? Monde->GetFirstPlayerController() : nullptr;
		if (Train && PC)
		{
			Train->EmmenerAuProchainArret(PC->GetPawn());
		}
	}

	FAutoConsoleCommandWithWorld GCommandeTrain(
		TEXT("Chelles.Train"),
		TEXT("Affiche l'etat de la ligne de train (arrets, position) dans le journal."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeTrain));

	FAutoConsoleCommandWithWorld GCommandeTrainVoir(
		TEXT("Chelles.TrainVoir"),
		TEXT("Vous emmene a cote du prochain arret du train."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeTrainVoir));
}

class FChellesTrainModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesTrainModule::AuDemarrage);
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
		// En multijoueur, seul le serveur cree l'acteur : il arrive ensuite chez les joueurs par le reseau
		if (MondeDuJeu->GetNetMode() == NM_Client)
		{
			return;
		}
		if (TrouverTrain(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<ATrainChelles>(ATrainChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesTrainModule, ChellesTrain)
