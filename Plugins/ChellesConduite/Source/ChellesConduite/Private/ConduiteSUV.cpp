// CHELLES 3D - Conduite du SUV (voir ConduiteSUV.h).
//
// Deux choses :
//  1) Au moment ou le SUV apparait : pneus plus accrocheurs, roues avant qui braquent moins,
//     amortisseurs plus fermes, centre de gravite plus bas. Les valeurs du pack sont changees
//     en memoire seulement et remises a la fin du jeu (rien n'est enregistre).
//  2) A chaque image, quand le SUV touche le sol : on retire la glisse sur le cote, on limite la
//     vitesse de rotation (plus de tete-a-queue), on empeche la caisse de trop se pencher et on la
//     plaque un peu au sol. C'est ce qui donne la conduite "facile" de la motoneige.

#include "ConduiteSUV.h"

#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodyInstance.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#include "TireConfig.h"
#include "VehicleWheel.h"
#include "WheeledVehicleMovementComponent.h"

namespace
{
	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesConduite"), TEXT("ConduiteSUV.txt"));
	}

	bool EstOui(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		return T.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || T.Equals(TEXT("1")) || T.Equals(TEXT("True"), ESearchCase::IgnoreCase) || T.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	UWheeledVehicleMovementComponent* Roulement(const APawn* Pion)
	{
		return Pion ? Pion->FindComponentByClass<UWheeledVehicleMovementComponent>() : nullptr;
	}

	UPrimitiveComponent* Caisse(const APawn* Pion)
	{
		return Pion ? Cast<UPrimitiveComponent>(Pion->GetRootComponent()) : nullptr;
	}
}

AConduiteSUV::AConduiteSUV()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	Vehicules.Add(TEXT("SUV_BP_child"));
}

void AConduiteSUV::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	UE_LOG(LogTemp, Log, TEXT("[SUV] Conduite stable %s (vehicules : %s)"), bActif ? TEXT("active") : TEXT("coupee"), *FString::Join(Vehicules, TEXT(", ")));
}

void AConduiteSUV::EndPlay(const EEndPlayReason::Type Raison)
{
	RemettreRoues();
	Super::EndPlay(Raison);
}

void AConduiteSUV::LireReglages()
{
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *CheminReglages()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SUV] %s introuvable : reglages par defaut."), *CheminReglages());
		return;
	}

	for (FString Ligne : Lignes)
	{
		Ligne = Ligne.TrimStartAndEnd();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle = Cle.TrimStartAndEnd();
		Valeur = Valeur.TrimStartAndEnd();
		const float Nombre = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));

		if (Cle.Equals(TEXT("Actif"), ESearchCase::IgnoreCase))                { bActif = EstOui(Valeur); }
		else if (Cle.Equals(TEXT("Vehicules"), ESearchCase::IgnoreCase))
		{
			TArray<FString> Noms;
			Valeur.ParseIntoArray(Noms, TEXT(","), true);
			Vehicules.Reset();
			for (const FString& Nom : Noms)
			{
				if (!Nom.TrimStartAndEnd().IsEmpty())
				{
					Vehicules.Add(Nom.TrimStartAndEnd());
				}
			}
		}
		else if (Cle.Equals(TEXT("Adherence"), ESearchCase::IgnoreCase))      { Adherence = FMath::Max(0.f, Nombre); }
		else if (Cle.Equals(TEXT("VirageMax"), ESearchCase::IgnoreCase))      { VirageMaxDeg = FMath::Max(5.f, Nombre); }
		else if (Cle.Equals(TEXT("RayonMini"), ESearchCase::IgnoreCase))      { RayonMini = FMath::Max(1.f, Nombre); }
		else if (Cle.Equals(TEXT("ForceVirage"), ESearchCase::IgnoreCase))    { AccelLaterale = FMath::Max(1.f, Nombre); }
		else if (Cle.Equals(TEXT("AntiRoulis"), ESearchCase::IgnoreCase))     { AntiRoulis = FMath::Max(0.f, Nombre); }
		else if (Cle.Equals(TEXT("Appui"), ESearchCase::IgnoreCase))          { Appui = FMath::Clamp(Nombre, 0.f, 3.f); }
		else if (Cle.Equals(TEXT("AbaisserCentre"), ESearchCase::IgnoreCase)) { AbaisserCentre = FMath::Clamp(Nombre, -200.f, 200.f); }
		else if (Cle.Equals(TEXT("Friction"), ESearchCase::IgnoreCase))       { Friction = FMath::Clamp(Nombre, 0.1f, 10.f); }
		else if (Cle.Equals(TEXT("Rigidite"), ESearchCase::IgnoreCase))       { Rigidite = FMath::Clamp(Nombre, 0.1f, 10.f); }
		else if (Cle.Equals(TEXT("Braquage"), ESearchCase::IgnoreCase))       { Braquage = FMath::Clamp(Nombre, 0.f, 80.f); }
		else if (Cle.Equals(TEXT("Amortissement"), ESearchCase::IgnoreCase))  { Amortissement = FMath::Clamp(Nombre, 0.f, 5.f); }
	}
}

