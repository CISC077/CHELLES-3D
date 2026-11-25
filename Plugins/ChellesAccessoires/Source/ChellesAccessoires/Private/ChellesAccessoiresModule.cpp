// CHELLES 3D - Module du plugin Accessoires : pose le gestionnaire au lancement du jeu + commandes console.
#include "CoreMinimal.h"
#include "AccessoiresChelles.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

namespace
{
	AAccessoiresJoueur* AccMonRelais(UWorld* Monde)
	{
		APlayerController* Moi = Monde ? Monde->GetFirstPlayerController() : nullptr;
		if (!Moi)
		{
			return nullptr;
		}
		for (TActorIterator<AAccessoiresJoueur> It(Monde); It; ++It)
		{
			if (It->GetOwner() == Moi)
			{
				return *It;
			}
		}
		return nullptr;
	}

	int32 AccType(const FString& Nom)
	{
		if (Nom.StartsWith(TEXT("Lamp"), ESearchCase::IgnoreCase) || Nom.StartsWith(TEXT("Torch"), ESearchCase::IgnoreCase)) return ChellesAccessoires::Lampe;
		if (Nom.StartsWith(TEXT("Jum"), ESearchCase::IgnoreCase) || Nom.StartsWith(TEXT("Zoom"), ESearchCase::IgnoreCase)) return ChellesAccessoires::Jumelles;
		if (Nom.StartsWith(TEXT("Grap"), ESearchCase::IgnoreCase)) return ChellesAccessoires::Grappin;
		if (Nom.StartsWith(TEXT("Kit"), ESearchCase::IgnoreCase) || Nom.StartsWith(TEXT("Soin"), ESearchCase::IgnoreCase)) return ChellesAccessoires::Kit;
		return -1;
	}

	/**
	 * Console du jeu (touche ²) :
	 *   Chelles.Accessoires Info                -> ce que le joueur possede (dans le journal)
	 *   Chelles.Accessoires Lampe | Jumelles | Grappin | Soin  -> utilise l'accessoire
	 *   Chelles.Accessoires Donner Grappin      -> donne un accessoire (Donner Kit 3 = 3 kits)
	 *   Chelles.Accessoires Tout                -> tout donner (pour tester)
	 *   Chelles.Accessoires Relire              -> relit Accessoires.txt
	 */
	void AccCommande(const TArray<FString>& Arguments, UWorld* Monde)
	{
		const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : TEXT("Info");
		if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
		{
			RechargerReglagesAccessoires();
			return;
		}
		AAccessoiresJoueur* Relais = AccMonRelais(Monde);
		if (!Relais)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Accessoires] Pas encore de relais pour ce joueur."));
			return;
		}
		if (Quoi.Equals(TEXT("Donner"), ESearchCase::IgnoreCase) && Arguments.Num() > 1)
		{
			const int32 Type = AccType(Arguments[1]);
			if (Type >= 0)
			{
				Relais->Donner(uint8(Type), Arguments.Num() > 2 ? FCString::Atoi(*Arguments[2]) : 1, true);
			}
			return;
		}
		if (Quoi.Equals(TEXT("Tout"), ESearchCase::IgnoreCase))
		{
			Relais->ToutDonner();
			return;
		}
		const int32 Type = AccType(Quoi);
		if (Type >= 0)
		{
			Relais->Utiliser(uint8(Type));
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("[Accessoires] %s"), *Relais->Resume());
	}

	FAutoConsoleCommandWithWorldAndArgs GCommandeAccessoires(
		TEXT("Chelles.Accessoires"),
		TEXT("Accessoires : Info | Lampe | Jumelles | Grappin | Soin | Donner <Lampe|Jumelles|Grappin|Kit> [n] | Tout | Relire"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AccCommande));
}

class FChellesAccessoiresModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesAccessoiresModule::AuDemarrage);
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
		RechargerReglagesAccessoires(); // relu a chaque partie : Accessoires.txt se change sans recompiler
		if (!ReglagesAccessoires().bActif)
		{
			UE_LOG(LogTemp, Log, TEXT("[Accessoires] Desactives dans Accessoires.txt"));
			return;
		}
		if (Monde->GetNetMode() == NM_Client)
		{
			return; // en multijoueur, le serveur s'en occupe
		}
		for (TActorIterator<AAccessoiresChelles> It(Monde); It; ++It)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AAccessoiresChelles>(AAccessoiresChelles::StaticClass(), FTransform::Identity, Params);
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesAccessoiresModule, ChellesAccessoires)
