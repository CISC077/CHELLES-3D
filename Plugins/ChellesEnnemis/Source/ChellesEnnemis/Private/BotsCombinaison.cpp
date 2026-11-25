// CHELLES 3D - Les 5 bots ennemis en combinaison Sci-Fi (voir BotsCombinaison.h)
//
// Rien n'est ecrit dans les fichiers du projet : tout se passe pendant le jeu.
//  - Serveur (ou solo) : pose les bots, decide du vol, des tirs, des pouvoirs et des degats.
//  - Chaque PC : habille les bots (combinaison de leur couleur), flammes sous les pieds en vol,
//    nom au-dessus de la tete, effets (explosions, glace, foudre...).
#include "BotsCombinaison.h"

#include <initializer_list>

#include "AIController.h"
#include "BrainComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace ChellesBots
{
	enum EArme : uint8
	{
		ArmeLaser = 0,    // rafale de 3 tirs laser violets
		ArmeFlammes,      // lance-flammes (courte portee)
		ArmeEclairs,      // fusil a eclairs (tres rapide)
		ArmeGlace,        // canon a glace (ralentit le joueur)
		ArmeRoquettes,    // lance-roquettes (explose)
		NombreArmes
	};

	enum EPouvoir : uint8
	{
		PouvoirTeleportation = 0, // reapparait dans le dos du joueur
		PouvoirPluieDeFeu,        // boules de feu qui tombent du ciel autour du joueur
		PouvoirFoudre,            // la foudre tombe sur le joueur (1 s pour s'ecarter)
		PouvoirGel,               // gele le joueur sur place
		PouvoirOndeDeChoc,        // pique du ciel et projette les joueurs autour
		NombrePouvoirs
	};

	enum EEffet : uint8
	{
		EffetImpact = 0,
		EffetFlammes,
		EffetExplosion,
		EffetGlace,
		EffetFoudre,
		EffetFumee,
		EffetMarqueFoudre,
		EffetOnde
	};

	/** Caracteristiques de chaque arme. */
	struct FArmeInfo
	{
		float Cadence;      // secondes entre deux rafales
		int32 Rafale;       // tirs par rafale
		float Intervalle;   // secondes entre deux tirs d'une rafale
		float Vitesse;      // cm/s
		float Taille;       // taille de la boule
		float Pourcent;     // % de la vie du joueur
		float Rayon;        // cm (0 = seulement le joueur touche)
		float Ralentir;     // secondes de ralentissement (glace)
		float PorteeMax;    // metres (0 = PorteeArmeMetres)
		float Dispersion;   // cm a 10 m
		uint8 Effet;
		FLinearColor Couleur;
	};

	const FArmeInfo& Info(uint8 Arme)
	{
		static const FArmeInfo Liste[NombreArmes] = {
			{ 2.2f, 3, 0.12f, 9000.f, 0.18f, 5.f, 0.f, 0.f, 0.f, 40.f, EffetImpact, FLinearColor(0.65f, 0.15f, 1.f) },
			{ 2.6f, 7, 0.07f, 2200.f, 0.35f, 2.f, 150.f, 0.f, 12.f, 90.f, EffetFlammes, FLinearColor(1.f, 0.35f, 0.02f) },
			{ 1.8f, 1, 0.f, 15000.f, 0.15f, 9.f, 0.f, 0.f, 0.f, 25.f, EffetFoudre, FLinearColor(1.f, 0.95f, 0.3f) },
			{ 2.6f, 1, 0.f, 3500.f, 0.35f, 7.f, 120.f, 3.f, 0.f, 30.f, EffetGlace, FLinearColor(0.55f, 0.85f, 1.f) },
			{ 4.0f, 1, 0.f, 2600.f, 0.40f, 16.f, 450.f, 0.f, 0.f, 30.f, EffetExplosion, FLinearColor(1.f, 0.55f, 0.1f) },
		};
		return Liste[FMath::Min<uint8>(Arme, NombreArmes - 1)];
	}
}

using namespace ChellesBots;

const FName ABotsCombinaison::MarqueBot(TEXT("ChellesBotCombinaison"));

static const FName BotMarqueVuParEnnemis(TEXT("ChellesEnnemiVu")); // AEnnemisChelles ne l'habille pas en humain
static const FName BotMarqueJoueurHorsJeu(TEXT("ChellesHorsJeu"));
static const FName BotNomTenueBot(TEXT("TenueBotChelles"));
static const FName BotNomTenueHumain(TEXT("TenueEnnemiChelles"));
static const FName BotNomEtiquette(TEXT("NomBotChelles"));
static const FString BotRaisonVol(TEXT("ChellesBotVol"));

namespace
{
	// --------- Variables des Blueprints (lues sans modifier les fichiers) ---------

	FProperty* BotTrouverVariable(UObject* Objet, std::initializer_list<const TCHAR*> Noms)
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

	bool BotLireNombre(UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(BotTrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? (float)P->GetFloatingPointPropertyValue(Adresse) : (float)P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	void BotMultiplierNombre(UObject* Objet, std::initializer_list<const TCHAR*> Noms, float Facteur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(BotTrouverVariable(Objet, Noms));
		if (!P)
		{
			return;
		}
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Adresse, P->GetFloatingPointPropertyValue(Adresse) * Facteur);
		}
		else
		{
			P->SetIntPropertyValue(Adresse, (int64)FMath::RoundToInt((float)P->GetSignedIntPropertyValue(Adresse) * Facteur));
		}
	}

