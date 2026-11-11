// CHELLES 3D - Module du plugin Modes de survie : ajoute le gestionnaire au lancement du jeu
// et les commandes de la console (touche ²).
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#include "SurvieChelles.h"

namespace
{
	ASurvieChelles* TrouverSurvie(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<ASurvieChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Chelles.Modes : ouvre l'ecran MODES (comme F7). */
	void ConsoleModes(UWorld* Monde)
	{
		if (ASurvieChelles* S = TrouverSurvie(Monde))
		{
			S->OuvrirModes();
		}
	}

	/** Chelles.RefugeIci : le centre de la zone du Refuge = vos pieds. */
	void ConsoleRefugeIci(UWorld* Monde)
	{
		if (ASurvieChelles* S = TrouverSurvie(Monde))
		{
			S->CommandeRefugeIci();
		}
	}

	/** Chelles.StandIci Arme Fusil / Munitions / Barriere 750 */
	void ConsoleStandIci(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (ASurvieChelles* S = TrouverSurvie(Monde))
		{
			S->CommandeStandIci(Arguments);
		}
	}

	/** Chelles.StandRetirer : retire le point d'achat le plus proche. */
	void ConsoleStandRetirer(UWorld* Monde)
	{
		if (ASurvieChelles* S = TrouverSurvie(Monde))
		{
			S->CommandeStandRetirer();
		}
	}

	/** Chelles.Points 5000 : donne des points (pour tester les achats). */
	void ConsolePoints(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (ASurvieChelles* S = TrouverSurvie(Monde))
		{
			S->CommandePoints(Arguments.Num() > 0 ? FCString::Atoi(*Arguments[0]) : 1000);
		}
	}

	/** Chelles.Vague 10 : saute a la vague 10. */
	void ConsoleVague(const TArray<FString>& Arguments, UWorld* Monde)
	{
		if (ASurvieChelles* S = TrouverSurvie(Monde))
		{
			S->CommandeVague(Arguments.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Arguments[0])) : 1);
		}
	}

	/** Chelles.RefugeFin : arrete le Dernier Refuge, retour au jeu libre. */
	void ConsoleFin(UWorld* Monde)
	{
		ASurvieChelles* S = TrouverSurvie(Monde);
		if (S && S->EstServeur())
		{
			S->Arreter();
		}
	}

	FAutoConsoleCommandWithWorld GCmdModes(TEXT("Chelles.Modes"),
		TEXT("Ouvre l'ecran MODES (comme F7)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleModes));

	FAutoConsoleCommandWithWorld GCmdRefugeIci(TEXT("Chelles.RefugeIci"),
		TEXT("Enregistre le centre de la zone du Dernier Refuge a vos pieds."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleRefugeIci));

	FAutoConsoleCommandWithWorldAndArgs GCmdStandIci(TEXT("Chelles.StandIci"),
		TEXT("Pose un point d'achat devant vous : Chelles.StandIci Arme Fusil | Munitions | Barriere 750"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleStandIci));

	FAutoConsoleCommandWithWorld GCmdStandRetirer(TEXT("Chelles.StandRetirer"),
		TEXT("Retire le point d'achat le plus proche (moins de 5 m)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleStandRetirer));

	FAutoConsoleCommandWithWorldAndArgs GCmdPoints(TEXT("Chelles.Points"),
		TEXT("Donne des points dans le Dernier Refuge (pour tester) : Chelles.Points 5000"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsolePoints));

	FAutoConsoleCommandWithWorldAndArgs GCmdVague(TEXT("Chelles.Vague"),
		TEXT("Saute a une vague du Dernier Refuge : Chelles.Vague 10"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ConsoleVague));

	FAutoConsoleCommandWithWorld GCmdFin(TEXT("Chelles.RefugeFin"),
		TEXT("Arrete le Dernier Refuge (retour au jeu libre)."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ConsoleFin));
}

class FChellesSurvieModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesSurvieModule::AuDemarrage);
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
		// En multijoueur, seul le serveur cree le gestionnaire : il arrive chez les joueurs par le reseau
		if (MondeDuJeu->GetNetMode() == NM_Client || TrouverSurvie(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<ASurvieChelles>(ASurvieChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesSurvieModule, ChellesSurvie)
