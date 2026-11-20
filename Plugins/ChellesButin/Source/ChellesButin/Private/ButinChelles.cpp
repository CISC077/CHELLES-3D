// CHELLES 3D - Butin : coffres (Fortnite / Apex), ping (Apex), series d'eliminations (Call of Duty)
#include "ButinChelles.h"
#include "CoffreButin.h"
#include "PingButin.h"
#include "RelaisButin.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

using namespace ChellesButin;

namespace
{
	const FName MarqueSuiviButin(TEXT("ChellesButinSuivi"));
	const FName MarqueMortButin(TEXT("ChellesButinMort"));

	bool ButinLireBooleen(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
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

	FNumericProperty* ButinNombre(const UObject* Objet, std::initializer_list<const TCHAR*> Noms)
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

	double ButinLireNombre(FNumericProperty* P, const UObject* Objet)
	{
		const void* Valeur = P->ContainerPtrToValuePtr<void>(Objet);
		return P->IsFloatingPoint() ? P->GetFloatingPointPropertyValue(Valeur) : double(P->GetSignedIntPropertyValue(Valeur));
	}

	void ButinEcrireNombre(FNumericProperty* P, UObject* Objet, double Nouveau)
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

	bool ButinPvp(UWorld* Monde)
	{
		if (!Monde || Monde->GetNetMode() == NM_Standalone)
		{
			return true;
		}
		bool bPvp = true; // reglage du menu de celui qui heberge (comme l'arsenal)
		if (GConfig)
		{
			GConfig->GetBool(TEXT("ChellesMenu"), TEXT("Pvp"), bPvp, GGameUserSettingsIni);
		}
		return bPvp;
	}

	FKey ButinTouche(const FString& Nom, const FKey& Defaut)
	{
		const FKey K(*Nom);
		return K.IsValid() ? K : Defaut;
	}

	FLinearColor ButinCouleur(uint8 Code)
	{
		switch (Code)
		{
		case 1: return FLinearColor(0.35f, 0.7f, 1.f);
		case 2: return FLinearColor(0.75f, 0.35f, 1.f);
		case 3: return FLinearColor(1.f, 0.78f, 0.15f);
		case 4: return FLinearColor(1.f, 0.3f, 0.2f);
		default: return FLinearColor(1.f, 1.f, 1.f);
		}
	}
}

/** Marques du RADAR dessinees par-dessus l'image (on les voit a travers les murs). */
class SRadarButin : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRadarButin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakObjectPtr<AButinChelles> InGestion)
	{
		Gestion = InGestion;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D::ZeroVector;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Rect, FSlateWindowElementList& Elements,
		int32 Couche, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		if (!Gestion.IsValid() || Gestion->PointsRadar.Num() == 0 || !GEngine || !GEngine->GameViewport)
		{
			return Couche;
		}
		FVector2D TailleVue;
		GEngine->GameViewport->GetViewportSize(TailleVue);
		const FVector2D Local = Geo.GetLocalSize();
		if (TailleVue.X < 1.f || TailleVue.Y < 1.f)
		{
			return Couche;
		}
		const FVector2D Rapport(Local.X / TailleVue.X, Local.Y / TailleVue.Y);
		const FSlateFontInfo Grand = FCoreStyle::GetDefaultFontStyle("Bold", 22);
		const FSlateFontInfo Petit = FCoreStyle::GetDefaultFontStyle("Bold", 11);
		const FLinearColor Rouge(1.f, 0.15f, 0.1f, 0.95f);
		for (const FPointRadar& P : Gestion->PointsRadar)
		{
			const FVector2D Ici(P.Ecran.X * Rapport.X, P.Ecran.Y * Rapport.Y);
			FSlateDrawElement::MakeText(Elements, Couche + 1, Geo.ToPaintGeometry(Ici - FVector2D(6.f, 30.f), FVector2D(40.f, 30.f)),
				FString(TEXT("!")), Grand, ESlateDrawEffect::None, Rouge);
			FSlateDrawElement::MakeText(Elements, Couche + 1, Geo.ToPaintGeometry(Ici - FVector2D(14.f, 0.f), FVector2D(80.f, 16.f)),
				FString::Printf(TEXT("%d m"), FMath::RoundToInt(P.Distance / 100.f)), Petit, ESlateDrawEffect::None, Rouge);
		}
		return Couche + 1;
	}

private:
	TWeakObjectPtr<AButinChelles> Gestion;
};

// =====================================================================================================
// Demarrage
// =====================================================================================================

AButinChelles::AButinChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // apres la camera : les marques du radar suivent l'image
	bReplicates = false; // un par machine : ce qui est partage passe par ARelaisButin
	SetActorHiddenInGame(true);
}

void AButinChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglagesButin(Reglages);
	if (GetNetMode() != NM_DedicatedServer)
	{
		CreerEcran();
	}
	UE_LOG(LogTemp, Log, TEXT("[Butin] Pret : coffres %s, ping %s, series %s."), Reglages.bCoffres ? TEXT("oui") : TEXT("non"),
		Reglages.bPing ? TEXT("oui") : TEXT("non"), Reglages.bSeries ? TEXT("oui") : TEXT("non"));
}

void AButinChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerEcran();
	Super::EndPlay(Raison);
}

void AButinChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AgeAnnonce += DeltaSeconds;
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	const float Maintenant = W->GetTimeSeconds();
	if (GetNetMode() != NM_Client)
	{
		if (Maintenant >= ProchainRelais)
		{
			ProchainRelais = Maintenant + 0.5f;
			GererRelais();
			GererSeries(Maintenant);
			VerifierMorts(Maintenant);
			GererCoffres(0.5f);
		}
		if (Maintenant >= ProchainSuivi)
		{
			ProchainSuivi = Maintenant + 2.f;
			SuivreEnnemis();
		}
		GererBombes(Maintenant);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		JoueurLocal(DeltaSeconds);
	}
}

// =====================================================================================================
// Reseau
// =====================================================================================================

APlayerController* AButinChelles::PCLocal() const
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

ARelaisButin* AButinChelles::RelaisDe(APlayerController* PC) const
{
	UWorld* W = GetWorld();
	if (!PC || !W)
	{
		return nullptr;
	}
	for (TActorIterator<ARelaisButin> It(W); It; ++It)
	{
		if (It->GetOwner() == PC)
		{
			return *It;
		}
	}
	return nullptr;
}

ARelaisButin* AButinChelles::MonRelais() const
{
	return RelaisDe(PCLocal());
}

void AButinChelles::GererRelais()
{
	UWorld* W = GetWorld();
	TArray<APlayerController*> Servis;
	TArray<ARelaisButin*> Relais;
	for (TActorIterator<ARelaisButin> It(W); It; ++It)
	{
		Relais.Add(*It);
	}
	for (ARelaisButin* R : Relais)
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
		W->SpawnActor<ARelaisButin>(ARelaisButin::StaticClass(), FTransform::Identity, Params);
	}
}

// =====================================================================================================
// Serveur : coffres de butin
// =====================================================================================================

void AButinChelles::GererCoffres(float Dt)
{
	UWorld* W = GetWorld();
	Coffres.RemoveAll([](const TWeakObjectPtr<ACoffreButin>& C) { return !C.IsValid(); });
	if (!Reglages.bCoffres)
	{
		return;
	}
	TArray<FVector> Joueurs;
	for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Joueurs.Add(P->GetActorLocation());
		}
	}
	if (Joueurs.Num() == 0)
	{
		return;
	}

	// Coffres oublies loin de tout le monde : retires (fluidite)
	for (const TWeakObjectPtr<ACoffreButin>& Faible : Coffres)
	{
		ACoffreButin* C = Faible.Get();
		if (!C || C->bOuvert)
		{
			continue;
		}
		float Plus = MAX_flt;
		for (const FVector& J : Joueurs)
		{
			Plus = FMath::Min(Plus, FVector::Dist(J, C->GetActorLocation()));
		}
		C->TempsLoin = Plus > Reglages.DistanceRetrait ? C->TempsLoin + Dt : 0.f;
		if (C->TempsLoin > 30.f)
		{
			C->Destroy();
		}
	}

	const float Maintenant = W->GetTimeSeconds();
	if (Maintenant < ProchainCoffre)
	{
		return;
	}
	ProchainCoffre = Maintenant + Reglages.Attente;
	for (const FVector& J : Joueurs)
	{
		if (Coffres.Num() >= Reglages.CoffresMax)
		{
			break;
		}
		int32 Autour = 0;
		for (const TWeakObjectPtr<ACoffreButin>& C : Coffres)
		{
			if (C.IsValid() && !C->bOuvert && FVector::Dist(C->GetActorLocation(), J) < Reglages.DistanceMax * 1.5f)
			{
				++Autour;
			}
		}
		if (Autour < Reglages.CoffresParJoueur)
		{
			PoserCoffre(J);
		}
	}
}

