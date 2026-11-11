// CHELLES 3D - Invasion zombies
//
// Toutes les 2 heures de jeu (reglable) :
//   1. sirene + message rouge sang "ATTENTION PURGE ZOMBIE INVASION" pendant 30 s ;
//   2. des hordes de zombies apparaissent a plusieurs endroits autour des joueurs et les poursuivent ;
//   3. chaque coup de zombie retire 20 % de la vie max du joueur (via la vie de FPS_Game : ApplyDamage) ;
//   4. une balle dans la tete tue le zombie, une balle dans le corps lui retire 33,4 % (3 balles).
//
// On n'ecrit RIEN dans les assets du projet : le zombie de FPS_Game (BP_AI_CharZombie) est
// seulement regle au moment ou il apparait (vie, degats, vitesse) et habille avec la peau DecayedZombieB.
#include "InvasionZombies.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

#include "SAlerteZombies.h"
#include "SireneChelles.h"

#define CHEMIN_MONTAGE_ATTAQUE TEXT("/Game/FPS_Game/Character/TPP/AnimStarterPack/Zombie/ZombieAttackMontage.ZombieAttackMontage")
#define CHEMIN_SANG TEXT("/Game/FPS_Game/Particles/P_body_bullet_NoStop.P_body_bullet_NoStop")
#define CHEMIN_SON_TETE TEXT("/Game/FPS_Game/Sounds/SFX/CharacterRelated/Other/Headshot.Headshot")

static const FName NomPeau(TEXT("PeauZombieChelles"));
static const FName MarqueZombie(TEXT("ChellesZombie"));
static const FName MarqueHabille(TEXT("ChellesZombieHabille"));

namespace
{
	// --------- Lecture / ecriture des variables des Blueprints FPS_Game, sans les modifier ---------

	FProperty* TrouverVariable(UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			for (TFieldIterator<FProperty> It(Objet->GetClass()); It; ++It)
			{
				if (It->GetName().Equals(Nom, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	bool LireNombre(UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(TrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? (float)P->GetFloatingPointPropertyValue(Adresse) : (float)P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	bool EcrireNombre(UObject* Objet, std::initializer_list<const TCHAR*> Noms, float Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(TrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Adresse, (double)Valeur);
		}
		else
		{
			P->SetIntPropertyValue(Adresse, (int64)Valeur);
		}
		return true;
	}

	bool LireBooleen(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(TrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	void EcrireBooleen(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool Valeur)
	{
		if (FBoolProperty* P = CastField<FBoolProperty>(TrouverVariable(Objet, Noms)))
		{
			P->SetPropertyValue_InContainer(Objet, Valeur);
		}
	}

	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesZombies"), TEXT("Zombies.txt"));
	}

	bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}
}

const FName AInvasionZombies::MarqueJoueurHorsJeu(TEXT("ChellesHorsJeu"));

AInvasionZombies::AInvasionZombies()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 5.f;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void AInvasionZombies::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInvasionZombies, bInvasionEnCours);
	DOREPLIFETIME(AInvasionZombies, ZombiesRestants);
	DOREPLIFETIME(AInvasionZombies, SecondesRestantes);
	DOREPLIFETIME(AInvasionZombies, Zombies);
}

bool AInvasionZombies::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

// ===================================================================================
//  Demarrage
// ===================================================================================

void AInvasionZombies::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();

	if (GetNetMode() != NM_DedicatedServer)
	{
		Peau = Reglages.PeauZombie.IsEmpty() ? nullptr : LoadObject<USkeletalMesh>(nullptr, *Reglages.PeauZombie);
		MontageAttaque = LoadObject<UAnimMontage>(nullptr, CHEMIN_MONTAGE_ATTAQUE);

		if (UGameViewportClient* Vue = GetWorld()->GetGameViewport())
		{
			Ecran = SNew(SAlerteZombies);
			Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 6);
		}
	}

	if (EstServeur())
	{
		MontageAttaque = MontageAttaque ? MontageAttaque : LoadObject<UAnimMontage>(nullptr, CHEMIN_MONTAGE_ATTAQUE);
		ClasseZombie = LoadClass<ACharacter>(nullptr, *Reglages.ClasseZombie);
		if (!ClasseZombie)
		{
			UE_LOG(LogTemp, Error, TEXT("[Zombies] Classe de zombie introuvable : %s (verifiez ClasseZombie= dans Zombies.txt)"), *Reglages.ClasseZombie);
		}
		ProchaineInvasion = GetWorld()->GetTimeSeconds() + Reglages.PremiereApresMinutes * 60.f;
		UE_LOG(LogTemp, Log, TEXT("[Zombies] Pret. Premiere invasion dans %.0f min, puis toutes les %.0f min. (Console : Chelles.Zombies pour tester)"),
			Reglages.PremiereApresMinutes, Reglages.IntervalleMinutes);
	}
}

void AInvasionZombies::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Ecran.IsValid())
	{
		if (UWorld* W = GetWorld())
		{
			if (UGameViewportClient* Vue = W->GetGameViewport())
			{
				Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
			}
		}
		Ecran.Reset();
	}
	if (SonSirene)
	{
		SonSirene->Stop();
	}
	Super::EndPlay(Raison);
}

