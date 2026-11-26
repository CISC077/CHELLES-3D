// CHELLES 3D - Commerces : pieces du jeu, boutiques au comptoir (boulangerie, tabac-presse, bar), bonus
#include "CommercesChelles.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

using namespace ChellesCommerces;

namespace
{
	const FName MarqueSuiviCommerces(TEXT("ChellesCommercesSuivi"));
	const FName MarqueMortCommerces(TEXT("ChellesCommercesMort"));
	const FLinearColor VertCommerces(0.45f, 1.f, 0.45f);
	const FLinearColor RougeCommerces(1.f, 0.4f, 0.3f);

	FNumericProperty* CommercesNombre(const UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			if (FNumericProperty* P = FindFProperty<FNumericProperty>(Objet->GetClass(), Nom))
			{
				return P;
			}
		}
		return nullptr;
	}

	double CommercesLire(FNumericProperty* P, const UObject* Objet)
	{
		const void* Valeur = P->ContainerPtrToValuePtr<void>(Objet);
		return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Valeur) : double(P->GetSignedIntPropertyValue(Valeur));
	}

	void CommercesEcrire(FNumericProperty* P, UObject* Objet, double Nouveau)
	{
		void* Valeur = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Valeur, Nouveau);
		}
		else
		{
			P->SetIntPropertyValue(Valeur, int64(FMath::RoundToDouble(Nouveau)));
		}
	}

	FNumericProperty* ProprieteVie(const UObject* Pion)
	{
		return CommercesNombre(Pion, { TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie"), TEXT("HP"), TEXT("health"), TEXT("HealthCurrent") });
	}

	FNumericProperty* ProprieteVieMax(const UObject* Pion)
	{
		return CommercesNombre(Pion, { TEXT("MaxHealth"), TEXT("Max Health"), TEXT("HealthMax"), TEXT("MaxHP"), TEXT("DefaultHealth"), TEXT("VieMax") });
	}

	bool CommercesLireBooleen(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		if (!Objet)
		{
			return false;
		}
		for (const TCHAR* Nom : Noms)
		{
			if (FBoolProperty* P = FindFProperty<FBoolProperty>(Objet->GetClass(), Nom))
			{
				Valeur = P->GetPropertyValue_InContainer(Objet);
				return true;
			}
		}
		return false;
	}

	/** Le pion est vivant (ou on ne sait pas lire sa vie). */
	bool PionVivant(const APawn* Pion)
	{
		if (!IsValid(Pion))
		{
			return false;
		}
		bool bMort = false;
		if (CommercesLireBooleen(Pion, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
		{
			return false;
		}
		if (FNumericProperty* Vie = ProprieteVie(Pion))
		{
			return CommercesLire(Vie, Pion) > 0.0;
		}
		return true;
	}
}

ACommercesChelles::ACommercesChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	bReplicates = false;
	SetActorHiddenInGame(true);
}

void ACommercesChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages(Reglages);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Charger();
		CreerEcran();
	}
	int32 NbArticles = 0;
	for (const FCommerce& C : Reglages.Commerces)
	{
		NbArticles += C.Articles.Num();
	}
	UE_LOG(LogTemp, Log, TEXT("[Commerces] Pret : %d commerces, %d articles, %d pieces."), Reglages.Commerces.Num(), NbArticles, Pieces());
}

void ACommercesChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Ouverte != INDEX_NONE)
	{
		FermerBoutique(false);
	}
	for (FMouvementCommerce& Mv : Mouvements)
	{
		Mv.Fin = 0.f;
	}
	GererMouvements();
	if (Sauvegarde)
	{
		Sauver();
	}
	RetirerEcran();
	Super::EndPlay(Raison);
}

void ACommercesChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	AgeGain += DeltaSeconds;
	if (GetNetMode() != NM_Client)
	{
		if (Maintenant >= ProchainRelais)
		{
			ProchainRelais = Maintenant + 0.5f;
			GererRelais();
			VerifierMorts(Maintenant);
		}
		if (Maintenant >= ProchainSuivi)
		{
			ProchainSuivi = Maintenant + 2.f;
			SuivreEnnemis();
		}
		GererEffets(DeltaSeconds);
	}
	GererMouvements();
	if (GetNetMode() != NM_DedicatedServer)
	{
		JoueurLocal(DeltaSeconds);
	}
}

// =====================================================================================================
// Reseau
// =====================================================================================================

APlayerController* ACommercesChelles::PCLocal() const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController())
		{
			return PC;
		}
	}
	return nullptr;
}

