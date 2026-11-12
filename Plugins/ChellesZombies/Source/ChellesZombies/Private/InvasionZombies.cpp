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

#include <initializer_list>

#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include "SAlerteZombies.h"
#include "SireneChelles.h"

#define CHEMIN_MONTAGE_ATTAQUE TEXT("/Game/FPS_Game/Character/TPP/AnimStarterPack/Zombie/ZombieAttackMontage.ZombieAttackMontage")
#define CHEMIN_SANG TEXT("/Game/FPS_Game/Particles/P_body_bullet_NoStop.P_body_bullet_NoStop")
#define CHEMIN_SON_TETE TEXT("/Game/FPS_Game/Sounds/SFX/CharacterRelated/Other/Headshot.Headshot")

// Zombies de feu : effets deja presents dans le projet (StarterContent / FPS_Game)
#define CHEMIN_YEUX TEXT("/Game/Chelles/Zombies/M_YeuxZombies.M_YeuxZombies")
#define CHEMIN_FORME_DE_BASE TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")

// Ou bruler sur le corps (os du mannequin), dans l'ordre : FlammesParZombie= garde les N premiers
static const TCHAR* const OsEnFlammes[] = { TEXT("spine_02"), TEXT("head"), TEXT("lowerarm_r"), TEXT("lowerarm_l"), TEXT("calf_l"), TEXT("calf_r") };
static const float TailleFlammes[] = { 0.55f, 0.32f, 0.3f, 0.3f, 0.3f, 0.3f };

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

	/** Ce qu'on voit du zombie : sa peau DecayedZombieB, sinon son corps. */
	USkeletalMeshComponent* VisuelDe(ACharacter* Zombie)
	{
		TArray<USkeletalMeshComponent*> Composants;
		Zombie->GetComponents<USkeletalMeshComponent>(Composants);
		for (USkeletalMeshComponent* C : Composants)
		{
			if (C && C->GetFName() == NomPeau)
			{
				return C;
			}
		}
		return Zombie->GetMesh();
	}

	template <typename T>
	T* ChargerPremier(std::initializer_list<const TCHAR*> Chemins)
	{
		for (const TCHAR* Chemin : Chemins)
		{
			if (T* Objet = LoadObject<T>(nullptr, Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Objet;
			}
		}
		return nullptr;
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

		// Zombies de feu
		Feu = ChargerPremier<UParticleSystem>({ TEXT("/Game/StarterContent/Particles/P_Fire.P_Fire"), TEXT("/Game/FPS_Game/Particles/P_Fire.P_Fire") });
		Fumee = ChargerPremier<UParticleSystem>({ TEXT("/Game/StarterContent/Particles/P_Smoke.P_Smoke"), TEXT("/Game/FPS_Game/Particles/P_Smoke.P_Smoke") });
		SonSortie = ChargerPremier<USoundBase>({ TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue"), TEXT("/Game/StarterContent/Audio/Explosion01.Explosion01") });
		SonFeu = ChargerPremier<USoundBase>({ TEXT("/Game/StarterContent/Audio/Fire01_Cue.Fire01_Cue"), TEXT("/Game/StarterContent/Audio/Fire01.Fire01") });
		FormeSphere = ChargerPremier<UStaticMesh>({ TEXT("/Engine/BasicShapes/Sphere.Sphere") });
		FormeCube = ChargerPremier<UStaticMesh>({ TEXT("/Engine/BasicShapes/Cube.Cube") });
		UMaterialInterface* DeBase = ChargerPremier<UMaterialInterface>({ CHEMIN_FORME_DE_BASE });
		MatYeux = ChargerPremier<UMaterialInterface>({ CHEMIN_YEUX });
		if (!MatYeux && DeBase)
		{
			// Sans le materiau lumineux (Tools\Zombies\creer_yeux_zombies.py) : simples yeux rouges
			UMaterialInstanceDynamic* Rouge = UMaterialInstanceDynamic::Create(DeBase, this);
			Rouge->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.f, 0.f));
			MatYeux = Rouge;
			UE_LOG(LogTemp, Log, TEXT("[Zombies] %s absent : yeux rouges simples (lancez Tools\\Zombies\\creer_yeux_zombies.py pour des yeux lumineux)."), CHEMIN_YEUX);
		}
		if (DeBase)
		{
			MatTerre = UMaterialInstanceDynamic::Create(DeBase, this);
			MatTerre->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.11f, 0.065f, 0.035f));
		}
		if (!Feu)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Zombies] Particule P_Fire introuvable (StarterContent ou FPS_Game) : zombies sans flammes."));
		}

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
		else if (Cle == TEXT("DistanceApparitionMinMetres")) Reglages.DistanceMinMetres = FMath::Max(3.f, Nombre);
		else if (Cle == TEXT("DistanceApparitionMaxMetres")) Reglages.DistanceMaxMetres = FMath::Max(10.f, Nombre);
		else if (Cle == TEXT("VolumeSirene")) Reglages.VolumeSirene = FMath::Clamp(Nombre, 0.f, 4.f);
		else if (Cle == TEXT("ClasseZombie") && !Valeur.IsEmpty()) Reglages.ClasseZombie = Valeur;
		else if (Cle == TEXT("PeauZombie")) Reglages.PeauZombie = Valeur;
		else if (Cle == TEXT("Message") && !Valeur.IsEmpty()) Reglages.Message = Valeur;
		else if (Cle == TEXT("SortieDeTerre")) Reglages.bSortieDeTerre = EstOui(Valeur);
		else if (Cle == TEXT("DureeSortieSecondes")) Reglages.DureeSortieSecondes = FMath::Clamp(Nombre, 0.5f, 6.f);
		else if (Cle == TEXT("FlammesParZombie")) Reglages.FlammesParZombie = FMath::Clamp((int32)Nombre, 0, (int32)UE_ARRAY_COUNT(OsEnFlammes));
		else if (Cle == TEXT("LueurDuFeu")) Reglages.bLueurDuFeu = EstOui(Valeur);
		else if (Cle == TEXT("YeuxRouges")) Reglages.bYeuxRouges = EstOui(Valeur);
		else if (Cle == TEXT("DistanceEffetsMetres")) Reglages.DistanceEffetsMetres = FMath::Clamp(Nombre, 10.f, 300.f);
		else if (Cle == TEXT("YeuxHauteur")) Reglages.YeuxHauteur = FMath::Clamp(Nombre, -30.f, 40.f);
		else if (Cle == TEXT("YeuxAvant")) Reglages.YeuxAvant = FMath::Clamp(Nombre, -30.f, 40.f);
		else if (Cle == TEXT("YeuxEcart")) Reglages.YeuxEcart = FMath::Clamp(Nombre, 0.f, 15.f);
		else if (Cle == TEXT("TailleYeux")) Reglages.TailleYeux = FMath::Clamp(Nombre, 0.1f, 5.f);
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
		// Invasion : chaque zombie sort de terre a un endroit au hasard autour d'un joueur (8 a 40 m par defaut)
		if (!bModeVague)
		{
			TArray<APawn*> Joueurs;
			JoueursVivants(Joueurs);
			for (int32 Essai = 0; Essai < 4 && !bFait && Joueurs.Num() > 0; ++Essai)
			{
				APawn* Autour = Joueurs[FMath::RandRange(0, Joueurs.Num() - 1)];
				FVector Sol;
				if (TrouverPointApparition(Autour->GetActorLocation(), Reglages.DistanceMinMetres * 100.f, Reglages.DistanceMaxMetres * 100.f, Sol)
					&& FaireApparaitreZombie(Sol))
				{
					bFait = true;
				}
			}
		}
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

	// Les zombies vivants poursuivent et attaquent.
	// ATTENTION : un coup de zombie previent les autres plugins (OnCoupSurJoueur) et applique des degats.
	// Le mode survie peut alors terminer la partie (joueur a terre -> fin de vague), ce qui vide
	// Zombies et Etats PENDANT cette boucle. On parcourt donc une copie de la liste, on relit l'etat
	// a chaque zombie, et on s'arrete des que l'invasion est finie (sinon : crash "Index >= 0").
	Zombies.RemoveAll([this](ACharacter* Z) { return !IsValid(Z) || !Etats.Contains(Z); });
	const TArray<ACharacter*> AParcourir = Zombies;
	int32 Vivants = 0;
	for (ACharacter* Z : AParcourir)
	{
		if (!bInvasionEnCours)
		{
			return; // la partie a ete terminee par un abonne pendant cette image
		}
		FEtatZombie* Etat = IsValid(Z) ? Etats.Find(Z) : nullptr;
		if (!Etat || Etat->bMort)
		{
			continue;
		}
		if (Z->GetActorLocation().Z < -100000.f)
		{
			Z->Destroy();
			Zombies.Remove(Z);
			continue;
		}
		++Vivants;
		PiloterZombie(Z, *Etat, Maintenant);
	}
	if (!bInvasionEnCours)
	{
		return;
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
	TickEffets(DeltaSeconds);

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
	FEtatZombie& Ajoute = Etats.Add(Zombie, Etat);
	// Il sort de terre : immobile le temps de la montee (les PC le voient monter quand ils l'habillent)
	CommencerSortie(Zombie, Ajoute, false);
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
		// pas de peau DecayedZombieB : on garde l'apparence du Blueprint
		Embraser(Zombie);
		SortirDeTerre(Zombie);
		return;
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

	// Zombie de feu : flammes, yeux rouges, puis il sort de terre
	Embraser(Zombie);
	SortirDeTerre(Zombie);
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

	// 1 bis) Il sort encore de terre : il ne bouge pas et n'attaque pas
	if (Maintenant < Etat.SortieJusqua)
	{
		return;
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
				if (!bGereAilleurs && bInvasionEnCours && IsValid(Victime) && IsValid(Zombie))
				{
					UGameplayStatics::ApplyDamage(Victime, Degats, Zombie->GetController(), Zombie, UDamageType::StaticClass());
				}
				// Le coup a pu terminer la partie (Etats vide) ou tuer ce zombie : "Etat" n'est alors plus utilisable
				if (!bInvasionEnCours || !IsValid(Zombie) || Etats.Find(Zombie) != &Etat || Etat.bMort)
				{
					return;
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
			CommencerSortie(Zombie, Etat, true); // il ressort de terre plus pres
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
				const float Proche = Reglages.DistanceMinMetres * 100.f;
				if (TrouverPointApparition(Cible->GetActorLocation(), Proche, FMath::Max(1500.f, Proche + 500.f), Sol))
				{
					const float DemiHauteur = Zombie->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					Zombie->SetActorLocation(Sol + FVector(0.f, 0.f, DemiHauteur + 5.f), false, nullptr, ETeleportType::TeleportPhysics);
					CommencerSortie(Zombie, Etat, true); // il ressort de terre ailleurs
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

	// Plus de flammes ni d'yeux rouges (et s'il sortait encore de terre, il s'arrete la)
	EteindreZombie(Zombie);

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

// ===================================================================================
//  Zombies de feu : sortie de terre, flammes, yeux rouges
//  (seulement de l'affichage, sur chaque PC : le serveur ne fait que les garder immobiles
//   le temps de la sortie)
// ===================================================================================

void AInvasionZombies::CommencerSortie(ACharacter* Zombie, FEtatZombie& Etat, bool bPrevenirLesJoueurs)
{
	if (!Reglages.bSortieDeTerre || !IsValid(Zombie))
	{
		return;
	}
	const float Maintenant = GetWorld()->GetTimeSeconds();
	// + 0,3 s : le temps que le zombie arrive chez les autres joueurs en multijoueur
	Etat.SortieJusqua = Maintenant + Reglages.DureeSortieSecondes + 0.3f;
	Etat.ProchainControle = Etat.SortieJusqua + 1.f;
	Etat.DernierePosition = Zombie->GetActorLocation();
	Etat.Bloque = 0;
	Etat.ContournerJusqua = 0.f;
	if (UCharacterMovementComponent* Mouvement = Zombie->GetCharacterMovement())
	{
		Mouvement->StopMovementImmediately();
	}
	if (bPrevenirLesJoueurs)
	{
		MulticastSortie(Zombie);
	}
}

void AInvasionZombies::MulticastSortie_Implementation(ACharacter* Zombie)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(Zombie) || !Zombie->Tags.Contains(MarqueHabille))
	{
		return; // pas encore habille ici : il sortira de terre quand il le sera
	}
	SortirDeTerre(Zombie);
}

void AInvasionZombies::Embraser(ACharacter* Zombie)
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(Zombie))
	{
		return;
	}
	USkeletalMeshComponent* Visuel = VisuelDe(Zombie);
	if (!Visuel)
	{
		return;
	}
	FEffetsZombie& E = Effets.FindOrAdd(Zombie);
	E.Visuel = Visuel;
	E.Phase = FMath::FRandRange(0.f, 20.f);

	// 1) Flammes sur le corps (elles restent droites, le feu monte vers le ciel)
	if (Feu)
	{
		const int32 Nombre = FMath::Min(Reglages.FlammesParZombie, (int32)UE_ARRAY_COUNT(OsEnFlammes));
		for (int32 i = 0; i < Nombre; ++i)
		{
			const FName Os(OsEnFlammes[i]);
			if (!Visuel->DoesSocketExist(Os))
			{
				continue;
			}
			UParticleSystemComponent* Flamme = UGameplayStatics::SpawnEmitterAttached(Feu, Visuel, Os, FVector::ZeroVector, FRotator::ZeroRotator,
				FVector(TailleFlammes[i]), EAttachLocation::SnapToTarget, false);
			if (Flamme)
			{
				Flamme->SetUsingAbsoluteRotation(true);
				Flamme->SetWorldRotation(FRotator::ZeroRotator);
				E.Flammes.Add(Flamme);
			}
		}
	}

	// 2) Lueur orange du feu (petite lampe sans ombre, seulement pres du joueur)
	if (Reglages.bLueurDuFeu)
	{
		UPointLightComponent* Lueur = NewObject<UPointLightComponent>(Zombie);
		const FName Poitrine(TEXT("spine_03"));
		if (Visuel->DoesSocketExist(Poitrine))
		{
			Lueur->SetupAttachment(Visuel, Poitrine);
		}
		else
		{
			Lueur->SetupAttachment(Visuel);
			Lueur->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
		}
		Lueur->SetMobility(EComponentMobility::Movable);
		Lueur->SetCastShadows(false);
		Lueur->bAffectTranslucentLighting = false;
		Lueur->SetLightColor(FLinearColor(1.f, 0.38f, 0.08f));
		Lueur->SetIntensity(3000.f);
		Lueur->SetAttenuationRadius(320.f);
		Lueur->RegisterComponent();
		Zombie->AddInstanceComponent(Lueur);
		E.Lueur = Lueur;
		E.bLueurAllumee = true;
	}

	// 3) Deux yeux rouges lumineux, colles a l'os de la tete (ils suivent tous ses mouvements)
	const FName Tete(TEXT("head"));
	USkeletalMesh* Modele = Visuel->SkeletalMesh;
	if (Reglages.bYeuxRouges && FormeSphere && MatYeux && Modele && Visuel->DoesSocketExist(Tete) && Modele->RefSkeleton.FindBoneIndex(Tete) != INDEX_NONE)
	{
		// Position calculee dans la pose de reference du mannequin (il regarde vers +Y, la tete en haut)
		const FTransform TeteRepos(Modele->GetComposedRefPoseMatrix(Tete));
		for (const float Cote : { -1.f, 1.f })
		{
			const FVector OeilRepos = TeteRepos.GetLocation() + FVector(Cote * Reglages.YeuxEcart, Reglages.YeuxAvant, Reglages.YeuxHauteur);
			UStaticMeshComponent* Oeil = NewObject<UStaticMeshComponent>(Zombie);
			Oeil->SetStaticMesh(FormeSphere);
			Oeil->SetMaterial(0, MatYeux);
			Oeil->SetMobility(EComponentMobility::Movable);
			Oeil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Oeil->SetGenerateOverlapEvents(false);
			Oeil->SetCastShadow(false);
			Oeil->SetupAttachment(Visuel, Tete);
			Oeil->SetRelativeLocation(TeteRepos.InverseTransformPosition(OeilRepos));
			Oeil->SetUsingAbsoluteScale(true);
			Oeil->SetRelativeScale3D(FVector(0.028f * Reglages.TailleYeux));
			Oeil->RegisterComponent();
			Zombie->AddInstanceComponent(Oeil);
			E.Yeux.Add(Oeil);
		}
	}
}

