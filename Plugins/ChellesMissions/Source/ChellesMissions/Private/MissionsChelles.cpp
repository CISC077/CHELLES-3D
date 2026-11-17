// CHELLES 3D - MISSIONS DANS LE QUARTIER : gestionnaire (voir MissionsChelles.h)
//
// Aucun asset ni Blueprint du projet n'est modifie, aucun autre plugin non plus :
//  - ChellesZombies est pilote par le nom de ses variables et fonctions (VagueNombre, VagueCentre...,
//    LancerVagueCommandee, TerminerVagueCommandee, bInvasionsAutoEnPause, OnZombieTue) ;
//  - le TRAMWAY (ChellesTrain) est seulement LU (Trajet, ParcouruServeur, Etat, ArretCible, ArretsInfos) ;
//  - le Dernier Refuge (ChellesSurvie) est seulement lu (Mode) : pas de mission pendant le Refuge ;
//  - l'XP est ajoutee a XPLocale de ChellesPouvoirs, comme le fait le Dernier Refuge.
#include "MissionsChelles.h"

#include "Algo/BinarySearch.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"

#include "MissionsOutils.h"
#include "SMissionsEcran.h"

using namespace MissionsOutils;

namespace
{
	FString DossierPlugin()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesMissions"));
	}

	float Nombre(const FString& Texte)
	{
		return FCString::Atof(*Texte.Replace(TEXT(","), TEXT(".")));
	}

	bool Oui(const FString& Texte)
	{
		return Texte.StartsWith(TEXT("O"), ESearchCase::IgnoreCase) || Texte.StartsWith(TEXT("Y"), ESearchCase::IgnoreCase)
			|| Texte == TEXT("1") || Texte.Equals(TEXT("vrai"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	bool LireVecteur(const FString& Texte, FVector& Resultat)
	{
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(","), true);
		if (Morceaux.Num() < 3)
		{
			return false;
		}
		Resultat = FVector(FCString::Atof(*Morceaux[0].TrimStartAndEnd()), FCString::Atof(*Morceaux[1].TrimStartAndEnd()), FCString::Atof(*Morceaux[2].TrimStartAndEnd()));
		return true;
	}

	FReglagesMissions LireReglages()
	{
		FReglagesMissions R;
		TArray<FString> Lignes;
		const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Missions.txt"));
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Log, TEXT("[Missions] %s absent : reglages par defaut."), *Chemin);
			return R;
		}
		for (FString Ligne : Lignes)
		{
			Ligne.TrimStartAndEndInline();
			FString Cle, Valeur;
			if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
			{
				continue;
			}
			Cle.TrimStartAndEndInline();
			Valeur.TrimStartAndEndInline();
			const float V = Nombre(Valeur);
			const int32 I = FMath::RoundToInt(V);
			const float M = V * 100.f; // metres -> cm

			if (Cle == TEXT("Actif")) { R.bActif = Oui(Valeur); }
			else if (Cle == TEXT("Touche")) { const FKey K(*Valeur); if (K.IsValid()) { R.Touche = K; } }
			else if (Cle == TEXT("TempsPourRejoindreSecondes")) { R.TempsPourRejoindre = FMath::Max(10.f, V); }
			else if (Cle == TEXT("AffichageFinSecondes")) { R.AffichageFin = FMath::Max(2.f, V); }
			else if (Cle == TEXT("VieZombie")) { R.VieZombie = FMath::Max(1.f, V); }
			else if (Cle == TEXT("VitesseZombie")) { R.VitesseZombie = FMath::Max(50.f, V); }
			else if (Cle == TEXT("TousATerreSecondes")) { R.TousATerre = FMath::Max(1.f, V); }

			else if (Cle == TEXT("TramArrets")) { R.TramArrets = FMath::Max(1, I); }
			else if (Cle == TEXT("TramVie")) { R.TramVie = FMath::Max(1.f, V); }
			else if (Cle == TEXT("TramRayonDegatsMetres")) { R.TramRayonDegats = FMath::Max(100.f, M); }
			else if (Cle == TEXT("TramDegatsParZombieSeconde")) { R.TramDegatsParZombie = FMath::Max(0.f, V); }
			else if (Cle == TEXT("TramLongueurMetres")) { R.TramLongueur = FMath::Max(100.f, M); }
			else if (Cle == TEXT("TramZombiesParArret")) { R.TramZombies = FMath::Clamp(I, 1, 100); }
			else if (Cle == TEXT("TramZombiesEnPlusParArret")) { R.TramZombiesEnPlus = FMath::Clamp(I, 0, 50); }
			else if (Cle == TEXT("TramApparitionMinMetres")) { R.TramApparitionMin = FMath::Max(0.f, M); }
			else if (Cle == TEXT("TramApparitionMaxMetres")) { R.TramApparitionMax = FMath::Max(300.f, M); }
			else if (Cle == TEXT("TramRejoindreMetres")) { R.TramRejoindre = FMath::Max(300.f, M); }
			else if (Cle == TEXT("TramDistanceMaxMetres")) { R.TramDistanceMax = FMath::Max(1000.f, M); }
			else if (Cle == TEXT("TramAbandonSecondes")) { R.TramAbandon = FMath::Max(3.f, V); }
			else if (Cle == TEXT("TramXP")) { R.TramXP = FMath::Max(0, I); }

			else if (Cle == TEXT("MairieDureeSecondes")) { R.MairieDuree = FMath::Max(20.f, V); }
			else if (Cle == TEXT("MairieVie")) { R.MairieVie = FMath::Max(1.f, V); }
			else if (Cle == TEXT("MairieRayonDegatsMetres")) { R.MairieRayonDegats = FMath::Max(100.f, M); }
			else if (Cle == TEXT("MairieDegatsParZombieSeconde")) { R.MairieDegatsParZombie = FMath::Max(0.f, V); }
			else if (Cle == TEXT("MairieZombiesPremiereVague")) { R.MairieZombies = FMath::Clamp(I, 1, 100); }
			else if (Cle == TEXT("MairieZombiesEnPlusParVague")) { R.MairieZombiesEnPlus = FMath::Clamp(I, 0, 50); }
			else if (Cle == TEXT("MairieApparitionMinMetres")) { R.MairieApparitionMin = FMath::Max(0.f, M); }
			else if (Cle == TEXT("MairieApparitionMaxMetres")) { R.MairieApparitionMax = FMath::Max(300.f, M); }
			else if (Cle == TEXT("MairieRejoindreMetres")) { R.MairieRejoindre = FMath::Max(300.f, M); }
			else if (Cle == TEXT("MairieXP")) { R.MairieXP = FMath::Max(0, I); }

			else if (Cle == TEXT("ChasseObjectif")) { R.ChasseObjectif = FMath::Max(1, I); }
			else if (Cle == TEXT("ChasseDureeSecondes")) { R.ChasseDuree = FMath::Max(20.f, V); }
			else if (Cle == TEXT("ChasseZombiesParVague")) { R.ChasseZombies = FMath::Clamp(I, 1, 100); }
			else if (Cle == TEXT("ChasseApparitionMinMetres")) { R.ChasseApparitionMin = FMath::Max(0.f, M); }
			else if (Cle == TEXT("ChasseApparitionMaxMetres")) { R.ChasseApparitionMax = FMath::Max(300.f, M); }
			else if (Cle == TEXT("ChasseXP")) { R.ChasseXP = FMath::Max(0, I); }
		}
		R.TramApparitionMax = FMath::Max(R.TramApparitionMax, R.TramApparitionMin + 300.f);
		R.MairieApparitionMax = FMath::Max(R.MairieApparitionMax, R.MairieApparitionMin + 300.f);
		R.ChasseApparitionMax = FMath::Max(R.ChasseApparitionMax, R.ChasseApparitionMin + 300.f);
		UE_LOG(LogTemp, Log, TEXT("[Missions] Reglages lus (%s). Touche %s."), R.bActif ? TEXT("actif") : TEXT("Actif=Non"), *R.Touche.ToString());
		return R;
	}

	const TCHAR* NomMission(uint8 Mission)
	{
		switch (Mission)
		{
		case EMissionChelles::Tramway: return TEXT("ESCORTE DU TRAMWAY");
		case EMissionChelles::Mairie: return TEXT("D\u00C9FENDRE LA MAIRIE");
		case EMissionChelles::Chasse: return TEXT("CHASSE AUX ZOMBIES");
		default: return TEXT("");
		}
	}
}

