// CHELLES 3D - Module du plugin Police : ajoute le gestionnaire des voitures de police au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "PoliceChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Police [Info|Relire|Liste] */
	void CommandePolice(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<APoliceChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandePolice(
		TEXT("Chelles.Police"),
		TEXT("Voitures de police : Info = etat, Liste = voitures posees, Relire = relire Police.txt"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandePolice));
}

class FChellesPoliceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesPoliceModule::AuDemarrage);
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
		if (TActorIterator<APoliceChelles>(Monde))
		{
			return; // deja la
		}
		// Un gestionnaire par machine (non copie par le reseau) : camera, sirene et circulation sont locales.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<APoliceChelles>(APoliceChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesPoliceModule, ChellesPolice)