ARelaisCommerces* ACommercesChelles::RelaisDe(APlayerController* PC) const
{
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<ARelaisCommerces> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

void ACommercesChelles::GererRelais()
{
	UWorld* W = GetWorld();
	TArray<APlayerController*> Servis;
	TArray<ARelaisCommerces*> Relais;
	for (TActorIterator<ARelaisCommerces> It(W); It; ++It)
	{
		Relais.Add(*It);
	}
	for (ARelaisCommerces* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			R->Destroy(); // joueur parti
			continue;
		}
		Servis.Add(PC);
	}
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || Servis.Contains(PC))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = PC;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		W->SpawnActor<ARelaisCommerces>(ARelaisCommerces::StaticClass(), FTransform::Identity, Params);
	}
}

// =====================================================================================================
// Serveur : achats et bonus
// =====================================================================================================

bool ACommercesChelles::Soigner(APawn* Pion, float Quantite) const
{
	FNumericProperty* Vie = ProprieteVie(Pion);
	if (!Vie || Quantite <= 0.f)
	{
		return false;
	}
	FNumericProperty* Max = ProprieteVieMax(Pion);
	const double Actuelle = CommercesLire(Vie, Pion);
	const double Plein = Max ? CommercesLire(Max, Pion) : FMath::Max(100.0, Actuelle);
	if (Actuelle <= 0.0 || Actuelle >= Plein - 0.5)
	{
		return false; // mort ou deja plein
	}
	const double Nouvelle = Quantite >= 999.f ? Plein : FMath::Min(Plein, Actuelle + Quantite);
	CommercesEcrire(Vie, Pion, Nouvelle);
	Pion->ForceNetUpdate();
	return true;
}

int32 ACommercesChelles::AjouterMunitions(APlayerController* PC, const TCHAR* Propriete, int32 Quantite) const
{
	// Munitions des armes lourdes du plugin ChellesArmes (AArsenalRelais), lues par leur nom (comme ChellesButin)
	UWorld* W = GetWorld();
	if (!PC || !W || Quantite <= 0)
	{
		return 0;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetOwner() != PC || It->GetClass()->GetName() != TEXT("ArsenalRelais"))
		{
			continue;
		}
		if (FIntProperty* P = FindFProperty<FIntProperty>(It->GetClass(), Propriete))
		{
			P->SetPropertyValue_InContainer(*It, P->GetPropertyValue_InContainer(*It) + Quantite);
			It->ForceNetUpdate();
			return Quantite;
		}
		return 0;
	}
	return 0;
}

void ACommercesChelles::AcheterServeur(ARelaisCommerces* R, uint8 Commerce, uint8 Article)
{
	if (!R)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(R->GetOwner());
	if (!PC || !Reglages.Commerces.IsValidIndex(Commerce) || !Reglages.Commerces[Commerce].Articles.IsValidIndex(Article))
	{
		R->ClientResultat(Commerce, Article, false, TEXT("Article inconnu (Commerces.txt diff\u00E9rent chez l'h\u00F4te ?)"));
		return;
	}
	const FArticle& A = Reglages.Commerces[Commerce].Articles[Article];
	APawn* Pion = PC->GetPawn();
	const float Maintenant = GetWorld()->GetTimeSeconds();
	bool bOk = false;
	FString Msg;
	int32 GainGrattage = 0;

	auto AjouterEffet = [&](uint8 Effet)
	{
		for (FEffetCommerce& E : Effets)
		{
			if (E.PC.Get() == PC && E.Effet == Effet)
			{
				E.Valeur = A.Valeur;
				E.Fin = FMath::Max(E.Fin, Maintenant) + A.Duree; // on rallonge
				return;
			}
		}
		FEffetCommerce E;
		E.PC = PC;
		E.Effet = Effet;
		E.Valeur = A.Valeur;
		E.Fin = Maintenant + A.Duree;
		Effets.Add(E);
	};

	switch (A.Effet)
	{
	case EffetVie:
		bOk = Soigner(Pion, A.Valeur);
		Msg = bOk ? TexteEffet(A) : FString(TEXT("Ta vie est d\u00E9j\u00E0 pleine : garde tes pi\u00E8ces !"));
		break;
	case EffetRegen:
	case EffetBouclier:
		if (!PionVivant(Pion))
		{
			Msg = TEXT("Impossible pour l'instant");
			break;
		}
		AjouterEffet(A.Effet);
		if (A.Effet == EffetBouclier && Pion)
		{
			Pion->OnTakeAnyDamage.AddUniqueDynamic(this, &ACommercesChelles::JoueurTouche);
		}
		bOk = true;
		Msg = TexteEffet(A);
		break;
	case EffetVitesse:
	case EffetSaut:
	{
		ACharacter* Perso = Cast<ACharacter>(Pion);
		if (!Perso || !PionVivant(Pion))
		{
			Msg = TEXT("Descends d'abord du v\u00E9hicule");
			break;
		}
		AppliquerMouvement(Perso, A.Effet, A.Valeur, A.Duree);
		if (!PC->IsLocalController())
		{
			R->ClientMouvement(A.Effet, A.Valeur, A.Duree);
		}
		bOk = true;
		Msg = TexteEffet(A);
		break;
	}
	case EffetXP:
		bOk = true; // l'XP est donnee sur la machine du joueur (plugin ChellesPouvoirs)
		Msg = TexteEffet(A);
		break;
	case EffetMunitions:
	{
		const int32 N = FMath::Max(1, FMath::RoundToInt(A.Valeur));
		const int32 Donnees = AjouterMunitions(PC, TEXT("ChargeMitrailleuse"), N) + AjouterMunitions(PC, TEXT("ChargeFlammes"), FMath::Max(1, N * 2 / 3));
		bOk = Donnees > 0;
		Msg = bOk ? FString::Printf(TEXT("Mitrailleuse +%d, lance-flammes +%d"), N, FMath::Max(1, N * 2 / 3))
			: FString(TEXT("Pas d'armes lourdes ici (plugin ChellesArmes absent)"));
		break;
	}
	case EffetGrattage:
	{
		bOk = true;
		const float Tirage = FMath::FRand();
		if (Tirage < 0.45f) GainGrattage = 0;
		else if (Tirage < 0.75f) GainGrattage = A.Prix;
		else if (Tirage < 0.93f) GainGrattage = A.Prix * 2;
		else if (Tirage < 0.99f) GainGrattage = A.Prix * 4;
		else GainGrattage = FMath::RoundToInt(A.Valeur);
		GainGrattage = FMath::Min(GainGrattage, FMath::Max(0, FMath::RoundToInt(A.Valeur)));
		Msg = GainGrattage > 0 ? FString::Printf(TEXT("GAGN\u00C9 : +%d pi\u00E8ces !"), GainGrattage) : FString(TEXT("Perdu... retente ta chance !"));
		break;
	}
	default:
		Msg = TEXT("Article sans effet");
		break;
	}
	R->ClientResultat(Commerce, Article, bOk, Msg);
	if (bOk && GainGrattage > 0)
	{
		R->ClientPieces(GainGrattage, TEXT("carte \u00E0 gratter"));
	}
	UE_LOG(LogTemp, Log, TEXT("[Commerces] %s achete \"%s\" (%s) : %s"), *PC->GetName(), *A.Nom, *Reglages.Commerces[Commerce].Cle,
		bOk ? TEXT("ok") : *Msg);
}

