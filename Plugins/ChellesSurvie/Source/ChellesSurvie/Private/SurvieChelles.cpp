// CHELLES 3D - Modes de survie : gestionnaire de partie
//
// DERNIER REFUGE (premier mode) :
//   - les joueurs defendent une zone (le parc de la Mairie, enregistree avec Chelles.RefugeIci) ;
//   - vagues sans fin, de plus en plus dures (Vagues.txt), lancees par le plugin ChellesZombies ;
//   - points par touche / kill / tete, depenses en armes, munitions et barrieres (Refuge.txt) ;
//   - un coup qui devrait tuer met le joueur "a terre" 30 s : un coequipier le releve (E maintenu),
//     sinon il est mort jusqu'a la vague suivante. Plus personne debout = fin de partie.
//
// Aucun asset ni Blueprint du projet n'est modifie. ChellesZombies est pilote par le nom de ses
// fonctions et variables (LancerVagueCommandee, VagueNombre...) : les deux plugins se compilent separement.
#include "SurvieChelles.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"

#include "SSurvieEcran.h"
#include "StandRefuge.h"
#include "SurvieOutils.h"
#include "SurvieRelais.h"

namespace
{
	/** Le choix du mode n'est propose automatiquement qu'une fois par lancement du jeu. */
	bool GModesDejaProposes = false;

	FString DossierPlugin()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesSurvie"));
	}

	bool LireFichier(const TCHAR* NomFichier, TArray<FString>& Lignes)
	{
		const FString Chemin = FPaths::Combine(DossierPlugin(), NomFichier);
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Log, TEXT("[Survie] %s absent : reglages par defaut."), *Chemin);
			return false;
		}
		return true;
	}

	FString VecteurTexte(const FVector& V)
	{
		return FString::Printf(TEXT("%.0f,%.0f,%.0f"), V.X, V.Y, V.Z);
	}

	bool TexteVecteur(const FString& Texte, FVector& V)
	{
		TArray<FString> Morceaux;
		Texte.ParseIntoArray(Morceaux, TEXT(","));
		if (Morceaux.Num() != 3)
		{
			return false;
		}
		V = FVector(FCString::Atof(*Morceaux[0]), FCString::Atof(*Morceaux[1]), FCString::Atof(*Morceaux[2]));
		return true;
	}

	/** Pieds du personnage (le centre de la capsule est a mi-hauteur). */
	FVector Pieds(const APawn* Pion)
	{
		const ACharacter* Perso = Cast<ACharacter>(Pion);
		const float DemiHauteur = (Perso && Perso->GetCapsuleComponent()) ? Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		return Pion->GetActorLocation() - FVector(0.f, 0.f, DemiHauteur);
	}

	const TCHAR* NomType(uint8 Type)
	{
		return Type == (uint8)ETypeStand::Munitions ? TEXT("Munitions") : (Type == (uint8)ETypeStand::Barriere ? TEXT("Barriere") : TEXT("Arme"));
	}
}

ASurvieChelles::ASurvieChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 5.f;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ASurvieChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASurvieChelles, Mode);
	DOREPLIFETIME(ASurvieChelles, Etat);
	DOREPLIFETIME(ASurvieChelles, Vague);
	DOREPLIFETIME(ASurvieChelles, FinPause);
	DOREPLIFETIME(ASurvieChelles, ZombiesRestants);
	DOREPLIFETIME(ASurvieChelles, bVagueCoureurs);
	DOREPLIFETIME(ASurvieChelles, TitreFin);
}

bool ASurvieChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

float ASurvieChelles::Maintenant() const
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
//  Demarrage / arret de l'acteur
// ===================================================================================

void ASurvieChelles::BeginPlay()
{
	Super::BeginPlay();
	if (EstServeur())
	{
		ChargerReglages();
		ChargerPositions();
		UE_LOG(LogTemp, Log, TEXT("[Survie] Pret. F7 = ecran MODES. Zone du Refuge %s."), bCentreConnu ? *VecteurTexte(Centre) : TEXT("pas encore enregistree (Chelles.RefugeIci)"));
	}
}

void ASurvieChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (EstServeur())
	{
		if (AActor* Inv = Invasion())
		{
			SurvieOutils::EcrireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, false);
		}
	}
	FermerModes();
	RetirerEcran();
	Super::EndPlay(Raison);
}

void ASurvieChelles::Tick(float DeltaSeconds)
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
//  Reglages et positions (fichiers du Bloc-notes)
// ===================================================================================

void ASurvieChelles::ChargerReglages()
{
	Courbe = FCourbeVagues();
	Economie = FEconomieRefuge();
	Catalogue = FCatalogueRefuge();

	TArray<FString> Lignes;
	if (LireFichier(TEXT("Vagues.txt"), Lignes))
	{
		Courbe.Lire(Lignes);
	}
	Lignes.Reset();
	if (LireFichier(TEXT("Refuge.txt"), Lignes))
	{
		Economie.Lire(Lignes);
		Catalogue.Lire(Lignes);
		for (const FString& Brute : Lignes)
		{
			FString Cle, Valeur;
			if (!FLigneReglage::Decouper(Brute, Cle, Valeur))
			{
				continue;
			}
			const float N = FLigneReglage::Nombre(Valeur);
			if (Cle == TEXT("DureeATerreSecondes")) DureeATerre = FMath::Clamp(N, 3.f, 300.f);
			else if (Cle == TEXT("DureeRelanceSecondes")) DureeRelance = FMath::Clamp(N, 0.5f, 30.f);
			else if (Cle == TEXT("ApparitionMinMetres")) RayonApparitionMin = FMath::Clamp(N, 5.f, 500.f) * 100.f;
			else if (Cle == TEXT("ApparitionMaxMetres")) RayonApparitionMax = FMath::Clamp(N, 10.f, 1000.f) * 100.f;
			else if (Cle == TEXT("VolumeSireneVague")) VolumeSireneVague = FMath::Clamp(N, 0.f, 4.f);
		}
	}
	RayonApparitionMax = FMath::Max(RayonApparitionMax, RayonApparitionMin + 500.f);

	// Sans fichier : quelques armes de FPS_Game pour pouvoir jouer quand meme
	if (Catalogue.Armes.Num() == 0)
	{
		TArray<FString> Defauts;
		Defauts.Add(TEXT("Arme.Pistolet=BPC_WeaponPistol|500"));
		Defauts.Add(TEXT("Arme.Pompe=BPC_WeaponShotgun|900"));
		Defauts.Add(TEXT("Arme.Fusil=BPC_WeaponRifle|1200"));
		Defauts.Add(TEXT("Arme.Sniper=BPC_WeaponSniper|1500"));
		Catalogue.Lire(Defauts);
	}
}

