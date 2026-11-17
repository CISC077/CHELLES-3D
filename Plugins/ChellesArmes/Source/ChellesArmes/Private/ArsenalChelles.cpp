// CHELLES 3D - Arsenal (2026-11-17) : blaster, bazooka guide, sabre lumineux, grenade a zombies
#include "ArsenalChelles.h"
#include "ArsenalDegats.h"
#include "ArsenalRelais.h"
#include "ArsenalVisuel.h"
#include "ArsenalVue.h"
#include "EffetArsenal.h"
#include "ProjectileArsenal.h"
#include "SonArsenalChelles.h"

#include "Camera/CameraComponent.h"
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

	if (Arme == Aucune)
	{
		Etiquette.Reset();
	}
	else
	{
		const TCHAR* Geste = (Arme == Sabre) ? TEXT("frapper") : (Arme == Grenade) ? TEXT("lancer") : TEXT("tirer");
		const FKey Touche(*Reglages.ToucheChanger);
		const FString NomTouche = Touche.IsValid() ? Touche.GetDisplayName().ToString() : Reglages.ToucheChanger;
		Etiquette = FString::Printf(TEXT("%s   -   clic gauche = %s   -   touche %s = arme suivante   -   molette = arme normale"),
			NomArme(Arme), Geste, *NomTouche);
		MasquerArmeNormale(Pion);
		PlacerVue(Pion);
		if (bTenu && PC && !PC->IsInputKeyDown(EKeys::LeftMouseButton))
		{
			bTenu = false;
		}
		if (bTenu && (Arme == Blaster || Arme == Sabre))
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
	Choisir(uint8((Arme + 1) % NombreArmes), true);
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
	if (!W || !Relais || !Reglages.bActif || (Quelle != Blaster && Quelle != Bazooka && Quelle != Grenade))
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

	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Pion->GetControlRotation().Vector();
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
			ChellesArsenalDegats::Trancher(P, Reglages.SabreDegats, Point, Vers, Pion, this);
		}
		else
		{
			FHitResult Faux(P, nullptr, Point, -Vers);
			Faux.ImpactPoint = Point;
			ChellesArsenalDegats::Blesser(P, Reglages.SabreDegats, Faux, Vers, Pion, this);
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
		Message(TEXT("Chelles.Arsenal Blaster | Bazooka | Sabre | Grenade | Aucune | Recharger"));
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
				return FText::FromString(Moi->AgeInfo < 3.5f ? Moi->Info : Moi->Etiquette);
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
