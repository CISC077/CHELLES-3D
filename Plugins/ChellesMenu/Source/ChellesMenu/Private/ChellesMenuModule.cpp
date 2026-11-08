// CHELLES 3D - Module du plugin Menu : ajoute l'acteur Menu au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "MenuChelles.h"

class FChellesMenuModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesMenuModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		if (!Monde || !Monde->IsGameWorld())
		{
			return;
		}
		if (TActorIterator<AMenuChelles>(Monde))
		{
			return; // deja un menu pose dans le niveau (reglages personnalises)
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AMenuChelles>(AMenuChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesMenuModule, ChellesMenu)