void ASurvieChelles::ChargerPositions()
{
	bCentreConnu = false;
	Places.Reset();
	TArray<FString> Lignes;
	if (!LireFichier(TEXT("Refuge_Positions.txt"), Lignes))
	{
		return;
	}
	for (const FString& Brute : Lignes)
	{
		FString Cle, Valeur;
		if (!FLigneReglage::Decouper(Brute, Cle, Valeur))
		{
			continue;
		}
		if (Cle == TEXT("Centre"))
		{
			bCentreConnu = TexteVecteur(Valeur, Centre);
		}
		else if (Cle == TEXT("Stand"))
		{
			// Stand=Type|Article|X,Y,Z|Lacet
			TArray<FString> Champs;
			Valeur.ParseIntoArray(Champs, TEXT("|"), false);
			if (Champs.Num() < 4)
			{
				continue;
			}
			FPlaceStand Place;
			Place.Type = Champs[0].Equals(TEXT("Munitions"), ESearchCase::IgnoreCase) ? (uint8)ETypeStand::Munitions
				: (Champs[0].Equals(TEXT("Barriere"), ESearchCase::IgnoreCase) ? (uint8)ETypeStand::Barriere : (uint8)ETypeStand::Arme);
			Place.Article = Champs[1].TrimStartAndEnd();
			if (!TexteVecteur(Champs[2], Place.Position))
			{
				continue;
			}
			Place.Lacet = FCString::Atof(*Champs[3]);
			Places.Add(Place);
		}
	}
}

void ASurvieChelles::SauverPositions() const
{
	TArray<FString> Lignes;
	Lignes.Add(TEXT("# CHELLES 3D - Dernier Refuge : positions enregistrees EN JEU avec la console (touche ²)."));
	Lignes.Add(TEXT("#   Chelles.RefugeIci              = le centre de la zone (les zombies arrivent autour)"));
	Lignes.Add(TEXT("#   Chelles.StandIci Arme Fusil    = un point d'achat d'arme (noms dans Refuge.txt)"));
	Lignes.Add(TEXT("#   Chelles.StandIci Munitions     = un point de munitions"));
	Lignes.Add(TEXT("#   Chelles.StandIci Barriere 750  = une barriere payante en travers de la rue, devant vous"));
	Lignes.Add(TEXT("#   Chelles.StandRetirer           = retire le point le plus proche"));
	Lignes.Add(TEXT("# Vous pouvez aussi effacer une ligne ici avec le Bloc-notes."));
	if (bCentreConnu)
	{
		Lignes.Add(TEXT("Centre=") + VecteurTexte(Centre));
	}
	for (const FPlaceStand& Place : Places)
	{
		Lignes.Add(FString::Printf(TEXT("Stand=%s|%s|%s|%.0f"), NomType(Place.Type), *Place.Article, *VecteurTexte(Place.Position), Place.Lacet));
	}
	const FString Chemin = FPaths::Combine(DossierPlugin(), TEXT("Refuge_Positions.txt"));
	if (!FFileHelper::SaveStringArrayToFile(Lignes, *Chemin, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("[Survie] Impossible d'ecrire %s"), *Chemin);
	}
}

// ===================================================================================
//  Liens avec ChellesZombies (par le nom, sans compiler ensemble)
// ===================================================================================

AActor* ASurvieChelles::Invasion()
{
	if (AActor* Deja = InvasionBranchee.Get())
	{
		return Deja;
	}
	return SurvieOutils::ActeurDeClasse(GetWorld(), TEXT("InvasionZombies"));
}

void ASurvieChelles::BrancherInvasion()
{
	AActor* Inv = SurvieOutils::ActeurDeClasse(GetWorld(), TEXT("InvasionZombies"));
	if (!Inv || InvasionBranchee.Get() == Inv)
	{
		return;
	}
	struct FLien { const TCHAR* Evenement; const TCHAR* Fonction; };
	const FLien Liens[] = {
		{ TEXT("OnZombieTue"), TEXT("QuandZombieTue") },
		{ TEXT("OnZombieTouche"), TEXT("QuandZombieTouche") },
		{ TEXT("OnCoupSurJoueur"), TEXT("QuandCoupSurJoueur") } };
	int32 Branches = 0;
	for (const FLien& Lien : Liens)
	{
		FMulticastDelegateProperty* P = FindFProperty<FMulticastDelegateProperty>(Inv->GetClass(), Lien.Evenement);
		if (!P)
		{
			continue;
		}
		FScriptDelegate Delegue;
		Delegue.BindUFunction(this, FName(Lien.Fonction));
		P->AddDelegate(Delegue, Inv);
		++Branches;
	}
	InvasionBranchee = Inv;
	if (Branches < 3)
	{
		UE_LOG(LogTemp, Error, TEXT("[Survie] Le plugin ChellesZombies est trop ancien (%d/3 evenements trouves) : recompilez ChellesZombies."), Branches);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Survie] Branche sur ChellesZombies."));
	}
}

void ASurvieChelles::QuandZombieTue(ACharacter* Zombie, AController* Tueur, bool bTete)
{
	ASurvieRelais* R = RelaisDe(Tueur);
	if (!R)
	{
		return;
	}
	// L'XP des pouvoirs : dans tous les modes
	R->ClientDonnerXP(Economie.XPPourKill(bTete));
	if (Mode != EModeSurvie::Refuge)
	{
		return;
	}
	++R->Kills;
	if (bTete)
	{
		++R->Tetes;
	}
	R->Points += Economie.PointsPourCoup(true, bTete);
}

void ASurvieChelles::QuandZombieTouche(ACharacter* Zombie, AController* Auteur)
{
	if (Mode != EModeSurvie::Refuge)
	{
		return;
	}
	if (ASurvieRelais* R = RelaisDe(Auteur))
	{
		R->Points += Economie.PointsPourCoup(false, false);
	}
}

