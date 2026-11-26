#include "CirculationSpawner.h"
#include "Algo/BinarySearch.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "ReferenceSkeleton.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCirculation, Log, All);

namespace
{
	constexpr float KmhVersCms = 100000.f / 3600.f;
	/** Rayon maximal des virages arrondis (cm). */
	constexpr float RayonVirage = 700.f;
	/** Acceleration et freinages (cm/s^2). */
	constexpr float Acceleration = 250.f;
	constexpr float FreinageConfort = 450.f;
	constexpr float FreinageMax = 1100.f;
	/** Distance gardee avec la voiture de devant, a l'arret (pare-choc a pare-choc, cm). */
	constexpr float DistanceArret = 250.f;
	/** Temps de securite avec la voiture de devant (secondes). */
	constexpr float TempsInterVehicules = 0.9f;
	/** Voitures creees par image au lancement (evite un a-coup). */
	constexpr int32 VoituresParLot = 5;
	/** Apres ce temps d'attente devant un carrefour bloque, la voiture passe quand meme (anti-blocage). */
	constexpr float DelaiForcerCarrefour = 6.f;
	/** Distance minimale entre deux voitures qui apparaissent (cm). */
	constexpr float EspacementApparition = 1200.f;
	/** Composant assez grand pour etre une route / un sol (et pas un arbre, un lampadaire, une voiture...). */
	constexpr float TailleMinimaleSol = 1500.f;
	/** Nombre maximal de troncons prevus a l'avance dans l'itineraire. */
	constexpr int32 MaxTronconsPrevus = 16;
	/** Devant un rond-point on attend plus longtemps avant de forcer (et seulement si la voie est vraiment libre). */
	constexpr float DelaiForcerRondPoint = 12.f;
	/** Deux ou trois voitures qui s'attendent mutuellement dans un carrefour ou un rond-point (interblocage) :
	 *  apres ce temps a l'arret, la voiture passe en ignorant celles qui coupent sa route (pas celle de devant). */
	constexpr float DelaiDebloquerCroisement = 3.f;
	/** Duree de ce passage force (secondes). */
	constexpr float DureePassageForce = 3.f;
	/** Bouchon hors de la vue du joueur : la voiture est deplacee ailleurs apres ce temps a l'arret. */
	constexpr float DelaiRecyclerBouchon = 15.f;

	float LargeurRue(const FString& Type)
	{
		// Memes largeurs que Tools/build_roads.py (en cm).
		if (Type == TEXT("motorway") || Type == TEXT("trunk")) { return 1100.f; }
		if (Type == TEXT("primary")) { return 1000.f; }
		if (Type == TEXT("secondary")) { return 900.f; }
		if (Type == TEXT("tertiary")) { return 800.f; }
		if (Type == TEXT("residential")) { return 600.f; }
		if (Type == TEXT("unclassified")) { return 550.f; }
		if (Type == TEXT("living_street")) { return 500.f; }
		if (Type == TEXT("service")) { return 400.f; }
		return 600.f;
	}

	float VitesseParDefautKmh(const FString& Type)
	{
		if (Type == TEXT("motorway")) { return 110.f; }
		if (Type == TEXT("trunk")) { return 90.f; }
		if (Type == TEXT("primary") || Type == TEXT("secondary") || Type == TEXT("tertiary")) { return 50.f; }
		if (Type == TEXT("unclassified")) { return 40.f; }
		if (Type == TEXT("residential")) { return 30.f; }
		if (Type == TEXT("living_street")) { return 20.f; }
		if (Type == TEXT("service")) { return 20.f; }
		return 30.f;
	}

	/** Lit "maxspeed" (ex. "50", "30 km/h", "FR:urban") ; 0 si illisible. */
	float LireVitesseKmh(const FString& Valeur)
	{
		if (Valeur.Contains(TEXT("urban"))) { return 50.f; }
		if (Valeur.Contains(TEXT("zone30"))) { return 30.f; }
		if (Valeur.Contains(TEXT("walk"))) { return 10.f; }
		const float Kmh = FCString::Atof(*Valeur);
		return Valeur.Contains(TEXT("mph")) ? Kmh * 1.609f : Kmh;
	}

	FString ChampTexte(const TSharedPtr<FJsonObject>& Objet, const TCHAR* Nom)
	{
		FString Valeur;
		if (Objet.IsValid())
		{
			Objet->TryGetStringField(Nom, Valeur);
		}
		return Valeur;
	}

	/** Decoupe "SM_SportsCar_01" en mots minuscules : sm, sports, car. */
	TArray<FString> DecouperEnMots(const FString& Nom)
	{
		TArray<FString> Mots;
		FString Courant;
		for (int32 i = 0; i < Nom.Len(); ++i)
		{
			const TCHAR C = Nom[i];
			if (!FChar::IsAlpha(C))
			{
				if (Courant.Len() > 0) { Mots.Add(Courant.ToLower()); }
				Courant.Reset();
				continue;
			}
			if (FChar::IsUpper(C) && Courant.Len() > 0 && FChar::IsLower(Courant[Courant.Len() - 1]))
			{
				Mots.Add(Courant.ToLower());
				Courant.Reset();
			}
			Courant.AppendChar(C);
		}
		if (Courant.Len() > 0) { Mots.Add(Courant.ToLower()); }
		return Mots;
	}

	/** Le nom ressemble-t-il a une voiture complete (et pas a une piece detachee) ? */
	bool NomDeVoiture(const FString& Nom)
	{
		static const TSet<FString> MotsVoiture = {
			TEXT("car"), TEXT("cars"), TEXT("vehicle"), TEXT("vehicule"), TEXT("voiture"), TEXT("sedan"),
			TEXT("hatchback"), TEXT("suv"), TEXT("pickup"), TEXT("truck"), TEXT("van"), TEXT("minivan"),
			TEXT("bus"), TEXT("taxi"), TEXT("coupe"), TEXT("jeep"), TEXT("police"), TEXT("ambulance"),
			TEXT("wagon"), TEXT("limo"), TEXT("auto"), TEXT("camion"), TEXT("berline"), TEXT("citadine")
		};
		static const TSet<FString> MotsPiece = {
			TEXT("wheel"), TEXT("wheels"), TEXT("tire"), TEXT("tires"), TEXT("tyre"), TEXT("tyres"), TEXT("rim"),
			TEXT("door"), TEXT("doors"), TEXT("glass"), TEXT("window"), TEXT("windos"), TEXT("windscreen"),
			TEXT("windshield"), TEXT("bumper"), TEXT("mirror"), TEXT("bonnet"), TEXT("hood"), TEXT("boot"),
			TEXT("trunk"), TEXT("wing"), TEXT("plate"), TEXT("licence"), TEXT("license"), TEXT("seat"),
			TEXT("interior"), TEXT("destroyed"), TEXT("broken"), TEXT("wreck"), TEXT("shard"), TEXT("scrap"),
			TEXT("chassis"), TEXT("engine"), TEXT("light"), TEXT("lights"), TEXT("steering"), TEXT("part"),
			TEXT("parts"), TEXT("sign"), TEXT("toy"), TEXT("icon"), TEXT("brake"), TEXT("caliper"),
			TEXT("crashed"), TEXT("damaged"), TEXT("proxy"), TEXT("collision"), TEXT("track"), TEXT("dashboard")
		};
		// Morceaux de nom interdits n'importe ou : panneaux ("Police_Signboard"), wagons de train,
		// niveaux de detail en double ("_LOD1"), squelette du kart de demo ("Vehicle_SkelMesh").
		static const TCHAR* MorceauxInterdits[] = {
			TEXT("sign"), TEXT("board"), TEXT("wagon"), TEXT("train"), TEXT("bearing"), TEXT("lod"),
			TEXT("skelmesh"), TEXT("kart"), TEXT("bike"), TEXT("scooter"), TEXT("parking"), TEXT("garage"),
			// Gros vehicules : trop longs, ils bouchent les carrefours. SUV_01a : roule a reculons.
			TEXT("truck"), TEXT("bus"), TEXT("camper"), TEXT("camion"), TEXT("lorry"), TEXT("suv_01a")
		};
		const FString Minuscule = Nom.ToLower();
		for (const TCHAR* Morceau : MorceauxInterdits)
		{
			if (Minuscule.Contains(Morceau)) { return false; }
		}
		bool bVoiture = false;
		for (const FString& Mot : DecouperEnMots(Nom))
		{
			if (MotsPiece.Contains(Mot)) { return false; }
			bVoiture |= MotsVoiture.Contains(Mot);
		}
		return bVoiture;
	}

	/** Verifie qu'un objet a la taille d'une voiture / d'un utilitaire / d'un bus et calcule son orientation. */
	bool TailleDeVoiture(const FBoxSphereBounds& Bornes, float& OutCorrection, float& OutLongueur)
	{
		const FVector E = Bornes.BoxExtent * 2.f;
		const float Longueur = FMath::Max(E.X, E.Y);
		const float Largeur = FMath::Min(E.X, E.Y);
		if (Longueur < 300.f || Longueur > 1400.f || Largeur < 140.f || Largeur > 320.f
			|| E.Z < 100.f || E.Z > 450.f || Longueur < Largeur * 1.6f)
		{
			return false;
		}
		// L'avant d'une voiture Unreal est en +X ; si le modele est allonge en Y on le tourne.
		OutCorrection = (E.Y > E.X) ? -90.f : 0.f;
		OutLongueur = Longueur;
		return true;
	}

	FVector Droite(const FVector& Direction)
	{
		return FVector(-Direction.Y, Direction.X, 0.f); // on roule a droite (France)
	}

	/** 26/11/2026 (agent 1) : sens d'une voiture a squelette d'apres ses roues (avant - arriere).
	 *  Renvoie la rotation (degres) qui met l'avant en +X, ou false si les roues ne sont pas reconnues. */
	bool OrientationParLesRoues(const USkeletalMesh* Mesh, float& OutCorrection)
	{
		if (!Mesh)
		{
			return false;
		}
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const TArray<FTransform>& Pose = Ref.GetRefBonePose();
		TArray<FTransform> Monde;
		Monde.SetNum(Pose.Num());
		static const TSet<FString> Avant = { TEXT("front"), TEXT("fl"), TEXT("fr"), TEXT("lf"), TEXT("rf"), TEXT("f"), TEXT("avant") };
		static const TSet<FString> Arriere = { TEXT("rear"), TEXT("back"), TEXT("rl"), TEXT("rr"), TEXT("lr"), TEXT("lb"), TEXT("rb"), TEXT("bl"), TEXT("br"), TEXT("b"), TEXT("arriere") };
		FVector SommeAvant = FVector::ZeroVector, SommeArriere = FVector::ZeroVector;
		int32 NbAvant = 0, NbArriere = 0;
		for (int32 i = 0; i < Pose.Num(); ++i)
		{
			const int32 Parent = Ref.GetParentIndex(i);
			Monde[i] = (Parent >= 0 && Parent < i) ? Pose[i] * Monde[Parent] : Pose[i];
			const FString Nom = Ref.GetBoneName(i).ToString().ToLower();
			if (!Nom.Contains(TEXT("wheel")) && !Nom.Contains(TEXT("tire")) && !Nom.Contains(TEXT("tyre")) && !Nom.Contains(TEXT("roue")))
			{
				continue;
			}
			TArray<FString> Mots;
			FString Propre = Nom;
			for (TCHAR& C : Propre.GetCharArray()) { if (C && !FChar::IsAlnum(C)) { C = TEXT(' '); } }
			Propre.ParseIntoArrayWS(Mots);
			bool bAvant = false, bArriere = false;
			for (const FString& M : Mots)
			{
				bAvant |= Avant.Contains(M);
				bArriere |= Arriere.Contains(M);
			}
			if (bAvant == bArriere)
			{
				continue;
			}
			if (bAvant) { SommeAvant += Monde[i].GetLocation(); ++NbAvant; }
			else { SommeArriere += Monde[i].GetLocation(); ++NbArriere; }
		}
		if (NbAvant == 0 || NbArriere == 0)
		{
			return false;
		}
		const FVector Sens = SommeAvant / NbAvant - SommeArriere / NbArriere;
		if (Sens.Size2D() < 50.f)
		{
			return false;
		}
		OutCorrection = -FMath::RadiansToDegrees(FMath::Atan2(Sens.Y, Sens.X));
		return true;
	}
}

// =====================================================================================================
// Construction
// =====================================================================================================