	bool BotLireBooleen(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(BotTrouverVariable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	bool BotEstVivant(APawn* Joueur)
	{
		if (!IsValid(Joueur) || Joueur->Tags.Contains(BotMarqueJoueurHorsJeu))
		{
			return false;
		}
		bool bMort = false;
		if (BotLireBooleen(Joueur, { TEXT("IsDead"), TEXT("isDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
		{
			return false;
		}
		float Vie = 1.f;
		if (BotLireNombre(Joueur, { TEXT("HealthCurrent"), TEXT("Health") }, Vie) && Vie <= 0.f)
		{
			return false;
		}
		return true;
	}

	float BotVieMax(APawn* Joueur)
	{
		float Max = 100.f;
		if (BotLireNombre(Joueur, { TEXT("HealthMax"), TEXT("MaxHealth") }, Max) && Max > 0.f)
		{
			return Max;
		}
		return 100.f;
	}

	bool BotEstUnJoueur(const AActor* Acteur)
	{
		const APawn* Pion = Cast<APawn>(Acteur);
		return Pion && Pion->GetController() && Pion->GetController()->IsPlayerController();
	}

	bool BotEstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	FString BotCheminComplet(const FString& Chemin)
	{
		if (Chemin.IsEmpty() || Chemin.Contains(TEXT(".")))
		{
			return Chemin;
		}
		return Chemin + TEXT(".") + FPaths::GetBaseFilename(Chemin);
	}

	uint8 BotArmeDuTexte(const FString& T)
	{
		if (T.Contains(TEXT("FLAM"), ESearchCase::IgnoreCase)) return ArmeFlammes;
		if (T.Contains(TEXT("ECLAIR"), ESearchCase::IgnoreCase) || T.Contains(TEXT("FOUDRE"), ESearchCase::IgnoreCase)) return ArmeEclairs;
		if (T.Contains(TEXT("GLACE"), ESearchCase::IgnoreCase) || T.Contains(TEXT("GIVRE"), ESearchCase::IgnoreCase)) return ArmeGlace;
		if (T.Contains(TEXT("ROQUET"), ESearchCase::IgnoreCase) || T.Contains(TEXT("ROCKET"), ESearchCase::IgnoreCase)) return ArmeRoquettes;
		return ArmeLaser;
	}

	uint8 BotPouvoirDuTexte(const FString& T)
	{
		if (T.Contains(TEXT("PLUIE"), ESearchCase::IgnoreCase) || T.Contains(TEXT("FEU"), ESearchCase::IgnoreCase)) return PouvoirPluieDeFeu;
		if (T.Contains(TEXT("FOUDRE"), ESearchCase::IgnoreCase)) return PouvoirFoudre;
		if (T.Contains(TEXT("GEL"), ESearchCase::IgnoreCase)) return PouvoirGel;
		if (T.Contains(TEXT("ONDE"), ESearchCase::IgnoreCase) || T.Contains(TEXT("CHOC"), ESearchCase::IgnoreCase)) return PouvoirOndeDeChoc;
		return PouvoirTeleportation;
	}

	// --------- Effets (particules et sons deja presents dans le projet) ---------

	template <typename T>
	T* BotChargerPremier(std::initializer_list<const TCHAR*> Chemins)
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

	template <typename T>
	struct FBotMemo
	{
		TWeakObjectPtr<T> Objet;
		bool bAbsent = false;

		T* Obtenir(std::initializer_list<const TCHAR*> Chemins)
		{
			if (!Objet.IsValid() && !bAbsent)
			{
				Objet = BotChargerPremier<T>(Chemins);
				bAbsent = !Objet.IsValid();
			}
			return Objet.Get();
		}
	};

#define CHELLES_BOT_EFFET(Type, Nom, ...) \
	Type* Nom() { static FBotMemo<Type> Memo; return Memo.Obtenir({ __VA_ARGS__ }); }

	CHELLES_BOT_EFFET(UParticleSystem, Flammes, TEXT("/Game/FPS_Game/Particles/P_Fire.P_Fire"), TEXT("/Game/StarterContent/Particles/P_Fire.P_Fire"))
	CHELLES_BOT_EFFET(UParticleSystem, Explosion, TEXT("/Game/FPS_Game/Particles/P_Explosion.P_Explosion"), TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion"))
	CHELLES_BOT_EFFET(UParticleSystem, Etincelles, TEXT("/Game/FPS_Game/Particles/P_Sparks.P_Sparks"), TEXT("/Game/StarterContent/Particles/P_Sparks.P_Sparks"))
	CHELLES_BOT_EFFET(UParticleSystem, Vapeur, TEXT("/Game/FPS_Game/Particles/P_Steam_Lit.P_Steam_Lit"), TEXT("/Game/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit"))
	CHELLES_BOT_EFFET(USoundBase, SonExplosion, TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue"), TEXT("/Game/StarterContent/Audio/Explosion01.Explosion01"))

#undef CHELLES_BOT_EFFET

	void BotBouffee(UWorld* Monde, UParticleSystem* Modele, const FVector& Ou, float Taille, float Duree)
	{
		if (!Monde || !Modele || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		UParticleSystemComponent* P = UGameplayStatics::SpawnEmitterAtLocation(Monde, Modele, Ou, FRotator::ZeroRotator, FVector(Taille), true);
		if (!P)
		{
			return;
		}
		TWeakObjectPtr<UParticleSystemComponent> Faible(P);
		FTimerHandle Minuteur;
		Monde->GetTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
		{
			if (Faible.IsValid())
			{
				Faible->DeactivateSystem();
			}
		}), Duree, false);
	}
}

// ===================================================================================
//  Reglages (Bots.txt)
// ===================================================================================

const FReglagesBots& FReglagesBots::Lire(bool bRelire)
{
	static FReglagesBots R;
	static bool bLu = false;
	if (bLu && !bRelire)
	{
		return R;
	}
	bLu = true;
	R = FReglagesBots();

	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesEnnemis"), TEXT("Bots.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[Bots] %s absent : reglages par defaut."), *Chemin);
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
		const float Nombre = FCString::Atof(*Valeur);

		if (Cle.Equals(TEXT("Bot"), ESearchCase::IgnoreCase))
		{
			// Bot=NOM|materiau|arme|pouvoir|R,G,B
			TArray<FString> Morceaux;
			Valeur.ParseIntoArray(Morceaux, TEXT("|"), false);
			if (Morceaux.Num() < 4)
			{
				continue;
			}
			for (FString& M : Morceaux)
			{
				M.TrimStartAndEndInline();
			}
			FTypeBot T;
			T.Nom = Morceaux[0];
			T.Materiau = Morceaux[1];
			T.Arme = BotArmeDuTexte(Morceaux[2]);
			T.Pouvoir = BotPouvoirDuTexte(Morceaux[3]);
			T.Couleur = Info(T.Arme).Couleur;
			if (Morceaux.Num() >= 5)
			{
				TArray<FString> RVB;
				Morceaux[4].ParseIntoArray(RVB, TEXT(","), true);
				if (RVB.Num() >= 3)
				{
					T.Couleur = FLinearColor(FCString::Atof(*RVB[0]), FCString::Atof(*RVB[1]), FCString::Atof(*RVB[2]));
				}
			}
			R.Types.Add(T);
		}
		else if (Cle == TEXT("Actif")) R.bActif = BotEstOui(Valeur);
		else if (Cle == TEXT("NombreDeChaque")) R.NombreDeChaque = FMath::Clamp(FCString::Atoi(*Valeur), 0, 10);
		else if (Cle == TEXT("Classe")) R.Classe = Valeur;
		else if (Cle == TEXT("Corps")) R.Corps = Valeur;
		else if (Cle == TEXT("ReapparitionSecondes")) R.ReapparitionSecondes = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("VieFois")) R.VieFois = FMath::Clamp(Nombre, 0.2f, 20.f);
		else if (Cle == TEXT("EspacementMetres")) R.EspacementMetres = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("LoinDesJoueursMetres")) R.LoinDesJoueursMetres = FMath::Max(0.f, Nombre);
		else if (Cle == TEXT("EcartNiveauRueMetres")) R.EcartNiveauRueMetres = FMath::Max(1.f, Nombre);
		else if (Cle == TEXT("DistanceDetectionMetres")) R.DistanceDetectionMetres = FMath::Max(5.f, Nombre);
		else if (Cle == TEXT("DistanceReposMetres")) R.DistanceReposMetres = FMath::Max(20.f, Nombre);
		else if (Cle == TEXT("Vol")) R.bVol = BotEstOui(Valeur);
		else if (Cle == TEXT("VolToutesLesSecondes")) R.VolToutesLesSecondes = FMath::Max(3.f, Nombre);
		else if (Cle == TEXT("DureeVol")) R.DureeVol = FMath::Clamp(Nombre, 1.f, 60.f);
		else if (Cle == TEXT("HauteurVolMetres")) R.HauteurVolMetres = FMath::Clamp(Nombre, 2.f, 50.f);
		else if (Cle == TEXT("VitesseVol")) R.VitesseVol = FMath::Clamp(Nombre, 100.f, 5000.f);
		else if (Cle == TEXT("PorteeArmeMetres")) R.PorteeArmeMetres = FMath::Max(5.f, Nombre);
		else if (Cle == TEXT("DegatsFois")) R.DegatsFois = FMath::Clamp(Nombre, 0.f, 10.f);
		else if (Cle == TEXT("RechargePouvoir")) R.RechargePouvoir = FMath::Max(2.f, Nombre);
		else if (Cle == TEXT("AfficherNom")) R.bAfficherNom = BotEstOui(Valeur);
	}
	R.DistanceReposMetres = FMath::Max(R.DistanceReposMetres, R.DistanceDetectionMetres + 10.f);

	if (R.Types.Num() == 0)
	{
		auto Ajouter = [](const TCHAR* Nom, const TCHAR* Mat, uint8 Arme, uint8 Pouvoir, FLinearColor C)
		{
			FTypeBot T;
			T.Nom = Nom;
			T.Materiau = Mat;
			T.Arme = Arme;
			T.Pouvoir = Pouvoir;
			T.Couleur = C;
			R.Types.Add(T);
		};
		Ajouter(TEXT("SPECTRE"), TEXT("/Game/SciFiSoldier/Materials/MI_SciFiSoldier.MI_SciFiSoldier"), ArmeLaser, PouvoirTeleportation, FLinearColor(0.65f, 0.2f, 1.f));
		Ajouter(TEXT("BRASIER"), TEXT("/Game/SciFiSoldier/Materials/MI_SciFiSoldier_2.MI_SciFiSoldier_2"), ArmeFlammes, PouvoirPluieDeFeu, FLinearColor(1.f, 0.2f, 0.05f));
		Ajouter(TEXT("FOUDRE"), TEXT("/Game/SciFiSoldier/Materials/MI_SciFiSoldier_3.MI_SciFiSoldier_3"), ArmeEclairs, PouvoirFoudre, FLinearColor(1.f, 0.85f, 0.1f));
		Ajouter(TEXT("GIVRE"), TEXT("/Game/SciFiSoldier/Materials/MI_SciFiSoldier_4.MI_SciFiSoldier_4"), ArmeGlace, PouvoirGel, FLinearColor(0.3f, 0.7f, 1.f));
		Ajouter(TEXT("TITAN"), TEXT("/Game/SciFiSoldier/Materials/MI_SciFiSoldier_5.MI_SciFiSoldier_5"), ArmeRoquettes, PouvoirOndeDeChoc, FLinearColor(0.5f, 0.6f, 0.25f));
	}
	return R;
}

// ===================================================================================
//  Projectile
// ===================================================================================

AProjectileBot::AProjectileBot()
{
	bReplicates = true;
	SetReplicatingMovement(true);
	InitialLifeSpan = 4.f;
	PrimaryActorTick.bCanEverTick = false;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(12.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Destructible, ECR_Block);
	Sphere->SetGenerateOverlapEvents(false);
	Sphere->CanCharacterStepUpOn = ECB_No;
	RootComponent = Sphere;

	Forme = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Forme"));
	Forme->SetupAttachment(Sphere);
	Forme->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Forme->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Boule(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Boule.Succeeded())
	{
		Forme->SetStaticMesh(Boule.Object);
	}

	Lumiere = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lumiere"));
	Lumiere->SetupAttachment(Sphere);
	Lumiere->SetIntensity(5000.f);
	Lumiere->SetAttenuationRadius(450.f);
	Lumiere->SetCastShadows(false);

	Mouvement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Mouvement"));
	Mouvement->UpdatedComponent = Sphere;
	Mouvement->InitialSpeed = 3000.f;
	Mouvement->MaxSpeed = 20000.f;
	Mouvement->ProjectileGravityScale = 0.f;
	Mouvement->bRotationFollowsVelocity = true;
	Mouvement->bShouldBounce = false;
}

void AProjectileBot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AProjectileBot, Couleur);
	DOREPLIFETIME(AProjectileBot, Taille);
	DOREPLIFETIME(AProjectileBot, Vitesse);
}

void AProjectileBot::Preparer(uint8 InEffet, float InVitesse, float InTaille, FLinearColor InCouleur, float InDegatsPourcent, float InRayon, float InGelSecondes)
{
	Effet = InEffet;
	Vitesse = InVitesse;
	Taille = InTaille;
	Couleur = InCouleur;
	DegatsPourcent = InDegatsPourcent;
	Rayon = InRayon;
	GelSecondes = InGelSecondes;
	Mouvement->InitialSpeed = InVitesse;
	Mouvement->MaxSpeed = FMath::Max(InVitesse, 100.f);
}

void AProjectileBot::BeginPlay()
{
	Super::BeginPlay();
	AppliquerApparence();
	if (HasAuthority())
	{
		Sphere->OnComponentHit.AddDynamic(this, &AProjectileBot::AuChoc);
	}
	else
	{
		// Chez les joueurs : la boule avance toute seule entre deux nouvelles du serveur
		Mouvement->Velocity = GetActorForwardVector() * Vitesse;
	}
}

void AProjectileBot::AppliquerApparence()
{
	Forme->SetRelativeScale3D(FVector(Taille));
	Lumiere->SetLightColor(Couleur);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, this);
		Mid->SetVectorParameterValue(TEXT("Color"), Couleur * 4.f);
		Forme->SetMaterial(0, Mid);
	}
}

