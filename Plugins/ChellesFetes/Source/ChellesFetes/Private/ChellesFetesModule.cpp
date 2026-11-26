// CHELLES 3D - Module du plugin Fetes : ajoute les evenements du calendrier au lancement de chaque partie (rien dans la map).
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "FetesChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Fetes [Info|Halloween|Juillet|Musique|Non|Auto|Feux|PoserScene|RetirerScene|Relire] */
	void CommandeFetes(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AFetesChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeFetes(
		TEXT("Chelles.Fetes"),
		TEXT("Fetes du calendrier : Info, Halloween, Juillet, Musique, Non, Auto (selon la date), Feux (feu d'artifice maintenant), PoserScene, RetirerScene, Relire"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeFetes));
}

class FChellesFetesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesFetesModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		// Uniquement en jeu, et pas sur un serveur dedie : chaque joueur voit la fete de son PC (decor sans collision).
		if (!Monde || !Monde->IsGameWorld() || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (TActorIterator<AFetesChelles>(Monde))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AFetesChelles>(AFetesChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesFetesModule, ChellesFetes)
