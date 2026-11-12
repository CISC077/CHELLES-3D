// CHELLES 3D - Bus pendant l'invasion zombie (voir BusChelles.h)
#include "BusChelles.h"
#include "ArretBusChelles.h"
#include "Algo/BinarySearch.h"
#include "Animation/AnimSequence.h"
#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogBusChelles, Log, All);

namespace
{
	constexpr float KmhVersCms = 100000.f / 3600.f;
	constexpr float Acceleration = 170.f;   // cm/s^2
	constexpr float Freinage = 300.f;       // cm/s^2 (confortable, pour arriver a l'arret)
	constexpr float FreinageUrgence = 900.f; // un joueur devant le bus
	constexpr int32 MaxPointsTrajet = 1800; // le trajet est envoye aux joueurs en une fois

	FReglagesBus GReglages;
	bool GReglagesLus = false;

	bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	void ChargerReglages()
	{
		GReglagesLus = true;
		GReglages = FReglagesBus();
		const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesBus"), TEXT("Bus.txt"));
		TArray<FString> Lignes;
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogBusChelles, Log, TEXT("[Bus] %s absent : reglages par defaut."), *Chemin);
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
			FReglagesBus& R = GReglages;
			if (Cle == TEXT("Actif")) R.bActif = EstOui(Valeur);
			else if (Cle == TEXT("RetirerLesVoitures")) R.bRetirerVoitures = EstOui(Valeur);
			else if (Cle == TEXT("ClasseBus") && !Valeur.IsEmpty()) R.ClasseBus = Valeur;
			else if (Cle == TEXT("VitesseKmh")) R.VitesseKmh = FMath::Clamp(Nombre, 5.f, 90.f);
			else if (Cle == TEXT("AttenteJoueurDehorsSecondes")) R.AttenteDehorsSecondes = FMath::Clamp(Nombre, 1.f, 300.f);
			else if (Cle == TEXT("AttenteJoueurDedansSecondes")) R.AttenteDedansSecondes = FMath::Clamp(Nombre, 1.f, 300.f);
			else if (Cle == TEXT("TouchePorte") && !Valeur.IsEmpty()) R.TouchePorte = Valeur;
			else if (Cle == TEXT("DistancePorteMetres")) R.DistancePorteMetres = FMath::Clamp(Nombre, 1.f, 10.f);
			else if (Cle == TEXT("HauteurPlancher")) R.HauteurPlancher = FMath::Clamp(Nombre, 5.f, 120.f);
			else if (Cle == TEXT("CorrectionOrientation")) R.CorrectionOrientation = Nombre;
			else if (Cle == TEXT("DistanceMaxArretRouteMetres")) R.DistanceMaxArretRouteMetres = FMath::Clamp(Nombre, 5.f, 500.f);
			else if (Cle == TEXT("MessageDepart")) R.bMessageDepart = EstOui(Valeur);
		}
		UE_LOG(LogBusChelles, Log, TEXT("[Bus] Reglages lus : %s (vitesse %.0f km/h, attente %.0f s dehors / %.0f s dedans, touche %s)."),
			*Chemin, GReglages.VitesseKmh, GReglages.AttenteDehorsSecondes, GReglages.AttenteDedansSecondes, *GReglages.TouchePorte);
	}

	UClass* TrouverClasse(const TCHAR* Chemin)
	{
		return FindObject<UClass>(nullptr, Chemin);
	}

	FVector Droite(const FVector& Direction)
	{
		return FVector(-Direction.Y, Direction.X, 0.f); // meme convention que ChellesCirculation (on roule a droite)
	}

	/** Le reseau routier du Circulation Spawner, lu par reflexion (le plugin circulation reste independant). */
	struct FReseau
	{
		struct FTroncon
		{
			int32 De = INDEX_NONE;
			int32 Vers = INDEX_NONE;
			int32 Inverse = INDEX_NONE;
			float Longueur = 0.f;
			float Decalage = 0.f;
			FVector Direction = FVector::ForwardVector;
		};
		TArray<FVector> Noeuds;
		TArray<TArray<int32>> Sorties;
		TArray<FTroncon> Troncons;

		FVector Point(int32 T, float S) const
		{
			const FTroncon& Tr = Troncons[T];
			const float Alpha = Tr.Longueur > 0.f ? FMath::Clamp(S / Tr.Longueur, 0.f, 1.f) : 0.f;
			return FMath::Lerp(Noeuds[Tr.De], Noeuds[Tr.Vers], Alpha) + Droite(Tr.Direction) * Tr.Decalage;
		}
	};

	bool LireReseau(UWorld* Monde, FReseau& R)
	{
		UClass* Classe = TrouverClasse(TEXT("/Script/ChellesCirculation.CirculationSpawner"));
		if (!Classe)
		{
			return false;
		}
		FArrayProperty* PN = FindFProperty<FArrayProperty>(Classe, TEXT("Noeuds"));
		FArrayProperty* PT = FindFProperty<FArrayProperty>(Classe, TEXT("Troncons"));
		FStructProperty* SN = PN ? CastField<FStructProperty>(PN->Inner) : nullptr;
		FStructProperty* ST = PT ? CastField<FStructProperty>(PT->Inner) : nullptr;
		if (!SN || !ST)
		{
			return false;
		}
		FStructProperty* NPosition = FindFProperty<FStructProperty>(SN->Struct, TEXT("Position"));
		FArrayProperty* NSorties = FindFProperty<FArrayProperty>(SN->Struct, TEXT("Sorties"));
		FIntProperty* TDe = FindFProperty<FIntProperty>(ST->Struct, TEXT("De"));
		FIntProperty* TVers = FindFProperty<FIntProperty>(ST->Struct, TEXT("Vers"));
		FIntProperty* TInverse = FindFProperty<FIntProperty>(ST->Struct, TEXT("Inverse"));
		FFloatProperty* TLongueur = FindFProperty<FFloatProperty>(ST->Struct, TEXT("Longueur"));
		FFloatProperty* TDecalage = FindFProperty<FFloatProperty>(ST->Struct, TEXT("Decalage"));
		FStructProperty* TDirection = FindFProperty<FStructProperty>(ST->Struct, TEXT("Direction"));
		if (!NPosition || !NSorties || !TDe || !TVers || !TInverse || !TLongueur || !TDecalage || !TDirection)
		{
			UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Le reseau routier de ChellesCirculation n'a pas la forme attendue."));
			return false;
		}
		for (TActorIterator<AActor> It(Monde, Classe); It; ++It)
		{
			AActor* Spawner = *It;
			FScriptArrayHelper HN(PN, PN->ContainerPtrToValuePtr<void>(Spawner));
			FScriptArrayHelper HT(PT, PT->ContainerPtrToValuePtr<void>(Spawner));
			if (HT.Num() == 0 || HN.Num() == 0)
			{
				continue;
			}
			R.Noeuds.SetNum(HN.Num());
			R.Sorties.SetNum(HN.Num());
			for (int32 i = 0; i < HN.Num(); ++i)
			{
				const uint8* Element = HN.GetRawPtr(i);
				R.Noeuds[i] = *NPosition->ContainerPtrToValuePtr<FVector>(Element);
				FScriptArrayHelper HS(NSorties, NSorties->ContainerPtrToValuePtr<void>(Element));
				for (int32 j = 0; j < HS.Num(); ++j)
				{
					R.Sorties[i].Add(*reinterpret_cast<const int32*>(HS.GetRawPtr(j)));
				}
			}
			R.Troncons.SetNum(HT.Num());
			for (int32 i = 0; i < HT.Num(); ++i)
			{
				const uint8* Element = HT.GetRawPtr(i);
				FReseau::FTroncon& T = R.Troncons[i];
				T.De = TDe->GetPropertyValue_InContainer(Element);
				T.Vers = TVers->GetPropertyValue_InContainer(Element);
				T.Inverse = TInverse->GetPropertyValue_InContainer(Element);
				T.Longueur = TLongueur->GetPropertyValue_InContainer(Element);
				T.Decalage = TDecalage->GetPropertyValue_InContainer(Element);
				T.Direction = *TDirection->ContainerPtrToValuePtr<FVector>(Element);
			}
			// Securite : indices valides
			const int32 NbNoeuds = R.Noeuds.Num();
			for (const FReseau::FTroncon& T : R.Troncons)
			{
				if (!R.Noeuds.IsValidIndex(T.De) || !R.Noeuds.IsValidIndex(T.Vers))
				{
					R = FReseau();
					return false;
				}
			}
			for (TArray<int32>& S : R.Sorties)
			{
				S.RemoveAll([&R](int32 T) { return !R.Troncons.IsValidIndex(T); });
			}
			UE_LOG(LogBusChelles, Log, TEXT("[Bus] Reseau routier lu : %d points, %d troncons."), NbNoeuds, R.Troncons.Num());
			return true;
		}
		return false;
	}

	/** Position d'un arret sur la route : troncon + distance sur ce troncon. */
	struct FArretSurRoute
	{
		int32 Troncon = INDEX_NONE;
		float S = 0.f;
		FVector Point = FVector::ZeroVector;
		FString Nom;
		int32 Ordre = 0;
	};

	/** Plus court chemin (en suivant le sens de circulation) d'un arret a l'autre. Renvoie les points du trajet. */
	bool CheminEntre(const FReseau& R, const FArretSurRoute& A, const FArretSurRoute& B, bool bMemeArret, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		if (!bMemeArret && A.Troncon == B.Troncon && B.S >= A.S)
		{
			OutPoints.Add(A.Point);
			OutPoints.Add(B.Point);
			return true;
		}
		const int32 N = R.Troncons.Num();
		TArray<float> Dist;
		Dist.Init(TNumericLimits<float>::Max(), N);
		TArray<int32> Precedent;
		Precedent.Init(INDEX_NONE, N);
		TArray<bool> Fait;
		Fait.Init(false, N);

		typedef TPair<float, int32> FEntree;
		TArray<FEntree> Tas;
		auto Plus = [](const FEntree& X, const FEntree& Y) { return X.Key < Y.Key; };

		auto Suivants = [&R](int32 T, TArray<int32>& Out)
		{
			Out.Reset();
			const FReseau::FTroncon& Tr = R.Troncons[T];
			for (int32 Sortie : R.Sorties[Tr.Vers])
			{
				if (Sortie != Tr.Inverse) { Out.Add(Sortie); }
			}
			if (Out.Num() == 0 && Tr.Inverse != INDEX_NONE)
			{
				Out.Add(Tr.Inverse); // cul-de-sac : demi-tour
			}
		};

		TArray<int32> Voisins;
		Suivants(A.Troncon, Voisins);
		const float Debut = FMath::Max(0.f, R.Troncons[A.Troncon].Longueur - A.S);
		for (int32 V : Voisins)
		{
			if (Debut < Dist[V])
			{
				Dist[V] = Debut;
				Precedent[V] = INDEX_NONE;
				Tas.HeapPush(FEntree(Debut, V), Plus);
			}
		}
		bool bTrouve = false;
		while (Tas.Num() > 0)
		{
			FEntree E;
			Tas.HeapPop(E, Plus);
			const int32 T = E.Value;
			if (Fait[T]) { continue; }
			Fait[T] = true;
			if (T == B.Troncon)
			{
				bTrouve = true;
				break;
			}
			const float Suite = Dist[T] + R.Troncons[T].Longueur;
			Suivants(T, Voisins);
			for (int32 V : Voisins)
			{
				if (!Fait[V] && Suite < Dist[V])
				{
					Dist[V] = Suite;
					Precedent[V] = T;
					Tas.HeapPush(FEntree(Suite, V), Plus);
				}
			}
		}
		if (!bTrouve)
		{
			return false;
		}
		TArray<int32> Suite;
		for (int32 T = B.Troncon; T != INDEX_NONE; T = Precedent[T])
		{
			Suite.Insert(T, 0);
			if (Suite.Num() > N) { return false; }
		}
		OutPoints.Add(A.Point);
		OutPoints.Add(R.Point(A.Troncon, R.Troncons[A.Troncon].Longueur));
		for (int32 i = 0; i < Suite.Num(); ++i)
		{
			const int32 T = Suite[i];
			OutPoints.Add(R.Point(T, 0.f));
			if (i < Suite.Num() - 1)
			{
				OutPoints.Add(R.Point(T, R.Troncons[T].Longueur));
			}
		}
		OutPoints.Add(B.Point);
		return true;
	}

	/** Arrondit les virages (Chaikin) sans bouger le premier et le dernier point. */
	void Arrondir(TArray<FVector>& Points, int32 Passes)
	{
		for (int32 Passe = 0; Passe < Passes && Points.Num() > 2; ++Passe)
		{
			TArray<FVector> Nouveaux;
			Nouveaux.Add(Points[0]);
			for (int32 i = 0; i < Points.Num() - 1; ++i)
			{
				const FVector& P = Points[i];
				const FVector& Q = Points[i + 1];
				if (i > 0) { Nouveaux.Add(FMath::Lerp(P, Q, 0.25f)); }
				if (i < Points.Num() - 2) { Nouveaux.Add(FMath::Lerp(P, Q, 0.75f)); }
			}
			Nouveaux.Add(Points.Last());
			Points = MoveTemp(Nouveaux);
		}
	}

	void RetirerDoublons(TArray<FVector>& Points)
	{
		TArray<FVector> Propres;
		for (const FVector& P : Points)
		{
			if (Propres.Num() == 0 || FVector::DistSquared2D(Propres.Last(), P) > 25.f * 25.f)
			{
				Propres.Add(P);
			}
		}
		if (Points.Num() > 0 && Propres.Num() > 0)
		{
			Propres.Last() = Points.Last(); // l'arret d'arrivee reste exact
		}
		Points = MoveTemp(Propres);
	}

	/** Points regulierement espaces (Pas en cm), premier et dernier conserves. */
	void Reechantillonner(const TArray<FVector>& Points, float Pas, TArray<FVector>& Out)
	{
		Out.Reset();
		if (Points.Num() == 0) { return; }
		Out.Add(Points[0]);
		float Reste = Pas;
		for (int32 i = 0; i < Points.Num() - 1; ++i)
		{
			const FVector A = Points[i];
			const FVector B = Points[i + 1];
			const float Longueur = FVector::Dist(A, B);
			float Pos = 0.f;
			while (Longueur - Pos >= Reste)
			{
				Pos += Reste;
				Out.Add(FMath::Lerp(A, B, Pos / Longueur));
				Reste = Pas;
			}
			Reste -= (Longueur - Pos);
		}
		if (FVector::DistSquared(Out.Last(), Points.Last()) > 1.f)
		{
			if (Out.Num() > 1 && FVector::Dist(Out.Last(), Points.Last()) < Pas * 0.3f)
			{
				Out.Last() = Points.Last();
			}
			else
			{
				Out.Add(Points.Last());
			}
		}
	}

	float LongueurPolyligne(const TArray<FVector>& Points)
	{
		float L = 0.f;
		for (int32 i = 0; i + 1 < Points.Num(); ++i) { L += FVector::Dist(Points[i], Points[i + 1]); }
		return L;
	}
}

