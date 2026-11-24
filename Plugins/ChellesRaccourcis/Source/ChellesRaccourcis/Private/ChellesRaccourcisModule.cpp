// CHELLES 3D - Module du plugin Touches F1-F12 (modes) + roue des armes TAB : ajoute les deux au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "RaccourcisF.h"
#include "RoueArmes.h"

namespace
{
	/** Console (touche au-dessus de Tab) : Chelles.Raccourcis [Info|Relire|0|1|Menu|F1..F12] */
	void CommandeRaccourcis(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ARaccourcisF> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeRaccourcis(
		TEXT("Chelles.Raccourcis"),
		TEXT("Touches F1-F12 : Info = touches et roue, Relire = relire Raccourcis.txt, 0 = couper, 1 = remettre, F1..F12 = simuler la touche"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeRaccourcis));

	/** Console : Chelles.RoueArmes [Info|Relire|0|1] */
	void CommandeRoueArmes(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ARoueArmes> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeRoueArmes(
		TEXT("Chelles.RoueArmes"),
		TEXT("Roue des armes (TAB) : Info = liste des armes, Relire = relire Raccourcis.txt et DT_Armes, 0 = couper, 1 = remettre"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeRoueArmes));
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
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (!TActorIterator<ARaccourcisF>(Monde))
		{
			Monde->SpawnActor<ARaccourcisF>(ARaccourcisF::StaticClass(), FTransform::Identity, Params);
		}
		if (!TActorIterator<ARoueArmes>(Monde))
		{
			Monde->SpawnActor<ARoueArmes>(ARoueArmes::StaticClass(), FTransform::Identity, Params);
		}
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesRaccourcisModule, ChellesRaccourcis)