void AProjectileBot::AuChoc(UPrimitiveComponent* Composant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact)
{
	Exploser(Impact.ImpactPoint, Autre);
}

void AProjectileBot::Exploser(const FVector& Ou, AActor* ToucheDirect)
{
	if (bFini || !HasAuthority())
	{
		return;
	}
	bFini = true;
	ABotsCombinaison* Chef = ABotsCombinaison::Trouver(GetWorld());
	if (Chef)
	{
		Chef->MulticastEffet(Effet, Ou, FMath::Max(0.6f, Rayon / 250.f));
	}
	AActor* Auteur = GetOwner();

	APawn* Direct = BotEstUnJoueur(ToucheDirect) ? Cast<APawn>(ToucheDirect) : nullptr;
	if (Direct && BotEstVivant(Direct))
	{
		ABotsCombinaison::BlesserJoueur(Direct, DegatsPourcent, Auteur);
		if (GelSecondes > 0.f && Chef)
		{
			Chef->MulticastFreiner(Direct, 0.4f, GelSecondes);
		}
	}
	if (Rayon > 0.f && Chef)
	{
		TArray<APawn*> Joueurs;
		Chef->JoueursVivants(Joueurs);
		for (APawn* J : Joueurs)
		{
			if (J == Direct)
			{
				continue;
			}
			if (FVector::Dist(J->GetActorLocation(), Ou) <= Rayon + 40.f)
			{
				ABotsCombinaison::BlesserJoueur(J, DegatsPourcent * 0.7f, Auteur);
				if (GelSecondes > 0.f)
				{
					Chef->MulticastFreiner(J, 0.4f, GelSecondes);
				}
			}
		}
	}
	Destroy();
}

// ===================================================================================
//  Le chef des bots
// ===================================================================================

ABotsCombinaison::ABotsCombinaison()
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

void ABotsCombinaison::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABotsCombinaison, Bots);
}

ABotsCombinaison* ABotsCombinaison::Trouver(const UWorld* Monde)
{
	if (!Monde)
	{
		return nullptr;
	}
	for (TActorIterator<ABotsCombinaison> It(const_cast<UWorld*>(Monde)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

bool ABotsCombinaison::EstUnBot(const ACharacter* Pion)
{
	if (!Pion)
	{
		return false;
	}
	if (Pion->Tags.Contains(MarqueBot))
	{
		return true;
	}
	if (ABotsCombinaison* Chef = Trouver(Pion->GetWorld()))
	{
		for (const FBotRep& B : Chef->Bots)
		{
			if (B.Pion == Pion)
			{
				return true;
			}
		}
	}
	return false;
}

void ABotsCombinaison::BeginPlay()
{
	Super::BeginPlay();
	const FReglagesBots& R = FReglagesBots::Lire(true);
	if (!R.bActif || R.Types.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Bots] Desactives (Actif=Non dans Bots.txt)."));
		SetActorTickEnabled(false);
		return;
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		CorpsSciFi = LoadObject<USkeletalMesh>(nullptr, *BotCheminComplet(R.Corps), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!CorpsSciFi)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bots] Combinaison introuvable : %s"), *R.Corps);
		}
		for (const FTypeBot& T : R.Types)
		{
			UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, *BotCheminComplet(T.Materiau), nullptr, LOAD_NoWarn | LOAD_Quiet);
			MateriauxTypes.Add(M);
			if (!M)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Bots] Couleur introuvable pour %s : %s"), *T.Nom, *T.Materiau);
			}
		}
	}
	if (EstServeur())
	{
		ClasseBot = LoadClass<ACharacter>(nullptr, *R.Classe, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!ClasseBot)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bots] Classe du soldat introuvable : %s (aucun bot pose)."), *R.Classe);
		}
		PoseA = GetWorld()->GetTimeSeconds() + 4.f; // apres les ennemis de ChellesEnnemis
	}
}