ACirculationSpawner::ACirculationSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // active seulement quand il y a des routes
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));

	TypesDeRues = { TEXT("secondary"), TEXT("tertiary"), TEXT("residential"), TEXT("unclassified"), TEXT("living_street") };

	// 26/11/2026 : formes du moteur pour dessiner les feux tricolores
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Cylindre(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Materiau(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	FeuMeshPoteau = Cylindre.Get();
	FeuMeshBoitier = Cube.Get();
	FeuMeshLampe = Sphere.Get();
	FeuMateriau = Materiau.Get();

#if WITH_EDITORONLY_DATA
	Icone = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icone"));
	if (Icone)
	{
		static ConstructorHelpers::FObjectFinderOptional<UTexture2D> Sprite(TEXT("/Engine/EditorResources/S_Pawn"));
		if (Sprite.Succeeded()) { Icone->SetSprite(Sprite.Get()); }
		Icone->SetupAttachment(RootComponent);
		Icone->SetRelativeScale3D(FVector(2.f));
	}
#endif
}

void ACirculationSpawner::Annoncer(const FString& Texte, const FColor& Couleur) const
{
	UE_LOG(LogCirculation, Log, TEXT("%s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 12.f, Couleur, Texte);
	}
}

// =====================================================================================================
// Reseau routier
// =====================================================================================================

bool ACirculationSpawner::LireOpenStreetMap(TArray<FNoeudRoutier>& OutNoeuds, TArray<FTronconRoutier>& OutTroncons, FString& OutMessage) const
{
	const FString Chemin = FPaths::IsRelative(FichierOpenStreetMap)
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), FichierOpenStreetMap))
		: FichierOpenStreetMap;

	FString Texte;
	if (!FFileHelper::LoadFileToString(Texte, *Chemin))
	{
		OutMessage = FString::Printf(TEXT("Circulation Chelles : fichier des rues introuvable (%s)."), *Chemin);
		return false;
	}
	TSharedPtr<FJsonObject> Racine;
	const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Texte);
	const TArray<TSharedPtr<FJsonValue>>* Elements = nullptr;
	if (!FJsonSerializer::Deserialize(Lecteur, Racine) || !Racine.IsValid() || !Racine->TryGetArrayField(TEXT("elements"), Elements))
	{
		OutMessage = FString::Printf(TEXT("Circulation Chelles : fichier des rues illisible (%s)."), *Chemin);
		return false;
	}

	// Meme projection que Tools/build_buildings.py, puis meme repere que chelles_complet.py : (x, -y).
	const double MetresParDegreLat = 111320.0;
	const double MetresParDegreLon = 111320.0 * FMath::Cos(static_cast<float>(LatitudeCentre * PI / 180.0));
	auto VersMonde = [&](double Lat, double Lon)
	{
		return FVector(
			static_cast<float>((Lon - LongitudeCentre) * MetresParDegreLon * 100.0 + DecalageDeLaCarte.X),
			static_cast<float>(-(Lat - LatitudeCentre) * MetresParDegreLat * 100.0 + DecalageDeLaCarte.Y),
			0.f);
	};

	TMap<int64, int32> IndexParCle;
	auto ObtenirNoeud = [&](double Lat, double Lon, const FVector& Position)
	{
		// Les rues qui se croisent partagent exactement le meme point OSM (cle calculee en double precision).
		const int64 LatEntier = static_cast<int64>(FMath::FloorToDouble(Lat * 1e7 + 0.5));
		const int64 LonEntier = static_cast<int64>(FMath::FloorToDouble(Lon * 1e7 + 0.5));
		const int64 Cle = (LatEntier << 32) ^ (LonEntier & 0xffffffffLL);
		if (const int32* Existant = IndexParCle.Find(Cle))
		{
			return *Existant;
		}
		const int32 Index = OutNoeuds.AddDefaulted();
		OutNoeuds[Index].Position = Position;
		IndexParCle.Add(Cle, Index);
		return Index;
	};

	TSet<uint64> Existants;
	bool bRueEnRondPoint = false;
	auto AjouterTroncon = [&](int32 A, int32 B, float Decalage, float Vitesse)
	{
		const uint64 Cle = (uint64(uint32(A)) << 32) | uint32(B);
		if (A == B || Existants.Contains(Cle)) { return; }
		Existants.Add(Cle);
		FTronconRoutier T;
		T.De = A;
		T.Vers = B;
		T.Decalage = Decalage;
		T.VitesseMax = Vitesse;
		T.bRondPoint = bRueEnRondPoint;
		OutTroncons.Add(T);
	};

	auto DansLaZone = [&](const FVector& P)
	{
		return LimiteDeLaZone <= 0.f
			|| (FMath::Abs(P.X - DecalageDeLaCarte.X) <= LimiteDeLaZone && FMath::Abs(P.Y - DecalageDeLaCarte.Y) <= LimiteDeLaZone);
	};

	int32 RuesLues = 0;
	for (const TSharedPtr<FJsonValue>& Valeur : *Elements)
	{
		const TSharedPtr<FJsonObject> Rue = Valeur.IsValid() ? Valeur->AsObject() : nullptr;
		const TSharedPtr<FJsonObject>* TagsPtr = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Geometrie = nullptr;
		if (!Rue.IsValid() || !Rue->TryGetObjectField(TEXT("tags"), TagsPtr) || !Rue->TryGetArrayField(TEXT("geometry"), Geometrie))
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& TagsRue = *TagsPtr;
		const FString Type = ChampTexte(TagsRue, TEXT("highway"));
		if (!TypesDeRues.Contains(Type) || ChampTexte(TagsRue, TEXT("area")) == TEXT("yes") || ChampTexte(TagsRue, TEXT("access")) == TEXT("no"))
		{
			continue;
		}

		const FString Jonction = ChampTexte(TagsRue, TEXT("junction"));
		bRueEnRondPoint = (Jonction == TEXT("roundabout") || Jonction == TEXT("circular"));
		const FString SensUnique = ChampTexte(TagsRue, TEXT("oneway"));
		const bool bSensInverse = (SensUnique == TEXT("-1") || SensUnique == TEXT("reverse"));
		const bool bSensUnique = bSensInverse || SensUnique == TEXT("yes") || SensUnique == TEXT("true") || SensUnique == TEXT("1")
			|| (bRueEnRondPoint && SensUnique != TEXT("no")) || (Type == TEXT("motorway") && SensUnique != TEXT("no"));
		const float Decalage = bSensUnique ? 0.f : LargeurRue(Type) * 0.25f;
		float Kmh = LireVitesseKmh(ChampTexte(TagsRue, TEXT("maxspeed")));
		if (Kmh <= 1.f) { Kmh = VitesseParDefautKmh(Type); }
		const float Vitesse = Kmh * KmhVersCms;

		int32 IndexPrecedent = INDEX_NONE;
		double LatPrecedente = 0.0, LonPrecedente = 0.0;
		FVector PositionPrecedente = FVector::ZeroVector;
		bool bAUnPrecedent = false;
		for (const TSharedPtr<FJsonValue>& PointValeur : *Geometrie)
		{
			const TSharedPtr<FJsonObject> Point = PointValeur.IsValid() ? PointValeur->AsObject() : nullptr;
			double Lat = 0.0, Lon = 0.0;
			if (!Point.IsValid() || !Point->TryGetNumberField(TEXT("lat"), Lat) || !Point->TryGetNumberField(TEXT("lon"), Lon))
			{
				continue;
			}
			const FVector Position = VersMonde(Lat, Lon);
			if (bAUnPrecedent && DansLaZone((Position + PositionPrecedente) * 0.5f))
			{
				const int32 A = (IndexPrecedent != INDEX_NONE) ? IndexPrecedent : ObtenirNoeud(LatPrecedente, LonPrecedente, PositionPrecedente);
				const int32 B = ObtenirNoeud(Lat, Lon, Position);
				if (!bSensUnique || !bSensInverse) { AjouterTroncon(A, B, Decalage, Vitesse); }
				if (!bSensUnique || bSensInverse) { AjouterTroncon(B, A, Decalage, Vitesse); }
				IndexPrecedent = B;
			}
			else
			{
				IndexPrecedent = INDEX_NONE; // hors de la zone construite : pas de route 3D ici
			}
			LatPrecedente = Lat;
			LonPrecedente = Lon;
			PositionPrecedente = Position;
			bAUnPrecedent = true;
		}
		++RuesLues;
	}
	UE_LOG(LogCirculation, Log, TEXT("OpenStreetMap : %d rue(s) retenue(s), %d troncon(s)."), RuesLues, OutTroncons.Num());
	return true;
}

void ACirculationSpawner::AjouterRoutesManuelles(TArray<FNoeudRoutier>& OutNoeuds, TArray<FTronconRoutier>& OutTroncons) const
{
	const float Vitesse = VitesseRoutesManuelles * KmhVersCms;
	auto NoeudProche = [&](const FVector& Position, bool bChercherExistant)
	{
		if (bChercherExistant)
		{
			// Les extremites des splines se raccordent aux points tout proches (autre spline ou rue OSM).
			for (int32 i = 0; i < OutNoeuds.Num(); ++i)
			{
				if (FVector::DistSquared2D(OutNoeuds[i].Position, Position) < FMath::Square(150.f))
				{
					return i;
				}
			}
		}
		const int32 Index = OutNoeuds.AddDefaulted();
		OutNoeuds[Index].Position = Position;
		OutNoeuds[Index].bAltitudeFixe = true;
		return Index;
	};

	for (const AActor* Acteur : RoutesManuelles)
	{
		if (!IsValid(Acteur)) { continue; }
		const bool bSensUnique = Acteur->ActorHasTag(TEXT("SensUnique"));
		const float Decalage = bSensUnique ? 0.f : LargeurRoutesManuelles * 0.25f;
		TArray<USplineComponent*> Splines;
		Acteur->GetComponents<USplineComponent>(Splines);
		for (const USplineComponent* Spline : Splines)
		{
			const float Longueur = Spline->GetSplineLength();
			if (Longueur < 100.f) { continue; }
			const int32 Pas = FMath::Max(1, FMath::CeilToInt(Longueur / 500.f));
			int32 Precedent = INDEX_NONE;
			for (int32 i = 0; i <= Pas; ++i)
			{
				const FVector Position = Spline->GetLocationAtDistanceAlongSpline(Longueur * i / Pas, ESplineCoordinateSpace::World);
				const int32 Courant = NoeudProche(Position, i == 0 || i == Pas);
				if (Precedent != INDEX_NONE && Precedent != Courant)
				{
					FTronconRoutier T;
					T.Decalage = Decalage;
					T.VitesseMax = Vitesse;
					T.De = Precedent;
					T.Vers = Courant;
					OutTroncons.Add(T);
					if (!bSensUnique)
					{
						Swap(T.De, T.Vers);
						OutTroncons.Add(T);
					}
				}
				Precedent = Courant;
			}
		}
	}
}