// =====================================================================================================
// Acteur
// =====================================================================================================

const FReglagesBus& ABusChelles::Reglages()
{
	if (!GReglagesLus)
	{
		ChargerReglages();
	}
	return GReglages;
}

ABusChelles::ABusChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 20.f;
	SetReplicatingMovement(false);
	SetHidden(true);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ABusChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABusChelles, bServiceActif);
	DOREPLIFETIME(ABusChelles, Bus);
	DOREPLIFETIME(ABusChelles, Trajet);
	DOREPLIFETIME(ABusChelles, ParcouruServeur);
	DOREPLIFETIME(ABusChelles, VitesseServeur);
	DOREPLIFETIME(ABusChelles, Etat);
	DOREPLIFETIME(ABusChelles, DepartDans);
	DOREPLIFETIME(ABusChelles, Portes);
	DOREPLIFETIME(ABusChelles, NomArretActuel);
}

void ABusChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages();
	if (EstServeur())
	{
		int32 Nombre = 0;
		for (TActorIterator<AArretBusChelles> It(GetWorld()); It; ++It) { ++Nombre; }
		UE_LOG(LogBusChelles, Log, TEXT("[Bus] Pret : %d arret(s) de bus dans la carte. Le bus roulera pendant l'invasion zombie (console : Chelles.Bus pour tester)."), Nombre);
	}
}

void ABusChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Ecran.IsValid())
	{
		if (UWorld* Monde = GetWorld())
		{
			if (UGameViewportClient* Vue = Monde->GetGameViewport())
			{
				Vue->RemoveViewportWidgetContent(Ecran.ToSharedRef());
			}
		}
		Ecran.Reset();
	}
	if (bVoituresRetirees)
	{
		RemettreVoitures();
	}
	if (EstServeur() && IsValid(Bus))
	{
		Bus->Destroy();
	}
	Super::EndPlay(Raison);
}

void ABusChelles::ForcerService(bool bOui)
{
	bForce = bOui;
	ProchaineVerification = 0.f;
	UE_LOG(LogBusChelles, Log, TEXT("[Bus] Test : %s."), bOui ? TEXT("bus en service sans invasion") : TEXT("fin du test"));
}

bool ABusChelles::LireInvasion() const
{
	UClass* Classe = TrouverClasse(TEXT("/Script/ChellesZombies.InvasionZombies"));
	if (!Classe)
	{
		return false;
	}
	FBoolProperty* Propriete = FindFProperty<FBoolProperty>(Classe, TEXT("bInvasionEnCours"));
	if (!Propriete)
	{
		return false;
	}
	for (TActorIterator<AActor> It(GetWorld(), Classe); It; ++It)
	{
		if (Propriete->GetPropertyValue_InContainer(*It))
		{
			return true;
		}
	}
	return false;
}

void ABusChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);

	if (EstServeur())
	{
		const float Maintenant = GetWorld()->GetTimeSeconds();
		if (Maintenant >= ProchaineVerification)
		{
			ProchaineVerification = Maintenant + 0.5f;
			CreerLesRelais();
			const bool bInvasion = Reglages().bActif && (bForce || LireInvasion());
			if (bInvasion && !bServiceActif)
			{
				DemarrerService();
			}
			else if (!bInvasion && bServiceActif && !bFinDemandee)
			{
				if (!IsValid(Bus) || !UnJoueurDansLeBus())
				{
					ArreterService();
				}
				else
				{
					// des joueurs sont dans le bus : il les depose au prochain arret puis disparait
					bFinDemandee = true;
					UE_LOG(LogBusChelles, Log, TEXT("[Bus] Fin de l'invasion : le bus depose ses passagers au prochain arret."));
				}
			}
		}
		if (bServiceActif && IsValid(Bus) && LongueurTrajet > 0.f)
		{
			TickServeur(DeltaSeconds);
		}
	}
	TickLocal(DeltaSeconds);
}

// =====================================================================================================
// Serveur : trajet et conduite
// =====================================================================================================

void ABusChelles::ListerJoueurs(TArray<APawn*>& OutJoueurs) const
{
	OutJoueurs.Reset();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (APawn* Pion = PC->GetPawn())
			{
				OutJoueurs.Add(Pion);
			}
		}
	}
}

bool ABusChelles::UnJoueurDansLeBus() const
{
	TArray<APawn*> Joueurs;
	ListerJoueurs(Joueurs);
	for (APawn* Joueur : Joueurs)
	{
		if (EstDansLeBus(Joueur->GetActorLocation()))
		{
			return true;
		}
	}
	return false;
}

bool ABusChelles::JoueurDevantLeBus() const
{
	if (!IsValid(Bus) || !BoiteLocale.IsValid)
	{
		return false;
	}
	TArray<APawn*> Joueurs;
	ListerJoueurs(Joueurs);
	const FTransform Repere = Bus->GetActorTransform();
	for (APawn* Joueur : Joueurs)
	{
		const FVector L = Repere.InverseTransformPosition(Joueur->GetActorLocation());
		if (L.X > BoiteLocale.Max.X - 20.f && L.X < BoiteLocale.Max.X + 900.f
			&& L.Y > BoiteLocale.Min.Y - 80.f && L.Y < BoiteLocale.Max.Y + 80.f
			&& L.Z > BoiteLocale.Min.Z - 200.f && L.Z < BoiteLocale.Max.Z + 200.f)
		{
			return true;
		}
	}
	return false;
}