void AInvasionZombies::ChargerReglages()
{
	const FString Chemin = CheminReglages();
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[Zombies] %s absent : reglages par defaut."), *Chemin);
		return;
	}

	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		const float Nombre = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));

		if (Cle == TEXT("Actif")) Reglages.bActif = EstOui(Valeur);
		else if (Cle == TEXT("IntervalleMinutes")) Reglages.IntervalleMinutes = FMath::Max(1.f, Nombre);
		else if (Cle == TEXT("PremiereInvasionApresMinutes")) Reglages.PremiereApresMinutes = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("DureeInvasionMinutes")) Reglages.DureeInvasionMinutes = FMath::Max(0.5f, Nombre);
		else if (Cle == TEXT("DureeMessageSecondes")) Reglages.DureeMessageSecondes = FMath::Clamp(Nombre, 3.f, 120.f);
		else if (Cle == TEXT("NombreZombies")) Reglages.NombreZombies = FMath::Clamp((int32)Nombre, 1, 200);
		else if (Cle == TEXT("ZombiesEnPlusParJoueur")) Reglages.EnPlusParJoueur = FMath::Clamp((int32)Nombre, 0, 100);
		else if (Cle == TEXT("NombreMaxZombies")) Reglages.NombreMax = FMath::Clamp((int32)Nombre, 1, 300);
		else if (Cle == TEXT("DegatsZombiePourcent")) Reglages.DegatsZombiePourcent = FMath::Clamp(Nombre, 0.f, 100.f);
		else if (Cle == TEXT("DegatsCorpsPourcent")) Reglages.DegatsCorpsPourcent = FMath::Clamp(Nombre, 1.f, 100.f);
		else if (Cle == TEXT("VitesseZombie")) Reglages.VitesseZombie = FMath::Clamp(Nombre, 50.f, 2000.f);
		else if (Cle == TEXT("DistanceApparitionMinMetres")) Reglages.DistanceMinMetres = FMath::Max(5.f, Nombre);
		else if (Cle == TEXT("DistanceApparitionMaxMetres")) Reglages.DistanceMaxMetres = FMath::Max(10.f, Nombre);
		else if (Cle == TEXT("VolumeSirene")) Reglages.VolumeSirene = FMath::Clamp(Nombre, 0.f, 4.f);
		else if (Cle == TEXT("ClasseZombie") && !Valeur.IsEmpty()) Reglages.ClasseZombie = Valeur;
		else if (Cle == TEXT("PeauZombie")) Reglages.PeauZombie = Valeur;
		else if (Cle == TEXT("Message") && !Valeur.IsEmpty()) Reglages.Message = Valeur;
	}
	Reglages.DistanceMaxMetres = FMath::Max(Reglages.DistanceMaxMetres, Reglages.DistanceMinMetres + 5.f);
	UE_LOG(LogTemp, Log, TEXT("[Zombies] Reglages lus dans %s"), *Chemin);
}

// ===================================================================================
//  Boucle
// ===================================================================================

void AInvasionZombies::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (EstServeur())
	{
		TickServeur(GetWorld()->GetTimeSeconds());
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickLocal(DeltaSeconds);
	}
}

void AInvasionZombies::TickServeur(float Maintenant)
{
	if (!bInvasionEnCours)
	{
		if (Reglages.bActif && !bInvasionsAutoEnPause && Maintenant >= ProchaineInvasion)
		{
			TArray<APawn*> Joueurs;
			JoueursVivants(Joueurs);
			if (Joueurs.Num() == 0)
			{
				ProchaineInvasion = Maintenant + 60.f; // personne en jeu (menu...) : on reessaie dans 1 minute
			}
			else
			{
				LancerInvasion();
			}
		}
		return;
	}

	// Apparitions etalees (un zombie toutes les 0,3 s) a partir de 3 s apres la sirene
	// En mode vague (Dernier Refuge), jamais plus de NombreMaxZombies en meme temps : les autres attendent leur tour
	bool bPlaceLibre = true;
	if (bModeVague)
	{
		int32 Debout = 0;
		for (ACharacter* Z : Zombies)
		{
			const FEtatZombie* EtatZ = Z ? Etats.Find(Z) : nullptr;
			if (EtatZ && !EtatZ->bMort)
			{
				++Debout;
			}
		}
		bPlaceLibre = Debout < Reglages.NombreMax;
	}
	if (AFaireApparaitre > 0 && bPlaceLibre && Maintenant >= ProchaineApparition && Maintenant >= DebutInvasion + 3.f)
	{
		ProchaineApparition = Maintenant + 0.3f;
		if (Foyers.Num() == 0)
		{
			ChoisirFoyers();
		}
		bool bFait = false;
		for (int32 Essai = 0; Essai < 4 && !bFait && Foyers.Num() > 0; ++Essai)
		{
			const FVector& Foyer = Foyers[FMath::RandRange(0, Foyers.Num() - 1)];
			FVector Sol;
			if (TrouverPointApparition(Foyer, 0.f, 6.f, Sol) && FaireApparaitreZombie(Sol))
			{
				bFait = true;
			}
		}
		--AFaireApparaitre; // meme en cas d'echec, pour ne jamais bloquer
		if (!bFait)
		{
			Foyers.Reset(); // on cherchera d'autres endroits
		}
	}

	// Les zombies vivants poursuivent et attaquent
	int32 Vivants = 0;
	for (int32 i = Zombies.Num() - 1; i >= 0; --i)
	{
		ACharacter* Z = Zombies[i];
		FEtatZombie* Etat = Z ? Etats.Find(Z) : nullptr;
		if (!IsValid(Z) || !Etat)
		{
			Zombies.RemoveAt(i);
			continue;
		}
		if (Etat->bMort)
		{
			continue;
		}
		if (Z->GetActorLocation().Z < -100000.f)
		{
			Z->Destroy();
			Zombies.RemoveAt(i);
			continue;
		}
		++Vivants;
		PiloterZombie(Z, *Etat, Maintenant);
	}

	ZombiesRestants = Vivants + AFaireApparaitre;
	if (Maintenant >= ProchaineSynchro)
	{
		ProchaineSynchro = Maintenant + 0.5f;
		SecondesRestantes = FMath::Max(0.f, FinInvasion - Maintenant);
	}

	if (Maintenant >= FinInvasion)
	{
		TerminerInvasion(false);
	}
	else if (AFaireApparaitre <= 0 && Vivants == 0 && Maintenant > DebutInvasion + 10.f)
	{
		TerminerInvasion(true);
	}
}

