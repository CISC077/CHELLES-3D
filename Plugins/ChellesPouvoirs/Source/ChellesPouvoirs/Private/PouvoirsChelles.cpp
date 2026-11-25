// CHELLES 3D - XP et pouvoirs : gagner de l'XP en jouant, debloquer et utiliser des pouvoirs (UE 4.27)
#include "PouvoirsChelles.h"
#include "PouvoirsRelais.h"
#include "PouvoirsTypes.h"
#include "BouleDeFeuChelles.h"
#include "XPSauvegarde.h"
#include "SauvegardeJoueur.h"
#include "SPouvoirsHud.h"

#include <initializer_list>

#include "AIController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundBase.h"
#include "Modules/ModuleManager.h"

using namespace ChellesPouvoirs;

namespace
{
	const TCHAR* EmplacementXP = TEXT("ChellesXP");

	FString TempsDeJeuAvant(int32 XPVoulue, int32 XPActuelle)
	{
		const int32 Minutes = FMath::Max(1, FMath::DivideAndRoundUp(XPVoulue - XPActuelle, XPParMinute));
		if (Minutes >= 60)
		{
			return FString::Printf(TEXT("%d h %02d"), Minutes / 60, Minutes % 60);
		}
		return FString::Printf(TEXT("%d min"), Minutes);
	}

	/** Le pion et tout ce qui y est accroche (armes, sacs...), sur 3 niveaux. */
	void ActeursDuPion(AActor* Racine, TArray<AActor*>& Sortie, int32 Profondeur = 0)
	{
		if (!Racine || Sortie.Contains(Racine) || Profondeur > 3)
		{
			return;
		}
		Sortie.Add(Racine);
		TArray<AActor*> Attaches;
		Racine->GetAttachedActors(Attaches);
		for (AActor* A : Attaches)
		{
			ActeursDuPion(A, Sortie, Profondeur + 1);
		}
	}
}

APouvoirsChelles::APouvoirsChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // pour cacher la barre d'XP quand le menu met en pause
	bReplicates = false; // un par machine : les choses partagees passent par APouvoirsRelais
	SetActorHiddenInGame(true);
	ToucheUtiliser = EKeys::P; // 25/11/2026 : P = POUVOIR (J = danser, demande MERIDJA)
	ToucheChanger = EKeys::Y;
}

APlayerController* APouvoirsChelles::GetPC() const
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

bool APouvoirsChelles::EstServeur() const
{
	return GetNetMode() != NM_Client;
}

APouvoirsRelais* APouvoirsChelles::MonRelais() const
{
	APlayerController* PC = GetPC();
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

void APouvoirsChelles::Message(const FString& Texte)
{
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] %s"), *Texte);
	Info = Texte;
	AgeInfo = 0.f;
}

// ---------------------------------------------------------------------------
// Demarrage / fin
// ---------------------------------------------------------------------------

void APouvoirsChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	LirePersonnages();
	if (GetNetMode() != NM_DedicatedServer)
	{
		Charger();
	}
	LierClavier();
	CreerHud();
}

void APouvoirsChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (bCharge)
	{
		Sauvegarder();
	}
	RetirerHud();
	Super::EndPlay(Raison);
}

void APouvoirsChelles::LierClavier()
{
	if (bLieClavier)
	{
		return;
	}
	APlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	EnableInput(PC);
	if (!InputComponent)
	{
		return;
	}
	if (ToucheUtiliser.IsValid())
	{
		FInputKeyBinding& B = InputComponent->BindKey(ToucheUtiliser, IE_Pressed, this, &APouvoirsChelles::Utiliser);
		B.bConsumeInput = false;
	}
	if (ToucheChanger.IsValid())
	{
		FInputKeyBinding& B = InputComponent->BindKey(ToucheChanger, IE_Pressed, this, &APouvoirsChelles::Changer);
		B.bConsumeInput = false;
	}
	bLieClavier = true;
}

// ---------------------------------------------------------------------------
// XP du joueur de cette machine
// ---------------------------------------------------------------------------

void APouvoirsChelles::Charger()
{
	NomSauvegarde = ChellesSauvegarde::Emplacement(this, EmplacementXP);
	if (UXPSauvegarde* S = ChellesSauvegarde::Lire<UXPSauvegarde>(NomSauvegarde))
	{
		XPLocale = FMath::Max(0, S->XP);
		SecondesJouees = FMath::Max(0.0, S->SecondesJouees);
		Selection = FMath::Clamp(S->Selection, 0, int32(NombrePouvoirs) - 1);
	}
	NiveauAnnonce = NiveauPourXP(XPLocale);
	bCharge = true;
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] XP chargee depuis %s.sav : %d (niveau %d, %.0f min de jeu)."),
		*NomSauvegarde, XPLocale, NiveauAnnonce, SecondesJouees / 60.0);
	if (XPLocale > 0)
	{
		Message(FString::Printf(TEXT("Progression retrouv\u00E9e : niveau %d, %d XP"), NiveauAnnonce, XPLocale));
	}
}

void APouvoirsChelles::Sauvegarder()
{
	if (!bCharge)
	{
		return; // jamais ecraser la sauvegarde avant de l'avoir lue
	}
	UXPSauvegarde* S = Cast<UXPSauvegarde>(UGameplayStatics::CreateSaveGameObject(UXPSauvegarde::StaticClass()));
	if (!S)
	{
		return;
	}
	S->XP = XPLocale;
	S->SecondesJouees = SecondesJouees;
	S->Selection = Selection;
	ChellesSauvegarde::Ecrire(S, NomSauvegarde);
}

bool APouvoirsChelles::EnJeu() const
{
	APlayerController* PC = GetPC();
	if (!PC || !PC->GetPawn() || PC->IsPaused())
	{
		return false;
	}
	// Souris visible = menu, chat ou options ouverts : on ne compte pas
	return !PC->bShowMouseCursor;
}

void APouvoirsChelles::CompterTemps(float Secondes)
{
	const int32 HeuresAvant = int32(SecondesJouees / 3600.0);
	SecondesJouees += Secondes;
	SecondesMinute += Secondes;
	while (SecondesMinute >= 60.f)
	{
		SecondesMinute -= 60.f;
		DonnerXP(XPParMinute, true);
	}
	if (int32(SecondesJouees / 3600.0) > HeuresAvant)
	{
		DonnerXP(BonusParHeure, true);
		Message(FString::Printf(TEXT("+%d XP : bonus pour une heure de jeu !"), BonusParHeure));
	}
}

void APouvoirsChelles::DonnerXP(int32 Quantite, bool bTempsDeJeu)
{
	if (Quantite > 0 && Reglages().FonctionPerso == int32(DoubleXP))
	{
		Quantite *= 2; // fonction DOUBLE XP du personnage choisi (Personnages.txt)
	}
	XPLocale = FMath::Max(0, XPLocale + Quantite);
	const int32 Niveau = NiveauPourXP(XPLocale);
	if (Niveau > NiveauAnnonce)
	{
		Banniere = FString::Printf(TEXT("NIVEAU %d : %s"), Niveau, Paliers()[Niveau - 1].Titre);
		SousBanniere.Empty();
		for (int32 i = 0; i < NombrePouvoirs; ++i)
		{
			if (Pouvoir(i).Niveau == Niveau)
			{
				SousBanniere = FString::Printf(TEXT("NOUVEAU POUVOIR : %s  -  touche P"), Pouvoir(i).Nom);
				Selection = i;
			}
		}
		AgeBanniere = 0.f;
		if (USoundBase* Son = LoadObject<USoundBase>(nullptr, TEXT("/Game/Sounds/InterfaceAudio/Stereo/MenuAccept_Stereo_Cue.MenuAccept_Stereo_Cue")))
		{
			UGameplayStatics::PlaySound2D(this, Son);
		}
		UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Niveau %d atteint (%d XP)."), Niveau, XPLocale);
	}
	NiveauAnnonce = Niveau;
	Sauvegarder();
}

