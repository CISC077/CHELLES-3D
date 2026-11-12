// CHELLES 3D - Module du plugin Optimisation : ajoute l'optimisation au lancement du jeu, sur chaque machine.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "OptimisationChelles.h"

class FChellesOptimisationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesOptimisationModule::AuDemarrage);
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
		if (TActorIterator<AOptimisationChelles>(MondeDuJeu))
		{
			return; // deja la
		}
		// Chaque machine cree le sien (solo, serveur, joueurs) : rien n'est envoye sur le reseau
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AOptimisationChelles* Optim = MondeDuJeu->SpawnActor<AOptimisationChelles>(AOptimisationChelles::StaticClass(), FTransform::Identity, Params);
		if (Optim)
		{
			Optim->LireReglages();
			// Avant le BeginPlay de la neige (ChellesNeige lit la densite au demarrage)
			Optim->PreparerNeige();
		}
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesOptimisationModule, ChellesOptimisation)