void ASurvieChelles::QuandCoupSurJoueur(APawn* Victime, float Degats)
{
	if (Mode != EModeSurvie::Refuge || (Etat != EEtatPartie::Vague && Etat != EEtatPartie::Pause) || !Victime)
	{
		return;
	}
	ASurvieRelais* R = RelaisDe(Victime->GetController());
	if (!R)
	{
		return;
	}
	if (R->Etat != EEtatJoueurSurvie::Debout)
	{
		// Deja a terre ou mort : le coup ne doit pas declencher la mort de FPS_Game
		if (AActor* Inv = Invasion())
		{
			SurvieOutils::EcrireBooleen(Inv, { TEXT("bCoupGereAilleurs") }, true);
		}
		return;
	}
	float Vie = 100.f;
	SurvieOutils::LireNombre(Victime, { TEXT("HealthCurrent"), TEXT("Health") }, Vie);
	if (Vie - Degats > 0.5f)
	{
		return; // le coup ne tue pas : FPS_Game l'applique normalement
	}
	// Le coup aurait tue : le joueur tombe a terre a la place, le coup n'est pas applique
	MettreATerre(R);
	if (AActor* Inv = Invasion())
	{
		SurvieOutils::EcrireBooleen(Inv, { TEXT("bCoupGereAilleurs") }, true);
	}
}

// ===================================================================================
//  Relais des joueurs
// ===================================================================================

void ASurvieChelles::Relais(TArray<ASurvieRelais*>& Resultat) const
{
	Resultat.Reset();
	for (TActorIterator<ASurvieRelais> It(GetWorld()); It; ++It)
	{
		if (!It->IsPendingKill())
		{
			Resultat.Add(*It);
		}
	}
}

ASurvieRelais* ASurvieChelles::RelaisDe(AController* Controleur) const
{
	if (!Controleur)
	{
		return nullptr;
	}
	for (TActorIterator<ASurvieRelais> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == Controleur)
		{
			return *It;
		}
	}
	return nullptr;
}