void AInvasionZombies::TickLocal(float DeltaSeconds)
{
	if (AgeAlerte >= 0.f)
	{
		AgeAlerte += DeltaSeconds;
		if (!bSireneArretee && AgeAlerte >= DureeAlerte - 1.5f)
		{
			bSireneArretee = true;
			if (SonSirene)
			{
				SonSirene->FadeOut(1.5f, 0.f);
			}
		}
		if (AgeAlerte > DureeAlerte)
		{
			AgeAlerte = -1.f;
		}
	}
	AgeFin += DeltaSeconds;

	// Habiller les zombies arrives par le reseau (ou crees ici)
	for (ACharacter* Z : Zombies)
	{
		if (IsValid(Z) && !Z->Tags.Contains(MarqueHabille))
		{
			Habiller(Z);
		}
	}

	if (Ecran.IsValid())
	{
		FEtatAlerteZombies& E = Ecran->Etat;
		E.AgeAlerte = AgeAlerte;
		E.DureeAlerte = DureeAlerte;
		if (!TexteAlerte.IsEmpty())
		{
			E.Texte = TexteAlerte;
		}
		E.bInvasion = bInvasionEnCours;
		E.Restants = ZombiesRestants;
		E.SecondesRestantes = SecondesRestantes;
		E.MessageFin = MessageFin;
		E.AgeFin = AgeFin;
	}
}

// ===================================================================================
//  Debut et fin de l'invasion
// ===================================================================================

void AInvasionZombies::LancerInvasion()
{
	if (!EstServeur() || bInvasionEnCours)
	{
		return;
	}
	if (!ClasseZombie)
	{
		ClasseZombie = LoadClass<ACharacter>(nullptr, *Reglages.ClasseZombie);
	}
	if (!ClasseZombie)
	{
		UE_LOG(LogTemp, Error, TEXT("[Zombies] Impossible de lancer l'invasion : classe %s introuvable."), *Reglages.ClasseZombie);
		return;
	}

	TArray<APawn*> Joueurs;
	JoueursVivants(Joueurs);
	const float Maintenant = GetWorld()->GetTimeSeconds();

	bModeVague = false;
	bInvasionEnCours = true;
	DebutInvasion = Maintenant;
	FinInvasion = Maintenant + Reglages.DureeInvasionMinutes * 60.f;
	ProchaineInvasion = Maintenant + Reglages.IntervalleMinutes * 60.f;
	AFaireApparaitre = FMath::Min(Reglages.NombreMax, Reglages.NombreZombies + Reglages.EnPlusParJoueur * FMath::Max(0, Joueurs.Num() - 1));
	ProchaineApparition = Maintenant;
	SecondesRestantes = FinInvasion - Maintenant;
	Foyers.Reset();
	ChoisirFoyers();

	UE_LOG(LogTemp, Log, TEXT("[Zombies] INVASION : %d zombies, %d joueur(s), %d foyer(s), duree %.1f min."),
		AFaireApparaitre, Joueurs.Num(), Foyers.Num(), Reglages.DureeInvasionMinutes);

	MulticastAlerte(Reglages.Message, Reglages.DureeMessageSecondes, Reglages.VolumeSirene);
}

