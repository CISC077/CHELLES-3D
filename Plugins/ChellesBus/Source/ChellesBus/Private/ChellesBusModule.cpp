// CHELLES 3D - Module du plugin Bus : ajoute l'acteur "Bus pendant l'invasion" au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "BusChelles.h"

namespace
{
	ABusChelles* TrouverBus(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<ABusChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Console du jeu : Chelles.Bus = route vide + bus tout de suite, sans attendre l'invasion (pour tester). */
	void CommandeBus(UWorld* Monde)
	{
		ABusChelles* Bus = TrouverBus(Monde);
		if (!Bus || !Bus->EstServeur())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bus] Seul le serveur (ou le jeu solo) peut lancer le bus."));
			return;
		}
		Bus->ForcerService(true);
	}

	/** Console : Chelles.BusFin arrete le test (le bus reste si une invasion est en cours). */
	void CommandeBusFin(UWorld* Monde)
	{
		ABusChelles* Bus = TrouverBus(Monde);
		if (Bus && Bus->EstServeur())
		{
			Bus->ForcerService(false);
		}
	}

	FAutoConsoleCommandWithWorld GCommandeBus(
		TEXT("Chelles.Bus"),
		TEXT("Vide la route et fait rouler le bus tout de suite (test, sans invasion)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeBus));

	FAutoConsoleCommandWithWorld GCommandeBusFin(
		TEXT("Chelles.BusFin"),
		TEXT("Arrete le test du bus."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeBusFin));
}

class FChellesBusModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesBusModule::AuDemarrage);
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
		if (TrouverBus(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<ABusChelles>(ABusChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesBusModule, ChellesBus)
