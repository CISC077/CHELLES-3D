// CHELLES 3D - Module du plugin Commerces : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "CommercesChelles.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

namespace
{
	/** Console (touche au-dessus de Tab) : Chelles.Commerces [Info|Relire|Pieces N|Ouvrir Boulangerie|Fermer] */
	void CommandeCommerces(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ACommercesChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeCommerces(
		TEXT("Chelles.Commerces"),
		TEXT("Commerces : Info = etat, Relire = relire Commerces.txt, Pieces N = ajouter N pieces (test), Ouvrir Boulangerie|Tabac|Bar = ouvrir la boutique ici (test), Fermer"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeCommerces));
}

class FChellesCommercesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesCommercesModule::AuDemarrage);
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
		if (TActorIterator<ACommercesChelles>(Monde))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<ACommercesChelles>(ACommercesChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesCommercesModule, ChellesCommerces)
