// CHELLES 3D - Arsenal (2026-11-17) : blaster, bazooka guide, sabre lumineux, grenade a zombies
#include "ArsenalChelles.h"
#include "ArsenalDegats.h"
#include "ArsenalRelais.h"
#include "ArsenalVisuel.h"
#include "ArsenalVue.h"
#include "EffetArsenal.h"
#include "ProjectileArsenal.h"
#include "RatelierArsenal.h"
#include "ArmesEffets.h"
#include "SonArsenalChelles.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"
#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace ChellesArsenal;

namespace
{
	UObject* ArsenalLireObjet(UObject* Objet, FName Nom, bool* bExiste = nullptr)
	{
		FObjectPropertyBase* P = Objet ? FindFProperty<FObjectPropertyBase>(Objet->GetClass(), Nom) : nullptr;
		if (bExiste)
		{
			*bExiste = P != nullptr;
		}
		return P ? P->GetObjectPropertyValue_InContainer(Objet) : nullptr;
	}

	/** Acteur d'un autre plugin trouve par le nom de sa classe (garde en memoire, cherche au plus 1 fois par seconde). */
	AActor* ArsenalActeurParClasse(UWorld* Monde, const TCHAR* NomClasse, TWeakObjectPtr<AActor>& Memoire, float& ProchaineRecherche)
	{
		if (!Monde)
		{
			return nullptr;
		}
		if (Memoire.IsValid() && Memoire->GetWorld() == Monde)
		{
			return Memoire.Get();
		}
		const float Maintenant = Monde->GetRealTimeSeconds();
		if (Maintenant < ProchaineRecherche)
		{
			return nullptr;
		}
		ProchaineRecherche = Maintenant + 1.f;
		for (TActorIterator<AActor> It(Monde); It; ++It)
		{
			if (It->GetClass()->GetName() == NomClasse)
			{
				Memoire = *It;
				return *It;
			}
		}
		return nullptr;
	}

	TWeakObjectPtr<AActor> GArsenalPouvoirs;
	float GArsenalPouvoirsRecherche = 0.f;

	bool ArsenalLireEntier(UObject* Objet, FName Nom, int32& Valeur)
	{
		FIntProperty* P = Objet ? FindFProperty<FIntProperty>(Objet->GetClass(), Nom) : nullptr;
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	FBoolProperty* ArsenalProprieteProfondeur()
	{
		static FBoolProperty* P = FindFProperty<FBoolProperty>(UPrimitiveComponent::StaticClass(), TEXT("bRenderInDepthPass"));
		return P;
	}

	bool ArsenalLireProfondeur(UPrimitiveComponent* C)
	{
		FBoolProperty* P = ArsenalProprieteProfondeur();
		return P ? P->GetPropertyValue_InContainer(C) : true;
	}

	void ArsenalEcrireProfondeur(UPrimitiveComponent* C, bool bOui)
	{
		FBoolProperty* P = ArsenalProprieteProfondeur();
		if (P && P->GetPropertyValue_InContainer(C) != bOui)
		{
			P->SetPropertyValue_InContainer(C, bOui);
			C->MarkRenderStateDirty();
		}
	}
}

// =====================================================================================
//  Demarrage / fin
// =====================================================================================

AArsenalChelles::AArsenalChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // apres les Blueprints (on recache l'arme normale)
	bReplicates = false; // un par machine : ce qui est partage passe par AArsenalRelais
	SetActorHiddenInGame(true);
}

void AArsenalChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages(Reglages);
	ChellesArsenalVisuel::DefinirLueur(Reglages.MateriauLueur);
	if (GetNetMode() != NM_DedicatedServer)
	{
		LierClavier();
		CreerEcran();
	}
	UE_LOG(LogTemp, Log, TEXT("[Arsenal] Pret. Touche %s = blaster / bazooka / sabre / grenade."), *Reglages.ToucheChanger);
}

void AArsenalChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	ArreterSonFlammes();
	RemontrerArmeNormale();
	BloquerTirNormal(nullptr, false);
	if (Vue)
	{
		Vue->Destroy();
		Vue = nullptr;
	}
	RetirerEcran();
	Super::EndPlay(Raison);
}

uint8 AArsenalChelles::ArmeLocale(UWorld* Monde)
{
	if (!Monde)
	{
		return Aucune;
	}
	for (TActorIterator<AArsenalChelles> It(Monde); It; ++It)
	{
		return It->Arme;
	}
	return Aucune;
}

void AArsenalChelles::Message(const FString& Texte)
{
	UE_LOG(LogTemp, Log, TEXT("[Arsenal] %s"), *Texte);
	Info = Texte;
	AgeInfo = 0.f;
}

float AArsenalChelles::Cadence(uint8 Quelle) const
{
	switch (Quelle)
	{
	case Blaster: return Reglages.BlasterCadence;
	case Bazooka: return Reglages.BazookaCadence;
	case Sabre:   return Reglages.SabreCadence;
	case Grenade: return Reglages.GrenadeCadence;
	case LanceFlammes: return Reglages.FlammesCadence;
	case Mitrailleuse: return Reglages.MitrailleuseCadence;
	default:      return 1.f;
	}
}

// =====================================================================================
//  Image par image
// =====================================================================================

void AArsenalChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AgeInfo += DeltaSeconds;
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();

	if (GetNetMode() != NM_Client && Maintenant >= ProchainRelais)
	{
		ProchainRelais = Maintenant + 0.5f;
		GererRelais();
		if (!bRateliersPoses && Maintenant > ProchainEssaiRateliers)
		{
			PoserRateliers();
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	LierClavier();
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	bCurseur = PC && PC->bShowMouseCursor;
	SuivreJoueur(PC, Pion);
	SuivreRateliers(PC, Pion);

	// Arme lourde vide : retour a l'arme normale
	if (EstArmeLourde(Arme) && !Reglages.bLourdesSansRatelier && ChargeLocale(Arme) <= 0 && MonRelais())
	{
		const uint8 Videe = Arme;
		Choisir(Aucune, false);
		Message(FString::Printf(TEXT("%s : plus de munitions. Trouve un autre ratelier sur un mur."), NomArme(Videe)));
	}
	if (!(bTenu && Arme == LanceFlammes))
	{
		ArreterSonFlammes();
	}

	if (Arme == Aucune)
	{
		Etiquette.Reset();
	}
	else
	{
		const TCHAR* Geste = (Arme == Sabre) ? TEXT("frapper") : (Arme == Grenade) ? TEXT("lancer") : (Arme == LanceFlammes) ? TEXT("bruler") : TEXT("tirer");
		const float Force = Puissance(PC);
		const FString Munitions = (EstArmeLourde(Arme) && !Reglages.bLourdesSansRatelier) ? FString::Printf(TEXT(" (%d)"), ChargeLocale(Arme)) : FString();
		const FString Bonus = Force > 1.001f ? FString::Printf(TEXT("   -   PUISSANCE +%d %%"), FMath::RoundToInt((Force - 1.f) * 100.f)) : FString();
		Etiquette = FString::Printf(TEXT("%s%s   -   clic gauche = %s   -   %s = arme suivante   -   molette = arme normale%s"),
			NomArme(Arme), *Munitions, Geste, *Reglages.ToucheChanger, *Bonus);
		MasquerArmeNormale(Pion);
		PlacerVue(Pion);
		if (bTenu && PC && !PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			bTenu = false;
		}
		if (bTenu && (Arme == Blaster || Arme == Sabre || Arme == Mitrailleuse || Arme == LanceFlammes))
		{
			EssayerTir(); // rafale tant que le clic est maintenu
		}
	}

	// BAZOOKA : le point vise est envoye au serveur tant qu'une fusee vole (elle le suit)
	if (PC && Pion && Maintenant < GuidageJusqua && Maintenant >= ProchaineVisee)
	{
		ProchaineVisee = Maintenant + 1.f / 15.f;
		FVector Camera, Avant, Cible;
		if (Visee(PC, Pion, Camera, Avant, Cible))
		{
			if (AArsenalRelais* R = MonRelais())
			{
				R->ServeurViser(Cible);
			}
		}
	}
}

// =====================================================================================
//  Joueur local
// =====================================================================================

APlayerController* AArsenalChelles::GetPC() const
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

AArsenalRelais* AArsenalChelles::MonRelais() const
{
	APlayerController* PC = GetPC();
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<AArsenalRelais> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

void AArsenalChelles::LierClavier()
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
	FKey Touche(*Reglages.ToucheChanger);
	if (!Touche.IsValid())
	{
		Touche = EKeys::Three;
	}
	FInputKeyBinding& B = InputComponent->BindKey(Touche, IE_Pressed, this, &AArsenalChelles::Changer);
	B.bConsumeInput = false;
	bLieClavier = true;
}

void AArsenalChelles::Changer()
{
	APlayerController* PC = GetPC();
	if (!PC || PC->bShowMouseCursor)
	{
		return; // menu ouvert
	}
	// Arme suivante, en sautant les armes lourdes pas encore prises (ou pas assez d'XP)
	uint8 Suivante = Arme;
	for (int32 i = 0; i < NombreArmes; ++i)
	{
		Suivante = uint8((Suivante + 1) % NombreArmes);
		if (Disponible(Suivante))
		{
			break;
		}
	}
	Choisir(Suivante, true);
}

void AArsenalChelles::Choisir(uint8 NouvelleArme, bool bMessage)
{
	if (NouvelleArme >= NombreArmes)
	{
		NouvelleArme = Aucune;
	}
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (NouvelleArme != Aucune)
	{
		if (!Reglages.bActif)
		{
			Message(TEXT("Arsenal coupe (Arsenal=Non dans Plugins\\ChellesArmes\\Arsenal.txt)."));
			return;
		}
		if (!Cast<ACharacter>(Pion))
		{
			Message(TEXT("Arsenal : seulement a pied (soldat ou heros), pas en vehicule."));
			return;
		}
		FString Raison;
		if (!Disponible(NouvelleArme, &Raison))
		{
			Message(Raison);
			return;
		}
	}

	RemontrerArmeNormale();
	Arme = NouvelleArme;
	bTenu = false;
	DernierTir = -100.f;

	if (Arme == Aucune)
	{
		BloquerTirNormal(PC, false);
		if (Vue)
		{
			Vue->Construire(Aucune, Reglages);
			Vue->SetActorHiddenInGame(true);
		}
		if (bMessage)
		{
			Message(TEXT("ARME NORMALE"));
		}
		return;
	}

	// On retient l'arme normale en main : si le joueur en change (molette), on lui rend ses armes normales
	ArmeNormale = Cast<AActor>(ArsenalLireObjet(Pion, TEXT("EquippedWeapon"), &bArmeNormaleConnue));
	PionSuivi = Pion;
	BloquerTirNormal(PC, true);
	if (!Vue)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Vue = GetWorld()->SpawnActor<AArsenalVue>(AArsenalVue::StaticClass(), FTransform::Identity, Params);
	}
	if (Vue)
	{
		Vue->SetActorHiddenInGame(false);
		Vue->Construire(Arme, Reglages);
	}
	PlacerVue(Pion);
	MasquerArmeNormale(Pion);

	if (bMessage)
	{
		switch (Arme)
		{
		case Blaster: Message(TEXT("BLASTER : maintiens le clic gauche pour tirer en rafale")); break;
		case Bazooka: Message(Reglages.bBazookaGuidee ? TEXT("BAZOOKA : tire, puis vise avec la souris : la fusee suit ton viseur")
			: TEXT("BAZOOKA : la fusee part tout droit et explose")); break;
		case Sabre:   Message(Reglages.bSabreTueZombies ? TEXT("SABRE LUMINEUX : clic gauche pour frapper, un coup tranche un zombie")
			: TEXT("SABRE LUMINEUX : clic gauche pour frapper")); break;
		case Grenade: Message(TEXT("GRENADE A ZOMBIES : lance-la, elle attire les zombies puis explose")); break;
		case LanceFlammes: Message(TEXT("LANCE-FLAMMES : maintiens le clic gauche pour cracher le feu (portee 9 m)")); break;
		case Mitrailleuse: Message(TEXT("MITRAILLEUSE LOURDE : maintiens le clic gauche pour tirer en rafale")); break;
		default: break;
		}
	}
}

