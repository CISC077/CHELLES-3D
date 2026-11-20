// CHELLES 3D - Module du plugin Drone : ajoute le gestionnaire des drones au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "DroneChelles.h"

namespace
{
	/** Console du jeu (touche ²) : Chelles.Drone = ranger / rappeler son drone (comme F10). */
	void CommandeBasculer(UWorld* Monde)
	{
		APlayerController* Moi = Monde ? Monde->GetFirstPlayerController() : nullptr;
		if (!Moi)
		{
			return;
		}
		for (TActorIterator<ADroneChelles> It(Monde); It; ++It)
		{
			if (It->GetOwner() == Moi)
			{
				It->Basculer();
				return;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[Drone] Pas encore de drone pour ce joueur."));
	}

	/** Console : Chelles.DroneInfo = les drones, ce qu'ils suivent et visent (dans le journal). */
	void CommandeInfo(UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		int32 Nombre = 0;
		for (TActorIterator<ADroneChelles> It(Monde); It; ++It)
		{
			UE_LOG(LogTemp, Log, TEXT("[Drone] %s"), *It->Resume());
			++Nombre;
		}
		UE_LOG(LogTemp, Log, TEXT("[Drone] %d drone(s) dans la partie"), Nombre);
	}

	FAutoConsoleCommandWithWorld GCommandeDrone(
		TEXT("Chelles.Drone"),
		TEXT("Range ou rappelle votre drone (comme la touche F10)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeBasculer));

	FAutoConsoleCommandWithWorld GCommandeDroneInfo(
		TEXT("Chelles.DroneInfo"),
		TEXT("Ecrit dans le journal les drones, le joueur qu'ils suivent et leur cible."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeInfo));
}

class FChellesDroneModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesDroneModule::AuDemarrage);
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
		// Relu a chaque partie : on peut changer Drone.txt sans recompiler
		RechargerReglagesDrone();
		if (!ReglagesDrone().bActif)
		{
			UE_LOG(LogTemp, Log, TEXT("[Drone] Desactive dans Drone.txt"));
			return;
		}
		// En multijoueur, seul le serveur pose les drones : ils arrivent ensuite chez les joueurs par le reseau
		if (MondeDuJeu->GetNetMode() == NM_Client)
		{
			return;
		}
		for (TActorIterator<ADronesChelles> It(MondeDuJeu); It; ++It)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<ADronesChelles>(ADronesChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesDroneModule, ChellesDrone)