void APouvoirsChelles::GagnerXPBonus(int32 Quantite, const FString& Raison)
{
	if (!bCharge || Quantite == 0)
	{
		return;
	}
	DonnerXP(Quantite, false); // sauvegarde tout de suite ; le serveur recoit la nouvelle XP au Tick
	if (!Raison.IsEmpty())
	{
		Message(FString::Printf(TEXT("+%d XP : %s"), Quantite, *Raison));
	}
}

// ---------------------------------------------------------------------------
// XP de combat (2026-11-18) : toucher / eliminer un ennemi
//  - le serveur branche chaque ennemi (bandits, soldats, chargers, zombies FPS_Game, ennemis du plugin
//    ChellesEnnemis, zombies de l'invasion) sur EnnemiBlesse ;
//  - il retient le dernier joueur qui l'a touche, et voit quand l'ennemi meurt (isDead, detruit...) ;
//  - l'XP part sur la machine du tireur (elle y est sauvegardee, comme l'XP du temps de jeu).
// ---------------------------------------------------------------------------

namespace
{
	const FName MarqueSuiviXP(TEXT("ChellesXPSuivi"));
	const FName MarqueMortXP(TEXT("ChellesXPMort"));

	bool LireBooleenXP(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
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
}

bool APouvoirsChelles::EstEnnemi(AActor* Acteur) const
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
	// Les Blueprints d'ennemis FPS_Game (et leurs enfants) sont ranges dans Blueprints/Bonus/AI
	for (const UClass* C = Perso->GetClass(); C; C = C->GetSuperClass())
	{
		if (C->GetPathName().Contains(TEXT("/FPS_Game/Blueprints/Bonus/AI/")))
		{
			return true;
		}
	}
	return false;
}

bool APouvoirsChelles::EstMort(AActor* Acteur)
{
	if (!IsValid(Acteur) || Acteur->IsPendingKillPending() || Acteur->Tags.Contains(MarqueMortXP))
	{
		return true;
	}
	bool bMort = false;
	if (LireBooleenXP(Acteur, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
	{
		return true;
	}
	// Les plugins Zombies / Ennemis (et beaucoup de Blueprints) donnent une duree de vie au corps qui tombe
	return Acteur->GetLifeSpan() > 0.f;
}

void APouvoirsChelles::SuivreEnnemis()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	for (TActorIterator<ACharacter> It(W); It; ++It)
	{
		ACharacter* E = *It;
		if (!IsValid(E) || E->Tags.Contains(MarqueSuiviXP) || !EstEnnemi(E) || EstMort(E))
		{
			continue;
		}
		E->Tags.AddUnique(MarqueSuiviXP);
		E->OnTakeAnyDamage.AddUniqueDynamic(this, &APouvoirsChelles::EnnemiBlesse);
	}
}

void APouvoirsChelles::EnnemiBlesse(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur)
{
	if (!Touche || Degats <= 0.f || Touche->Tags.Contains(MarqueMortXP))
	{
		return;
	}
	// Qui a tire ? (meme logique que les plugins Zombies et Ennemis)
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
		return; // un ennemi qui en touche un autre, un baril, le feu... : pas d'XP
	}
	UWorld* W = GetWorld();
	const float Maintenant = W ? W->GetTimeSeconds() : 0.f;

	FCibleXP& Cible = CiblesTouchees.FindOrAdd(Touche);
	Cible.Tireur = Tireur;
	Cible.Heure = Maintenant;

	// Le coup qui tue : l'ennemi est deja marque mort pendant ce meme appel (plugins Zombies / Ennemis)
	if (EstMort(Touche))
	{
		VerifierMorts(Maintenant);
		return;
	}

	// Petite XP par coup, au plus 4 fois par seconde (une rafale ne rapporte pas une fortune)
	float& Dernier = DernierXPTouche.FindOrAdd(Tireur);
	if (Maintenant - Dernier >= 0.25f)
	{
		Dernier = Maintenant;
		EnvoyerXPCombat(Tireur, XPParTouche, false);
	}
}

