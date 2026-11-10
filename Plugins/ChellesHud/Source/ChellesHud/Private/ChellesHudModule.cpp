// CHELLES 3D - Module du plugin HUD : ajoute le HUD au lancement du jeu, sur chaque PC qui affiche le jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "HudChelles.h"

namespace
{
	/** Console du jeu (touche ²) : Chelles.Hud 0 cache le HUD, Chelles.Hud 1 le remontre, sans chiffre = basculer. */
	void CommandeHud(const TArray<FString>& Arguments)
	{
		if (Arguments.Num() > 0)
		{
			GHudChellesAffiche = FCString::Atoi(*Arguments[0]) != 0;
		}
		else
		{
			GHudChellesAffiche = !GHudChellesAffiche;
		}
		UE_LOG(LogTemp, Log, TEXT("[HUD] HUD %s."), GHudChellesAffiche ? TEXT("affiche") : TEXT("cache"));
	}

	FAutoConsoleCommand GCommandeHud(
		TEXT("Chelles.Hud"),
		TEXT("Chelles.Hud 0 cache le HUD (vie, XP, zombies), Chelles.Hud 1 le remontre."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CommandeHud));
}

class FChellesHudModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesHudModule::AuDemarrage);
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
		// Un serveur sans ecran n'a rien a afficher ; chaque joueur cree son propre HUD (pas de reseau)
		if (MondeDuJeu->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (TActorIterator<AHudChelles>(MondeDuJeu))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<AHudChelles>(AHudChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesHudModule, ChellesHud)
