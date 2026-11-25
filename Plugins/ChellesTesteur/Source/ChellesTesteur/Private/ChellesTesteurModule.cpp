// CHELLES 3D - Module du plugin Testeur (Agent 5) : pose le testeur dans la carte, SEULEMENT avec -ChellesTest.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "TesteurChelles.h"

class FChellesTesteurModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Jeu normal, editeur, serveur : rien du tout.
		if (!FParse::Param(FCommandLine::Get(), TEXT("ChellesTest")))
		{
			return;
		}
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesTesteurModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		if (Abonnement.IsValid())
		{
			FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
		}
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		if (!Monde || !Monde->IsGameWorld() || Monde->GetNetMode() == NM_DedicatedServer || Monde->GetNetMode() == NM_Client)
		{
			return;
		}
		if (TActorIterator<ATesteurChelles>(Monde))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<ATesteurChelles>(ATesteurChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesTesteurModule, ChellesTesteur)
