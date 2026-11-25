// CHELLES 3D - PvP : gestionnaire des combats entre joueurs
//
//   - le PvP se joue en MANCHES de DureeMancheMinutes (PvP.txt), puis 12 s de treve + tableau des scores ;
//   - un joueur qui recoit CoupsAvantATerre coups d'autres joueurs tombe A TERRE (il ne meurt pas) ;
//     FPS_Game ne peut pas le tuer avec ces coups : le serveur remet sa vie d'avant apres chaque coup ;
//   - un ami lui JETTE UN KIT DE SURVIE (touche Y) : quand le kit tombe a moins de 2,5 m de lui, il est
//     reanime automatiquement (50 % de sa vie). Le lancer vise tout seul l'ami a terre qu'on regarde ;
//   - personne ne l'aide pendant DureeATerreSecondes : il est elimine (mort normale de FPS_Game).
//
// Aucun asset ni Blueprint du projet n'est modifie. Les autres plugins ne sont lus que par le nom de leurs
// variables (ChellesSurvie : Mode ; ChellesMenu : reglage Pvp) : les plugins se compilent separement.
#include "PvPChelles.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "KitSurvie.h"
#include "PvPOutils.h"

namespace
{
	const FName TagHorsJeu(TEXT("ChellesHorsJeu"));
	const FName TagKit(TEXT("ChellesKitSurvie"));

	/** Vie donnee pendant le calcul d'un coup PvP : FPS_Game ne peut pas tuer avec. */
	constexpr float VieBouclier = 10000000.f;

	FString DossierPlugin()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesPvP"));
	}

	bool Oui(const FString& Valeur)
	{
		const FString V = Valeur.TrimStartAndEnd().ToLower();
		return V == TEXT("oui") || V == TEXT("1") || V == TEXT("true") || V == TEXT("vrai") || V == TEXT("yes");
	}

	FString Chrono(float Secondes)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Secondes));
		return FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60);
	}

	void Accroupir(APawn* P, bool bOui)
	{
		ACharacter* Perso = Cast<ACharacter>(P);
		if (!Perso)
		{
			return;
		}
		if (bOui)
		{
			if (Perso->GetCharacterMovement())
			{
				Perso->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
			}
			Perso->Crouch();
		}
		else
		{
			Perso->UnCrouch();
		}
	}
}

APvPChelles::APvPChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 4.f;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void APvPChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APvPChelles, bActif);
	DOREPLIFETIME(APvPChelles, Manche);
	DOREPLIFETIME(APvPChelles, FinManche);
	DOREPLIFETIME(APvPChelles, NumeroManche);
	DOREPLIFETIME(APvPChelles, CoupsMax);
	DOREPLIFETIME(APvPChelles, DureeATerreRepl);
	DOREPLIFETIME(APvPChelles, TitreScores);
}

bool APvPChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

float APvPChelles::Maintenant() const
{
	const UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return 0.f;
	}
	const AGameStateBase* EtatJeu = Monde->GetGameState();
	return EtatJeu ? EtatJeu->GetServerWorldTimeSeconds() : Monde->GetTimeSeconds();
}

// ===================================================================================
//  Demarrage / arret
// ===================================================================================

void APvPChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages(); // chaque machine : la touche du kit ; le serveur : tout le reste
	if (EstServeur())
	{
		UE_LOG(LogTemp, Log, TEXT("[PvP] Pret : %d coups avant d'etre a terre, manches de %.0f min, %.0f s a terre, %d kits de survie, touche %s."),
			CoupsMax, DureeManche / 60.f, DureeATerre, KitsDepart, *ToucheKit.GetDisplayName().ToString());
	}
}

void APvPChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerEcran();
	Super::EndPlay(Raison);
}