void ABotsCombinaison::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Maintenant = GetWorld()->GetTimeSeconds();

	if (EstServeur())
	{
		if (!bPoseFaite && Maintenant >= PoseA && ClasseBot)
		{
			bPoseFaite = true;
			PreparerPlaces();
			PoserTout();
		}
		if (Maintenant >= ProchaineSurveillance)
		{
			ProchaineSurveillance = Maintenant + 2.f;
			Surveiller(Maintenant);
		}
		for (FBotServeur& B : Serveur)
		{
			ACharacter* E = B.Pion.Get();
			if (IsValid(E) && B.MortDepuis < 0.f)
			{
				Piloter(B, E, Maintenant, DeltaSeconds);
			}
		}
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		// Habits, reacteurs et noms : cherches 2 fois par seconde, noms tournes vers la camera a chaque image
		if (Maintenant >= ProchainHabillage)
		{
			ProchainHabillage = Maintenant + 0.5f;
			OnRep_Bots();
		}
		TournerLesNoms();
	}
	FreinsQuiFinissent(Maintenant);
}

// ===================================================================================
//  Poser les bots (serveur)
// ===================================================================================

void ABotsCombinaison::PreparerPlaces()
{
	bPlacesPretes = true;
	const FReglagesBots& R = FReglagesBots::Lire();
	UWorld* W = GetWorld();

	FBox Zone(ForceInit);
	for (TActorIterator<ANavMeshBoundsVolume> It(W); It; ++It)
	{
		Zone += It->GetComponentsBoundingBox(true);
	}
	if (!Zone.IsValid)
	{
		for (TActorIterator<APlayerStart> It(W); It; ++It)
		{
			Zone += FBox::BuildAABB(It->GetActorLocation(), FVector(40000.f, 40000.f, 5000.f));
		}
	}
	if (!Zone.IsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bots] Zone de jeu introuvable : aucun bot pose."));
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesBotsSol), false);
	for (TActorIterator<APawn> It(W); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	TArray<FVector> Sols;
	for (int32 i = 0; i < 1200; ++i)
	{
		const float X = FMath::FRandRange(Zone.Min.X, Zone.Max.X);
		const float Y = FMath::FRandRange(Zone.Min.Y, Zone.Max.Y);
		FHitResult Sol;
		if (!W->LineTraceSingleByChannel(Sol, FVector(X, Y, Zone.Max.Z + 5000.f), FVector(X, Y, Zone.Min.Z - 5000.f), ECC_Visibility, Params))
		{
			continue;
		}
		if (Sol.ImpactNormal.Z < 0.75f)
		{
			continue;
		}
		const AActor* Touche = Sol.GetActor();
		const FString NomTouche = Touche ? Touche->GetName().ToLower() : FString();
		if (NomTouche.Contains(TEXT("sky")) || NomTouche.Contains(TEXT("water")) || NomTouche.Contains(TEXT("cloud")))
		{
			continue;
		}
		Sols.Add(Sol.ImpactPoint);
	}
	if (Sols.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bots] Aucun sol trouve : aucun bot pose."));
		return;
	}
	TArray<float> Hauteurs;
	for (const FVector& S : Sols)
	{
		Hauteurs.Add(S.Z);
	}
	Hauteurs.Sort();
	const float NiveauRue = Hauteurs[FMath::Clamp(FMath::FloorToInt(Hauteurs.Num() * 0.3f), 0, Hauteurs.Num() - 1)];
	const float Ecart = R.EcartNiveauRueMetres * 100.f;

	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(W);
	TArray<FVector> HorsNav;
	for (const FVector& S : Sols)
	{
		if (FMath::Abs(S.Z - NiveauRue) > Ecart)
		{
			continue;
		}
		FNavLocation SurNav;
		if (Nav && Nav->ProjectPointToNavigation(S, SurNav, FVector(150.f, 150.f, 250.f)))
		{
			Places.Add(SurNav.Location);
		}
		else
		{
			HorsNav.Add(S);
		}
	}
	if (Places.Num() == 0)
	{
		Places = HorsNav;
	}
	UE_LOG(LogTemp, Log, TEXT("[Bots] %d places possibles au niveau de la rue."), Places.Num());
}

bool ABotsCombinaison::ChoisirPlace(const ACharacter* Modele, FVector& Resultat)
{
	if (Places.Num() == 0)
	{
		return false;
	}
	const FReglagesBots& R = FReglagesBots::Lire();
	UWorld* W = GetWorld();
	const UCapsuleComponent* Capsule = Modele ? Modele->GetCapsuleComponent() : nullptr;
	const float DemiHauteur = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	const float RayonCapsule = Capsule ? Capsule->GetScaledCapsuleRadius() : 40.f;

	TArray<FVector> Deja;
	for (const FBotServeur& B : Serveur)
	{
		if (B.Pion.IsValid())
		{
			Deja.Add(B.Pion->GetActorLocation());
		}
	}
	TArray<FVector> Joueurs;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Joueurs.Add(P->GetActorLocation());
		}
	}
	for (TActorIterator<APlayerStart> It(W); It; ++It)
	{
		Joueurs.Add(It->GetActorLocation());
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesBotsPlace), false);
	for (int32 Essai = 0; Essai < 80; ++Essai)
	{
		const float Facteur = Essai < 40 ? 1.f : (Essai < 60 ? 0.5f : 0.2f);
		const FVector Sol = Places[FMath::RandRange(0, Places.Num() - 1)];
		bool bOk = true;
		for (const FVector& D : Deja)
		{
			if (FVector::Dist2D(D, Sol) < R.EspacementMetres * 100.f * Facteur)
			{
				bOk = false;
				break;
			}
		}
		for (int32 i = 0; bOk && i < Joueurs.Num(); ++i)
		{
			if (FVector::Dist2D(Joueurs[i], Sol) < R.LoinDesJoueursMetres * 100.f * Facteur)
			{
				bOk = false;
			}
		}
		if (!bOk)
		{
			continue;
		}
		const FVector Centre = Sol + FVector(0.f, 0.f, DemiHauteur + 5.f);
		if (W->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(RayonCapsule, DemiHauteur), Params))
		{
			continue;
		}
		Resultat = Centre;
		return true;
	}
	return false;
}