void ACirculationSpawner::FinaliserReseau(TArray<FNoeudRoutier>& InOutNoeuds, TArray<FTronconRoutier>& InOutTroncons)
{
	// 1) Longueurs et directions ; on jette les troncons de longueur nulle.
	InOutTroncons.RemoveAll([&](FTronconRoutier& T)
	{
		const FVector D = InOutNoeuds[T.Vers].Position - InOutNoeuds[T.De].Position;
		T.Longueur = D.Size2D();
		T.Direction = FVector(D.X, D.Y, 0.f).GetSafeNormal();
		return T.Longueur < 1.f;
	});

	// 2) On ne garde que la plus grande partie du reseau ou l'on peut aller partout (composante fortement
	//    connexe) : une voiture ne se retrouve jamais coincee dans un sens unique sans issue.
	const int32 N = InOutNoeuds.Num();
	TArray<TArray<int32>> Suivants, Precedents;
	Suivants.SetNum(N);
	Precedents.SetNum(N);
	for (const FTronconRoutier& T : InOutTroncons)
	{
		Suivants[T.De].Add(T.Vers);
		Precedents[T.Vers].Add(T.De);
	}
	TArray<int32> Ordre;
	Ordre.Reserve(N);
	TArray<uint8> Vu;
	Vu.Init(0, N);
	TArray<TPair<int32, int32>> Pile;
	for (int32 Depart = 0; Depart < N; ++Depart)
	{
		if (Vu[Depart]) { continue; }
		Vu[Depart] = 1;
		Pile.Add(TPair<int32, int32>(Depart, 0));
		while (Pile.Num() > 0)
		{
			const int32 Noeud = Pile.Last().Key;
			const int32 Rang = Pile.Last().Value;
			if (Rang < Suivants[Noeud].Num())
			{
				++Pile.Last().Value;
				const int32 Voisin = Suivants[Noeud][Rang];
				if (!Vu[Voisin])
				{
					Vu[Voisin] = 1;
					Pile.Add(TPair<int32, int32>(Voisin, 0));
				}
			}
			else
			{
				Ordre.Add(Noeud);
				Pile.Pop(false);
			}
		}
	}
	TArray<int32> Composante;
	Composante.Init(INDEX_NONE, N);
	int32 MeilleureComposante = INDEX_NONE, MeilleureTaille = 0, NombreComposantes = 0;
	TArray<int32> Pile2;
	for (int32 k = Ordre.Num() - 1; k >= 0; --k)
	{
		const int32 Depart = Ordre[k];
		if (Composante[Depart] != INDEX_NONE) { continue; }
		int32 Taille = 0;
		Composante[Depart] = NombreComposantes;
		Pile2.Add(Depart);
		while (Pile2.Num() > 0)
		{
			const int32 Noeud = Pile2.Pop(false);
			++Taille;
			for (const int32 Voisin : Precedents[Noeud])
			{
				if (Composante[Voisin] == INDEX_NONE)
				{
					Composante[Voisin] = NombreComposantes;
					Pile2.Add(Voisin);
				}
			}
		}
		if (Taille > MeilleureTaille)
		{
			MeilleureTaille = Taille;
			MeilleureComposante = NombreComposantes;
		}
		++NombreComposantes;
	}

	// 3) Renumerotation compacte.
	TArray<int32> NouvelIndex;
	NouvelIndex.Init(INDEX_NONE, N);
	TArray<FNoeudRoutier> Noeuds2;
	for (int32 i = 0; i < N; ++i)
	{
		if (Composante[i] == MeilleureComposante)
		{
			NouvelIndex[i] = Noeuds2.Add(InOutNoeuds[i]);
			Noeuds2.Last().Sorties.Reset();
			Noeuds2.Last().NombreDeVoisins = 0;
		}
	}
	TArray<FTronconRoutier> Troncons2;
	for (FTronconRoutier T : InOutTroncons)
	{
		if (NouvelIndex[T.De] != INDEX_NONE && NouvelIndex[T.Vers] != INDEX_NONE)
		{
			T.De = NouvelIndex[T.De];
			T.Vers = NouvelIndex[T.Vers];
			T.Inverse = INDEX_NONE;
			Troncons2.Add(T);
		}
	}

	// 4) Sorties, sens inverse, nombre de voisins (carrefours).
	TMap<uint64, int32> IndexTroncon;
	for (int32 i = 0; i < Troncons2.Num(); ++i)
	{
		IndexTroncon.Add((uint64(uint32(Troncons2[i].De)) << 32) | uint32(Troncons2[i].Vers), i);
		Noeuds2[Troncons2[i].De].Sorties.Add(i);
	}
	TArray<TSet<int32>> Voisins;
	Voisins.SetNum(Noeuds2.Num());
	for (int32 i = 0; i < Troncons2.Num(); ++i)
	{
		FTronconRoutier& T = Troncons2[i];
		if (const int32* Inverse = IndexTroncon.Find((uint64(uint32(T.Vers)) << 32) | uint32(T.De)))
		{
			T.Inverse = *Inverse;
		}
		Voisins[T.De].Add(T.Vers);
		Voisins[T.Vers].Add(T.De);
	}
	for (int32 i = 0; i < Noeuds2.Num(); ++i)
	{
		Noeuds2[i].NombreDeVoisins = Voisins[i].Num();
	}

	InOutNoeuds = MoveTemp(Noeuds2);
	InOutTroncons = MoveTemp(Troncons2);
}

bool ACirculationSpawner::TrouverSol(const FVector& Point, float DemiPlage, float& OutZ) const
{
	UWorld* Monde = GetWorld();
	if (!Monde) { return false; }
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CirculationSol), true, this);
	const FVector Haut = Point + FVector(0.f, 0.f, DemiPlage);
	const FVector Bas = Point - FVector(0.f, 0.f, DemiPlage);
	for (int32 Essai = 0; Essai < 8; ++Essai)
	{
		FHitResult Impact;
		if (!Monde->LineTraceSingleByChannel(Impact, Haut, Bas, ECC_Visibility, Params))
		{
			return false;
		}
		const UPrimitiveComponent* Composant = Impact.GetComponent();
		const AActor* Proprietaire = Composant ? Composant->GetOwner() : nullptr;
		// On ne garde que les grandes surfaces (routes, sol, Landscape) : pas les arbres, lampadaires ou voitures.
		const bool bGrand = Composant && (Composant->Bounds.BoxExtent.X > TailleMinimaleSol || Composant->Bounds.BoxExtent.Y > TailleMinimaleSol);
		if (bGrand && Impact.ImpactNormal.Z > 0.5f && !(Proprietaire && Proprietaire->IsA<AVehiculeCirculation>()))
		{
			OutZ = Impact.ImpactPoint.Z;
			return true;
		}
		if (Proprietaire)
		{
			Params.AddIgnoredActor(Proprietaire);
		}
		else if (Composant)
		{
			Params.AddIgnoredComponent(Composant);
		}
		else
		{
			return false;
		}
	}
	return false;
}

void ACirculationSpawner::MesurerAltitudes()
{
	const float ZReference = GetActorLocation().Z;
	TArray<bool> Connue;
	Connue.Init(false, Noeuds.Num());
	int32 Manquants = 0;
	for (int32 i = 0; i < Noeuds.Num(); ++i)
	{
		FNoeudRoutier& Noeud = Noeuds[i];
		if (Noeud.bAltitudeFixe)
		{
			Connue[i] = true;
			continue;
		}
		float Z = 0.f;
		if (TrouverSol(FVector(Noeud.Position.X, Noeud.Position.Y, ZReference), 5000.f, Z))
		{
			Noeud.Position.Z = Z;
			Connue[i] = true;
		}
		else
		{
			++Manquants;
		}
	}
	// Points sans sol mesure : altitude moyenne des voisins, sinon celle du Spawner.
	for (int32 Passe = 0; Passe < 6 && Manquants > 0; ++Passe)
	{
		for (const FTronconRoutier& T : Troncons)
		{
			if (!Connue[T.Vers] && Connue[T.De])
			{
				Noeuds[T.Vers].Position.Z = Noeuds[T.De].Position.Z;
				Connue[T.Vers] = true;
				--Manquants;
			}
		}
	}
	for (int32 i = 0; i < Noeuds.Num(); ++i)
	{
		if (!Connue[i]) { Noeuds[i].Position.Z = ZReference; }
	}
	if (Manquants > 0)
	{
		UE_LOG(LogCirculation, Warning, TEXT("%d point(s) de route sans sol trouve : altitude du Spawner utilisee."), Manquants);
	}
}

void ACirculationSpawner::DessinerReseau(float Duree) const
{
	UWorld* Monde = GetWorld();
	if (!Monde) { return; }
	const bool bPermanent = Duree < 0.f;
	const FVector Hauteur(0.f, 0.f, 40.f);
	for (int32 i = 0; i < Troncons.Num(); ++i)
	{
		const FTronconRoutier& T = Troncons[i];
		const FVector A = PointSurTroncon(i, 0.f) + Hauteur;
		const FVector B = PointSurTroncon(i, T.Longueur) + Hauteur;
		const FColor Couleur = (T.Inverse == INDEX_NONE) ? FColor(255, 170, 0) : FColor(40, 220, 60);
		DrawDebugLine(Monde, A, B, Couleur, bPermanent, Duree, 0, 12.f);
		if (Noeuds[T.Vers].NombreDeVoisins >= 3)
		{
			DrawDebugPoint(Monde, Noeuds[T.Vers].Position + Hauteur, 14.f, FColor::Red, bPermanent, Duree);
		}
	}
}

void ACirculationSpawner::ConstruireLeReseauRoutier()
{
	TArray<FNoeudRoutier> NouveauxNoeuds;
	TArray<FTronconRoutier> NouveauxTroncons;
	if (bUtiliserOpenStreetMap)
	{
		FString Message;
		if (!LireOpenStreetMap(NouveauxNoeuds, NouveauxTroncons, Message))
		{
			Annoncer(Message, FColor::Orange);
		}
	}
	AjouterRoutesManuelles(NouveauxNoeuds, NouveauxTroncons);
	FinaliserReseau(NouveauxNoeuds, NouveauxTroncons);
	if (NouveauxTroncons.Num() == 0)
	{
		Annoncer(TEXT("Circulation Chelles : aucune route trouvee (ni rue OSM, ni spline dans 'Routes Manuelles')."), FColor::Red);
		return;
	}

	Modify(); // Ctrl+Z possible, et le niveau est marque "a enregistrer"
	Noeuds = MoveTemp(NouveauxNoeuds);
	Troncons = MoveTemp(NouveauxTroncons);
	MesurerAltitudes();
	DessinerReseau(30.f);

	float Total = 0.f;
	for (const FTronconRoutier& T : Troncons) { Total += T.Longueur; }
	Annoncer(FString::Printf(TEXT("Circulation Chelles : reseau construit (%d points, %d troncons, %.1f km de voies). "
		"Lignes vertes = double sens, orange = sens unique. Enregistrez le niveau."),
		Noeuds.Num(), Troncons.Num(), Total / 100000.f), FColor::Green);
}

// =====================================================================================================
// Vehicules
// =====================================================================================================

void ACirculationSpawner::ChercherVoituresDansContent(TArray<FModeleVehicule>& OutModeles) const
{
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	FARFilter Filtre;
	Filtre.ClassNames.Add(UStaticMesh::StaticClass()->GetFName());
	Filtre.ClassNames.Add(USkeletalMesh::StaticClass()->GetFName());
	Filtre.PackagePaths.Add(FName(*DossierDeRecherche));
	Filtre.bRecursivePaths = true;
	TArray<FAssetData> Assets;
	Registre.GetAssets(Filtre, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.ObjectPath.LexicalLess(B.ObjectPath); });

	for (const FAssetData& Donnee : Assets)
	{
		const FString Chemin = Donnee.PackagePath.ToString();
		const FString CheminMinuscule = Chemin.ToLower() + TEXT("/");
		const bool bDossierExclu = CheminMinuscule.Contains(TEXT("/parts/")) || CheminMinuscule.Contains(TEXT("/part/"))
			|| CheminMinuscule.Contains(TEXT("train")) || CheminMinuscule.Contains(TEXT("/lods/"))
			|| CheminMinuscule.Contains(TEXT("destroy")) || CheminMinuscule.Contains(TEXT("/vehicleadv/"));
		if (Chemin.StartsWith(TEXT("/Game/Chelles")) || Chemin.Contains(TEXT("/Developers/")) || bDossierExclu
			|| !NomDeVoiture(Donnee.AssetName.ToString()))
		{
			continue; // on ne charge que les assets dont le nom ressemble a une voiture
		}
		UObject* Asset = Donnee.GetAsset();
		FModeleVehicule Modele;
		FBoxSphereBounds Bornes;
		if (UStaticMesh* Statique = Cast<UStaticMesh>(Asset))
		{
			Modele.MeshStatique = Statique;
			Bornes = Statique->GetBounds();
		}
		else if (USkeletalMesh* Squelettique = Cast<USkeletalMesh>(Asset))
		{
			Modele.MeshSquelettique = Squelettique;
			Bornes = Squelettique->GetImportedBounds();
		}
		else
		{
			continue;
		}
		float Longueur = 0.f;
		if (!TailleDeVoiture(Bornes, Modele.CorrectionOrientation, Longueur))
		{
			UE_LOG(LogCirculation, Log, TEXT("Ignore %s (taille %s)"), *Asset->GetPathName(), *(Bornes.BoxExtent * 2.f).ToString());
			continue;
		}
		if (Longueur > LongueurMaximale)
		{
			UE_LOG(LogCirculation, Log, TEXT("Ignore %s (trop long : %.0f cm > %.0f cm)"), *Asset->GetPathName(), Longueur, LongueurMaximale);
			continue;
		}
		Modele.Frequence = (Longueur > 800.f) ? 0.25f : 1.f; // bus et camions plus rares
		OutModeles.Add(Modele);
		UE_LOG(LogCirculation, Log, TEXT("Voiture trouvee : %s (longueur %.0f cm, rotation %.0f)"),
			*Asset->GetPathName(), Longueur, Modele.CorrectionOrientation);
	}
}

void ACirculationSpawner::RemplirLaListeDesVoitures()
{
	TArray<FModeleVehicule> Trouves;
	ChercherVoituresDansContent(Trouves);
	if (Trouves.Num() == 0)
	{
		Annoncer(TEXT("Circulation Chelles : aucune voiture trouvee dans ") + DossierDeRecherche, FColor::Orange);
		return;
	}
	Modify();
	Modeles = Trouves;
	Annoncer(FString::Printf(TEXT("Circulation Chelles : %d voiture(s) ajoutee(s) a la liste. Si une voiture roule de travers "
		"ou a reculons, changez sa 'Correction Orientation' (90, -90 ou 180). Enregistrez le niveau."), Trouves.Num()), FColor::Green);
}

const FModeleVehicule* ACirculationSpawner::TirerUnModele() const
{
	if (ModelesActifs.Num() == 0 || FrequenceTotale <= 0.f)
	{
		return nullptr;
	}
	float Tirage = FMath::FRand() * FrequenceTotale;
	for (const FModeleVehicule& Modele : ModelesActifs)
	{
		Tirage -= Modele.Frequence;
		if (Tirage <= 0.f) { return &Modele; }
	}
	return &ModelesActifs.Last();
}

void ACirculationSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!bActif)
	{
		return;
	}

	// Si le fichier des rues est la (editeur), on reconstruit le reseau a chaque lancement : il est
	// toujours a jour (et les niveaux enregistres avec une ancienne version profitent des corrections).
	// Sinon (jeu package), on garde le reseau enregistre avec le niveau.
	{
		TArray<FNoeudRoutier> NouveauxNoeuds;
		TArray<FTronconRoutier> NouveauxTroncons;
		FString Message;
		const bool bOSM = bUtiliserOpenStreetMap && LireOpenStreetMap(NouveauxNoeuds, NouveauxTroncons, Message);
		if (bOSM || Troncons.Num() == 0)
		{
			AjouterRoutesManuelles(NouveauxNoeuds, NouveauxTroncons);
			FinaliserReseau(NouveauxNoeuds, NouveauxTroncons);
			if (NouveauxTroncons.Num() > 0)
			{
				Noeuds = MoveTemp(NouveauxNoeuds);
				Troncons = MoveTemp(NouveauxTroncons);
			}
		}
	}
	if (Troncons.Num() == 0)
	{
		Annoncer(TEXT("Circulation Chelles : aucune route. Selectionnez le Circulation Spawner et cliquez sur "
			"'Construire le reseau routier' (ou ajoutez des splines dans 'Routes Manuelles')."), FColor::Orange);
		return;
	}

	MesurerAltitudes();
	ReservationsCarrefours.Init(FReservationCarrefour(), Noeuds.Num());
	NoeudSurRondPoint.Init(false, Noeuds.Num());
	int32 TronconsRondPoint = 0;
	for (const FTronconRoutier& T : Troncons)
	{
		if (T.bRondPoint)
		{
			NoeudSurRondPoint[T.De] = NoeudSurRondPoint[T.Vers] = true;
			++TronconsRondPoint;
		}
	}
	UE_LOG(LogCirculation, Log, TEXT("%d troncon(s) de rond-point."), TronconsRondPoint);
	LongueursCumulees.Reset(Troncons.Num());
	float Cumul = 0.f;
	for (const FTronconRoutier& T : Troncons)
	{
		Cumul += T.Longueur;
		LongueursCumulees.Add(Cumul);
	}

	// 26/11/2026 (agent 1) : reglages sans ouvrir l'editeur, dans Plugins\ChellesCirculation\Circulation.txt
	//  NombreDeVoitures, PositionVoie (0,25 = milieu de la voie de droite), Orientation.<modele>=180, Exclure=...
	TMap<FString, float> OrientationsTexte;
	TArray<FString> ModelesExclus;
	float PositionVoie = 0.25f;
	// 26/11/2026 : feux tricolores
	int32 FeuxMaximum = 25;
	bool bFeuxGrandesRues = true;
	float FeuxDistanceMinimum = 4000.f;
	{
		TArray<FString> Lignes;
		const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesCirculation"), TEXT("Circulation.txt"));
		if (FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
		{
			for (FString Ligne : Lignes)
			{
				Ligne.TrimStartAndEndInline();
				FString Cle, Valeur;
				if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || !Ligne.Split(TEXT("="), &Cle, &Valeur))
				{
					continue;
				}
				Cle.TrimStartAndEndInline();
				Valeur.TrimStartAndEndInline();
				if (Cle.Equals(TEXT("NombreDeVoitures"), ESearchCase::IgnoreCase) && FCString::Atoi(*Valeur) > 0)
				{
					NombreDeVoitures = FMath::Clamp(FCString::Atoi(*Valeur), 1, 400);
				}
				else if (Cle.Equals(TEXT("PositionVoie"), ESearchCase::IgnoreCase))
				{
					PositionVoie = FMath::Clamp(FCString::Atof(*Valeur), 0.1f, 0.45f);
				}
				else if (Cle.StartsWith(TEXT("Orientation."), ESearchCase::IgnoreCase))
				{
					OrientationsTexte.Add(Cle.Mid(12).ToLower(), FCString::Atof(*Valeur));
				}
				else if (Cle.Equals(TEXT("FeuxTricolores"), ESearchCase::IgnoreCase))
				{
					bFeuxTricolores = FCString::Atoi(*Valeur) != 0;
				}
				else if (Cle.Equals(TEXT("FeuxNombreMaximum"), ESearchCase::IgnoreCase))
				{
					FeuxMaximum = FMath::Clamp(FCString::Atoi(*Valeur), 0, 200);
				}
				else if (Cle.Equals(TEXT("FeuxDureeVert"), ESearchCase::IgnoreCase))
				{
					FeuDureeVert = FMath::Clamp(FCString::Atof(*Valeur), 3.f, 90.f);
				}
				else if (Cle.Equals(TEXT("FeuxDureeOrange"), ESearchCase::IgnoreCase))
				{
					FeuDureeOrange = FMath::Clamp(FCString::Atof(*Valeur), 1.f, 10.f);
				}
				else if (Cle.Equals(TEXT("FeuxDureeRougePartout"), ESearchCase::IgnoreCase))
				{
					FeuDureeRougePartout = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 10.f);
				}
				else if (Cle.Equals(TEXT("FeuxSeulementGrandesRues"), ESearchCase::IgnoreCase))
				{
					bFeuxGrandesRues = FCString::Atoi(*Valeur) != 0;
				}
				else if (Cle.Equals(TEXT("FeuxDistanceMinimum"), ESearchCase::IgnoreCase))
				{
					FeuxDistanceMinimum = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 500.f) * 100.f; // metres -> cm
				}
				else if (Cle.Equals(TEXT("Exclure"), ESearchCase::IgnoreCase))
				{
					TArray<FString> Noms;
					Valeur.ParseIntoArray(Noms, TEXT(","), true);
					for (FString& N : Noms) { ModelesExclus.Add(N.TrimStartAndEnd().ToLower()); }
				}
			}
		}
	}
	// Feux tricolores : avant la mise a l'echelle des decalages (la largeur des rues = Decalage x 4)
	ConstruireFeux(bFeuxTricolores ? FeuxMaximum : 0, bFeuxGrandesRues, FeuxDistanceMinimum, PositionVoie);
	DessinerFeux();
	for (FTronconRoutier& T : Troncons)
	{
		T.Decalage *= PositionVoie / 0.25f; // le reseau est construit avec 0,25
	}

	ModelesActifs.Reset();
	for (const FModeleVehicule& Modele : Modeles)
	{
		if (!Modele.EstValide() || Modele.Frequence <= 0.f) { continue; }
		// Liste remplie par une ancienne version : on retire les faux vehicules (panneaux, wagons, pieces...).
		const UObject* Asset = Modele.MeshStatique ? static_cast<const UObject*>(Modele.MeshStatique) : Modele.MeshSquelettique;
		const FString Chemin = Asset->GetPathName().ToLower();
		if (!NomDeVoiture(Asset->GetName()) || Chemin.Contains(TEXT("/parts/")) || Chemin.Contains(TEXT("train"))
			|| Chemin.Contains(TEXT("/vehicleadv/")))
		{
			UE_LOG(LogCirculation, Log, TEXT("Modele retire (pas une voiture) : %s"), *Asset->GetPathName());
			continue;
		}
		// Camions et bus trop longs pour les ronds-points : retires, meme s'ils sont dans la liste enregistree.
		const FBoxSphereBounds Bornes = Modele.MeshStatique ? Modele.MeshStatique->GetBounds() : Modele.MeshSquelettique->GetImportedBounds();
		const float Longueur = FMath::Max(Bornes.BoxExtent.X, Bornes.BoxExtent.Y) * 2.f;
		if (Longueur > LongueurMaximale)
		{
			UE_LOG(LogCirculation, Log, TEXT("Modele retire (trop long : %.0f cm) : %s"), Longueur, *Asset->GetPathName());
			continue;
		}
		ModelesActifs.Add(Modele);
	}
	if (ModelesActifs.Num() == 0)
	{
		ChercherVoituresDansContent(ModelesActifs);
	}
	// 26/11/2026 (agent 1) : modele exclu, orientation ecrite dans Circulation.txt, sinon sens d'apres les roues
	for (int32 i = ModelesActifs.Num() - 1; i >= 0; --i)
	{
		FModeleVehicule& Modele = ModelesActifs[i];
		const UObject* Asset = Modele.MeshStatique ? static_cast<const UObject*>(Modele.MeshStatique) : Modele.MeshSquelettique;
		const FString Nom = Asset ? Asset->GetName() : FString();
		if (ModelesExclus.Contains(Nom.ToLower()))
		{
			UE_LOG(LogCirculation, Display, TEXT("Modele exclu (Circulation.txt) : %s"), *Nom);
			ModelesActifs.RemoveAt(i);
			continue;
		}
		const float Avant = Modele.CorrectionOrientation;
		FString Origine = TEXT("niveau");
		float Roues = 0.f;
		if (const float* Texte = OrientationsTexte.Find(Nom.ToLower()))
		{
			Modele.CorrectionOrientation = *Texte;
			Origine = TEXT("Circulation.txt");
		}
		else if (OrientationParLesRoues(Modele.MeshSquelettique, Roues))
		{
			Modele.CorrectionOrientation = Roues;
			Origine = TEXT("roues");
		}
		UE_LOG(LogCirculation, Display, TEXT("Modele %s : rotation %.0f (avant %.0f, source %s)"),
			*Nom, Modele.CorrectionOrientation, Avant, *Origine);
	}
	FrequenceTotale = 0.f;
	for (const FModeleVehicule& Modele : ModelesActifs) { FrequenceTotale += Modele.Frequence; }
	if (ModelesActifs.Num() == 0)
	{
		Annoncer(TEXT("Circulation Chelles : aucune voiture trouvee, des boites colorees sont utilisees. "
			"Cliquez sur 'Remplir la liste des voitures' dans le panneau Details."), FColor::Orange);
	}

	if (bAfficherLeReseau)
	{
		DessinerReseau(-1.f);
	}

	VoituresRestantesAFaireApparaitre = NombreDeVoitures;
	ProchainRecyclage = GetWorld()->GetTimeSeconds() + 2.f;
	SetActorTickEnabled(true);
}

void ACirculationSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetActorTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void ACirculationSpawner::FaireApparaitreUnLot()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	for (int32 i = 0; i < VoituresParLot && VoituresRestantesAFaireApparaitre > 0; ++i, --VoituresRestantesAFaireApparaitre)
	{
		AVehiculeCirculation* Vehicule = GetWorld()->SpawnActor<AVehiculeCirculation>(
			AVehiculeCirculation::StaticClass(), FTransform(GetActorLocation()), Params);
		if (!Vehicule) { continue; }

		const FModeleVehicule* Modele = TirerUnModele();
		if (!Modele || !Vehicule->Configurer(*Modele, DistanceAffichage, bCouleursAleatoires))
		{
			Vehicule->ConfigurerSilhouette(DistanceAffichage);
		}
		Vehicules.Add(Vehicule);
		if (!PlacerVehicule(Vehicule, false))
		{
			Vehicules.Pop(false);
			Vehicule->Destroy();
		}
	}

	if (VoituresRestantesAFaireApparaitre == 0)
	{
		float Total = 0.f;
		for (const FTronconRoutier& T : Troncons) { Total += T.Longueur; }
		Annoncer(FString::Printf(TEXT("Circulation Chelles : %d voiture(s) en circulation, %d modele(s), %.1f km de voies."),
			Vehicules.Num(), ModelesActifs.Num(), Total / 100000.f), FColor::Green);
	}
}

