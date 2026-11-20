// CHELLES 3D - Module du plugin Butin : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "ButinChelles.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

namespace
{
	/** Console (touche ²) : Chelles.Butin [Info|Relire|Coffre|Serie N] */
	void CommandeButin(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AButinChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeButin(
		TEXT("Chelles.Butin"),
		TEXT("Butin : Info = etat, Relire = relire Butin.txt, Coffre = coffre legendaire devant soi (test), Serie N = ajouter N eliminations (test)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeButin));
}

class FChellesButinModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesButinModule::AuDemarrage);
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
		if (TActorIterator<AButinChelles>(Monde))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AButinChelles>(AButinChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesButinModule, ChellesButin)
