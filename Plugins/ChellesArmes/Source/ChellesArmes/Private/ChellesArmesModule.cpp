// CHELLES 3D - Module du plugin Armes : ajoute le gestionnaire des armes au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "ArmesChelles.h"
#include "ArsenalChelles.h"
#include "CoffresFPS.h"
#include "CaisseMunitions.h"
#include "ProtectionTireur.h"

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
		TEXT("Regle l'apparence de l'arme en main : Avant/Haut/Cote <cm>, Tangage/Lacet/Roulis <degres>, Echelle <x>, Zero, Original, Tir <Laser|Feu|Glace|Foudre|Aucun>, Modele <chemin|Aucun>, Recharger. Exemple : Chelles.Arme Roulis 180"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeArme));

	/** Console : Chelles.Arsenal Blaster | Bazooka | Sabre | Grenade | Aucune | Recharger */
	void CommandeArsenal(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<AArsenalChelles> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeArsenal(
		TEXT("Chelles.Arsenal"),
		TEXT("Prendre une arme de l'arsenal : Chelles.Arsenal Blaster | Bazooka | Sabre | Grenade | Aucune | Recharger (relit Arsenal.txt)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeArsenal));

	/** Console (25/11/2026) : Chelles.CoffresFPS Liste | Ouvrir  (coffres d'armes du kit FPS_Game) */
	void CommandeCoffres(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ACoffresFPS> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeCoffres(
		TEXT("Chelles.CoffresFPS"),
		TEXT("Coffres d'armes du kit FPS_Game : Chelles.CoffresFPS Liste (journal) | Ouvrir (ouvre le plus proche)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeCoffres));

	/** Console (25/11/2026) : Chelles.Caisses Nombre | Relire | Reposer  (caisses de munitions) */
	void CommandeCaisses(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (!Monde)
		{
			return;
		}
		for (TActorIterator<ACaissesMunitionsGestion> It(Monde); It; ++It)
		{
			It->Commande(Arguments);
			return;
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeCaisses(
		TEXT("Chelles.Caisses"),
		TEXT("Caisses de munitions : Chelles.Caisses Nombre | Relire (relit Arsenal.txt) | Reposer (les reposer ailleurs)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeCaisses));

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
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (!TActorIterator<AArmesChelles>(MondeDuJeu))
		{
			MondeDuJeu->SpawnActor<AArmesChelles>(AArmesChelles::StaticClass(), FTransform::Identity, Params);
		}
		// Arsenal (2026-11-17) : blaster, bazooka, sabre lumineux, grenade a zombies (touche N)
		if (!TActorIterator<AArsenalChelles>(MondeDuJeu))
		{
			MondeDuJeu->SpawnActor<AArsenalChelles>(AArsenalChelles::StaticClass(), FTransform::Identity, Params);
		}
		// Protection du tireur (27/11/2026) : ses propres projectiles ne le touchent plus (bug du sniper)
		if (!TActorIterator<AProtectionTireur>(MondeDuJeu))
		{
			MondeDuJeu->SpawnActor<AProtectionTireur>(AProtectionTireur::StaticClass(), FTransform::Identity, Params);
		}
		// Caisses de munitions dispersees (25/11/2026) : posees par le serveur / l'hote / le solo
		if (!TActorIterator<ACaissesMunitionsGestion>(MondeDuJeu) && MondeDuJeu->GetNetMode() != NM_Client)
		{
			MondeDuJeu->SpawnActor<ACaissesMunitionsGestion>(ACaissesMunitionsGestion::StaticClass(), FTransform::Identity, Params);
		}
		// Coffres d'armes du kit FPS_Game (25/11/2026) : E ouvre le coffre le plus proche si le kit ne le fait plus
		if (!TActorIterator<ACoffresFPS>(MondeDuJeu) && MondeDuJeu->GetNetMode() != NM_DedicatedServer)
		{
			MondeDuJeu->SpawnActor<ACoffresFPS>(ACoffresFPS::StaticClass(), FTransform::Identity, Params);
		}
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesArmesModule, ChellesArmes)