const FReglagesMissions& AMissionsChelles::Reglages()
{
	static const FReglagesMissions R = LireReglages();
	return R;
}

AMissionsChelles::AMissionsChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 10.f;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AMissionsChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMissionsChelles, Mission);
	DOREPLIFETIME(AMissionsChelles, Phase);
	DOREPLIFETIME(AMissionsChelles, Secondes);
	DOREPLIFETIME(AMissionsChelles, Vie);
	DOREPLIFETIME(AMissionsChelles, VieMax);
	DOREPLIFETIME(AMissionsChelles, Avancement);
	DOREPLIFETIME(AMissionsChelles, Objectif);
	DOREPLIFETIME(AMissionsChelles, ZombiesRestants);
	DOREPLIFETIME(AMissionsChelles, Cible);
	DOREPLIFETIME(AMissionsChelles, bCibleConnue);
	DOREPLIFETIME(AMissionsChelles, Detail);
	DOREPLIFETIME(AMissionsChelles, bAlerte);
	DOREPLIFETIME(AMissionsChelles, XPGagnee);
	DOREPLIFETIME(AMissionsChelles, bTramwayDispo);
	DOREPLIFETIME(AMissionsChelles, bMairieConnue);
	DOREPLIFETIME(AMissionsChelles, bZombiesDispo);
}

void AMissionsChelles::BeginPlay()
{
	Super::BeginPlay();
	Reglages();
	if (EstServeur())
	{
		ChargerPositions();
	}
}

void AMissionsChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (EstServeur() && bPauseAutoPosee)
	{
		if (AActor* Inv = Invasion())
		{
			EcrireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, bPauseAutoAvant);
		}
		bPauseAutoPosee = false;
	}
	FermerMenu();
	RetirerEcran();
	Super::EndPlay(Raison);
}

void AMissionsChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Reglages().bActif)
	{
		return;
	}
	if (EstServeur())
	{
		TickServeur(DeltaSeconds);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLocal(DeltaSeconds);
	}
}

// ===================================================================================
//  Positions (Missions_Positions.txt, sinon le centre du Dernier Refuge)
// ===================================================================================

void AMissionsChelles::ChargerPositions()
{
	bMairiePlacee = false;
	TArray<FString> Lignes;
	if (FFileHelper::LoadFileToStringArray(Lignes, *FPaths::Combine(DossierPlugin(), TEXT("Missions_Positions.txt"))))
	{
		for (const FString& Ligne : Lignes)
		{
			FString Cle, Valeur;
			if (!Ligne.StartsWith(TEXT("#")) && Ligne.Split(TEXT("="), &Cle, &Valeur) && Cle.TrimStartAndEnd() == TEXT("Mairie")
				&& LireVecteur(Valeur, CentreMairie))
			{
				bMairiePlacee = true;
				UE_LOG(LogTemp, Log, TEXT("[Missions] Mairie (Missions_Positions.txt) : %s"), *CentreMairie.ToString());
			}
		}
	}
	if (!bMairiePlacee)
	{
		// Le Dernier Refuge se joue deja au parc de la Mairie : on reprend son centre
		Lignes.Reset();
		const FString CheminRefuge = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesSurvie"), TEXT("Refuge_Positions.txt"));
		if (FFileHelper::LoadFileToStringArray(Lignes, *CheminRefuge))
		{
			for (const FString& Ligne : Lignes)
			{
				FString Cle, Valeur;
				if (!Ligne.StartsWith(TEXT("#")) && Ligne.Split(TEXT("="), &Cle, &Valeur) && Cle.TrimStartAndEnd() == TEXT("Centre")
					&& LireVecteur(Valeur, CentreMairie))
				{
					bMairiePlacee = true;
					UE_LOG(LogTemp, Log, TEXT("[Missions] Mairie = centre du Dernier Refuge : %s"), *CentreMairie.ToString());
				}
			}
		}
	}
	if (!bMairiePlacee)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Missions] Place de la Mairie inconnue : allez-y en jeu et tapez Chelles.MissionIci Mairie (console, touche \u00B2)."));
	}
	bMairieConnue = bMairiePlacee;
}

void AMissionsChelles::SauverPositions() const
{
	TArray<FString> Lignes;
	Lignes.Add(TEXT("# CHELLES 3D - Missions : places enregistrees EN JEU avec la console (touche \u00B2)."));
	Lignes.Add(TEXT("#   Chelles.MissionIci Mairie   = le centre de la Mairie a defendre = vos pieds"));
	Lignes.Add(TEXT("# Sans ligne Mairie=, le centre du Dernier Refuge (plugin ChellesSurvie) est utilise."));
	if (bMairiePlacee)
	{
		Lignes.Add(FString::Printf(TEXT("Mairie=%.0f,%.0f,%.0f"), CentreMairie.X, CentreMairie.Y, CentreMairie.Z));
	}
	const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Missions_Positions.txt"));
	if (!FFileHelper::SaveStringArrayToFile(Lignes, *Chemin, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("[Missions] Impossible d'ecrire %s"), *Chemin);
	}
}