bool ACirculationSpawner::PlacerVehicule(AVehiculeCirculation* Vehicule, bool bLoinDuJoueur)
{
	const int32 IndexVehicule = Vehicules.Find(Vehicule);
	LibererCarrefours(Vehicule, IndexVehicule, true);
	Vehicule->ReinitialiserConduite();
	if (LongueursCumulees.Num() == 0) { return false; }

	FVector PositionJoueur = FVector::ZeroVector;
	FVector VueJoueur = FVector::ForwardVector;
	bool bJoueur = false;
	if (APlayerController* Controleur = GetWorld()->GetFirstPlayerController())
	{
		FRotator Rotation;
		Controleur->GetPlayerViewPoint(PositionJoueur, Rotation);
		VueJoueur = Rotation.Vector().GetSafeNormal2D();
		bJoueur = Controleur->GetPawn() != nullptr;
	}

	for (int32 Essai = 0; Essai < 40; ++Essai)
	{
		// Point au hasard, proportionnellement a la longueur des rues.
		const float Tirage = FMath::FRand() * LongueursCumulees.Last();
		const int32 T = FMath::Clamp(Algo::UpperBound(LongueursCumulees, Tirage), 0, Troncons.Num() - 1);
		const float S = FMath::FRand() * Troncons[T].Longueur;
		const FVector Point = PointSurTroncon(T, S);

		if (bJoueur && bSuivreLeJoueur)
		{
			const float D = FVector::Dist2D(Point, PositionJoueur);
			if (D > RayonAutourDuJoueur * 0.9f) { continue; }
			// Reapparition hors de la vue du joueur (pas de voiture qui "pop" devant lui).
			if (bLoinDuJoueur && (D < 6000.f || (D < 20000.f && (((Point - PositionJoueur).GetSafeNormal2D()) | VueJoueur) > 0.2f)))
			{
				continue;
			}
		}

		bool bLibre = true;
		for (const AVehiculeCirculation* Autre : Vehicules)
		{
			if (Autre != Vehicule && IsValid(Autre) && Autre->Itineraire.Num() > 0
				&& FVector::DistSquared(Autre->GetActorLocation(), Point) < FMath::Square(EspacementApparition))
			{
				bLibre = false;
				break;
			}
		}
		if (!bLibre) { continue; }

		Vehicule->Itineraire.Add(T);
		Vehicule->Distance = S;
		Vehicule->Temperament = FMath::FRandRange(0.85f, 1.1f);
		Vehicule->Vitesse = Troncons[T].VitesseMax * 0.5f * MultiplicateurDeVitesse;
		EtendreItineraire(Vehicule);

		FVector Position, Tangente;
		CalculerPose(Vehicule, Position, Tangente);
		Vehicule->RotationAffichee = Tangente.Rotation();
		Vehicule->bPoseInitialisee = true;
		Vehicule->SetActorLocationAndRotation(Position, Vehicule->RotationAffichee, false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}
	return false;
}

void ACirculationSpawner::EtendreItineraire(AVehiculeCirculation* Vehicule) const
{
	// On prevoit au moins 3 troncons ET 60 m de route : sur un rond-point les troncons sont tres courts,
	// et avec 4 troncons seulement la voiture ne "voyait" que 15 m devant elle.
	float Prevu = -Vehicule->Distance;
	for (const int32 Troncon : Vehicule->Itineraire) { Prevu += Troncons[Troncon].Longueur; }
	while (Vehicule->Itineraire.Num() > 0 && Vehicule->Itineraire.Num() < MaxTronconsPrevus
		&& (Vehicule->Itineraire.Num() < 3 || Prevu < 6000.f))
	{
		const FTronconRoutier& Dernier = Troncons[Vehicule->Itineraire.Last()];
		const FNoeudRoutier& Noeud = Noeuds[Dernier.Vers];
		// Tout droit plus probable que tourner ; demi-tour seulement dans une impasse.
		float Poids[16];
		int32 Choix[16];
		int32 Nombre = 0;
		float Total = 0.f;
		for (const int32 Sortie : Noeud.Sorties)
		{
			if (Nombre >= 16) { break; }
			if (Sortie == Dernier.Inverse && Noeud.Sorties.Num() > 1) { continue; }
			const float Cos = Dernier.Direction | Troncons[Sortie].Direction;
			Poids[Nombre] = 0.35f + FMath::Max(0.f, Cos);
			Choix[Nombre] = Sortie;
			Total += Poids[Nombre];
			++Nombre;
		}
		if (Nombre == 0) { break; }
		float Tirage = FMath::FRand() * Total;
		int32 Retenu = Choix[Nombre - 1];
		for (int32 i = 0; i < Nombre; ++i)
		{
			Tirage -= Poids[i];
			if (Tirage <= 0.f) { Retenu = Choix[i]; break; }
		}
		Vehicule->Itineraire.Add(Retenu);
		Prevu += Troncons[Retenu].Longueur;
	}
}

void ACirculationSpawner::LibererCarrefours(AVehiculeCirculation* Vehicule, int32 IndexVehicule, bool bTous)
{
	auto EstDerriere = [&](int32 Noeud)
	{
		const TArray<int32>& It = Vehicule->Itineraire;
		if (It.Num() == 0) { return true; }
		for (const int32 Troncon : It)
		{
			if (Troncons[Troncon].Vers == Noeud) { return false; } // encore devant nous
		}
		// Juste apres le carrefour : on le libere quand l'arriere de la voiture l'a quitte.
		return Troncons[It[0]].De != Noeud || Vehicule->Distance > Vehicule->Longueur * 0.5f + 500.f;
	};

	for (int32 k = Vehicule->CarrefoursReserves.Num() - 1; k >= 0; --k)
	{
		const int32 Noeud = Vehicule->CarrefoursReserves[k];
		if (!bTous && !EstDerriere(Noeud)) { continue; }
		if (ReservationsCarrefours.IsValidIndex(Noeud))
		{
			FReservationCarrefour& R = ReservationsCarrefours[Noeud];
			R.Nombre = FMath::Max(0, R.Nombre - 1);
			if (R.Nombre == 0) { R.TronconEntree = INDEX_NONE; }
		}
		Vehicule->CarrefoursReserves.RemoveAt(k);
	}
	if (Vehicule->CarrefourForce != INDEX_NONE && (bTous || EstDerriere(Vehicule->CarrefourForce)))
	{
		Vehicule->CarrefourForce = INDEX_NONE;
	}
	if (Vehicule->CarrefourDemande != INDEX_NONE && (bTous || EstDerriere(Vehicule->CarrefourDemande)))
	{
		if (ReservationsCarrefours.IsValidIndex(Vehicule->CarrefourDemande)
			&& ReservationsCarrefours[Vehicule->CarrefourDemande].Demandeur == IndexVehicule)
		{
			ReservationsCarrefours[Vehicule->CarrefourDemande].Demandeur = INDEX_NONE;
		}
		Vehicule->CarrefourDemande = INDEX_NONE;
	}
}

// =====================================================================================================
// Conduite
// =====================================================================================================

FVector ACirculationSpawner::PointSurTroncon(int32 Troncon, float S) const
{
	const FTronconRoutier& T = Troncons[Troncon];
	const float Alpha = (T.Longueur > 0.f) ? FMath::Clamp(S / T.Longueur, 0.f, 1.f) : 0.f;
	return FMath::Lerp(Noeuds[T.De].Position, Noeuds[T.Vers].Position, Alpha) + Droite(T.Direction) * T.Decalage;
}

void ACirculationSpawner::CalculerPose(const AVehiculeCirculation* Vehicule, FVector& OutPosition, FVector& OutTangente) const
{
	const int32 E0 = Vehicule->Itineraire[0];
	const FTronconRoutier& T0 = Troncons[E0];
	const float S = Vehicule->Distance;
	auto Rayon = [](float Longueur) { return FMath::Min(RayonVirage, Longueur * 0.45f); };

	// Pres d'un noeud, on suit une courbe (Bezier) entre les deux troncons : virages arrondis,
	// et passage en douceur d'une voie decalee a l'autre.
	int32 Ea = INDEX_NONE, Eb = INDEX_NONE;
	float U = 0.f;
	const float R0 = Rayon(T0.Longueur);
	if (Vehicule->Itineraire.Num() > 1 && S > T0.Longueur - R0)
	{
		Ea = E0;
		Eb = Vehicule->Itineraire[1];
		const float Total = R0 + Rayon(Troncons[Eb].Longueur);
		U = (S - (T0.Longueur - R0)) / FMath::Max(Total, 1.f);
	}
	else if (Vehicule->TronconPrecedent != INDEX_NONE && S < R0 && Troncons[Vehicule->TronconPrecedent].Vers == T0.De)
	{
		Ea = Vehicule->TronconPrecedent;
		Eb = E0;
		const float Ra = Rayon(Troncons[Ea].Longueur);
		U = (Ra + S) / FMath::Max(Ra + R0, 1.f);
	}

	if (Ea == INDEX_NONE)
	{
		OutPosition = PointSurTroncon(E0, S);
		const float Pente = (T0.Longueur > 0.f) ? (Noeuds[T0.Vers].Position.Z - Noeuds[T0.De].Position.Z) / T0.Longueur : 0.f;
		OutTangente = T0.Direction + FVector(0.f, 0.f, Pente);
		return;
	}

	const FTronconRoutier& A = Troncons[Ea];
	const FTronconRoutier& B = Troncons[Eb];
	const float Ra = Rayon(A.Longueur);
	const float Rb = Rayon(B.Longueur);
	const FVector P0 = PointSurTroncon(Ea, A.Longueur - Ra);
	const FVector P2 = PointSurTroncon(Eb, Rb);
	const FVector FinA = PointSurTroncon(Ea, A.Longueur);
	const FVector DebutB = PointSurTroncon(Eb, 0.f);
	const FVector Milieu = (FinA + DebutB) * 0.5f;

	// Point de controle : intersection des deux voies (coin du virage).
	FVector P1 = Milieu;
	const float Croix = A.Direction.X * B.Direction.Y - A.Direction.Y * B.Direction.X;
	if (FMath::Abs(Croix) < 0.05f)
	{
		if ((A.Direction | B.Direction) < 0.f)
		{
			// Demi-tour (impasse) : on deborde un peu vers l'avant pour faire une boucle.
			P1 = Milieu + A.Direction * FMath::Max(200.f, (A.Decalage + B.Decalage) * 1.5f);
		}
	}
	else
	{
		const FVector Ecart = DebutB - FinA;
		const float t = (Ecart.X * B.Direction.Y - Ecart.Y * B.Direction.X) / Croix;
		if (FMath::Abs(t) < FMath::Max(Ra, Rb) * 1.5f)
		{
			P1 = FinA + A.Direction * t;
			P1.Z = Milieu.Z;
		}
	}

	U = FMath::Clamp(U, 0.f, 1.f);
	const float V = 1.f - U;
	OutPosition = P0 * (V * V) + P1 * (2.f * V * U) + P2 * (U * U);
	OutTangente = (P1 - P0) * (2.f * V) + (P2 - P1) * (2.f * U);
	if (OutTangente.SizeSquared2D() < 1.f)
	{
		OutTangente = B.Direction;
	}
}

bool ACirculationSpawner::SortieLibre(int32 IndexVehicule, int32 TronconSortie) const
{
	if (!Troncons.IsValidIndex(TronconSortie)) { return true; }
	const AVehiculeCirculation* Vehicule = Vehicules[IndexVehicule];
	// Il faut la place de la voiture entiere apres le carrefour, plus une marge.
	const float PlaceNecessaire = Vehicule->Longueur + DistanceArret + 200.f;
	for (int32 j = 0; j < Vehicules.Num(); ++j)
	{
		const AVehiculeCirculation* Autre = Vehicules[j];
		if (j == IndexVehicule || !IsValid(Autre) || Autre->Itineraire.Num() == 0) { continue; }
		if (Autre->Itineraire[0] == TronconSortie && Autre->Vitesse < 300.f
			&& Autre->Distance - Autre->Longueur * 0.5f < PlaceNecessaire)
		{
			return false;
		}
	}
	return true;
}

bool ACirculationSpawner::CarrefourDegage(int32 IndexVehicule, int32 Noeud) const
{
	const FVector Centre = Noeuds[Noeud].Position;
	for (int32 j = 0; j < Vehicules.Num(); ++j)
	{
		const AVehiculeCirculation* Autre = Vehicules[j];
		if (j == IndexVehicule || !IsValid(Autre) || EstGaree(j)) { continue; }
		if (FVector::DistSquared2D(Autre->GetActorLocation(), Centre) < FMath::Square(Autre->Longueur * 0.5f + 250.f))
		{
			return false;
		}
	}
	return true;
}

float ACirculationSpawner::DistanceAuVehiculeDevant(int32 IndexVehicule, float& OutVitesseDevant, bool bIgnorerCroisements, bool& bOutCroisement) const
{
	const AVehiculeCirculation* Vehicule = Vehicules[IndexVehicule];
	const TArray<int32>& It = Vehicule->Itineraire;
	// Distance (depuis la voiture) jusqu'au debut de chaque troncon de l'itineraire.
	float Debut[MaxTronconsPrevus];
	const int32 N = FMath::Min(It.Num(), MaxTronconsPrevus);
	Debut[0] = -Vehicule->Distance;
	for (int32 k = 1; k < N; ++k)
	{
		Debut[k] = Debut[k - 1] + Troncons[It[k - 1]].Longueur;
	}

	const FVector MaPosition = Vehicule->GetActorLocation();
	FVector Avant = Vehicule->RotationAffichee.Vector();
	Avant.Z = 0.f;
	Avant.Normalize();

	float Meilleur = BIG_NUMBER;
	OutVitesseDevant = 0.f;
	bOutCroisement = false;
	for (int32 j = 0; j < Vehicules.Num(); ++j)
	{
		const AVehiculeCirculation* Autre = Vehicules[j];
		if (j == IndexVehicule || !IsValid(Autre) || Autre->Itineraire.Num() == 0) { continue; }
		const FVector PositionAutre = Autre->GetActorLocation();
		if (FVector::DistSquared2D(MaPosition, PositionAutre) > FMath::Square(6000.f)) { continue; }

		// 1) Sur ma route (meme troncon devant moi, ou un des troncons suivants) : distance exacte.
		float Ecart = BIG_NUMBER;
		const int32 TronconAutre = Autre->Itineraire[0];
		for (int32 k = 0; k < N; ++k)
		{
			if (It[k] == TronconAutre)
			{
				const float D = Debut[k] + Autre->Distance;
				if (k > 0 || D > 0.f) { Ecart = D; }
				break;
			}
		}
		// 2) Sinon, voiture juste devant moi (virage, carrefour) : test geometrique.
		bool bCroisement = false;
		if (Ecart == BIG_NUMBER)
		{
			if (bIgnorerCroisements) { continue; }
			bCroisement = true;
			const FVector Rel(PositionAutre.X - MaPosition.X, PositionAutre.Y - MaPosition.Y, 0.f);
			const float Longitudinal = Rel | Avant;
			const float Lateral = FMath::Abs(Rel.X * Avant.Y - Rel.Y * Avant.X);
			if (Longitudinal > 0.f && Longitudinal < (Vehicule->Longueur + Autre->Longueur) * 0.5f + 600.f
				&& Lateral < (Vehicule->Largeur + Autre->Largeur) * 0.4f)
			{
				Ecart = Longitudinal;
			}
			else
			{
				continue;
			}
		}
		Ecart -= (Vehicule->Longueur + Autre->Longueur) * 0.5f;
		if (Ecart < Meilleur)
		{
			Meilleur = Ecart;
			OutVitesseDevant = Autre->Vitesse;
			bOutCroisement = bCroisement;
		}
	}
	return Meilleur;
}

float ACirculationSpawner::DistanceAuxPietons(AVehiculeCirculation* Vehicule, const FVector& Position, const FVector& Avant) const
{
	const float Portee = 300.f + Vehicule->Vitesse * 1.3f;
	const FVector Centre = Position + Avant * (Vehicule->Longueur * 0.5f + Portee * 0.5f) + FVector(0.f, 0.f, 100.f);
	const FCollisionShape Boite = FCollisionShape::MakeBox(FVector(Portee * 0.5f, Vehicule->Largeur * 0.5f + 60.f, 120.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CirculationPietons), false, Vehicule);
	TArray<FOverlapResult> Resultats;
	GetWorld()->OverlapMultiByObjectType(Resultats, Centre, FRotationMatrix::MakeFromX(Avant).ToQuat(),
		FCollisionObjectQueryParams(ECC_Pawn), Boite, Params);

	float Meilleure = BIG_NUMBER;
	for (const FOverlapResult& Resultat : Resultats)
	{
		if (const AActor* Acteur = Resultat.GetActor())
		{
			const float D = ((Acteur->GetActorLocation() - Position) | Avant) - Vehicule->Longueur * 0.5f - 40.f;
			Meilleure = FMath::Min(Meilleure, FMath::Max(0.f, D));
		}
	}
	return Meilleure;
}

void ACirculationSpawner::ConduireVehicule(int32 IndexVehicule, float DeltaSeconds, const FVector& PositionJoueur,
	const FVector& VueJoueur, bool bJoueurValide)
{
	AVehiculeCirculation* Vehicule = Vehicules[IndexVehicule];
	if (!IsValid(Vehicule) || Vehicule->Itineraire.Num() == 0)
	{
		return;
	}
	const float Maintenant = GetWorld()->GetTimeSeconds();
	const float Facteur = Vehicule->Temperament * MultiplicateurDeVitesse;
	const TArray<int32>& It = Vehicule->Itineraire;
	const FTronconRoutier& T0 = Troncons[It[0]];

	// ---- 1. Vitesse permise : limitation + ralentir avant les virages
	float Cible = T0.VitesseMax * Facteur;
	float DistanceAuNoeud = T0.Longueur - Vehicule->Distance;
	int32 Carrefour = INDEX_NONE;
	int32 TronconEntree = INDEX_NONE;
	int32 TronconSortie = INDEX_NONE;
	float DistanceCarrefour = BIG_NUMBER;
	for (int32 k = 0; k + 1 < It.Num() && DistanceAuNoeud < 8000.f; ++k)
	{
		const FTronconRoutier& A = Troncons[It[k]];
		const FTronconRoutier& B = Troncons[It[k + 1]];
		const float Cos = A.Direction | B.Direction;
		float VitesseVirage = B.VitesseMax * Facteur;
		if (Cos < 0.97f)
		{
			// 90 degres : environ 1/3 de la vitesse ; demi-tour : au pas.
			VitesseVirage = FMath::Max(300.f, VitesseVirage * FMath::Pow(FMath::Clamp((Cos + 1.f) * 0.5f, 0.f, 1.f), 1.5f));
		}
		Cible = FMath::Min(Cible, FMath::Sqrt(FMath::Square(VitesseVirage) + 2.f * FreinageConfort * FMath::Max(0.f, DistanceAuNoeud - 100.f)));
		if (Carrefour == INDEX_NONE && Noeuds[A.Vers].NombreDeVoisins >= 3)
		{
			Carrefour = A.Vers;
			TronconEntree = It[k];
			TronconSortie = It[k + 1];
			DistanceCarrefour = DistanceAuNoeud;
		}
		DistanceAuNoeud += B.Longueur;
	}

	// ---- 1b. Feux tricolores (26/11/2026) : on s'arrete au rouge (sauf si c'est impossible meme en freinant fort)
	//      et a l'orange si on peut s'arreter sans piler. Les voitures en file derriere attendent aussi.
	int32 NoeudFeu = INDEX_NONE;
	bool bArretAuFeu = false;
	float ObstacleFeu = BIG_NUMBER;
	if (Feux.Num() > 0)
	{
		float D = T0.Longueur - Vehicule->Distance;
		for (int32 k = 0; k < It.Num() && D < 8000.f; ++k)
		{
			const int32 Entree = It[k];
			const int32 N = Troncons[Entree].Vers;
			if (FeuDuNoeud.IsValidIndex(N) && FeuDuNoeud[N] != INDEX_NONE && PhaseDuTroncon.IsValidIndex(Entree) && PhaseDuTroncon[Entree] >= 0)
			{
				const FFeuTricolore& Feu = Feux[FeuDuNoeud[N]];
				const uint8 Etat = EtatFeu(Feu, static_cast<uint8>(PhaseDuTroncon[Entree]), Maintenant);
				const float Ligne = D - Vehicule->Longueur * 0.5f - Feu.Retrait;
				NoeudFeu = N;
				if (Etat != 0 && Ligne > 0.f)
				{
					const float Freinage = (Etat == 2) ? FreinageMax : FreinageConfort;
					if (Ligne >= FMath::Square(Vehicule->Vitesse) / (2.f * Freinage))
					{
						bArretAuFeu = true;
						ObstacleFeu = Ligne;
					}
				}
				break;
			}
			if (k + 1 < It.Num()) { D += Troncons[It[k + 1]].Longueur; }
		}
	}

	// ---- 2. Carrefours : une seule direction a la fois (les voitures de la meme rue passent a la suite).
	//      Rond-point : priorite aux voitures deja engagees (code de la route francais) ; on n'entre que si
	//      personne n'arrive et s'il y a la place de degager (sinon le rond-point se bloque).
	float Obstacle = BIG_NUMBER;
	if (Carrefour != INDEX_NONE && ReservationsCarrefours.IsValidIndex(Carrefour) && !Vehicule->CarrefoursReserves.Contains(Carrefour)
		&& Vehicule->CarrefourForce != Carrefour && !(bArretAuFeu && Carrefour == NoeudFeu))
	{
		const float LigneDArret = DistanceCarrefour - Vehicule->Longueur * 0.5f - 350.f;
		const float DistanceDeReservation = FMath::Max(1500.f, FMath::Square(Vehicule->Vitesse) / (2.f * FreinageConfort) + Vehicule->Longueur + 400.f);
		if (DistanceCarrefour < DistanceDeReservation)
		{
			FReservationCarrefour& R = ReservationsCarrefours[Carrefour];
			const bool bSurLAnneau = Troncons[TronconEntree].bRondPoint;
			const bool bEntreeDeRondPoint = !bSurLAnneau && NoeudSurRondPoint.IsValidIndex(Carrefour) && NoeudSurRondPoint[Carrefour];
			const bool bDemandeurValide = Vehicules.IsValidIndex(R.Demandeur) && IsValid(Vehicules[R.Demandeur])
				&& Vehicules[R.Demandeur]->CarrefourDemande == Carrefour;
			if (!bDemandeurValide) { R.Demandeur = INDEX_NONE; }
			const bool bTropPres = LigneDArret < FMath::Square(Vehicule->Vitesse) / (2.f * FreinageMax) || LigneDArret <= 0.f;
			const bool bPlaceApres = bTropPres || SortieLibre(IndexVehicule, TronconSortie);
			// Carrefour a feux, au vert : les rues du meme groupe (vert en meme temps) passent ensemble.
			const bool bAvecFeu = FeuDuNoeud.IsValidIndex(Carrefour) && FeuDuNoeud[Carrefour] != INDEX_NONE
				&& PhaseDuTroncon.IsValidIndex(TronconEntree) && PhaseDuTroncon[TronconEntree] >= 0;

			bool bPasse = false;
			if (bSurLAnneau)
			{
				bPasse = true; // deja dans le rond-point : prioritaire
			}
			else if (bEntreeDeRondPoint)
			{
				bPasse = bPlaceApres && (R.Nombre == 0 || bTropPres);
			}
			else if (bAvecFeu)
			{
				const bool bMemeGroupe = R.Nombre == 0 || (PhaseDuTroncon.IsValidIndex(R.TronconEntree)
					&& PhaseDuTroncon[R.TronconEntree] == PhaseDuTroncon[TronconEntree]);
				bPasse = bMemeGroupe && (bPlaceApres || bTropPres);
			}
			else
			{
				const bool bLibre = R.Nombre == 0 && (R.Demandeur == INDEX_NONE || R.Demandeur == IndexVehicule);
				const bool bALaSuite = R.Nombre > 0 && R.TronconEntree == TronconEntree && (R.Demandeur == INDEX_NONE || bTropPres);
				bPasse = (bLibre || bALaSuite) && bPlaceApres;
			}

			if (bPasse)
			{
				if (R.Nombre == 0) { R.TronconEntree = TronconEntree; }
				++R.Nombre;
				if (R.Demandeur == IndexVehicule) { R.Demandeur = INDEX_NONE; }
				Vehicule->CarrefourDemande = INDEX_NONE;
				Vehicule->CarrefoursReserves.Add(Carrefour);
				Vehicule->AttenteCarrefour = 0.f;
			}
			else if (LigneDArret > 0.f)
			{
				Obstacle = LigneDArret;
				// Au rond-point on ne bloque pas l'anneau en "demandant" le passage.
				if (!bEntreeDeRondPoint && !bAvecFeu && R.Demandeur == INDEX_NONE && bPlaceApres)
				{
					R.Demandeur = IndexVehicule;
					Vehicule->CarrefourDemande = Carrefour;
				}
				if (Vehicule->Vitesse < 30.f)
				{
					Vehicule->AttenteCarrefour += DeltaSeconds;
					const float Delai = bEntreeDeRondPoint ? DelaiForcerRondPoint : DelaiForcerCarrefour;
					// Anti-blocage : on passe quand meme, mais SEULEMENT si le carrefour et la sortie sont
					// vraiment vides (avant : les voitures se traversaient et s'entassaient).
					if (Vehicule->AttenteCarrefour > Delai && CarrefourDegage(IndexVehicule, Carrefour) && SortieLibre(IndexVehicule, TronconSortie))
					{
						Vehicule->CarrefourForce = Carrefour;
						Vehicule->AttenteCarrefour = 0.f;
						if (R.Demandeur == IndexVehicule) { R.Demandeur = INDEX_NONE; }
						Vehicule->CarrefourDemande = INDEX_NONE;
						Obstacle = BIG_NUMBER;
					}
				}
			}
		}
	}
	Obstacle = FMath::Min(Obstacle, ObstacleFeu);
	if (Obstacle < BIG_NUMBER)
	{
		Cible = FMath::Min(Cible, FMath::Sqrt(2.f * FreinageConfort * FMath::Max(0.f, Obstacle - 100.f)));
	}

	// ---- 3. Voiture de devant : distance de securite
	float VitesseDevant = 0.f;
	bool bBloqueParCroisement = false;
	const bool bPassageForce = Maintenant < Vehicule->FinPassageForce;
	const float Ecart = DistanceAuVehiculeDevant(IndexVehicule, VitesseDevant, bPassageForce, bBloqueParCroisement);
	// Anti-interblocage : chacun attend l'autre au milieu du carrefour / du rond-point -> l'un finit par passer.
	// Le delai depend du temperament, pour que les voitures bloquees ne repartent pas toutes en meme temps.
	if (bBloqueParCroisement && Vehicule->Vitesse < 30.f)
	{
		Vehicule->TempsBloqueEnCroisement += DeltaSeconds;
		if (Vehicule->TempsBloqueEnCroisement > DelaiDebloquerCroisement * Vehicule->Temperament)
		{
			Vehicule->FinPassageForce = Maintenant + DureePassageForce;
			Vehicule->TempsBloqueEnCroisement = 0.f;
		}
	}
	else if (!bBloqueParCroisement)
	{
		Vehicule->TempsBloqueEnCroisement = 0.f;
	}
	if (Ecart < BIG_NUMBER)
	{
		const float Marge = DistanceArret + TempsInterVehicules * Vehicule->Vitesse;
		Cible = (Ecart <= DistanceArret)
			? 0.f
			: FMath::Min(Cible, FMath::Sqrt(FMath::Max(0.f, FMath::Square(VitesseDevant) + 2.f * FreinageConfort * (Ecart - Marge))));
	}

	// ---- 4. Pietons et joueur devant la voiture (teste 6 a 7 fois par seconde, seulement pres du joueur)
	const FVector Position = Vehicule->GetActorLocation();
	FVector Avant = Vehicule->RotationAffichee.Vector();
	Avant.Z = 0.f;
	Avant.Normalize();
	if (bSArreterPourLesPietons)
	{
		const bool bProcheDuJoueur = !bJoueurValide || FVector::DistSquared2D(Position, PositionJoueur) < FMath::Square(15000.f);
		if (Maintenant >= Vehicule->ProchainTestPieton)
		{
			Vehicule->ProchainTestPieton = Maintenant + 0.15f + FMath::FRand() * 0.05f;
			Vehicule->DistancePieton = bProcheDuJoueur ? DistanceAuxPietons(Vehicule, Position, Avant) : BIG_NUMBER;
		}
		if (Vehicule->DistancePieton < BIG_NUMBER)
		{
			Cible = FMath::Min(Cible, FMath::Sqrt(2.f * FreinageConfort * FMath::Max(0.f, Vehicule->DistancePieton - 150.f)));
			if (Vehicule->Vitesse < 20.f && Vehicule->DistancePieton < 600.f)
			{
				Vehicule->TempsBloqueParPieton += DeltaSeconds;
				if (Klaxon && Vehicule->TempsBloqueParPieton > 2.5f && Maintenant >= Vehicule->ProchainKlaxon)
				{
					UGameplayStatics::PlaySoundAtLocation(this, Klaxon, Position);
					Vehicule->ProchainKlaxon = Maintenant + 10.f;
				}
			}
			else
			{
				Vehicule->TempsBloqueParPieton = 0.f;
			}
		}
		else
		{
			Vehicule->TempsBloqueParPieton = 0.f;
		}
	}

	// ---- 4b. Voiture de police derriere (plugin ChellesPolice) : on se range a droite puis on s'arrete
	const bool bSeRange = Maintenant < Vehicule->FinRangement;
	if (bSeRange)
	{
		// au pas pendant qu'on s'ecarte, puis a l'arret le long du trottoir
		Cible = FMath::Min(Cible, Vehicule->EcartRange >= EcartRangement * 0.7f ? 0.f : 350.f);
	}

	// ---- 5. Acceleration / freinage progressifs
	Vehicule->Vitesse = (Vehicule->Vitesse < Cible)
		? FMath::Min(Cible, Vehicule->Vitesse + Acceleration * DeltaSeconds)
		: FMath::Max(Cible, Vehicule->Vitesse - FreinageMax * DeltaSeconds);

	// ---- 6. Avancer sur l'itineraire
	Vehicule->Distance += Vehicule->Vitesse * DeltaSeconds;
	while (Vehicule->Itineraire.Num() > 0 && Vehicule->Distance >= Troncons[Vehicule->Itineraire[0]].Longueur)
	{
		Vehicule->Distance -= Troncons[Vehicule->Itineraire[0]].Longueur;
		Vehicule->TronconPrecedent = Vehicule->Itineraire[0];
		Vehicule->Itineraire.RemoveAt(0, 1, false);
		EtendreItineraire(Vehicule);
	}
	if (Vehicule->Itineraire.Num() == 0)
	{
		PlacerVehicule(Vehicule, true); // ne devrait pas arriver (reseau sans impasse a sens unique)
		return;
	}
	LibererCarrefours(Vehicule, IndexVehicule, false);

	// ---- 7. Position et orientation (lissee) dans le niveau
	FVector NouvellePosition, Tangente;
	CalculerPose(Vehicule, NouvellePosition, Tangente);
	// Rangee pour la police : decalee vers la droite (puis revient doucement sur sa voie)
	Vehicule->EcartRange = FMath::FInterpConstantTo(Vehicule->EcartRange, bSeRange ? EcartRangement : 0.f, DeltaSeconds, 250.f);
	if (Vehicule->EcartRange > 1.f)
	{
		NouvellePosition += Droite(Tangente.GetSafeNormal2D()) * Vehicule->EcartRange;
	}
	const FRotator RotationCible = Tangente.Rotation();
	Vehicule->RotationAffichee = Vehicule->bPoseInitialisee
		? FMath::RInterpTo(Vehicule->RotationAffichee, RotationCible, DeltaSeconds, 10.f)
		: RotationCible;
	Vehicule->RotationAffichee.Roll = 0.f;
	Vehicule->bPoseInitialisee = true;
	Vehicule->SetActorLocationAndRotation(NouvellePosition, Vehicule->RotationAffichee);

	// ---- 8. Bouchon sans fin hors de la vue du joueur : on deplace la voiture ailleurs (pas l'attente au feu rouge)
	if (Vehicule->Vitesse < 5.f && !bArretAuFeu)
	{
		Vehicule->TempsALArret += DeltaSeconds;
		const bool bHorsDeVue = !bJoueurValide || FVector::DistSquared2D(NouvellePosition, PositionJoueur) > FMath::Square(8000.f)
			|| (((NouvellePosition - PositionJoueur).GetSafeNormal2D()) | VueJoueur) < 0.f;
		if (Vehicule->TempsALArret > DelaiRecyclerBouchon && bHorsDeVue)
		{
			PlacerVehicule(Vehicule, true);
		}
	}
	else
	{
		Vehicule->TempsALArret = 0.f;
	}
}

void ACirculationSpawner::RecyclerLesVoituresLointaines(const FVector& PositionJoueur, const FVector& VueJoueur, bool bJoueurValide)
{
	if (!bSuivreLeJoueur || !bJoueurValide)
	{
		return;
	}
	for (int32 i = 0; i < Vehicules.Num(); ++i)
	{
		AVehiculeCirculation* Vehicule = Vehicules[i];
		if (IsValid(Vehicule) && !EstGaree(i) && FVector::DistSquared2D(Vehicule->GetActorLocation(), PositionJoueur) > FMath::Square(RayonAutourDuJoueur))
		{
			PlacerVehicule(Vehicule, true);
		}
	}
}

void ACirculationSpawner::GarerVehicule(int32 IndexVehicule)
{
	AVehiculeCirculation* Vehicule = Vehicules[IndexVehicule];
	LibererCarrefours(Vehicule, IndexVehicule, true);
	Vehicule->ReinitialiserConduite(); // sans itineraire : les autres voitures l'ignorent
	Vehicule->SetActorHiddenInGame(true);
	Vehicule->SetActorEnableCollision(false);
	Garees[IndexVehicule] = true;
}

void ACirculationSpawner::AppliquerLimite()
{
	while (Garees.Num() < Vehicules.Num())
	{
		Garees.Add(false);
	}
	if (Garees.Num() > Vehicules.Num())
	{
		Garees.SetNum(Vehicules.Num());
	}
	const int32 Limite = LimiteEnJeu > 0 ? LimiteEnJeu : MAX_int32;
	for (int32 i = 0; i < Vehicules.Num(); ++i)
	{
		AVehiculeCirculation* Vehicule = Vehicules[i];
		if (!IsValid(Vehicule))
		{
			continue;
		}
		const bool bGarer = i >= Limite;
		if (bGarer && !Garees[i])
		{
			GarerVehicule(i);
		}
		else if (!bGarer && Garees[i])
		{
			Garees[i] = false;
			Vehicule->SetActorHiddenInGame(false);
			Vehicule->SetActorEnableCollision(true);
			PlacerVehicule(Vehicule, true); // revient hors de la vue du joueur
		}
	}
}

void ACirculationSpawner::UneVoitureParRue(const FVector& PositionJoueur, const FVector& VueJoueur, bool bJoueurValide)
{
	if (LimiteEnJeu <= 0 || !bUneVoitureParRue)
	{
		return;
	}
	for (int32 i = 0; i < Vehicules.Num(); ++i)
	{
		AVehiculeCirculation* Vehicule = Vehicules[i];
		if (!IsValid(Vehicule) || EstGaree(i) || Vehicule->Itineraire.Num() == 0)
		{
			continue;
		}
		const FVector Ici = Vehicule->GetActorLocation();
		for (int32 j = 0; j < i; ++j)
		{
			const AVehiculeCirculation* Autre = Vehicules[j];
			if (!IsValid(Autre) || EstGaree(j) || Autre->Itineraire.Num() == 0
				|| FVector::DistSquared2D(Autre->GetActorLocation(), Ici) > FMath::Square(EcartUneParRue))
			{
				continue;
			}
			// Deux voitures dans la meme rue : celle-ci part ailleurs, si le joueur ne la voit pas
			const bool bHorsDeVue = !bJoueurValide || FVector::DistSquared2D(Ici, PositionJoueur) > FMath::Square(8000.f)
				|| (((Ici - PositionJoueur).GetSafeNormal2D()) | VueJoueur) < 0.f;
			if (bHorsDeVue)
			{
				PlacerVehicule(Vehicule, true);
			}
			break;
		}
	}
}

int32 ACirculationSpawner::DegagerDevant(FVector Position, FVector Direction, float Longueur, float Largeur, float Duree)
{
	Direction.Z = 0.f;
	if (!Direction.Normalize() || !GetWorld() || Troncons.Num() == 0)
	{
		return 0;
	}
	const FVector Cote = Droite(Direction);
	const float Maintenant = GetWorld()->GetTimeSeconds();
	int32 Nombre = 0;
	for (int32 i = 0; i < Vehicules.Num(); ++i)
	{
		AVehiculeCirculation* Vehicule = Vehicules[i];
		if (!IsValid(Vehicule) || EstGaree(i) || Vehicule->Itineraire.Num() == 0)
		{
			continue;
		}
		const FVector Ecart = Vehicule->GetActorLocation() - Position;
		const float Devant = Ecart | Direction;
		if (Devant < -150.f || Devant > Longueur)
		{
			continue; // derriere la police ou trop loin
		}
		// Position sur la voie, sans l'ecart deja pris pour se ranger
		const float SurLaVoie = (Ecart | Cote) - Vehicule->EcartRange;
		if (FMath::Abs(SurLaVoie) > Largeur)
		{
			continue; // pas sur le chemin de la police
		}
		FVector Sens = Vehicule->RotationAffichee.Vector();
		Sens.Z = 0.f;
		if (!Sens.Normalize() || (Sens | Direction) < 0.5f)
		{
			continue; // vient en face ou d'une rue en travers : ne gene pas, on la laisse rouler
		}
		if (Vehicule->FinRangement <= Maintenant)
		{
			Vehicule->DebutRangement = Maintenant;
		}
		Vehicule->FinRangement = Maintenant + Duree;
		++Nombre;
		// Rue trop etroite : malgre l'ecart, la voiture est toujours juste devant la police depuis un moment.
		// On la fait repartir ailleurs (hors de la vue du joueur), la route est libre.
		if (Devant < 1500.f && Maintenant - Vehicule->DebutRangement > 2.5f && FMath::Abs(Ecart | Cote) < 230.f)
		{
			PlacerVehicule(Vehicule, true);
		}
	}
	return Nombre;
}

void ACirculationSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Troncons.Num() == 0)
	{
		return;
	}
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f); // pas de saut geant apres un ralentissement

	FVector PositionJoueur = FVector::ZeroVector;
	FVector VueJoueur = FVector::ForwardVector;
	bool bJoueurValide = false;
	if (APlayerController* Controleur = GetWorld()->GetFirstPlayerController())
	{
		FRotator Rotation;
		Controleur->GetPlayerViewPoint(PositionJoueur, Rotation);
		VueJoueur = Rotation.Vector().GetSafeNormal2D();
		bJoueurValide = Controleur->GetPawn() != nullptr;
	}

	if (VoituresRestantesAFaireApparaitre > 0)
	{
		FaireApparaitreUnLot();
	}
	AppliquerLimite();

	for (int32 i = 0; i < Vehicules.Num(); ++i)
	{
		ConduireVehicule(i, DeltaSeconds, PositionJoueur, VueJoueur, bJoueurValide);
	}
	MettreAJourFeux();

	const float Maintenant = GetWorld()->GetTimeSeconds();
	if (Maintenant >= ProchainRecyclage)
	{
		ProchainRecyclage = Maintenant + 1.f;
		RecyclerLesVoituresLointaines(PositionJoueur, VueJoueur, bJoueurValide);
		UneVoitureParRue(PositionJoueur, VueJoueur, bJoueurValide);
	}
}