bool ABusChelles::ConstruireTrajet(const FVector& PresDe, float& OutDepart, int32& OutPremierArret)
{
	const FReglagesBus& R = Reglages();

	// 1. Les arrets poses dans la carte
	TArray<AArretBusChelles*> Arrets;
	for (TActorIterator<AArretBusChelles> It(GetWorld()); It; ++It)
	{
		if (It->bActif)
		{
			Arrets.Add(*It);
		}
	}
	if (Arrets.Num() == 0)
	{
		UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Aucun arret de bus dans la carte : posez des BP_Arret_Bus (Content/Chelles/Bus) au bord des routes."));
		return false;
	}

	// 2. Le reseau routier de la circulation
	FReseau Reseau;
	const bool bReseau = LireReseau(GetWorld(), Reseau);
	if (!bReseau)
	{
		UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Pas de reseau routier (Circulation Spawner absent ?) : le bus ira en ligne droite d'un arret a l'autre."));
	}

	// 3. Chaque arret est rattache a la voie la plus proche, de preference celle qui passe a cote du trottoir de l'arret
	TArray<FArretSurRoute> SurRoute;
	for (AArretBusChelles* Arret : Arrets)
	{
		FArretSurRoute A;
		A.Nom = Arret->NomArret;
		A.Ordre = Arret->Ordre;
		const FVector Ici = Arret->GetActorLocation();
		if (bReseau)
		{
			float Meilleur = TNumericLimits<float>::Max();
			float MeilleureDistance = 0.f;
			for (int32 T = 0; T < Reseau.Troncons.Num(); ++T)
			{
				const FReseau::FTroncon& Tr = Reseau.Troncons[T];
				if (Tr.Longueur < 1.f) { continue; }
				const FVector P0 = Reseau.Point(T, 0.f);
				const FVector P1 = Reseau.Point(T, Tr.Longueur);
				const FVector Seg = FVector(P1.X - P0.X, P1.Y - P0.Y, 0.f);
				const float L2 = Seg.SizeSquared();
				if (L2 < 1.f) { continue; }
				const float U = FMath::Clamp(FVector::DotProduct(FVector(Ici.X - P0.X, Ici.Y - P0.Y, 0.f), Seg) / L2, 0.f, 1.f);
				const FVector P = FMath::Lerp(P0, P1, U);
				const float D = FVector::Dist2D(Ici, P);
				// L'arret doit etre a droite de la voie (portes du bus du cote du trottoir)
				const bool bAGauche = FVector::DotProduct(FVector(Ici.X - P.X, Ici.Y - P.Y, 0.f), Droite(Tr.Direction)) < 0.f;
				const float Score = D + ((bAGauche && Tr.Inverse != INDEX_NONE) ? 1500.f : 0.f);
				if (Score < Meilleur)
				{
					Meilleur = Score;
					MeilleureDistance = D;
					A.Troncon = T;
					A.S = U * Tr.Longueur;
					A.Point = Reseau.Point(T, A.S);
				}
			}
			if (A.Troncon == INDEX_NONE || MeilleureDistance > R.DistanceMaxArretRouteMetres * 100.f)
			{
				UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Arret '%s' ignore : trop loin d'une route (%.0f m)."), *Arret->NomArret, MeilleureDistance / 100.f);
				continue;
			}
		}
		else
		{
			// sans reseau : le bus s'arrete 3 m devant l'arret (la fleche bleue montre la route)
			A.Point = Ici + Arret->GetActorForwardVector().GetSafeNormal2D() * 300.f;
		}
		A.Nom.ReplaceInline(TEXT("Arret"), TEXT("Arr\u00EAt"), ESearchCase::CaseSensitive);
		SurRoute.Add(A);
	}
	if (SurRoute.Num() == 0)
	{
		return false;
	}
	if (!bReseau && SurRoute.Num() < 2)
	{
		UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Il faut au moins 2 arrets de bus (sans reseau routier)."));
		return false;
	}

	// 4. Ordre de passage : "Ordre" si l'utilisateur l'a rempli, sinon l'arret le plus proche a chaque fois
	TArray<int32> OrdreArrets;
	{
		bool bOrdreManuel = false;
		for (const FArretSurRoute& A : SurRoute) { bOrdreManuel |= A.Ordre > 0; }
		for (int32 i = 0; i < SurRoute.Num(); ++i) { OrdreArrets.Add(i); }
		if (bOrdreManuel)
		{
			OrdreArrets.StableSort([&SurRoute](int32 X, int32 Y)
			{
				const int32 OX = SurRoute[X].Ordre > 0 ? SurRoute[X].Ordre : MAX_int32;
				const int32 OY = SurRoute[Y].Ordre > 0 ? SurRoute[Y].Ordre : MAX_int32;
				return OX < OY;
			});
		}
		else
		{
			TArray<int32> Restants = OrdreArrets;
			OrdreArrets.Reset();
			int32 Actuel = Restants[0];
			Restants.RemoveAt(0);
			OrdreArrets.Add(Actuel);
			while (Restants.Num() > 0)
			{
				int32 Meilleur = 0;
				float MeilleureD = TNumericLimits<float>::Max();
				for (int32 j = 0; j < Restants.Num(); ++j)
				{
					const float D = FVector::DistSquared2D(SurRoute[Actuel].Point, SurRoute[Restants[j]].Point);
					if (D < MeilleureD) { MeilleureD = D; Meilleur = j; }
				}
				Actuel = Restants[Meilleur];
				Restants.RemoveAt(Meilleur);
				OrdreArrets.Add(Actuel);
			}
		}
	}

	// 5. Les morceaux de trajet d'un arret au suivant (boucle)
	const int32 N = OrdreArrets.Num();
	TArray<TArray<FVector>> Morceaux;
	Morceaux.SetNum(N);
	float LongueurTotale = 0.f;
	for (int32 k = 0; k < N; ++k)
	{
		const FArretSurRoute& A = SurRoute[OrdreArrets[k]];
		const FArretSurRoute& B = SurRoute[OrdreArrets[(k + 1) % N]];
		TArray<FVector>& Points = Morceaux[k];
		if (!bReseau || !CheminEntre(Reseau, A, B, N == 1, Points))
		{
			if (bReseau)
			{
				UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Pas de route de '%s' vers '%s' : ligne droite."), *A.Nom, *B.Nom);
			}
			if (N == 1)
			{
				UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Un seul arret et pas de boucle possible : posez au moins 2 arrets."));
				return false;
			}
			Points = { A.Point, B.Point };
		}
		RetirerDoublons(Points);
		Arrondir(Points, 2);
		LongueurTotale += LongueurPolyligne(Points);
	}
	if (LongueurTotale < 500.f)
	{
		UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Trajet trop court (%.0f m) : eloignez les arrets."), LongueurTotale / 100.f);
		return false;
	}
	const float Pas = FMath::Max(250.f, LongueurTotale / (MaxPointsTrajet - N - 1));

	TArray<FVector_NetQuantize> Points;
	TArray<int32> IndexArrets;
	for (int32 k = 0; k < N; ++k)
	{
		TArray<FVector> Regulier;
		Reechantillonner(Morceaux[k], Pas, Regulier);
		IndexArrets.Add(Points.Num());
		for (int32 i = 0; i < Regulier.Num(); ++i)
		{
			if (i == Regulier.Num() - 1 && k < N - 1) { continue; } // le premier point du morceau suivant est le meme
			if (i == Regulier.Num() - 1 && k == N - 1)
			{
				Points.Add(FVector_NetQuantize(Points[0])); // boucle fermee : exactement le point de depart
				continue;
			}
			Points.Add(FVector_NetQuantize(Regulier[i]));
		}
	}
	Trajet = MoveTemp(Points);
	OnRep_Trajet();
	if (LongueurTrajet <= 0.f)
	{
		return false;
	}
	DistancesArrets.Reset();
	NomsArrets.Reset();
	for (int32 k = 0; k < N; ++k)
	{
		DistancesArrets.Add(Cumuls[IndexArrets[k]]);
		NomsArrets.Add(SurRoute[OrdreArrets[k]].Nom);
	}

	// 6. Le bus arrive bientot a l'arret le plus proche du joueur : il apparait 150 m avant (ou a l'arret precedent)
	int32 PlusProche = 0;
	float D2 = TNumericLimits<float>::Max();
	for (int32 k = 0; k < N; ++k)
	{
		const float D = FVector::DistSquared2D(PresDe, Trajet[IndexArrets[k]]);
		if (D < D2) { D2 = D; PlusProche = k; }
	}
	const float Ecart = Boucler(DistancesArrets[PlusProche] - DistancesArrets[(PlusProche + N - 1) % N]);
	const float Recul = (N == 1) ? FMath::Min(15000.f, LongueurTrajet * 0.5f) : FMath::Min(15000.f, Ecart > 1.f ? Ecart : LongueurTrajet);
	OutDepart = Boucler(DistancesArrets[PlusProche] - Recul);
	OutPremierArret = PlusProche;

	FString Liste;
	for (const FString& Nom : NomsArrets) { Liste += (Liste.IsEmpty() ? TEXT("") : TEXT(" > ")) + Nom; }
	UE_LOG(LogBusChelles, Log, TEXT("[Bus] Trajet : %d arret(s), boucle de %.0f m (%s)."), N, LongueurTrajet / 100.f, *Liste);
	return true;
}

void ABusChelles::DemarrerService()
{
	bServiceActif = true;
	bFinDemandee = false;
	OnRep_Service();

	FVector PresDe = FVector::ZeroVector;
	TArray<APawn*> Joueurs;
	ListerJoueurs(Joueurs);
	if (Joueurs.Num() > 0) { PresDe = Joueurs[0]->GetActorLocation(); }

	float Depart = 0.f;
	int32 Premier = 0;
	if (!ConstruireTrajet(PresDe, Depart, Premier))
	{
		UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Invasion : route videe, mais pas de bus (voir les messages ci-dessus)."));
		return;
	}
	UClass* Classe = LoadClass<APawn>(nullptr, *Reglages().ClasseBus);
	if (!Classe)
	{
		UE_LOG(LogBusChelles, Error, TEXT("[Bus] Bus introuvable : %s (verifiez ClasseBus= dans Bus.txt)."), *Reglages().ClasseBus);
		return;
	}
	const FVector Av = PointA(Depart + 300.f);
	const FVector Ar = PointA(Depart - 300.f);
	const FRotator Rotation(0.f, (Av - Ar).Rotation().Yaw + Reglages().CorrectionOrientation, 0.f);
	const FTransform Transformation(Rotation, PointA(Depart) + FVector(0.f, 0.f, 150.f));
	APawn* Nouveau = GetWorld()->SpawnActorDeferred<APawn>(Classe, Transformation, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Nouveau)
	{
		UE_LOG(LogBusChelles, Error, TEXT("[Bus] Impossible de faire apparaitre le bus."));
		return;
	}
	Nouveau->AutoPossessAI = EAutoPossessAI::Disabled;
	Nouveau->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	UGameplayStatics::FinishSpawningActor(Nouveau, Transformation);
	Nouveau->SetReplicates(true);
	Nouveau->SetReplicateMovement(false); // chaque machine place le bus elle-meme sur le trajet (plus fluide)
	Nouveau->bAlwaysRelevant = true;

	Bus = Nouveau;
	Parcouru = Depart;
	ParcouruServeur = Depart;
	ParcouruLocal = Depart;
	Vitesse = 0.f;
	Etat = 0;
	Portes = 0;
	ProchainArret = Premier;
	NomArretActuel.Reset();
	bBusPrepare = false;
	bZInitialise = false;
	OnRep_Bus();
	UE_LOG(LogBusChelles, Log, TEXT("[Bus] Le bus est en service. Prochain arret : %s."), *NomsArrets[Premier]);
}

