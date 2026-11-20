// CHELLES 3D - Module du plugin Vol de voiture : ajoute le gestionnaire au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "VolVoitureChelles.h"

namespace
{
	/** Console (touche ²) : Chelles.Vol [Info|Relire|Etoiles N|Garer] */
	void CommandeVolVoiture(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AVolVoitureChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeVolVoiture(
		TEXT("Chelles.Vol"),
		TEXT("Vol de voiture : Info = etat, Relire = relire VolVoiture.txt, Etoiles N = niveau RECHERCHE, Garer = garer des autos tout de suite"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeVolVoiture));
}

class FChellesVolVoitureModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesVolVoitureModule::AuDemarrage);
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
		if (TActorIterator<AVolVoitureChelles>(Monde))
		{
			return; // deja la
		}
		// Un gestionnaire par machine (non copie par le reseau)
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AVolVoitureChelles>(AVolVoitureChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesVolVoitureModule, ChellesVolVoiture)