void AInvasionZombies::SortirDeTerre(ACharacter* Zombie)
{
	if (GetNetMode() == NM_DedicatedServer || !Reglages.bSortieDeTerre || !IsValid(Zombie))
	{
		return;
	}
	FEffetsZombie& E = Effets.FindOrAdd(Zombie);
	USkeletalMeshComponent* Visuel = E.Visuel.Get();
	if (!Visuel)
	{
		Visuel = VisuelDe(Zombie);
		E.Visuel = Visuel;
	}
	if (!Visuel)
	{
		return;
	}
	if (!E.bBaseConnue)
	{
		E.BaseRelative = Visuel->GetRelativeLocation();
		E.bBaseConnue = true;
	}
	const float DemiHauteur = Zombie->GetCapsuleComponent() ? Zombie->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
	E.Profondeur = DemiHauteur * 2.f + 15.f; // toute sa hauteur : la tete commence sous le sol
	E.AgeSortie = 0.f;
	E.ProchaineMotte = 0.f;
	E.Sol = Zombie->GetActorLocation() - FVector(0.f, 0.f, DemiHauteur);
	Visuel->SetRelativeLocation(E.BaseRelative - FVector(0.f, 0.f, E.Profondeur));

	// Effets de terre et sons seulement s'il est assez pres pour etre vu
	bool bProche = true;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			bProche = FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(), E.Sol) < FMath::Square(Reglages.DistanceEffetsMetres * 100.f);
		}
	}
	E.bProche = bProche;
	if (!bProche)
	{
		return;
	}
	LancerMotte(E.Sol, true); // butte de terre qui se souleve
	if (Fumee)
	{
		if (UParticleSystemComponent* Nuage = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Fumee, E.Sol, FRotator::ZeroRotator, FVector(0.8f), true))
		{
			TWeakObjectPtr<UParticleSystemComponent> Faible(Nuage);
			FTimerHandle Minuteur;
			GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
			{
				if (Faible.IsValid())
				{
					Faible->DeactivateSystem();
				}
			}), FMath::Max(0.8f, Reglages.DureeSortieSecondes * 0.6f), false);
		}
	}
	if (SonSortie)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SonSortie, E.Sol, 0.3f, 0.45f); // grondement sourd
	}
	if (SonFeu)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SonFeu, E.Sol, 0.5f, 1.f);
	}
}