void ABusChelles::ArreterService()
{
	bServiceActif = false;
	bFinDemandee = false;
	Etat = 0;
	Portes = 0;
	DepartDans = 0.f;
	NomArretActuel.Reset();
	if (IsValid(Bus))
	{
		Bus->Destroy();
	}
	Bus = nullptr;
	Trajet.Reset();
	OnRep_Trajet();
	DistancesArrets.Reset();
	NomsArrets.Reset();
	PortesBus.Reset();
	bBusPrepare = false;
	OnRep_Service();
	UE_LOG(LogBusChelles, Log, TEXT("[Bus] Fin du service : le bus est retire, les voitures reviennent."));
}

void ABusChelles::TickServeur(float DeltaSeconds)
{
	const int32 N = DistancesArrets.Num();
	if (N == 0)
	{
		return;
	}
	const FReglagesBus& R = Reglages();
	const float Maintenant = GetWorld()->GetTimeSeconds();
	auto Partir = [this, N]()
	{
		ProchainArret = (ProchainArret + 1) % N;
		Parcouru = Boucler(Parcouru + 1.f); // (un seul arret : sinon il serait deja "arrive")
		Etat = 0;
		DepartDans = 0.f;
		NomArretActuel.Reset();
	};

	switch (Etat)
	{
	case 0: // roule vers le prochain arret
	{
		// la porte avant s'arrete devant l'arret (pas le milieu du bus)
		const float AvancePorte = PortesBus.Num() > 0 ? FMath::Clamp(PortesBus[0].CentreLocal.X, -800.f, 800.f) : 0.f;
		const float Cible = Boucler(DistancesArrets[ProchainArret] - AvancePorte);
		const float Reste = Boucler(Cible - Parcouru);

		float VMax = R.VitesseKmh * KmhVersCms;
		// on ralentit avant les virages
		const FVector D0 = (PointA(Parcouru + 300.f) - PointA(Parcouru - 300.f)).GetSafeNormal2D();
		const FVector D1 = (PointA(Parcouru + 1800.f) - PointA(Parcouru + 1000.f)).GetSafeNormal2D();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(D0, D1), -1.f, 1.f)));
		if (Angle > 55.f) { VMax = FMath::Min(VMax, 300.f); }
		else if (Angle > 20.f) { VMax = FMath::Min(VMax, 480.f); }

		float VCible = FMath::Min(VMax, FMath::Sqrt(2.f * Freinage * FMath::Max(0.f, Reste - 20.f)) + 40.f);
		float Frein = Freinage;
		if (JoueurDevantLeBus())
		{
			VCible = 0.f; // on ne roule pas sur un joueur
			Frein = FreinageUrgence;
		}
		Vitesse = (Vitesse < VCible) ? FMath::Min(VCible, Vitesse + Acceleration * DeltaSeconds)
			: FMath::Max(VCible, Vitesse - Frein * DeltaSeconds);
		const float Avance = FMath::Min(Vitesse * DeltaSeconds, Reste);
		Parcouru = Boucler(Parcouru + Avance);
		if (Reste - Avance < 5.f)
		{
			Parcouru = Cible;
			Vitesse = 0.f;
			TempsALArret = 0.f;
			NomArretActuel = NomsArrets[ProchainArret];
			if (bFinDemandee)
			{
				Etat = 3;
				Portes = (uint8)((1 << FMath::Min(PortesBus.Num(), 8)) - 1);
				OnRep_Portes();
			}
			else
			{
				Etat = 1; // portes fermees : les joueurs les ouvrent eux-memes
			}
		}
		break;
	}
	case 1: // a l'arret
	{
		if (bFinDemandee)
		{
			Etat = 3;
			TempsALArret = 0.f;
			Portes = (uint8)((1 << FMath::Min(PortesBus.Num(), 8)) - 1);
			OnRep_Portes();
			break;
		}
		TempsALArret += DeltaSeconds;
		const float Attente = UnJoueurDansLeBus() ? R.AttenteDedansSecondes : R.AttenteDehorsSecondes;
		DepartDans = FMath::Max(0.f, Attente - TempsALArret);
		if (TempsALArret >= Attente)
		{
			if (Portes != 0)
			{
				Portes = 0; // le chauffeur ferme les portes avant de partir
				OnRep_Portes();
				Etat = 2;
				FinFermeture = Maintenant + 1.5f;
				DepartDans = 0.f;
			}
			else
			{
				Partir();
			}
		}
		break;
	}
	case 2: // fermeture des portes
		if (Maintenant >= FinFermeture)
		{
			Partir();
		}
		break;
	case 3: // terminus (fin de l'invasion) : tout le monde descend
		TempsALArret += DeltaSeconds;
		DepartDans = 0.f;
		if ((TempsALArret > 4.f && !UnJoueurDansLeBus()) || TempsALArret > 60.f)
		{
			ArreterService();
			return;
		}
		break;
	default:
		Etat = 0;
		break;
	}
	ParcouruServeur = Parcouru;
	VitesseServeur = (Etat == 0) ? Vitesse : 0.f;
}

void ABusChelles::BasculerPorte(int32 Index, APawn* Joueur)
{
	if (!EstServeur() || Etat != 1 || !IsValid(Joueur) || !PortesBus.IsValidIndex(Index) || Index >= 8)
	{
		return;
	}
	if (PorteLaPlusProche(Joueur, Reglages().DistancePorteMetres * 100.f + 150.f) != Index)
	{
		return; // trop loin de cette porte
	}
	Portes ^= (uint8)(1 << Index);
	OnRep_Portes();
}

void ABusChelles::CreerLesRelais()
{
	const ENetMode Mode = GetNetMode();
	if (Mode != NM_ListenServer && Mode != NM_DedicatedServer)
	{
		return;
	}
	TSet<AActor*> DejaServis;
	for (TActorIterator<ABusRelaisChelles> It(GetWorld()); It; ++It)
	{
		DejaServis.Add(It->GetOwner());
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->IsLocalController() && !DejaServis.Contains(PC))
		{
			FActorSpawnParameters Params;
			Params.Owner = PC;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<ABusRelaisChelles>(ABusRelaisChelles::StaticClass(), FTransform::Identity, Params);
		}
	}
}

// =====================================================================================================
// Chaque machine : route vide, bus, portes, ecran
// =====================================================================================================

void ABusChelles::OnRep_Service()
{
	if (bServiceActif && Reglages().bMessageDepart)
	{
		Message = TEXT("INVASION : PLUS DE VOITURES, UN BUS DE SECOURS CIRCULE !\nRejoignez un arr\u00EAt de bus.");
		FinMessage = GetWorld()->GetTimeSeconds() + 8.f;
	}
	if (!bServiceActif)
	{
		FinMessage = 0.f;
		PortesBus.Reset();
		bBusPrepare = false;
	}
	ProchainRetrait = 0.f;
}

