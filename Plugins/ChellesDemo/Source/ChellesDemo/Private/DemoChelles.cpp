// CHELLES 3D - Version demo (beta) : 20 minutes de jeu puis ecran FIN DE LA DEMO.
#include "DemoChelles.h"
#include "SDemoChelles.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogChellesDemo, Log, All);

namespace
{
	const TCHAR* const SectionDemo = TEXT("ChellesDemo");

	// Garde le temps d'une carte a l'autre (Solo -> Multi recharge la carte) tant que le jeu reste ouvert.
	double GSecondesJouees = 0.0;
	bool GTempsCharge = false;
	bool GTestEditeur = false;
	float GDureeTest = 0.f;

	bool LireBool(const TCHAR* Cle, bool Defaut)
	{
		bool Valeur = Defaut;
		if (GConfig)
		{
			GConfig->GetBool(SectionDemo, Cle, Valeur, GGameIni);
		}
		return Valeur;
	}

	FString LireTexte(const TCHAR* Cle, const FString& Defaut)
	{
		FString Valeur;
		if (GConfig && GConfig->GetString(SectionDemo, Cle, Valeur, GGameIni) && !Valeur.TrimStartAndEnd().IsEmpty())
		{
			return Valeur.TrimStartAndEnd();
		}
		return Defaut;
	}

	float DureeSecondes()
	{
		if (GTestEditeur && GDureeTest > 0.f)
		{
			return GDureeTest * 60.f;
		}
		float Minutes = 20.f;
		if (GConfig)
		{
			GConfig->GetFloat(SectionDemo, TEXT("DureeMinutes"), Minutes, GGameIni);
		}
		return FMath::Max(0.5f, Minutes) * 60.f;
	}

	/** TempsCumule=True : les 20 minutes comptent au total sur ce PC, meme si on relance le jeu. */
	bool TempsCumule()
	{
		return !GTestEditeur && LireBool(TEXT("TempsCumule"), false);
	}
}

ADemoChelles::ADemoChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // pour garder l'ecran de fin, meme en pause
	bReplicates = false;
	SetCanBeDamaged(false);
}

bool ADemoChelles::DemoActive()
{
#if WITH_EDITOR
	return GTestEditeur;
#else
	return LireBool(TEXT("Active"), true);
#endif
}

void ADemoChelles::LancerTest(UWorld* Monde, float Minutes)
{
	if (Monde)
	{
		for (TActorIterator<ADemoChelles> It(Monde); It; ++It)
		{
			It->Destroy();
		}
	}
	GSecondesJouees = 0.0;
	GTempsCharge = true;
	GTestEditeur = Minutes > 0.f;
	GDureeTest = Minutes;
	if (!GTestEditeur || !Monde || !Monde->IsGameWorld())
	{
		UE_LOG(LogChellesDemo, Display, TEXT("[Demo] test arrete."));
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Monde->SpawnActor<ADemoChelles>(ADemoChelles::StaticClass(), FTransform::Identity, Params);
	UE_LOG(LogChellesDemo, Display, TEXT("[Demo] test lance : %.1f minute(s)."), Minutes);
}

void ADemoChelles::BeginPlay()
{
	Super::BeginPlay();
	if (!GTempsCharge)
	{
		GTempsCharge = true;
		float Deja = 0.f;
		if (TempsCumule() && GConfig && GConfig->GetFloat(SectionDemo, TEXT("TempsJoue"), Deja, GGameUserSettingsIni))
		{
			GSecondesJouees = FMath::Max(0.f, Deja);
		}
	}
	ProchaineMemorisation = FPlatformTime::Seconds() + 10.0;
	bAlerte5 = SecondesRestantes() <= 5.f * 60.f;
	bAlerte1 = SecondesRestantes() <= 60.f;
	UE_LOG(LogChellesDemo, Display, TEXT("[Demo] %.0f s deja jouees sur %.0f s."), GSecondesJouees, DureeSecondes());
}

void ADemoChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	Memoriser();
	RetirerEcran();
	if (Raison == EEndPlayReason::EndPlayInEditor)
	{
		// fin du test dans l'editeur : la prochaine partie (Play) redemarre sans demo
		GTestEditeur = false;
		GSecondesJouees = 0.0;
	}
	Super::EndPlay(Raison);
}

APlayerController* ADemoChelles::Joueur() const
{
	UWorld* Monde = GetWorld();
	return (Monde && GEngine) ? GEngine->GetFirstLocalPlayerController(Monde) : nullptr;
}

float ADemoChelles::SecondesRestantes() const
{
	return FMath::Max(0.f, DureeSecondes() - static_cast<float>(GSecondesJouees));
}

bool ADemoChelles::EnTrainDeJouer() const
{
	const UWorld* Monde = GetWorld();
	const APlayerController* J = Joueur();
	// Menu, habillage, options... : le jeu est en pause ou la souris est affichee -> le temps ne compte pas.
	return Monde && J && !Monde->IsPaused() && !J->bShowMouseCursor;
}