void APvPChelles::ChargerReglages()
{
	const int32 CoupsRecus = CoupsMax;
	const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("PvP.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[PvP] %s absent : reglages par defaut."), *Chemin);
	}
	for (const FString& Brute : Lignes)
	{
		FString Ligne = Brute;
		int32 Diese = INDEX_NONE;
		if (Ligne.FindChar(TEXT('#'), Diese))
		{
			Ligne = Ligne.Left(Diese);
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle = Cle.TrimStartAndEnd();
		Valeur = Valeur.TrimStartAndEnd();
		const float N = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));

		if (Cle == TEXT("Actif")) bActifFichier = Oui(Valeur);
		else if (Cle == TEXT("DureeMancheMinutes")) DureeManche = FMath::Clamp(N, 0.f, 600.f) * 60.f;
		else if (Cle == TEXT("CoupsAvantATerre")) CoupsMax = FMath::Clamp(FMath::RoundToInt(N), 1, 100);
		else if (Cle == TEXT("DureeATerreSecondes")) DureeATerre = FMath::Clamp(N, 3.f, 600.f);
		else if (Cle == TEXT("RecuperationCoupSecondes")) RecuperationCoup = FMath::Clamp(N, 0.f, 600.f);
		else if (Cle == TEXT("VieApresReanimation")) PourcentVieReanimation = FMath::Clamp(N, 1.f, 100.f);
		else if (Cle == TEXT("KitsDeSurvieDepart")) KitsDepart = FMath::Clamp(FMath::RoundToInt(N), 0, 99);
		else if (Cle == TEXT("KitsDeSurvieMax")) KitsMax = FMath::Clamp(FMath::RoundToInt(N), 1, 99);
		else if (Cle == TEXT("KitsRendusAChaqueManche")) bKitsAChaqueManche = Oui(Valeur);
		else if (Cle == TEXT("RayonKitMetres")) RayonKit = FMath::Clamp(N, 0.5f, 20.f) * 100.f;
		else if (Cle == TEXT("ForceLancer")) ForceLancer = FMath::Clamp(N, 200.f, 5000.f);
		else if (Cle == TEXT("AideAuLancerMetres")) PorteeAide = FMath::Clamp(N, 0.f, 200.f) * 100.f;
		else if (Cle == TEXT("DureeVieKitSecondes")) DureeVieKit = FMath::Clamp(N, 3.f, 600.f);
		else if (Cle == TEXT("JoueursMinimum")) JoueursMin = FMath::Clamp(FMath::RoundToInt(N), 1, 64);
		else if (Cle == TEXT("DureeScoresSecondes")) DureeScores = FMath::Clamp(N, 0.f, 120.f);
		else if (Cle == TEXT("ModeleKit")) ModeleKit = Valeur;
		else if (Cle == TEXT("ToucheKit"))
		{
			const FKey Touche(*Valeur);
			if (Touche.IsValid())
			{
				ToucheKit = Touche;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[PvP] Touche inconnue dans PvP.txt : %s (Y gardee)."), *Valeur);
			}
		}
	}
	KitsDepart = FMath::Min(KitsDepart, KitsMax);
	if (!EstServeur())
	{
		// Chez un joueur connecte : seule la touche compte, le reste vient du serveur par le reseau
		CoupsMax = CoupsRecus;
		return;
	}
	DureeATerreRepl = DureeATerre;
}

void APvPChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (EstServeur())
	{
		TickServeur(Maintenant());
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLocal(DeltaSeconds);
	}
}

// ===================================================================================
//  Serveur : joueurs et manches
// ===================================================================================

bool APvPChelles::CalculerActif() const
{
	if (!bActifFichier)
	{
		return false;
	}
	// Reglage PVP du menu de l'hote (comme le plugin ChellesArmes). Sans reglage : PvP permis.
	if (GetNetMode() != NM_Standalone && GConfig)
	{
		bool bPvpMenu = true;
		GConfig->GetBool(TEXT("ChellesMenu"), TEXT("Pvp"), bPvpMenu, GGameUserSettingsIni);
		if (!bPvpMenu)
		{
			return false;
		}
	}
	// Pendant un mode de survie (Dernier Refuge...), on joue ensemble : pas de PvP
	if (AActor* Survie = PvPOutils::ActeurDeClasse(GetWorld(), TEXT("SurvieChelles")))
	{
		float Mode = 0.f;
		if (PvPOutils::LireNombre(Survie, { TEXT("Mode") }, Mode) && Mode > 0.5f)
		{
			return false;
		}
	}
	return true;
}

void APvPChelles::Relais(TArray<APvPRelais*>& Resultat) const
{
	Resultat.Reset();
	for (TActorIterator<APvPRelais> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			Resultat.Add(*It);
		}
	}
}

APvPRelais* APvPChelles::RelaisDe(const AController* Controleur) const
{
	if (!Controleur)
	{
		return nullptr;
	}
	for (TActorIterator<APvPRelais> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == Controleur)
		{
			return *It;
		}
	}
	return nullptr;
}

APvPRelais* APvPChelles::RelaisDuPion(const APawn* Pion) const
{
	if (!Pion)
	{
		return nullptr;
	}
	for (TActorIterator<APvPRelais> It(GetWorld()); It; ++It)
	{
		if (It->Pion == Pion)
		{
			return *It;
		}
	}
	return nullptr;
}

APvPRelais* APvPChelles::MonRelais() const
{
	UWorld* Monde = GetWorld();
	APlayerController* PC = Monde ? Monde->GetFirstPlayerController() : nullptr;
	return (PC && PC->IsLocalController()) ? RelaisDe(PC) : nullptr;
}

void APvPChelles::GererRelais()
{
	UWorld* Monde = GetWorld();
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		APvPRelais* R = RelaisDe(PC);
		if (!R)
		{
			FActorSpawnParameters Params;
			Params.Owner = PC;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			R = Monde->SpawnActor<APvPRelais>(APvPRelais::StaticClass(), FTransform::Identity, Params);
			if (!R)
			{
				continue;
			}
			R->Kits = KitsDepart;
			ListeRelais.Add(R);
		}
		if (PC->PlayerState)
		{
			R->Nom = PC->PlayerState->GetPlayerName();
		}
		if (R->Nom.IsEmpty())
		{
			R->Nom = TEXT("Joueur");
		}

		APawn* P = PC->GetPawn();
		if (P != R->Pion)
		{
			R->Pion = P;
			// Nouveau personnage (reapparition apres la mort...) : on repart debout, sans coups
			if (P && R->Etat != EEtatJoueurPvP::Debout)
			{
				RemettreDebout(R, true);
			}
		}
		Ecouter(R, P);
	}
	// Les joueurs partis
	for (int32 i = ListeRelais.Num() - 1; i >= 0; --i)
	{
		APvPRelais* R = ListeRelais[i];
		if (!IsValid(R))
		{
			ListeRelais.RemoveAt(i);
		}
		else if (!IsValid(R->GetOwner()))
		{
			R->Destroy();
			ListeRelais.RemoveAt(i);
		}
	}
}