ACoffreButin* AButinChelles::PoserCoffre(const FVector& Autour)
{
	UWorld* W = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ButinPlaceCoffre), true);
	for (int32 Essai = 0; Essai < 16; ++Essai)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Distance = FMath::FRandRange(Reglages.DistanceMin, FMath::Max(Reglages.DistanceMin + 100.f, Reglages.DistanceMax));
		const FVector Haut = Autour + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 400.f);
		FHitResult Sol;
		if (!W->LineTraceSingleByChannel(Sol, Haut, Haut - FVector(0.f, 0.f, 2400.f), ECC_Visibility, Params))
		{
			continue;
		}
		if (Sol.ImpactNormal.Z < 0.8f || Cast<APawn>(Sol.GetActor()) || FMath::Abs(Sol.ImpactPoint.Z - Autour.Z) > 400.f)
		{
			continue;
		}
		// Rien juste au-dessus (pas sous un toit, une voiture, un arbre bas)
		FHitResult Dessus;
		if (W->LineTraceSingleByChannel(Dessus, Sol.ImpactPoint + FVector(0.f, 0.f, 30.f), Sol.ImpactPoint + FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
		{
			continue;
		}
		bool bTropPres = false;
		for (const TWeakObjectPtr<ACoffreButin>& C : Coffres)
		{
			if (C.IsValid() && FVector::DistSquared(C->GetActorLocation(), Sol.ImpactPoint) < FMath::Square(1500.f))
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}
		const float Tirage = FMath::FRandRange(0.f, 100.f);
		uint8 Rarete = Commun;
		if (Tirage < Reglages.ChanceLegendaire)
		{
			Rarete = Legendaire;
		}
		else if (Tirage < Reglages.ChanceLegendaire + Reglages.ChanceRare)
		{
			Rarete = Rare;
		}
		const FTransform Place(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Sol.ImpactPoint);
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawn.bDeferConstruction = true;
		ACoffreButin* C = W->SpawnActor<ACoffreButin>(ACoffreButin::StaticClass(), Place, Spawn);
		if (!C)
		{
			continue;
		}
		C->Rarete = Rarete;
		C->FinishSpawning(Place);
		Coffres.Add(C);
		return C;
	}
	return nullptr;
}

void AButinChelles::OuvrirServeur(ARelaisButin* R, ACoffreButin* Coffre)
{
	if (!R || !IsValid(Coffre) || Coffre->bOuvert || !Reglages.bCoffres)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(R->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion || FVector::Dist(Pion->GetActorLocation(), Coffre->Centre()) > 350.f)
	{
		return;
	}
	Coffre->Ouvrir();
	Recompenser(R, PC, Coffre->Rarete);
}