void AMissionsChelles::CommandeMissionIci(const TArray<FString>& Arguments)
{
	UWorld* Monde = GetWorld();
	APlayerController* PC = Monde ? Monde->GetFirstPlayerController() : nullptr;
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!EstServeur())
	{
		MessageLocal(TEXT("Seul l'h\u00F4te de la partie peut placer les missions."));
		return;
	}
	if (!Pion)
	{
		return;
	}
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Mairie"));
	if (Quoi.Equals(TEXT("Mairie"), ESearchCase::IgnoreCase))
	{
		CentreMairie = Pion->GetActorLocation();
		bMairiePlacee = true;
		bMairieConnue = true;
		SauverPositions();
		MessageLocal(TEXT("Mairie enregistr\u00E9e ici (Missions_Positions.txt)."));
		UE_LOG(LogTemp, Log, TEXT("[Missions] Mairie placee : %s"), *CentreMairie.ToString());
	}
	else
	{
		MessageLocal(TEXT("Usage : Chelles.MissionIci Mairie"));
	}
}

// ===================================================================================
//  Les autres plugins (par leur nom)
// ===================================================================================

AActor* AMissionsChelles::Invasion() const
{
	return ActeurDeClasse(GetWorld(), TEXT("InvasionZombies"));
}

AActor* AMissionsChelles::Tramway() const
{
	return ActeurDeClasse(GetWorld(), TEXT("TrainChelles"));
}