ACharacter* ABotsCombinaison::PoserBot(uint8 Type)
{
	if (!ClasseBot)
	{
		return nullptr;
	}
	const FReglagesBots& R = FReglagesBots::Lire();
	const ACharacter* Modele = Cast<ACharacter>(ClasseBot->GetDefaultObject());
	FVector Centre;
	if (!ChoisirPlace(Modele, Centre))
	{
		return nullptr;
	}
	const FTransform Ou(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Centre);
	ACharacter* E = GetWorld()->SpawnActorDeferred<ACharacter>(ClasseBot, Ou, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!E)
	{
		return nullptr;
	}
	E->AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	E->Tags.AddUnique(MarqueBot);
	E->Tags.AddUnique(BotMarqueVuParEnnemis); // pas habille en humain par AEnnemisChelles
	UGameplayStatics::FinishSpawningActor(E, Ou);
	if (!IsValid(E))
	{
		return nullptr;
	}
	if (!E->GetController())
	{
		E->SpawnDefaultController();
	}
	// Plus solide qu'un soldat normal
	if (!FMath::IsNearlyEqual(R.VieFois, 1.f))
	{
		BotMultiplierNombre(E, { TEXT("HealthMax"), TEXT("MaxHealth") }, R.VieFois);
		BotMultiplierNombre(E, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth") }, R.VieFois);
	}
	if (UCharacterMovementComponent* M = E->GetCharacterMovement())
	{
		M->MaxFlySpeed = FMath::Max(M->MaxFlySpeed, R.VitesseVol);
		M->BrakingDecelerationFlying = 0.f;
	}

	const float Maintenant = GetWorld()->GetTimeSeconds();
	FBotServeur B;
	B.Pion = E;
	B.Type = Type;
	B.Phase = FMath::FRandRange(0.f, 2.f * PI);
	B.ProchainVol = Maintenant + FMath::FRandRange(3.f, R.VolToutesLesSecondes);
	B.ProchainPouvoir = Maintenant + FMath::FRandRange(4.f, R.RechargePouvoir);
	B.ProchainTir = Maintenant + FMath::FRandRange(1.f, 3.f);
	Serveur.Add(B);
	return E;
}

void ABotsCombinaison::PoserTout()
{
	const FReglagesBots& R = FReglagesBots::Lire();
	int32 Poses = 0;
	for (int32 n = 0; n < R.NombreDeChaque; ++n)
	{
		for (int32 t = 0; t < R.Types.Num(); ++t)
		{
			if (PoserBot((uint8)t))
			{
				++Poses;
			}
		}
	}
	MettreAJourRep();
	UE_LOG(LogTemp, Log, TEXT("[Bots] %d bots en combinaison poses dans la carte."), Poses);
}

bool ABotsCombinaison::EstMort(ACharacter* E) const
{
	if (!IsValid(E) || E->IsPendingKillPending())
	{
		return true;
	}
	if (E->GetGameTimeSinceCreation() < 3.f)
	{
		return false;
	}
	bool bMort = false;
	if (BotLireBooleen(E, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead"), TEXT("bDead") }, bMort) && bMort)
	{
		return true;
	}
	float Vie = 1.f;
	if (BotLireNombre(E, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth") }, Vie) && Vie <= 0.f)
	{
		return true;
	}
	return false;
}

void ABotsCombinaison::Surveiller(float Maintenant)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	bool bChange = false;
	TArray<uint8> ARemplacer;
	for (int32 i = Serveur.Num() - 1; i >= 0; --i)
	{
		FBotServeur& B = Serveur[i];
		ACharacter* E = B.Pion.Get();
		if (B.MortDepuis < 0.f && EstMort(E))
		{
			B.MortDepuis = Maintenant;
			bChange = true;
			if (IsValid(E))
			{
				// Mort en plein vol : il tombe
				if (UCharacterMovementComponent* M = E->GetCharacterMovement())
				{
					if (M->MovementMode == MOVE_Flying)
					{
						M->SetMovementMode(MOVE_Falling);
					}
				}
			}
			B.bEnVol = false;
		}
		if (B.MortDepuis >= 0.f && R.ReapparitionSecondes > 0.f && Maintenant - B.MortDepuis >= R.ReapparitionSecondes)
		{
			ARemplacer.Add(B.Type);
			Serveur.RemoveAt(i);
			bChange = true;
		}
		else if (B.MortDepuis >= 0.f && R.ReapparitionSecondes <= 0.f && !IsValid(E))
		{
			Serveur.RemoveAt(i);
			bChange = true;
		}
	}
	for (uint8 Type : ARemplacer)
	{
		PoserBot(Type);
	}
	if (bChange)
	{
		MettreAJourRep();
	}
}

void ABotsCombinaison::MettreAJourRep()
{
	TArray<FBotRep> Nouveau;
	for (const FBotServeur& B : Serveur)
	{
		if (B.Pion.IsValid() && B.MortDepuis < 0.f)
		{
			FBotRep Rep;
			Rep.Pion = B.Pion.Get();
			Rep.Type = B.Type;
			Rep.bVol = B.bEnVol || B.bPique;
			Nouveau.Add(Rep);
		}
	}
	Bots = Nouveau;
	ForceNetUpdate();
}

void ABotsCombinaison::JoueursVivants(TArray<APawn*>& Resultat) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (Pion && BotEstVivant(Pion))
		{
			Resultat.Add(Pion);
		}
	}
}

void ABotsCombinaison::BlesserJoueur(APawn* Joueur, float Pourcent, AActor* Auteur)
{
	if (!IsValid(Joueur) || Pourcent <= 0.f)
	{
		return;
	}
	const float Degats = BotVieMax(Joueur) * Pourcent / 100.f * FReglagesBots::Lire().DegatsFois;
	if (Degats <= 0.f)
	{
		return;
	}
	APawn* PionAuteur = Cast<APawn>(Auteur);
	AController* Controleur = IsValid(PionAuteur) ? PionAuteur->GetController() : nullptr;
	UGameplayStatics::ApplyDamage(Joueur, Degats, Controleur, IsValid(Auteur) ? Auteur : nullptr, UDamageType::StaticClass());
}

// ===================================================================================
//  Cerveau des bots (serveur)
// ===================================================================================

void ABotsCombinaison::Piloter(FBotServeur& B, ACharacter* E, float Maintenant, float DeltaSeconds)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	const FTypeBot& Type = R.Types[FMath::Min<int32>(B.Type, R.Types.Num() - 1)];
	const FArmeInfo& Arme = Info(Type.Arme);
	UCharacterMovementComponent* M = E->GetCharacterMovement();

	// En vol : il suit sa trajectoire a chaque image
	if (B.bEnVol)
	{
		VolerVers(B, E, Maintenant);
	}

	// Foudre en attente : elle tombe
	if (B.FoudreA > 0.f && Maintenant >= B.FoudreA)
	{
		B.FoudreA = -1.f;
		MulticastEffet(EffetFoudre, B.FoudreOu, 2.5f);
		TArray<APawn*> Joueurs;
		JoueursVivants(Joueurs);
		for (APawn* J : Joueurs)
		{
			if (FVector::Dist2D(J->GetActorLocation(), B.FoudreOu) < 450.f && FMath::Abs(J->GetActorLocation().Z - B.FoudreOu.Z) < 400.f)
			{
				BlesserJoueur(J, 20.f, E);
			}
		}
	}

	// Rafale en cours
	if (B.RafaleRestante > 0 && Maintenant >= B.ProchainTirRafale)
	{
		APawn* Cible = B.Cible.Get();
		if (Cible && BotEstVivant(Cible))
		{
			Tirer(B, E, Cible);
			--B.RafaleRestante;
			B.ProchainTirRafale = Maintenant + Arme.Intervalle;
		}
		else
		{
			B.RafaleRestante = 0;
		}
	}

	// Pique (onde de choc) : il frappe en touchant le sol
	if (B.bPique && M && (M->IsMovingOnGround() || Maintenant > B.FinVol + 4.f))
	{
		B.bPique = false;
		OndeDeChoc(B, E);
		Atterrir(B, E);
	}

	if (Maintenant < B.ProchaineDecision)
	{
		return;
	}
	B.ProchaineDecision = Maintenant + 0.25f;

	// Le joueur vivant le plus proche
	TArray<APawn*> Joueurs;
	JoueursVivants(Joueurs);
	APawn* Proche = nullptr;
	float DistProche = TNumericLimits<float>::Max();
	for (APawn* J : Joueurs)
	{
		const float D = FVector::Dist(J->GetActorLocation(), E->GetActorLocation());
		if (D < DistProche)
		{
			DistProche = D;
			Proche = J;
		}
	}

	// Personne a moins de DistanceRepos : le bot se repose (rien a calculer)
	if (!Proche || DistProche > R.DistanceReposMetres * 100.f)
	{
		if (B.bEnVol)
		{
			Atterrir(B, E);
		}
		B.Cible = nullptr;
		B.RafaleRestante = 0;
		B.bEndormi = true;
		B.ProchaineDecision = Maintenant + 2.f;
		return;
	}
	B.bEndormi = false;

	const float Detection = R.DistanceDetectionMetres * 100.f;
	if (DistProche <= Detection)
	{
		B.Cible = Proche;
	}
	else if (B.Cible.IsValid() && (!BotEstVivant(B.Cible.Get()) || FVector::Dist(B.Cible->GetActorLocation(), E->GetActorLocation()) > Detection * 1.6f))
	{
		B.Cible = nullptr;
	}
	APawn* Cible = B.Cible.Get();

	// Vol
	if (R.bVol && !B.bEnVol && !B.bPique && Maintenant >= B.ProchainVol && (Cible || (DistProche < 9000.f && FMath::FRand() < 0.05f)))
	{
		Decoller(B, E, Maintenant);
	}
	else if (B.bEnVol && Maintenant >= B.FinVol)
	{
		if (Type.Pouvoir == PouvoirOndeDeChoc && Cible)
		{
			// TITAN : il pique vers le sol et projette tout le monde autour
			B.bEnVol = false;
			B.bPique = true;
			if (M)
			{
				M->SetMovementMode(MOVE_Falling);
				const FVector Vers = (Cible->GetActorLocation() - E->GetActorLocation()).GetSafeNormal2D();
				M->Velocity = Vers * 900.f + FVector(0.f, 0.f, -2500.f);
			}
		}
		else
		{
			Atterrir(B, E);
		}
	}

	if (!Cible)
	{
		return;
	}
	const float Dist = FVector::Dist(Cible->GetActorLocation(), E->GetActorLocation());

	// Arme
	const float Portee = (Arme.PorteeMax > 0.f ? Arme.PorteeMax : R.PorteeArmeMetres) * 100.f;
	if (B.RafaleRestante == 0 && Maintenant >= B.ProchainTir && Dist <= Portee && VoitLeJoueur(E, Cible))
	{
		B.RafaleRestante = Arme.Rafale;
		B.ProchainTirRafale = Maintenant;
		B.ProchainTir = Maintenant + Arme.Cadence * FMath::FRandRange(0.85f, 1.15f);
	}

	// Pouvoir
	if (Maintenant >= B.ProchainPouvoir && Dist <= Detection)
	{
		LancerPouvoir(B, E, Cible, Maintenant);
		B.ProchainPouvoir = Maintenant + R.RechargePouvoir * FMath::FRandRange(0.8f, 1.2f);
	}
}

void ABotsCombinaison::Decoller(FBotServeur& B, ACharacter* E, float Maintenant)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	if (AAIController* C = Cast<AAIController>(E->GetController()))
	{
		C->StopMovement();
		if (UBrainComponent* Cerveau = C->GetBrainComponent())
		{
			Cerveau->PauseLogic(BotRaisonVol);
		}
	}
	if (UCharacterMovementComponent* M = E->GetCharacterMovement())
	{
		M->SetMovementMode(MOVE_Flying);
		M->Velocity = FVector(0.f, 0.f, 1000.f);
	}
	B.bEnVol = true;
	B.FinVol = Maintenant + R.DureeVol * FMath::FRandRange(0.8f, 1.2f);
	B.PointDecollage = E->GetActorLocation();
	MettreAJourRep();
}

