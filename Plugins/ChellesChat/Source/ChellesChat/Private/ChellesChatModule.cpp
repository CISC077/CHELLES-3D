// CHELLES 3D - Module du plugin Chat global : ajoute la fenetre du chat au lancement du jeu, sur chaque PC qui affiche le jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "ChatGlobalChelles.h"
#include "ConnexionChatGlobal.h"

namespace
{
	/** Console du jeu (touche au-dessus de Tab) : Chelles.ChatGlobal = etat de la connexion. */
	void CommandeEtat(const TArray<FString>& Arguments)
	{
		const FConnexionChatGlobal& C = FConnexionChatGlobal::Get();
		static const TCHAR* Noms[] = { TEXT("eteint"), TEXT("sans pseudo"), TEXT("connexion"), TEXT("connecte"), TEXT("attente"), TEXT("bloque") };
		UE_LOG(LogTemp, Log, TEXT("[ChatGlobal] Option %s, etat %s, pseudo memorise '%s', pseudo serveur '%s', %d en ligne (%d en jeu), %d lignes."),
			C.OptionActive() ? TEXT("ON") : TEXT("OFF"), Noms[(int32)C.Etat()], *C.PseudoMemorise(), *C.PseudoServeur(), C.EnLigne(), C.JoueursDuJeu(), C.Lignes().Num());
	}

	/** Chelles.ChatGlobalPseudo Nom = choisir un autre pseudo. */
	void CommandePseudo(const TArray<FString>& Arguments)
	{
		FConnexionChatGlobal::Get().ChoisirPseudo(FString::Join(Arguments, TEXT(" ")));
	}

	FAutoConsoleCommand GCommandeEtat(
		TEXT("Chelles.ChatGlobal"),
		TEXT("Affiche l'etat du chat global (site + jeu) dans le journal."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CommandeEtat));

	FAutoConsoleCommand GCommandePseudo(
		TEXT("Chelles.ChatGlobalPseudo"),
		TEXT("Chelles.ChatGlobalPseudo Nom : change le pseudo du chat global."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CommandePseudo));
}

class FChellesChatModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (IsRunningDedicatedServer())
		{
			return; // le serveur multijoueur du VPS n'a pas d'ecran : pas de chat global
		}
		FConnexionChatGlobal::Creer();
		// On ferme la connexion proprement avant que le moteur ne s'arrete
		AbonnementSortie = FCoreDelegates::OnPreExit.AddStatic(&FConnexionChatGlobal::Detruire);
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesChatModule::AuDemarrage);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnWorldInitializedActors.Remove(Abonnement);
		FCoreDelegates::OnPreExit.Remove(AbonnementSortie);
		if (FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
		{
			FConnexionChatGlobal::Detruire();
		}
		// sinon : la connexion a deja ete fermee par OnPreExit (ou le moteur s'arrete, on ne touche plus a rien)
	}

private:
	static void AuDemarrage(const UWorld::FActorsInitializedParams& Infos)
	{
		UWorld* Monde = Infos.World;
		if (!Monde || !Monde->IsGameWorld() || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		if (TActorIterator<AChatGlobalChelles>(Monde))
		{
			return; // deja la
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AChatGlobalChelles>(AChatGlobalChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
	FDelegateHandle AbonnementSortie;
};

IMPLEMENT_MODULE(FChellesChatModule, ChellesChat)