void ACommercesChelles::GererEffets(float Dt)
{
	UWorld* W = GetWorld();
	const float Maintenant = W->GetTimeSeconds();
	const bool bRegen = Maintenant >= ProchainRegen;
	if (bRegen)
	{
		ProchainRegen = Maintenant + 1.f;
	}
	for (int32 i = Effets.Num() - 1; i >= 0; --i)
	{
		FEffetCommerce& E = Effets[i];
		APlayerController* PC = E.PC.Get();
		if (!PC || Maintenant >= E.Fin)
		{
			if (PC && E.Effet == EffetBouclier)
			{
				if (ARelaisCommerces* R = RelaisDe(PC))
				{
					R->ClientResultat(255, 255, true, TEXT("Bouclier termin\u00E9"));
				}
			}
			Effets.RemoveAt(i);
			continue;
		}
		if (bRegen && E.Effet == EffetRegen)
		{
			Soigner(PC->GetPawn(), E.Valeur);
		}
	}
}

void ACommercesChelles::JoueurTouche(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur)
{
	APawn* Pion = Cast<APawn>(Touche);
	if (!Pion || Degats <= 0.f || !GetWorld())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Pion->GetController());
	const float Maintenant = GetWorld()->GetTimeSeconds();
	for (const FEffetCommerce& E : Effets)
	{
		if (E.PC.Get() == PC && PC && E.Effet == EffetBouclier && Maintenant < E.Fin)
		{
			// la vie a deja ete retiree par le Blueprint du personnage : on en rend une partie
			FNumericProperty* Vie = ProprieteVie(Pion);
			if (Vie && CommercesLire(Vie, Pion) > 0.0)
			{
				FNumericProperty* Max = ProprieteVieMax(Pion);
				const double Actuelle = CommercesLire(Vie, Pion);
				const double Plein = Max ? CommercesLire(Max, Pion) : FMath::Max(100.0, Actuelle);
				CommercesEcrire(Vie, Pion, FMath::Min(Plein, Actuelle + double(Degats) * FMath::Clamp(E.Valeur, 0.f, 100.f) / 100.0));
				Pion->ForceNetUpdate();
			}
			return;
		}
	}
}