void ABotsCombinaison::Atterrir(FBotServeur& B, ACharacter* E)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	if (UCharacterMovementComponent* M = E->GetCharacterMovement())
	{
		if (M->MovementMode == MOVE_Flying)
		{
			M->SetMovementMode(MOVE_Falling);
		}
	}
	if (AAIController* C = Cast<AAIController>(E->GetController()))
	{
		if (UBrainComponent* Cerveau = C->GetBrainComponent())
		{
			Cerveau->ResumeLogic(BotRaisonVol);
		}
	}
	B.bEnVol = false;
	B.ProchainVol = GetWorld()->GetTimeSeconds() + R.VolToutesLesSecondes * FMath::FRandRange(0.7f, 1.3f);
	MettreAJourRep();
}

void ABotsCombinaison::VolerVers(FBotServeur& B, ACharacter* E, float Maintenant)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	UCharacterMovementComponent* M = E->GetCharacterMovement();
	if (!M)
	{
		return;
	}
	if (M->MovementMode != MOVE_Flying)
	{
		M->SetMovementMode(MOVE_Flying);
	}
	const float Hauteur = R.HauteurVolMetres * 100.f;
	FVector Destination;
	APawn* Cible = B.Cible.Get();
	if (Cible)
	{
		// Il tourne autour du joueur, en l'air
		const float Angle = Maintenant * 0.6f + B.Phase;
		Destination = Cible->GetActorLocation() + FVector(FMath::Cos(Angle) * 900.f, FMath::Sin(Angle) * 900.f, Hauteur);
	}
	else
	{
		const float Angle = Maintenant * 0.3f + B.Phase;
		Destination = B.PointDecollage + FVector(FMath::Cos(Angle) * 500.f, FMath::Sin(Angle) * 500.f, Hauteur);
	}
	const FVector Ecart = Destination - E->GetActorLocation();
	const FVector Voulue = (Ecart * 2.f).GetClampedToMaxSize(R.VitesseVol);
	M->Velocity = FMath::Lerp(M->Velocity, Voulue, FMath::Clamp(GetWorld()->GetDeltaSeconds() * 3.f, 0.f, 1.f));

	const FVector Regard = Cible ? (Cible->GetActorLocation() - E->GetActorLocation()) : M->Velocity;
	if (!Regard.IsNearlyZero(1.f))
	{
		E->SetActorRotation(FRotator(0.f, Regard.Rotation().Yaw, 0.f));
	}
}

FVector ABotsCombinaison::BoucheDuCanon(ACharacter* E) const
{
	const FVector Avant = E->GetActorForwardVector();
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (Corps && Corps->DoesSocketExist(TEXT("hand_r")))
	{
		return Corps->GetSocketLocation(TEXT("hand_r")) + Avant * 50.f;
	}
	return E->GetActorLocation() + Avant * 70.f + FVector(0.f, 0.f, 40.f);
}

bool ABotsCombinaison::VoitLeJoueur(ACharacter* E, APawn* Joueur) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesBotsVue), false);
	Params.AddIgnoredActor(E);
	TArray<AActor*> Attaches;
	E->GetAttachedActors(Attaches);
	Params.AddIgnoredActors(Attaches);
	FHitResult Impact;
	const FVector Depart = E->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	const FVector Arrivee = Joueur->GetActorLocation() + FVector(0.f, 0.f, 40.f);
	if (!GetWorld()->LineTraceSingleByChannel(Impact, Depart, Arrivee, ECC_Visibility, Params))
	{
		return true;
	}
	const AActor* Touche = Impact.GetActor();
	return Touche == Joueur || (Touche && Touche->GetOwner() == Joueur);
}

AProjectileBot* ABotsCombinaison::Lancer(ACharacter* E, const FVector& Depart, const FVector& Direction, uint8 Effet, float Vitesse, float TailleTir,
	FLinearColor CouleurTir, float Pourcent, float RayonTir, float Gel, float DureeVie)
{
	const FTransform Ou(Direction.Rotation(), Depart);
	AProjectileBot* P = GetWorld()->SpawnActorDeferred<AProjectileBot>(AProjectileBot::StaticClass(), Ou, E, E, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!P)
	{
		return nullptr;
	}
	P->Preparer(Effet, Vitesse, TailleTir, CouleurTir, Pourcent, RayonTir, Gel);
	if (IsValid(E))
	{
		P->Sphere->IgnoreActorWhenMoving(E, true);
		TArray<AActor*> Attaches;
		E->GetAttachedActors(Attaches);
		for (AActor* A : Attaches)
		{
			P->Sphere->IgnoreActorWhenMoving(A, true);
		}
	}
	UGameplayStatics::FinishSpawningActor(P, Ou);
	if (IsValid(P))
	{
		P->SetLifeSpan(FMath::Max(0.2f, DureeVie));
	}
	return P;
}

void ABotsCombinaison::Tirer(FBotServeur& B, ACharacter* E, APawn* Cible)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	const FTypeBot& Type = R.Types[FMath::Min<int32>(B.Type, R.Types.Num() - 1)];
	const FArmeInfo& Arme = Info(Type.Arme);
	const FVector Depart = BoucheDuCanon(E);
	const FVector Vise = Cible->GetActorLocation() + FVector(0.f, 0.f, 30.f);
	const float Dist = FVector::Dist(Depart, Vise);
	const float Dispersion = Arme.Dispersion * Dist / 1000.f;
	const FVector Point = Vise + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.5f, 0.5f)) * Dispersion;
	const float Portee = (Arme.PorteeMax > 0.f ? Arme.PorteeMax : R.PorteeArmeMetres) * 100.f;
	const float DureeVie = Portee * 1.3f / Arme.Vitesse + 0.1f;
	E->SetActorRotation(FRotator(0.f, (Vise - E->GetActorLocation()).Rotation().Yaw, 0.f));
	Lancer(E, Depart, (Point - Depart).GetSafeNormal(), Arme.Effet, Arme.Vitesse, Arme.Taille, Arme.Couleur, Arme.Pourcent, Arme.Rayon, Arme.Ralentir, DureeVie);
}