void ABusChelles::OnRep_Bus()
{
	bBusPrepare = false;
	bZInitialise = false;
	PortesBus.Reset();
	NettoyagesRestants = 12;
	ProchainNettoyageWidgets = 0.f;
}

void ABusChelles::OnRep_Trajet()
{
	Cumuls.Reset();
	LongueurTrajet = 0.f;
	if (Trajet.Num() < 2)
	{
		return;
	}
	Cumuls.Add(0.f);
	for (int32 i = 1; i < Trajet.Num(); ++i)
	{
		Cumuls.Add(Cumuls.Last() + FMath::Max(1.f, FVector::Dist(Trajet[i - 1], Trajet[i])));
	}
	LongueurTrajet = Cumuls.Last();
	if (!EstServeur())
	{
		ParcouruLocal = ParcouruServeur;
		bZInitialise = false;
	}
}

void ABusChelles::OnRep_Portes()
{
	AppliquerPortes(false);
}

float ABusChelles::Boucler(float S) const
{
	if (LongueurTrajet <= 0.f)
	{
		return 0.f;
	}
	S = FMath::Fmod(S, LongueurTrajet);
	return S < 0.f ? S + LongueurTrajet : S;
}

FVector ABusChelles::PointA(float S) const
{
	if (Trajet.Num() == 0)
	{
		return FVector::ZeroVector;
	}
	if (Trajet.Num() == 1 || LongueurTrajet <= 0.f)
	{
		return Trajet[0];
	}
	S = Boucler(S);
	const int32 i = FMath::Clamp(Algo::UpperBound(Cumuls, S) - 1, 0, Trajet.Num() - 2);
	const float Longueur = Cumuls[i + 1] - Cumuls[i];
	const float Alpha = Longueur > 0.f ? FMath::Clamp((S - Cumuls[i]) / Longueur, 0.f, 1.f) : 0.f;
	return FMath::Lerp((FVector)Trajet[i], (FVector)Trajet[i + 1], Alpha);
}

bool ABusChelles::EstDansLeBus(const FVector& Position) const
{
	if (!IsValid(Bus) || !BoiteLocale.IsValid)
	{
		return false;
	}
	const FVector L = Bus->GetActorTransform().InverseTransformPosition(Position);
	return L.X > BoiteLocale.Min.X + 10.f && L.X < BoiteLocale.Max.X - 10.f
		&& L.Y > BoiteLocale.Min.Y + 5.f && L.Y < BoiteLocale.Max.Y - 5.f
		&& L.Z > PlancherLocal - 30.f && L.Z < BoiteLocale.Max.Z;
}

int32 ABusChelles::PorteLaPlusProche(const APawn* Joueur, float DistanceMax) const
{
	if (!IsValid(Bus) || !Joueur)
	{
		return INDEX_NONE;
	}
	const FTransform Repere = Bus->GetActorTransform();
	const FVector Ici = Joueur->GetActorLocation();
	int32 Meilleure = INDEX_NONE;
	float MeilleureD = DistanceMax;
	for (int32 i = 0; i < PortesBus.Num(); ++i)
	{
		const FVector Centre = Repere.TransformPosition(PortesBus[i].CentreLocal);
		const float D = FVector::Dist2D(Centre, Ici);
		if (D < MeilleureD && FMath::Abs(Centre.Z - Ici.Z) < 300.f)
		{
			MeilleureD = D;
			Meilleure = i;
		}
	}
	return Meilleure;
}

void ABusChelles::PreparerBus()
{
	if (!IsValid(Bus))
	{
		return;
	}
	const FReglagesBus& R = Reglages();

	// Plus de physique de vehicule : le bus est pose sur le trajet a chaque image
	USkeletalMeshComponent* Corps = Cast<USkeletalMeshComponent>(Bus->GetRootComponent());
	if (UPrimitiveComponent* Racine = Cast<UPrimitiveComponent>(Bus->GetRootComponent()))
	{
		Racine->SetSimulatePhysics(false);
		Racine->SetEnableGravity(false);
		// le corps du bus ne bloque plus les personnages : des murs invisibles le remplacent (avec l'ouverture des portes)
		Racine->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Racine->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	TInlineComponentArray<UMovementComponent*> Mouvements(Bus);
	for (UMovementComponent* Mouvement : Mouvements)
	{
		Mouvement->Deactivate();
		Mouvement->SetComponentTickEnabled(false);
	}

	// Taille du bus (repere du bus)
	BoiteLocale = FBox(ForceInit);
	if (Corps && Corps->SkeletalMesh)
	{
		BoiteLocale = Corps->SkeletalMesh->GetImportedBounds().GetBox();
	}
	if (!BoiteLocale.IsValid || BoiteLocale.GetSize().X < 300.f || BoiteLocale.GetSize().Y < 100.f)
	{
		BoiteLocale = FBox(FVector(-600.f, -130.f, 0.f), FVector(600.f, 130.f, 320.f));
		UE_LOG(LogBusChelles, Warning, TEXT("[Bus] Taille du bus inconnue : boite de 12 m utilisee."));
	}
	PlancherLocal = BoiteLocale.Min.Z + R.HauteurPlancher;

	// Les portes : composants Skeletal Mesh dont le modele contient "door"
	PortesBus.Reset();
	TInlineComponentArray<USkeletalMeshComponent*> Squelettes(Bus);
	const FTransform RepereBus = Bus->GetActorTransform();
	for (USkeletalMeshComponent* C : Squelettes)
	{
		if (C == Corps || !C->SkeletalMesh || !C->SkeletalMesh->GetName().Contains(TEXT("door")))
		{
			continue;
		}
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FPorteBus Porte;
		Porte.Composant = C;
		const FBox Boite = C->SkeletalMesh->GetImportedBounds().GetBox().TransformBy(C->GetComponentTransform().GetRelativeTransform(RepereBus));
		Porte.CentreLocal = Boite.GetCenter();
		Porte.DemiTailleLocale = Boite.GetExtent();
		// animation d'ouverture du pack : <modele>_Anim
		const FString Paquet = C->SkeletalMesh->GetOutermost()->GetName() + TEXT("_Anim");
		const FString Nom = C->SkeletalMesh->GetName() + TEXT("_Anim");
		UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *(Paquet + TEXT(".") + Nom), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Anim && Anim->GetSkeleton() == C->SkeletalMesh->GetSkeleton())
		{
			Porte.Animation = Anim;
		}
		UE_LOG(LogBusChelles, Log, TEXT("[Bus] Porte : %s (animation %s)."), *C->SkeletalMesh->GetName(), Porte.Animation.IsValid() ? *Nom : TEXT("aucune : la porte disparait quand elle s'ouvre"));
		PortesBus.Add(Porte);
	}
	PortesBus.Sort([](const FPorteBus& A, const FPorteBus& B) { return A.CentreLocal.X > B.CentreLocal.X; }); // avant d'abord

	// Murs invisibles : plancher, toit, avant, arriere, cotes ; ouvertures aux portes
	const FVector Min = BoiteLocale.Min;
	const FVector Max = BoiteLocale.Max;
	const float Ep = 8.f;
	const float Bas = Min.Z + 15.f;
	const float Haut = Max.Z;
	const float CX = (Min.X + Max.X) * 0.5f;
	const float CY = (Min.Y + Max.Y) * 0.5f;
	const float EX = (Max.X - Min.X) * 0.5f;
	const float EY = (Max.Y - Min.Y) * 0.5f;
	float SommeY = 0.f;
	for (const FPorteBus& P : PortesBus) { SommeY += P.CentreLocal.Y - CY; }
	const bool bPortesADroite = PortesBus.Num() == 0 || SommeY >= 0.f;
	const float YPortes = bPortesADroite ? Max.Y - Ep : Min.Y + Ep;
	const float YAutre = bPortesADroite ? Min.Y + Ep : Max.Y - Ep;

	int32 Numero = 0;
	auto CreerMur = [this, &Numero](const FVector& Centre, const FVector& DemiTaille) -> UBoxComponent*
	{
		UBoxComponent* Boite = NewObject<UBoxComponent>(Bus, *FString::Printf(TEXT("MurBusChelles_%d"), Numero++));
		Boite->SetupAttachment(Bus->GetRootComponent());
		Boite->SetBoxExtent(DemiTaille.ComponentMax(FVector(2.f)), false);
		Boite->SetRelativeLocation(Centre);
		Boite->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Boite->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Boite->SetHiddenInGame(true);
		Boite->SetCanEverAffectNavigation(false);
		Boite->RegisterComponent();
		return Boite;
	};
	CreerMur(FVector(CX, CY, (Bas + PlancherLocal) * 0.5f), FVector(EX - 2.f, EY - 2.f, FMath::Max(5.f, (PlancherLocal - Bas) * 0.5f)));
	CreerMur(FVector(CX, CY, Haut - Ep), FVector(EX, EY, Ep));
	CreerMur(FVector(Max.X - Ep, CY, (Bas + Haut) * 0.5f), FVector(Ep, EY, (Haut - Bas) * 0.5f));
	CreerMur(FVector(Min.X + Ep, CY, (Bas + Haut) * 0.5f), FVector(Ep, EY, (Haut - Bas) * 0.5f));
	CreerMur(FVector(CX, YAutre, (Bas + Haut) * 0.5f), FVector(EX, Ep, (Haut - Bas) * 0.5f));

	// cote des portes : des morceaux de mur entre les ouvertures, et un "bouchon" dans chaque ouverture (porte fermee)
	TArray<FVector2D> Ouvertures;
	for (FPorteBus& P : PortesBus)
	{
		const float Demi = FMath::Max(P.DemiTailleLocale.X, 45.f);
		Ouvertures.Add(FVector2D(P.CentreLocal.X - Demi, P.CentreLocal.X + Demi));
		P.Bloqueur = CreerMur(FVector(P.CentreLocal.X, YPortes, (Bas + Haut) * 0.5f), FVector(Demi, Ep, (Haut - Bas) * 0.5f));
		// la porte est cote trottoir : le joueur s'en approche par l'exterieur ou par l'interieur
		P.CentreLocal.Y = YPortes;
		P.CentreLocal.Z = PlancherLocal + 90.f;
	}
	Ouvertures.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
	float X = Min.X;
	for (const FVector2D& O : Ouvertures)
	{
		if (O.X > X + 5.f)
		{
			CreerMur(FVector((X + O.X) * 0.5f, YPortes, (Bas + Haut) * 0.5f), FVector((O.X - X) * 0.5f, Ep, (Haut - Bas) * 0.5f));
		}
		X = FMath::Max(X, O.Y);
	}
	if (Max.X > X + 5.f)
	{
		CreerMur(FVector((X + Max.X) * 0.5f, YPortes, (Bas + Haut) * 0.5f), FVector((Max.X - X) * 0.5f, Ep, (Haut - Bas) * 0.5f));
	}

	for (FPorteBus& P : PortesBus) { P.bOuverteAffichee = false; }
	AppliquerPortes(true);
	bBusPrepare = true;
	bZInitialise = false;
	PlacerBus(EstServeur() ? Parcouru : ParcouruLocal, 0.f, true);
	UE_LOG(LogBusChelles, Log, TEXT("[Bus] Bus pret : %.1f m de long, %d porte(s) cote %s."), BoiteLocale.GetSize().X / 100.f, PortesBus.Num(), bPortesADroite ? TEXT("droit") : TEXT("gauche"));
}