bool AConduiteSUV::EstConcerne(const APawn* Pion) const
{
	if (!Pion || !Roulement(Pion))
	{
		return false;
	}
	// La classe ou une de ses classes parentes porte un des noms de la liste
	for (const UClass* Classe = Pion->GetClass(); Classe; Classe = Classe->GetSuperClass())
	{
		const FString Nom = Classe->GetName();
		for (const FString& Cherche : Vehicules)
		{
			if (Nom.StartsWith(Cherche, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}
	return false;
}

void AConduiteSUV::ChercherVehicules()
{
	Suivis.RemoveAll([](const TWeakObjectPtr<APawn>& P) { return !P.IsValid(); });
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Pion = *It;
		if (Pion && !Prets.Contains(Pion) && EstConcerne(Pion))
		{
			Preparer(Pion);
			Prets.Add(Pion);
			Suivis.Add(Pion);
		}
	}
}

void AConduiteSUV::RegleRoue(UClass* ClasseRoue, bool bAvant)
{
	if (!ClasseRoue)
	{
		return;
	}
	for (const FRoueOrigineChelles& O : Origines)
	{
		if (O.Classe == ClasseRoue)
		{
			return; // deja reglee
		}
	}
	UVehicleWheel* Roue = ClasseRoue->GetDefaultObject<UVehicleWheel>();
	if (!Roue)
	{
		return;
	}

	FRoueOrigineChelles O;
	O.Classe = ClasseRoue;
	O.Pneu = Roue->TireConfig;
	O.LatStiffValue = Roue->LatStiffValue;
	O.LongStiffValue = Roue->LongStiffValue;
	O.SteerAngle = Roue->SteerAngle;
	O.SuspensionDampingRatio = Roue->SuspensionDampingRatio;
	Origines.Add(O);

	// Pneu plus accrocheur (nouveau pneu en memoire, celui du pack n'est pas touche)
	UTireConfig* Pneu = NewObject<UTireConfig>(this);
	const float Base = Roue->TireConfig ? Roue->TireConfig->GetFrictionScale() : 1.f;
	Pneu->SetFrictionScale(Base * Friction);
	Pneus.Add(Pneu);
	Roue->TireConfig = Pneu;

	Roue->LatStiffValue = O.LatStiffValue * Rigidite;
	Roue->LongStiffValue = O.LongStiffValue * FMath::Max(1.f, Rigidite * 0.8f);
	if (bAvant && Braquage > 0.f && Roue->SteerAngle > Braquage)
	{
		Roue->SteerAngle = Braquage;
	}
	if (Amortissement > 0.f)
	{
		Roue->SuspensionDampingRatio = FMath::Max(Roue->SuspensionDampingRatio, Amortissement);
	}

	UE_LOG(LogTemp, Log, TEXT("[SUV] Roue %s : friction %.2f -> %.2f, rigidite %.1f -> %.1f, braquage %.0f -> %.0f, amortisseurs %.2f -> %.2f"),
		*ClasseRoue->GetName(), Base, Base * Friction, O.LatStiffValue, Roue->LatStiffValue, O.SteerAngle, Roue->SteerAngle,
		O.SuspensionDampingRatio, Roue->SuspensionDampingRatio);
}

void AConduiteSUV::RemettreRoues()
{
	for (const FRoueOrigineChelles& O : Origines)
	{
		if (UVehicleWheel* Roue = O.Classe ? O.Classe->GetDefaultObject<UVehicleWheel>() : nullptr)
		{
			Roue->TireConfig = Cast<UTireConfig>(O.Pneu);
			Roue->LatStiffValue = O.LatStiffValue;
			Roue->LongStiffValue = O.LongStiffValue;
			Roue->SteerAngle = O.SteerAngle;
			Roue->SuspensionDampingRatio = O.SuspensionDampingRatio;
		}
	}
	Origines.Reset();
}

void AConduiteSUV::Preparer(APawn* Pion)
{
	UWheeledVehicleMovementComponent* Mouvement = Roulement(Pion);
	if (!Mouvement || !bActif)
	{
		return;
	}

	// 1) Roues (lues par Unreal au moment ou la physique du vehicule est creee)
	for (const FWheelSetup& Reglage : Mouvement->WheelSetups)
	{
		UClass* Classe = Reglage.WheelClass;
		const UVehicleWheel* Defaut = Classe ? Classe->GetDefaultObject<UVehicleWheel>() : nullptr;
		const bool bAvant = Defaut && !Reglage.bDisableSteering && Defaut->SteerAngle > 0.1f;
		RegleRoue(Classe, bAvant);
	}

	// 2) Centre de gravite plus bas = la voiture ne bascule plus sur le cote dans les virages
	if (UPrimitiveComponent* Corps = Caisse(Pion))
	{
		if (FBodyInstance* Corps0 = Corps->GetBodyInstance())
		{
			Corps0->COMNudge.Z += AbaisserCentre;
			Corps0->UpdateMassProperties();
		}
	}

	// 3) On recree la physique du vehicule pour qu'il prenne les nouvelles roues
	Mouvement->RecreatePhysicsState();

	UE_LOG(LogTemp, Log, TEXT("[SUV] %s prepare : masse %.0f kg, centre de gravite %+.0f cm"), *Pion->GetName(), Mouvement->Mass, AbaisserCentre);
}

void AConduiteSUV::Aider(APawn* Pion, float Dt)
{
	UWheeledVehicleMovementComponent* Mouvement = Roulement(Pion);
	UPrimitiveComponent* Corps = Caisse(Pion);
	if (!Mouvement || !Corps || !Corps->IsSimulatingPhysics() || Dt <= 0.f)
	{
		return;
	}

	// Seulement quand au moins 2 roues touchent le sol (en l'air on laisse faire)
	int32 AuSol = 0;
	for (const UVehicleWheel* Roue : Mouvement->Wheels)
	{
		if (Roue && !Roue->IsInAir())
		{
			++AuSol;
		}
	}
	if (AuSol < 2)
	{
		return;
	}

	const FTransform T = Corps->GetComponentTransform();
	const FVector Avant = T.GetUnitAxis(EAxis::X);
	const FVector Droite = T.GetUnitAxis(EAxis::Y);
	const FVector Haut = T.GetUnitAxis(EAxis::Z);

	FVector Vitesse = Corps->GetPhysicsLinearVelocity();                  // cm/s
	FVector Rotation = Corps->GetPhysicsAngularVelocityInRadians();       // rad/s

	// a) Plus de glisse sur le cote : la voiture va la ou elle pointe
	const float Lateral = FVector::DotProduct(Vitesse, Droite);
	Vitesse -= Droite * Lateral * (1.f - FMath::Exp(-Adherence * Dt));

	// b) Rotation limitee : on tourne franchement, mais sans tete-a-queue
	const float VitM = FMath::Abs(FVector::DotProduct(Vitesse, Avant)) / 100.f; // m/s
	float LacetMax = FMath::DegreesToRadians(VirageMaxDeg);
	if (VitM > 0.5f)
	{
		LacetMax = FMath::Min(LacetMax, FMath::Max(VitM / RayonMini, FMath::DegreesToRadians(25.f)));
		LacetMax = FMath::Min(LacetMax, FMath::Max(AccelLaterale / VitM, FMath::DegreesToRadians(12.f)));
	}
	const float Lacet = FVector::DotProduct(Rotation, Haut);
	if (FMath::Abs(Lacet) > LacetMax)
	{
		Rotation -= Haut * (Lacet - FMath::Sign(Lacet) * LacetMax);
	}

	// c) Anti-roulis : la caisse ne se penche plus / ne se retourne plus
	const FVector Bascule = Rotation - Haut * FVector::DotProduct(Rotation, Haut);
	Rotation -= Bascule * (1.f - FMath::Exp(-AntiRoulis * Dt));

	Corps->SetPhysicsLinearVelocity(Vitesse);
	Corps->SetPhysicsAngularVelocityInRadians(Rotation);

	// d) Plaquee au sol quand elle va vite (comme un appui aerodynamique)
	if (Appui > 0.f)
	{
		const float Part = FMath::Clamp(VitM / 20.f, 0.f, 1.f);
		Corps->AddForce(-Haut * 980.f * Appui * Part, NAME_None, true);
	}
}

void AConduiteSUV::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ProchaineRecherche -= DeltaSeconds;
	if (ProchaineRecherche <= 0.f)
	{
		ProchaineRecherche = 0.5f;
		if (bActif)
		{
			ChercherVehicules();
		}
	}

	if (!bActif)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	for (const TWeakObjectPtr<APawn>& Pion : Suivis)
	{
		if (Pion.IsValid())
		{
			Aider(Pion.Get(), Dt);
		}
	}
}

void AConduiteSUV::Commande(const TArray<FString>& Arguments)
{
	const FString Mot = Arguments.Num() > 0 ? Arguments[0] : FString(TEXT("Info"));
	if (Mot == TEXT("0"))
	{
		bActif = false;
		UE_LOG(LogTemp, Log, TEXT("[SUV] Aide a la conduite coupee (les pneus restent regles jusqu'a la fin du jeu)."));
	}
	else if (Mot == TEXT("1"))
	{
		bActif = true;
		UE_LOG(LogTemp, Log, TEXT("[SUV] Aide a la conduite remise."));
	}
	else if (Mot.Equals(TEXT("Relire"), ESearchCase::IgnoreCase))
	{
		LireReglages();
		UE_LOG(LogTemp, Log, TEXT("[SUV] Reglages relus. Adherence, virage, anti-roulis, appui : tout de suite. Pneus, braquage, centre de gravite : au prochain Play."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[SUV] Aide %s, %d vehicule(s) suivi(s), adherence %.1f, virage max %.0f deg/s, anti-roulis %.1f, appui %.2f, friction x%.1f, braquage %.0f"),
			bActif ? TEXT("active") : TEXT("coupee"), Suivis.Num(), Adherence, VirageMaxDeg, AntiRoulis, Appui, Friction, Braquage);
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
