// CHELLES 3D - Module du plugin Pouvoirs : ajoute l'acteur "Pouvoirs et XP" au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "PouvoirsChelles.h"

namespace
{
	/** Console du jeu (touche ²) : Chelles.XP 500 ajoute 500 XP pour tester, Chelles.XP -500 en retire. */
	void CommandeXP(const TArray<FString>& Arguments, UWorld* MondeConsole)
	{
		if (!MondeConsole || Arguments.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Pouvoirs] Exemple : Chelles.XP 500"));
			return;
		}
		for (TActorIterator<APouvoirsChelles> It(MondeConsole); It; ++It)
		{
			It->AjouterXPTest(FCString::Atoi(*Arguments[0]));
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeXPChelles(
		TEXT("Chelles.XP"),
		TEXT("Ajoute (ou retire) de l'XP au joueur, pour tester les pouvoirs. Exemple : Chelles.XP 500"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeXP));
}

class FChellesPouvoirsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesPouvoirsModule::AuDemarrage);
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
		if (TActorIterator<APouvoirsChelles>(MondeDuJeu))
		{
			return; // deja pose dans le niveau (reglages personnalises)
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<APouvoirsChelles>(APouvoirsChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesPouvoirsModule, ChellesPouvoirs)