void AMissionsChelles::BrancherInvasion()
{
	AActor* Inv = Invasion();
	if (!Inv || InvasionBranchee.Get() == Inv)
	{
		return;
	}
	FMulticastDelegateProperty* P = FindFProperty<FMulticastDelegateProperty>(Inv->GetClass(), TEXT("OnZombieTue"));
	if (P)
	{
		FScriptDelegate Delegue;
		Delegue.BindUFunction(this, TEXT("QuandZombieTue"));
		P->AddDelegate(Delegue, Inv);
		UE_LOG(LogTemp, Log, TEXT("[Missions] Branche sur ChellesZombies."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Missions] ChellesZombies trop ancien (OnZombieTue absent) : la chasse ne comptera pas les kills."));
	}
	InvasionBranchee = Inv;
}

bool AMissionsChelles::VagueEnCours() const
{
	bool bEnCours = false;
	if (AActor* Inv = Invasion())
	{
		LireBooleen(Inv, { TEXT("bInvasionEnCours") }, bEnCours);
	}
	return bEnCours;
}

bool AMissionsChelles::ModeSurvieActif() const
{
	AActor* Survie = ActeurDeClasse(GetWorld(), TEXT("SurvieChelles"));
	float Mode = 0.f;
	return Survie && LireNombre(Survie, { TEXT("Mode") }, Mode) && Mode != 0.f;
}

bool AMissionsChelles::LancerVague(int32 NombreZ, const FVector& Centre, float RayonMin, float RayonMax, const FString& MessageVague)
{
	AActor* Inv = Invasion();
	if (!Inv)
	{
		return false;
	}
	if (VagueEnCours())
	{
		if (!bNotreVague)
		{
			return false; // une invasion qui n'est pas la notre : on ne la coupe pas
		}
		Appeler(Inv, TEXT("TerminerVagueCommandee"));
	}
	const FReglagesMissions& R = Reglages();
	EcrireNombre(Inv, { TEXT("VagueNombre") }, (float)NombreZ);
	EcrireNombre(Inv, { TEXT("VagueVie") }, R.VieZombie);
	EcrireNombre(Inv, { TEXT("VagueVitesse") }, R.VitesseZombie);
	EcrireNombre(Inv, { TEXT("VaguePartCostauds") }, NumeroVague >= 2 ? 0.1f : 0.f);
	EcrireNombre(Inv, { TEXT("VagueVieCostaud") }, R.VieZombie * 3.f);
	EcrireVecteur(Inv, TEXT("VagueCentre"), Centre);
	EcrireNombre(Inv, { TEXT("VagueRayonMin") }, RayonMin);
	EcrireNombre(Inv, { TEXT("VagueRayonMax") }, RayonMax);
	EcrireTexte(Inv, TEXT("VagueMessage"), MessageVague);
	EcrireNombre(Inv, { TEXT("VagueDureeMessage") }, 4.f);
	EcrireNombre(Inv, { TEXT("VagueVolumeSirene") }, 0.35f);
	if (!Appeler(Inv, TEXT("LancerVagueCommandee")))
	{
		UE_LOG(LogTemp, Error, TEXT("[Missions] ChellesZombies trop ancien (LancerVagueCommandee absent) : recompilez ChellesZombies."));
		return false;
	}
	bNotreVague = VagueEnCours();
	if (bNotreVague)
	{
		++NumeroVague;
		UE_LOG(LogTemp, Log, TEXT("[Missions] Vague %d : %d zombies autour de %s."), NumeroVague, NombreZ, *Centre.ToString());
	}
	return bNotreVague;
}

void AMissionsChelles::TerminerNotreVague()
{
	if (bNotreVague && !ModeSurvieActif() && VagueEnCours())
	{
		if (AActor* Inv = Invasion())
		{
			Appeler(Inv, TEXT("TerminerVagueCommandee"));
		}
	}
	bNotreVague = false;
}

void AMissionsChelles::ZombiesDebout(TArray<AActor*>& Resultat) const
{
	AActor* Inv = Invasion();
	if (!Inv)
	{
		return;
	}
	TArray<AActor*> Tous;
	LireActeurs(Inv, TEXT("Zombies"), Tous);
	for (AActor* Z : Tous)
	{
		if (ZombieDebout(Z))
		{
			Resultat.Add(Z);
		}
	}
}

void AMissionsChelles::JoueursDebout(TArray<APawn*>& Resultat) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (!IsValid(Pion) || Pion->Tags.Contains(FName(TEXT("ChellesHorsJeu"))))
		{
			continue;
		}
		bool bMort = false;
		if (LireBooleen(Pion, { TEXT("IsDead"), TEXT("isDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
		{
			continue;
		}
		float VieJoueur = 1.f;
		if (LireNombre(Pion, { TEXT("HealthCurrent"), TEXT("Health") }, VieJoueur) && VieJoueur <= 0.f)
		{
			continue;
		}
		Resultat.Add(Pion);
	}
}

bool AMissionsChelles::JoueurPres(const FVector& Point, float Rayon) const
{
	TArray<APawn*> Joueurs;
	JoueursDebout(Joueurs);
	for (APawn* J : Joueurs)
	{
		if (FVector::Dist2D(J->GetActorLocation(), Point) <= Rayon)
		{
			return true;
		}
	}
	return false;
}

int32 AMissionsChelles::ZombiesPres(const TArray<FVector>& Points, float Rayon) const
{
	TArray<AActor*> Zombies;
	ZombiesDebout(Zombies);
	int32 N = 0;
	for (AActor* Z : Zombies)
	{
		const FVector P = Z->GetActorLocation();
		for (const FVector& Q : Points)
		{
			if (FVector::Dist2D(P, Q) <= Rayon && FMath::Abs(P.Z - Q.Z) < 500.f)
			{
				++N;
				break;
			}
		}
	}
	return N;
}

// ---- Le TRAMWAY (lu dans ATrainChelles) ----

FVector AMissionsChelles::PointSurTrajet(float S) const
{
	if (TrajetTram.Num() == 0)
	{
		return FVector::ZeroVector;
	}
	const float Total = CumulsTram.Num() > 0 ? CumulsTram.Last() : 0.f;
	if (TrajetTram.Num() < 2 || Total <= 0.f)
	{
		return TrajetTram[0];
	}
	S = FMath::Fmod(S, Total);
	if (S < 0.f)
	{
		S += Total;
	}
	const int32 i = FMath::Clamp(Algo::UpperBound(CumulsTram, S) - 1, 0, TrajetTram.Num() - 2);
	const float Longueur = CumulsTram[i + 1] - CumulsTram[i];
	const float Alpha = Longueur > 0.f ? FMath::Clamp((S - CumulsTram[i]) / Longueur, 0.f, 1.f) : 0.f;
	return FMath::Lerp(TrajetTram[i], TrajetTram[i + 1], Alpha);
}

bool AMissionsChelles::PositionTramway(TArray<FVector>& Points)
{
	AActor* Train = Tramway();
	if (!Train)
	{
		return false;
	}
	if (TrajetTram.Num() < 2)
	{
		TArray<FVector> Lu;
		if (!LirePoints(Train, TEXT("Trajet"), Lu) || Lu.Num() < 2)
		{
			return false;
		}
		TrajetTram = MoveTemp(Lu);
		CumulsTram.SetNum(TrajetTram.Num());
		CumulsTram[0] = 0.f;
		for (int32 i = 1; i < TrajetTram.Num(); ++i)
		{
			CumulsTram[i] = CumulsTram[i - 1] + FVector::Dist(TrajetTram[i - 1], TrajetTram[i]);
		}
	}
	float Tete = 0.f;
	LireNombre(Train, { TEXT("ParcouruServeur") }, Tete);
	const float L = Reglages().TramLongueur;
	Points.Reset();
	Points.Add(PointSurTrajet(Tete));
	Points.Add(PointSurTrajet(Tete - L * 0.5f));
	Points.Add(PointSurTrajet(Tete - L));
	return true;
}

FString AMissionsChelles::NomArretTramway(int32 Index) const
{
	AActor* Train = Tramway();
	FString Nom = Train ? LireTexteDansTableau(Train, TEXT("ArretsInfos"), Index, TEXT("Nom")) : FString();
	return Nom.IsEmpty() ? FString(TEXT("l'arr\u00EAt suivant")) : Nom;
}

// ===================================================================================
//  Debut et fin d'une mission (serveur)
// ===================================================================================

FString AMissionsChelles::Demarrer(uint8 Quelle)
{
	const FReglagesMissions& R = Reglages();
	UWorld* Monde = GetWorld();
	if (!EstServeur() || !Monde)
	{
		return TEXT("Seul l'h\u00F4te de la partie lance les missions.");
	}
	if (!R.bActif)
	{
		return TEXT("Missions d\u00E9sactiv\u00E9es (Actif=Non dans Missions.txt).");
	}
	if (Phase == EPhaseMission::Rejoindre || Phase == EPhaseMission::EnCours)
	{
		return TEXT("Une mission est d\u00E9j\u00E0 en cours.");
	}
	if (Quelle < EMissionChelles::Tramway || Quelle > EMissionChelles::Chasse)
	{
		return TEXT("Mission inconnue (1 = tramway, 2 = Mairie, 3 = chasse).");
	}
	if (ModeSurvieActif())
	{
		return TEXT("Arr\u00EAtez d'abord le Dernier Refuge (F7 > JEU LIBRE).");
	}
	AActor* Inv = Invasion();
	if (!Inv)
	{
		return TEXT("Les zombies (plugin ChellesZombies) ne sont pas dans la partie.");
	}
	if (VagueEnCours())
	{
		return TEXT("Une invasion zombie est en cours : attendez qu'elle se termine.");
	}
	BrancherInvasion();

	TArray<FVector> PointsTram;
	if (Quelle == EMissionChelles::Tramway)
	{
		TrajetTram.Reset();
		if (!PositionTramway(PointsTram))
		{
			return TEXT("Le TRAMWAY n'est pas sur la carte (plugin ChellesTrain, ou trajet pas encore pr\u00EAt).");
		}
	}
	if (Quelle == EMissionChelles::Mairie)
	{
		ChargerPositions();
		if (!bMairiePlacee)
		{
			return TEXT("Place de la Mairie inconnue : allez-y et tapez Chelles.MissionIci Mairie (console \u00B2).");
		}
	}

	const float Heure = Monde->GetTimeSeconds();
	Mission = Quelle;
	Avancement = 0;
	Objectif = 0;
	ZombiesRestants = 0;
	NumeroVague = 0;
	bNotreVague = false;
	HorsDePortee = 0.f;
	SansJoueur = 0.f;
	EtatTramAvant = -1;
	bAlerte = false;
	Detail.Reset();
	bCibleConnue = false;

	// Pas d'invasion automatique pendant la mission (remise comme avant a la fin)
	if (!bPauseAutoPosee)
	{
		LireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, bPauseAutoAvant);
		bPauseAutoPosee = true;
	}
	EcrireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, true);

	switch (Quelle)
	{
	case EMissionChelles::Tramway:
		Phase = EPhaseMission::Rejoindre;
		FinPhase = Heure + R.TempsPourRejoindre;
		VieMax = Vie = R.TramVie;
		Objectif = R.TramArrets;
		XPGagnee = R.TramXP;
		Cible = PointsTram[1];
		bCibleConnue = true;
		break;
	case EMissionChelles::Mairie:
		Phase = EPhaseMission::Rejoindre;
		FinPhase = Heure + R.TempsPourRejoindre;
		VieMax = Vie = R.MairieVie;
		XPGagnee = R.MairieXP;
		Cible = CentreMairie;
		bCibleConnue = true;
		break;
	default: // Chasse : tout de suite, la ou on est
		Phase = EPhaseMission::EnCours;
		FinPhase = Heure + R.ChasseDuree;
		VieMax = Vie = 0.f;
		Objectif = R.ChasseObjectif;
		XPGagnee = R.ChasseXP;
		ProchaineVague = Heure + 1.f;
		break;
	}
	Secondes = FinPhase - Heure;
	UE_LOG(LogTemp, Log, TEXT("[Missions] Mission %s lancee."), NomMission(Quelle));
	return FString();
}

void AMissionsChelles::Abandonner()
{
	if (EstServeur() && (Phase == EPhaseMission::Rejoindre || Phase == EPhaseMission::EnCours))
	{
		Finir(false, TEXT("Mission abandonn\u00E9e"));
	}
}

void AMissionsChelles::Finir(bool bReussie, const FString& Raison)
{
	TerminerNotreVague();
	if (bPauseAutoPosee)
	{
		// Si le Dernier Refuge a ete lance entre-temps, c'est lui qui gere la pause : on n'y touche pas
		if (!ModeSurvieActif())
		{
			if (AActor* Inv = Invasion())
			{
				EcrireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, bPauseAutoAvant);
			}
		}
		bPauseAutoPosee = false;
	}
	Phase = bReussie ? EPhaseMission::Reussie : EPhaseMission::Ratee;
	Detail = Raison;
	bAlerte = false;
	bCibleConnue = false;
	ZombiesRestants = 0;
	Secondes = 0.f;
	if (!bReussie)
	{
		XPGagnee = 0;
	}
	FinPhase = GetWorld()->GetTimeSeconds() + Reglages().AffichageFin;
	UE_LOG(LogTemp, Log, TEXT("[Missions] %s : %s (%s)."), NomMission(Mission), bReussie ? TEXT("REUSSIE") : TEXT("RATEE"), *Raison);
}