ASurvieRelais* ASurvieChelles::MonRelais() const
{
	return RelaisDe(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
}

int32 ASurvieChelles::NombreJoueurs() const
{
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	return FMath::Max(1, Tous.Num());
}

void ASurvieChelles::GererRelais()
{
	UWorld* Monde = GetWorld();
	// Un relais par joueur connecte
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		ASurvieRelais* R = RelaisDe(PC);
		if (!R)
		{
			FActorSpawnParameters Params;
			Params.Owner = PC;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			R = Monde->SpawnActor<ASurvieRelais>(ASurvieRelais::StaticClass(), FTransform::Identity, Params);
			if (!R)
			{
				continue;
			}
			// Un joueur qui arrive en pleine partie commence avec les points de depart
			R->Points = Mode == EModeSurvie::Refuge ? Economie.PointsDepart : 0;
			ListeRelais.Add(R);
		}
		R->Pion = PC->GetPawn();
		if (PC->PlayerState)
		{
			R->Nom = PC->PlayerState->GetPlayerName();
		}
		if (R->Nom.IsEmpty())
		{
			R->Nom = TEXT("Joueur");
		}
	}
	// Les joueurs partis
	for (int32 i = ListeRelais.Num() - 1; i >= 0; --i)
	{
		ASurvieRelais* R = ListeRelais[i];
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

// ===================================================================================
//  Serveur : deroulement de la partie
// ===================================================================================

void ASurvieChelles::TickServeur(float Heure)
{
	const float Ecoule = DerniereHeureServeur < 0.f ? 0.f : FMath::Clamp(Heure - DerniereHeureServeur, 0.f, 0.5f);
	DerniereHeureServeur = Heure;

	if (Heure >= ProchaineGestion)
	{
		ProchaineGestion = Heure + 0.5f;
		GererRelais();
		BrancherInvasion();
	}

	if (Mode != EModeSurvie::Refuge)
	{
		return;
	}

	AActor* Inv = Invasion();
	bool bEnCours = false;
	float Restants = 0.f;
	if (Inv)
	{
		SurvieOutils::LireBooleen(Inv, { TEXT("bInvasionEnCours") }, bEnCours);
		SurvieOutils::LireNombre(Inv, { TEXT("ZombiesRestants") }, Restants);
	}
	ZombiesRestants = bEnCours ? (int32)Restants : 0;

	switch (Etat)
	{
	case EEtatPartie::Pause:
		if (Heure >= FinPause)
		{
			LancerVagueSuivante();
		}
		break;

	case EEtatPartie::Vague:
		if (!bVagueDemarree && bEnCours)
		{
			bVagueDemarree = true;
		}
		if (bVagueDemarree && !bEnCours)
		{
			// Tous les zombies de la vague sont tombes
			Etat = EEtatPartie::Pause;
			FinPause = Heure + Courbe.PauseEntreVaguesSecondes;
			AvisATous(FString::Printf(TEXT("VAGUE %d REPOUSSÉE !"), Vague));
			UE_LOG(LogTemp, Log, TEXT("[Survie] Vague %d repoussee."), Vague);
		}
		else if (!bVagueDemarree && Heure > VagueLanceeA + 5.f && Inv)
		{
			// La vague n'a pas demarre (invasion encore en train de finir...) : on redemande
			VagueLanceeA = Heure;
			SurvieOutils::Appeler(Inv, TEXT("LancerVagueCommandee"));
		}
		break;

	case EEtatPartie::Fin:
		if (Heure >= FinPause)
		{
			Arreter();
		}
		return;

	default:
		return;
	}

	// Joueurs a terre : relance en cours ou fin du delai
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	for (ASurvieRelais* R : Tous)
	{
		if (R->Etat != EEtatJoueurSurvie::ATerre)
		{
			continue;
		}
		ASurvieRelais* Aide = R->Releveur.Get();
		const bool bAideValable = Aide && Aide->Etat == EEtatJoueurSurvie::Debout && IsValid(Aide->Pion) && IsValid(R->Pion)
			&& FVector::Dist(Aide->Pion->GetActorLocation(), R->Pion->GetActorLocation()) < 250.f;
		if (bAideValable)
		{
			R->Relance = FMath::Min(1.f, R->Relance + Ecoule / DureeRelance);
			if (R->Relance >= 1.f)
			{
				Remettre(R, false);
				++Aide->Releves;
				Aide->Points += Economie.PointsRelever;
				AvisATous(FString::Printf(TEXT("%s a relevé %s"), *Aide->Nom, *R->Nom));
				continue;
			}
		}
		else if (R->Relance > 0.f || Aide)
		{
			R->Relance = 0.f;
			R->Releveur = nullptr;
		}
		if (Heure >= R->FinATerre)
		{
			TuerJoueur(R);
		}
	}
	VerifierFin();
}

void ASurvieChelles::Demarrer(EModeSurvie NouveauMode, int32 VagueDepart)
{
	if (!EstServeur())
	{
		return;
	}
	if (NouveauMode != EModeSurvie::Refuge)
	{
		Arreter();
		return;
	}
	if (Mode == EModeSurvie::Refuge)
	{
		Arreter(); // on recommence proprement
	}

	ChargerReglages();
	ChargerPositions();
	GererRelais();
	BrancherInvasion();
	if (!bCentreConnu)
	{
		Centre = CentreParDefaut();
		bCentreConnu = true;
		SauverPositions();
		AvisATous(TEXT("Zone du Refuge = ici. Pour la mettre au parc de la Mairie : console Chelles.RefugeIci"));
	}

	if (AActor* Inv = Invasion())
	{
		SurvieOutils::EcrireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, true);
		bool bEnCours = false;
		if (SurvieOutils::LireBooleen(Inv, { TEXT("bInvasionEnCours") }, bEnCours) && bEnCours)
		{
			SurvieOutils::Appeler(Inv, TEXT("TerminerVagueCommandee"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Survie] Plugin ChellesZombies absent : pas de zombies pour le Dernier Refuge."));
	}

	Mode = EModeSurvie::Refuge;
	Vague = FMath::Max(0, VagueDepart - 1);
	Etat = EEtatPartie::Pause;
	FinPause = Maintenant() + 10.f;
	TitreFin.Reset();
	bVagueDemarree = false;
	bVagueCoureurs = false;

	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	for (ASurvieRelais* R : Tous)
	{
		R->Points = Economie.PointsDepart;
		R->Kills = 0;
		R->Tetes = 0;
		R->FoisATerre = 0;
		R->Releves = 0;
		Remettre(R, true);
	}
	PoserStands();
	AvisATous(TEXT("DERNIER REFUGE : première vague dans 10 secondes !"));
	UE_LOG(LogTemp, Log, TEXT("[Survie] Dernier Refuge lance (vague de depart %d, %d joueur(s), centre %s)."), VagueDepart, Tous.Num(), *VecteurTexte(Centre));
}

void ASurvieChelles::Arreter()
{
	if (!EstServeur())
	{
		return;
	}
	if (AActor* Inv = Invasion())
	{
		bool bEnCours = false;
		if (Mode != EModeSurvie::Libre && SurvieOutils::LireBooleen(Inv, { TEXT("bInvasionEnCours") }, bEnCours) && bEnCours)
		{
			SurvieOutils::Appeler(Inv, TEXT("TerminerVagueCommandee"));
		}
		SurvieOutils::EcrireBooleen(Inv, { TEXT("bInvasionsAutoEnPause") }, false);
	}
	RetirerStands();
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	for (ASurvieRelais* R : Tous)
	{
		if (R->Etat != EEtatJoueurSurvie::Debout)
		{
			Remettre(R, true);
		}
	}
	if (Mode != EModeSurvie::Libre)
	{
		UE_LOG(LogTemp, Log, TEXT("[Survie] Retour au jeu libre."));
	}
	Mode = EModeSurvie::Libre;
	Etat = EEtatPartie::Inactif;
	Vague = 0;
	ZombiesRestants = 0;
	bVagueCoureurs = false;
}

void ASurvieChelles::LancerVagueSuivante()
{
	++Vague;
	const FVagueCalculee V = Courbe.Calculer(Vague, NombreJoueurs());

	// Les morts reviennent au debut de chaque vague
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	for (ASurvieRelais* R : Tous)
	{
		if (R->Etat == EEtatJoueurSurvie::Mort)
		{
			Remettre(R, true);
			Avis(R, TEXT("Tu es de retour !"));
		}
	}

	Etat = EEtatPartie::Vague;
	bVagueCoureurs = V.bCoureurs;
	bVagueDemarree = false;
	VagueLanceeA = Maintenant();

	AActor* Inv = Invasion();
	if (!Inv)
	{
		return;
	}
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueNombre") }, (float)V.Nombre);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueVie") }, V.Vie);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueVitesse") }, V.Vitesse);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VaguePartCostauds") }, V.PartCostauds);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueVieCostaud") }, V.VieCostaud);
	SurvieOutils::EcrireVecteur(Inv, TEXT("VagueCentre"), Centre);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueRayonMin") }, RayonApparitionMin);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueRayonMax") }, RayonApparitionMax);
	SurvieOutils::EcrireTexte(Inv, TEXT("VagueMessage"), V.bCoureurs ? FString::Printf(TEXT("VAGUE %d - COUREURS !"), Vague) : FString::Printf(TEXT("VAGUE %d"), Vague));
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueDureeMessage") }, 4.f);
	SurvieOutils::EcrireNombre(Inv, { TEXT("VagueVolumeSirene") }, VolumeSireneVague);
	if (!SurvieOutils::Appeler(Inv, TEXT("LancerVagueCommandee")))
	{
		UE_LOG(LogTemp, Error, TEXT("[Survie] LancerVagueCommandee introuvable : recompilez le plugin ChellesZombies."));
	}
	UE_LOG(LogTemp, Log, TEXT("[Survie] Vague %d : %d zombies, vie %.0f, vitesse %.0f, costauds %.0f %%%s."),
		Vague, V.Nombre, V.Vie, V.Vitesse, V.PartCostauds * 100.f, V.bCoureurs ? TEXT(", COUREURS") : TEXT(""));
}

void ASurvieChelles::VerifierFin()
{
	if (Mode != EModeSurvie::Refuge || (Etat != EEtatPartie::Vague && Etat != EEtatPartie::Pause))
	{
		return;
	}
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	if (Tous.Num() == 0)
	{
		return;
	}
	for (ASurvieRelais* R : Tous)
	{
		if (R->Etat == EEtatJoueurSurvie::Debout)
		{
			return;
		}
	}
	// Plus personne debout
	Etat = EEtatPartie::Fin;
	FinPause = Maintenant() + 15.f;
	TitreFin = TEXT("LE REFUGE EST TOMBÉ");
	if (AActor* Inv = Invasion())
	{
		SurvieOutils::Appeler(Inv, TEXT("TerminerVagueCommandee"));
	}
	UE_LOG(LogTemp, Log, TEXT("[Survie] Fin de partie a la vague %d."), Vague);
	for (ASurvieRelais* R : Tous)
	{
		UE_LOG(LogTemp, Log, TEXT("[Survie]   %s : %d pts, %d kills, %d tetes, %d fois a terre, %d releves"), *R->Nom, R->Points, R->Kills, R->Tetes, R->FoisATerre, R->Releves);
	}
}

// ===================================================================================
//  Serveur : a terre, mort, retour
// ===================================================================================

