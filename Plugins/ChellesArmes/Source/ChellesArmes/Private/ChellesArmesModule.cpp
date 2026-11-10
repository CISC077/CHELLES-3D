// CHELLES 3D - Module du plugin Armes : ajoute le gestionnaire des armes au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "ArmesChelles.h"

namespace
{
	AArmesChelles* Gestionnaire(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AArmesChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Console (touche ²) : reglage de l'apparence de l'arme en main. Exemple : Chelles.Arme Roulis 180 */
	void CommandeArme(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (AArmesChelles* G = Gestionnaire(Monde))
		{
			G->Commande(Arguments);
		}
	}

	/** Console : Chelles.Armes 0 = apparences d'origine, Chelles.Armes 1 = nouvelles apparences. */
	void CommandeArmes(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (AArmesChelles* G = Gestionnaire(Monde))
		{
			const bool bOui = Arguments.Num() > 0 ? FCString::Atoi(*Arguments[0]) != 0 : !G->Reglages.bApparences;
			G->ActiverApparences(bOui);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeArme(
		TEXT("Chelles.Arme"),
		TEXT("Regle l'apparence de l'arme en main : Avant/Haut/Cote <cm>, Tangage/Lacet/Roulis <degres>, Echelle <x>, Zero, Original, Tir <Laser|Feu|Glace|Foudre|Aucun>, Recharger. Exemple : Chelles.Arme Roulis 180"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeArme));

	FAutoConsoleCommandWithWorldAndArgs GCommandeArmes(
		TEXT("Chelles.Armes"),
		TEXT("Chelles.Armes 0 = armes d'origine du kit FPS_Game, Chelles.Armes 1 = nouvelles apparences."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeArmes));
}

class FChellesArmesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesArmesModule::AuDemarrage);
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
		if (TActorIterator<AArmesChelles>(MondeDuJeu))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<AArmesChelles>(AArmesChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesArmesModule, ChellesArmes)