bool AButinChelles::Soigner(APawn* Pion) const
{
	FNumericProperty* Vie = ButinNombre(Pion, { TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie"), TEXT("HP"), TEXT("health") });
	if (!Vie)
	{
		return false;
	}
	FNumericProperty* Max = ButinNombre(Pion, { TEXT("MaxHealth"), TEXT("Max Health"), TEXT("HealthMax"), TEXT("MaxHP"), TEXT("DefaultHealth"), TEXT("VieMax") });
	const double Actuelle = ButinLireNombre(Vie, Pion);
	const double Plein = Max ? ButinLireNombre(Max, Pion) : FMath::Max(100.0, Actuelle);
	if (Actuelle <= 0.0 || Actuelle >= Plein - 0.5)
	{
		return false; // mort ou deja plein : on donne autre chose
	}
	ButinEcrireNombre(Vie, Pion, Plein);
	Pion->ForceNetUpdate();
	return true;
}

int32 AButinChelles::AjouterMunitions(APlayerController* PC, const TCHAR* Propriete, int32 Quantite) const
{
	// Munitions des armes lourdes du plugin ChellesArmes (AArsenalRelais), lues par leur nom
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

void AButinChelles::Recompenser(ARelaisButin* R, APlayerController* PC, uint8 Rarete)
{
	TArray<FString> Gains;
	int32 XP = 0;
	APawn* Pion = PC->GetPawn();
	auto Mitrailleuse = [&](int32 N) -> bool
	{
		if (AjouterMunitions(PC, TEXT("ChargeMitrailleuse"), N) > 0)
		{
			Gains.Add(FString::Printf(TEXT("MITRAILLEUSE +%d"), N));
			return true;
		}
		return false;
	};
	auto Flammes = [&](int32 N) -> bool
	{
		if (AjouterMunitions(PC, TEXT("ChargeFlammes"), N) > 0)
		{
			Gains.Add(FString::Printf(TEXT("LANCE-FLAMMES +%d"), N));
			return true;
		}
		return false;
	};
	auto Soins = [&]() -> bool
	{
		if (Soigner(Pion))
		{
			Gains.Add(TEXT("VIE AU MAX"));
			return true;
		}
		return false;
	};

	switch (Rarete)
	{
	case Legendaire:
		XP = Reglages.XPLegendaire;
		Soins();
		Mitrailleuse(Reglages.BallesMitrailleuse);
		Flammes(Reglages.JetsFlammes);
		break;
	case Rare:
	{
		XP = Reglages.XPRare;
		const int32 Choix = FMath::RandRange(0, 2);
		const bool bOk = Choix == 0 ? Soins() : (Choix == 1 ? Mitrailleuse(Reglages.BallesMitrailleuse) : Flammes(Reglages.JetsFlammes));
		if (!bOk && !Mitrailleuse(Reglages.BallesMitrailleuse))
		{
			XP += Reglages.XPCommun;
		}
		break;
	}
	default:
	{
		const int32 Choix = FMath::RandRange(0, 2);
		bool bOk = false;
		if (Choix == 1)
		{
			bOk = Soins();
		}
		else if (Choix == 2)
		{
			bOk = Mitrailleuse(Reglages.BallesMitrailleuse / 2);
		}
		if (!bOk)
		{
			XP = Reglages.XPCommun;
		}
		break;
	}
	}
	if (XP > 0)
	{
		Gains.Insert(FString::Printf(TEXT("+%d XP"), XP), 0);
		R->ClientXP(XP, FString(NomRarete(Rarete)).ToLower());
	}
	const uint8 Couleur = Rarete == Legendaire ? 3 : (Rarete == Rare ? 2 : 1);
	R->ClientAnnonce(FString::Printf(TEXT("%s :  %s"), NomRarete(Rarete), *FString::Join(Gains, TEXT("   "))), Couleur, false);
}

// =====================================================================================================
// Serveur : ping et frappe aerienne
// =====================================================================================================

void AButinChelles::PingServeur(ARelaisButin* R, FVector Point, uint8 Genre)
{
	UWorld* W = GetWorld();
	if (!R || !W || !Reglages.bPing)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(R->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	const float Maintenant = W->GetTimeSeconds();
	if (!Pion || Maintenant < R->ProchainPing || FVector::Dist(Point, Pion->GetActorLocation()) > 35000.f)
	{
		return;
	}
	R->ProchainPing = Maintenant + 0.4f;
	if (R->DernierPing.IsValid())
	{
		R->DernierPing->Destroy(); // un seul ping par joueur
	}
	const FTransform Place(Point);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.bDeferConstruction = true;
	APingButin* P = W->SpawnActor<APingButin>(APingButin::StaticClass(), Place, Spawn);
	if (!P)
	{
		return;
	}
	P->Genre = FMath::Min<uint8>(Genre, PingVehicule);
	P->Auteur = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
	P->FinishSpawning(Place);
	P->SetLifeSpan(Reglages.DureePing);
	R->DernierPing = P;
}

void AButinChelles::FrappeServeur(ARelaisButin* R, FVector Point)
{
	UWorld* W = GetWorld();
	if (!R || !W || !R->bFrappePrete)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(R->GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion || FVector::Dist(Point, Pion->GetActorLocation()) > 30000.f)
	{
		return;
	}
	R->bFrappePrete = false;
	R->ForceNetUpdate();

	// Repere rouge pour tout le monde, puis les bombes tombent
	const FTransform Place(Point);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.bDeferConstruction = true;
	if (APingButin* P = W->SpawnActor<APingButin>(APingButin::StaticClass(), Place, Spawn))
	{
		P->Genre = PingFrappe;
		P->Auteur = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
		P->FinishSpawning(Place);
		P->SetLifeSpan(1.5f + Reglages.FrappeBombes * 0.4f + 1.f);
	}
	const float Maintenant = W->GetTimeSeconds();
	for (int32 i = 0; i < Reglages.FrappeBombes; ++i)
	{
		FBombeButin B;
		const FVector2D Ecart = FMath::RandPointInCircle(Reglages.FrappeRayon * 1.1f);
		B.Point = Point + FVector(Ecart.X, Ecart.Y, 0.f);
		B.Heure = Maintenant + 1.5f + i * 0.4f;
		B.PC = PC;
		Bombes.Add(B);
	}
	R->ClientAnnonce(TEXT("FRAPPE AERIENNE EN APPROCHE !"), 4, true);
}

void AButinChelles::GererBombes(float Maintenant)
{
	UWorld* W = GetWorld();
	if (Bombes.Num() == 0 || !W)
	{
		return;
	}
	const bool bPvp = ButinPvp(W);
	for (int32 i = Bombes.Num() - 1; i >= 0; --i)
	{
		const FBombeButin B = Bombes[i];
		if (B.Heure > Maintenant)
		{
			continue;
		}
		Bombes.RemoveAtSwap(i);
		APlayerController* PC = B.PC.Get();
		APawn* Lanceur = PC ? PC->GetPawn() : nullptr;

		FVector Impact = B.Point;
		FHitResult Sol;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ButinSolBombe), false);
		if (W->LineTraceSingleByChannel(Sol, B.Point + FVector(0.f, 0.f, 3000.f), B.Point - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params))
		{
			Impact = Sol.ImpactPoint;
		}
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		W->SpawnActor<AEffetButin>(AEffetButin::StaticClass(), FTransform(Impact), Spawn);

		// Degats autour (le lanceur, et les autres joueurs si le PVP est coupe, sont epargnes)
		TArray<AActor*> Epargnes;
		if (Lanceur)
		{
			Epargnes.Add(Lanceur);
		}
		for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* P = It->Get() ? It->Get()->GetPawn() : nullptr;
			if (P && !bPvp)
			{
				Epargnes.AddUnique(P);
			}
		}
		UGameplayStatics::ApplyRadialDamage(W, Reglages.FrappeDegats, Impact + FVector(0.f, 0.f, 60.f), Reglages.FrappeRayon,
			UDamageType::StaticClass(), Epargnes, Lanceur, PC, false, ECC_Visibility);
		// Le souffle projette les personnages proches
		for (TActorIterator<ACharacter> It(W); It; ++It)
		{
			ACharacter* C = *It;
			if (Epargnes.Contains(C))
			{
				continue;
			}
			const FVector Vers = C->GetActorLocation() - Impact;
			const float D = Vers.Size();
			if (D < Reglages.FrappeRayon)
			{
				const float K = 1.f - D / Reglages.FrappeRayon;
				C->LaunchCharacter(Vers.GetSafeNormal2D() * 700.f * K + FVector(0.f, 0.f, 500.f * K), true, true);
			}
		}
	}
}

// =====================================================================================================
// Serveur : series d'eliminations
// =====================================================================================================

bool AButinChelles::EstEnnemi(const AActor* Acteur)
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
	// Ennemis du kit FPS_Game (et ceux du plugin ChellesEnnemis) : ranges dans Blueprints/Bonus/AI
	for (const UClass* C = Perso->GetClass(); C; C = C->GetSuperClass())
	{
		if (C->GetPathName().Contains(TEXT("/FPS_Game/Blueprints/Bonus/AI/")))
		{
			return true;
		}
	}
	return false;
}