void APvPChelles::Ecouter(APvPRelais* R, APawn* P)
{
	if (R->PionEcoute.Get() == P)
	{
		return;
	}
	if (APawn* Ancien = R->PionEcoute.Get())
	{
		Ancien->OnTakePointDamage.RemoveDynamic(this, &APvPChelles::QuandDegatsPrecis);
		Ancien->OnTakeRadialDamage.RemoveDynamic(this, &APvPChelles::QuandDegatsZone);
		Ancien->OnTakeAnyDamage.RemoveDynamic(this, &APvPChelles::QuandDegats);
	}
	R->PionEcoute = nullptr;
	// Seulement les personnages FPS_Game (avec une vie) : pas les vehicules, le cheval...
	if (IsValid(P) && PvPOutils::AUneVie(P))
	{
		P->OnTakePointDamage.AddUniqueDynamic(this, &APvPChelles::QuandDegatsPrecis);
		P->OnTakeRadialDamage.AddUniqueDynamic(this, &APvPChelles::QuandDegatsZone);
		P->OnTakeAnyDamage.AddUniqueDynamic(this, &APvPChelles::QuandDegats);
		R->PionEcoute = P;
	}
}

void APvPChelles::TickServeur(float Heure)
{
	if (Heure >= ProchaineGestion)
	{
		ProchaineGestion = Heure + 0.5f;
		bActif = CalculerActif();
		GererRelais();
	}

	TArray<APvPRelais*> Tous;
	Relais(Tous);
	int32 Presents = 0;
	for (APvPRelais* R : Tous)
	{
		if (IsValid(R->Pion))
		{
			++Presents;
		}
	}

	// ---- Les manches ----
	if (!bActif)
	{
		if (Manche != EEtatManchePvP::Attente)
		{
			Manche = EEtatManchePvP::Attente;
			for (APvPRelais* R : Tous)
			{
				if (R->Etat == EEtatJoueurPvP::ATerre)
				{
					Reanimer(R, nullptr, false);
				}
			}
			AvisATous(TEXT("PvP coupé."));
		}
		return;
	}
	switch (Manche)
	{
	case EEtatManchePvP::Attente:
		if (Presents >= JoueursMin)
		{
			NouvelleManche();
		}
		break;
	case EEtatManchePvP::EnCours:
		if (Presents < JoueursMin)
		{
			Manche = EEtatManchePvP::Attente;
			FinManche = 0.f;
			AvisATous(TEXT("PvP en pause : il faut au moins 2 joueurs."));
		}
		else if (FinManche > 0.f && Heure >= FinManche)
		{
			FinDeManche();
		}
		break;
	case EEtatManchePvP::Scores:
		if (Heure >= FinManche)
		{
			if (Presents >= JoueursMin)
			{
				NouvelleManche();
			}
			else
			{
				Manche = EEtatManchePvP::Attente;
				FinManche = 0.f;
			}
		}
		break;
	}

	// ---- Chaque joueur ----
	for (APvPRelais* R : Tous)
	{
		APawn* P = R->Pion;
		if (R->bCoupEnCours)
		{
			// Securite : un coup dont FPS_Game n'a jamais fini le calcul ne doit pas laisser le bouclier
			R->bCoupEnCours = false;
			if (IsValid(P) && PvPOutils::Vie(P) > VieBouclier * 0.5f)
			{
				PvPOutils::DonnerVie(P, R->VieAvantCoup);
			}
		}
		switch (R->Etat)
		{
		case EEtatJoueurPvP::Debout:
			if (IsValid(P) && PvPOutils::EstMort(P))
			{
				R->Etat = EEtatJoueurPvP::Mort; // mort par autre chose (zombie, chute...) : FPS_Game s'en occupe
				R->Coups = 0;
			}
			else if (R->Coups > 0 && RecuperationCoup > 0.f && Heure - R->HeureDernierCoup >= RecuperationCoup)
			{
				--R->Coups; // un coup "oublie" a chaque periode sans etre touche
				R->HeureDernierCoup = Heure;
			}
			break;

		case EEtatJoueurPvP::ATerre:
			if (!IsValid(P) || PvPOutils::EstMort(P))
			{
				R->Etat = EEtatJoueurPvP::Mort;
			}
			else if (Heure >= R->FinATerre)
			{
				Achever(R);
			}
			break;

		case EEtatJoueurPvP::Mort:
			// Coup de grace donne mais FPS_Game n'a pas tue (tir ami coupe...) : on fait reapparaitre nous-memes
			if (R->bCoupDeGrace && IsValid(P) && P == R->PionEcoute.Get())
			{
				const bool bMortFPS = PvPOutils::EstMort(P);
				if ((!bMortFPS && Heure - R->HeureCoupDeGrace > 4.f) || Heure - R->HeureCoupDeGrace > 20.f)
				{
					R->bCoupDeGrace = false;
					APlayerController* PC = R->GetPC();
					AGameModeBase* Jeu = GetWorld()->GetAuthGameMode();
					if (PC && Jeu)
					{
						UE_LOG(LogTemp, Log, TEXT("[PvP] %s : reapparition forcee."), *R->Nom);
						PC->UnPossess();
						P->Destroy();
						Jeu->RestartPlayer(PC);
					}
				}
			}
			break;
		}
	}

	// ---- Kits de survie au sol ----
	if (Heure >= ProchaineVerifKits)
	{
		ProchaineVerifKits = Heure + 0.2f;
		VerifierKits();
	}
}