void AArsenalChelles::SuivreJoueur(APlayerController* PC, APawn* Pion)
{
	if (Arme == Aucune)
	{
		PionSuivi = Pion;
		return;
	}
	// Nouveau personnage (roue TAB, vehicule, mort...) : retour a l'arme normale
	if (Pion != PionSuivi.Get() || !Cast<ACharacter>(Pion))
	{
		Choisir(Aucune, false);
		PionSuivi = Pion;
		return;
	}
	// Le joueur a pris une autre arme normale (molette, menu des armes...) : on lui laisse
	if (bArmeNormaleConnue)
	{
		AActor* Actuelle = Cast<AActor>(ArsenalLireObjet(Pion, TEXT("EquippedWeapon")));
		if (Actuelle != ArmeNormale.Get())
		{
			Choisir(Aucune, true);
		}
	}
}

void AArsenalChelles::BloquerTirNormal(APlayerController* PC, bool bOui)
{
	// Une couche de touches posee AU-DESSUS de celles du personnage : le clic gauche est garde pour l'arsenal,
	// l'action "Fire" du kit FPS_Game ne le recoit plus. Retiree des que l'arme normale revient.
	if (bOui && PC)
	{
		if (!EntreesTir)
		{
			EntreesTir = NewObject<UInputComponent>(this, TEXT("EntreesArsenal"));
			EntreesTir->Priority = 10;
			FInputKeyBinding& Presse = EntreesTir->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AArsenalChelles::ClicPresse);
			Presse.bConsumeInput = true;
			FInputKeyBinding& Relache = EntreesTir->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AArsenalChelles::ClicRelache);
			Relache.bConsumeInput = true;
		}
		if (PCBloque.Get() == PC)
		{
			return;
		}
		if (PCBloque.IsValid())
		{
			PCBloque->PopInputComponent(EntreesTir);
		}
		PC->PushInputComponent(EntreesTir);
		PCBloque = PC;
		return;
	}
	if (PCBloque.IsValid() && EntreesTir)
	{
		PCBloque->PopInputComponent(EntreesTir);
	}
	PCBloque.Reset();
}

void AArsenalChelles::MasquerArmeNormale(APawn* Pion)
{
	AActor* Normale = Cast<AActor>(ArsenalLireObjet(Pion, TEXT("EquippedWeapon")));
	if (!Normale)
	{
		return;
	}
	TInlineComponentArray<UPrimitiveComponent*> Liste;
	Normale->GetComponents(Liste);
	for (UPrimitiveComponent* C : Liste)
	{
		if (!C)
		{
			continue;
		}
		const bool bDeja = Masques.ContainsByPredicate([C](const FArsenalMasque& M) { return M.Composant.Get() == C; });
		if (!bDeja)
		{
			FArsenalMasque M;
			M.Composant = C;
			M.bRenduPrincipal = C->bRenderInMainPass;
			M.bProfondeur = ArsenalLireProfondeur(C);
			M.bOmbre = C->CastShadow;
			Masques.Add(M);
		}
		if (C->bRenderInMainPass)
		{
			C->SetRenderInMainPass(false);
		}
		ArsenalEcrireProfondeur(C, false);
		if (C->CastShadow)
		{
			C->SetCastShadow(false);
		}
	}
}

void AArsenalChelles::RemontrerArmeNormale()
{
	for (const FArsenalMasque& M : Masques)
	{
		if (UPrimitiveComponent* C = M.Composant.Get())
		{
			C->SetRenderInMainPass(M.bRenduPrincipal);
			ArsenalEcrireProfondeur(C, M.bProfondeur);
			C->SetCastShadow(M.bOmbre);
		}
	}
	Masques.Reset();
}

void AArsenalChelles::PlacerVue(APawn* Pion)
{
	if (!Vue || !Pion)
	{
		return;
	}
	// La camera active du personnage (vue 1re personne du kit FPS_Game)
	UCameraComponent* Camera = nullptr;
	TInlineComponentArray<UCameraComponent*> Cameras;
	Pion->GetComponents(Cameras);
	for (UCameraComponent* C : Cameras)
	{
		if (C && C->IsActive())
		{
			Camera = C;
			break;
		}
	}
	if (Camera)
	{
		if (Vue->GetRootComponent()->GetAttachParent() != Camera)
		{
			Vue->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
		return;
	}
	// Pas de camera sur le personnage : on suit le point de vue du joueur
	if (Vue->GetRootComponent()->GetAttachParent())
	{
		Vue->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	if (APlayerController* PC = Cast<APlayerController>(Pion->GetController()))
	{
		FVector Ou;
		FRotator Regard;
		PC->GetPlayerViewPoint(Ou, Regard);
		Vue->SetActorLocationAndRotation(Ou, Regard);
	}
}

// =====================================================================================
//  Tir (machine du joueur)
// =====================================================================================

void AArsenalChelles::ClicPresse()
{
	bTenu = true;
	EssayerTir();
}

void AArsenalChelles::ClicRelache()
{
	bTenu = false;
}

bool AArsenalChelles::Visee(APlayerController* PC, APawn* Pion, FVector& Camera, FVector& Avant, FVector& Cible) const
{
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return false;
	}
	FRotator Regard;
	PC->GetPlayerViewPoint(Camera, Regard);
	Avant = Regard.Vector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ViseeArsenal), false, Pion);
	if (Pion)
	{
		TArray<AActor*> Attaches;
		Pion->GetAttachedActors(Attaches);
		Params.AddIgnoredActors(Attaches);
	}
	if (Vue)
	{
		Params.AddIgnoredActor(Vue);
	}
	FHitResult Impact;
	const FVector Loin = Camera + Avant * 30000.f;
	Cible = W->LineTraceSingleByChannel(Impact, Camera + Avant * 30.f, Loin, ECC_Visibility, Params) ? FVector(Impact.ImpactPoint) : Loin;
	return true;
}