void AInvasionZombies::LancerVagueCommandee()
{
	if (!EstServeur() || bInvasionEnCours)
	{
		return;
	}
	if (!ClasseZombie)
	{
		ClasseZombie = LoadClass<ACharacter>(nullptr, *Reglages.ClasseZombie);
	}
	if (!ClasseZombie)
	{
		UE_LOG(LogTemp, Error, TEXT("[Zombies] Impossible de lancer la vague : classe %s introuvable."), *Reglages.ClasseZombie);
		return;
	}
	const float Maintenant = GetWorld()->GetTimeSeconds();
	bModeVague = true;
	bInvasionEnCours = true;
	DebutInvasion = Maintenant;
	FinInvasion = Maintenant + 3600.f; // une vague ne s'arrete pas toute seule : il faut tuer tous les zombies
	AFaireApparaitre = FMath::Clamp(VagueNombre, 1, 300);
	ProchaineApparition = Maintenant;
	SecondesRestantes = 0.f;
	Foyers.Reset();
	ChoisirFoyers();

	UE_LOG(LogTemp, Log, TEXT("[Zombies] VAGUE : %d zombies, vie %.0f, vitesse %.0f, costauds %.0f %%, %d foyer(s)."),
		AFaireApparaitre, VagueVie, VagueVitesse, VaguePartCostauds * 100.f, Foyers.Num());

	if (!VagueMessage.IsEmpty())
	{
		MulticastAlerte(VagueMessage, VagueDureeMessage, VagueVolumeSirene);
	}
}

void AInvasionZombies::TerminerVagueCommandee()
{
	TerminerInvasion(false);
}

void AInvasionZombies::TerminerInvasion(bool bRepoussee)
{
	if (!EstServeur() || !bInvasionEnCours)
	{
		return;
	}
	const bool bEtaitVague = bModeVague;
	bModeVague = false;
	bInvasionEnCours = false;
	AFaireApparaitre = 0;
	ZombiesRestants = 0;
	SecondesRestantes = 0.f;

	// Les survivants s'effondrent
	for (ACharacter* Z : TArray<ACharacter*>(Zombies))
	{
		FEtatZombie* Etat = Z ? Etats.Find(Z) : nullptr;
		if (IsValid(Z) && Etat && !Etat->bMort)
		{
			Tuer(Z, false, FVector::ZeroVector);
		}
	}
	Zombies.Reset();
	Etats.Reset();

	const float Maintenant = GetWorld()->GetTimeSeconds();
	ProchaineInvasion = FMath::Max(ProchaineInvasion, Maintenant + 60.f);
	UE_LOG(LogTemp, Log, TEXT("[Zombies] Fin de l'invasion (%s). Prochaine dans %.0f min."),
		bRepoussee ? TEXT("repoussee") : TEXT("temps ecoule"), (ProchaineInvasion - Maintenant) / 60.f);

	// En vague commandee, c'est le plugin du mode de jeu qui affiche son propre message
	MulticastFin(bEtaitVague ? FString() : (bRepoussee ? FString(TEXT("INVASION REPOUSSÉE !")) : FString(TEXT("FIN DE LA PURGE"))));
}

void AInvasionZombies::MulticastAlerte_Implementation(const FString& Texte, float Duree, float Volume)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TexteAlerte = Texte;
	DureeAlerte = Duree;
	AgeAlerte = 0.f;
	AgeFin = 100.f;

	if (SonSirene)
	{
		SonSirene->Stop();
	}
	Sirene = NewObject<USireneChelles>(this);
	SonSirene = UGameplayStatics::CreateSound2D(this, Sirene, Volume, 1.f, 0.f, nullptr, false, false);
	if (SonSirene)
	{
		SonSirene->Play();
		bSireneArretee = false;
	}
}

void AInvasionZombies::MulticastFin_Implementation(const FString& Texte)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	MessageFin = Texte;
	AgeFin = Texte.IsEmpty() ? 100.f : 0.f;
	if (AgeAlerte >= 0.f)
	{
		AgeAlerte = -1.f;
	}
	if (SonSirene && !bSireneArretee)
	{
		bSireneArretee = true;
		SonSirene->FadeOut(1.f, 0.f);
	}
}

// ===================================================================================
//  Ou faire apparaitre les zombies
// ===================================================================================

void AInvasionZombies::JoueursVivants(TArray<APawn*>& Resultat) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (Pion && EstVivant(Pion))
		{
			Resultat.Add(Pion);
		}
	}
}