void ABotsCombinaison::LancerPouvoir(FBotServeur& B, ACharacter* E, APawn* Cible, float Maintenant)
{
	const FReglagesBots& R = FReglagesBots::Lire();
	const FTypeBot& Type = R.Types[FMath::Min<int32>(B.Type, R.Types.Num() - 1)];
	UWorld* W = GetWorld();

	switch (Type.Pouvoir)
	{
	case PouvoirTeleportation:
	{
		// Il disparait dans la fumee et reapparait dans le dos du joueur
		const UCapsuleComponent* Capsule = E->GetCapsuleComponent();
		const float DemiHauteur = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
		const float RayonCapsule = Capsule ? Capsule->GetScaledCapsuleRadius() : 40.f;
		const FVector Dos = -Cible->GetActorForwardVector().GetSafeNormal2D();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ChellesBotsTeleport), false);
		Params.AddIgnoredActor(E);
		const float Angles[] = { 0.f, 40.f, -40.f, 90.f, -90.f };
		for (float A : Angles)
		{
			const FVector Essai = Cible->GetActorLocation() + Dos.RotateAngleAxis(A, FVector::UpVector) * 450.f;
			FHitResult Sol;
			if (!W->LineTraceSingleByChannel(Sol, Essai + FVector(0.f, 0.f, 200.f), Essai - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
			{
				continue;
			}
			const FVector Centre = Sol.ImpactPoint + FVector(0.f, 0.f, DemiHauteur + 5.f);
			if (W->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(RayonCapsule, DemiHauteur), Params))
			{
				continue;
			}
			MulticastEffet(EffetFumee, E->GetActorLocation(), 1.5f);
			const FRotator Regard(0.f, (Cible->GetActorLocation() - Centre).Rotation().Yaw, 0.f);
			if (E->TeleportTo(Centre, Regard))
			{
				MulticastEffet(EffetFumee, Centre, 1.5f);
				if (B.bEnVol)
				{
					Atterrir(B, E);
				}
				// Et il tire aussitot
				B.RafaleRestante = Info(Type.Arme).Rafale;
				B.ProchainTirRafale = Maintenant + 0.35f;
			}
			break;
		}
		break;
	}
	case PouvoirPluieDeFeu:
	{
		// 6 boules de feu tombent du ciel autour du joueur
		for (int32 i = 0; i < 6; ++i)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const float Dist = i == 0 ? 0.f : FMath::FRandRange(150.f, 650.f);
			const FVector Point = Cible->GetActorLocation() + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, -80.f);
			const FVector Depart = Point + FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), 1800.f + i * 150.f);
			Lancer(E, Depart, (Point - Depart).GetSafeNormal(), EffetExplosion, 2000.f, 0.5f, FLinearColor(1.f, 0.25f, 0.02f), 10.f, 300.f, 0.f, 3.5f);
		}
		break;
	}
	case PouvoirFoudre:
	{
		// La foudre tombera dans 1 seconde a l'endroit du joueur (etincelles = attention !)
		B.FoudreOu = Cible->GetActorLocation() - FVector(0.f, 0.f, 80.f);
		B.FoudreA = Maintenant + 1.f;
		MulticastEffet(EffetMarqueFoudre, B.FoudreOu, 1.f);
		break;
	}
	case PouvoirGel:
	{
		if (FVector::Dist(Cible->GetActorLocation(), E->GetActorLocation()) < 3000.f && VoitLeJoueur(E, Cible))
		{
			MulticastEffet(EffetGlace, Cible->GetActorLocation(), 1.6f);
			MulticastFreiner(Cible, 0.f, 2.5f);
			BlesserJoueur(Cible, 5.f, E);
		}
		break;
	}
	case PouvoirOndeDeChoc:
	default:
	{
		// TITAN : il saute haut puis retombe en pique (voir Piloter)
		if (B.bEnVol)
		{
			B.FinVol = Maintenant;
		}
		else if (!B.bPique)
		{
			Decoller(B, E, Maintenant);
			B.FinVol = Maintenant + 2.f;
		}
		break;
	}
	}
}

void ABotsCombinaison::OndeDeChoc(FBotServeur& B, ACharacter* E)
{
	const FVector Centre = E->GetActorLocation();
	MulticastEffet(EffetOnde, Centre, 2.5f);
	TArray<APawn*> Joueurs;
	JoueursVivants(Joueurs);
	for (APawn* J : Joueurs)
	{
		const FVector Ecart = J->GetActorLocation() - Centre;
		const float Dist = Ecart.Size();
		if (Dist > 900.f)
		{
			continue;
		}
		BlesserJoueur(J, 12.f * (1.f - 0.5f * Dist / 900.f), E);
		if (ACharacter* C = Cast<ACharacter>(J))
		{
			MulticastProjeter(C, Ecart.GetSafeNormal2D() * 1300.f + FVector(0.f, 0.f, 700.f));
		}
	}
}

// ===================================================================================
//  Effets chez tous les joueurs
// ===================================================================================

void ABotsCombinaison::MulticastEffet_Implementation(uint8 Effet, FVector Ou, float TailleEffet)
{
	UWorld* W = GetWorld();
	if (!W || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	switch (Effet)
	{
	case EffetImpact:
		BotBouffee(W, Etincelles(), Ou, 0.6f * TailleEffet, 0.4f);
		break;
	case EffetFlammes:
		BotBouffee(W, Flammes(), Ou, 0.5f * TailleEffet, 0.8f);
		break;
	case EffetExplosion:
		BotBouffee(W, Explosion(), Ou, TailleEffet, 2.f);
		if (USoundBase* Son = SonExplosion())
		{
			UGameplayStatics::PlaySoundAtLocation(W, Son, Ou, 0.7f);
		}
		break;
	case EffetGlace:
		BotBouffee(W, Vapeur(), Ou, TailleEffet, 1.5f);
		BotBouffee(W, Etincelles(), Ou, 0.5f * TailleEffet, 0.6f);
		break;
	case EffetFoudre:
		// Un trait d'etincelles du ciel jusqu'au sol, puis l'explosion
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Point = Ou + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), i * 300.f);
			BotBouffee(W, Etincelles(), Point, 0.8f * TailleEffet, 0.4f);
		}
		BotBouffee(W, Explosion(), Ou, 0.6f * TailleEffet, 1.5f);
		if (USoundBase* Son = SonExplosion())
		{
			UGameplayStatics::PlaySoundAtLocation(W, Son, Ou, 0.8f, 1.6f);
		}
		break;
	case EffetFumee:
		BotBouffee(W, Vapeur(), Ou, TailleEffet, 1.f);
		break;
	case EffetMarqueFoudre:
		BotBouffee(W, Etincelles(), Ou, 1.2f * TailleEffet, 1.f);
		break;
	case EffetOnde:
	default:
		BotBouffee(W, Explosion(), Ou, TailleEffet, 2.f);
		BotBouffee(W, Vapeur(), Ou, 1.5f * TailleEffet, 1.5f);
		if (USoundBase* Son = SonExplosion())
		{
			UGameplayStatics::PlaySoundAtLocation(W, Son, Ou, 1.f, 0.7f);
		}
		break;
	}
}

void ABotsCombinaison::MulticastFreiner_Implementation(APawn* Joueur, float Facteur, float Duree)
{
	ACharacter* C = Cast<ACharacter>(Joueur);
	UCharacterMovementComponent* M = C ? C->GetCharacterMovement() : nullptr;
	if (!M)
	{
		return;
	}
	FFrein* Frein = Freins.Find(Joueur);
	if (!Frein)
	{
		FFrein Nouveau;
		Nouveau.VitesseOrigine = M->MaxWalkSpeed;
		Nouveau.VolOrigine = M->MaxFlySpeed;
		Frein = &Freins.Add(Joueur, Nouveau);
	}
	Frein->Fin = FMath::Max(Frein->Fin, GetWorld()->GetTimeSeconds() + Duree);
	M->MaxWalkSpeed = Frein->VitesseOrigine * Facteur;
	M->MaxFlySpeed = Frein->VolOrigine * FMath::Max(Facteur, 0.1f);
	if (Facteur <= 0.f)
	{
		M->StopMovementImmediately();
	}
}