void AMissionsChelles::Nettoyer()
{
	Mission = EMissionChelles::Aucune;
	Phase = EPhaseMission::Aucune;
	Detail.Reset();
	Avancement = 0;
	Objectif = 0;
	Vie = VieMax = 0.f;
	XPGagnee = 0;
}

void AMissionsChelles::QuandZombieTue(ACharacter* Zombie, AController* Tueur, bool bTete)
{
	if (Mission == EMissionChelles::Chasse && Phase == EPhaseMission::EnCours && Tueur && Tueur->IsPlayerController())
	{
		++Avancement;
	}
}

// ===================================================================================
//  Serveur : chaque image
// ===================================================================================

void AMissionsChelles::TickServeur(float DeltaSeconds)
{
	UWorld* Monde = GetWorld();
	const float Heure = Monde->GetTimeSeconds();

	// Une fois par seconde : ce qui est disponible pour l'ecran MISSIONS
	if (Heure >= ProchainControle)
	{
		ProchainControle = Heure + 1.f;
		BrancherInvasion();
		bZombiesDispo = Invasion() != nullptr;
		if (Phase != EPhaseMission::EnCours || Mission != EMissionChelles::Tramway)
		{
			TrajetTram.Reset(); // relu au besoin (le trajet est construit quelques secondes apres le debut)
			TArray<FVector> Points;
			bTramwayDispo = PositionTramway(Points);
		}
	}

	if (Phase == EPhaseMission::Reussie || Phase == EPhaseMission::Ratee)
	{
		if (Heure >= FinPhase)
		{
			Nettoyer();
		}
		return;
	}
	if (Phase == EPhaseMission::Aucune)
	{
		return;
	}

	if (ModeSurvieActif())
	{
		bNotreVague = false; // les zombies sont maintenant ceux du Dernier Refuge
		Finir(false, TEXT("Le Dernier Refuge a \u00E9t\u00E9 lanc\u00E9"));
		return;
	}

	// Tout le monde a terre / mort trop longtemps
	TArray<APawn*> Joueurs;
	JoueursDebout(Joueurs);
	SansJoueur = Joueurs.Num() == 0 ? SansJoueur + DeltaSeconds : 0.f;
	if (SansJoueur >= Reglages().TousATerre)
	{
		Finir(false, TEXT("Tous les joueurs sont tomb\u00E9s"));
		return;
	}

	// Zombies restants de notre vague
	if (bNotreVague)
	{
		float Restants = 0.f;
		if (AActor* Inv = Invasion())
		{
			LireNombre(Inv, { TEXT("ZombiesRestants") }, Restants);
		}
		ZombiesRestants = VagueEnCours() ? (int32)Restants : 0;
	}
	else
	{
		ZombiesRestants = 0;
	}

	switch (Mission)
	{
	case EMissionChelles::Tramway: TickTramway(DeltaSeconds, Heure); break;
	case EMissionChelles::Mairie: TickMairie(DeltaSeconds, Heure); break;
	case EMissionChelles::Chasse: TickChasse(DeltaSeconds, Heure); break;
	default: Nettoyer(); break;
	}
}

void AMissionsChelles::TickTramway(float DeltaSeconds, float Heure)
{
	const FReglagesMissions& R = Reglages();
	TArray<FVector> Points;
	if (!PositionTramway(Points))
	{
		Finir(false, TEXT("Le TRAMWAY a disparu"));
		return;
	}
	const FVector Milieu = Points[1];
	Cible = Milieu;
	bCibleConnue = true;

	AActor* Train = Tramway();
	float EtatF = 0.f, ArretF = 0.f;
	LireNombre(Train, { TEXT("Etat") }, EtatF);
	LireNombre(Train, { TEXT("ArretCible") }, ArretF);
	const int32 EtatTram = FMath::RoundToInt(EtatF); // 0 roule, 1 a l'arret portes ouvertes, 2 fermeture
	const int32 Arret = FMath::RoundToInt(ArretF);

	if (Phase == EPhaseMission::Rejoindre)
	{
		Secondes = FMath::Max(0.f, FinPhase - Heure);
		Detail = TEXT("Rejoignez le TRAMWAY (rep\u00E8re dor\u00E9)");
		if (JoueurPres(Milieu, R.TramRejoindre))
		{
			Phase = EPhaseMission::EnCours;
			Secondes = 0.f;
			EtatTramAvant = EtatTram;
			HorsDePortee = 0.f;
			LancerVague(R.TramZombies, Milieu, R.TramApparitionMin, R.TramApparitionMax, TEXT("LE TRAMWAY EST ATTAQU\u00C9 !"));
		}
		else if (Secondes <= 0.f)
		{
			Finir(false, TEXT("Vous n'avez pas rejoint le TRAMWAY \u00E0 temps"));
		}
		return;
	}

	// ---- Arrivee a un arret : un arret de plus, et une nouvelle attaque ----
	if (EtatTram == 1 && EtatTramAvant != 1 && EtatTramAvant != -1)
	{
		++Avancement;
		const FString NomArret = NomArretTramway(Arret);
		if (Avancement >= Objectif)
		{
			Finir(true, FString::Printf(TEXT("Le TRAMWAY est arriv\u00E9 \u00E0 %s"), *NomArret));
			return;
		}
		LancerVague(R.TramZombies + R.TramZombiesEnPlus * Avancement, Milieu, R.TramApparitionMin, R.TramApparitionMax, TEXT("LE TRAMWAY EST ATTAQU\u00C9 !"));
	}
	EtatTramAvant = EtatTram;

	// ---- Degats : les zombies colles au tramway ----
	const int32 Pres = ZombiesPres(Points, R.TramRayonDegats);
	Vie = FMath::Max(0.f, Vie - Pres * R.TramDegatsParZombie * DeltaSeconds);
	if (Vie <= 0.f)
	{
		Finir(false, TEXT("Le TRAMWAY a \u00E9t\u00E9 d\u00E9truit par les zombies"));
		return;
	}

	// ---- Personne pres du tramway ----
	if (!JoueurPres(Milieu, R.TramDistanceMax))
	{
		HorsDePortee += DeltaSeconds;
		bAlerte = true;
		Detail = FString::Printf(TEXT("Revenez pr\u00E8s du TRAMWAY ! %d s"), FMath::Max(0, FMath::CeilToInt(R.TramAbandon - HorsDePortee)));
		if (HorsDePortee >= R.TramAbandon)
		{
			Finir(false, TEXT("Vous avez abandonn\u00E9 le TRAMWAY"));
		}
		return;
	}
	HorsDePortee = 0.f;
	bAlerte = Pres > 0;
	if (Pres > 0)
	{
		Detail = FString::Printf(TEXT("%d zombie%s attaque%s le TRAMWAY !"), Pres, Pres > 1 ? TEXT("s") : TEXT(""), Pres > 1 ? TEXT("nt") : TEXT(""));
	}
	else if (EtatTram == 0)
	{
		Detail = FString::Printf(TEXT("Prochain arr\u00EAt : %s"), *NomArretTramway(Arret));
	}
	else
	{
		Detail = FString::Printf(TEXT("Arr\u00EAt %s : prot\u00E9gez les portes !"), *NomArretTramway(Arret));
	}
}