void ACommercesChelles::AppliquerMouvement(ACharacter* Perso, uint8 Effet, float Multiplicateur, float Duree)
{
	UCharacterMovementComponent* M = Perso ? Perso->GetCharacterMovement() : nullptr;
	if (!M || !GetWorld())
	{
		return;
	}
	const float Fin = GetWorld()->GetTimeSeconds() + Duree;
	for (FMouvementCommerce& Mv : Mouvements)
	{
		if (Mv.Perso.Get() == Perso && Mv.Effet == Effet)
		{
			Mv.Fin = Fin; // deja actif (hote : serveur et joueur sur la meme machine) : on rallonge
			return;
		}
	}
	FMouvementCommerce Mv;
	Mv.Perso = Perso;
	Mv.Effet = Effet;
	Mv.Fin = Fin;
	if (Effet == EffetVitesse)
	{
		Mv.Original = M->MaxWalkSpeed;
		M->MaxWalkSpeed = Mv.Original * Multiplicateur;
	}
	else
	{
		Mv.Original = M->JumpZVelocity;
		M->JumpZVelocity = Mv.Original * Multiplicateur;
	}
	Mouvements.Add(Mv);
}

void ACommercesChelles::MouvementLocal(uint8 Effet, float Multiplicateur, float Duree)
{
	if (APlayerController* PC = PCLocal())
	{
		AppliquerMouvement(Cast<ACharacter>(PC->GetPawn()), Effet, Multiplicateur, Duree);
	}
}

void ACommercesChelles::GererMouvements()
{
	if (Mouvements.Num() == 0 || !GetWorld())
	{
		return;
	}
	const float Maintenant = GetWorld()->GetTimeSeconds();
	for (int32 i = Mouvements.Num() - 1; i >= 0; --i)
	{
		FMouvementCommerce& Mv = Mouvements[i];
		ACharacter* Perso = Mv.Perso.Get();
		if (Perso && Maintenant < Mv.Fin)
		{
			continue;
		}
		if (Perso)
		{
			if (UCharacterMovementComponent* M = Perso->GetCharacterMovement())
			{
				if (Mv.Effet == EffetVitesse)
				{
					M->MaxWalkSpeed = Mv.Original;
				}
				else
				{
					M->JumpZVelocity = Mv.Original;
				}
			}
		}
		Mouvements.RemoveAt(i);
	}
}

// =====================================================================================================
// Serveur : pieces gagnees en eliminant zombies et ennemis
// =====================================================================================================

bool ACommercesChelles::EstEnnemi(const AActor* Acteur)
{
	const ACharacter* Perso = Cast<ACharacter>(Acteur);
	if (!Perso || Perso->IsPlayerControlled())
	{
		return false;
	}
	static const FName MarqueZombie(TEXT("ChellesZombie"));
	static const FName MarqueEnnemi(TEXT("ChellesEnnemiAuto"));
	if (Perso->Tags.Contains(MarqueZombie) || Perso->Tags.Contains(MarqueEnnemi))
	{
		return true;
	}
	for (const UClass* C = Perso->GetClass(); C; C = C->GetSuperClass())
	{
		if (C->GetPathName().Contains(TEXT("/FPS_Game/Blueprints/Bonus/AI/")))
		{
			return true;
		}
	}
	return false;
}