bool AButinChelles::EstMort(const AActor* Acteur)
{
	if (!IsValid(Acteur) || Acteur->IsPendingKillPending() || Acteur->Tags.Contains(MarqueMortButin))
	{
		return true;
	}
	bool bMort = false;
	if (ButinLireBooleen(Acteur, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
	{
		return true;
	}
	return Acteur->GetLifeSpan() > 0.f; // corps qui va disparaitre (plugins Zombies / Ennemis, beaucoup de Blueprints)
}

bool AButinChelles::PionMort(const APawn* Pion)
{
	if (!IsValid(Pion))
	{
		return false;
	}
	bool bMort = false;
	if (ButinLireBooleen(Pion, { TEXT("isDead"), TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort) && bMort)
	{
		return true;
	}
	if (FNumericProperty* Vie = ButinNombre(Pion, { TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie") }))
	{
		return ButinLireNombre(Vie, Pion) <= 0.0;
	}
	return false;
}

void AButinChelles::SuivreEnnemis()
{
	if (!Reglages.bSeries)
	{
		return;
	}
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* E = *It;
		if (!IsValid(E) || E->Tags.Contains(MarqueSuiviButin) || !EstEnnemi(E) || EstMort(E))
		{
			continue;
		}
		E->Tags.AddUnique(MarqueSuiviButin);
		E->OnTakeAnyDamage.AddUniqueDynamic(this, &AButinChelles::EnnemiTouche);
	}
}

void AButinChelles::EnnemiTouche(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur)
{
	if (!Touche || Degats <= 0.f || Touche->Tags.Contains(MarqueMortButin))
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
	const float Maintenant = GetWorld()->GetTimeSeconds();
	FCibleButin& C = Cibles.FindOrAdd(Touche);
	C.Tireur = Tireur;
	C.Heure = Maintenant;
	if (EstMort(Touche))
	{
		VerifierMorts(Maintenant);
	}
}

void AButinChelles::VerifierMorts(float Maintenant)
{
	TArray<TWeakObjectPtr<AController>> Tueurs;
	for (auto It = Cibles.CreateIterator(); It; ++It)
	{
		AActor* E = It.Key().Get();
		const FCibleButin& C = It.Value();
		if (EstMort(E))
		{
			if (Maintenant - C.Heure <= 4.f && C.Tireur.IsValid())
			{
				Tueurs.Add(C.Tireur);
			}
			if (IsValid(E))
			{
				E->Tags.AddUnique(MarqueMortButin);
				E->OnTakeAnyDamage.RemoveDynamic(this, &AButinChelles::EnnemiTouche);
			}
			It.RemoveCurrent();
		}
		else if (Maintenant - C.Heure > 60.f)
		{
			It.RemoveCurrent();
		}
	}
	for (const TWeakObjectPtr<AController>& T : Tueurs)
	{
		if (T.IsValid())
		{
			Elimination(T.Get());
		}
	}
}

void AButinChelles::Elimination(AController* Tireur)
{
	ARelaisButin* R = RelaisDe(Cast<APlayerController>(Tireur));
	if (!R || !Reglages.bSeries)
	{
		return;
	}
	const float Maintenant = GetWorld()->GetTimeSeconds();
	const int32 S = ++R->Serie;
	R->ForceNetUpdate();
	if (Reglages.SerieRadar > 0 && S == Reglages.SerieRadar)
	{
		R->bRadar = true;
		R->FinRadar = Maintenant + Reglages.DureeRadar;
		R->ClientAnnonce(FString::Printf(TEXT("SERIE DE %d : RADAR !  Les ennemis sont marques pendant %d s"), S,
			FMath::RoundToInt(Reglages.DureeRadar)), 1, true);
	}
	else if (Reglages.SerieFrappe > 0 && S == Reglages.SerieFrappe)
	{
		R->bFrappePrete = true;
		R->ClientAnnonce(FString::Printf(TEXT("SERIE DE %d : FRAPPE AERIENNE prete !  Vise et appuie sur %s"), S,
			Reglages.ToucheFrappe == TEXT("Four") ? TEXT("4") : *Reglages.ToucheFrappe), 4, true);
	}
	else if (Reglages.SerieXP > 0 && S == Reglages.SerieXP)
	{
		R->bRadar = true;
		R->FinRadar = Maintenant + Reglages.DureeRadar;
		R->ClientXP(Reglages.XPSerie, FString::Printf(TEXT("serie de %d eliminations"), S));
		R->ClientAnnonce(FString::Printf(TEXT("SERIE DE %d : DECHAINE !  +%d XP et RADAR"), S, Reglages.XPSerie), 3, true);
	}
	else
	{
		R->ClientAnnonce(FString::Printf(TEXT("ELIMINATION  x%d"), S), 0, false);
	}
}

void AButinChelles::GererSeries(float Maintenant)
{
	for (TActorIterator<ARelaisButin> It(GetWorld()); It; ++It)
	{
		ARelaisButin* R = *It;
		APlayerController* PC = Cast<APlayerController>(R->GetOwner());
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (R->bRadar && Maintenant > R->FinRadar)
		{
			R->bRadar = false;
			R->ForceNetUpdate();
		}
		// Mort : la serie repart de zero (monter en voiture ou changer de heros ne compte pas)
		if (R->Serie > 0 && PionMort(Pion))
		{
			R->Serie = 0;
			R->bRadar = false;
			R->bFrappePrete = false;
			R->ForceNetUpdate();
			R->ClientAnnonce(TEXT("Serie perdue"), 0, false);
		}
		R->DernierPion = Pion;
	}
}

// =====================================================================================================
// Joueur local
// =====================================================================================================

void AButinChelles::JoueurLocal(float Dt)
{
	APlayerController* PC = PCLocal();
	if (!PC)
	{
		return;
	}
	bCurseur = PC->bShowMouseCursor;
	APawn* Pion = PC->GetPawn();
	ARelaisButin* R = MonRelais();
	TexteAide.Reset();
	TexteSerie.Reset();
	if (R && R->Serie > 0)
	{
		TexteSerie = FString::Printf(TEXT("SERIE : %d"), R->Serie);
		if (R->bFrappePrete)
		{
			TexteSerie += FString::Printf(TEXT("     FRAPPE AERIENNE PRETE (touche %s)"), Reglages.ToucheFrappe == TEXT("Four") ? TEXT("4") : *Reglages.ToucheFrappe);
		}
		if (R->bRadar)
		{
			TexteSerie += TEXT("     RADAR");
		}
	}

	// Coffre a portee
	if (R && Reglages.bCoffres && Cast<ACharacter>(Pion) && !bCurseur)
	{
		ACoffreButin* Proche = nullptr;
		float Meilleur = FMath::Square(250.f);
		for (TActorIterator<ACoffreButin> It(GetWorld()); It; ++It)
		{
			if (It->bOuvert)
			{
				continue;
			}
			const float D = FVector::DistSquared(It->Centre(), Pion->GetActorLocation());
			if (D < Meilleur)
			{
				Meilleur = D;
				Proche = *It;
			}
		}
		if (Proche)
		{
			TexteAide = FString::Printf(TEXT("%s : ouvrir le %s"), *Reglages.ToucheOuvrir, NomRarete(Proche->Rarete));
			if (PC->WasInputKeyJustPressed(ButinTouche(Reglages.ToucheOuvrir, EKeys::E)))
			{
				R->ServeurOuvrir(Proche);
			}
		}
	}

	if (R && Pion && !bCurseur)
	{
		if (Reglages.bPing && PC->WasInputKeyJustPressed(ButinTouche(Reglages.TouchePing, EKeys::J)))
		{
			Marquer(PC, Pion);
		}
		if (R->bFrappePrete && PC->WasInputKeyJustPressed(ButinTouche(Reglages.ToucheFrappe, EKeys::Four)))
		{
			AppelerFrappe(PC, Pion);
		}
	}
	GererRadar(PC, Pion, R && R->bRadar && !bCurseur);
}

void AButinChelles::Marquer(APlayerController* PC, APawn* Pion)
{
	FVector Oeil;
	FRotator Regard;
	PC->GetPlayerViewPoint(Oeil, Regard);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ButinViseePing), false, Pion);
	FHitResult Vise;
	if (!GetWorld()->LineTraceSingleByChannel(Vise, Oeil, Oeil + Regard.Vector() * 30000.f, ECC_Visibility, Params))
	{
		Annonce(TEXT("Trop loin pour marquer"), 0, false);
		return;
	}
	AActor* Acteur = Vise.GetActor();
	uint8 Genre = PingIci;
	if (Cast<ACoffreButin>(Acteur))
	{
		Genre = PingButin;
	}
	else if (EstEnnemi(Acteur))
	{
		Genre = PingEnnemi;
	}
	else if (Acteur && !Cast<ACharacter>(Acteur) && (Cast<APawn>(Acteur) || Acteur->GetClass()->GetName().Contains(TEXT("Vehic"))))
	{
		Genre = PingVehicule;
	}
	else
	{
		for (TActorIterator<ACoffreButin> It(GetWorld()); It; ++It)
		{
			if (!It->bOuvert && FVector::DistSquared(It->GetActorLocation(), Vise.ImpactPoint) < FMath::Square(300.f))
			{
				Genre = PingButin;
				break;
			}
		}
	}
	if (ARelaisButin* R = MonRelais())
	{
		R->ServeurPing(Vise.ImpactPoint, Genre);
	}
}

void AButinChelles::AppelerFrappe(APlayerController* PC, APawn* Pion)
{
	FVector Oeil;
	FRotator Regard;
	PC->GetPlayerViewPoint(Oeil, Regard);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ButinViseeFrappe), false, Pion);
	FHitResult Vise;
	if (!GetWorld()->LineTraceSingleByChannel(Vise, Oeil, Oeil + Regard.Vector() * 25000.f, ECC_Visibility, Params))
	{
		Annonce(TEXT("Vise le sol (moins de 250 m) pour la frappe aerienne"), 4, false);
		return;
	}
	if (FVector::Dist(Vise.ImpactPoint, Pion->GetActorLocation()) < 800.f)
	{
		Annonce(TEXT("Trop pres de toi ! Vise plus loin"), 4, false);
		return;
	}
	if (ARelaisButin* R = MonRelais())
	{
		R->ServeurFrappe(Vise.ImpactPoint);
	}
}

void AButinChelles::GererRadar(APlayerController* PC, APawn* Pion, bool bActif)
{
	PointsRadar.Reset();
	if (!bActif || !PC || !PC->PlayerCameraManager)
	{
		return;
	}
	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* E = *It;
		if (!EstEnnemi(E) || PionMort(E) || E->IsHidden())
		{
			continue;
		}
		const float Distance = FVector::Dist(Camera, E->GetActorLocation());
		if (Distance > Reglages.DistanceRadar)
		{
			continue;
		}
		const float Haut = E->GetCapsuleComponent() ? E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
		FVector2D Ecran;
		if (PC->ProjectWorldLocationToScreen(E->GetActorLocation() + FVector(0.f, 0.f, Haut + 30.f), Ecran, false))
		{
			FPointRadar P;
			P.Ecran = Ecran;
			P.Distance = Distance;
			PointsRadar.Add(P);
			if (PointsRadar.Num() >= 40)
			{
				break;
			}
		}
	}
}