void APvPChelles::NouvelleManche()
{
	++NumeroManche;
	Manche = EEtatManchePvP::EnCours;
	FinManche = DureeManche > 0.f ? Maintenant() + DureeManche : 0.f;
	TitreScores.Reset();
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	for (APvPRelais* R : Tous)
	{
		if (R->Etat == EEtatJoueurPvP::ATerre)
		{
			Reanimer(R, nullptr, false);
		}
		R->Coups = 0;
		R->MisesATerre = 0;
		R->Eliminations = 0;
		R->Reanimations = 0;
		R->FoisATerre = 0;
		if (bKitsAChaqueManche)
		{
			R->Kits = FMath::Max(R->Kits, KitsDepart);
		}
	}
	const FString Duree = DureeManche > 0.f ? FString::Printf(TEXT("%.0f min"), DureeManche / 60.f) : FString(TEXT("sans limite"));
	AvisATous(FString::Printf(TEXT("MANCHE %d : PvP ouvert (%s). %d coups = à terre. Touche %s = jeter un kit de survie à un ami."),
		NumeroManche, *Duree, CoupsMax, *ToucheKit.GetDisplayName().ToString()));
	UE_LOG(LogTemp, Log, TEXT("[PvP] Manche %d lancee (%d joueur(s))."), NumeroManche, Tous.Num());
}

void APvPChelles::FinDeManche()
{
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	APvPRelais* Meilleur = nullptr;
	for (APvPRelais* R : Tous)
	{
		if (!Meilleur || R->Eliminations > Meilleur->Eliminations
			|| (R->Eliminations == Meilleur->Eliminations && R->MisesATerre > Meilleur->MisesATerre))
		{
			Meilleur = R;
		}
		if (R->Etat == EEtatJoueurPvP::ATerre)
		{
			Reanimer(R, nullptr, false); // treve : tout le monde se releve
		}
	}
	if (Meilleur && (Meilleur->Eliminations > 0 || Meilleur->MisesATerre > 0))
	{
		TitreScores = FString::Printf(TEXT("FIN DE LA MANCHE %d : %s gagne (%d élimination%s)"), NumeroManche, *Meilleur->Nom,
			Meilleur->Eliminations, Meilleur->Eliminations > 1 ? TEXT("s") : TEXT(""));
	}
	else
	{
		TitreScores = FString::Printf(TEXT("FIN DE LA MANCHE %d : match nul"), NumeroManche);
	}
	Manche = EEtatManchePvP::Scores;
	FinManche = Maintenant() + DureeScores;
	AvisATous(TitreScores);
	UE_LOG(LogTemp, Log, TEXT("[PvP] %s"), *TitreScores);
}

// ===================================================================================
//  Serveur : les coups entre joueurs
// ===================================================================================

bool APvPChelles::EstCoupPvP(AActor* Victime, AController* Auteur, AActor* Cause, APvPRelais*& RVictime, AController*& AuteurReel) const
{
	RVictime = nullptr;
	AuteurReel = Auteur;
	if (!bActif)
	{
		return false; // PvP coupe : FPS_Game fait comme avant
	}
	APawn* PV = Cast<APawn>(Victime);
	RVictime = RelaisDuPion(PV);
	if (!RVictime)
	{
		return false;
	}
	if (!AuteurReel && Cause)
	{
		AuteurReel = Cause->GetInstigatorController();
		if (!AuteurReel)
		{
			if (const APawn* PorteurArme = Cast<APawn>(Cause->GetOwner()))
			{
				AuteurReel = PorteurArme->GetController();
			}
		}
	}
	const APlayerController* Tireur = Cast<APlayerController>(AuteurReel);
	return Tireur && Tireur != PV->GetController();
}

void APvPChelles::QuandDegatsPrecis(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	// Appele AVANT que FPS_Game retire la vie (evenement AnyDamage) : on met un bouclier, remis juste apres
	APvPRelais* R = nullptr;
	AController* Auteur = nullptr;
	if (!EstCoupPvP(DamagedActor, InstigatedBy, DamageCauser, R, Auteur) || R->bCoupDeGrace || R->bCoupEnCours)
	{
		return;
	}
	R->bCoupEnCours = true;
	R->VieAvantCoup = PvPOutils::Vie(DamagedActor);
	PvPOutils::DonnerVie(DamagedActor, VieBouclier);
	if (Manche == EEtatManchePvP::EnCours && R->Etat == EEtatJoueurPvP::Debout)
	{
		CompterCoup(R, Auteur);
	}
}

void APvPChelles::QuandDegatsZone(AActor* DamagedActor, float Damage, const UDamageType* DamageType, FVector Origin, FHitResult HitInfo, AController* InstigatedBy, AActor* DamageCauser)
{
	// Grenades, explosions : meme chose qu'un tir
	QuandDegatsPrecis(DamagedActor, Damage, InstigatedBy, Origin, nullptr, NAME_None, FVector::ZeroVector, DamageType, DamageCauser);
}

