// CHELLES 3D - Module du plugin Appartement : pose le gestionnaire au lancement du jeu + commandes console.
#include "CoreMinimal.h"
#include "AppartementChelles.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

namespace
{
	AAppartementJoueur* MonAppartement(UWorld* Monde)
	{
		APlayerController* Moi = Monde ? Monde->GetFirstPlayerController() : nullptr;
		if (!Moi)
		{
			return nullptr;
		}
		for (TActorIterator<AAppartementJoueur> It(Monde); It; ++It)
		{
			if (It->GetOwner() == Moi)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/**
	 * Console du jeu (touche ²) :
	 *   Chelles.Appartement            -> rentrer / sortir (comme F1)
	 *   Chelles.Appartement Info       -> ce qui est range + trophees (dans le journal)
	 *   Chelles.Appartement Vider      -> vide la garde-robe et le garage (les trophees restent)
	 *   Chelles.Appartement Test       -> donne des trophees pour tester la vitrine
	 *   Chelles.Appartement Relire     -> relit Appartement.txt
	 */
	void CommandeAppartement(const TArray<FString>& Arguments, UWorld* Monde)
	{
		const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : FString();
		if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
		{
			RechargerReglagesAppartement();
			return;
		}
		AAppartementJoueur* A = MonAppartement(Monde);
		if (!A)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Appartement] Pas encore d'appartement pour ce joueur (Actif=Non dans Appartement.txt ?)."));
			return;
		}
		if (Quoi.IsEmpty() || Quoi.Equals(TEXT("Entrer"), ESearchCase::IgnoreCase) || Quoi.Equals(TEXT("Sortir"), ESearchCase::IgnoreCase))
		{
			A->EntrerOuSortir();
		}
		else if (Quoi.Equals(TEXT("Vider"), ESearchCase::IgnoreCase))
		{
			A->ViderRangements();
		}
		else if (Quoi.Equals(TEXT("Test"), ESearchCase::IgnoreCase))
		{
			A->TrophesDeTest();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Appartement] %s"), *A->Resume());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeAppartement(
		TEXT("Chelles.Appartement"),
		TEXT("Appartement du joueur : (rien) = entrer/sortir | Info | Vider | Test | Relire"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CommandeAppartement));
}

class FChellesAppartementModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesAppartementModule::AuDemarrage);
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
		RechargerReglagesAppartement(); // relu a chaque partie : Appartement.txt se change sans recompiler
		if (!ReglagesAppartement().bActif)
		{
			UE_LOG(LogTemp, Log, TEXT("[Appartement] Desactive dans Appartement.txt"));
			return;
		}
		if (Monde->GetNetMode() == NM_Client)
		{
			return; // en multijoueur, le serveur cree les appartements
		}
		for (TActorIterator<AAppartementGestion> It(Monde); It; ++It)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AAppartementGestion>(AAppartementGestion::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesAppartementModule, ChellesAppartement)