bool AInvasionZombies::EstVivant(APawn* Joueur)
{
	if (!IsValid(Joueur))
	{
		return false;
	}
	if (Joueur->Tags.Contains(MarqueJoueurHorsJeu))
	{
		return false; // a terre ou mort en mode survie : les zombies le laissent
	}
	bool bMort = false;
	if (LireBooleen(Joueur, { TEXT("IsDead"), TEXT("isDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
	{
		return false;
	}
	float Vie = 1.f;
	if (LireNombre(Joueur, { TEXT("HealthCurrent"), TEXT("Health") }, Vie) && Vie <= 0.f)
	{
		return false;
	}
	return true;
}

float AInvasionZombies::VieMax(APawn* Joueur)
{
	float Max = 100.f;
	if (LireNombre(Joueur, { TEXT("HealthMax"), TEXT("MaxHealth") }, Max) && Max > 0.f)
	{
		return Max;
	}
	return 100.f;
}

void AInvasionZombies::ChoisirFoyers()
{
	TArray<APawn*> Joueurs;
	JoueursVivants(Joueurs);
	const int32 Voulus = FMath::Clamp(AFaireApparaitre / 6, 3, 8);
	const float DistMin = Reglages.DistanceMinMetres * 100.f;
	const float DistMax = Reglages.DistanceMaxMetres * 100.f;

	// Vague commandee : les foyers sont autour de la zone de jeu (ex. le Refuge), pas des joueurs
	if (bModeVague)
	{
		for (int32 i = 0; i < Voulus * 3 && Foyers.Num() < Voulus; ++i)
		{
			FVector Point;
			if (TrouverPointApparition(VagueCentre, VagueRayonMin, FMath::Max(VagueRayonMax, VagueRayonMin + 300.f), Point))
			{
				Foyers.Add(Point);
			}
		}
		if (Foyers.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[Zombies] %d foyer(s) d'apparition autour de la zone de la vague."), Foyers.Num());
			return;
		}
	}

	for (int32 i = 0; i < Voulus * 3 && Foyers.Num() < Voulus; ++i)
	{
		FVector Autour;
		if (Joueurs.Num() > 0)
		{
			Autour = Joueurs[i % Joueurs.Num()]->GetActorLocation();
		}
		else
		{
			break;
		}
		FVector Point;
		if (TrouverPointApparition(Autour, DistMin, DistMax, Point))
		{
			Foyers.Add(Point);
		}
	}

	// Secours : les departs de joueurs de la carte (tag ChellesDepart), puis tout pres des joueurs
	if (Foyers.Num() == 0)
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			Foyers.Add(It->GetActorLocation());
		}
	}
	if (Foyers.Num() == 0)
	{
		for (APawn* J : Joueurs)
		{
			FVector Point;
			if (TrouverPointApparition(J->GetActorLocation(), 1200.f, 2000.f, Point))
			{
				Foyers.Add(Point);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Zombies] %d foyer(s) d'apparition."), Foyers.Num());
}

bool AInvasionZombies::TrouverPointApparition(const FVector& Autour, float DistMin, float DistMax, FVector& Resultat) const
{
	UWorld* W = GetWorld();
	const ACharacter* Modele = ClasseZombie ? Cast<ACharacter>(ClasseZombie->GetDefaultObject()) : nullptr;
	const float DemiHauteur = (Modele && Modele->GetCapsuleComponent()) ? Modele->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	const float Rayon = (Modele && Modele->GetCapsuleComponent()) ? Modele->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesZombiesSol), false);
	for (TActorIterator<APawn> It(W); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	for (int32 Essai = 0; Essai < 25; ++Essai)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Dist = FMath::FRandRange(DistMin, DistMax);
		const FVector XY = Autour + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);

		FHitResult Sol;
		const FVector Haut(XY.X, XY.Y, Autour.Z + 3000.f);
		const FVector Bas(XY.X, XY.Y, Autour.Z - 3000.f);
		if (!W->LineTraceSingleByChannel(Sol, Haut, Bas, ECC_Visibility, Params))
		{
			continue;
		}
		// Pas sur un toit ni au fond d'un trou : a peu pres a la hauteur du joueur
		if (FMath::Abs(Sol.ImpactPoint.Z - Autour.Z) > 450.f || Sol.ImpactNormal.Z < 0.7f)
		{
			continue;
		}
		const FVector Centre = Sol.ImpactPoint + FVector(0.f, 0.f, DemiHauteur + 5.f);
		if (W->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Rayon, DemiHauteur), Params))
		{
			continue; // dans un mur, une voiture...
		}
		Resultat = Sol.ImpactPoint;
		return true;
	}
	return false;
}

// ===================================================================================
//  Les zombies
// ===================================================================================

ACharacter* AInvasionZombies::FaireApparaitreZombie(const FVector& Sol)
{
	UWorld* W = GetWorld();
	const ACharacter* Modele = Cast<ACharacter>(ClasseZombie->GetDefaultObject());
	const float DemiHauteur = (Modele && Modele->GetCapsuleComponent()) ? Modele->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;

	const FTransform Ou(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Sol + FVector(0.f, 0.f, DemiHauteur + 5.f));
	ACharacter* Z = W->SpawnActorDeferred<ACharacter>(ClasseZombie, Ou, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Z)
	{
		return nullptr;
	}
	// Controleur IA simple : c'est ce plugin qui le guide (la carte n'a pas de NavMesh)
	Z->AIControllerClass = AAIController::StaticClass();
	Z->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	UGameplayStatics::FinishSpawningActor(Z, Ou);
	if (!IsValid(Z))
	{
		return nullptr;
	}
	if (!Z->GetController())
	{
		Z->SpawnDefaultController();
	}

	PreparerZombie(Z);
	Zombies.Add(Z);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Habiller(Z);
	}
	return Z;
}