FText ADemoChelles::Alerte() const
{
	return (FPlatformTime::Seconds() < FinAlerte) ? TexteAlerte : FText::GetEmpty();
}

FText ADemoChelles::Message() const
{
	return FText::FromString(LireTexte(TEXT("Message"),
		TEXT("Cette version est une d\u00E9mo. Si tu es int\u00E9ress\u00E9 par la version compl\u00E8te, ")
		TEXT("il te suffit de nous rejoindre sur FACEBOOK et nous en discuterons.")));
}

FString ADemoChelles::Lien() const
{
	return LireTexte(TEXT("Lien"), TEXT("https://www.facebook.com/TIKT0K3D/"));
}

void ADemoChelles::OuvrirLien()
{
	const FString Url = Lien();
	if (Url.StartsWith(TEXT("http")))
	{
		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
	}
}

void ADemoChelles::QuitterLeJeu()
{
	Memoriser();
#if WITH_EDITOR
	if (GTestEditeur)
	{
		// Test dans l'editeur : on enleve juste l'ecran pour ne pas fermer l'editeur
		APlayerController* J = Joueur();
		LancerTest(GetWorld(), 0.f);
		if (J)
		{
			J->SetPause(false);
			J->ResetIgnoreInputFlags();
			J->bShowMouseCursor = false;
			J->SetInputMode(FInputModeGameOnly());
		}
		return;
	}
#endif
	UKismetSystemLibrary::QuitGame(this, Joueur(), EQuitPreference::Quit, false);
}

void ADemoChelles::AjouterEcran()
{
	UWorld* Monde = GetWorld();
	UGameViewportClient* Vue = Monde ? Monde->GetGameViewport() : nullptr;
	if (Ecran.IsValid() || !Vue)
	{
		return;
	}
	Ecran = SNew(SDemoChelles).Demo(this);
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 2000); // au-dessus du menu (1000)
}

void ADemoChelles::RetirerEcran()
{
	if (!Ecran.IsValid())
	{
		return;
	}
	UWorld* Monde = GetWorld();
	if (UGameViewportClient* Vue = Monde ? Monde->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
	}
	Ecran.Reset();
}

void ADemoChelles::Memoriser()
{
	if (TempsCumule() && GConfig)
	{
		GConfig->SetFloat(SectionDemo, TEXT("TempsJoue"), static_cast<float>(GSecondesJouees), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void ADemoChelles::Tick(float Delta)
{
	Super::Tick(Delta);
	if (!Joueur())
	{
		return;
	}
	AjouterEcran();

	if (bTerminee)
	{
		MaintenirFin();
		return;
	}

	if (EnTrainDeJouer())
	{
		// Temps reel, sans compter les gros a-coups (chargement de la carte)
		GSecondesJouees += FMath::Min(FApp::GetDeltaTime(), 0.25);
	}

	const double Maintenant = FPlatformTime::Seconds();
	if (Maintenant >= ProchaineMemorisation)
	{
		ProchaineMemorisation = Maintenant + 10.0;
		Memoriser();
	}

	const float Reste = SecondesRestantes();
	if (!bAlerte5 && Reste <= 5.f * 60.f)
	{
		bAlerte5 = true;
		TexteAlerte = FText::FromString(TEXT("D\u00C9MO : PLUS QUE 5 MINUTES"));
		FinAlerte = Maintenant + 6.0;
	}
	if (!bAlerte1 && Reste <= 60.f)
	{
		bAlerte1 = true;
		TexteAlerte = FText::FromString(TEXT("D\u00C9MO : DERNI\u00C8RE MINUTE !"));
		FinAlerte = Maintenant + 6.0;
	}
	if (Reste <= 0.f)
	{
		Terminer();
	}
}

void ADemoChelles::Terminer()
{
	if (bTerminee)
	{
		return;
	}
	bTerminee = true;
	Memoriser();
	UE_LOG(LogChellesDemo, Display, TEXT("[Demo] temps ecoule : ecran FIN DE LA DEMO."));
	if (APlayerController* J = Joueur())
	{
		J->SetIgnoreMoveInput(true);
		J->SetIgnoreLookInput(true);
	}
	MaintenirFin();
}

void ADemoChelles::MaintenirFin()
{
	UWorld* Monde = GetWorld();
	APlayerController* J = Joueur();
	if (!Monde || !J || !Ecran.IsValid())
	{
		return;
	}
	// En solo, le jeu reste en pause. En multijoueur on ne met pas les autres joueurs en pause.
	if (Monde->GetNetMode() == NM_Standalone && !Monde->IsPaused())
	{
		J->SetPause(true);
	}
	// Si un autre ecran a cache la souris ou repris le clavier, on remet l'ecran de fin
	if (!J->bShowMouseCursor)
	{
		J->bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Ecran->BoutonPrincipal().IsValid() ? Ecran->BoutonPrincipal() : TSharedPtr<SWidget>(Ecran));
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		J->SetInputMode(Mode);
	}
}