void AArsenalChelles::EssayerTir()
{
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	UWorld* W = GetWorld();
	if (Arme == Aucune || !PC || !Pion || !W || PC->bShowMouseCursor || W->IsPaused())
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant - DernierTir < Cadence(Arme))
	{
		return;
	}
	AArsenalRelais* Relais = MonRelais();
	if (!Relais)
	{
		return; // pas encore cree par le serveur (premiere seconde de jeu)
	}
	DernierTir = Maintenant;

	FVector Camera, Avant, Cible;
	Visee(PC, Pion, Camera, Avant, Cible);
	const FVector Origine = Vue ? Vue->Bouche() : Camera + Avant * 60.f;
	FVector Direction = (Cible - Origine).GetSafeNormal();
	if (FVector::DotProduct(Direction, Avant) < 0.3f)
	{
		Direction = Avant;
	}

	switch (Arme)
	{
	case Blaster:
		Relais->ServeurTirer(Blaster, Origine, Direction);
		break;
	case Bazooka:
		Relais->ServeurTirer(Bazooka, Origine, Direction);
		if (Reglages.bBazookaGuidee)
		{
			GuidageJusqua = Maintenant + Reglages.BazookaDuree + 0.3f;
			ProchaineVisee = Maintenant;
		}
		break;
	case Grenade:
		// Lancer en cloche, un peu au-dessus du viseur
		Relais->ServeurTirer(Grenade, Origine, (Avant + FVector(0.f, 0.f, 0.28f)).GetSafeNormal());
		break;
	case Mitrailleuse:
		Relais->ServeurTirer(Mitrailleuse, Origine, Direction);
		break;
	case LanceFlammes:
	{
		Relais->ServeurFlammes(Camera, Avant);
		// Le jet de feu, vu tout de suite par le joueur (les autres le voient par le serveur)
		const float Portee = Reglages.FlammesPortee;
		for (int32 i = 1; i <= 3; ++i)
		{
			const float K = (float(i) / 3.f) * FMath::FRandRange(0.8f, 1.f);
			const FVector Point = Origine + Avant * Portee * K * 0.9f + FMath::VRand() * 15.f * K;
			ChellesArmesEffets::Bouffee(W, ChellesArmesEffets::Flammes(), Point, 0.35f + 0.6f * K, 0.25f);
		}
		if (!SonFlammes.IsValid())
		{
			USoundBase* Feu = LoadObject<USoundBase>(nullptr, TEXT("/Game/StarterContent/Audio/Fire01_Cue.Fire01_Cue"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (Feu)
			{
				SonFlammes = UGameplayStatics::SpawnSoundAttached(Feu, Pion->GetRootComponent(), NAME_None, FVector::ZeroVector,
					EAttachLocation::KeepRelativeOffset, true, 1.2f * Reglages.Volume, 1.3f);
			}
		}
		break;
	}
	case Sabre:
		Relais->ServeurSabre(Camera, Avant);
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::CoupSabre, Origine, 0.9f * Reglages.Volume, FMath::FRandRange(0.92f, 1.08f));
		break;
	default:
		break;
	}
	if (Vue)
	{
		Vue->Tirer();
	}
}

// =====================================================================================
//  Serveur
// =====================================================================================

void AArsenalChelles::GererRelais()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	TArray<APlayerController*> Servis;
	TArray<AArsenalRelais*> Relais;
	for (TActorIterator<AArsenalRelais> It(W); It; ++It)
	{
		Relais.Add(*It);
	}
	for (AArsenalRelais* R : Relais)
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
		W->SpawnActor<AArsenalRelais>(AArsenalRelais::StaticClass(), FTransform::Identity, Params);
	}
}

void AArsenalChelles::TirServeur(AArsenalRelais* Relais, uint8 Quelle, FVector Origine, FVector Direction)
{
	UWorld* W = GetWorld();
	if (!W || !Relais || !Reglages.bActif || (Quelle != Blaster && Quelle != Bazooka && Quelle != Grenade && Quelle != Mitrailleuse))
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant - Relais->DernierTir[Quelle] < Cadence(Quelle) * 0.8f)
	{
		return; // trop rapide
	}
	Relais->DernierTir[Quelle] = Maintenant;
	if (Quelle == Mitrailleuse)
	{
		if (Reglages.MitrailleuseXP > XPDuJoueur(PC))
		{
			return;
		}
		if (!Reglages.bLourdesSansRatelier)
		{
			if (Relais->ChargeMitrailleuse <= 0)
			{
				return;
			}
			--Relais->ChargeMitrailleuse;
		}
	}
	const float Force = Puissance(PC);

	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Pion->GetControlRotation().Vector();
	}
	if (Quelle == Mitrailleuse)
	{
		// Petite dispersion de rafale
		Direction = FMath::VRandCone(Direction, FMath::DegreesToRadians(1.6f));
	}
	if (FVector::Dist(Origine, Pion->GetActorLocation()) > 600.f)
	{
		Origine = Pion->GetPawnViewLocation() + Direction * 60.f;
	}

	const FTransform Depart(Direction.Rotation(), Origine);
	FActorSpawnParameters Params;
	Params.Owner = Pion;
	Params.Instigator = Pion;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	AProjectileArsenal* P = W->SpawnActor<AProjectileArsenal>(AProjectileArsenal::StaticClass(), Depart, Params);
	if (!P)
	{
		return;
	}
	P->Arme = Quelle;
	P->bToucherJoueurs = ChellesArsenalDegats::PvpActif(W);
	switch (Quelle)
	{
	case Blaster:
		P->Vitesse = Reglages.BlasterVitesse;
		P->Degats = Reglages.BlasterDegats;
		P->Couleur = Reglages.BlasterCouleur.ToFColor(true);
		break;
	case Bazooka:
		P->Vitesse = Reglages.BazookaVitesse;
		P->Degats = Reglages.BazookaDegats;
		P->Rayon = Reglages.BazookaRayon;
		P->Poussee = Reglages.BazookaPoussee;
		P->Virage = Reglages.bBazookaGuidee ? Reglages.BazookaVirage : 0.f;
		P->DureeVie = Reglages.BazookaDuree;
		P->Relais = Relais;
		P->Couleur = FColor(255, 120, 30);
		break;
	case Mitrailleuse:
		// Meme projectile que le blaster (trait lumineux), en jaune et plus rapide
		P->Arme = Blaster;
		P->Vitesse = Reglages.MitrailleuseVitesse;
		P->Degats = Reglages.MitrailleuseDegats;
		P->Couleur = Reglages.MitrailleuseCouleur.ToFColor(true);
		break;
	case Grenade:
		P->Vitesse = Reglages.GrenadeVitesse;
		P->Degats = Reglages.GrenadeDegats;
		P->Rayon = Reglages.GrenadeRayon;
		P->Poussee = Reglages.GrenadePoussee;
		P->Attente = Reglages.GrenadeAttente;
		P->RayonAppat = Reglages.GrenadeRayonAppat;
		P->Couleur = FColor(255, 30, 20);
		break;
	default:
		break;
	}
	P->Degats *= Force;
	P->FinishSpawning(Depart);
}