void AButinChelles::Annonce(const FString& Texte, uint8 Couleur, bool bSon)
{
	UE_LOG(LogTemp, Log, TEXT("[Butin] %s"), *Texte);
	TexteAnnonce = Texte;
	CouleurAnnonce = ButinCouleur(Couleur);
	AgeAnnonce = 0.f;
	if (bSon)
	{
		if (USoundBase* Son = LoadObject<USoundBase>(nullptr, *Reglages.SonSerie, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UGameplayStatics::PlaySound2D(this, Son, 0.9f);
		}
	}
}

void AButinChelles::DonnerXPLocale(int32 Quantite, const FString& Raison)
{
	// L'XP est tenue et sauvegardee par le plugin ChellesPouvoirs sur la machine du joueur : on l'appelle par son nom
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
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("[Butin] +%d XP (%s) : plugin ChellesPouvoirs absent, XP non comptee."), Quantite, *Raison);
}

// =====================================================================================================
// Ecran
// =====================================================================================================

void AButinChelles::CreerEcran()
{
	UWorld* W = GetWorld();
	if (Ecran.IsValid() || !W || !FSlateApplication::IsInitialized())
	{
		return;
	}
	UGameViewportClient* VueJeu = W->GetGameViewport();
	if (!VueJeu)
	{
		return;
	}
	TWeakObjectPtr<AButinChelles> Moi(this);
	Ecran = SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SRadarButin, Moi)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 110.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f))
			.Justification(ETextJustify::Center)
			.ColorAndOpacity_Lambda([Moi]()
			{
				if (!Moi.IsValid())
				{
					return FSlateColor(FLinearColor::Transparent);
				}
				FLinearColor C = Moi->CouleurAnnonce;
				C.A = FMath::Clamp(4.f - Moi->AgeAnnonce, 0.f, 1.f);
				return FSlateColor(C);
			})
			.Text_Lambda([Moi]()
			{
				return (Moi.IsValid() && !Moi->bCurseur && Moi->AgeAnnonce < 4.f) ? FText::FromString(Moi->TexteAnnonce) : FText::GetEmpty();
			})
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.f, 160.f, 0.f, 0.f))
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
			.ShadowOffset(FVector2D(1.f, 1.f))
			.ColorAndOpacity(FLinearColor(1.f, 0.55f, 0.15f, 0.95f))
			.Text_Lambda([Moi]()
			{
				return (Moi.IsValid() && !Moi->bCurseur) ? FText::FromString(Moi->TexteSerie) : FText::GetEmpty();
			})
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 225.f))
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.3f, 1.f))
			.Text_Lambda([Moi]()
			{
				return (Moi.IsValid() && !Moi->bCurseur) ? FText::FromString(Moi->TexteAide) : FText::GetEmpty();
			})
		];
	VueJeu->AddViewportWidgetContent(Ecran.ToSharedRef(), 5);
}

