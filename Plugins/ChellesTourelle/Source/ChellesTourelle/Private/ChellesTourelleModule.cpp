// CHELLES 3D - Module du plugin Tourelle : pose le gestionnaire au lancement du jeu + commandes console.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "TourelleCadeau.h"
#include "TourelleChelles.h"

namespace
{
	ATourelleJoueur* MonRelais(UWorld* Monde)
	{
		APlayerController* Moi = Monde ? Monde->GetFirstPlayerController() : nullptr;
		if (!Moi)
		{
			return nullptr;
		}
		for (TActorIterator<ATourelleJoueur> It(Monde); It; ++It)
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
	 *   Chelles.Tourelle Info       -> temps de jeu, cadeau, tourelles de la carte (dans le journal)
	 *   Chelles.Tourelle Poser      -> pose sa tourelle devant soi (si elle est offerte)
	 *   Chelles.Tourelle Ramasser   -> reprend sa tourelle
	 *   Chelles.Tourelle Temps 50   -> pour tester : regle son temps de jeu a 50 minutes
	 *   Chelles.Tourelle Relire     -> relit Tourelle.txt
	 */
	void Commande(const TArray<FString>& Arguments, UWorld* Monde)
	{
		const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
		ATourelleJoueur* Relais = MonRelais(Monde);
		if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
		{
			RechargerReglagesTourelle();
			return;
		}
		if (Quoi.Equals(TEXT("Temps"), ESearchCase::IgnoreCase) && Relais)
		{
			Relais->ReglerTemps(Arguments.Num() > 1 ? FCString::Atof(*Arguments[1]) : ReglagesTourelle().MinutesAvantCadeau);
			return;
		}
		if (Quoi.Equals(TEXT("Poser"), ESearchCase::IgnoreCase) && Relais)
		{
			FString Raison;
			if (!Relais->Poser(Raison))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Tourelle] %s"), *Raison);
			}
			return;
		}
		if (Quoi.Equals(TEXT("Ramasser"), ESearchCase::IgnoreCase) && Relais)
		{
			Relais->Ramasser();
			return;
		}
		if (Relais)
		{
			UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s"), *Relais->Resume());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Tourelle] Pas encore de relais pour ce joueur."));
		}
		int32 Nombre = 0;
		for (TActorIterator<ATourelleChelles> It(Monde); It; ++It)
		{
			UE_LOG(LogTemp, Log, TEXT("[Tourelle] %s"), *It->Resume());
			++Nombre;
		}
		UE_LOG(LogTemp, Log, TEXT("[Tourelle] %d tourelle(s) sur la carte"), Nombre);
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeTourelle(
		TEXT("Chelles.Tourelle"),
		TEXT("Tourelle : Info | Poser | Ramasser | Temps <minutes> | Relire"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Commande));
}

class FChellesTourelleModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesTourelleModule::AuDemarrage);
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
		RechargerReglagesTourelle(); // relu a chaque partie : Tourelle.txt se change sans recompiler
		if (!ReglagesTourelle().bActif)
		{
			UE_LOG(LogTemp, Log, TEXT("[Tourelle] Desactivee dans Tourelle.txt"));
			return;
		}
		if (Monde->GetNetMode() == NM_Client)
		{
			return; // en multijoueur, le serveur s'en occupe
		}
		for (TActorIterator<ATourellesChelles> It(Monde); It; ++It)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<ATourellesChelles>(ATourellesChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesTourelleModule, ChellesTourelle)