void ABotsCombinaison::FreinsQuiFinissent(float Maintenant)
{
	for (auto It = Freins.CreateIterator(); It; ++It)
	{
		APawn* Joueur = It.Key().Get();
		if (!IsValid(Joueur))
		{
			It.RemoveCurrent();
			continue;
		}
		if (Maintenant < It.Value().Fin)
		{
			continue;
		}
		if (ACharacter* C = Cast<ACharacter>(Joueur))
		{
			if (UCharacterMovementComponent* M = C->GetCharacterMovement())
			{
				M->MaxWalkSpeed = It.Value().VitesseOrigine;
				M->MaxFlySpeed = It.Value().VolOrigine;
			}
		}
		It.RemoveCurrent();
	}
}

void ABotsCombinaison::MulticastProjeter_Implementation(ACharacter* Joueur, FVector Elan)
{
	if (IsValid(Joueur))
	{
		Joueur->LaunchCharacter(Elan, true, true);
	}
}

// ===================================================================================
//  Habits, reacteurs et noms (sur chaque PC)
// ===================================================================================

void ABotsCombinaison::OnRep_Bots()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (const FBotRep& B : Bots)
	{
		ACharacter* E = B.Pion;
		if (!IsValid(E))
		{
			continue;
		}
		if (!Habilles.Contains(E))
		{
			Habiller(E, B.Type);
		}
		MontrerVol(E, B.Type, B.bVol);
	}
	// Bots morts ou partis : plus de reacteurs
	for (auto It = Reacteurs.CreateIterator(); It; ++It)
	{
		ACharacter* E = It.Key().Get();
		bool bEncore = false;
		for (const FBotRep& B : Bots)
		{
			if (B.Pion == E && B.bVol)
			{
				bEncore = true;
				break;
			}
		}
		if (!bEncore)
		{
			for (const TWeakObjectPtr<UParticleSystemComponent>& P : It.Value())
			{
				if (P.IsValid())
				{
					P->DeactivateSystem();
				}
			}
			It.RemoveCurrent();
		}
	}
	for (auto It = Habilles.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ABotsCombinaison::Habiller(ACharacter* E, uint8 Type)
{
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (!Corps || !CorpsSciFi)
	{
		return;
	}
	const FReglagesBots& R = FReglagesBots::Lire();
	Habilles.Add(E);

	// Si un vrai humain a ete pose avant nous (ChellesEnnemis), on le cache
	TArray<USkeletalMeshComponent*> Composants;
	E->GetComponents<USkeletalMeshComponent>(Composants);
	for (USkeletalMeshComponent* C : Composants)
	{
		if (C && C != Corps && C->GetFName() == BotNomTenueHumain)
		{
			C->SetVisibility(false);
		}
	}

	// La combinaison a les memes os que le mannequin : elle suit exactement ses animations
	Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	USkeletalMeshComponent* Habit = NewObject<USkeletalMeshComponent>(E, BotNomTenueBot);
	Habit->SetSkeletalMesh(CorpsSciFi);
	Habit->SetupAttachment(Corps);
	Habit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Habit->SetGenerateOverlapEvents(false);
	Habit->RegisterComponent();
	Habit->AttachToComponent(Corps, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Habit->SetMasterPoseComponent(Corps);
	if (MateriauxTypes.IsValidIndex(Type) && MateriauxTypes[Type])
	{
		for (int32 i = 0; i < Habit->GetNumMaterials(); ++i)
		{
			Habit->SetMaterial(i, MateriauxTypes[Type]);
		}
	}
	E->AddInstanceComponent(Habit);
	// Le corps d'origine devient invisible mais garde ses collisions : les balles touchent ses os (tete...)
	Corps->SetHiddenInGame(true, false);
	Corps->SetCastShadow(false);

	// Son nom au-dessus de la tete
	if (R.bAfficherNom && R.Types.IsValidIndex(Type))
	{
		UTextRenderComponent* Etiquette = NewObject<UTextRenderComponent>(E, BotNomEtiquette);
		Etiquette->SetupAttachment(E->GetRootComponent());
		Etiquette->RegisterComponent();
		Etiquette->AttachToComponent(E->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		const float DemiHauteur = E->GetCapsuleComponent() ? E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		Etiquette->SetRelativeLocation(FVector(0.f, 0.f, DemiHauteur + 45.f));
		Etiquette->SetText(FText::FromString(R.Types[Type].Nom));
		Etiquette->SetTextRenderColor(R.Types[Type].Couleur.ToFColor(true));
		Etiquette->SetHorizontalAlignment(EHTA_Center);
		Etiquette->SetVerticalAlignment(EVRTA_TextCenter);
		Etiquette->SetWorldSize(32.f);
		Etiquette->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Etiquette->SetCastShadow(false);
		E->AddInstanceComponent(Etiquette);
		Noms.Add(Etiquette);
	}
}

void ABotsCombinaison::MontrerVol(ACharacter* E, uint8 Type, bool bVol)
{
	if (!bVol || Reacteurs.Contains(E))
	{
		return;
	}
	UParticleSystem* Feu = Flammes();
	USkeletalMeshComponent* Corps = E->GetMesh();
	if (!Feu || !Corps)
	{
		return;
	}
	TArray<TWeakObjectPtr<UParticleSystemComponent>> Liste;
	const FName Pieds[] = { TEXT("foot_l"), TEXT("foot_r") };
	for (const FName& Pied : Pieds)
	{
		if (!Corps->DoesSocketExist(Pied))
		{
			continue;
		}
		// Flammes vers le bas, comme des reacteurs
		UParticleSystemComponent* P = UGameplayStatics::SpawnEmitterAttached(Feu, Corps, Pied, FVector::ZeroVector, FRotator(180.f, 0.f, 0.f),
			FVector(0.25f), EAttachLocation::SnapToTarget, true);
		if (P)
		{
			Liste.Add(P);
		}
	}
	Reacteurs.Add(E, Liste);
}

void ABotsCombinaison::TournerLesNoms()
{
	if (Noms.Num() == 0)
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	for (int32 i = Noms.Num() - 1; i >= 0; --i)
	{
		UTextRenderComponent* Nom = Noms[i].Get();
		if (!Nom)
		{
			Noms.RemoveAt(i);
			continue;
		}
		const FVector Ou = Nom->GetComponentLocation();
		const bool bPres = FVector::DistSquared(Ou, Camera) < FMath::Square(6000.f);
		Nom->SetVisibility(bPres);
		if (bPres)
		{
			Nom->SetWorldRotation(FRotator(0.f, (Camera - Ou).Rotation().Yaw, 0.f));
		}
	}
}

// ===================================================================================
//  Console : Chelles.Bots [Relire|Plus]
// ===================================================================================

void ABotsCombinaison::Commande(const TArray<FString>& Arguments)
{
	const FString Mot = Arguments.Num() > 0 ? Arguments[0] : FString();
	if (Mot.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		FReglagesBots::Lire(true);
		UE_LOG(LogTemp, Log, TEXT("[Bots] Bots.txt relu."));
		return;
	}
	if (Mot.Equals(TEXT("Plus"), ESearchCase::IgnoreCase))
	{
		if (!EstServeur())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Bots] Seul le serveur (ou le jeu solo) peut poser des bots."));
			return;
		}
		if (!bPlacesPretes)
		{
			PreparerPlaces();
		}
		PoserTout();
		return;
	}
	const FReglagesBots& R = FReglagesBots::Lire();
	FString Texte = FString::Printf(TEXT("[Bots] %d bots en vie :"), Bots.Num());
	for (const FBotRep& B : Bots)
	{
		const FString Nom = R.Types.IsValidIndex(B.Type) ? R.Types[B.Type].Nom : TEXT("?");
		Texte += FString::Printf(TEXT(" %s%s"), *Nom, B.bVol ? TEXT("(en vol)") : TEXT(""));
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Cyan, Texte);
	}
}