// =====================================================================================================
// Feux tricolores (26/11/2026)
// =====================================================================================================

void ACirculationSpawner::ConstruireFeux(int32 NombreMaximum, bool bSeulementGrandesRues, float DistanceMinimum, float PositionVoie)
{
	Feux.Reset();
	FeuDuNoeud.Init(INDEX_NONE, Noeuds.Num());
	PhaseDuTroncon.Init(-1, Troncons.Num());
	if (NombreMaximum <= 0)
	{
		UE_LOG(LogCirculation, Display, TEXT("Feux tricolores : coupes (Circulation.txt)."));
		return;
	}

	// Troncons qui arrivent a chaque noeud (hors rond-point)
	TArray<TArray<int32>> Entrees;
	Entrees.SetNum(Noeuds.Num());
	for (int32 t = 0; t < Troncons.Num(); ++t)
	{
		const FTronconRoutier& T = Troncons[t];
		if (!T.bRondPoint && Entrees.IsValidIndex(T.Vers))
		{
			Entrees[T.Vers].Add(t);
		}
	}
	// Demi-largeur d'une rue (cm) : Decalage = largeur x 0,25 sur une rue a double sens (avant reglage PositionVoie)
	auto DemiLargeur = [](const FTronconRoutier& T) { return T.Decalage > 0.f ? T.Decalage * 2.f : 300.f; };
	const float VitesseGrandeRue = 45.f * KmhVersCms;

	struct FCandidat { int32 Noeud; float Score; };
	TArray<FCandidat> Candidats;
	for (int32 n = 0; n < Noeuds.Num(); ++n)
	{
		if (Noeuds[n].NombreDeVoisins < 3 || Entrees[n].Num() < 2 || (NoeudSurRondPoint.IsValidIndex(n) && NoeudSurRondPoint[n]))
		{
			continue;
		}
		float VitesseMax = 0.f;
		for (const int32 e : Entrees[n]) { VitesseMax = FMath::Max(VitesseMax, Troncons[e].VitesseMax); }
		if (bSeulementGrandesRues && VitesseMax < VitesseGrandeRue)
		{
			continue;
		}
		Candidats.Add({ n, Noeuds[n].NombreDeVoisins * 100.f + VitesseMax / KmhVersCms });
	}
	Candidats.Sort([](const FCandidat& A, const FCandidat& B) { return A.Score > B.Score; });

	const float DemiCycle = FeuDureeVert + FeuDureeOrange + FeuDureeRougePartout;
	int32 NombreDeFeux = 0;
	for (const FCandidat& C : Candidats)
	{
		if (Feux.Num() >= NombreMaximum)
		{
			break;
		}
		const FVector& Centre = Noeuds[C.Noeud].Position;
		bool bTropPres = false;
		for (const FFeuTricolore& Autre : Feux)
		{
			if (FVector::DistSquared2D(Noeuds[Autre.Noeud].Position, Centre) < FMath::Square(DistanceMinimum))
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}
		// Groupe 0 = la rue la plus rapide et celle d'en face ; groupe 1 = les rues en travers.
		const TArray<int32>& E = Entrees[C.Noeud];
		int32 Reference = E[0];
		for (const int32 e : E) { if (Troncons[e].VitesseMax > Troncons[Reference].VitesseMax) { Reference = e; } }
		const FVector Axe = Troncons[Reference].Direction;
		FFeuTricolore Feu;
		Feu.Noeud = C.Noeud;
		int32 Nombre[2] = { 0, 0 };
		float DemiLargeurMax = 300.f;
		for (const int32 e : E)
		{
			const uint8 Phase = FMath::Abs(Troncons[e].Direction | Axe) > 0.7f ? 0 : 1;
			Feu.Entrees.Add(e);
			Feu.Phases.Add(Phase);
			Feu.Lampes.Add(INDEX_NONE);
			Feu.EtatAffiche.Add(3);
			++Nombre[Phase];
			DemiLargeurMax = FMath::Max(DemiLargeurMax, DemiLargeur(Troncons[e]));
		}
		if (Nombre[0] == 0 || Nombre[1] == 0)
		{
			continue; // toutes les rues dans le meme axe : pas besoin de feu
		}
		Feu.Retrait = FMath::Clamp(DemiLargeurMax + 150.f, 450.f, 900.f);
		Feu.Decalage = FMath::FRand() * 2.f * DemiCycle;
		FeuDuNoeud[C.Noeud] = Feux.Num();
		for (int32 i = 0; i < Feu.Entrees.Num(); ++i)
		{
			PhaseDuTroncon[Feu.Entrees[i]] = static_cast<int8>(Feu.Phases[i]);
		}
		NombreDeFeux += Feu.Entrees.Num();
		Feux.Add(MoveTemp(Feu));
	}
	UE_LOG(LogCirculation, Display, TEXT("Feux tricolores : %d carrefour(s), %d feu(x) (vert %.0f s, orange %.0f s, %d carrefour(s) possibles)."),
		Feux.Num(), NombreDeFeux, FeuDureeVert, FeuDureeOrange, Candidats.Num());
	for (const FFeuTricolore& Feu : Feux)
	{
		const FVector& P = Noeuds[Feu.Noeud].Position;
		UE_LOG(LogCirculation, Log, TEXT("  Feu au noeud %d (X=%.0f Y=%.0f) : %d rue(s)"), Feu.Noeud, P.X, P.Y, Feu.Entrees.Num());
	}
}