void AInvasionZombies::EteindreZombie(ACharacter* Zombie)
{
	FEffetsZombie* E = Effets.Find(Zombie);
	if (!E)
	{
		return;
	}
	// S'il etait encore sous terre, il reprend sa place avant de tomber
	if (E->AgeSortie >= 0.f && E->Visuel.IsValid())
	{
		E->Visuel->SetRelativeLocation(E->BaseRelative);
	}
	if (E->Lueur.IsValid())
	{
		E->Lueur->DestroyComponent();
	}
	for (const TWeakObjectPtr<UStaticMeshComponent>& Oeil : E->Yeux)
	{
		if (Oeil.IsValid())
		{
			Oeil->DestroyComponent(); // yeux eteints : il est mort
		}
	}
	// Le corps brule encore un peu, puis le feu s'eteint
	const TArray<TWeakObjectPtr<UParticleSystemComponent>> Flammes = E->Flammes;
	if (Flammes.Num() > 0 && GetWorld())
	{
		FTimerHandle Minuteur;
		GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Flammes]()
		{
			for (const TWeakObjectPtr<UParticleSystemComponent>& F : Flammes)
			{
				if (F.IsValid())
				{
					F->DeactivateSystem();
				}
			}
		}), 3.f, false);
	}
	Effets.Remove(Zombie);
}

