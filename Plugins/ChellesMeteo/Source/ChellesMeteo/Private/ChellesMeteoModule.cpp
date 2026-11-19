// CHELLES 3D - Module du plugin Meteo : ajoute la meteo au lancement de chaque partie (rien dans la map).
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "MeteoChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Meteo [Info|Heure 19|Auto|Hiver 0/1|Neige 0/1|Sapins 0/1|PoserSapin|RetirerSapin|Relire] */
	void CommandeMeteo(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AMeteoChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeMeteo(
		TEXT("Chelles.Meteo"),
		TEXT("Meteo : Info, Heure 19 (forcer une heure), Auto, Hiver 1/0, Neige 1/0, Sapins 1/0, PoserSapin, RetirerSapin, Relire"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeMeteo));
}

class FChellesMeteoModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesMeteoModule::AuDemarrage);
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
		if (TActorIterator<AMeteoChelles>(Monde))
		{
			return; // deja la
		}
		// Chaque machine a sa meteo (heure de son PC). Sur un serveur dedie : seulement les sapins (pour les collisions).
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AMeteoChelles>(AMeteoChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesMeteoModule, ChellesMeteo)