void APouvoirsChelles::VerifierMorts(float Maintenant)
{
	for (auto It = CiblesTouchees.CreateIterator(); It; ++It)
	{
		AActor* E = It.Key().Get();
		const FCibleXP& C = It.Value();
		if (EstMort(E))
		{
			// Tue (ou disparu) peu apres un coup d'un joueur : il gagne l'elimination
			if (Maintenant - C.Heure <= 4.f && C.Tireur.IsValid())
			{
				EnvoyerXPCombat(C.Tireur.Get(), XPParEnnemiTue, true);
			}
			if (IsValid(E))
			{
				E->Tags.AddUnique(MarqueMortXP);
				E->OnTakeAnyDamage.RemoveDynamic(this, &APouvoirsChelles::EnnemiBlesse);
			}
			It.RemoveCurrent();
		}
		else if (Maintenant - C.Heure > 60.f)
		{
			It.RemoveCurrent(); // touche il y a longtemps et toujours vivant : on oublie
		}
	}
	for (auto It = DernierXPTouche.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void APouvoirsChelles::EnvoyerXPCombat(AController* Tireur, int32 Quantite, bool bTue)
{
	APlayerController* PC = Cast<APlayerController>(Tireur);
	if (!PC || Quantite <= 0)
	{
		return;
	}
	if (PC->IsLocalController())
	{
		GagnerXPCombat(Quantite, bTue); // solo ou joueur qui heberge
		return;
	}
	// Joueur distant : son XP est sauvegardee sur son PC, on la lui envoie par son relais
	for (TActorIterator<APouvoirsRelais> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			It->ClientXPCombat(Quantite, bTue);
			return;
		}
	}
}

void APouvoirsChelles::GagnerXPCombat(int32 Quantite, bool bTue)
{
	if (!bCharge || Quantite <= 0)
	{
		return;
	}
	XPCombatEnAttente += Quantite;
	if (bTue)
	{
		// Elimination : tout de suite, avec un message
		const int32 Total = XPCombatEnAttente;
		XPCombatEnAttente = 0;
		DonnerXP(Total, false);
		Message(FString::Printf(TEXT("+%d XP : ennemi éliminé !"), Total));
		if (UWorld* W = GetWorld())
		{
			ProchainBilanCombat = W->GetRealTimeSeconds() + 2.f; // le message reste lisible
		}
	}
	// Sinon les coups sont cumules et comptes toutes les 2 s (voir Tick) : pas une sauvegarde par balle
}

void APouvoirsChelles::AjouterXPTest(int32 Quantite)
{
	DonnerXP(Quantite, false);
	Message(FString::Printf(TEXT("Test : %+d XP (total %d)"), Quantite, XPLocale));
	if (APouvoirsRelais* R = MonRelais())
	{
		R->ServeurXP(XPLocale);
	}
}

// ---------------------------------------------------------------------------
// Touches J et Y
// ---------------------------------------------------------------------------

void APouvoirsChelles::Changer()
{
	if (!EnJeu())
	{
		return;
	}
	for (int32 Pas = 1; Pas <= NombrePouvoirs; ++Pas)
	{
		const int32 Suivant = (Selection + Pas) % NombrePouvoirs;
		if (EstDebloque(Suivant, XPLocale))
		{
			Selection = Suivant;
			Message(FString::Printf(TEXT("%s : %s"), Pouvoir(Selection).Nom, Pouvoir(Selection).Aide));
			Sauvegarder(); // le pouvoir choisi est retrouve a la prochaine partie
			return;
		}
	}
	Message(FString::Printf(TEXT("Aucun pouvoir pour l'instant : %s s'ouvre dans %s de jeu."),
		Pouvoir(0).Nom, *TempsDeJeuAvant(Paliers()[Pouvoir(0).Niveau - 1].XP, XPLocale)));
}

void APouvoirsChelles::Utiliser()
{
	if (!PermisPourPerso(Selection, Reglages().PouvoirPerso))
	{
		Selection = Reglages().PouvoirPerso; // pouvoir garde d'une ancienne partie : on prend celui du perso
	}
	Demander(Selection);
}

void APouvoirsChelles::Demander(int32 Quel)
{
	if (!EnJeu() || Quel < 0 || Quel >= NombrePouvoirs)
	{
		return;
	}
	APlayerController* PC = GetPC();
	APouvoirsRelais* R = MonRelais();
	if (!PC || !R)
	{
		Message(TEXT("Les pouvoirs se pr\u00E9parent, r\u00E9essaie dans une seconde."));
		return;
	}
	const FInfoPouvoir& P = Pouvoir(Quel);
	if (!EstDebloque(Quel, XPLocale))
	{
		Message(FString::Printf(TEXT("%s s'ouvre au niveau %d, dans %s de jeu."), P.Nom, P.Niveau,
			*TempsDeJeuAvant(Paliers()[P.Niveau - 1].XP, XPLocale)));
		return;
	}
	if (Quel == Envol && R->EffetActif(Envol))
	{
		// 2026-11-22 : J pendant le vol = atterrir (le vol peut etre sans limite)
		R->ServeurFinEnvol();
		return;
	}
	if (R->EffetActif(Quel) && Quel != Cameleon)
	{
		return;
	}
	const float Attente = R->RestantRecharge(Quel);
	if (Attente > 0.f)
	{
		Message(FString::Printf(TEXT("%s se recharge : encore %d s"), P.Nom, FMath::CeilToInt(Attente)));
		return;
	}
	if (R->XP != XPLocale)
	{
		R->ServeurXP(XPLocale); // arrive au serveur avant la demande de pouvoir
	}
	FVector Origine;
	FRotator Visee;
	PC->GetPlayerViewPoint(Origine, Visee);
	R->ServeurPouvoir(uint8(Quel), Origine, Visee);
}

// ---------------------------------------------------------------------------
// Tout le monde vole (2026-11-22, demande MERIDJA) : sauter puis ESPACE en l'air = ENVOL.
// Le soldat garde ses armes en volant. Spider-Male / Spider-Femelle / PANTERA gardent leur
// propre vol (plugin ChellesRoue) : on ne touche pas a ces personnages-la.
// ---------------------------------------------------------------------------

void APouvoirsChelles::ChargerReglages()
{
	FReglagesPouvoirs& Reg = Reglages();
	Reg = FReglagesPouvoirs(); // valeurs par defaut si le fichier manque
	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesPouvoirs"), TEXT("Pouvoirs.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] %s absent : tous les pouvoirs et le vol pour tous (reglages par defaut)."), *Chemin);
		return;
	}
	auto Oui = [](const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("True"), ESearchCase::IgnoreCase) || V == TEXT("1");
	};
	for (const FString& Brute : Lignes)
	{
		const FString Ligne = Brute.TrimStartAndEnd();
		FString Cle, Valeur;
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		if (Cle.Equals(TEXT("TousLesPouvoirs"), ESearchCase::IgnoreCase))
		{
			Reg.bTousLesPouvoirs = Oui(Valeur);
		}
		else if (Cle.Equals(TEXT("VolPourTous"), ESearchCase::IgnoreCase))
		{
			Reg.bVolPourTous = Oui(Valeur);
		}
		else if (Cle.Equals(TEXT("DureeVol"), ESearchCase::IgnoreCase))
		{
			Reg.DureeEnvol = FMath::Max(0.f, FCString::Atof(*Valeur));
		}
		else if (Cle.Equals(TEXT("RechargeVol"), ESearchCase::IgnoreCase))
		{
			Reg.RechargeEnvol = FMath::Max(0.f, FCString::Atof(*Valeur));
		}
		else if (Cle.Equals(TEXT("UnPouvoirParPerso"), ESearchCase::IgnoreCase))
		{
			Reg.bUnPouvoirParPerso = Oui(Valeur);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Reglages : TousLesPouvoirs=%d VolPourTous=%d DureeVol=%.0f RechargeVol=%.0f UnPouvoirParPerso=%d"),
		Reg.bTousLesPouvoirs, Reg.bVolPourTous, Reg.DureeEnvol, Reg.RechargeEnvol, Reg.bUnPouvoirParPerso);
}

bool APouvoirsChelles::AUnVolAPart(APawn* Pion)
{
	if (!Pion)
	{
		return true;
	}
	// Soldat habille en Spider-Male / Spider-Femelle / PANTERA par la roue : ESPACE en l'air = leur vol a eux
	if (Pion->Tags.Contains(FName(TEXT("ChellesSansHabillage"))))
	{
		return true;
	}
	// Personnage du pack SuperheroFlight (le volant de Spider-Male...) : il vole deja tout seul
	for (UActorComponent* C : Pion->GetComponents())
	{
		if (C && C->GetClass()->GetName().Contains(TEXT("SuperheroFlight")))
		{
			return true;
		}
	}
	return false;
}

void APouvoirsChelles::SuivreVolPourTous(float DeltaSeconds)
{
	APlayerController* PC = GetPC();
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	APouvoirsRelais* R = MonRelais();
	UCharacterMovementComponent* Mouvement = Perso ? Perso->GetCharacterMovement() : nullptr;
	if (!Mouvement || !R || !EnJeu())
	{
		TempsAuSol = 0.f;
		return;
	}

	if (R->EffetActif(Envol))
	{
		// En vol : on se pose en descendant (CTRL / C) jusqu'a toucher le sol
		const bool bDescend = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::C) || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Right);
		bool bSolProche = false;
		if (bDescend && Perso->GetCapsuleComponent())
		{
			const float DemiHauteur = Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Pieds = Perso->GetActorLocation();
			FHitResult Sol;
			FCollisionQueryParams Requete(SCENE_QUERY_STAT(AtterrirChelles), false, Perso);
			bSolProche = GetWorld()->LineTraceSingleByChannel(Sol, Pieds, Pieds - FVector(0.f, 0.f, DemiHauteur + 40.f), ECC_Visibility, Requete);
		}
		TempsAuSol = bSolProche ? TempsAuSol + DeltaSeconds : 0.f;
		if (TempsAuSol >= 0.15f)
		{
			TempsAuSol = 0.f;
			R->ServeurFinEnvol();
		}
		return;
	}
	TempsAuSol = 0.f;

	if (!Reglages().bVolPourTous || !PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		return;
	}
	// ESPACE au sol = le saut normal. ESPACE en l'air (apres un saut ou une chute) = s'envoler.
	if (!Mouvement->IsFalling() || AUnVolAPart(Perso))
	{
		return;
	}
	Demander(Envol);
}

void APouvoirsChelles::FinirEnvol(APouvoirsRelais* Relais)
{
	if (!Relais || !Relais->EffetActif(Envol))
	{
		return;
	}
	const float Maintenant = Relais->Maintenant();
	Relais->FinEffet[Envol] = Maintenant;
	Relais->FinRecharge[Envol] = Maintenant + Pouvoir(Envol).Recharge;
	Relais->ForceNetUpdate();
}

// ---------------------------------------------------------------------------
// Serveur
// ---------------------------------------------------------------------------

bool APouvoirsChelles::PvpActif() const
{
	if (GetNetMode() == NM_Standalone)
	{
		return true; // en solo il n'y a que des passants et des ennemis
	}
	bool bPvp = true; // reglage PVP du menu (celui qui heberge decide)
	if (GConfig)
	{
		GConfig->GetBool(TEXT("ChellesMenu"), TEXT("Pvp"), bPvp, GGameUserSettingsIni);
	}
	return bPvp;
}