void ASurvieChelles::MettreATerre(ASurvieRelais* R)
{
	R->Etat = EEtatJoueurSurvie::ATerre;
	R->FinATerre = Maintenant() + DureeATerre;
	R->Relance = 0.f;
	R->Releveur = nullptr;
	++R->FoisATerre;
	R->Points -= Economie.PerteQuandATerre(R->Points);

	if (ACharacter* Perso = Cast<ACharacter>(R->Pion))
	{
		Perso->Tags.AddUnique(TEXT("ChellesHorsJeu"));
		SurvieOutils::EcrireNombre(Perso, { TEXT("HealthCurrent"), TEXT("Health") }, 1.f);
		if (Perso->GetCharacterMovement())
		{
			Perso->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
		}
		Perso->Crouch();
	}
	AvisATous(FString::Printf(TEXT("%s est à terre !"), *R->Nom));
	UE_LOG(LogTemp, Log, TEXT("[Survie] %s a terre."), *R->Nom);
	VerifierFin();
}

void ASurvieChelles::TuerJoueur(ASurvieRelais* R)
{
	R->Etat = EEtatJoueurSurvie::Mort;
	R->Relance = 0.f;
	R->Releveur = nullptr;
	if (APawn* P = R->Pion)
	{
		P->Tags.AddUnique(TEXT("ChellesHorsJeu"));
		P->SetActorHiddenInGame(true);
	}
	AvisATous(FString::Printf(TEXT("%s est mort. Retour à la prochaine vague."), *R->Nom));
	UE_LOG(LogTemp, Log, TEXT("[Survie] %s mort."), *R->Nom);
	VerifierFin();
}

