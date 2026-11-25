// CHELLES 3D - Module du plugin PvP : ajoute le gestionnaire au lancement du jeu
// et les commandes de la console (touche ²).
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#include "PvPChelles.h"

namespace
{
	APvPChelles* TrouverPvP(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<APvPChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Chelles.PvP : etat du PvP et de chaque joueur (dans le journal). */
	void ConsoleEtat(UWorld* Monde)
	{
		if (APvPChelles* G = TrouverPvP(Monde))
		{
			G->CommandeEtat();
		}
	}

	/** Chelles.PvPManche : termine la manche et en relance une (serveur / hote). */
	void ConsoleManche(UWorld* Monde)
	{
		APvPChelles* G = TrouverPvP(Monde);
		if (G && G->EstServeur())
		{
			G->ChargerReglages();
			G->NouvelleManche();
		}
	}

	/** Chelles.PvPKits 3 : donne 3 kits de survie a chaque joueur (pour tester). */
	void ConsoleKits(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (APvPChelles* G = TrouverPvP(Monde))
		{
			G->CommandeKits(Arguments.Num() > 0 ? FCString::Atoi(*Arguments[0]) : 3);
		}
	}

	/** Chelles.PvPATerre : l'hote se met a terre lui-meme (pour tester la reanimation). */
	void ConsoleATerre(UWorld* Monde)
	{
		if (APvPChelles* G = TrouverPvP(Monde))
		{
			G->CommandeMoiATerre();
		}
	}

	FAutoConsoleCommandWithWorld GCmdEtat(TEXT("Chelles.PvP"),
		TEXT("Etat du PvP et de chaque joueur (coups, kits, eliminations) dans le journal."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleEtat));

	FAutoConsoleCommandWithWorld GCmdManche(TEXT("Chelles.PvPManche"),
		TEXT("Relit PvP.txt et relance une nouvelle manche PvP (hote seulement)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleManche));

	FAutoConsoleCommandWithWorldAndArgs GCmdKits(TEXT("Chelles.PvPKits"),
		TEXT("Donne des kits de survie a chaque joueur : Chelles.PvPKits 3"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleKits));

	FAutoConsoleCommandWithWorld GCmdATerre(TEXT("Chelles.PvPATerre"),
		TEXT("L'hote se met a terre lui-meme (pour tester le kit de survie)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleATerre));
}

class FChellesPvPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesPvPModule::AuDemarrage);
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
		// En multijoueur, seul le serveur cree le gestionnaire : il arrive chez les joueurs par le reseau
		if (MondeDuJeu->GetNetMode() == NM_Client || TrouverPvP(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<APvPChelles>(APvPChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesPvPModule, ChellesPvP)