void AMissionsChelles::TickMairie(float DeltaSeconds, float Heure)
{
	const FReglagesMissions& R = Reglages();
	Cible = CentreMairie;
	bCibleConnue = true;

	if (Phase == EPhaseMission::Rejoindre)
	{
		Secondes = FMath::Max(0.f, FinPhase - Heure);
		Detail = TEXT("Rendez-vous \u00E0 la Mairie (rep\u00E8re dor\u00E9)");
		if (JoueurPres(CentreMairie, R.MairieRejoindre))
		{
			Phase = EPhaseMission::EnCours;
			FinPhase = Heure + R.MairieDuree;
			ProchaineVague = Heure + 2.f;
		}
		else if (Secondes <= 0.f)
		{
			Finir(false, TEXT("Vous n'\u00EAtes pas arriv\u00E9s \u00E0 la Mairie \u00E0 temps"));
		}
		return;
	}

	Secondes = FMath::Max(0.f, FinPhase - Heure);
	Avancement = NumeroVague;

	// Vagues l'une apres l'autre, de plus en plus grosses, avec 4 s de repit
	const bool bEnCours = VagueEnCours();
	if (bNotreVague && !bEnCours)
	{
		bNotreVague = false;
		ProchaineVague = Heure + 4.f;
	}
	if (!bNotreVague && !bEnCours && Heure >= ProchaineVague && Secondes > 8.f)
	{
		if (!LancerVague(R.MairieZombies + R.MairieZombiesEnPlus * NumeroVague, CentreMairie, R.MairieApparitionMin, R.MairieApparitionMax,
			NumeroVague == 0 ? FString(TEXT("LA MAIRIE EST ATTAQU\u00C9E !")) : FString()))
		{
			ProchaineVague = Heure + 2.f;
		}
	}

	TArray<FVector> Centre;
	Centre.Add(CentreMairie);
	const int32 Pres = ZombiesPres(Centre, R.MairieRayonDegats);
	Vie = FMath::Max(0.f, Vie - Pres * R.MairieDegatsParZombie * DeltaSeconds);
	if (Vie <= 0.f)
	{
		Finir(false, TEXT("La Mairie est tomb\u00E9e aux mains des zombies"));
		return;
	}
	if (Secondes <= 0.f)
	{
		Finir(true, TEXT("La Mairie a tenu !"));
		return;
	}
	bAlerte = Pres > 0;
	if (Pres > 0)
	{
		Detail = FString::Printf(TEXT("%d zombie%s dans la Mairie !"), Pres, Pres > 1 ? TEXT("s") : TEXT(""));
	}
	else if (!bNotreVague)
	{
		Detail = TEXT("Rechargez : une vague arrive...");
	}
	else
	{
		Detail = FString::Printf(TEXT("Vague %d \u00B7 tenez jusqu'\u00E0 la fin du chrono"), NumeroVague);
	}
}

void AMissionsChelles::TickChasse(float DeltaSeconds, float Heure)
{
	const FReglagesMissions& R = Reglages();
	Secondes = FMath::Max(0.f, FinPhase - Heure);
	bCibleConnue = false;

	if (Avancement >= Objectif)
	{
		Finir(true, FString::Printf(TEXT("%d zombies \u00E9limin\u00E9s"), Avancement));
		return;
	}
	if (Secondes <= 0.f)
	{
		Finir(false, FString::Printf(TEXT("Temps \u00E9coul\u00E9 : %d / %d zombies"), Avancement, Objectif));
		return;
	}

	const bool bEnCours = VagueEnCours();
	if (bNotreVague && !bEnCours)
	{
		bNotreVague = false;
		ProchaineVague = Heure + 2.f;
	}
	if (!bNotreVague && !bEnCours && Heure >= ProchaineVague)
	{
		TArray<APawn*> Joueurs;
		JoueursDebout(Joueurs);
		if (Joueurs.Num() > 0)
		{
			const FVector Centre = Joueurs[FMath::RandRange(0, Joueurs.Num() - 1)]->GetActorLocation();
			if (!LancerVague(R.ChasseZombies, Centre, R.ChasseApparitionMin, R.ChasseApparitionMax,
				NumeroVague == 0 ? FString(TEXT("CHASSE AUX ZOMBIES !")) : FString()))
			{
				ProchaineVague = Heure + 2.f;
			}
		}
	}
	bAlerte = false;
	Detail = bNotreVague ? FString(TEXT("Ils sortent de terre autour de vous !")) : FString(TEXT("D'autres zombies arrivent..."));
}

// ===================================================================================
//  Chaque machine : ecran, touche N, balise, XP
// ===================================================================================

void AMissionsChelles::MessageLocal(const FString& Texte)
{
	Message = Texte;
	AgeMessage = 0.f;
	UE_LOG(LogTemp, Log, TEXT("[Missions] %s"), *Texte);
}

