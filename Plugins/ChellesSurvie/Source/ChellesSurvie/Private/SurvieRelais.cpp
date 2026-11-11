// CHELLES 3D - Modes de survie : relais d'un joueur
#include "SurvieRelais.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#include "StandRefuge.h"
#include "SurvieChelles.h"
#include "SurvieOutils.h"

ASurvieRelais::ASurvieRelais()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	NetUpdateFrequency = 10.f;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ASurvieRelais::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASurvieRelais, Nom);
	DOREPLIFETIME(ASurvieRelais, Pion);
	DOREPLIFETIME(ASurvieRelais, Points);
	DOREPLIFETIME(ASurvieRelais, Kills);
	DOREPLIFETIME(ASurvieRelais, Tetes);
	DOREPLIFETIME(ASurvieRelais, FoisATerre);
	DOREPLIFETIME(ASurvieRelais, Releves);
	DOREPLIFETIME(ASurvieRelais, Etat);
	DOREPLIFETIME(ASurvieRelais, FinATerre);
	DOREPLIFETIME(ASurvieRelais, Relance);
}

APlayerController* ASurvieRelais::GetPC() const
{
	return Cast<APlayerController>(GetOwner());
}

float ASurvieRelais::Maintenant() const
{
	const UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return 0.f;
	}
	const AGameStateBase* EtatJeu = Monde->GetGameState();
	return EtatJeu ? EtatJeu->GetServerWorldTimeSeconds() : Monde->GetTimeSeconds();
}

ASurvieChelles* ASurvieRelais::Gestionnaire() const
{
	for (TActorIterator<ASurvieChelles> It(GetWorld()); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// ---- Demandes du joueur (executees sur le serveur) ----

void ASurvieRelais::ServeurAcheter_Implementation(AStandRefuge* Stand)
{
	if (ASurvieChelles* G = Gestionnaire())
	{
		G->Acheter(this, Stand);
	}
}

void ASurvieRelais::ServeurRelever_Implementation(ASurvieRelais* Cible, bool bAppuie)
{
	if (ASurvieChelles* G = Gestionnaire())
	{
		G->Relever(this, Cible, bAppuie);
	}
}

void ASurvieRelais::ServeurChoisirMode_Implementation(uint8 NouveauMode, int32 VagueDepart)
{
	ASurvieChelles* G = Gestionnaire();
	APlayerController* PC = GetPC();
	// Seul l'hote (ou le joueur solo) choisit : son PlayerController est local sur le serveur
	if (!G || !PC || !PC->IsLocalController())
	{
		if (G)
		{
			G->Avis(this, TEXT("Seul l'hôte de la partie peut choisir le mode."));
		}
		return;
	}
	if (NouveauMode == (uint8)EModeSurvie::Refuge)
	{
		G->Demarrer(EModeSurvie::Refuge, FMath::Clamp(VagueDepart, 1, 100));
	}
	else
	{
		G->Arreter();
	}
}

// ---- Reponses du serveur (executees sur la machine du joueur) ----

void ASurvieRelais::ClientAvis_Implementation(const FString& Texte)
{
	if (ASurvieChelles* G = Gestionnaire())
	{
		G->MessageLocal(Texte);
	}
}

void ASurvieRelais::ClientDonnerXP_Implementation(int32 Quantite)
{
	// L'XP des pouvoirs vit sur la machine du joueur (plugin ChellesPouvoirs, variable XPLocale) :
	// on l'augmente par son nom, le plugin la sauvegarde et l'annonce au serveur tout seul.
	AActor* Pouvoirs = SurvieOutils::ActeurDeClasse(GetWorld(), TEXT("PouvoirsChelles"));
	float XP = 0.f;
	if (Pouvoirs && Quantite > 0 && SurvieOutils::LireNombre(Pouvoirs, { TEXT("XPLocale") }, XP))
	{
		SurvieOutils::EcrireNombre(Pouvoirs, { TEXT("XPLocale") }, XP + Quantite);
	}
}

void ASurvieRelais::ClientRecharger_Implementation()
{
	if (APlayerController* PC = GetPC())
	{
		SurvieOutils::RemplirMunitions(PC->GetPawn());
	}
}

void ASurvieRelais::ClientCreerArme_Implementation(const FString& Chemin, FVector Lieu)
{
	UClass* Classe = LoadClass<AActor>(nullptr, *Chemin);
	if (!Classe || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Survie] Arme introuvable sur cette machine : %s"), *Chemin);
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	GetWorld()->SpawnActor<AActor>(Classe, FTransform(Lieu), Params);
}
