// CHELLES 3D - Module du plugin Defis : lance le defi de la semaine au debut de chaque partie
// et ajoute les commandes de la console (touche ²) : Chelles.Defi, Chelles.DefiEtat
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#include "DefisChelles.h"

namespace
{
	FAutoConsoleCommand GCmdDefi(TEXT("Chelles.Defi"),
		TEXT("Redemande le defi de la semaine au serveur et remontre le bandeau."),
		FConsoleCommandDelegate::CreateLambda([]() { FDefisChelles::Get().CommandeMontrer(); }));

	FAutoConsoleCommand GCmdDefiEtat(TEXT("Chelles.DefiEtat"),
		TEXT("Affiche le defi de la semaine dans le journal (Output Log)."),
		FConsoleCommandDelegate::CreateLambda([]() { FDefisChelles::Get().DecrireEtat(); }));
}

class FChellesDefisModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesDefisModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
		FDefisChelles::Get().Arreter();
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		// Pas sur le serveur dedie du VPS (personne ne regarde son ecran)
		if (!Monde || !Monde->IsGameWorld() || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		FDefisChelles::Get().NouveauMonde(Monde);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesDefisModule, ChellesDefis)
