// CHELLES 3D - Module du plugin Wraith : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "WraithChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Wraith [Info|0|1|Skin N|Relire|Arme X Y Z|ArmeRot P Y R|ArmeEchelle E|Params] */
	void CommandeWraith(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AWraithChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeWraith(
		TEXT("Chelles.Wraith"),
		TEXT("Wraith a la place du soldat : Info, 0 = soldat d'origine, 1 = Wraith, Skin N, Relire, Arme X Y Z, ArmeRot P Y R, ArmeEchelle E, Params"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeWraith));
}

class FChellesWraithModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesWraithModule::AuDemarrage);
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
		if (TActorIterator<AWraithChelles>(Monde))
		{
			return; // deja la
		}
		// Un gestionnaire par machine : chacun habille les soldats qu'il voit
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AWraithChelles>(AWraithChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesWraithModule, ChellesWraith)