void AInvasionZombies::PreparerZombie(ACharacter* Zombie)
{
	Zombie->Tags.AddUnique(MarqueZombie);

	// Le Blueprint garde sa propre vie : on la rend enorme, c'est le plugin qui decide de la mort
	// (tete = mort, corps = 33,4 %). Ses propres degats au joueur sont mis a 0 : le plugin applique 20 %.
	EcrireNombre(Zombie, { TEXT("Health"), TEXT("HealthCurrent") }, 1000000.f);
	EcrireNombre(Zombie, { TEXT("Damage") }, 0.f);
	EcrireNombre(Zombie, { TEXT("BaseDamage") }, 0.f);

	if (UCharacterMovementComponent* Mouvement = Zombie->GetCharacterMovement())
	{
		Mouvement->MaxWalkSpeed = Reglages.VitesseZombie;
		Mouvement->bOrientRotationToMovement = true;
		Mouvement->bUseControllerDesiredRotation = false;
		Mouvement->RotationRate = FRotator(0.f, 360.f, 0.f);
		Mouvement->JumpZVelocity = FMath::Max(Mouvement->JumpZVelocity, 520.f);
		Mouvement->SetMovementMode(MOVE_Walking);
	}
	Zombie->bUseControllerRotationYaw = false;

	if (USkeletalMeshComponent* Corps = Zombie->GetMesh())
	{
		Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	Zombie->OnTakeAnyDamage.AddDynamic(this, &AInvasionZombies::ZombieToucheQuelconque);
	Zombie->OnTakePointDamage.AddDynamic(this, &AInvasionZombies::ZombieTouchePrecis);

	FEtatZombie Etat;
	Etat.Vie = 100.f;
	Etat.Vitesse = Reglages.VitesseZombie * FMath::FRandRange(0.8f, 1.15f);
	if (bModeVague)
	{
		Etat.Vie = FMath::Max(1.f, VagueVie);
		Etat.Vitesse = VagueVitesse * FMath::FRandRange(0.85f, 1.1f);
		if (FMath::FRand() < VaguePartCostauds)
		{
			// Costaud : beaucoup plus de vie, un peu plus lent, plus grand (vu par le serveur ; en multi
			// les autres joueurs le voient a taille normale)
			Etat.Vie = FMath::Max(Etat.Vie, VagueVieCostaud);
			Etat.Vitesse *= 0.85f;
			Zombie->SetActorScale3D(FVector(1.2f));
		}
	}
	if (UCharacterMovementComponent* Mouvement = Zombie->GetCharacterMovement())
	{
		Mouvement->MaxWalkSpeed = Etat.Vitesse;
	}
	Etat.DernierePosition = Zombie->GetActorLocation();
	Etat.CoteContournement = FMath::RandBool() ? 1.f : -1.f;
	Etats.Add(Zombie, Etat);
}

void AInvasionZombies::Habiller(ACharacter* Zombie)
{
	Zombie->Tags.AddUnique(MarqueHabille);
	USkeletalMeshComponent* Corps = Zombie->GetMesh();
	if (!Corps)
	{
		return;
	}
	Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	if (!Peau)
	{
		return; // pas de peau DecayedZombieB : on garde l'apparence du Blueprint
	}

	// La peau DecayedZombieB a les memes os que le mannequin : elle suit exactement ses animations de zombie
	USkeletalMeshComponent* Habit = NewObject<USkeletalMeshComponent>(Zombie, NomPeau);
	Habit->SetSkeletalMesh(Peau);
	Habit->SetupAttachment(Corps);
	Habit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Habit->SetGenerateOverlapEvents(false);
	Habit->RegisterComponent();
	Habit->AttachToComponent(Corps, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Habit->SetMasterPoseComponent(Corps);
	Zombie->AddInstanceComponent(Habit);

	// Le corps d'origine devient invisible mais garde ses collisions : les balles touchent ses os (tete...)
	Corps->SetHiddenInGame(true, false);
	Corps->SetCastShadow(false);
}

void AInvasionZombies::PiloterZombie(ACharacter* Zombie, FEtatZombie& Etat, float Maintenant)
{
	// 1) Choisir le joueur vivant le plus proche (deux fois par seconde)
	if (Maintenant >= Etat.ProchainChoixCible || !EstVivant(Etat.Cible.Get()))
	{
		Etat.ProchainChoixCible = Maintenant + 0.5f;
		TArray<APawn*> Joueurs;
		JoueursVivants(Joueurs);
		APawn* Meilleur = nullptr;
		float MeilleureDist = TNumericLimits<float>::Max();
		for (APawn* J : Joueurs)
		{
			const float D = FVector::DistSquared(J->GetActorLocation(), Zombie->GetActorLocation());
			if (D < MeilleureDist)
			{
				MeilleureDist = D;
				Meilleur = J;
			}
		}
		Etat.Cible = Meilleur;
	}

	// 2) Coup en cours : les degats tombent au milieu de l'animation
	if (Etat.CoupPrevuA > 0.f && Maintenant >= Etat.CoupPrevuA)
	{
		Etat.CoupPrevuA = -1.f;
		APawn* Victime = Etat.CibleDuCoup.Get();
		if (EstVivant(Victime))
		{
			const FVector Ecart = Victime->GetActorLocation() - Zombie->GetActorLocation();
			if (Ecart.Size2D() < 240.f && FMath::Abs(Ecart.Z) < 200.f)
			{
				const float Degats = VieMax(Victime) * Reglages.DegatsZombiePourcent / 100.f;
				// Un mode de jeu (survie) peut gerer le coup lui-meme : joueur "a terre" au lieu de mourir
				bCoupGereAilleurs = false;
				OnCoupSurJoueur.Broadcast(Victime, Degats);
				const bool bGereAilleurs = bCoupGereAilleurs;
				bCoupGereAilleurs = false;
				if (!bGereAilleurs)
				{
					UGameplayStatics::ApplyDamage(Victime, Degats, Zombie->GetController(), Zombie, UDamageType::StaticClass());
				}
			}
		}
	}

	APawn* Cible = Etat.Cible.Get();
	if (!Cible)
	{
		return;
	}

	FVector Vers = Cible->GetActorLocation() - Zombie->GetActorLocation();
	const float Hauteur = Vers.Z;
	Vers.Z = 0.f;
	const float Distance = Vers.Size();
	const FVector Direction = Vers.GetSafeNormal();

	// Optimisation : a plus de 40 m du joueur, animation 15 fois par seconde au lieu de chaque image
	// (on ne voit pas la difference de si loin). Repasse a la normale en dessous de 35 m.
	const bool bLoin = Distance > (Etat.bAllege ? 3500.f : 4000.f);
	if (bLoin != Etat.bAllege)
	{
		Etat.bAllege = bLoin;
		if (USkeletalMeshComponent* Corps = Zombie->GetMesh())
		{
			Corps->SetComponentTickInterval(bLoin ? 1.f / 15.f : 0.f);
		}
	}

	// Trop loin de tout le monde (bloque derriere un batiment...) : il reapparait plus pres
	if (Distance > 15000.f)
	{
		FVector Sol;
		if (TrouverPointApparition(Cible->GetActorLocation(), Reglages.DistanceMinMetres * 100.f, Reglages.DistanceMaxMetres * 100.f, Sol))
		{
			const float DemiHauteur = Zombie->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Zombie->SetActorLocation(Sol + FVector(0.f, 0.f, DemiHauteur + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		return;
	}

	// 3) A portee : il attaque
	if (Distance < 160.f && FMath::Abs(Hauteur) < 180.f)
	{
		Zombie->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
		if (Maintenant >= Etat.ProchaineAttaque)
		{
			Etat.ProchaineAttaque = Maintenant + 1.6f;
			Etat.CoupPrevuA = Maintenant + 0.55f;
			Etat.CibleDuCoup = Cible;
			MulticastAttaque(Zombie);
		}
		Etat.Bloque = 0;
		Etat.DernierePosition = Zombie->GetActorLocation();
		return;
	}

	// 4) Sinon il marche droit vers le joueur (et contourne s'il est coince)
	FVector Marche = Direction;
	if (Maintenant < Etat.ContournerJusqua)
	{
		Marche = Direction.RotateAngleAxis(70.f * Etat.CoteContournement, FVector::UpVector);
	}
	Zombie->AddMovementInput(Marche, 1.f);

	if (Maintenant >= Etat.ProchainControle)
	{
		Etat.ProchainControle = Maintenant + 1.f;
		// Le Blueprint peut remettre sa propre vitesse : on reimpose celle de l'invasion
		if (UCharacterMovementComponent* Mouvement = Zombie->GetCharacterMovement())
		{
			Mouvement->MaxWalkSpeed = Etat.Vitesse;
		}
		const float Parcouru = FVector::Dist2D(Zombie->GetActorLocation(), Etat.DernierePosition);
		Etat.DernierePosition = Zombie->GetActorLocation();
		if (Parcouru < 40.f)
		{
			++Etat.Bloque;
			Zombie->Jump();
			Etat.ContournerJusqua = Maintenant + 1.5f;
			if (Etat.Bloque % 3 == 0)
			{
				Etat.CoteContournement = -Etat.CoteContournement;
			}
			if (Etat.Bloque >= 8)
			{
				// Coince depuis 8 s : il reapparait ailleurs autour du joueur
				Etat.Bloque = 0;
				FVector Sol;
				if (TrouverPointApparition(Cible->GetActorLocation(), 1500.f, Reglages.DistanceMinMetres * 100.f, Sol))
				{
					const float DemiHauteur = Zombie->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					Zombie->SetActorLocation(Sol + FVector(0.f, 0.f, DemiHauteur + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
		else
		{
			Etat.Bloque = 0;
		}
	}
}

void AInvasionZombies::MulticastAttaque_Implementation(ACharacter* Zombie)
{
	if (!IsValid(Zombie) || !MontageAttaque || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (USkeletalMeshComponent* Corps = Zombie->GetMesh())
	{
		if (UAnimInstance* Anim = Corps->GetAnimInstance())
		{
			Anim->Montage_Play(MontageAttaque, 1.f);
		}
	}
}

// ===================================================================================
//  Degats recus par les zombies
// ===================================================================================

void AInvasionZombies::ZombieTouchePrecis(AActor* Touche, float Degats, AController* Auteur, FVector PointImpact, UPrimitiveComponent* Composant,
	FName Os, FVector Direction, const UDamageType* TypeDegats, AActor* Causeur)
{
	ACharacter* Z = Cast<ACharacter>(Touche);
	FEtatZombie* Etat = Z ? Etats.Find(Z) : nullptr;
	if (!Etat || Etat->bMort)
	{
		return;
	}

	// Tete : par le nom de l'os touche, sinon par la hauteur de l'impact
	const FString NomOs = Os.ToString();
	bool bTete = NomOs.Contains(TEXT("head"), ESearchCase::IgnoreCase) || NomOs.Contains(TEXT("neck"), ESearchCase::IgnoreCase);
	if (!bTete && Z->GetMesh() && Z->GetMesh()->DoesSocketExist(TEXT("head")))
	{
		const float HauteurTete = Z->GetMesh()->GetSocketLocation(TEXT("head")).Z;
		bTete = PointImpact.Z >= HauteurTete - 8.f && FVector::Dist2D(PointImpact, Z->GetMesh()->GetSocketLocation(TEXT("head"))) < 35.f;
	}

	Etat->ImageCoupPrecis = GFrameCounter;
	Etat->bCoupTete = bTete;
	Etat->DirectionCoup = Direction;
}

void AInvasionZombies::ZombieToucheQuelconque(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur)
{
	ACharacter* Z = Cast<ACharacter>(Touche);
	FEtatZombie* Etat = Z ? Etats.Find(Z) : nullptr;
	if (!Etat || Etat->bMort || Degats <= 0.f)
	{
		return;
	}
	// Un zombie ne blesse pas un autre zombie
	if (Causeur && Causeur->Tags.Contains(MarqueZombie))
	{
		return;
	}

	// Qui a tire (pour les points du mode survie)
	AController* Tireur = Auteur;
	if (!Tireur && Causeur)
	{
		Tireur = Causeur->GetInstigatorController();
		if (!Tireur)
		{
			if (APawn* PionCauseur = Cast<APawn>(Causeur->GetOwner()))
			{
				Tireur = PionCauseur->GetController();
			}
		}
	}
	if (Tireur)
	{
		Etat->DernierAuteur = Tireur;
	}

	const bool bPrecis = Etat->ImageCoupPrecis == GFrameCounter;
	if (bPrecis && Etat->bCoupTete)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Zombies] Tir dans la tete : %s"), *Z->GetName());
		Tuer(Z, true, Etat->DirectionCoup, Tireur);
		return;
	}

	Etat->Vie -= Reglages.DegatsCorpsPourcent;
	if (Etat->Vie <= 0.5f)
	{
		Tuer(Z, false, bPrecis ? Etat->DirectionCoup : FVector::ZeroVector, Tireur);
	}
	else
	{
		OnZombieTouche.Broadcast(Z, Tireur);
	}
}

void AInvasionZombies::Tuer(ACharacter* Zombie, bool bTete, const FVector& Direction, AController* Tueur)
{
	FEtatZombie* Etat = Etats.Find(Zombie);
	if (Etat)
	{
		if (Etat->bMort)
		{
			return;
		}
		Etat->bMort = true;
		Etat->Vie = 0.f;
	}

	// On previent aussi le Blueprint (pour ses animations), sans toucher a son fichier
	EcrireBooleen(Zombie, { TEXT("isDead"), TEXT("IsDead") }, true);

	if (UCharacterMovementComponent* Mouvement = Zombie->GetCharacterMovement())
	{
		Mouvement->StopMovementImmediately();
		Mouvement->DisableMovement();
	}
	Zombie->OnTakeAnyDamage.RemoveDynamic(this, &AInvasionZombies::ZombieToucheQuelconque);
	Zombie->OnTakePointDamage.RemoveDynamic(this, &AInvasionZombies::ZombieTouchePrecis);
	Zombie->DetachFromControllerPendingDestroy();
	Zombie->SetLifeSpan(12.f);

	MulticastMort(Zombie, bTete, Direction);

	// Seulement les vraies morts au combat (pas l'effondrement de fin d'invasion)
	if (Tueur)
	{
		OnZombieTue.Broadcast(Zombie, Tueur, bTete);
	}
}

void AInvasionZombies::MulticastMort_Implementation(ACharacter* Zombie, bool bTete, FVector Direction)
{
	if (!IsValid(Zombie))
	{
		return;
	}
	if (UCapsuleComponent* Capsule = Zombie->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	USkeletalMeshComponent* Corps = Zombie->GetMesh();
	if (!Corps)
	{
		return;
	}
	if (UAnimInstance* Anim = Corps->GetAnimInstance())
	{
		Anim->StopAllMontages(0.1f);
	}

	// Chute en poupee de chiffon (la peau suit le corps invisible)
	Corps->SetCollisionProfileName(TEXT("Ragdoll"));
	Corps->SetSimulatePhysics(true);
	Corps->WakeAllRigidBodies();
	if (!Direction.IsNearlyZero())
	{
		Corps->AddImpulse(Direction.GetSafeNormal() * 350.f, NAME_None, true);
	}

	if (bTete && GetNetMode() != NM_DedicatedServer)
	{
		const FVector Tete = Corps->DoesSocketExist(TEXT("head")) ? Corps->GetSocketLocation(TEXT("head")) : Zombie->GetActorLocation() + FVector(0.f, 0.f, 70.f);
		if (UParticleSystem* Sang = LoadObject<UParticleSystem>(nullptr, CHEMIN_SANG))
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Sang, Tete, FRotator::ZeroRotator, FVector(1.5f));
		}
		if (USoundBase* Son = LoadObject<USoundBase>(nullptr, CHEMIN_SON_TETE))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Son, Tete);
		}
	}
}