void AInvasionZombies::LancerMotte(const FVector& Sol, bool bButte)
{
	if (!FormeSphere || !FormeCube || Mottes.Num() > 160)
	{
		return;
	}
	UStaticMeshComponent* Forme = NewObject<UStaticMeshComponent>(this);
	Forme->SetStaticMesh(bButte ? FormeSphere : FormeCube);
	if (MatTerre)
	{
		Forme->SetMaterial(0, MatTerre);
	}
	Forme->SetMobility(EComponentMobility::Movable);
	Forme->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Forme->SetGenerateOverlapEvents(false);
	Forme->SetCastShadow(false);
	Forme->RegisterComponent();

	FMotteTerre M;
	M.Forme = Forme;
	M.bButte = bButte;
	M.SolZ = Sol.Z + 3.f;
	if (bButte)
	{
		M.Position = Sol;
		M.Taille = FVector(1.3f, 1.3f, 0.22f);
		M.Vie = Reglages.DureeSortieSecondes + 2.5f;
		Forme->SetWorldRotation(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	}
	else
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const FVector Dehors(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		M.Position = Sol + Dehors * 20.f + FVector(0.f, 0.f, 5.f);
		M.Vitesse = Dehors * FMath::FRandRange(80.f, 220.f) + FVector(0.f, 0.f, FMath::FRandRange(250.f, 480.f));
		M.Taille = FVector(FMath::FRandRange(0.05f, 0.11f));
		M.Vie = FMath::FRandRange(1.2f, 1.8f);
		Forme->SetWorldRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
	}
	Forme->SetWorldLocation(M.Position);
	Forme->SetWorldScale3D(M.Taille * 0.01f);
	Mottes.Add(M);
}

void AInvasionZombies::TickEffets(float DeltaSeconds)
{
	UWorld* W = GetWorld();
	const float Maintenant = W->GetTimeSeconds();

	FVector Camera = FVector::ZeroVector;
	bool bCamera = false;
	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			Camera = PC->PlayerCameraManager->GetCameraLocation();
			bCamera = true;
		}
	}
	const bool bControle = Maintenant >= ProchainControleEffets;
	if (bControle)
	{
		ProchainControleEffets = Maintenant + 0.4f;
	}
	const float DistEffets = Reglages.DistanceEffetsMetres * 100.f;
	const float DistLueur = FMath::Min(DistEffets, 2500.f);

	for (auto It = Effets.CreateIterator(); It; ++It)
	{
		ACharacter* Z = It.Key().Get();
		if (!IsValid(Z))
		{
			It.RemoveCurrent();
			continue;
		}
		FEffetsZombie& E = It.Value();

		// Optimisation : loin du joueur, flammes et lueur eteintes (on ne les voit plus)
		if (bControle && bCamera)
		{
			const float DistCarre = FVector::DistSquared(Camera, Z->GetActorLocation());
			const bool bProche = DistCarre < FMath::Square(DistEffets);
			if (bProche != E.bProche)
			{
				E.bProche = bProche;
				for (const TWeakObjectPtr<UParticleSystemComponent>& F : E.Flammes)
				{
					if (F.IsValid())
					{
						if (bProche)
						{
							F->ActivateSystem(true);
						}
						else
						{
							F->DeactivateSystem();
						}
					}
				}
			}
			const bool bLueur = DistCarre < FMath::Square(DistLueur);
			if (E.Lueur.IsValid() && bLueur != E.bLueurAllumee)
			{
				E.bLueurAllumee = bLueur;
				E.Lueur->SetVisibility(bLueur);
			}
		}

		// La lueur du feu vacille
		if (E.bLueurAllumee && E.Lueur.IsValid())
		{
			const float T = Maintenant * 9.f + E.Phase;
			E.Lueur->SetIntensity(3000.f * (0.75f + 0.15f * FMath::Sin(T) + 0.1f * FMath::Sin(T * 2.7f + 1.3f)));
		}

		// Sortie de terre : le corps monte (vite au debut puis plus lentement) en tremblant un peu
		if (E.AgeSortie >= 0.f)
		{
			E.AgeSortie += DeltaSeconds;
			const float A = FMath::Clamp(E.AgeSortie / Reglages.DureeSortieSecondes, 0.f, 1.f);
			const float Monte = 1.f - FMath::Pow(1.f - A, 2.2f);
			FVector Tremble = FVector::ZeroVector;
			if (A < 1.f)
			{
				Tremble = FVector(FMath::FRandRange(-1.5f, 1.5f), FMath::FRandRange(-1.5f, 1.5f), 0.f) * (1.f - A);
			}
			if (USkeletalMeshComponent* V = E.Visuel.Get())
			{
				V->SetRelativeLocation(E.BaseRelative + FVector(0.f, 0.f, -E.Profondeur * (1.f - Monte)) + Tremble);
			}
			// Mottes de terre projetees pendant qu'il sort
			if (E.bProche && A < 0.75f && E.AgeSortie >= E.ProchaineMotte)
			{
				E.ProchaineMotte = E.AgeSortie + 0.12f;
				LancerMotte(E.Sol, false);
				LancerMotte(E.Sol, false);
			}
			if (A >= 1.f)
			{
				E.AgeSortie = -1.f;
			}
		}
	}

	// Mottes et buttes de terre
	for (int32 i = Mottes.Num() - 1; i >= 0; --i)
	{
		FMotteTerre& M = Mottes[i];
		UStaticMeshComponent* Forme = M.Forme.Get();
		M.Age += DeltaSeconds;
		if (!Forme || M.Age >= M.Vie)
		{
			if (Forme)
			{
				Forme->DestroyComponent();
			}
			Mottes.RemoveAtSwap(i);
			continue;
		}
		float Echelle = 1.f;
		if (M.bButte)
		{
			// Se souleve vite, reste, puis s'affaisse dans le sol
			const float Fin = FMath::Clamp((M.Vie - M.Age) / 1.5f, 0.f, 1.f);
			Echelle = FMath::Clamp(M.Age / 0.3f, 0.f, 1.f) * Fin;
			Forme->SetWorldLocation(M.Position - FVector(0.f, 0.f, (1.f - Fin) * 20.f));
		}
		else
		{
			M.Vitesse.Z -= 980.f * DeltaSeconds;
			M.Position += M.Vitesse * DeltaSeconds;
			if (M.Position.Z < M.SolZ)
			{
				M.Position.Z = M.SolZ;
				M.Vitesse = FVector::ZeroVector;
			}
			Forme->SetWorldLocation(M.Position);
			Echelle = FMath::Clamp((M.Vie - M.Age) / 0.5f, 0.f, 1.f);
		}
		Forme->SetWorldScale3D(M.Taille * FMath::Max(Echelle, 0.01f));
	}
}
