// CHELLES 3D - Module du plugin Inventaire cyberpunk : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "InventaireCyber.h"

namespace
{
	/** Console (touche ²) : Chelles.Inventaire [Info|0|1|Relire|Fermer] */
	void CommandeInventaire(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AInventaireCyber> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeInventaire(
		TEXT("Chelles.Inventaire"),
		TEXT("Menu armes cyberpunk : Info = etat, 0 = couper, 1 = remettre, Relire = relire Inventaire.txt, Fermer = fermer le menu"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeInventaire));
}

class FChellesInventaireModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesInventaireModule::AuDemarrage);
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
			return; // le menu n'existe que sur l'ecran d'un joueur
		}
		if (TActorIterator<AInventaireCyber>(Monde))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AInventaireCyber>(AInventaireCyber::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesInventaireModule, ChellesInventaire)