void APvPChelles::QuandDegats(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	// Appele APRES le calcul de FPS_Game
	APvPRelais* R = nullptr;
	AController* Auteur = nullptr;
	if (!EstCoupPvP(DamagedActor, InstigatedBy, DamageCauser, R, Auteur) || R->bCoupDeGrace)
	{
		return;
	}
	if (R->bCoupEnCours)
	{
		// Tir ou explosion : on remet la vie d'avant le coup (le PvP compte les coups, pas la vie)
		R->bCoupEnCours = false;
		PvPOutils::DonnerVie(DamagedActor, R->VieAvantCoup);
		return;
	}
	// Degats "simples" (sans point d'impact) d'un autre joueur : FPS_Game les a deja retires, on les rend
	if (Manche == EEtatManchePvP::EnCours && R->Etat == EEtatJoueurPvP::Debout)
	{
		CompterCoup(R, Auteur);
	}
	if (!PvPOutils::EstMort(DamagedActor))
	{
		const float Vie = PvPOutils::Vie(DamagedActor);
		PvPOutils::DonnerVie(DamagedActor, FMath::Min(Vie + Damage, PvPOutils::VieMax(DamagedActor)));
	}
}

void APvPChelles::CompterCoup(APvPRelais* R, AController* Auteur)
{
	R->Coups = FMath::Min(R->Coups + 1, CoupsMax);
	R->DernierAttaquant = Auteur;
	R->HeureDernierCoup = Maintenant();
	if (R->Coups >= CoupsMax)
	{
		MettreATerre(R);
	}
}

void APvPChelles::MettreATerre(APvPRelais* R)
{
	R->Etat = EEtatJoueurPvP::ATerre;
	R->FinATerre = Maintenant() + DureeATerre;
	++R->FoisATerre;
	APvPRelais* Attaquant = RelaisDe(R->DernierAttaquant.Get());
	if (Attaquant && Attaquant != R)
	{
		++Attaquant->MisesATerre;
	}
	if (APawn* P = R->Pion)
	{
		P->Tags.AddUnique(TagHorsJeu);
		Accroupir(P, true);
	}
	AvisATous(FString::Printf(TEXT("%s a mis %s à terre ! Un ami peut lui jeter un kit de survie (touche %s)."),
		Attaquant ? *Attaquant->Nom : TEXT("Un joueur"), *R->Nom, *ToucheKit.GetDisplayName().ToString()));
	UE_LOG(LogTemp, Log, TEXT("[PvP] %s a terre."), *R->Nom);
}

void APvPChelles::RemettreDebout(APvPRelais* R, bool bToutOublier)
{
	R->Etat = EEtatJoueurPvP::Debout;
	R->Coups = 0;
	R->bCoupDeGrace = false;
	R->bCoupEnCours = false;
	R->FinATerre = 0.f;
	if (bToutOublier)
	{
		R->DernierAttaquant = nullptr;
	}
	if (APawn* P = R->Pion)
	{
		P->Tags.Remove(TagHorsJeu);
		Accroupir(P, false);
	}
}

void APvPChelles::Reanimer(APvPRelais* R, APvPRelais* Sauveur, bool bParKit)
{
	RemettreDebout(R, true);
	if (APawn* P = R->Pion)
	{
		PvPOutils::EcrireBooleen(P, { TEXT("IsDead"), TEXT("isDead") }, false);
		const float VieMax = PvPOutils::VieMax(P);
		PvPOutils::DonnerVie(P, FMath::Max(PvPOutils::Vie(P), VieMax * PourcentVieReanimation / 100.f));
	}
	if (bParKit && Sauveur && Sauveur != R)
	{
		++Sauveur->Reanimations;
		AvisATous(FString::Printf(TEXT("%s a réanimé %s avec un kit de survie !"), *Sauveur->Nom, *R->Nom));
	}
	else if (bParKit)
	{
		AvisATous(FString::Printf(TEXT("%s est réanimé par un kit de survie !"), *R->Nom));
	}
	UE_LOG(LogTemp, Log, TEXT("[PvP] %s reanime."), *R->Nom);
}

void APvPChelles::Achever(APvPRelais* R)
{
	APvPRelais* Attaquant = RelaisDe(R->DernierAttaquant.Get());
	if (Attaquant && Attaquant != R)
	{
		++Attaquant->Eliminations;
	}
	R->Etat = EEtatJoueurPvP::Mort;
	R->Coups = 0;
	R->bCoupDeGrace = true;
	R->HeureCoupDeGrace = Maintenant();
	AvisATous(Attaquant
		? FString::Printf(TEXT("%s est éliminé par %s."), *R->Nom, *Attaquant->Nom)
		: FString::Printf(TEXT("%s est éliminé."), *R->Nom));
	UE_LOG(LogTemp, Log, TEXT("[PvP] %s elimine."), *R->Nom);

	APawn* P = R->Pion;
	if (!IsValid(P))
	{
		return;
	}
	P->Tags.Remove(TagHorsJeu);
	Accroupir(P, false);
	// Mort normale de FPS_Game (animation, reapparition) : on laisse passer ce coup-la
	PvPOutils::DonnerVie(P, 1.f);
	UGameplayStatics::ApplyDamage(P, PvPOutils::VieMax(P) * 100.f, R->DernierAttaquant.Get(), nullptr, UDamageType::StaticClass());
}