bool ACommercesChelles::EstMort(const AActor* Acteur)
{
	if (!IsValid(Acteur) || Acteur->IsPendingKillPending() || Acteur->Tags.Contains(MarqueMortCommerces))
	{
		return true;
	}
	bool bMort = false;
	if (CommercesLireBooleen(Acteur, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
	{
		return true;
	}
	return Acteur->GetLifeSpan() > 0.f; // corps qui va disparaitre
}

void ACommercesChelles::SuivreEnnemis()
{
	if (Reglages.ParZombie <= 0 && Reglages.ParEnnemi <= 0)
	{
		return;
	}
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* E = *It;
		if (!IsValid(E) || E->Tags.Contains(MarqueSuiviCommerces) || !EstEnnemi(E) || EstMort(E))
		{
			continue;
		}
		E->Tags.AddUnique(MarqueSuiviCommerces);
		E->OnTakeAnyDamage.AddUniqueDynamic(this, &ACommercesChelles::EnnemiTouche);
	}
}

void ACommercesChelles::EnnemiTouche(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur)
{
	if (!Touche || Degats <= 0.f || Touche->Tags.Contains(MarqueMortCommerces))
	{
		return;
	}
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
	if (!Tireur || !Tireur->IsPlayerController())
	{
		return;
	}
	static const FName MarqueZombie(TEXT("ChellesZombie"));
	const float Maintenant = GetWorld()->GetTimeSeconds();
	FCibleCommerce& C = Cibles.FindOrAdd(Touche);
	C.Tireur = Tireur;
	C.Heure = Maintenant;
	C.bZombie = Touche->Tags.Contains(MarqueZombie);
	if (EstMort(Touche))
	{
		VerifierMorts(Maintenant);
	}
}

void ACommercesChelles::VerifierMorts(float Maintenant)
{
	TArray<TPair<TWeakObjectPtr<AController>, bool>> Tueurs;
	for (auto It = Cibles.CreateIterator(); It; ++It)
	{
		AActor* E = It.Key().Get();
		const FCibleCommerce& C = It.Value();
		if (EstMort(E))
		{
			if (Maintenant - C.Heure <= 4.f && C.Tireur.IsValid())
			{
				Tueurs.Add(TPair<TWeakObjectPtr<AController>, bool>(C.Tireur, C.bZombie));
			}
			if (IsValid(E))
			{
				E->Tags.AddUnique(MarqueMortCommerces);
				E->OnTakeAnyDamage.RemoveDynamic(this, &ACommercesChelles::EnnemiTouche);
			}
			It.RemoveCurrent();
		}
		else if (Maintenant - C.Heure > 60.f)
		{
			It.RemoveCurrent();
		}
	}
	for (const TPair<TWeakObjectPtr<AController>, bool>& T : Tueurs)
	{
		ARelaisCommerces* R = RelaisDe(Cast<APlayerController>(T.Key.Get()));
		const int32 N = T.Value ? Reglages.ParZombie : Reglages.ParEnnemi;
		if (R && N > 0)
		{
			R->ClientPieces(N, T.Value ? TEXT("zombie") : TEXT("ennemi"));
		}
	}
}

// =====================================================================================================
// Joueur local : pieces, sauvegarde, comptoirs
// =====================================================================================================

FString ACommercesChelles::NomSauvegarde() const
{
	FString Nom = TEXT("ChellesCommerces");
	FString Joueur;
	if (FParse::Value(FCommandLine::Get(), TEXT("Joueur="), Joueur) && !Joueur.IsEmpty())
	{
		Nom += TEXT("_") + Joueur;
	}
	else if (GPlayInEditorID > 0)
	{
		Nom += FString::Printf(TEXT("_PIE%d"), GPlayInEditorID + 1); // 2e fenetre du Play dans l'editeur
	}
	return Nom;
}

void ACommercesChelles::Charger()
{
	const FString Nom = NomSauvegarde();
	if (UGameplayStatics::DoesSaveGameExist(Nom, 0))
	{
		Sauvegarde = Cast<UPiecesChelles>(UGameplayStatics::LoadGameFromSlot(Nom, 0));
	}
	if (!Sauvegarde)
	{
		Sauvegarde = Cast<UPiecesChelles>(UGameplayStatics::CreateSaveGameObject(UPiecesChelles::StaticClass()));
		if (Sauvegarde)
		{
			Sauvegarde->Pieces = Reglages.PiecesDepart;
			bModifie = true;
		}
	}
}

void ACommercesChelles::Sauver()
{
	if (!Sauvegarde)
	{
		return;
	}
	UGameplayStatics::SaveGameToSlot(Sauvegarde, NomSauvegarde(), 0);
	bModifie = false;
}

void ACommercesChelles::Son(const FString& Chemin, float Volume) const
{
	if (Chemin.IsEmpty())
	{
		return;
	}
	if (USoundBase* S = LoadObject<USoundBase>(nullptr, *Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		UGameplayStatics::PlaySound2D(this, S, Volume);
	}
}

void ACommercesChelles::AfficherMessage(const FString& Texte, const FLinearColor& Couleur)
{
	Message = Texte;
	CouleurMsg = Couleur;
}

void ACommercesChelles::GagnerPieces(int32 Quantite, const FString& Raison)
{
	if (!Sauvegarde || Quantite == 0)
	{
		return;
	}
	Sauvegarde->Pieces = FMath::Clamp(Sauvegarde->Pieces + Quantite, 0, 9999999);
	if (Quantite > 0)
	{
		Sauvegarde->TotalGagne += Quantite;
	}
	bModifie = true;
	Gain = FString::Printf(TEXT("%+d pi\u00E8ce%s  (%s)"), Quantite, FMath::Abs(Quantite) > 1 ? TEXT("s") : TEXT(""), *Raison);
	AgeGain = 0.f;
	if (Quantite >= 3 && !Raison.Equals(TEXT("XP")) && !Raison.Equals(TEXT("temps de jeu")))
	{
		Son(Reglages.SonPieces, 0.35f);
	}
}

void ACommercesChelles::DonnerXPLocale(int32 Quantite, const FString& Raison)
{
	// L'XP est tenue et sauvegardee par le plugin ChellesPouvoirs : on l'appelle par son nom (comme ChellesButin)
	UWorld* W = GetWorld();
	if (!W || Quantite <= 0)
	{
		return;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetClass()->GetName() != TEXT("PouvoirsChelles"))
		{
			continue;
		}
		UFunction* F = It->FindFunction(TEXT("GagnerXPBonus"));
		if (!F || F->ParmsSize <= 0)
		{
			break;
		}
		uint8* Params = (uint8*)FMemory_Alloca(F->ParmsSize);
		FMemory::Memzero(Params, F->ParmsSize);
		for (TFieldIterator<FProperty> P(F); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
		{
			P->InitializeValue_InContainer(Params);
		}
		if (FIntProperty* PQ = CastField<FIntProperty>(F->FindPropertyByName(TEXT("Quantite"))))
		{
			PQ->SetPropertyValue_InContainer(Params, Quantite);
		}
		if (FStrProperty* PR = CastField<FStrProperty>(F->FindPropertyByName(TEXT("Raison"))))
		{
			PR->SetPropertyValue_InContainer(Params, Raison);
		}
		It->ProcessEvent(F, Params);
		for (TFieldIterator<FProperty> P(F); P && P->HasAnyPropertyFlags(CPF_Parm); ++P)
		{
			P->DestroyValue_InContainer(Params);
		}
		// l'XP achetee ne doit pas redonner des pieces
		DernierXP = -1;
		AttenteXP = 1.5f;
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Commerces] +%d XP (%s) : plugin ChellesPouvoirs absent, XP non comptee."), Quantite, *Raison);
}

void ACommercesChelles::SuivreXP(float Dt)
{
	UWorld* W = GetWorld();
	if (!W || W->IsPaused())
	{
		return;
	}
	// pieces au temps de jeu (0 par defaut)
	if (Reglages.ParMinute > 0.f)
	{
		ResteMinute += Dt / 60.f * Reglages.ParMinute;
		if (ResteMinute >= 1.f)
		{
			const int32 N = FMath::FloorToInt(ResteMinute);
			ResteMinute -= N;
			GagnerPieces(N, TEXT("temps de jeu"));
		}
	}
	// pieces gagnees avec l'XP (plugin ChellesPouvoirs, XPLocale)
	if (AttenteXP > 0.f)
	{
		AttenteXP -= Dt; // laisse le temps au plugin Pouvoirs de charger l'XP sauvegardee
		return;
	}
	ProchainXP -= Dt;
	if (ProchainXP > 0.f || Reglages.ParXP <= 0.f)
	{
		return;
	}
	ProchainXP = 1.f;
	AActor* P = ActeurPouvoirs.Get();
	if (!P)
	{
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			if (It->GetClass()->GetName() == TEXT("PouvoirsChelles"))
			{
				P = *It;
				ActeurPouvoirs = P;
				break;
			}
		}
		if (!P)
		{
			ProchainXP = 10.f;
			return;
		}
	}
	FIntProperty* Prop = FindFProperty<FIntProperty>(P->GetClass(), TEXT("XPLocale"));
	if (!Prop)
	{
		return;
	}
	const int32 XP = Prop->GetPropertyValue_InContainer(P);
	if (DernierXP >= 0)
	{
		const int32 Delta = XP - DernierXP;
		if (Delta > 0 && Delta < 600) // un grand saut = sauvegarde chargee ou commande de test
		{
			ResteXP += Delta * Reglages.ParXP;
			if (ResteXP >= 1.f)
			{
				const int32 N = FMath::FloorToInt(ResteXP);
				ResteXP -= N;
				GagnerPieces(N, TEXT("XP"));
			}
		}
	}
	DernierXP = XP;
}

void ACommercesChelles::ChercherComptoirs()
{
	Comptoirs.Reset();
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->Tags.Num() == 0)
		{
			continue;
		}
		for (int32 c = 0; c < Reglages.Commerces.Num(); ++c)
		{
			if (A->Tags.Contains(Reglages.Commerces[c].Tag))
			{
				FComptoirCommerce C;
				C.Acteur = A;
				C.Commerce = c;
				Comptoirs.Add(C);
				break;
			}
		}
	}
	if (++RecherchesComptoirs <= 2)
	{
		UE_LOG(LogTemp, Log, TEXT("[Commerces] %d morceaux de comptoir trouves dans la carte."), Comptoirs.Num());
	}
}

