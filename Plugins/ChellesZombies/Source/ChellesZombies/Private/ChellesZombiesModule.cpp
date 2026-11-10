// CHELLES 3D - Module du plugin Zombies : ajoute l'acteur "Invasion zombies" au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "InvasionZombies.h"

namespace
{
	AInvasionZombies* TrouverInvasion(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AInvasionZombies> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Console du jeu (touche ²) : Chelles.Zombies lance une invasion tout de suite (pour tester). */
	void CommandeLancer(UWorld* Monde)
	{
		AInvasionZombies* Invasion = TrouverInvasion(Monde);
		if (!Invasion || !Invasion->EstServeur())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Zombies] Seul le serveur (ou le jeu solo) peut lancer l'invasion."));
			return;
		}
		Invasion->LancerInvasion();
	}

	/** Console : Chelles.ZombiesFin arrete l'invasion en cours. */
	void CommandeFin(UWorld* Monde)
	{
		AInvasionZombies* Invasion = TrouverInvasion(Monde);
		if (Invasion && Invasion->EstServeur())
		{
			Invasion->TerminerInvasion(false);
		}
	}

	FAutoConsoleCommandWithWorld GCommandeZombies(
		TEXT("Chelles.Zombies"),
		TEXT("Lance tout de suite une invasion de zombies (pour tester)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeLancer));

	FAutoConsoleCommandWithWorld GCommandeZombiesFin(
		TEXT("Chelles.ZombiesFin"),
		TEXT("Arrete l'invasion de zombies en cours."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeFin));
}

class FChellesZombiesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesZombiesModule::AuDemarrage);
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
		if (TrouverInvasion(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<AInvasionZombies>(AInvasionZombies::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesZombiesModule, ChellesZombies)