void ASurvieChelles::Remettre(ASurvieRelais* R, bool bReapparaitre)
{
	R->Etat = EEtatJoueurSurvie::Debout;
	R->Relance = 0.f;
	R->Releveur = nullptr;
	APawn* P = R->Pion;
	if (!P)
	{
		return;
	}
	P->Tags.Remove(TEXT("ChellesHorsJeu"));
	P->SetActorHiddenInGame(false);
	float VieMax = 100.f;
	SurvieOutils::LireNombre(P, { TEXT("HealthMax"), TEXT("MaxHealth") }, VieMax);
	SurvieOutils::EcrireNombre(P, { TEXT("HealthCurrent"), TEXT("Health") }, bReapparaitre ? VieMax : VieMax * 0.5f);
	SurvieOutils::EcrireBooleen(P, { TEXT("IsDead"), TEXT("isDead") }, false);
	if (ACharacter* Perso = Cast<ACharacter>(P))
	{
		Perso->UnCrouch();
	}
	if (bReapparaitre && bCentreConnu && Mode == EModeSurvie::Refuge)
	{
		FVector Sol;
		if (TrouverSol(Centre, 400.f, Sol))
		{
			const ACharacter* Perso = Cast<ACharacter>(P);
			const float DemiHauteur = (Perso && Perso->GetCapsuleComponent()) ? Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			P->SetActorLocation(Sol + FVector(0.f, 0.f, DemiHauteur + 10.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void ASurvieChelles::Relever(ASurvieRelais* Releveur, ASurvieRelais* Cible, bool bAppuie)
{
	if (!Releveur || !Cible || Releveur == Cible)
	{
		return;
	}
	if (!bAppuie)
	{
		if (Cible->Releveur.Get() == Releveur)
		{
			Cible->Releveur = nullptr;
			Cible->Relance = 0.f;
		}
		return;
	}
	if (Cible->Etat != EEtatJoueurSurvie::ATerre || Releveur->Etat != EEtatJoueurSurvie::Debout)
	{
		return;
	}
	if (!Cible->Releveur.IsValid())
	{
		Cible->Releveur = Releveur;
	}
}

// ===================================================================================
//  Serveur : achats et points d'achat
// ===================================================================================

void ASurvieChelles::Acheter(ASurvieRelais* Acheteur, AStandRefuge* Stand)
{
	if (!Acheteur || !IsValid(Stand) || Mode != EModeSurvie::Refuge || Etat == EEtatPartie::Fin)
	{
		return;
	}
	APawn* P = Acheteur->Pion;
	if (!P || Acheteur->Etat != EEtatJoueurSurvie::Debout || Stand->bOuvert)
	{
		return;
	}
	const float Distance = FMath::Sqrt(Stand->GetComponentsBoundingBox(true).ComputeSquaredDistanceToPoint(P->GetActorLocation()));
	if (Distance > Stand->Portee() + 80.f)
	{
		return; // trop loin (marge pour le decalage du reseau)
	}
	if (Acheteur->Points < Stand->Prix)
	{
		Avis(Acheteur, FString::Printf(TEXT("Pas assez de points (%d / %d)"), Acheteur->Points, Stand->Prix));
		return;
	}

	if (Stand->Type == ETypeStand::Barriere)
	{
		FEconomieRefuge::Payer(Acheteur->Points, Stand->Prix);
		Stand->Ouvrir();
		AvisATous(FString::Printf(TEXT("%s a ouvert une barrière"), *Acheteur->Nom));
		return;
	}

	if (Stand->Type == ETypeStand::Munitions)
	{
		const int32 Armes = SurvieOutils::RemplirMunitions(P);
		if (Armes == 0)
		{
			Avis(Acheteur, TEXT("Aucune arme à recharger"));
			return;
		}
		FEconomieRefuge::Payer(Acheteur->Points, Stand->Prix);
		Acheteur->ClientRecharger();
		Avis(Acheteur, TEXT("Munitions pleines !"));
		return;
	}

	// Arme : on ne revend pas tant que la precedente traine par terre
	if (AActor* AuSol = Acheteur->ArmeAuSol.Get())
	{
		if (!AuSol->IsPendingKill() && AuSol->GetAttachParentActor() == nullptr && FVector::Dist(AuSol->GetActorLocation(), Stand->GetActorLocation()) < 1000.f)
		{
			Avis(Acheteur, TEXT("Ramasse d'abord ton arme (E)"));
			return;
		}
	}
	UClass* Classe = LoadClass<AActor>(nullptr, *Stand->ClasseArme);
	if (!Classe)
	{
		Avis(Acheteur, TEXT("Arme introuvable (voir Refuge.txt)"));
		UE_LOG(LogTemp, Error, TEXT("[Survie] Arme introuvable : %s"), *Stand->ClasseArme);
		return;
	}
	FEconomieRefuge::Payer(Acheteur->Points, Stand->Prix);

	// L'arme apparait entre le stand et le joueur : il la ramasse comme d'habitude (touche E de FPS_Game)
	const FVector VersJoueur = (P->GetActorLocation() - Stand->GetActorLocation()).GetSafeNormal2D();
	const FVector Lieu = Stand->GetActorLocation() + VersJoueur * 90.f + FVector(0.f, 0.f, 60.f);
	APlayerController* PC = Acheteur->GetPC();
	const AActor* Modele = Classe->GetDefaultObject<AActor>();
	if (PC && !PC->IsLocalController() && Modele && !Modele->GetIsReplicated())
	{
		// Arme non repliquee (armes solo de FPS_Game) : elle apparait directement chez le joueur
		Acheteur->ClientCreerArme(Classe->GetPathName(), Lieu);
	}
	else
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Acheteur->ArmeAuSol = GetWorld()->SpawnActor<AActor>(Classe, FTransform(Lieu), Params);
	}
	Avis(Acheteur, FString::Printf(TEXT("%s achetée ! Ramasse-la avec E"), *Stand->Libelle));
}

void ASurvieChelles::PoserStands()
{
	RetirerStands();
	TArray<FPlaceStand> AFaire = Places;
	if (AFaire.Num() == 0 && bCentreConnu)
	{
		// Aucun point enregistre : un petit marche autour du centre pour pouvoir jouer tout de suite
		const struct { uint8 Type; const TCHAR* Article; FVector Decalage; } Defaut[] = {
			{ (uint8)ETypeStand::Munitions, TEXT(""), FVector(350.f, 0.f, 0.f) },
			{ (uint8)ETypeStand::Arme, TEXT("Pistolet"), FVector(0.f, 350.f, 0.f) },
			{ (uint8)ETypeStand::Arme, TEXT("Pompe"), FVector(-350.f, 0.f, 0.f) },
			{ (uint8)ETypeStand::Arme, TEXT("Fusil"), FVector(0.f, -350.f, 0.f) } };
		for (const auto& D : Defaut)
		{
			FPlaceStand Place;
			Place.Type = D.Type;
			Place.Article = D.Article;
			Place.Position = Centre + D.Decalage;
			Place.Lacet = (-D.Decalage).Rotation().Yaw; // face au centre
			AFaire.Add(Place);
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FPlaceStand& Place : AFaire)
	{
		FString Libelle;
		FString Classe;
		int32 Prix = 0;
		if (Place.Type == (uint8)ETypeStand::Arme)
		{
			const FArticleRefuge* Article = Catalogue.Trouver(Place.Article);
			if (!Article)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Survie] Arme \"%s\" absente de Refuge.txt : point d'achat ignore."), *Place.Article);
				continue;
			}
			Libelle = Article->Nom;
			Classe = FCatalogueRefuge::CheminClasse(Article->Classe);
			Prix = Article->Prix;
		}
		else if (Place.Type == (uint8)ETypeStand::Munitions)
		{
			Libelle = TEXT("Munitions");
			Prix = Catalogue.PrixMunitions;
		}
		else
		{
			Libelle = TEXT("Barriere");
			const int32 PrixPlace = FCString::Atoi(*Place.Article);
			Prix = PrixPlace > 0 ? PrixPlace : Catalogue.PrixBarriere;
		}

		// Pose au sol exactement sous la position enregistree
		FVector Sol = Place.Position;
		FHitResult Impact;
		FCollisionQueryParams Requete(SCENE_QUERY_STAT(ChellesSurvieStand), false);
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			Requete.AddIgnoredActor(*It);
		}
		if (GetWorld()->LineTraceSingleByChannel(Impact, Place.Position + FVector(0.f, 0.f, 200.f), Place.Position - FVector(0.f, 0.f, 600.f), ECC_Visibility, Requete))
		{
			Sol = Impact.ImpactPoint;
		}
		AStandRefuge* Stand = GetWorld()->SpawnActor<AStandRefuge>(AStandRefuge::StaticClass(), FTransform(FRotator(0.f, Place.Lacet, 0.f), Sol), Params);
		if (Stand)
		{
			Stand->Regler((ETypeStand)Place.Type, Libelle, Prix, Classe);
			Stands.Add(Stand);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Survie] %d point(s) d'achat poses."), Stands.Num());
}

void ASurvieChelles::RetirerStands()
{
	for (AStandRefuge* Stand : Stands)
	{
		if (IsValid(Stand))
		{
			Stand->Destroy();
		}
	}
	Stands.Reset();
}

bool ASurvieChelles::TrouverSol(const FVector& Autour, float Rayon, FVector& Sol) const
{
	FCollisionQueryParams Requete(SCENE_QUERY_STAT(ChellesSurvieSol), false);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		Requete.AddIgnoredActor(*It);
	}
	for (int32 Essai = 0; Essai < 16; ++Essai)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = Essai == 0 ? 0.f : FMath::FRandRange(Rayon * 0.3f, Rayon);
		const FVector XY = Autour + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
		FHitResult Impact;
		if (GetWorld()->LineTraceSingleByChannel(Impact, XY + FVector(0.f, 0.f, 300.f), XY - FVector(0.f, 0.f, 1000.f), ECC_Visibility, Requete)
			&& Impact.ImpactNormal.Z > 0.7f && FMath::Abs(Impact.ImpactPoint.Z - Autour.Z) < 400.f)
		{
			Sol = Impact.ImpactPoint;
			return true;
		}
	}
	return false;
}

FVector ASurvieChelles::CentreParDefaut() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->GetPawn())
	{
		return Pieds(PC->GetPawn());
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get() && It->Get()->GetPawn())
		{
			return Pieds(It->Get()->GetPawn());
		}
	}
	return FVector::ZeroVector;
}

void ASurvieChelles::Avis(ASurvieRelais* Destinataire, const FString& Texte)
{
	if (Destinataire)
	{
		Destinataire->ClientAvis(Texte);
	}
}

void ASurvieChelles::AvisATous(const FString& Texte)
{
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	for (ASurvieRelais* R : Tous)
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

void ASurvieChelles::CommandeRefugeIci()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!EstServeur() || !PC || !PC->GetPawn())
	{
		MessageLocal(TEXT("Seul l'hôte (ou le jeu solo) peut placer le Refuge."));
		return;
	}
	ChargerPositions();
	Centre = Pieds(PC->GetPawn());
	bCentreConnu = true;
	SauverPositions();
	MessageLocal(TEXT("Zone du Refuge enregistrée ici"));
	UE_LOG(LogTemp, Log, TEXT("[Survie] Centre du Refuge : %s"), *VecteurTexte(Centre));
}

