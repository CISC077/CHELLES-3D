// CHELLES 3D - Module du plugin Menu : ajoute l'acteur Menu au lancement du jeu.
#include "CoreMinimal.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "MenuChelles.h"

class FChellesMenuModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Abonnement = FWorldDelegates::OnWorldInitializedActors.AddStatic(&FChellesMenuModule::AuDemarrage);
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
		PoserDeparts(Monde);
		if (TActorIterator<AMenuChelles>(Monde))
		{
			return; // deja un menu pose dans le niveau (reglages personnalises)
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Monde->SpawnActor<AMenuChelles>(AMenuChelles::StaticClass(), FTransform::Identity, Params);
	}

	/** Cherche le sol sous un point (de tres haut vers tres bas). */
	static bool Sol(UWorld* Monde, const FVector& Point, FVector& Trouve)
	{
		FHitResult Impact;
		FCollisionQueryParams Params(FName(TEXT("ChellesDepart")), true);
		const FVector Haut(Point.X, Point.Y, Point.Z + 20000.f);
		const FVector Bas(Point.X, Point.Y, Point.Z - 60000.f);
		if (Monde->LineTraceSingleByChannel(Impact, Haut, Bas, ECC_Visibility, Params))
		{
			Trouve = Impact.ImpactPoint + FVector(0.f, 0.f, 120.f);
			return true;
		}
		return false;
	}

	/**
	 * La carte Rachid n'a pas de PlayerStart : en multijoueur le joueur apparaissait en 0,0,0, sous la carte.
	 * Si la carte n'en a aucun, on en pose 8 au sol, autour de l'endroit ou le joueur a joue en solo
	 * (memorise dans GameUserSettings.ini, cle Depart), sinon pres d'un vehicule pose dans la carte.
	 */
	static void PoserDeparts(UWorld* Monde)
	{
		if (Monde->GetNetMode() == NM_Client || TActorIterator<APlayerStart>(Monde))
		{
			return;
		}
		TArray<FVector> Candidats;
		FString Texte;
		FVector Memorise;
		if (GConfig && GConfig->GetString(TEXT("ChellesMenu"), TEXT("Depart"), Texte, GGameUserSettingsIni) && Memorise.InitFromString(Texte))
		{
			Candidats.Add(Memorise);
		}
		for (TActorIterator<AActor> It(Monde); It; ++It)
		{
			const FString Classe = It->GetClass()->GetName();
			if (Classe.Contains(TEXT("Snowmobile")) || Classe.Contains(TEXT("SpiderMan_Vol")) || Classe.Contains(TEXT("Horse")) || Classe.Contains(TEXT("Hatchback")))
			{
				Candidats.Add(It->GetActorLocation());
			}
		}
		Candidats.Add(FVector::ZeroVector);

		for (const FVector& Centre : Candidats)
		{
			FVector Base;
			if (!Sol(Monde, Centre, Base))
			{
				continue;
			}
			int32 Poses = 0;
			for (int32 i = 0; i < 8; ++i)
			{
				const float Angle = FMath::DegreesToRadians(i * 45.f);
				FVector Point;
				if (!Sol(Monde, Base + FVector(FMath::Cos(Angle) * 250.f, FMath::Sin(Angle) * 250.f, 0.f), Point))
				{
					continue;
				}
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (Monde->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Point, FRotator::ZeroRotator, Params))
				{
					++Poses;
				}
			}
			if (Poses > 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Menu] Carte sans point de depart : %d departs poses autour de %s"), Poses, *Base.ToString());
				return;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[Menu] Carte sans point de depart et aucun sol trouve pour en poser."));
	}

	FDelegateHandle Abonnement;
};

IMPLEMENT_MODULE(FChellesMenuModule, ChellesMenu)
