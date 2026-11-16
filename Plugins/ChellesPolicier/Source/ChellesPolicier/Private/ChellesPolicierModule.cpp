// CHELLES 3D - Module du plugin Policier : ajoute le gestionnaire des agents de police au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "PolicierChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Policier [Info|Liste|Montrer|Relire] */
	void CommandePolicier(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<APolicierChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandePolicier(
		TEXT("Chelles.Policier"),
		TEXT("Agents de police : Info = etat, Liste = rapport dans Policier_Rapport.txt, Montrer = dessine les postes, Relire = relire Policier.txt"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandePolicier));
}

class FChellesPolicierModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesPolicierModule::AuDemarrage);
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
			return; // decor seulement : rien a faire sur le serveur dedie
		}
		if (TActorIterator<APolicierChelles>(Monde))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<APolicierChelles>(APolicierChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesPolicierModule, ChellesPolicier)