void APouvoirsChelles::GererRelais()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	TArray<APlayerController*> Servis;
	TArray<APouvoirsRelais*> Relais;
	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		Relais.Add(*It);
	}
	for (APouvoirsRelais* R : Relais)
	{
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		if (!PC)
		{
			R->Destroy(); // joueur parti
			continue;
		}
		Servis.Add(PC);
		APawn* Actuel = PC->GetPawn();
		if (R->Pion != Actuel)
		{
			FinirEnvol(R); // nouveau personnage (roue, mort, vehicule) : il ne s'envole pas tout seul
			R->Pion = Actuel;
			R->ForceNetUpdate();
		}
		if (R->CibleGel && !R->EffetActif(Gel))
		{
			R->CibleGel = nullptr;
		}
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
		if (APouvoirsRelais* Nouveau = W->SpawnActor<APouvoirsRelais>(APouvoirsRelais::StaticClass(), FTransform::Identity, Params))
		{
			Nouveau->Pion = PC->GetPawn();
			UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Pouvoirs prets pour le joueur %s."), *PC->GetName());
		}
	}
}

APawn* APouvoirsChelles::ViserCible(APawn* Lanceur, const FVector& Origine, const FVector& Direction) const
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return nullptr;
	}
	FCollisionQueryParams Requete(SCENE_QUERY_STAT(GelChelles), false, Lanceur);
	TArray<AActor*> AIgnorer;
	ActeursDuPion(Lanceur, AIgnorer);
	Requete.AddIgnoredActors(AIgnorer);
	FHitResult Impact;
	const FVector Fin = Origine + Direction * 3000.f; // 30 m
	if (!W->SweepSingleByChannel(Impact, Origine, Fin, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(45.f), Requete))
	{
		return nullptr;
	}
	AActor* Touche = Impact.GetActor();
	for (int32 i = 0; Touche && i < 4; ++i)
	{
		if (APawn* Cible = Cast<APawn>(Touche))
		{
			return Cible;
		}
		Touche = Touche->GetAttachParentActor() ? Touche->GetAttachParentActor() : Touche->GetOwner();
	}
	return nullptr;
}

const TArray<FString>& APouvoirsChelles::ApparencesPour(USkeletalMeshComponent* Corps)
{
	static const TArray<FString> Vide;
	if (!Corps || !Corps->SkeletalMesh || !Corps->SkeletalMesh->GetSkeleton())
	{
		return Vide;
	}
	const FString Squelette = Corps->SkeletalMesh->GetSkeleton()->GetPathName();
	if (const TArray<FString>* Deja = ApparencesParSquelette.Find(Squelette))
	{
		return *Deja;
	}
	TArray<FString>& Liste = ApparencesParSquelette.Add(Squelette);
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Meshes;
	Registre.GetAssetsByClass(USkeletalMesh::StaticClass()->GetFName(), Meshes, true);
	const TCHAR* Exclus[] = { TEXT("Arms"), TEXT("FP_"), TEXT("FPP"), TEXT("Hand"), TEXT("Weapon"), TEXT("Head"), TEXT("Hair"), TEXT("Shoe"), TEXT("Hat"), TEXT("Glove"), TEXT("Upper"), TEXT("Lower"), TEXT("Legs"), TEXT("Apercu") };
	for (const FAssetData& A : Meshes)
	{
		FString SonSquelette;
		if (!A.GetTagValue(FName(TEXT("Skeleton")), SonSquelette) || !SonSquelette.Contains(Squelette))
		{
			continue;
		}
		const FString Nom = A.AssetName.ToString();
		bool bExclu = false;
		for (const TCHAR* Mot : Exclus)
		{
			if (Nom.Contains(Mot))
			{
				bExclu = true;
				break;
			}
		}
		if (!bExclu)
		{
			Liste.Add(A.ObjectPath.ToString());
		}
	}
	Liste.Sort();
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] CAMELEON : %d apparences compatibles avec %s."), Liste.Num(), *Squelette);
	return Liste;
}

bool APouvoirsChelles::ServeurCameleon(APouvoirsRelais* Relais, APawn* Pion)
{
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (!Perso || !Perso->GetMesh())
	{
		Relais->ClientAvis(TEXT("CAM\u00C9L\u00C9ON marche seulement \u00E0 pied."));
		return false;
	}
	const TArray<FString>& Liste = ApparencesPour(Perso->GetMesh());
	const FString Origine = Perso->GetMesh()->SkeletalMesh ? Perso->GetMesh()->SkeletalMesh->GetPathName() : FString();
	if (Liste.Num() == 0 || (Liste.Num() == 1 && Liste[0] == Origine))
	{
		Relais->ClientAvis(TEXT("Aucune autre apparence compatible trouv\u00E9e pour ce personnage."));
		return false;
	}
	// Deguise -> on redevient soi-meme. Sinon -> une apparence au hasard (differente de l'origine).
	if (!Relais->Deguisement.IsEmpty())
	{
		Relais->IndexDeguisement = 0;
		Relais->Deguisement.Empty();
		Relais->ClientAvis(TEXT("CAM\u00C9L\u00C9ON : tu reprends ton apparence."));
		return true;
	}
	int32 Choix = FMath::RandRange(0, Liste.Num() - 1);
	if (Liste[Choix] == Origine)
	{
		Choix = (Choix + 1) % Liste.Num();
	}
	Relais->IndexDeguisement = Choix + 1;
	Relais->Deguisement = Liste[Choix];
	Relais->ClientAvis(FString::Printf(TEXT("CAM\u00C9L\u00C9ON : te voil\u00E0 en %s  (P = redevenir toi)"),
		*FPackageName::ObjectPathToObjectName(Relais->Deguisement)));
	return true;
}

void APouvoirsChelles::ExecuterServeur(APouvoirsRelais* Relais, int32 Quel, FVector Origine, FRotator Visee)
{
	if (!Relais || Quel < 0 || Quel >= NombrePouvoirs)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	const FInfoPouvoir& P = Pouvoir(Quel);
	if (!PermisPourPerso(Quel, Relais->PouvoirPerso == 255 ? -1 : int32(Relais->PouvoirPerso)))
	{
		Relais->ClientAvis(FString::Printf(TEXT("%s n'est pas le pouvoir de ce personnage."), P.Nom));
		return;
	}
	if (!EstDebloqueXP(Quel, Relais->XP))
	{
		Relais->ClientAvis(FString::Printf(TEXT("%s pas encore d\u00E9bloqu\u00E9 (niveau %d)."), P.Nom, P.Niveau));
		return;
	}
	if (Relais->RestantRecharge(Quel) > 0.f)
	{
		return;
	}
	// Point de visee trop loin du personnage : on prend sa tete (anti-triche simple)
	if (FVector::Dist(Origine, Pion->GetActorLocation()) > 1000.f)
	{
		Origine = Pion->GetPawnViewLocation();
		Visee = PC->GetControlRotation();
	}
	const FVector Direction = Visee.Vector();
	const float Maintenant = Relais->Maintenant();

	switch (Quel)
	{
	case Envol:
		if (!Cast<ACharacter>(Pion))
		{
			Relais->ClientAvis(TEXT("Descends du v\u00E9hicule pour t'envoler."));
			return;
		}
		break;

	case BouleDeFeu:
	{
		FActorSpawnParameters Params;
		Params.Owner = Pion;
		Params.Instigator = Pion;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABouleDeFeuChelles* Boule = GetWorld()->SpawnActor<ABouleDeFeuChelles>(ABouleDeFeuChelles::StaticClass(),
			Origine + Direction * 110.f, Visee, Params);
		if (!Boule)
		{
			return;
		}
		Boule->bToucherJoueurs = PvpActif();
		break;
	}

	case Verre:
		break;

	case Cameleon:
		if (!ServeurCameleon(Relais, Pion))
		{
			return;
		}
		break;

	case Gel:
	{
		APawn* Cible = ViserCible(Pion, Origine, Direction);
		if (!Cible)
		{
			Relais->ClientAvis(TEXT("GEL : vise un joueur ou un passant (30 m maximum)."));
			return;
		}
		if (Cible->IsPlayerControlled() && !PvpActif())
		{
			Relais->ClientAvis(TEXT("GEL : le PVP est coup\u00E9 dans cette partie, impossible de geler un joueur."));
			return;
		}
		Relais->CibleGel = Cible;
		if (APlayerController* Victime = Cast<APlayerController>(Cible->GetController()))
		{
			for (TActorIterator<APouvoirsRelais> It(GetWorld()); It; ++It)
			{
				if (It->GetOwner() == Victime)
				{
					It->ClientAvis(FString::Printf(TEXT("%s t'a gel\u00E9 !"),
						PC->PlayerState ? *PC->PlayerState->GetPlayerName() : TEXT("Un joueur")));
				}
			}
		}
		break;
	}

	default:
		return;
	}

	Relais->FinEffet[Quel] = Maintenant + P.Duree;
	Relais->FinRecharge[Quel] = Maintenant + P.Duree + P.Recharge;
	Relais->ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] %s utilise %s."), *PC->GetName(), P.Nom);
}

