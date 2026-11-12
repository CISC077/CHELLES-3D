// CHELLES 3D - Module du plugin Sons des vehicules : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "SonsVehicules.h"

namespace
{
	/** Console (touche ²) : Chelles.Vehicules [0|1|Info|Relire|Klaxon] */
	void CommandeVehicules(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ASonsVehicules> It(Monde); It; ++It)
		{
			TArray<FString> Args = Arguments;
			if (Args.Num() == 0)
			{
				Args.Add(TEXT("Info"));
			}
			It->Commande(Args);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeVehicules(
		TEXT("Chelles.Vehicules"),
		TEXT("Sons des vehicules : Info = etat, 0 = couper, 1 = remettre, Relire = relire SonsVehicules.txt, Klaxon = fait klaxonner le vehicule le plus proche"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeVehicules));
}

class FChellesSonsVehiculesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesSonsVehiculesModule::AuDemarrage);
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
		if (TActorIterator<ASonsVehicules>(Monde))
		{
			return; // deja la
		}
		// Un gestionnaire par machine (le serveur s'en sert aussi pour faire passer les klaxons en multi)
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<ASonsVehicules>(ASonsVehicules::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesSonsVehiculesModule, ChellesSonsVehicules)
