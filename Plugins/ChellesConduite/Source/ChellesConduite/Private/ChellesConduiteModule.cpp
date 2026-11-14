// CHELLES 3D - Module du plugin Conduite du SUV : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "ConduiteSUV.h"

namespace
{
	/** Console (touche ²) : Chelles.SUV [Info|0|1|Relire] */
	void CommandeSUV(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AConduiteSUV> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeSUV(
		TEXT("Chelles.SUV"),
		TEXT("Conduite du SUV : Info = etat, 0 = couper l'aide, 1 = remettre, Relire = relire ConduiteSUV.txt"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeSUV));
}

class FChellesConduiteModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesConduiteModule::AuDemarrage);
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
		if (TActorIterator<AConduiteSUV>(Monde))
		{
			return; // deja la
		}
		// Un gestionnaire par machine : chaque PC simule la voiture, l'aide est la meme partout
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AConduiteSUV>(AConduiteSUV::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesConduiteModule, ChellesConduite)