void ASurvieChelles::CommandeStandIci(const TArray<FString>& Arguments)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!EstServeur() || !P)
	{
		MessageLocal(TEXT("Seul l'hôte (ou le jeu solo) peut placer les points d'achat."));
		return;
	}
	if (Arguments.Num() == 0)
	{
		MessageLocal(TEXT("Exemples : Chelles.StandIci Arme Fusil / Munitions / Barriere 750"));
		return;
	}
	ChargerReglages();
	ChargerPositions();

	FPlaceStand Place;
	const FString& TypeTexte = Arguments[0];
	const float Lacet = P->GetActorRotation().Yaw;
	const FVector Devant = FRotator(0.f, Lacet, 0.f).Vector();
	if (TypeTexte.Equals(TEXT("Munitions"), ESearchCase::IgnoreCase))
	{
		Place.Type = (uint8)ETypeStand::Munitions;
		Place.Lacet = Lacet + 180.f;
	}
	else if (TypeTexte.Equals(TEXT("Barriere"), ESearchCase::IgnoreCase) || TypeTexte.Equals(TEXT("Barrière"), ESearchCase::IgnoreCase))
	{
		Place.Type = (uint8)ETypeStand::Barriere;
		Place.Article = Arguments.Num() > 1 ? Arguments[1] : FString::FromInt(Catalogue.PrixBarriere);
		Place.Lacet = Lacet; // le mur se met en travers, devant vous
	}
	else if (TypeTexte.Equals(TEXT("Arme"), ESearchCase::IgnoreCase) && Arguments.Num() > 1)
	{
		if (!Catalogue.Trouver(Arguments[1]))
		{
			FString Noms;
			for (const FArticleRefuge& A : Catalogue.Armes)
			{
				Noms += (Noms.IsEmpty() ? TEXT("") : TEXT(", ")) + A.Nom;
			}
			MessageLocal(FString::Printf(TEXT("Arme inconnue. Choix : %s"), *Noms));
			return;
		}
		Place.Type = (uint8)ETypeStand::Arme;
		Place.Article = Arguments[1];
		Place.Lacet = Lacet + 180.f;
	}
	else
	{
		MessageLocal(TEXT("Exemples : Chelles.StandIci Arme Fusil / Munitions / Barriere 750"));
		return;
	}
	Place.Position = Pieds(P) + Devant * 150.f;
	Places.Add(Place);
	SauverPositions();
	MessageLocal(FString::Printf(TEXT("Point enregistré : %s %s"), NomType(Place.Type), *Place.Article));
	if (Mode == EModeSurvie::Refuge)
	{
		PoserStands(); // visible tout de suite
	}
}

void ASurvieChelles::CommandeStandRetirer()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!EstServeur() || !P)
	{
		return;
	}
	ChargerPositions();
	int32 Meilleur = INDEX_NONE;
	float MeilleureDist = 500.f;
	for (int32 i = 0; i < Places.Num(); ++i)
	{
		const float D = FVector::Dist(Places[i].Position, Pieds(P));
		if (D < MeilleureDist)
		{
			MeilleureDist = D;
			Meilleur = i;
		}
	}
	if (Meilleur == INDEX_NONE)
	{
		MessageLocal(TEXT("Aucun point d'achat à moins de 5 m"));
		return;
	}
	Places.RemoveAt(Meilleur);
	SauverPositions();
	MessageLocal(TEXT("Point d'achat retiré"));
	if (Mode == EModeSurvie::Refuge)
	{
		PoserStands();
	}
}

void ASurvieChelles::CommandePoints(int32 Quantite)
{
	if (!EstServeur())
	{
		return;
	}
	if (ASurvieRelais* R = MonRelais())
	{
		R->Points = FMath::Max(0, R->Points + Quantite);
	}
}

void ASurvieChelles::CommandeVague(int32 Numero)
{
	if (!EstServeur() || Mode != EModeSurvie::Refuge)
	{
		MessageLocal(TEXT("Lancez d'abord le Dernier Refuge (F7)"));
		return;
	}
	if (AActor* Inv = Invasion())
	{
		SurvieOutils::Appeler(Inv, TEXT("TerminerVagueCommandee"));
	}
	Vague = FMath::Max(0, Numero - 1);
	Etat = EEtatPartie::Pause;
	FinPause = Maintenant() + 3.f;
}

// ===================================================================================
//  Chaque machine : ecran, touches
// ===================================================================================

void ASurvieChelles::MessageLocal(const FString& Texte)
{
	Message = Texte;
	AgeMessage = 0.f;
}

void ASurvieChelles::CreerEcran()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (Ecran.IsValid() || !Vue)
	{
		return;
	}
	Ecran = SNew(SSurvieEcran);
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 5);
}

void ASurvieChelles::RetirerEcran()
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

void ASurvieChelles::OuvrirModes()
{
	UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (EcranModes.IsValid() || !Vue || !PC)
	{
		return;
	}
	GModesDejaProposes = true;
	bModesProposes = true;
	TWeakObjectPtr<ASurvieChelles> Moi(this);
	EcranModes = SNew(SModesSurvie)
		.bPeutChoisir(GetNetMode() != NM_Client)
		.QuandChoisi_Lambda([Moi](uint8 Quel, int32 VagueDepart)
		{
			if (Moi.IsValid())
			{
				Moi->ChoisirMode(Quel, VagueDepart);
			}
		})
		.QuandFerme_Lambda([Moi]()
		{
			if (Moi.IsValid())
			{
				Moi->FermerModes();
			}
		});
	Vue->AddViewportWidgetContent(EcranModes.ToSharedRef(), 900);

	FInputModeUIOnly Entree;
	Entree.SetWidgetToFocus(EcranModes);
	Entree.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Entree);
	PC->bShowMouseCursor = true;
}