void ABusChelles::AppliquerPortes(bool bInstantane)
{
	for (int32 i = 0; i < PortesBus.Num(); ++i)
	{
		FPorteBus& P = PortesBus[i];
		const bool bOuverte = i < 8 && (Portes & (1 << i)) != 0;
		if (bOuverte == P.bOuverteAffichee && !bInstantane)
		{
			continue;
		}
		P.bOuverteAffichee = bOuverte;
		if (UBoxComponent* Bloqueur = P.Bloqueur.Get())
		{
			Bloqueur->SetCollisionEnabled(bOuverte ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		}
		USkeletalMeshComponent* C = P.Composant.Get();
		if (!C)
		{
			continue;
		}
		if (UAnimSequence* Anim = P.Animation.Get())
		{
			const float Duree = Anim->GetPlayLength();
			C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			C->PlayAnimation(Anim, false);
			if (bInstantane)
			{
				C->SetPosition(bOuverte ? Duree : 0.f, false);
				C->Stop();
			}
			else if (bOuverte)
			{
				C->SetPosition(0.f, false);
				C->SetPlayRate(1.f);
			}
			else
			{
				C->SetPosition(Duree, false);
				C->SetPlayRate(-1.f); // l'ouverture jouee a l'envers = fermeture
			}
		}
		else
		{
			C->SetVisibility(!bOuverte, true);
		}
	}
}

void ABusChelles::PlacerBus(float S, float DeltaSeconds, bool bTeleport)
{
	if (!IsValid(Bus) || LongueurTrajet <= 0.f)
	{
		return;
	}
	const float Demi = FMath::Clamp(BoiteLocale.GetExtent().X * 0.6f, 150.f, 600.f);
	const FVector Centre = PointA(S);
	const FVector Avant = PointA(S + Demi);
	const FVector Arriere = PointA(S - Demi);
	FVector Direction = Avant - Arriere;
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		Direction = Bus->GetActorRotation().Vector();
	}
	const float Lacet = Direction.Rotation().Yaw + Reglages().CorrectionOrientation;

	// Hauteur du sol sous l'avant et l'arriere (routes, trottoirs, Landscape = decor statique)
	auto Sol = [this, &Centre](const FVector& P, float& OutZ) -> bool
	{
		const float Depuis = bZInitialise ? ZLisse + 400.f : Centre.Z + 2500.f;
		const float Jusqua = bZInitialise ? ZLisse - 1500.f : Centre.Z - 4000.f;
		FHitResult Impact;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SolBusChelles), false, Bus);
		Params.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByObjectType(Impact, FVector(P.X, P.Y, Depuis), FVector(P.X, P.Y, Jusqua),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			OutZ = Impact.ImpactPoint.Z;
			return true;
		}
		return false;
	};
	float ZAvant = 0.f, ZArriere = 0.f;
	const bool bAv = Sol(Avant, ZAvant);
	const bool bAr = Sol(Arriere, ZArriere);
	float Tangage = 0.f;
	float ZSol;
	if (bAv && bAr)
	{
		ZSol = (ZAvant + ZArriere) * 0.5f;
		Tangage = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ZAvant - ZArriere, FVector::Dist2D(Avant, Arriere))), -12.f, 12.f);
	}
	else if (bAv || bAr)
	{
		ZSol = bAv ? ZAvant : ZArriere;
	}
	else
	{
		ZSol = bZInitialise ? ZLisse : Centre.Z;
	}
	if (!bZInitialise || bTeleport || DeltaSeconds <= 0.f)
	{
		ZLisse = ZSol;
		bZInitialise = true;
	}
	else
	{
		ZLisse = FMath::FInterpTo(ZLisse, ZSol, DeltaSeconds, 8.f);
	}
	const FVector Position(Centre.X, Centre.Y, ZLisse - BoiteLocale.Min.Z);
	Bus->SetActorLocationAndRotation(Position, FRotator(Tangage, Lacet, 0.f), false, nullptr,
		bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
}