UInstancedStaticMeshComponent* ACirculationSpawner::CreerCoucheFeux(UStaticMesh* Mesh, const FLinearColor& Couleur, bool bOmbre)
{
	UInstancedStaticMeshComponent* Couche = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	Couche->SetupAttachment(RootComponent);
	Couche->SetUsingAbsoluteLocation(true);
	Couche->SetUsingAbsoluteRotation(true);
	Couche->SetUsingAbsoluteScale(true);
	Couche->SetWorldTransform(FTransform::Identity);
	Couche->SetStaticMesh(Mesh);
	Couche->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Couche->SetCanEverAffectNavigation(false);
	Couche->SetCastShadow(bOmbre);
	Couche->SetCullDistances(0, 25000);
	if (FeuMateriau)
	{
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(FeuMateriau, this);
		Mat->SetVectorParameterValue(TEXT("Color"), Couleur);
		Couche->SetMaterial(0, Mat);
	}
	Couche->RegisterComponent();
	CouchesFeux.Add(Couche);
	return Couche;
}

void ACirculationSpawner::DessinerFeux()
{
	for (UInstancedStaticMeshComponent* Couche : CouchesFeux)
	{
		if (IsValid(Couche)) { Couche->DestroyComponent(); }
	}
	CouchesFeux.Reset();
	PosesLampes.Reset();
	if (Feux.Num() == 0)
	{
		return;
	}
	if (!FeuMeshPoteau || !FeuMeshBoitier || !FeuMeshLampe)
	{
		UE_LOG(LogCirculation, Warning, TEXT("Feux tricolores : formes du moteur introuvables, les feux marchent mais ne sont pas dessines."));
		return;
	}
	UInstancedStaticMeshComponent* Poteaux = CreerCoucheFeux(FeuMeshPoteau, FLinearColor(0.08f, 0.08f, 0.09f), true);
	UInstancedStaticMeshComponent* Boitiers = CreerCoucheFeux(FeuMeshBoitier, FLinearColor(0.015f, 0.015f, 0.015f), true);
	UInstancedStaticMeshComponent* Eteintes = CreerCoucheFeux(FeuMeshLampe, FLinearColor(0.03f, 0.03f, 0.03f), false);
	CreerCoucheFeux(FeuMeshLampe, FLinearColor(0.f, 1.f, 0.25f), false);   // 3 : vert
	CreerCoucheFeux(FeuMeshLampe, FLinearColor(1.f, 0.45f, 0.f), false);   // 4 : orange
	CreerCoucheFeux(FeuMeshLampe, FLinearColor(1.f, 0.02f, 0.02f), false); // 5 : rouge

	// Hauteurs des lampes (cm au-dessus du centre du boitier) : rouge en haut, orange, vert en bas.
	static const float HauteurLampe[3] = { -30.f, 0.f, 30.f }; // vert, orange, rouge
	int32 Lampe = 0;
	for (FFeuTricolore& Feu : Feux)
	{
		const FVector Centre = Noeuds[Feu.Noeud].Position;
		for (int32 i = 0; i < Feu.Entrees.Num(); ++i)
		{
			const FTronconRoutier& T = Troncons[Feu.Entrees[i]];
			const FVector Dir = T.Direction.GetSafeNormal2D();
			const float DemiLargeur = T.Decalage > 0.f ? T.Decalage * 2.f : 300.f;
			// Poteau sur le trottoir de droite, a la ligne d'arret.
			FVector Pied = Centre - Dir * Feu.Retrait + Droite(Dir) * (DemiLargeur + 60.f);
			float Z = Centre.Z;
			if (TrouverSol(FVector(Pied.X, Pied.Y, Centre.Z), 600.f, Z)) { Pied.Z = Z; } else { Pied.Z = Centre.Z; }
			const FRotator Rot(0.f, Dir.Rotation().Yaw, 0.f);
			Poteaux->AddInstance(FTransform(Rot, Pied + FVector(0.f, 0.f, 170.f), FVector(0.14f, 0.14f, 3.4f)));
			const FVector Boite = Pied + FVector(0.f, 0.f, 380.f);
			Boitiers->AddInstance(FTransform(Rot, Boite, FVector(0.28f, 0.36f, 1.f)));
			for (int32 c = 0; c < 3; ++c)
			{
				const FVector P = Boite - Dir * 12.f + FVector(0.f, 0.f, HauteurLampe[c]);
				Eteintes->AddInstance(FTransform(Rot, P, FVector(0.22f)));
				const FTransform Allumee(Rot, Boite - Dir * 14.f + FVector(0.f, 0.f, HauteurLampe[c]), FVector(0.25f));
				PosesLampes.Add(Allumee);
				// au depart toutes eteintes (cachees dans le boitier)
				CouchesFeux[3 + c]->AddInstance(FTransform(Rot, Boite, FVector(0.001f)));
			}
			Feu.Lampes[i] = Lampe++;
			Feu.EtatAffiche[i] = 3;
		}
	}
	MettreAJourFeux();
}