// ===================================================================================
//  Serveur : kits de survie
// ===================================================================================

APvPRelais* APvPChelles::CibleDuLancer(APawn* Lanceur, const FVector& Oeil, const FVector& Direction) const
{
	if (PorteeAide <= 0.f)
	{
		return nullptr;
	}
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	APvPRelais* Meilleur = nullptr;
	float MeilleurAlignement = FMath::Cos(FMath::DegreesToRadians(30.f));
	for (APvPRelais* R : Tous)
	{
		if (R->Etat != EEtatJoueurPvP::ATerre || !IsValid(R->Pion) || R->Pion == Lanceur)
		{
			continue;
		}
		const FVector Vers = R->Pion->GetActorLocation() - Oeil;
		if (Vers.Size() > PorteeAide)
		{
			continue;
		}
		const float Alignement = FVector::DotProduct(Vers.GetSafeNormal(), Direction);
		if (Alignement >= MeilleurAlignement)
		{
			MeilleurAlignement = Alignement;
			Meilleur = R;
		}
	}
	return Meilleur;
}

void APvPChelles::JeterKit(APvPRelais* Lanceur)
{
	if (!Lanceur || !bActif)
	{
		return;
	}
	APawn* P = Lanceur->Pion;
	const float Heure = Maintenant();
	if (!IsValid(P) || Lanceur->Etat != EEtatJoueurPvP::Debout || Heure < Lanceur->ProchainKitPermis)
	{
		return;
	}
	if (Lanceur->Kits <= 0)
	{
		Lanceur->ClientAvis(TEXT("Tu n'as plus de kit de survie."));
		return;
	}

	FVector Oeil;
	FRotator Visee;
	P->GetActorEyesViewPoint(Oeil, Visee);
	const FVector Direction = Visee.Vector();
	const FVector Depart = Oeil + Direction * 60.f - FVector(0.f, 0.f, 15.f);

	// Aide au lancer : si on regarde un ami a terre, le kit part en cloche vers lui
	FVector Vitesse = Direction * ForceLancer + FVector(0.f, 0.f, 250.f);
	APvPRelais* Cible = CibleDuLancer(P, Oeil, Direction);
	if (Cible)
	{
		FVector Arc;
		if (UGameplayStatics::SuggestProjectileVelocity_CustomArc(this, Arc, Depart, Cible->Pion->GetActorLocation(), 0.f, 0.45f))
		{
			Vitesse = Arc;
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = Lanceur;
	Params.Instigator = P;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKitSurvie* Kit = GetWorld()->SpawnActor<AKitSurvie>(AKitSurvie::StaticClass(), FTransform(Visee, Depart), Params);
	if (!Kit)
	{
		return;
	}
	if (!ModeleKit.IsEmpty())
	{
		Kit->ChangerModele(ModeleKit);
	}
	Kit->SetLifeSpan(DureeVieKit);
	Kit->Lancer(Vitesse);

	--Lanceur->Kits;
	Lanceur->ProchainKitPermis = Heure + AttenteEntreKits;
	Lanceur->ClientAvis(Cible
		? FString::Printf(TEXT("Kit de survie lancé vers %s (il t'en reste %d)."), *Cible->Nom, Lanceur->Kits)
		: FString::Printf(TEXT("Kit de survie lancé (il t'en reste %d)."), Lanceur->Kits));
}

void APvPChelles::VerifierKits()
{
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	for (APvPRelais* R : Tous)
	{
		if (R->Etat != EEtatJoueurPvP::ATerre || !IsValid(R->Pion))
		{
			continue;
		}
		// Les objets qui bougent autour du joueur a terre (rapide : pas de tour de tout le niveau)
		TArray<FOverlapResult> Objets;
		FCollisionObjectQueryParams Types;
		Types.AddObjectTypesToQuery(ECC_PhysicsBody);
		Types.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PvPKitSurvie), false, R->Pion);
		GetWorld()->OverlapMultiByObjectType(Objets, R->Pion->GetActorLocation(), FQuat::Identity, Types, FCollisionShape::MakeSphere(RayonKit), Params);
		for (const FOverlapResult& O : Objets)
		{
			AActor* Kit = O.GetActor();
			if (!IsValid(Kit) || !Kit->Tags.Contains(TagKit))
			{
				continue;
			}
			// Qui l'a jete ? (notre kit : le relais ; un kit d'un autre plugin : son pion)
			APvPRelais* Sauveur = Cast<APvPRelais>(Kit->GetOwner());
			if (!Sauveur)
			{
				Sauveur = RelaisDuPion(Kit->GetInstigator());
			}
			if (Sauveur == R)
			{
				continue; // on ne se reanime pas avec son propre kit
			}
			Kit->Destroy();
			Reanimer(R, Sauveur, true);
			break;
		}
	}
}

void APvPChelles::AjouterKitsSurvie(APawn* Joueur, int32 Nombre)
{
	if (!EstServeur())
	{
		return;
	}
	APvPRelais* R = RelaisDuPion(Joueur);
	if (!R && Joueur)
	{
		R = RelaisDe(Joueur->GetController());
	}
	if (R && Nombre != 0)
	{
		R->Kits = FMath::Clamp(R->Kits + Nombre, 0, KitsMax);
		if (Nombre > 0)
		{
			R->ClientAvis(FString::Printf(TEXT("+%d kit de survie (tu en as %d)."), Nombre, R->Kits));
		}
	}
}