void ACommercesChelles::JoueurLocal(float Dt)
{
	UWorld* W = GetWorld();
	const float Maintenant = W->GetTimeSeconds();

	SuivreXP(Dt);
	if (bAchatEnCours && Maintenant - DebutAchat > 5.f)
	{
		bAchatEnCours = false;
		AfficherMessage(TEXT("Pas de r\u00E9ponse du serveur : r\u00E9essaie"), RougeCommerces);
	}
	for (int32 i = BonusAffiches.Num() - 1; i >= 0; --i)
	{
		if (Maintenant >= BonusAffiches[i].Value)
		{
			BonusAffiches.RemoveAt(i);
		}
	}
	ProchaineSauvegarde -= Dt;
	if (bModifie && ProchaineSauvegarde <= 0.f)
	{
		ProchaineSauvegarde = 5.f;
		Sauver();
	}
	if (Maintenant >= ProchainComptoirs)
	{
		ProchainComptoirs = Maintenant + (RecherchesComptoirs < 2 ? 8.f : 60.f);
		ChercherComptoirs();
	}

	// ---- boutique : s'ouvre toute seule au comptoir
	if (Maintenant < ProchainProche)
	{
		return;
	}
	ProchainProche = Maintenant + 0.2f;
	APlayerController* PC = PCLocal();
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (bOuvertureForcee)
	{
		return;
	}
	if (!Perso)
	{
		if (Ouverte != INDEX_NONE)
		{
			FermerBoutique(false);
		}
		return;
	}
	const FVector Ici = Perso->GetActorLocation();
	int32 Proche = INDEX_NONE;
	float MeilleurCarre = FMath::Square(Reglages.Distance + 150.f);
	bool bTresProche = false;
	for (const FComptoirCommerce& C : Comptoirs)
	{
		AActor* A = C.Acteur.Get();
		if (!A)
		{
			continue;
		}
		const FBox Boite = A->GetComponentsBoundingBox(true);
		const float D2 = Boite.IsValid ? Boite.ComputeSquaredDistanceToPoint(Ici) : FVector::DistSquared(A->GetActorLocation(), Ici);
		if (D2 < MeilleurCarre)
		{
			MeilleurCarre = D2;
			Proche = C.Commerce;
			bTresProche = D2 <= FMath::Square(Reglages.Distance);
		}
	}
	if (Proche == INDEX_NONE)
	{
		FermeeIci = INDEX_NONE;
		if (Ouverte != INDEX_NONE)
		{
			FermerBoutique(false);
		}
		return;
	}
	if (Ouverte == INDEX_NONE && bTresProche && FermeeIci != Proche && PionVivant(Perso))
	{
		OuvrirBoutique(Proche);
	}
}

