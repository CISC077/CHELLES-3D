// CHELLES 3D - Module du plugin Pluie : ajoute la pluie au lancement de chaque partie (rien dans la map).
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "PluieChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Pluie [Info|Oui|Non|Auto|Flaques|Relire] */
	void CommandePluie(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<APluieChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandePluie(
		TEXT("Chelles.Pluie"),
		TEXT("Pluie : Info, Oui (averse tout de suite), Non (couper), Auto (meteo normale), Flaques (remplir), Relire (Pluie.txt)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandePluie));
}

class FChellesPluieModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesPluieModule::AuDemarrage);
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
			return;
		}
		if (TActorIterator<APluieChelles>(Monde))
		{
			return; // deja la
		}
		// Chaque PC a sa pluie (meme tirage pour tous : date + heure), rien n'est envoye sur le reseau.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<APluieChelles>(APluieChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesPluieModule, ChellesPluie)