void AMissionsChelles::CreerEcran()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (Ecran.IsValid() || !Vue)
	{
		return;
	}
	Ecran = SNew(SMissionsEcran);
	// Couche 6 : au-dessus du HUD (3) et du Dernier Refuge (5), sous les menus
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 6);
}

void AMissionsChelles::RetirerEcran()
{
	if (Ecran.IsValid())
	{
		if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
		Ecran.Reset();
	}
	if (Balise)
	{
		Balise->DestroyComponent();
		Balise = nullptr;
	}
	if (LumiereBalise)
	{
		LumiereBalise->DestroyComponent();
		LumiereBalise = nullptr;
	}
}

void AMissionsChelles::OuvrirMenu()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (Menu.IsValid() || !Vue || !PC)
	{
		return;
	}
	const FReglagesMissions& R = Reglages();
	const FString SansZombies = TEXT("Les zombies (plugin ChellesZombies) ne sont pas dans la partie.");

	TArray<FChoixMission> Choix;
	{
		FChoixMission C;
		C.Numero = EMissionChelles::Tramway;
		C.Titre = TEXT("ESCORTE DU TRAMWAY");
		C.Description = FString::Printf(TEXT("Le TRAMWAY est attaqu\u00E9 \u00E0 chaque arr\u00EAt. Restez pr\u00E8s de lui et gardez-le en vie pendant %d arr\u00EAts."), R.TramArrets);
		C.Recompense = FString::Printf(TEXT("+%d XP"), R.TramXP);
		C.bDisponible = bTramwayDispo && bZombiesDispo;
		C.PourquoiPas = !bZombiesDispo ? SansZombies : FString(TEXT("Le TRAMWAY n'est pas encore sur la carte (plugin ChellesTrain)."));
		Choix.Add(C);
	}
	{
		FChoixMission C;
		C.Numero = EMissionChelles::Mairie;
		C.Titre = TEXT("D\u00C9FENDRE LA MAIRIE");
		C.Description = FString::Printf(TEXT("Tenez la Mairie %d min contre des vagues de zombies. S'ils entrent, elle perd de la vie."), FMath::Max(1, FMath::RoundToInt(R.MairieDuree / 60.f)));
		C.Recompense = FString::Printf(TEXT("+%d XP"), R.MairieXP);
		C.bDisponible = bMairieConnue && bZombiesDispo;
		C.PourquoiPas = !bZombiesDispo ? SansZombies : FString(TEXT("Place de la Mairie inconnue : allez-y et tapez Chelles.MissionIci Mairie (console \u00B2)."));
		Choix.Add(C);
	}
	{
		FChoixMission C;
		C.Numero = EMissionChelles::Chasse;
		C.Titre = TEXT("CHASSE AUX ZOMBIES");
		C.Description = FString::Printf(TEXT("Tuez %d zombies en %d min, l\u00E0 o\u00F9 vous \u00EAtes."), R.ChasseObjectif, FMath::Max(1, FMath::RoundToInt(R.ChasseDuree / 60.f)));
		C.Recompense = FString::Printf(TEXT("+%d XP"), R.ChasseXP);
		C.bDisponible = bZombiesDispo;
		C.PourquoiPas = SansZombies;
		Choix.Add(C);
	}

	TWeakObjectPtr<AMissionsChelles> Moi(this);
	Menu = SNew(SMissionsMenu)
		.bPeutChoisir(GetNetMode() != NM_Client)
		.bMissionEnCours(Phase == EPhaseMission::Rejoindre || Phase == EPhaseMission::EnCours)
		.NomTouche(R.Touche.GetDisplayName().ToString())
		.Choix(Choix)
		.QuandChoisie_Lambda([Moi](uint8 Quelle)
		{
			if (Moi.IsValid())
			{
				Moi->ChoisirDansMenu(Quelle);
			}
		})
		.QuandFerme_Lambda([Moi]()
		{
			if (Moi.IsValid())
			{
				Moi->FermerMenu();
			}
		});
	Menu->ToucheFermer = R.Touche;
	Vue->AddViewportWidgetContent(Menu.ToSharedRef(), 900);

	FInputModeUIOnly Entree;
	Entree.SetWidgetToFocus(Menu);
	Entree.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Entree);
	PC->bShowMouseCursor = true;
}

void AMissionsChelles::FermerMenu()
{
	if (!Menu.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void AMissionsChelles::ChoisirDansMenu(uint8 Quelle)
{
	FermerMenu();
	if (!EstServeur())
	{
		MessageLocal(TEXT("Seul l'h\u00F4te de la partie lance les missions."));
		return;
	}
	if (Quelle == 0)
	{
		Abandonner();
		return;
	}
	const FString Erreur = Demarrer(Quelle);
	if (!Erreur.IsEmpty())
	{
		MessageLocal(Erreur);
	}
}

void AMissionsChelles::MettreAJourBalise(float DeltaSeconds)
{
	const bool bVoir = bCibleConnue && (Phase == EPhaseMission::Rejoindre || Phase == EPhaseMission::EnCours);
	if (!bVoir)
	{
		if (Balise) { Balise->SetVisibility(false); }
		if (LumiereBalise) { LumiereBalise->SetVisibility(false); }
		return;
	}
	if (!Balise)
	{
		Balise = NewObject<UStaticMeshComponent>(this, TEXT("BaliseMission"));
		Balise->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Balise->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Balise->SetCastShadow(false);
		Balise->SetupAttachment(RootComponent);
		Balise->SetUsingAbsoluteLocation(true);
		Balise->SetUsingAbsoluteRotation(true);
		Balise->SetUsingAbsoluteScale(true);
		Balise->SetWorldScale3D(FVector(0.35f, 0.35f, 60.f)); // 35 cm de large, 60 m de haut
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			MatBalise = UMaterialInstanceDynamic::Create(Base, this);
			MatBalise->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.7f, 0.05f));
			Balise->SetMaterial(0, MatBalise);
		}
		Balise->RegisterComponent();
	}
	if (!LumiereBalise)
	{
		LumiereBalise = NewObject<UPointLightComponent>(this, TEXT("LumiereMission"));
		LumiereBalise->SetupAttachment(RootComponent);
		LumiereBalise->SetUsingAbsoluteLocation(true);
		LumiereBalise->SetLightColor(FLinearColor(1.f, 0.7f, 0.15f));
		LumiereBalise->SetAttenuationRadius(1500.f);
		LumiereBalise->SetCastShadows(false);
		LumiereBalise->RegisterComponent();
	}
	PhaseBalise += DeltaSeconds;
	const FVector Pied = Cible;
	Balise->SetWorldLocation(Pied + FVector(0.f, 0.f, 3000.f));
	Balise->SetVisibility(true);
	LumiereBalise->SetWorldLocation(Pied + FVector(0.f, 0.f, 400.f));
	LumiereBalise->SetIntensity(6000.f + 3000.f * FMath::Sin(PhaseBalise * 4.f));
	LumiereBalise->SetVisibility(true);
}