uint8 ACirculationSpawner::EtatFeu(const FFeuTricolore& Feu, uint8 Phase, float Temps) const
{
	const float DemiCycle = FeuDureeVert + FeuDureeOrange + FeuDureeRougePartout;
	float t = FMath::Fmod(Temps + Feu.Decalage + (Phase == 1 ? DemiCycle : 0.f), 2.f * DemiCycle);
	if (t < 0.f) { t += 2.f * DemiCycle; }
	if (t < FeuDureeVert) { return 0; }
	if (t < FeuDureeVert + FeuDureeOrange) { return 1; }
	return 2;
}

void ACirculationSpawner::MettreAJourFeux()
{
	if (Feux.Num() == 0 || CouchesFeux.Num() < 6)
	{
		return;
	}
	const float Temps = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bool bChange[3] = { false, false, false };
	for (FFeuTricolore& Feu : Feux)
	{
		for (int32 i = 0; i < Feu.Entrees.Num(); ++i)
		{
			const int32 Lampe = Feu.Lampes[i];
			if (Lampe == INDEX_NONE) { continue; }
			const uint8 Etat = EtatFeu(Feu, Feu.Phases[i], Temps);
			const uint8 Avant = Feu.EtatAffiche[i];
			if (Etat == Avant) { continue; }
			if (Avant < 3)
			{
				FTransform Cachee = PosesLampes[Lampe * 3 + Avant];
				Cachee.SetScale3D(FVector(0.001f));
				CouchesFeux[3 + Avant]->UpdateInstanceTransform(Lampe, Cachee, false, false, true);
				bChange[Avant] = true;
			}
			CouchesFeux[3 + Etat]->UpdateInstanceTransform(Lampe, PosesLampes[Lampe * 3 + Etat], false, false, true);
			bChange[Etat] = true;
			Feu.EtatAffiche[i] = Etat;
		}
	}
	for (int32 c = 0; c < 3; ++c)
	{
		if (bChange[c] && IsValid(CouchesFeux[3 + c])) { CouchesFeux[3 + c]->MarkRenderStateDirty(); }
	}
}