void ASurvieChelles::FermerModes()
{
	if (!EcranModes.IsValid())
	{
		return;
	}
	if (UGameViewportClient* Vue = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(EcranModes.ToSharedRef());
	}
	EcranModes.Reset();
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void ASurvieChelles::ChoisirMode(uint8 Quel, int32 VagueDepart)
{
	FermerModes();
	if (ASurvieRelais* R = MonRelais())
	{
		R->ServeurChoisirMode(Quel, VagueDepart);
	}
	else if (EstServeur())
	{
		// Le relais n'existe pas encore (tout debut de partie) : on decide directement
		if (Quel == (uint8)EModeSurvie::Refuge)
		{
			Demarrer(EModeSurvie::Refuge, VagueDepart);
		}
		else
		{
			Arreter();
		}
	}
}

AStandRefuge* ASurvieChelles::StandProche(APawn* Pion) const
{
	AStandRefuge* Meilleur = nullptr;
	float MeilleureDist = TNumericLimits<float>::Max();
	for (TActorIterator<AStandRefuge> It(GetWorld()); It; ++It)
	{
		if (It->bOuvert)
		{
			continue;
		}
		const float D = FMath::Sqrt(It->GetComponentsBoundingBox(true).ComputeSquaredDistanceToPoint(Pion->GetActorLocation()));
		if (D <= It->Portee() && D < MeilleureDist)
		{
			MeilleureDist = D;
			Meilleur = *It;
		}
	}
	return Meilleur;
}

ASurvieRelais* ASurvieChelles::CoequipierATerreProche(APawn* Pion) const
{
	TArray<ASurvieRelais*> Tous;
	Relais(Tous);
	for (ASurvieRelais* R : Tous)
	{
		if (R->Etat == EEtatJoueurSurvie::ATerre && IsValid(R->Pion) && R->Pion != Pion
			&& FVector::Dist(R->Pion->GetActorLocation(), Pion->GetActorLocation()) < 200.f)
		{
			return R;
		}
	}
	return nullptr;
}

void ASurvieChelles::AppliquerEtatLocal(ASurvieRelais* R)
{
	APlayerController* PC = R->GetPC();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	if (!P || R->Etat == DernierEtatLocal)
	{
		return;
	}
	DernierEtatLocal = R->Etat;
	// A terre ou mort : on ne bouge plus et on ne tire plus (le personnage reste a sa place)
	if (R->Etat == EEtatJoueurSurvie::Debout)
	{
		P->EnableInput(PC);
	}
	else
	{
		P->DisableInput(PC);
	}
}

void ASurvieChelles::TickLocal(float DeltaSeconds)
{
	CreerEcran();
	AgeMessage += DeltaSeconds;

	UWorld* Monde = GetWorld();
	APlayerController* PC = Monde->GetFirstPlayerController();
	APawn* P = PC ? PC->GetPawn() : nullptr;
	ASurvieRelais* R = MonRelais();
	const float Heure = Maintenant();

	// ---- Ecran MODES : propose une fois quand le jeu demarre (apres le menu SOLO / Multi-Player) ----
	if (!bModesProposes && !GModesDejaProposes && GetNetMode() != NM_Client && P && !Monde->IsPaused())
	{
		DepuisJeuLance += DeltaSeconds;
		if (DepuisJeuLance >= 2.f)
		{
			OuvrirModes();
		}
	}
	if (PC && PC->WasInputKeyJustPressed(EKeys::F7) && !EcranModes.IsValid())
	{
		OuvrirModes();
	}

	// ---- Etat du joueur de cette machine ----
	FString Invite;
	float RelanceDonnee = -1.f;
	if (R && P && Mode == EModeSurvie::Refuge)
	{
		AppliquerEtatLocal(R);
		if (R->Etat == EEtatJoueurSurvie::Debout && !EcranModes.IsValid())
		{
			const bool bE = PC->IsInputKeyDown(EKeys::E);
			ASurvieRelais* Blesse = CoequipierATerreProche(P);
			if (Blesse)
			{
				Invite = FString::Printf(TEXT("[E maintenu] Relever %s"), *Blesse->Nom);
				if (bE)
				{
					RelanceDonnee = Blesse->Relance;
				}
			}
			else if (AStandRefuge* Stand = StandProche(P))
			{
				Invite = Stand->Invite();
				if (PC->WasInputKeyJustPressed(EKeys::E) && Heure >= ProchaineDemandeAchat)
				{
					ProchaineDemandeAchat = Heure + 0.4f;
					R->ServeurAcheter(Stand);
				}
			}

			// Relance : on previent le serveur seulement quand ca change
			ASurvieRelais* NouvelleCible = (bE && Blesse) ? Blesse : nullptr;
			if (NouvelleCible != CibleReleveeLocale.Get())
			{
				if (ASurvieRelais* Ancienne = CibleReleveeLocale.Get())
				{
					R->ServeurRelever(Ancienne, false);
				}
				if (NouvelleCible)
				{
					R->ServeurRelever(NouvelleCible, true);
				}
				CibleReleveeLocale = NouvelleCible;
			}
		}
	}
	else if (R && P && R->Etat != DernierEtatLocal)
	{
		AppliquerEtatLocal(R); // fin de partie : on rend les commandes
	}

	if (!Ecran.IsValid())
	{
		return;
	}
	FEtatEcranSurvie& E = Ecran->Etat;
	E.bActif = Mode == EModeSurvie::Refuge && Etat != EEtatPartie::Inactif && Etat != EEtatPartie::Fin;
	E.NomMode = TEXT("DERNIER REFUGE");
	E.Vague = Vague;
	E.ZombiesRestants = ZombiesRestants;
	E.bVagueEnCours = Etat == EEtatPartie::Vague;
	E.bCoureurs = bVagueCoureurs && Etat == EEtatPartie::Vague;
	E.SecondesPause = Etat == EEtatPartie::Pause ? FMath::Max(0.f, FinPause - Heure) : 0.f;
	E.MesPoints = R ? R->Points : 0;
	E.MonEtat = R ? (uint8)R->Etat : 0;
	E.SecondesATerre = R ? FMath::Max(0.f, R->FinATerre - Heure) : 0.f;
	E.MaRelance = R ? R->Relance : 0.f;
	E.Invite = Invite;
	E.RelanceDonnee = RelanceDonnee;
	E.bFin = Mode == EModeSurvie::Refuge && Etat == EEtatPartie::Fin;
	E.TitreFin = TitreFin;
	E.SecondesFin = FMath::Max(0.f, FinPause - Heure);
	E.Message = Message;
	E.AgeMessage = AgeMessage;

	E.Scores.Reset();
	if (E.bFin)
	{
		TArray<ASurvieRelais*> Tous;
		Relais(Tous);
		for (ASurvieRelais* Joueur : Tous)
		{
			FLigneScore L;
			L.Nom = Joueur->Nom;
			L.Points = Joueur->Points;
			L.Kills = Joueur->Kills;
			L.Tetes = Joueur->Tetes;
			L.FoisATerre = Joueur->FoisATerre;
			L.Releves = Joueur->Releves;
			L.Etat = (uint8)Joueur->Etat;
			L.bMoi = Joueur == R;
			E.Scores.Add(L);
		}
		E.Scores.Sort([](const FLigneScore& A, const FLigneScore& B) { return A.Kills > B.Kills; });
	}
}
