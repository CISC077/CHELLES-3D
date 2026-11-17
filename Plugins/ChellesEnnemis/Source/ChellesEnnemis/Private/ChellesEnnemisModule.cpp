// CHELLES 3D - Module du plugin Ennemis : ajoute l'acteur "Ennemis Chelles" au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "EnnemisChelles.h"

namespace
{
	AEnnemisChelles* TrouverEnnemis(UWorld* Monde)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AEnnemisChelles> It(Monde); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Console du jeu (touche ²) : Chelles.Ennemis affiche les ennemis trouves dans chaque dossier. */
	void CommandeResume(UWorld* Monde)
	{
		if (AEnnemisChelles* Ennemis = TrouverEnnemis(Monde))
		{
			Ennemis->Resume();
		}
	}

	/** Console : Chelles.EnnemisPlus pose encore NombreParDossier ennemis de chaque dossier. */
	void CommandePlus(UWorld* Monde)
	{
		AEnnemisChelles* Ennemis = TrouverEnnemis(Monde);
		if (!Ennemis || !Ennemis->EstServeur())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Ennemis] Seul le serveur (ou le jeu solo) peut poser des ennemis."));
			return;
		}
		Ennemis->AjouterUneVague();
	}

	FAutoConsoleCommandWithWorld GCommandeEnnemis(
		TEXT("Chelles.Ennemis"),
		TEXT("Affiche les ennemis trouves dans chaque dossier FPS_Game/Blueprints/Bonus/AI et combien sont en vie."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandeResume));

	FAutoConsoleCommandWithWorld GCommandeEnnemisPlus(
		TEXT("Chelles.EnnemisPlus"),
		TEXT("Pose encore NombreParDossier ennemis de chaque dossier dans la carte."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&CommandePlus));
}

class FChellesEnnemisModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesEnnemisModule::AuDemarrage);
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
		// En multijoueur, seul le serveur cree l'acteur : il arrive ensuite chez les joueurs par le reseau
		if (MondeDuJeu->GetNetMode() == NM_Client)
		{
			return;
		}
		// Patrouilleurs poses dans la carte sans points de patrouille : on leur en donne avant leur BeginPlay
		{
			TArray<AActor*> Acteurs;
			for (TActorIterator<AActor> It(MondeDuJeu); It; ++It)
			{
				Acteurs.Add(*It); // copie : RemplirPatrouille pose de nouveaux acteurs (TargetPoint)
			}
			int32 Remplis = 0;
			for (AActor* Acteur : Acteurs)
			{
				if (AEnnemisChelles::RemplirPatrouille(Acteur))
				{
					++Remplis;
				}
			}
			if (Remplis > 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[Ennemis] %d patrouilleurs sans points : points de patrouille ajoutes"), Remplis);
			}
		}
		if (TrouverEnnemis(MondeDuJeu))
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MondeDuJeu->SpawnActor<AEnnemisChelles>(AEnnemisChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesEnnemisModule, ChellesEnnemis)