// ---------------------------------------------------------------------------
// Effets vus sur cette machine (pour tous les joueurs)
// ---------------------------------------------------------------------------

bool APouvoirsChelles::AppliquerApparence(APawn* Pion, const FString& Chemin)
{
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (!Perso || Chemin.IsEmpty())
	{
		return false;
	}
	USkeletalMeshComponent* Corps = Perso->GetMesh();
	if (!Corps || !Corps->SkeletalMesh)
	{
		return false;
	}
	FDeguisementChelles* D = Deguises.Find(Pion);
	if (!D || D->Chemin != Chemin)
	{
		USkeletalMesh* Mesh = FindObject<USkeletalMesh>(nullptr, *Chemin);
		if (!Mesh)
		{
			Mesh = LoadObject<USkeletalMesh>(nullptr, *Chemin);
			if (Mesh)
			{
				MeshesCharges.AddUnique(Mesh);
			}
		}
		if (!Mesh || Mesh->GetSkeleton() != Corps->SkeletalMesh->GetSkeleton())
		{
			return false;
		}
		if (!D)
		{
			USkeletalMeshComponent* Copie = NewObject<USkeletalMeshComponent>(Perso, MakeUniqueObjectName(Perso, USkeletalMeshComponent::StaticClass(), TEXT("ChellesCameleon")));
			Copie->SetupAttachment(Corps);
			Copie->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Copie->SetOwnerNoSee(Corps->bOwnerNoSee);
			Copie->SetOnlyOwnerSee(Corps->bOnlyOwnerSee);
			Copie->SetSkeletalMesh(Mesh);
			Copie->RegisterComponent();
			Copie->SetMasterPoseComponent(Corps);

			FDeguisementChelles Nouveau;
			Nouveau.Copie = Copie;
			Nouveau.Corps = Corps;
			Nouveau.AncienTickAnim = uint8(Corps->VisibilityBasedAnimTickOption);
			Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			D = &Deguises.Add(Pion, MoveTemp(Nouveau));
		}
		else if (D->Copie.IsValid())
		{
			D->Copie->SetSkeletalMesh(Mesh, false);
		}
		D->Chemin = Chemin;
	}

	// L'ancienne apparence (corps + morceaux d'habillage qui le suivent) reste cachee
	TArray<USkinnedMeshComponent*> Peaux;
	Perso->GetComponents<USkinnedMeshComponent>(Peaux);
	for (USkinnedMeshComponent* S : Peaux)
	{
		if (!S || S == D->Copie.Get() || S->bOnlyOwnerSee)
		{
			continue;
		}
		const bool bAncienne = (S == Corps) || (S->MasterPoseComponent.Get() == Corps);
		if (bAncienne && !S->bHiddenInGame)
		{
			S->SetHiddenInGame(true);
			D->MorceauxCaches.AddUnique(S);
		}
	}
	return true;
}

void APouvoirsChelles::RetirerApparence(APawn* Pion)
{
	FDeguisementChelles* D = Deguises.Find(Pion);
	if (!D)
	{
		return;
	}
	if (D->Copie.IsValid())
	{
		D->Copie->DestroyComponent();
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& C : D->MorceauxCaches)
	{
		if (C.IsValid())
		{
			C->SetHiddenInGame(false);
		}
	}
	if (D->Corps.IsValid())
	{
		D->Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption(D->AncienTickAnim);
	}
	Deguises.Remove(Pion);
}

void APouvoirsChelles::Degeler(APawn* Pion)
{
	TWeakObjectPtr<UPointLightComponent>* Lumiere = Geles.Find(Pion);
	if (Lumiere && Lumiere->IsValid())
	{
		(*Lumiere)->DestroyComponent();
	}
	Geles.Remove(Pion);
	if (!Pion)
	{
		return;
	}
	Pion->CustomTimeDilation = 1.f;
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (Perso && (EstServeur() || Perso->IsLocallyControlled()))
	{
		UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement();
		if (Mouvement && Mouvement->MovementMode == MOVE_None)
		{
			Mouvement->SetMovementMode(MOVE_Falling);
		}
	}
	if (EstServeur())
	{
		if (AAIController* IA = Cast<AAIController>(Pion->GetController()))
		{
			if (UBrainComponent* Cerveau = IA->GetBrainComponent())
			{
				Cerveau->ResumeLogic(TEXT("Gel Chelles"));
			}
		}
	}
}