void AArsenalChelles::SabreServeur(AArsenalRelais* Relais, FVector Camera, FVector Direction)
{
	UWorld* W = GetWorld();
	if (!W || !Relais || !Reglages.bActif)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant - Relais->DernierTir[Sabre] < Reglages.SabreCadence * 0.8f)
	{
		return;
	}
	Relais->DernierTir[Sabre] = Maintenant;

	if (FVector::Dist(Camera, Pion->GetPawnViewLocation()) > 300.f)
	{
		Camera = Pion->GetPawnViewLocation();
	}
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Pion->GetControlRotation().Vector();
	}
	const bool bJoueurs = ChellesArsenalDegats::PvpActif(W);
	const float Portee = Reglages.SabrePortee;
	const float DegatsSabre = Reglages.SabreDegats * Puissance(PC);

	// Tout ce qui est devant, a portee de lame (un arc d'environ 140 degres)
	TArray<APawn*> Touches;
	for (TActorIterator<APawn> It(W); It; ++It)
	{
		APawn* P = *It;
		if (!ChellesArsenalDegats::PeutBlesser(P, Pion, bJoueurs))
		{
			continue;
		}
		const FVector Vers = P->GetActorLocation() - Camera;
		const float Distance = Vers.Size();
		if (Distance > Portee + 50.f)
		{
			continue;
		}
		if (Distance > 90.f && FVector::DotProduct(Vers.GetSafeNormal2D(), Direction.GetSafeNormal2D()) < 0.35f)
		{
			continue;
		}
		Touches.Add(P);
	}
	for (APawn* P : Touches)
	{
		if (!IsValid(P))
		{
			continue;
		}
		const FVector Point = P->GetActorLocation() + FVector(0.f, 0.f, 30.f);
		const FVector Vers = (P->GetActorLocation() - Camera).GetSafeNormal();
		if (ACharacter* Perso = Cast<ACharacter>(P))
		{
			Perso->LaunchCharacter(Vers.GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 150.f), false, false);
		}
		if (Reglages.bSabreTueZombies && ChellesArsenalDegats::EstZombie(P))
		{
			ChellesArsenalDegats::Trancher(P, DegatsSabre, Point, Vers, Pion, this);
		}
		else
		{
			FHitResult Faux(P, nullptr, Point, -Vers);
			Faux.ImpactPoint = Point;
			ChellesArsenalDegats::Blesser(P, DegatsSabre, Faux, Vers, Pion, this);
		}
		AEffetArsenal::Creer(W, ChellesEffetArsenal::Etincelles, Point, Reglages.SabreCouleur, Pion);
	}

	// Objets physiques devant la lame : pousses
	TArray<FOverlapResult> Objets;
	FCollisionObjectQueryParams Types;
	Types.AddObjectTypesToQuery(ECC_PhysicsBody);
	Types.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SabreArsenal), false, Pion);
	W->OverlapMultiByObjectType(Objets, Camera + Direction * Portee * 0.6f, FQuat::Identity, Types, FCollisionShape::MakeSphere(Portee * 0.5f), Params);
	for (const FOverlapResult& O : Objets)
	{
		UPrimitiveComponent* C = O.GetComponent();
		if (C && C->IsSimulatingPhysics())
		{
			C->AddImpulse(Direction * 500.f, NAME_None, true);
		}
	}

	// Le "vvvoum" pour les autres joueurs
	AEffetArsenal::Creer(W, ChellesEffetArsenal::CoupSabre, Camera + Direction * 80.f, Reglages.SabreCouleur, Pion);
}

// =====================================================================================
//  Console : Chelles.Arsenal ...
// =====================================================================================

void AArsenalChelles::Commande(const TArray<FString>& Arguments)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString();
	if (Ordre.Equals(TEXT("Recharger"), ESearchCase::IgnoreCase))
	{
		ChargerReglages(Reglages);
		ChellesArsenalVisuel::DefinirLueur(Reglages.MateriauLueur);
		const uint8 Garde = Arme;
		Choisir(Aucune, false);
		if (Garde != Aucune)
		{
			Choisir(Garde, false);
		}
		Message(TEXT("Arsenal.txt relu."));
		return;
	}
	if (Ordre.IsEmpty())
	{
		Message(TEXT("Chelles.Arsenal Blaster | Bazooka | Sabre | Grenade | LanceFlammes | Mitrailleuse | Aucune | Remplir | Recharger"));
		return;
	}
	if (Ordre.Equals(TEXT("Remplir"), ESearchCase::IgnoreCase))
	{
		// Test (solo ou hote) : munitions des armes lourdes sans passer par un ratelier
		if (GetNetMode() != NM_Client)
		{
			if (AArsenalRelais* R = MonRelais())
			{
				R->ChargeFlammes = Reglages.FlammesCharge;
				R->ChargeMitrailleuse = Reglages.MitrailleuseCharge;
				Message(TEXT("Armes lourdes remplies (touche 3 pour les choisir)."));
			}
		}
		return;
	}
	Choisir(ArmeDepuisTexte(Ordre), true);
}