bool APvPChelles::EstATerre(APawn* Joueur) const
{
	const APvPRelais* R = RelaisDuPion(Joueur);
	return R && R->Etat == EEtatJoueurPvP::ATerre;
}

void APvPChelles::AvisATous(const FString& Texte)
{
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	for (APvPRelais* R : Tous)
	{
		R->ClientAvis(Texte);
	}
	if (Tous.Num() == 0)
	{
		MessageLocal(Texte);
	}
}

// ===================================================================================
//  Console (touche ²)
// ===================================================================================

void APvPChelles::CommandeEtat()
{
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	UE_LOG(LogTemp, Display, TEXT("[PvP] Actif=%d Manche=%d n°%d, %d joueur(s), %d coups avant a terre."), bActif ? 1 : 0, (int32)Manche, NumeroManche, Tous.Num(), CoupsMax);
	for (APvPRelais* R : Tous)
	{
		UE_LOG(LogTemp, Display, TEXT("[PvP]   %s : etat %d, coups %d, kits %d, elim %d, a terre %d, reanim %d"),
			*R->Nom, (int32)R->Etat, R->Coups, R->Kits, R->Eliminations, R->MisesATerre, R->Reanimations);
	}
	MessageLocal(FString::Printf(TEXT("PvP %s — manche %d — %d joueur(s)"), bActif ? TEXT("ouvert") : TEXT("coupé"), NumeroManche, Tous.Num()));
}

void APvPChelles::CommandeKits(int32 Nombre)
{
	if (!EstServeur())
	{
		return;
	}
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	for (APvPRelais* R : Tous)
	{
		R->Kits = FMath::Clamp(Nombre, 0, 99);
	}
	AvisATous(FString::Printf(TEXT("Chaque joueur a maintenant %d kit(s) de survie."), FMath::Clamp(Nombre, 0, 99)));
}

void APvPChelles::CommandeMoiATerre()
{
	if (!EstServeur())
	{
		return;
	}
	if (APvPRelais* R = MonRelais())
	{
		if (R->Etat == EEtatJoueurPvP::Debout && IsValid(R->Pion))
		{
			R->Coups = CoupsMax;
			MettreATerre(R);
		}
	}
}

// ===================================================================================
//  Chaque machine : ecran et touche du kit
// ===================================================================================

void APvPChelles::MessageLocal(const FString& Texte)
{
	Message = Texte;
	AgeMessage = 0.f;
}

void APvPChelles::CreerEcran()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (Ecran.IsValid() || !Vue)
	{
		return;
	}
	TWeakObjectPtr<APvPChelles> Moi(this);
	const FSlateFontInfo Petite = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	const FSlateFontInfo Grande = FCoreStyle::GetDefaultFontStyle("Bold", 34);
	const FLinearColor Ombre(0.f, 0.f, 0.f, 0.9f);

	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		// Bandeau du haut : manche, temps, coups, kits
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.f, 70.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Font(Petite)
			.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f))
			.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(Ombre)
			.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteBandeau() : FText::GetEmpty(); })
			.Visibility_Lambda([Moi]() { return (Moi.IsValid() && Moi->MontrerBandeau()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		]
		// Centre : A TERRE / ELIMINE / scores
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.f, 0.f, 0.f, 120.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(Grande)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FLinearColor(1.f, 0.2f, 0.15f))
				.ShadowOffset(FVector2D(3.f, 3.f)).ShadowColorAndOpacity(Ombre)
				.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteCentre() : FText::GetEmpty(); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 12.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(Petite)
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f))
				.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(Ombre)
				.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteScores() : FText::GetEmpty(); })
			]
		]
		// Bas : invite (jeter un kit) + messages
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 190.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(Petite)
				.ColorAndOpacity(FLinearColor(0.35f, 1.f, 0.45f))
				.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(Ombre)
				.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteInvite() : FText::GetEmpty(); })
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 6.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Font(Petite)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f))
				.ShadowOffset(FVector2D(2.f, 2.f)).ShadowColorAndOpacity(Ombre)
				.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteMessage() : FText::GetEmpty(); })
			]
		];
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 6);
}

void APvPChelles::RetirerEcran()
{
	if (Ecran.IsValid())
	{
		if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
		Ecran.Reset();
	}
}

bool APvPChelles::MontrerBandeau() const
{
	return bActif && Manche != EEtatManchePvP::Attente;
}

FText APvPChelles::TexteBandeau() const
{
	const APvPRelais* R = MonRelais();
	FString Temps;
	if (Manche == EEtatManchePvP::Scores)
	{
		Temps = FString::Printf(TEXT("prochaine manche dans %s"), *Chrono(FinManche - Maintenant()));
	}
	else
	{
		Temps = FinManche > 0.f ? Chrono(FinManche - Maintenant()) : FString(TEXT("sans limite"));
	}
	FString Texte = FString::Printf(TEXT("PvP  ·  MANCHE %d  ·  %s"), NumeroManche, *Temps);
	if (R)
	{
		Texte += FString::Printf(TEXT("  ·  COUPS %d/%d  ·  KITS DE SURVIE %d (%s)"), R->Coups, CoupsMax, R->Kits, *ToucheKit.GetDisplayName().ToString());
	}
	return FText::FromString(Texte);
}