void ABusChelles::RetirerVoitures()
{
	UClass* ClasseVoiture = TrouverClasse(TEXT("/Script/ChellesCirculation.VehiculeCirculation"));
	UClass* ClasseCirculation = TrouverClasse(TEXT("/Script/ChellesCirculation.CirculationSpawner"));
	if (ClasseCirculation)
	{
		for (TActorIterator<AActor> It(GetWorld(), ClasseCirculation); It; ++It)
		{
			if (It->IsActorTickEnabled())
			{
				It->SetActorTickEnabled(false); // les voitures ne roulent plus et n'apparaissent plus
				CirculationsArretees.Add(*It);
			}
		}
	}
	int32 Cachees = 0;
	if (ClasseVoiture)
	{
		for (TActorIterator<AActor> It(GetWorld(), ClasseVoiture); It; ++It)
		{
			if (!It->IsHidden())
			{
				It->SetActorHiddenInGame(true);
				It->SetActorEnableCollision(false);
				VoituresCachees.Add(*It);
				++Cachees;
			}
		}
	}
	if (!bVoituresRetirees)
	{
		UE_LOG(LogBusChelles, Log, TEXT("[Bus] Route videe : %d voiture(s) retiree(s) pendant l'invasion."), Cachees);
	}
	bVoituresRetirees = true;
}

void ABusChelles::RemettreVoitures()
{
	for (const TWeakObjectPtr<AActor>& Circulation : CirculationsArretees)
	{
		if (AActor* A = Circulation.Get()) { A->SetActorTickEnabled(true); }
	}
	for (const TWeakObjectPtr<AActor>& Voiture : VoituresCachees)
	{
		if (AActor* A = Voiture.Get())
		{
			A->SetActorHiddenInGame(false);
			A->SetActorEnableCollision(true);
		}
	}
	UE_LOG(LogBusChelles, Log, TEXT("[Bus] Les voitures reviennent (%d)."), VoituresCachees.Num());
	CirculationsArretees.Reset();
	VoituresCachees.Reset();
	bVoituresRetirees = false;
}

void ABusChelles::CacherCompteurDuBus()
{
	// Le Blueprint du bus affiche son compteur de vitesse a l'ecran : inutile ici (personne ne conduit)
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (Widget && Widget->GetWorld() == GetWorld() && Widget->IsInViewport()
			&& Widget->GetClass()->GetPathName().Contains(TEXT("/Bus_sity/")))
		{
			Widget->RemoveFromParent();
		}
	}
}

void ABusChelles::TickLocal(float DeltaSeconds)
{
	UWorld* Monde = GetWorld();
	const float Maintenant = Monde->GetTimeSeconds();
	const FReglagesBus& R = Reglages();

	// Route vide pendant l'invasion
	if (bServiceActif && R.bRetirerVoitures)
	{
		if (Maintenant >= ProchainRetrait)
		{
			ProchainRetrait = Maintenant + 1.f;
			RetirerVoitures();
		}
	}
	else if (bVoituresRetirees)
	{
		RemettreVoitures();
	}

	// Le bus suit son trajet
	if (IsValid(Bus) && LongueurTrajet > 0.f)
	{
		if (!bBusPrepare)
		{
			PreparerBus();
		}
		if (EstServeur())
		{
			ParcouruLocal = Parcouru;
		}
		else
		{
			ParcouruLocal = Boucler(ParcouruLocal + VitesseServeur * DeltaSeconds);
			float Ecart = Boucler(ParcouruServeur - ParcouruLocal);
			if (Ecart > LongueurTrajet * 0.5f) { Ecart -= LongueurTrajet; }
			ParcouruLocal = FMath::Abs(Ecart) > 2000.f ? ParcouruServeur : Boucler(ParcouruLocal + Ecart * FMath::Min(1.f, 3.f * DeltaSeconds));
		}
		PlacerBus(ParcouruLocal, DeltaSeconds, false);

		if (NettoyagesRestants > 0 && Maintenant >= ProchainNettoyageWidgets)
		{
			--NettoyagesRestants;
			ProchainNettoyageWidgets = Maintenant + 0.5f;
			CacherCompteurDuBus();
		}
	}

	// Le joueur de cette machine : portes (touche E) et texte a l'ecran
	TexteAide.Reset();
	APlayerController* PC = Monde->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	if (!Ecran.IsValid())
	{
		CreerEcran();
	}
	APawn* Joueur = PC->GetPawn();
	if (!Joueur || !IsValid(Bus) || !bBusPrepare)
	{
		return;
	}
	const bool bDedans = EstDansLeBus(Joueur->GetActorLocation());
	if (Etat == 1)
	{
		const int32 Porte = PorteLaPlusProche(Joueur, R.DistancePorteMetres * 100.f);
		if (Porte != INDEX_NONE && Porte < 8)
		{
			const bool bOuverte = (Portes & (1 << Porte)) != 0;
			const TCHAR* Laquelle = (Porte == 0) ? TEXT("avant") : (Porte == PortesBus.Num() - 1 ? TEXT("arri\u00E8re") : TEXT("du milieu"));
			TexteAide = FString::Printf(TEXT("[%s]  %s la porte %s"), *R.TouchePorte, bOuverte ? TEXT("Fermer") : TEXT("Ouvrir"), Laquelle);
			if (PC->WasInputKeyJustPressed(FKey(*R.TouchePorte)))
			{
				if (EstServeur())
				{
					BasculerPorte(Porte, Joueur);
				}
				else
				{
					for (TActorIterator<ABusRelaisChelles> It(Monde); It; ++It)
					{
						if (It->GetOwner() == PC)
						{
							It->ServeurBasculerPorte(Porte);
							break;
						}
					}
				}
			}
		}
		if (bDedans || FVector::Dist(Joueur->GetActorLocation(), Bus->GetActorLocation()) < 3000.f)
		{
			const FString Depart = FString::Printf(TEXT("BUS  -  %s  -  d\u00E9part dans %d s"), *NomArretActuel, FMath::CeilToInt(DepartDans));
			TexteAide = TexteAide.IsEmpty() ? Depart : Depart + TEXT("\n") + TexteAide;
		}
	}
	else if (Etat == 2 && (bDedans || FVector::Dist(Joueur->GetActorLocation(), Bus->GetActorLocation()) < 3000.f))
	{
		TexteAide = TEXT("BUS  -  fermeture des portes, d\u00E9part !");
	}
	else if (Etat == 3 && bDedans)
	{
		TexteAide = TEXT("BUS  -  Terminus : fin de l'invasion, tout le monde descend !");
	}
}

FText ABusChelles::TexteEcran() const
{
	FString Texte;
	if (GetWorld() && GetWorld()->GetTimeSeconds() < FinMessage)
	{
		Texte = Message;
	}
	if (!TexteAide.IsEmpty())
	{
		Texte = Texte.IsEmpty() ? TexteAide : Texte + TEXT("\n\n") + TexteAide;
	}
	return FText::FromString(Texte);
}

void ABusChelles::CreerEcran()
{
	UGameViewportClient* Vue = GetWorld()->GetGameViewport();
	if (!Vue)
	{
		return;
	}
	TWeakObjectPtr<ABusChelles> Moi(this);
	Ecran = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 150.f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(STextBlock)
			.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteEcran() : FText::GetEmpty(); })
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
			.ColorAndOpacity(FLinearColor(1.f, 0.82f, 0.15f))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			.Justification(ETextJustify::Center)
		];
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 40);
}

// =====================================================================================================
// Relais (multijoueur)
// =====================================================================================================

ABusRelaisChelles::ABusRelaisChelles()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	bAlwaysRelevant = false;
	NetUpdateFrequency = 1.f;
	SetHidden(true);
}

bool ABusRelaisChelles::ServeurBasculerPorte_Validate(int32 Index)
{
	return Index >= 0 && Index < 8;
}

void ABusRelaisChelles::ServeurBasculerPorte_Implementation(int32 Index)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	for (TActorIterator<ABusChelles> It(GetWorld()); It; ++It)
	{
		It->BasculerPorte(Index, PC->GetPawn());
		return;
	}
}