void AMissionsChelles::TickLocal(float DeltaSeconds)
{
	CreerEcran();
	AgeMessage += DeltaSeconds;

	UWorld* Monde = GetWorld();
	APlayerController* PC = Monde->GetFirstPlayerController();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;

	if (PC && !Menu.IsValid() && PC->WasInputKeyJustPressed(Reglages().Touche))
	{
		OuvrirMenu();
	}

	// ---- Fin de mission vue ici : XP de reussite (seulement pour ceux qui l'ont jouee) ----
	AgeFinLocale += DeltaSeconds;
	if (Phase != PhaseVueAvant)
	{
		if (Phase == EPhaseMission::Reussie || Phase == EPhaseMission::Ratee)
		{
			AgeFinLocale = 0.f;
		}
		if (Phase == EPhaseMission::Reussie && PhaseVueAvant == EPhaseMission::EnCours && XPGagnee > 0)
		{
			AActor* Pouvoirs = ActeurDeClasse(Monde, TEXT("PouvoirsChelles"));
			float XP = 0.f;
			if (Pouvoirs && LireNombre(Pouvoirs, { TEXT("XPLocale") }, XP))
			{
				EcrireNombre(Pouvoirs, { TEXT("XPLocale") }, XP + XPGagnee);
				UE_LOG(LogTemp, Log, TEXT("[Missions] +%d XP (pouvoirs)."), XPGagnee);
			}
		}
		PhaseVueAvant = Phase;
	}

	MettreAJourBalise(DeltaSeconds);

	if (!Ecran.IsValid())
	{
		return;
	}
	FEtatEcranMissions& E = Ecran->Etat;
	E.Mission = Mission;
	E.Phase = Phase;
	E.Titre = NomMission(Mission);
	E.Secondes = Secondes;
	E.ZombiesRestants = ZombiesRestants;
	E.Detail = Detail;
	E.bAlerte = bAlerte;
	E.XP = XPGagnee;
	E.AgeFin = AgeFinLocale;
	E.Message = Message;
	E.AgeMessage = AgeMessage;
	E.PartVie = VieMax > 0.f ? Vie / VieMax : -1.f;
	E.bChrono = false;
	E.Avancement.Reset();

	const bool bRejoindre = Phase == EPhaseMission::Rejoindre;
	switch (Mission)
	{
	case EMissionChelles::Tramway:
		E.NomVie = TEXT("TRAMWAY");
		E.Consigne = bRejoindre ? FString(TEXT("Rejoignez le TRAMWAY avant qu'il soit attaqu\u00E9"))
			: FString::Printf(TEXT("Gardez le TRAMWAY en vie pendant %d arr\u00EAts"), Objectif);
		E.Avancement = bRejoindre ? FString(TEXT("EN ROUTE")) : FString::Printf(TEXT("ARR\u00CATS %d / %d"), Avancement, Objectif);
		E.bChrono = bRejoindre;
		break;
	case EMissionChelles::Mairie:
		E.NomVie = TEXT("MAIRIE");
		E.Consigne = bRejoindre ? FString(TEXT("Rendez-vous \u00E0 la Mairie de Chelles"))
			: FString(TEXT("Emp\u00EAchez les zombies d'entrer dans la Mairie"));
		E.Avancement = bRejoindre ? FString(TEXT("EN ROUTE")) : (Avancement > 0 ? FString::Printf(TEXT("VAGUE %d"), Avancement) : FString(TEXT("PR\u00C9PAREZ-VOUS")));
		E.bChrono = true;
		break;
	case EMissionChelles::Chasse:
		E.NomVie.Reset();
		E.PartVie = -1.f;
		E.Consigne = FString::Printf(TEXT("Tuez %d zombies avant la fin du chrono"), Objectif);
		E.Avancement = FString::Printf(TEXT("ZOMBIES %d / %d"), Avancement, Objectif);
		E.bChrono = true;
		break;
	default:
		break;
	}

	// ---- Repere a l'ecran vers l'objectif ----
	E.bRepere = false;
	if (bCibleConnue && (Phase == EPhaseMission::Rejoindre || Phase == EPhaseMission::EnCours) && PC && PC->PlayerCameraManager)
	{
		const FVector Point = (FVector)Cible + FVector(0.f, 0.f, 250.f);
		const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
		const FVector Local = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(Point - Camera);
		int32 L = 0, H = 0;
		PC->GetViewportSize(L, H);
		FVector2D SurEcran;
		if (Local.X > 10.f && L > 0 && H > 0 && PC->ProjectWorldLocationToScreen(Point, SurEcran, false))
		{
			E.Repere = FVector2D(SurEcran.X / L, SurEcran.Y / H);
			E.bRepereDerriere = false;
		}
		else
		{
			// Derriere : un point hors de l'ecran, du cote ou il faut se tourner
			FVector2D Dir(Local.Y, -Local.Z * 0.3f);
			if (Dir.IsNearlyZero())
			{
				Dir = FVector2D(0.f, 1.f);
			}
			E.Repere = FVector2D(0.5f, 0.5f) + Dir.GetSafeNormal() * 2.f;
			E.bRepereDerriere = true;
		}
		const float Metres = Pion ? FVector::Dist(Pion->GetActorLocation(), Cible) / 100.f : 0.f;
		E.DistanceRepere = Pion ? FString::Printf(TEXT("%d m"), FMath::RoundToInt(Metres)) : FString();
		// Tout pres : pas besoin du repere
		E.bRepere = !Pion || Metres > 12.f;
	}
}

// ===================================================================================
//  Console
// ===================================================================================

void AMissionsChelles::DecrireEtat() const
{
	static const TCHAR* Phases[] = { TEXT("aucune"), TEXT("rejoindre"), TEXT("en cours"), TEXT("reussie"), TEXT("ratee") };
	UE_LOG(LogTemp, Display, TEXT("[Missions] Mission : %s | phase : %s | avancement %d/%d | vie %.0f/%.0f | %.0f s | zombies %d | %s"),
		Mission ? NomMission(Mission) : TEXT("aucune"), Phase < 5 ? Phases[Phase] : TEXT("?"), Avancement, Objectif, Vie, VieMax, Secondes,
		ZombiesRestants, *Detail);
	UE_LOG(LogTemp, Display, TEXT("[Missions] Disponibles : tramway %s, Mairie %s, zombies %s. Touche %s."),
		bTramwayDispo ? TEXT("oui") : TEXT("non"), bMairieConnue ? TEXT("oui") : TEXT("non"), bZombiesDispo ? TEXT("oui") : TEXT("non"),
		*Reglages().Touche.ToString());
}