FText APvPChelles::TexteCentre() const
{
	const APvPRelais* R = MonRelais();
	if (!bActif || !R)
	{
		return FText::GetEmpty();
	}
	if (R->Etat == EEtatJoueurPvP::ATerre)
	{
		return FText::FromString(FString::Printf(TEXT("À TERRE !  %s"), *Chrono(R->FinATerre - Maintenant())));
	}
	if (R->Etat == EEtatJoueurPvP::Mort && Manche != EEtatManchePvP::Attente)
	{
		return FText::FromString(TEXT("ÉLIMINÉ"));
	}
	if (Manche == EEtatManchePvP::Scores)
	{
		return FText::FromString(TitreScores);
	}
	return FText::GetEmpty();
}

FText APvPChelles::TexteScores() const
{
	const APvPRelais* Moi = MonRelais();
	if (Moi && bActif && Moi->Etat == EEtatJoueurPvP::ATerre)
	{
		return FText::FromString(TEXT("Un ami peut te jeter un kit de survie pour te réanimer."));
	}
	if (!bActif || Manche != EEtatManchePvP::Scores)
	{
		return FText::GetEmpty();
	}
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	Tous.Sort([](const APvPRelais& A, const APvPRelais& B)
	{
		return A.Eliminations != B.Eliminations ? A.Eliminations > B.Eliminations : A.MisesATerre > B.MisesATerre;
	});
	FString Texte;
	for (const APvPRelais* R : Tous)
	{
		Texte += FString::Printf(TEXT("%s  —  %d élimination(s) · %d mise(s) à terre · %d réanimation(s)\n"),
			*R->Nom, R->Eliminations, R->MisesATerre, R->Reanimations);
	}
	return FText::FromString(Texte.TrimEnd());
}

FText APvPChelles::TexteInvite() const
{
	return FText::FromString(InviteLocale);
}

FText APvPChelles::TexteMessage() const
{
	return AgeMessage < 5.f ? FText::FromString(Message) : FText::GetEmpty();
}

APvPRelais* APvPChelles::AmiATerreProche(APawn* Pion, float& Distance) const
{
	Distance = 0.f;
	if (!Pion)
	{
		return nullptr;
	}
	TArray<APvPRelais*> Tous;
	Relais(Tous);
	APvPRelais* Meilleur = nullptr;
	float MeilleureDistance = 6000.f; // 60 m
	for (APvPRelais* R : Tous)
	{
		if (R->Etat != EEtatJoueurPvP::ATerre || !IsValid(R->Pion) || R->Pion == Pion)
		{
			continue;
		}
		const float D = FVector::Dist(R->Pion->GetActorLocation(), Pion->GetActorLocation());
		if (D < MeilleureDistance)
		{
			MeilleureDistance = D;
			Meilleur = R;
		}
	}
	Distance = MeilleureDistance;
	return Meilleur;
}

void APvPChelles::AppliquerEtatLocal(APvPRelais* R)
{
	APlayerController* PC = R ? R->GetPC() : nullptr;
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P)
	{
		return;
	}
	if (DernierPionLocal.Get() != P)
	{
		DernierPionLocal = P;
		DernierEtatLocal = EEtatJoueurPvP::Debout; // un nouveau personnage a ses commandes
	}
	const EEtatJoueurPvP Voulu = bActif ? R->Etat : EEtatJoueurPvP::Debout;
	if (Voulu == DernierEtatLocal)
	{
		return;
	}
	DernierEtatLocal = Voulu;
	// A terre : on ne bouge plus et on ne tire plus (on peut toujours regarder autour de soi)
	if (Voulu == EEtatJoueurPvP::ATerre)
	{
		P->DisableInput(PC);
	}
	else if (Voulu == EEtatJoueurPvP::Debout)
	{
		P->EnableInput(PC);
	}
}

void APvPChelles::TickLocal(float DeltaSeconds)
{
	CreerEcran();
	AgeMessage += DeltaSeconds;
	InviteLocale.Reset();

	UWorld* Monde = GetWorld();
	APlayerController* PC = Monde ? Monde->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	APvPRelais* R = RelaisDe(PC);
	if (!R)
	{
		return;
	}
	AppliquerEtatLocal(R);
	if (!bActif || R->Etat != EEtatJoueurPvP::Debout)
	{
		return;
	}
	APawn* P = PC->GetPawn();
	float Distance = 0.f;
	if (APvPRelais* Ami = AmiATerreProche(P, Distance))
	{
		const FString Touche = ToucheKit.GetDisplayName().ToString();
		InviteLocale = R->Kits > 0
			? FString::Printf(TEXT("[%s] Jeter un kit de survie à %s (%.0f m) — vise-le, le kit part tout seul vers lui"), *Touche, *Ami->Nom, Distance / 100.f)
			: FString::Printf(TEXT("%s est à terre à %.0f m — tu n'as plus de kit de survie"), *Ami->Nom, Distance / 100.f);
	}

	// Touche du kit (pas quand un menu est ouvert avec la souris)
	const float Heure = Maintenant();
	if (!PC->bShowMouseCursor && PC->WasInputKeyJustPressed(ToucheKit) && Heure >= ProchaineDemandeKit)
	{
		ProchaineDemandeKit = Heure + 0.3f;
		R->ServeurJeterKit();
	}
}
