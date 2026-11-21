// CHELLES 3D - Module du plugin Raccourcis armes F1-F12 : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "RaccourcisArmes.h"

namespace
{
	/** Console (touche ²) : Chelles.Raccourcis [Info|Relire|0|1|F1..F12] */
	void CommandeRaccourcis(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ARaccourcisArmes> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeRaccourcis(
		TEXT("Chelles.Raccourcis"),
		TEXT("Armes sur F1-F12 : Info = touches et soldat, Relire = relire Raccourcis.txt, 0 = couper, 1 = remettre, F1..F12 = simuler la touche"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeRaccourcis));
}

class FChellesRaccourcisModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesRaccourcisModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		if (!Monde || !Monde->IsGameWorld() || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return; // les touches n'existent que sur la machine d'un joueur
		}
		if (TActorIterator<ARaccourcisArmes>(Monde))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<ARaccourcisArmes>(ARaccourcisArmes::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesRaccourcisModule, ChellesRaccourcis)