void AButinChelles::RetirerEcran()
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

// =====================================================================================================
// Console : Chelles.Butin ...
// =====================================================================================================

void AButinChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Ordre = Arguments.Num() > 0 ? Arguments[0] : FString();
	if (Ordre.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		ChargerReglagesButin(Reglages);
		Annonce(TEXT("Butin.txt relu."), 0, false);
		return;
	}
	if (Ordre.Equals(TEXT("Coffre"), ESearchCase::IgnoreCase))
	{
		// Test (solo ou hote) : un coffre legendaire juste devant le joueur
		APlayerController* PC = PCLocal();
		APawn* Pion = PC ? PC->GetPawn() : nullptr;
		if (GetNetMode() == NM_Client || !Pion)
		{
			Annonce(TEXT("Chelles.Butin Coffre : seulement en solo ou pour celui qui heberge"), 0, false);
			return;
		}
		const FVector Devant = Pion->GetActorLocation() + Pion->GetActorForwardVector() * 300.f;
		FHitResult Sol;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ButinCoffreTest), false, Pion);
		FVector Lieu = Devant;
		if (GetWorld()->LineTraceSingleByChannel(Sol, Devant + FVector(0.f, 0.f, 200.f), Devant - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params))
		{
			Lieu = Sol.ImpactPoint;
		}
		const FTransform Place(FRotator(0.f, Pion->GetActorRotation().Yaw + 180.f, 0.f), Lieu);
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Spawn.bDeferConstruction = true;
		if (ACoffreButin* C = GetWorld()->SpawnActor<ACoffreButin>(ACoffreButin::StaticClass(), Place, Spawn))
		{
			C->Rarete = Legendaire;
			C->FinishSpawning(Place);
			Coffres.Add(C);
		}
		return;
	}
	if (Ordre.Equals(TEXT("Serie"), ESearchCase::IgnoreCase))
	{
		// Test : ajoute des eliminations a la serie du joueur
		if (GetNetMode() == NM_Client)
		{
			return;
		}
		const int32 N = Arguments.Num() > 1 ? FMath::Clamp(FCString::Atoi(*Arguments[1]), 1, 20) : 1;
		for (int32 i = 0; i < N; ++i)
		{
			Elimination(PCLocal());
		}
		return;
	}
	ARelaisButin* R = MonRelais();
	int32 Ouverts = 0, Fermes = 0;
	for (TActorIterator<ACoffreButin> It(GetWorld()); It; ++It)
	{
		(It->bOuvert ? Ouverts : Fermes)++;
	}
	Annonce(FString::Printf(TEXT("Butin : %d coffres a ouvrir, serie %d, radar %s, frappe %s.  Chelles.Butin Relire | Coffre | Serie N"),
		Fermes, R ? R->Serie : 0, (R && R->bRadar) ? TEXT("oui") : TEXT("non"), (R && R->bFrappePrete) ? TEXT("prete") : TEXT("non")), 0, false);
}