void APouvoirsChelles::AppliquerEffets(float DeltaSeconds)
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const bool bServeur = EstServeur();
	APlayerController* MonPC = GetPC();

	TSet<APawn*> DoiventVoler;
	TSet<APawn*> DoiventGeler;
	TSet<UPrimitiveComponent*> ACacher;
	TSet<APawn*> Deguisables;
	float Heure = 0.f;

	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		APouvoirsRelais* R = *It;
		Heure = R->Maintenant();
		APawn* Pion = R->Pion;
		if (bServeur)
		{
			if (APlayerController* PC = Cast<APlayerController>(R->GetOwner()))
			{
				Pion = PC->GetPawn(); // le serveur connait le pion a jour
			}
		}

		if (R->EffetActif(Gel))
		{
			if (APawn* Cible = Cast<APawn>(R->CibleGel))
			{
				DoiventGeler.Add(Cible);
			}
		}
		if (!Pion)
		{
			continue;
		}
		if (R->EffetActif(Envol) && Cast<ACharacter>(Pion) && (bServeur || Pion->IsLocallyControlled()))
		{
			DoiventVoler.Add(Pion);
		}
		if (R->EffetActif(Verre) && !Pion->IsLocallyControlled())
		{
			TArray<AActor*> Acteurs;
			ActeursDuPion(Pion, Acteurs);
			for (AActor* A : Acteurs)
			{
				TArray<UPrimitiveComponent*> Morceaux;
				A->GetComponents<UPrimitiveComponent>(Morceaux);
				for (UPrimitiveComponent* C : Morceaux)
				{
					ACacher.Add(C);
				}
			}
		}
		if (AppliquerApparence(Pion, R->Deguisement))
		{
			Deguisables.Add(Pion);
		}
	}

	// --- ENVOL ---
	for (APawn* Pion : DoiventVoler)
	{
		if (DoiventGeler.Contains(Pion))
		{
			continue; // le gel gagne
		}
		ACharacter* Perso = Cast<ACharacter>(Pion);
		UCharacterMovementComponent* Mouvement = Perso ? Perso->GetCharacterMovement() : nullptr;
		if (!Mouvement)
		{
			continue;
		}
		if (Mouvement->MovementMode != MOVE_Flying)
		{
			Mouvement->SetMovementMode(MOVE_Flying);
			Mouvement->MaxFlySpeed = FMath::Max(Mouvement->MaxFlySpeed, 1400.f);
			Mouvement->BrakingDecelerationFlying = FMath::Max(Mouvement->BrakingDecelerationFlying, 1500.f);
		}
		Volants.Add(Pion);
		if (MonPC && Perso->IsLocallyControlled())
		{
			float Vertical = 0.f;
			if (MonPC->IsInputKeyDown(EKeys::SpaceBar) || MonPC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom))
			{
				Vertical += 1.f;
			}
			if (MonPC->IsInputKeyDown(EKeys::LeftControl) || MonPC->IsInputKeyDown(EKeys::C) || MonPC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Right))
			{
				Vertical -= 1.f;
			}
			if (Vertical != 0.f)
			{
				Perso->AddMovementInput(FVector::UpVector, Vertical);
			}
		}
	}
	for (auto ItV = Volants.CreateIterator(); ItV; ++ItV)
	{
		APawn* Pion = ItV->Get();
		if (Pion && DoiventVoler.Contains(Pion) && !DoiventGeler.Contains(Pion))
		{
			continue;
		}
		if (ACharacter* Perso = Cast<ACharacter>(Pion))
		{
			if (Perso->GetCharacterMovement() && Perso->GetCharacterMovement()->MovementMode == MOVE_Flying)
			{
				Perso->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
			}
		}
		ItV.RemoveCurrent();
	}

	// --- EFFET VERRE : invisible, avec un reflet bref toutes les 1,7 s ---
	const bool bReflet = FMath::Fmod(Heure, 1.7f) < 0.08f;
	for (UPrimitiveComponent* C : ACacher)
	{
		const TWeakObjectPtr<UPrimitiveComponent> Faible(C);
		if (!MorceauxCaches.Contains(Faible))
		{
			if (C->bHiddenInGame)
			{
				continue; // deja cache par le jeu : on n'y touche pas
			}
			MorceauxCaches.Add(Faible);
		}
		const bool bDejaCache = C->bHiddenInGame;
		if (bDejaCache == bReflet)
		{
			C->SetHiddenInGame(!bReflet);
		}
	}
	for (auto ItC = MorceauxCaches.CreateIterator(); ItC; ++ItC)
	{
		UPrimitiveComponent* C = ItC->Get();
		if (C && ACacher.Contains(C))
		{
			continue;
		}
		if (C)
		{
			C->SetHiddenInGame(false);
		}
		ItC.RemoveCurrent();
	}

	// --- GEL ---
	for (APawn* Cible : DoiventGeler)
	{
		if (!Geles.Contains(Cible))
		{
			Cible->CustomTimeDilation = 0.02f; // l'animation se fige
			UPointLightComponent* Lumiere = NewObject<UPointLightComponent>(Cible, MakeUniqueObjectName(Cible, UPointLightComponent::StaticClass(), TEXT("ChellesGel")));
			Lumiere->SetupAttachment(Cible->GetRootComponent());
			Lumiere->SetIntensity(5000.f);
			Lumiere->SetLightColor(FLinearColor(0.45f, 0.75f, 1.f));
			Lumiere->SetAttenuationRadius(320.f);
			Lumiere->SetCastShadows(false);
			Lumiere->RegisterComponent();
			Geles.Add(Cible, Lumiere);
			if (bServeur)
			{
				if (AAIController* IA = Cast<AAIController>(Cible->GetController()))
				{
					IA->StopMovement();
					if (UBrainComponent* Cerveau = IA->GetBrainComponent())
					{
						Cerveau->PauseLogic(TEXT("Gel Chelles"));
					}
				}
			}
		}
		ACharacter* Perso = Cast<ACharacter>(Cible);
		if (Perso && (bServeur || Perso->IsLocallyControlled()))
		{
			UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement();
			if (Mouvement && Mouvement->MovementMode != MOVE_None)
			{
				Mouvement->StopMovementImmediately();
				Mouvement->DisableMovement();
			}
		}
	}
	TArray<TWeakObjectPtr<APawn>> AncienGeles;
	Geles.GetKeys(AncienGeles);
	for (const TWeakObjectPtr<APawn>& Faible : AncienGeles)
	{
		APawn* Pion = Faible.Get();
		if (!Pion || !DoiventGeler.Contains(Pion))
		{
			if (Pion)
			{
				Degeler(Pion);
			}
			else
			{
				Geles.Remove(Faible);
			}
		}
	}

	// --- CAMELEON : rendre l'apparence d'origine a ceux qui ne sont plus deguises ---
	TArray<TWeakObjectPtr<APawn>> AnciensDeguises;
	Deguises.GetKeys(AnciensDeguises);
	for (const TWeakObjectPtr<APawn>& Faible : AnciensDeguises)
	{
		APawn* Pion = Faible.Get();
		if (!Pion)
		{
			Deguises.Remove(Faible);
		}
		else if (!Deguisables.Contains(Pion))
		{
			RetirerApparence(Pion);
		}
	}
}

// ---------------------------------------------------------------------------
// Affichage
// ---------------------------------------------------------------------------

void APouvoirsChelles::CreerHud()
{
	if (Hud.IsValid() || !bAfficher || GetNetMode() == NM_DedicatedServer || !GetPC())
	{
		return;
	}
	UWorld* W = GetWorld();
	UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr;
	if (!Vue)
	{
		return;
	}
	Hud = SNew(SPouvoirsHud);
	Vue->AddViewportWidgetContent(Hud.ToSharedRef(), 4);
}

void APouvoirsChelles::RetirerHud()
{
	if (!Hud.IsValid())
	{
		return;
	}
	UWorld* W = GetWorld();
	if (UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Hud.ToSharedRef());
	}
	Hud.Reset();
}

void APouvoirsChelles::MajHud()
{
	if (!Hud.IsValid())
	{
		return;
	}
	FEtatPouvoirsHud& E = Hud->Etat;
	APlayerController* PC = GetPC();
	E.bVisible = bAfficher && PC && PC->GetPawn() && !PC->IsPaused();
	E.XP = XPLocale;
	E.Niveau = NiveauPourXP(XPLocale);
	E.Selection = Selection;
	E.Banniere = Banniere;
	E.SousBanniere = SousBanniere;
	E.AgeBanniere = AgeBanniere;
	E.Info = Info;
	E.AgeInfo = AgeInfo;
	E.MoiVerre = 0.f;
	E.MoiGele = 0.f;
	APouvoirsRelais* Mien = MonRelais();
	for (int32 i = 0; i < NombrePouvoirs; ++i)
	{
		E.Recharge[i] = Mien ? Mien->RestantRecharge(i) : 0.f;
		E.Effet[i] = (Mien && i != Cameleon) ? Mien->RestantEffet(i) : 0.f;
	}
	if (Mien)
	{
		E.MoiVerre = Mien->RestantEffet(Verre);
	}
	APawn* MonPion = PC ? PC->GetPawn() : nullptr;
	if (MonPion)
	{
		for (TActorIterator<APouvoirsRelais> It(GetWorld()); It; ++It)
		{
			if (It->CibleGel == MonPion)
			{
				E.MoiGele = FMath::Max(E.MoiGele, It->RestantEffet(Gel));
			}
		}
	}
}

void APouvoirsChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Reel = float(FApp::GetDeltaTime());
	AgeBanniere += Reel;
	AgeInfo += Reel;

	if (!bLieClavier)
	{
		LierClavier();
	}
	if (!Hud.IsValid())
	{
		CreerHud();
	}

	const float Horloge = W->GetRealTimeSeconds();
	if (EstServeur() && Horloge >= ProchaineGestion)
	{
		ProchaineGestion = Horloge + 0.5f;
		GererRelais();
	}

	// XP de combat : le serveur suit les ennemis (nouveaux chaque seconde) et regarde qui est mort
	if (EstServeur())
	{
		if (Horloge >= ProchainSuivi)
		{
			ProchainSuivi = Horloge + 1.f;
			SuivreEnnemis();
		}
		if (Horloge >= ProchaineVerif && CiblesTouchees.Num() > 0)
		{
			ProchaineVerif = Horloge + 0.2f;
			VerifierMorts(W->GetTimeSeconds());
		}
	}
	// Coups qui ont touche : comptes toutes les 2 s
	if (XPCombatEnAttente > 0 && Horloge >= ProchainBilanCombat)
	{
		ProchainBilanCombat = Horloge + 2.f;
		const int32 Gain = XPCombatEnAttente;
		XPCombatEnAttente = 0;
		DonnerXP(Gain, false);
		Message(FString::Printf(TEXT("+%d XP : ennemi touché"), Gain));
	}

	if (bCharge && GetPC())
	{
		if (EnJeu())
		{
			CompterTemps(Reel);
		}
		APouvoirsRelais* R = MonRelais();
		if (R && R->XP != XPLocale && Horloge >= ProchainEnvoi)
		{
			ProchainEnvoi = Horloge + 1.f;
			R->ServeurXP(XPLocale);
		}
	}

	if (Horloge >= ProchainPerso)
	{
		ProchainPerso = Horloge + 1.f;
		SuivrePerso(Horloge);
	}
	if (!W->IsPaused())
	{
		SuivreVolPourTous(DeltaSeconds);
		AppliquerEffets(DeltaSeconds);
		if (Horloge >= ProchaineFonction)
		{
			ProchaineFonction = Horloge + 0.25f;
			AppliquerFonctions();
		}
		if (EstServeur() && Horloge >= ProchaineRegen)
		{
			ProchaineRegen = Horloge + 1.f;
			Regenerer();
		}
	}
	MajHud();
}

// ---------------------------------------------------------------------------
// 24/11/2026 (demande MERIDJA) : on choisit son personnage dans le lobby avant de jouer, et chaque
// personnage a UN pouvoir (touche P) et UNE fonction (avantage permanent). Tout le monde vole.
// Le partage se regle dans Plugins\ChellesPouvoirs\Personnages.txt (Bloc-notes, sans recompiler).
// Le lobby (plugin ChellesMenu) ecrit le nom du personnage dans [ChellesPerso] Nom.
// ---------------------------------------------------------------------------

namespace
{
	FString PersoNormaliser(const FString& Brut)
	{
		FString R;
		for (TCHAR C : Brut.ToUpper())
		{
			switch (C)
			{
			case 0x00C9: case 0x00C8: case 0x00CA: case 0x00CB: case 0x00E9: case 0x00E8: case 0x00EA: case 0x00EB: C = TEXT('E'); break;
			case 0x00C0: case 0x00C2: case 0x00C4: case 0x00E0: case 0x00E2: case 0x00E4: C = TEXT('A'); break;
			case 0x00D4: case 0x00D6: case 0x00F4: case 0x00F6: C = TEXT('O'); break;
			case 0x00CE: case 0x00CF: case 0x00EE: case 0x00EF: C = TEXT('I'); break;
			case 0x00D9: case 0x00DB: case 0x00DC: case 0x00F9: case 0x00FB: case 0x00FC: C = TEXT('U'); break;
			case 0x00C7: case 0x00E7: C = TEXT('C'); break;
			default: break;
			}
			if (FChar::IsAlnum(C))
			{
				R.AppendChar(C);
			}
		}
		return R;
	}

	int32 PersoPouvoirDepuisNom(const FString& Nom)
	{
		const FString N = PersoNormaliser(Nom);
		if (N.Contains(TEXT("FEU"))) return int32(BouleDeFeu);
		if (N.Contains(TEXT("VERRE"))) return int32(Verre);
		if (N.Contains(TEXT("CAMELEON"))) return int32(Cameleon);
		if (N.Contains(TEXT("GEL")) || N.Contains(TEXT("GLACE"))) return int32(Gel);
		if (N.Contains(TEXT("ENVOL")) || N.Contains(TEXT("VOL"))) return int32(Envol);
		return -2; // AUCUN ou inconnu : seulement le vol
	}

	int32 PersoFonctionDepuisNom(const FString& Nom)
	{
		const FString N = PersoNormaliser(Nom);
		if (N.Contains(TEXT("VITESSE"))) return int32(Vitesse);
		if (N.Contains(TEXT("SAUT"))) return int32(SuperSaut);
		if (N.Contains(TEXT("VOLRAPIDE"))) return int32(VolRapide);
		if (N.Contains(TEXT("PLANEUR"))) return int32(Planeur);
		if (N.Contains(TEXT("REGEN"))) return int32(Regeneration);
		if (N.Contains(TEXT("XP"))) return int32(DoubleXP);
		return int32(SansFonction);
	}

	/** Vie du personnage par reflexion (memes noms que ChellesHud). */
	FNumericProperty* PersoProprieteVie(UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			if (FNumericProperty* P = FindFProperty<FNumericProperty>(Objet->GetClass(), FName(Nom)))
			{
				return P;
			}
		}
		return nullptr;
	}

	double PersoLireNombre(UObject* Objet, FNumericProperty* P)
	{
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Adresse) : double(P->GetSignedIntPropertyValue(Adresse));
	}

	void PersoEcrireNombre(UObject* Objet, FNumericProperty* P, double Valeur)
	{
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Adresse, Valeur);
		}
		else
		{
			P->SetIntPropertyValue(Adresse, int64(FMath::RoundToInt(float(Valeur))));
		}
	}
}

void APouvoirsChelles::LirePersonnages()
{
	PersosPouvoirs.Reset();
	PersoParDefaut = TPair<int32, int32>(int32(BouleDeFeu), int32(VolRapide));
	auto Ajouter = [this](const TCHAR* Nom, int32 P, int32 F) { PersosPouvoirs.Add(PersoNormaliser(Nom), TPair<int32, int32>(P, F)); };
	// Partage par defaut (si Personnages.txt manque)
	Ajouter(TEXT("SOLDAT"), BouleDeFeu, Regeneration);
	Ajouter(TEXT("SPIDER-MALE"), Gel, SuperSaut);
	Ajouter(TEXT("SPIDER-FEMELLE"), Verre, Vitesse);
	Ajouter(TEXT("PANTERA"), Cameleon, Planeur);
	Ajouter(TEXT("FEMININ"), Gel, Vitesse);
	Ajouter(TEXT("MASCULIN"), BouleDeFeu, SuperSaut);
	Ajouter(TEXT("LENA"), Verre, VolRapide);
	Ajouter(TEXT("SOFIA"), Cameleon, Vitesse);
	Ajouter(TEXT("LUCIA"), Gel, Planeur);
	Ajouter(TEXT("FLASH ECLAIR"), Gel, Regeneration);
	Ajouter(TEXT("FLAMME FIRE"), BouleDeFeu, Planeur);
	Ajouter(TEXT("SUPERPOWERS"), Verre, SuperSaut);
	Ajouter(TEXT("DYNAMIC COMBAT"), Cameleon, DoubleXP);
	Ajouter(TEXT("MOTONEIGE"), -2, Vitesse);
	Ajouter(TEXT("VOITURE"), -2, Vitesse);
	Ajouter(TEXT("HORSE"), -2, Vitesse);
	Ajouter(TEXT("CHEVAL"), -2, Vitesse);

	const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesPouvoirs"), TEXT("Personnages.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
	{
		UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] %s absent : partage des pouvoirs par defaut."), *Chemin);
		return;
	}
	int32 Lus = 0;
	for (const FString& Brute : Lignes)
	{
		const FString Ligne = Brute.TrimStartAndEnd();
		FString Nom, Reste, TextePouvoir, TexteFonction;
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Nom, &Reste))
		{
			continue;
		}
		if (!Reste.Split(TEXT("|"), &TextePouvoir, &TexteFonction))
		{
			TextePouvoir = Reste;
		}
		const TPair<int32, int32> Valeur(PersoPouvoirDepuisNom(TextePouvoir), PersoFonctionDepuisNom(TexteFonction));
		const FString Cle = PersoNormaliser(Nom);
		if (Cle.IsEmpty())
		{
			continue;
		}
		if (Cle == TEXT("DEFAUT"))
		{
			PersoParDefaut = Valeur;
		}
		else
		{
			PersosPouvoirs.Add(Cle, Valeur);
		}
		++Lus;
	}
	UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Personnages.txt : %d personnages lus."), Lus);
}