void ACommercesChelles::OuvrirBoutique(int32 Commerce)
{
	if (!Reglages.Commerces.IsValidIndex(Commerce))
	{
		return;
	}
	if (Ouverte != INDEX_NONE)
	{
		FermerBoutique(false);
	}
	UWorld* W = GetWorld();
	UGameViewportClient* VueJeu = W ? W->GetGameViewport() : nullptr;
	APlayerController* PC = PCLocal();
	if (!VueJeu || !PC)
	{
		return;
	}
	Ouverte = Commerce;
	AfficherMessage(TEXT("Clique sur un article pour l'acheter"), FLinearColor(0.85f, 0.85f, 0.85f));
	Fenetre = CreerFenetre(Commerce);
	VueJeu->AddViewportWidgetContent(Fenetre.ToSharedRef(), 20);

	bCurseurAvant = PC->bShowMouseCursor;
	PC->bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	PC->SetInputMode(Mode);
	PC->SetIgnoreLookInput(true);
	Son(Reglages.SonPieces, 0.3f);
}

void ACommercesChelles::FermerBoutique(bool bParBouton)
{
	if (Ouverte == INDEX_NONE)
	{
		return;
	}
	if (bParBouton)
	{
		FermeeIci = Ouverte;
	}
	Ouverte = INDEX_NONE;
	bOuvertureForcee = false;
	if (Fenetre.IsValid())
	{
		if (UWorld* W = GetWorld())
		{
			if (UGameViewportClient* VueJeu = W->GetGameViewport())
			{
				VueJeu->RemoveViewportWidgetContent(Fenetre.ToSharedRef());
			}
		}
		Fenetre.Reset();
	}
	if (APlayerController* PC = PCLocal())
	{
		PC->bShowMouseCursor = bCurseurAvant;
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetIgnoreLookInput(false);
	}
}

void ACommercesChelles::Acheter(int32 Article)
{
	if (!Reglages.Commerces.IsValidIndex(Ouverte) || !Reglages.Commerces[Ouverte].Articles.IsValidIndex(Article) || bAchatEnCours)
	{
		return;
	}
	const FArticle& A = Reglages.Commerces[Ouverte].Articles[Article];
	if (Pieces() < A.Prix)
	{
		AfficherMessage(FString::Printf(TEXT("Pas assez de pi\u00E8ces : il en faut %d (tu en as %d)"), A.Prix, Pieces()), RougeCommerces);
		return;
	}
	ARelaisCommerces* R = RelaisDe(PCLocal());
	if (!R)
	{
		AfficherMessage(TEXT("La boutique se pr\u00E9pare... r\u00E9essaie dans une seconde"), RougeCommerces);
		return;
	}
	bAchatEnCours = true;
	DebutAchat = GetWorld()->GetTimeSeconds();
	R->ServeurAcheter(uint8(Ouverte), uint8(Article));
}

