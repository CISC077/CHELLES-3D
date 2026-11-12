// CHELLES 3D - Module du plugin Demo : ajoute le compteur de la demo au lancement du jeu,
// et la commande console Chelles.Demo pour le tester dans l'editeur.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#include "DemoChelles.h"

namespace
{
	/** Chelles.Demo 1 : teste la demo avec 1 minute (0 = arreter le test). */
	void ConsoleDemo(const TArray<FString>& Arguments, UWorld* Monde)
	{
		const float Minutes = Arguments.Num() > 0 ? FCString::Atof(*Arguments[0]) : 1.f;
		ADemoChelles::LancerTest(Monde, Minutes);
	}

	FAutoConsoleCommandWithWorldAndArgs GCmdDemo(TEXT("Chelles.Demo"),
		TEXT("Teste la version demo : Chelles.Demo 1 (1 minute de jeu puis ecran de fin). Chelles.Demo 0 = arreter."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleDemo));
}

class FChellesDemoModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesDemoModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* MondeDuJeu = Infos.World;
		if (!MondeDuJeu || !MondeDuJeu->IsGameWorld() || MondeDuJeu->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (!ADemoChelles::DemoActive() || TActorIterator<ADemoChelles>(MondeDuJeu))
		{
			return;
		}
		// Chaque PC a son propre compteur (rien n'est envoye par le reseau)
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<ADemoChelles>(ADemoChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesDemoModule, ChellesDemo)
