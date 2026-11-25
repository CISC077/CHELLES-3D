// CHELLES 3D - Plugin ChellesChargement (25/11/2026)
// 1) Jeu final : ecran de chargement (MoviePlayer) pendant l'ouverture d'une carte
//    (lancement du jeu, SOLO, rejoindre un serveur).
// 2) Debut de partie (jeu final ET bouton Jouer de l'editeur) : le meme ecran reste par-dessus
//    le jeu le temps que les textures arrivent (2,5 s mini, 10 s maxi), puis disparait en fondu.
// Reglages et astuces : Plugins\ChellesChargement\Astuces.txt
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Containers/Ticker.h"
#include "ContentStreaming.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"

#include "ReglagesChargement.h"
#include "SEcranChargement.h"

class FChellesChargementModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FReglagesChargement::Get().Charger();
		const FReglagesChargement& Reglages = FReglagesChargement::Get();

		if (Reglages.bChargementCarte && IsMoviePlayerEnabled())
		{
			if (IGameMoviePlayer* Lecteur = GetMoviePlayer())
			{
				// Appele par Unreal juste avant d'afficher un ecran de chargement
				Lecteur->OnPrepareLoadingScreen().AddRaw(this, &FChellesChargementModule::PreparerEcran);
			}
			PoigneeCarte = FCoreUObjectDelegates::PreLoadMap.AddRaw(this, &FChellesChargementModule::AvantCarte);
		}

		if (Reglages.bDebutPartie && !IsRunningCommandlet() && !IsRunningDedicatedServer())
		{
			PoigneeMonde = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FChellesChargementModule::MondeCree);
			PoigneeTicker = FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FChellesChargementModule::Surveiller), 0.f);
		}
	}

	virtual void ShutdownModule() override
	{
		if (PoigneeCarte.IsValid())
		{
			FCoreUObjectDelegates::PreLoadMap.Remove(PoigneeCarte);
		}
		if (PoigneeMonde.IsValid())
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(PoigneeMonde);
		}
		if (PoigneeTicker.IsValid())
		{
			FTicker::GetCoreTicker().RemoveTicker(PoigneeTicker);
		}
		Superposes.Reset();
	}

	virtual bool IsGameModule() const override
	{
		return true;
	}

private:
	struct FSuperpose
	{
		TWeakObjectPtr<UWorld> Monde;
		TWeakObjectPtr<UGameViewportClient> Vue;
		TSharedPtr<SEcranChargement> Ecran;
		double Cree = 0.0;
		double Affiche = 0.0;
		bool bFin = false;
	};

	// ------------------------------------------------ chargement d'une carte (jeu final)

	void PreparerEcran()
	{
		IGameMoviePlayer* Lecteur = GetMoviePlayer();
		if (!Lecteur)
		{
			return;
		}
		FLoadingScreenAttributes Ecran;
		Ecran.bAutoCompleteWhenLoadingCompletes = true;
		Ecran.bMoviesAreSkippable = false;
		Ecran.MinimumLoadingScreenDisplayTime = FReglagesChargement::Get().DureeMini;
		Ecran.WidgetLoadingScreen = SNew(SEcranChargement).Carte(CarteEnCours).Superpose(false);
		Lecteur->SetupLoadingScreen(Ecran);
	}

	void AvantCarte(const FString& Carte)
	{
		CarteEnCours = Carte;
		IGameMoviePlayer* Lecteur = GetMoviePlayer();
		if (Lecteur && !Lecteur->LoadingScreenIsPrepared() && !Lecteur->IsMovieCurrentlyPlaying())
		{
			PreparerEcran();
		}
	}

	// ------------------------------------------------ debut de partie (par-dessus le jeu)

	void MondeCree(UWorld* Monde, const UWorld::InitializationValues IVS)
	{
		if (!Monde || !(Monde->WorldType == EWorldType::Game || Monde->WorldType == EWorldType::PIE))
		{
			return;
		}
		FSuperpose Nouveau;
		Nouveau.Monde = Monde;
		Nouveau.Cree = FPlatformTime::Seconds();
		Superposes.Add(Nouveau);
	}

	bool Surveiller(float DeltaTime)
	{
		if (Superposes.Num() == 0)
		{
			return true;
		}
		const FReglagesChargement& Reglages = FReglagesChargement::Get();
		const double Maintenant = FPlatformTime::Seconds();
		for (int32 i = Superposes.Num() - 1; i >= 0; --i)
		{
			FSuperpose& Entree = Superposes[i];
			UWorld* Monde = Entree.Monde.Get();
			if (!Entree.Ecran.IsValid())
			{
				// pas encore affiche : on attend que le jeu ait commence
				if (!Monde || Maintenant - Entree.Cree > 120.0)
				{
					Superposes.RemoveAt(i);
					continue;
				}
				if (Monde->GetNetMode() == NM_DedicatedServer)
				{
					Superposes.RemoveAt(i);
					continue;
				}
				UGameViewportClient* Vue = Monde->GetGameViewport();
				if (!Vue || !Monde->HasBegunPlay())
				{
					continue;
				}
				Entree.Vue = Vue;
				Entree.Ecran = SNew(SEcranChargement).Carte(UWorld::RemovePIEPrefix(Monde->GetMapName())).Superpose(true);
				Vue->AddViewportWidgetContent(Entree.Ecran.ToSharedRef(), 10000);
				Entree.Affiche = Maintenant;
				continue;
			}

			UGameViewportClient* Vue = Entree.Vue.Get();
			if (!Monde || !Vue)
			{
				if (Vue)
				{
					Vue->RemoveViewportWidgetContent(Entree.Ecran.ToSharedRef());
				}
				Superposes.RemoveAt(i);
				continue;
			}
			const double Ecoule = Maintenant - Entree.Affiche;
			if (!Entree.bFin)
			{
				const bool bTexturesPretes = IStreamingManager::Get().GetNumWantingResources() == 0;
				if (Ecoule >= Reglages.DureeMini && (bTexturesPretes || Ecoule >= Reglages.DureeMaxi))
				{
					Entree.Ecran->Terminer();
					Entree.bFin = true;
				}
			}
			else if (Entree.Ecran->EstFini())
			{
				Vue->RemoveViewportWidgetContent(Entree.Ecran.ToSharedRef());
				Superposes.RemoveAt(i);
			}
		}
		return true;
	}

	TArray<FSuperpose> Superposes;
	FString CarteEnCours;
	FDelegateHandle PoigneeCarte;
	FDelegateHandle PoigneeMonde;
	FDelegateHandle PoigneeTicker;
};

IMPLEMENT_MODULE(FChellesChargementModule, ChellesChargement)