// =====================================================================================
//  Texte a l'ecran + petit viseur
// =====================================================================================

void AArsenalChelles::CreerEcran()
{
	UWorld* W = GetWorld();
	if (Ecran.IsValid() || !W || GetNetMode() == NM_DedicatedServer || !FSlateApplication::IsInitialized())
	{
		return;
	}
	UGameViewportClient* VueJeu = W->GetGameViewport();
	if (!VueJeu)
	{
		return;
	}
	TWeakObjectPtr<AArsenalChelles> Moi(this);
	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 185.f))
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			.Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([Moi]()
			{
				if (Moi.IsValid() && Moi->AgeInfo < 3.5f)
				{
					return FSlateColor(FLinearColor(1.f, 0.85f, 0.3f, FMath::Clamp(3.5f - Moi->AgeInfo, 0.f, 1.f)));
				}
				return FSlateColor(FLinearColor(0.45f, 1.f, 0.95f, 0.9f));
			})
			.Text_Lambda([Moi]()
			{
				if (!Moi.IsValid() || Moi->bCurseur)
				{
					return FText::GetEmpty();
				}
				if (Moi->AgeInfo < 3.5f)
				{
					return FText::FromString(Moi->Info);
				}
				return FText::FromString(Moi->Etiquette.IsEmpty() ? Moi->AideRatelier : Moi->Etiquette);
			})
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ColorAndOpacity(FLinearColor(1.f, 0.25f, 0.2f, 0.9f))
			.Text_Lambda([Moi]()
			{
				const bool bVoir = Moi.IsValid() && !Moi->bCurseur && Moi->Arme != Aucune && Moi->Arme != Sabre;
				return bVoir ? FText::FromString(TEXT("+")) : FText::GetEmpty();
			})
		];
	VueJeu->AddViewportWidgetContent(Ecran.ToSharedRef(), 4);
}

void AArsenalChelles::RetirerEcran()
{
	if (!Ecran.IsValid())
	{
		return;
	}
	if (UWorld* W = GetWorld())
	{
		if (UGameViewportClient* VueJeu = W->GetGameViewport())
		{
			VueJeu->RemoveViewportWidgetContent(Ecran.ToSharedRef());
		}
	}
	Ecran.Reset();
}

// =====================================================================================
//  Armes lourdes : lance-flammes, mitrailleuse, rateliers au mur, puissance selon l'XP (2026-11-20)
// =====================================================================================

void AArsenalChelles::FlammesServeur(AArsenalRelais* Relais, FVector Camera, FVector Direction)
{
	UWorld* W = GetWorld();
	if (!W || !Relais || !Reglages.bActif)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion || Reglages.FlammesXP > XPDuJoueur(PC))
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant - Relais->DernierTir[LanceFlammes] < Reglages.FlammesCadence * 0.8f)
	{
		return;
	}
	Relais->DernierTir[LanceFlammes] = Maintenant;
	if (!Reglages.bLourdesSansRatelier)
	{
		if (Relais->ChargeFlammes <= 0)
		{
			return;
		}
		--Relais->ChargeFlammes;
	}

	if (FVector::Dist(Camera, Pion->GetPawnViewLocation()) > 300.f)
	{
		Camera = Pion->GetPawnViewLocation();
	}
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Pion->GetControlRotation().Vector();
	}
	const bool bJoueurs = ChellesArsenalDegats::PvpActif(W);
	const float Portee = Reglages.FlammesPortee;
	const float CosAngle = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Reglages.FlammesAngle, 2.f, 80.f)));
	const float Degats = Reglages.FlammesDegats * Puissance(PC);

	// Tout ce qui est dans le cone du jet, sans mur entre les deux
	TArray<APawn*> Touches;
	for (TActorIterator<APawn> It(W); It; ++It)
	{
		APawn* P = *It;
		if (!ChellesArsenalDegats::PeutBlesser(P, Pion, bJoueurs))
		{
			continue;
		}
		const FVector Vers = P->GetActorLocation() - Camera;
		const float Distance = Vers.Size();
		if (Distance > Portee || (Distance > 80.f && FVector::DotProduct(Vers / FMath::Max(Distance, 1.f), Direction) < CosAngle))
		{
			continue;
		}
		Touches.Add(P);
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlammesArsenal), false, Pion);
	for (APawn* P : Touches)
	{
		if (!IsValid(P))
		{
			continue;
		}
		FHitResult Mur;
		Params.ClearIgnoredActors();
		Params.AddIgnoredActor(Pion);
		Params.AddIgnoredActor(P);
		if (W->LineTraceSingleByChannel(Mur, Camera, P->GetActorLocation(), ECC_Visibility, Params))
		{
			continue; // cache derriere un mur
		}
		const FVector Vers = (P->GetActorLocation() - Camera).GetSafeNormal();
		const FVector Point = P->GetActorLocation();
		FHitResult Faux(P, nullptr, Point, -Vers);
		Faux.ImpactPoint = Point;
		ChellesArsenalDegats::Blesser(P, Degats, Faux, Vers, Pion, this);
	}

	// Les autres joueurs voient le feu (un effet tous les 3 jets suffit)
	if (++JetsFlammes % 3 == 0 && W->GetNetMode() != NM_Standalone)
	{
		AEffetArsenal::Creer(W, ChellesEffetArsenal::Flammes, Camera + Direction * 60.f, FLinearColor(Direction.X * 0.5f + 0.5f, Direction.Y * 0.5f + 0.5f, Direction.Z * 0.5f + 0.5f), Pion); // direction du jet rangee dans la couleur
	}
}