void APouvoirsChelles::SuivrePerso(float Horloge)
{
	FReglagesPouvoirs& Reg = Reglages();
	if (!Reg.bUnPouvoirParPerso)
	{
		Reg.PouvoirPerso = -1;
		Reg.FonctionPerso = 0;
		return;
	}
	FString Nom = TEXT("SOLDAT");
	if (GConfig)
	{
		FString Lu;
		if (GConfig->GetString(TEXT("ChellesPerso"), TEXT("Nom"), Lu, GGameUserSettingsIni) && !Lu.TrimStartAndEnd().IsEmpty())
		{
			Nom = Lu.TrimStartAndEnd();
		}
	}
	if (Nom != PersoLu)
	{
		PersoLu = Nom;
		const TPair<int32, int32>* Trouve = PersosPouvoirs.Find(PersoNormaliser(Nom));
		const TPair<int32, int32> Choix = Trouve ? *Trouve : PersoParDefaut;
		// -2 = aucun pouvoir a part le vol : on garde Envol comme seul pouvoir permis
		Reg.PouvoirPerso = Choix.Key < 0 ? int32(Envol) : Choix.Key;
		Reg.FonctionPerso = FMath::Clamp(Choix.Value, 0, int32(NombreFonctions) - 1);
		Reg.NomPerso = Nom;
		Selection = Reg.PouvoirPerso;
		FString Texte = FString::Printf(TEXT("%s : pouvoir %s (P)"), *Nom.ToUpper(), Pouvoir(Reg.PouvoirPerso).Nom);
		if (Reg.PouvoirPerso == int32(Envol))
		{
			Texte = FString::Printf(TEXT("%s : pas de pouvoir spécial"), *Nom.ToUpper());
		}
		if (Reg.FonctionPerso != int32(SansFonction))
		{
			Texte += FString::Printf(TEXT(" + fonction %s : %s"), NomFonction(Reg.FonctionPerso), AideFonction(Reg.FonctionPerso));
		}
		Texte += TEXT(". Tout le monde vole : ESPACE en l'air.");
		Message(Texte);
		UE_LOG(LogTemp, Log, TEXT("[Pouvoirs] Personnage %s : pouvoir %d, fonction %d"), *Nom, Reg.PouvoirPerso, Reg.FonctionPerso);
	}
	APouvoirsRelais* R = MonRelais();
	if (R && (R->PouvoirPerso != uint8(Reg.PouvoirPerso) || R->FonctionPerso != uint8(Reg.FonctionPerso)))
	{
		if (EstServeur())
		{
			R->PouvoirPerso = uint8(Reg.PouvoirPerso);
			R->FonctionPerso = uint8(Reg.FonctionPerso);
		}
		else
		{
			R->ServeurPerso(uint8(Reg.PouvoirPerso), uint8(Reg.FonctionPerso));
		}
	}
}

void APouvoirsChelles::AppliquerFonctions()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const APouvoirsRelais* Mien = MonRelais();
	TSet<APawn*> Vus;
	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		APouvoirsRelais* R = *It;
		APawn* Pion = R->Pion;
		if (EstServeur())
		{
			if (APlayerController* PC = Cast<APlayerController>(R->GetOwner()))
			{
				Pion = PC->GetPawn();
			}
		}
		ACharacter* Perso = Cast<ACharacter>(Pion);
		UCharacterMovementComponent* Mouv = Perso ? Perso->GetCharacterMovement() : nullptr;
		if (!Mouv)
		{
			continue;
		}
		Vus.Add(Pion);
		// Mon propre perso : la valeur locale est a jour tout de suite (le relais suit une seconde apres)
		uint8 Fonction = (R == Mien) ? uint8(Reglages().FonctionPerso) : R->FonctionPerso;
		if (!Reglages().bUnPouvoirParPerso)
		{
			Fonction = 0;
		}
		FSuiviFonction& S = SuivisFonction.FindOrAdd(TWeakObjectPtr<APawn>(Pion));
		auto Proche = [](float A, float B) { return FMath::Abs(A - B) <= FMath::Max(1.f, FMath::Abs(B) * 0.01f); };
		// Annule l'ancienne fonction quand le perso en change
		if (S.Fonction != Fonction)
		{
			if (S.Marche > 0.f && Proche(Mouv->MaxWalkSpeed, S.Marche)) Mouv->MaxWalkSpeed /= 1.5f;
			if (S.Saut > 0.f && Proche(Mouv->JumpZVelocity, S.Saut)) Mouv->JumpZVelocity /= 1.8f;
			if (S.Vol > 0.f && Proche(Mouv->MaxFlySpeed, S.Vol)) Mouv->MaxFlySpeed /= 2.f;
			if (S.Gravite > 0.f && Proche(Mouv->GravityScale, S.Gravite)) Mouv->GravityScale /= 0.5f;
			S = FSuiviFonction();
			S.Fonction = Fonction;
		}
		// Applique (ou re-applique si un autre plugin a remis la valeur de base)
		auto Suivre = [&Proche](float& Valeur, float& Ecrit, float Facteur)
		{
			if (Ecrit < 0.f || !Proche(Valeur, Ecrit))
			{
				Valeur *= Facteur;
				Ecrit = Valeur;
			}
		};
		switch (Fonction)
		{
		case Vitesse: Suivre(Mouv->MaxWalkSpeed, S.Marche, 1.5f); break;
		case SuperSaut: Suivre(Mouv->JumpZVelocity, S.Saut, 1.8f); break;
		case VolRapide: Suivre(Mouv->MaxFlySpeed, S.Vol, 2.f); break;
		case Planeur:
			Suivre(Mouv->GravityScale, S.Gravite, 0.5f);
			Mouv->AirControl = FMath::Max(Mouv->AirControl, 0.8f);
			break;
		default: break;
		}
	}
	for (auto It = SuivisFonction.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Vus.Contains(It.Key().Get()))
		{
			It.RemoveCurrent();
		}
	}
}

void APouvoirsChelles::Regenerer()
{
	UWorld* W = GetWorld();
	if (!W || !Reglages().bUnPouvoirParPerso)
	{
		return;
	}
	for (TActorIterator<APouvoirsRelais> It(W); It; ++It)
	{
		APouvoirsRelais* R = *It;
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		APawn* Pion = PC ? PC->GetPawn() : R->Pion;
		const uint8 Fonction = (PC && PC->IsLocalController()) ? uint8(Reglages().FonctionPerso) : R->FonctionPerso;
		if (!Pion || Fonction != uint8(Regeneration))
		{
			continue;
		}
		FNumericProperty* Vie = PersoProprieteVie(Pion, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie") });
		FNumericProperty* VieMax = PersoProprieteVie(Pion, { TEXT("HealthMax"), TEXT("MaxHealth"), TEXT("VieMax") });
		if (!Vie)
		{
			continue;
		}
		const double Actuelle = PersoLireNombre(Pion, Vie);
		const double Max = VieMax ? PersoLireNombre(Pion, VieMax) : 100.0;
		if (Actuelle <= 0.0 || Actuelle >= Max)
		{
			continue; // mort (on ne ressuscite pas) ou deja pleine
		}
		PersoEcrireNombre(Pion, Vie, FMath::Min(Max, Actuelle + 3.0));
	}
}