void ACommercesChelles::ResultatLocal(uint8 Commerce, uint8 Article, bool bOk, const FString& Texte)
{
	if (Commerce == 255) // simple information du serveur
	{
		AfficherMessage(Texte, FLinearColor(0.85f, 0.85f, 0.85f));
		Gain = Texte;
		AgeGain = 0.f;
		return;
	}
	bAchatEnCours = false;
	if (!bOk || !Reglages.Commerces.IsValidIndex(Commerce) || !Reglages.Commerces[Commerce].Articles.IsValidIndex(Article))
	{
		AfficherMessage(Texte, RougeCommerces);
		return;
	}
	const FArticle& A = Reglages.Commerces[Commerce].Articles[Article];
	if (Sauvegarde)
	{
		Sauvegarde->Pieces = FMath::Max(0, Sauvegarde->Pieces - A.Prix);
		Sauvegarde->TotalDepense += A.Prix;
		bModifie = true;
		Sauver(); // achat : on enregistre tout de suite
		ProchaineSauvegarde = 5.f;
	}
	if (A.Effet == EffetXP)
	{
		DonnerXPLocale(FMath::RoundToInt(A.Valeur), A.Nom);
	}
	if (A.Duree > 0.f)
	{
		const float Fin = GetWorld()->GetTimeSeconds() + A.Duree;
		bool bTrouve = false;
		for (TPair<FString, float>& B : BonusAffiches)
		{
			if (B.Key == A.Nom)
			{
				B.Value = FMath::Max(B.Value, GetWorld()->GetTimeSeconds()) + A.Duree;
				bTrouve = true;
			}
		}
		if (!bTrouve)
		{
			BonusAffiches.Add(TPair<FString, float>(A.Nom, Fin));
		}
	}
	AfficherMessage(FString::Printf(TEXT("Achet\u00E9 : %s  -%d pi\u00E8ces.  %s"), *A.Nom, A.Prix, *Texte), VertCommerces);
	Son(Reglages.SonAchat, 0.7f);
}

FString ACommercesChelles::TexteBonus() const
{
	if (BonusAffiches.Num() == 0 || !GetWorld())
	{
		return FString();
	}
	const float Maintenant = GetWorld()->GetTimeSeconds();
	FString T;
	for (const TPair<FString, float>& B : BonusAffiches)
	{
		T += FString::Printf(TEXT("%s%s  %d s"), T.IsEmpty() ? TEXT("") : TEXT("\n"), *B.Key, FMath::Max(0, FMath::CeilToInt(B.Value - Maintenant)));
	}
	return T;
}

// =====================================================================================================
// Console
// =====================================================================================================

void ACommercesChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Quoi = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Info"));
	if (Quoi.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		if (Ouverte != INDEX_NONE)
		{
			FermerBoutique(false);
		}
		ChargerReglages(Reglages);
		ChercherComptoirs();
		AfficherMessage(TEXT("Commerces.txt relu"), VertCommerces);
		Gain = TEXT("Commerces.txt relu");
		AgeGain = 0.f;
	}
	else if (Quoi.Equals(TEXT("Pieces"), ESearchCase::IgnoreCase))
	{
		const int32 N = Arguments.Num() > 1 ? FCString::Atoi(*Arguments[1]) : 100;
		GagnerPieces(N, TEXT("console"));
		Sauver();
	}
	else if (Quoi.Equals(TEXT("Ouvrir"), ESearchCase::IgnoreCase))
	{
		const FString Cle = Arguments.Num() > 1 ? Arguments[1] : FString(TEXT("Boulangerie"));
		for (int32 c = 0; c < Reglages.Commerces.Num(); ++c)
		{
			if (Reglages.Commerces[c].Cle.Equals(Cle, ESearchCase::IgnoreCase))
			{
				OuvrirBoutique(c);
				bOuvertureForcee = Ouverte != INDEX_NONE;
				return;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[Commerces] commerce inconnu : %s (Boulangerie, Tabac ou Bar)"), *Cle);
	}
	else if (Quoi.Equals(TEXT("Fermer"), ESearchCase::IgnoreCase))
	{
		FermerBoutique(true);
	}
	else
	{
		TMap<int32, int32> ParCommerce;
		for (const FComptoirCommerce& C : Comptoirs)
		{
			ParCommerce.FindOrAdd(C.Commerce)++;
		}
		FString T = FString::Printf(TEXT("%d pi\u00E8ces"), Pieces());
		for (int32 c = 0; c < Reglages.Commerces.Num(); ++c)
		{
			const int32* N = ParCommerce.Find(c);
			T += FString::Printf(TEXT(" | %s : %s"), *Reglages.Commerces[c].Nom, N ? TEXT("dans la carte") : TEXT("ABSENT"));
		}
		UE_LOG(LogTemp, Log, TEXT("[Commerces] %s"), *T);
		Gain = T;
		AgeGain = 0.f;
	}
}