void AArsenalChelles::PrendreServeur(AArsenalRelais* Relais, ARatelierArsenal* Ratelier)
{
	if (!Relais || !IsValid(Ratelier) || !Reglages.bActif)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(Relais->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion || FVector::Dist(Pion->GetActorLocation(), Ratelier->Centre()) > 400.f)
	{
		return;
	}
	if (Ratelier->bVide)
	{
		Relais->ClientAvis(TEXT("Ce ratelier est vide : l'arme revient bientot."));
		return;
	}
	const uint8 Quelle = Ratelier->Arme;
	const int32 Requis = (Quelle == LanceFlammes) ? Reglages.FlammesXP : Reglages.MitrailleuseXP;
	const int32 XP = XPDuJoueur(PC);
	if (XP < Requis)
	{
		Relais->ClientAvis(FString::Printf(TEXT("%s : il faut %d XP (tu en as %d). Joue encore un peu !"), NomArme(Quelle), Requis, XP));
		return;
	}
	if (Quelle == LanceFlammes)
	{
		Relais->ChargeFlammes = Reglages.FlammesCharge;
	}
	else if (Quelle == Mitrailleuse)
	{
		Relais->ChargeMitrailleuse = Reglages.MitrailleuseCharge;
	}
	else
	{
		return;
	}
	Relais->ForceNetUpdate();
	Ratelier->Vider(Reglages.RatelierAttente);
	Relais->ClientPris(Quelle);
}

void AArsenalChelles::AvisLocal(const FString& Texte)
{
	Message(Texte);
}

void AArsenalChelles::ArmeLourdePrise(uint8 Quelle)
{
	Choisir(Quelle, true);
}

bool AArsenalChelles::Disponible(uint8 Quelle, FString* OutRaison) const
{
	if (!Reglages.bActif)
	{
		if (OutRaison) *OutRaison = TEXT("Arsenal coupe (Arsenal=Non dans Plugins\\ChellesArmes\\Arsenal.txt).");
		return false;
	}
	if (!EstArmeLourde(Quelle))
	{
		return true;
	}
	const int32 Requis = (Quelle == LanceFlammes) ? Reglages.FlammesXP : Reglages.MitrailleuseXP;
	const int32 XP = XPLocale();
	if (XP < Requis)
	{
		if (OutRaison) *OutRaison = FString::Printf(TEXT("%s : debloque a %d XP (tu en as %d)."), NomArme(Quelle), Requis, XP);
		return false;
	}
	if (!Reglages.bLourdesSansRatelier && ChargeLocale(Quelle) <= 0)
	{
		if (OutRaison) *OutRaison = FString::Printf(TEXT("%s : va la prendre sur un ratelier accroche a un mur (touche %s)."), NomArme(Quelle), *Reglages.ToucheRatelier);
		return false;
	}
	return true;
}

int32 AArsenalChelles::ChargeLocale(uint8 Quelle) const
{
	const AArsenalRelais* R = MonRelais();
	if (!R)
	{
		return 0;
	}
	return Quelle == LanceFlammes ? R->ChargeFlammes : (Quelle == Mitrailleuse ? R->ChargeMitrailleuse : 0);
}

int32 AArsenalChelles::XPLocale() const
{
	// XP du joueur de cette machine, tenue par le plugin ChellesPouvoirs (lue par son nom : aucun lien de compilation)
	int32 XP = 0;
	AActor* Pouvoirs = ArsenalActeurParClasse(GetWorld(), TEXT("PouvoirsChelles"), GArsenalPouvoirs, GArsenalPouvoirsRecherche);
	ArsenalLireEntier(Pouvoirs, TEXT("XPLocale"), XP);
	return XP;
}

int32 AArsenalChelles::XPDuJoueur(APlayerController* PC) const
{
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return 0;
	}
	if (PC->IsLocalController())
	{
		return XPLocale();
	}
	// Joueur distant : son relais ChellesPouvoirs copie son XP sur le serveur
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->GetOwner() == PC && It->GetClass()->GetName() == TEXT("PouvoirsRelais"))
		{
			int32 XP = 0;
			ArsenalLireEntier(*It, TEXT("XP"), XP);
			return XP;
		}
	}
	return 0;
}

float AArsenalChelles::Puissance(APlayerController* PC) const
{
	if (Reglages.PuissanceParNiveau <= 0.f)
	{
		return 1.f;
	}
	const int32 Niveau = NiveauArsenal(XPDuJoueur(PC));
	return 1.f + Reglages.PuissanceParNiveau * float(FMath::Max(0, Niveau - 1));
}

void AArsenalChelles::PoserRateliers()
{
	bRateliersPoses = true;
	UWorld* W = GetWorld();
	if (!W || !Reglages.bActif || !Reglages.bRateliers || Reglages.RateliersMax <= 0)
	{
		return;
	}
	for (TActorIterator<ARatelierArsenal> It(W); It; ++It)
	{
		return; // deja poses (la carte a ete rechargee, ou un autre gestionnaire l'a fait)
	}

	// Points de depart : les socles d'armes poses par Tools\Bonus (sinon autour du joueur)
	TArray<FVector> Departs;
	static const FName MarqueBonus(TEXT("ChellesBonusAjoute"));
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		if (It->Tags.Contains(MarqueBonus) && It->GetClass()->GetName().Contains(TEXT("Factory")))
		{
			Departs.Add(It->GetActorLocation());
		}
	}
	// 25/11/2026 (agent Armements) : les socles Bonus ont ete retires de Rachid (retirer_pickups),
	// le journal disait "0 ratelier(s)". On prend aussi les coffres d'armes du kit (BP_Chest...),
	// les points de depart des joueurs, puis un cercle autour du joueur.
	if (Departs.Num() < Reglages.RateliersMax)
	{
		for (TActorIterator<AActor> It(W); It; ++It)
		{
			const FString Classe = It->GetClass()->GetName();
			if (Classe.StartsWith(TEXT("BP_Chest")) || Classe.StartsWith(TEXT("Replicated_Chest")) || Classe.StartsWith(TEXT("BP_Loot_Chest"))
				|| Classe.Contains(TEXT("PlayerStart")))
			{
				Departs.Add(It->GetActorLocation());
			}
		}
	}
	if (Departs.Num() < Reglages.RateliersMax)
	{
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			if (APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
			{
				const FVector Centre = P->GetActorLocation();
				Departs.Add(Centre);
				static const float Rayons[] = { 2000.f, 4000.f, 7000.f };
				for (float Rayon : Rayons)
				{
					for (int32 i = 0; i < 8; ++i)
					{
						const float Angle = FMath::DegreesToRadians(45.f * i + Rayon * 0.01f);
						Departs.Add(Centre + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Rayon);
					}
				}
				break;
			}
		}
	}

	TArray<FVector> Poses;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RatelierArsenal), false);
	for (const FVector& Depart : Departs)
	{
		if (Poses.Num() >= Reglages.RateliersMax)
		{
			break;
		}
		// Le sol sous le point de depart
		FHitResult Sol;
		FVector Base = Depart;
		if (W->LineTraceSingleByChannel(Sol, Depart + FVector(0.f, 0.f, 300.f), Depart - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			Base = Sol.ImpactPoint;
		}
		const FVector Hauteur = Base + FVector(0.f, 0.f, 150.f);

		// Le mur le plus proche, dans 12 directions
		FHitResult Meilleur;
		bool bTrouve = false;
		for (int32 i = 0; i < 12; ++i)
		{
			const float Angle = FMath::DegreesToRadians(30.f * i);
			const FVector Vers(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
			FHitResult Mur;
			if (!W->LineTraceSingleByChannel(Mur, Hauteur, Hauteur + Vers * 500.f, ECC_Visibility, Params))
			{
				continue;
			}
			if (FMath::Abs(Mur.ImpactNormal.Z) > 0.3f || Cast<APawn>(Mur.GetActor()))
			{
				continue;
			}
			if (Mur.GetComponent() && Mur.GetComponent()->IsSimulatingPhysics())
			{
				continue;
			}
			if (!bTrouve || Mur.Distance < Meilleur.Distance)
			{
				Meilleur = Mur;
				bTrouve = true;
			}
		}
		if (!bTrouve)
		{
			continue;
		}
		const FVector Normale = FVector(Meilleur.ImpactNormal.X, Meilleur.ImpactNormal.Y, 0.f).GetSafeNormal();
		const FVector Lieu = Meilleur.ImpactPoint + Normale * 5.f;
		bool bTropPres = false;
		for (const FVector& Autre : Poses)
		{
			if (FVector::DistSquared(Autre, Lieu) < FMath::Square(1500.f))
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawn.bDeferConstruction = true;
		const FTransform Place(Normale.Rotation(), Lieu);
		ARatelierArsenal* R = W->SpawnActor<ARatelierArsenal>(ARatelierArsenal::StaticClass(), Place, Spawn);
		if (!R)
		{
			continue;
		}
		R->Arme = (Poses.Num() % 2 == 0) ? LanceFlammes : Mitrailleuse;
		R->FinishSpawning(Place);
		Poses.Add(Lieu);
	}
	UE_LOG(LogTemp, Log, TEXT("[Arsenal] %d ratelier(s) d'armes lourdes accroche(s) aux murs (%d point(s) de depart)."), Poses.Num(), Departs.Num());
	if (Poses.Num() == 0 && ++EssaisRateliers < 12)
	{
		bRateliersPoses = false; // nouvel essai dans 5 s (le joueur n'est peut-etre pas encore la)
		ProchainEssaiRateliers = W->GetTimeSeconds() + 5.f;
	}
}

void AArsenalChelles::SuivreRateliers(APlayerController* PC, APawn* Pion)
{
	AideRatelier.Reset();
	UWorld* W = GetWorld();
	if (!W || !PC || !Pion || !Reglages.bActif || !Reglages.bRateliers || !Cast<ACharacter>(Pion))
	{
		return;
	}
	ARatelierArsenal* Proche = nullptr;
	float Meilleure = FMath::Square(220.f);
	for (TActorIterator<ARatelierArsenal> It(W); It; ++It)
	{
		const float D = FVector::DistSquared(It->Centre(), Pion->GetActorLocation());
		if (D < Meilleure)
		{
			Meilleure = D;
			Proche = *It;
		}
	}
	if (!Proche)
	{
		return;
	}
	if (Proche->bVide)
	{
		AideRatelier = TEXT("Ratelier vide : l'arme revient bientot");
		return;
	}
	const int32 Requis = (Proche->Arme == LanceFlammes) ? Reglages.FlammesXP : Reglages.MitrailleuseXP;
	const int32 XP = XPLocale();
	if (XP < Requis)
	{
		AideRatelier = FString::Printf(TEXT("%s : il faut %d XP (tu en as %d)"), NomArme(Proche->Arme), Requis, XP);
		return;
	}
	AideRatelier = FString::Printf(TEXT("%s pour prendre : %s"), *Reglages.ToucheRatelier, NomArme(Proche->Arme));
	const FKey Touche(*Reglages.ToucheRatelier);
	if (Touche.IsValid() && PC->WasInputKeyJustPressed(Touche) && !PC->bShowMouseCursor)
	{
		if (AArsenalRelais* R = MonRelais())
		{
			R->ServeurPrendre(Proche);
		}
	}
}

void AArsenalChelles::ArreterSonFlammes()
{
	if (SonFlammes.IsValid())
	{
		SonFlammes->FadeOut(0.2f, 0.f);
	}
	SonFlammes.Reset();
}
