// CHELLES 3D - Ligne de train autour de la carte (voir TrainChelles.h)
#include "TrainChelles.h"
#include "ArretTrainChelles.h"
#include "Algo/BinarySearch.h"
#include "Components/BoxComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrainChelles, Log, All);

namespace
{
	constexpr float KmhVersCms = 100000.f / 3600.f;
	constexpr float Acceleration = 110.f;     // cm/s^2
	constexpr float Freinage = 200.f;         // cm/s^2 (arrivee douce a l'arret)
	constexpr float FreinageUrgence = 800.f;  // un joueur sur la voie
	constexpr int32 MaxPointsTrajet = 1800;   // le trajet est envoye aux joueurs en une fois
	constexpr float Ecartement = 143.5f;      // ecartement des rails (cm)
	constexpr float EspaceEntreVoitures = 70.f;

	const TCHAR* MeshLocomotive = TEXT("/Game/Intercity_Train/Meshes/SM_PowerCar.SM_PowerCar");
	const TCHAR* MeshVoiture = TEXT("/Game/Intercity_Train/Meshes/SM_IntermediateCar.SM_IntermediateCar");
	const TCHAR* MeshBogie = TEXT("/Game/Intercity_Train/Meshes/SM_Bogie.SM_Bogie");
	const TCHAR* MeshPorte = TEXT("/Game/Intercity_Train/Meshes/SM_Door.SM_Door");
	const TCHAR* MeshInterieurLoco = TEXT("/Game/Intercity_Train/Meshes/SM_Interior_PC.SM_Interior_PC");
	const TCHAR* MeshInterieurVoiture = TEXT("/Game/Intercity_Train/Meshes/SM_Interior_IC.SM_Interior_IC");
	const TCHAR* MeshPassagersLoco = TEXT("/Game/Intercity_Train/Meshes/SM_Passengers_PC.SM_Passengers_PC");
	const TCHAR* MeshPassagersVoiture = TEXT("/Game/Intercity_Train/Meshes/SM_Passengers_IC.SM_Passengers_IC");
	const TCHAR* MeshToit = TEXT("/Game/Intercity_Train/Meshes/SM_Roof_Equipment_Electric.SM_Roof_Equipment_Electric");
	const TCHAR* MeshConducteur = TEXT("/Game/Intercity_Train/Meshes/SM_Driver.SM_Driver");

	FReglagesTrain GReglages;
	bool GReglagesLus = false;

	bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	float Nombre(const FString& Texte)
	{
		return FCString::Atof(*Texte.Replace(TEXT(","), TEXT(".")));
	}

	FString CheminFichier(const TCHAR* Nom)
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesTrain"), Nom);
	}

	void ChargerReglages()
	{
		GReglagesLus = true;
		GReglages = FReglagesTrain();
		const FString Chemin = CheminFichier(TEXT("Train.txt"));
		TArray<FString> Lignes;
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			// jeu exporte sans Train.txt : les arrets de depart
			const TCHAR* ParDefaut[][2] = {
				{ TEXT("Marechal Foch"), TEXT("2 Avenue du Marechal Foch") },
				{ TEXT("Resistance 46"), TEXT("46 Avenue de la Resistance") },
				{ TEXT("Resistance 11"), TEXT("11 Avenue de la Resistance") },
				{ TEXT("Freres Verdeaux"), TEXT("4 Rue des Freres Verdeaux") },
				{ TEXT("Adolphe Besson"), TEXT("5 Rue Adolphe Besson") },
				{ TEXT("Ilette 1"), TEXT("1 Rue de l'Ilette") },
				{ TEXT("Ilette 22"), TEXT("22 Rue de l'Ilette") },
				{ TEXT("Rene Salle 10"), TEXT("10 Rue Rene Salle") },
				{ TEXT("Rene Salle 1"), TEXT("1 Rue Rene Salle") } };
			for (const auto& Ligne : ParDefaut)
			{
				FArretEcrit A;
				A.Nom = Ligne[0];
				A.Adresse = Ligne[1];
				GReglages.Arrets.Add(A);
			}
			UE_LOG(LogTrainChelles, Warning, TEXT("[Train] %s absent : reglages et arrets par defaut."), *Chemin);
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
			FReglagesTrain& R = GReglages;
			if (Cle == TEXT("Arret"))
			{
				TArray<FString> Morceaux;
				Valeur.ParseIntoArray(Morceaux, TEXT("|"), false);
				for (FString& M : Morceaux) { M.TrimStartAndEndInline(); }
				if (Morceaux.Num() == 0 || Morceaux[0].IsEmpty())
				{
					continue;
				}
				FArretEcrit A;
				A.Nom = Morceaux[0];
				A.Adresse = Morceaux.Num() > 1 ? Morceaux[1] : Morceaux[0];
				if (Morceaux.Num() >= 4 && !Morceaux[2].IsEmpty() && !Morceaux[3].IsEmpty())
				{
					A.bCoordonnees = true;
					A.Position = FVector2D(Nombre(Morceaux[2]), Nombre(Morceaux[3]));
				}
				R.Arrets.Add(A);
			}
			else if (Cle == TEXT("Actif")) R.bActif = EstOui(Valeur);
			else if (Cle == TEXT("RegrouperArretsProchesMetres")) R.RegrouperMetres = FMath::Clamp(Nombre(Valeur), 0.f, 1000.f);
			else if (Cle == TEXT("OrdreDesArrets")) R.bOrdreAuto = !Valeur.Equals(TEXT("Liste"), ESearchCase::IgnoreCase);
			else if (Cle == TEXT("DistanceMaxArretRouteMetres")) R.DistanceMaxArretRouteMetres = FMath::Clamp(Nombre(Valeur), 5.f, 500.f);
			else if (Cle == TEXT("NombreDeVoitures")) R.NombreDeVoitures = FMath::Clamp(FCString::Atoi(*Valeur), 1, 8);
			else if (Cle == TEXT("Echelle")) R.Echelle = FMath::Clamp(Nombre(Valeur), 0.3f, 2.f);
			else if (Cle == TEXT("VitesseKmh")) R.VitesseKmh = FMath::Clamp(Nombre(Valeur), 5.f, 120.f);
			else if (Cle == TEXT("AttentePortesOuvertesSecondes")) R.AttenteSecondes = FMath::Clamp(Nombre(Valeur), 3.f, 600.f);
			else if (Cle == TEXT("MilieuDeLaRoute")) R.bMilieuDeLaRoute = EstOui(Valeur);
			else if (Cle == TEXT("Rails")) R.bRails = EstOui(Valeur);
			else if (Cle == TEXT("Traverses")) R.bTraverses = EstOui(Valeur);
			else if (Cle == TEXT("Passagers")) R.bPassagers = EstOui(Valeur);
			else if (Cle == TEXT("RetirerLesVoitures")) R.bRetirerVoitures = EstOui(Valeur);
			else if (Cle == TEXT("Portes")) R.bPortesDeuxCotes = Valeur.Equals(TEXT("DeuxCotes"), ESearchCase::IgnoreCase);
			else if (Cle == TEXT("InverserLeSens")) R.bInverserSens = EstOui(Valeur);
			else if (Cle == TEXT("HauteurRails")) R.HauteurRails = FMath::Clamp(Nombre(Valeur), -300.f, 300.f);
			else if (Cle == TEXT("HauteurPlancher")) R.HauteurPlancher = Nombre(Valeur);
			else if (Cle == TEXT("Messages")) R.bMessages = EstOui(Valeur);
		}
		UE_LOG(LogTrainChelles, Log, TEXT("[Train] Reglages lus : %s (%d arret(s) ecrit(s), %d voiture(s), %.0f km/h, portes %.0f s)."),
			*Chemin, GReglages.Arrets.Num(), GReglages.NombreDeVoitures, GReglages.VitesseKmh, GReglages.AttenteSecondes);
	}

	/** Le nom ecrit sans accents dans Train.txt, affiche avec ses accents. */
	FString AvecAccents(FString Nom)
	{
		Nom.ReplaceInline(TEXT("Arret"), TEXT("Arr\u00EAt"), ESearchCase::CaseSensitive);
		Nom.ReplaceInline(TEXT("Marechal"), TEXT("Mar\u00E9chal"), ESearchCase::CaseSensitive);
		Nom.ReplaceInline(TEXT("Resistance"), TEXT("R\u00E9sistance"), ESearchCase::CaseSensitive);
		Nom.ReplaceInline(TEXT("Freres"), TEXT("Fr\u00E8res"), ESearchCase::CaseSensitive);
		Nom.ReplaceInline(TEXT("Ilette"), TEXT("\u00CElette"), ESearchCase::CaseSensitive);
		Nom.ReplaceInline(TEXT("Rene Salle"), TEXT("Ren\u00E9 Sall\u00E9"), ESearchCase::CaseSensitive);
		return Nom;
	}

	/** "Ilette 22" -> "Ilette" (quand deux arrets voisins sont regroupes). */
	FString SansNumero(FString Nom)
	{
		Nom.TrimEndInline();
		while (Nom.Len() > 0 && (FChar::IsDigit(Nom[Nom.Len() - 1]) || Nom[Nom.Len() - 1] == TEXT(' ')))
		{
			Nom.LeftChopInline(1);
		}
		return Nom.IsEmpty() ? TEXT("Arret") : Nom;
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
		bool bMilieu = true;

		FVector Point(int32 T, float S) const
		{
			const FTroncon& Tr = Troncons[T];
			const float Alpha = Tr.Longueur > 0.f ? FMath::Clamp(S / Tr.Longueur, 0.f, 1.f) : 0.f;
			const FVector P = FMath::Lerp(Noeuds[Tr.De], Noeuds[Tr.Vers], Alpha);
			return bMilieu ? P : P + Droite(Tr.Direction) * Tr.Decalage;
		}
	};

	UClass* ClasseSpawner()
	{
		return FindObject<UClass>(nullptr, TEXT("/Script/ChellesCirculation.CirculationSpawner"));
	}

	AActor* TrouverSpawner(UWorld* Monde)
	{
		UClass* Classe = ClasseSpawner();
		if (!Classe) { return nullptr; }
		for (TActorIterator<AActor> It(Monde, Classe); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	/** Meme conversion latitude/longitude -> carte que la circulation (et que chelles_complet.py). */
	struct FProjection
	{
		double Lat0 = 48.8795;
		double Lon0 = 2.5912;
		FVector2D Decalage = FVector2D::ZeroVector;

		FVector2D VersCarte(double Lat, double Lon) const
		{
			const double MLat = 111320.0;
			const double MLon = 111320.0 * FMath::Cos(static_cast<float>(Lat0 * PI / 180.0));
			return FVector2D(static_cast<float>((Lon - Lon0) * MLon * 100.0 + Decalage.X),
				static_cast<float>(-(Lat - Lat0) * MLat * 100.0 + Decalage.Y));
		}
	};

	FProjection LireProjection(UWorld* Monde)
	{
		FProjection P;
		P.Lat0 = ATrainChelles::Reglages().LatitudeCentre;
		P.Lon0 = ATrainChelles::Reglages().LongitudeCentre;
		AActor* Spawner = TrouverSpawner(Monde);
		UClass* Classe = ClasseSpawner();
		if (!Spawner || !Classe)
		{
			return P;
		}
		if (FDoubleProperty* Lat = FindFProperty<FDoubleProperty>(Classe, TEXT("LatitudeCentre")))
		{
			P.Lat0 = Lat->GetPropertyValue_InContainer(Spawner);
		}
		if (FDoubleProperty* Lon = FindFProperty<FDoubleProperty>(Classe, TEXT("LongitudeCentre")))
		{
			P.Lon0 = Lon->GetPropertyValue_InContainer(Spawner);
		}
		if (FStructProperty* Dec = FindFProperty<FStructProperty>(Classe, TEXT("DecalageDeLaCarte")))
		{
			if (Dec->Struct == TBaseStructure<FVector2D>::Get())
			{
				P.Decalage = *Dec->ContainerPtrToValuePtr<FVector2D>(Spawner);
			}
		}
		return P;
	}

	bool LireReseau(UWorld* Monde, FReseau& R)
	{
		UClass* Classe = ClasseSpawner();
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
			UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Le reseau routier de ChellesCirculation n'a pas la forme attendue."));
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
			for (const FReseau::FTroncon& T : R.Troncons)
			{
				if (!R.Noeuds.IsValidIndex(T.De) || !R.Noeuds.IsValidIndex(T.Vers))
				{
					R = FReseau();
					return false;
				}
			}
			for (FReseau::FTroncon& T : R.Troncons)
			{
				if (!R.Troncons.IsValidIndex(T.Inverse)) { T.Inverse = INDEX_NONE; }
			}
			for (TArray<int32>& S : R.Sorties)
			{
				S.RemoveAll([&R](int32 T) { return !R.Troncons.IsValidIndex(T); });
			}
			UE_LOG(LogTrainChelles, Log, TEXT("[Train] Reseau routier lu : %d points, %d troncons."), R.Noeuds.Num(), R.Troncons.Num());
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
	};

	/** Plus court chemin (en suivant le sens de circulation) d'un point de la route a un autre. */
	bool CheminEntre(const FReseau& R, const FArretSurRoute& A, const FArretSurRoute& B, TArray<FVector>& OutPoints)
	{
		OutPoints.Reset();
		if (A.Troncon == B.Troncon && B.S >= A.S + 100.f)
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
			Propres.Last() = Points.Last();
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
			if (Longueur < KINDA_SMALL_NUMBER) { continue; }
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

	UMaterialInstanceDynamic* MateriauCouleur(UObject* Proprietaire, const FLinearColor& Couleur)
	{
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (!Base) { return nullptr; }
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Proprietaire);
		if (Mid) { Mid->SetVectorParameterValue(TEXT("Color"), Couleur); }
		return Mid;
	}
}

// =====================================================================================================
// Acteur
// =====================================================================================================

const FReglagesTrain& ATrainChelles::Reglages()
{
	if (!GReglagesLus)
	{
		ChargerReglages();
	}
	return GReglages;
}

ATrainChelles::ATrainChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 10.f;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void ATrainChelles::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATrainChelles, Trajet);
	DOREPLIFETIME(ATrainChelles, ArretsInfos);
	DOREPLIFETIME(ATrainChelles, ParcouruServeur);
	DOREPLIFETIME(ATrainChelles, VitesseServeur);
	DOREPLIFETIME(ATrainChelles, Etat);
	DOREPLIFETIME(ATrainChelles, DepartDans);
	DOREPLIFETIME(ATrainChelles, ArretCible);
}

void ATrainChelles::BeginPlay()
{
	Super::BeginPlay();
	ChargerReglages(); // relu a chaque partie : on peut modifier Train.txt sans recompiler
	if (EstServeur() && Reglages().bActif)
	{
		LancerGeocodage();
	}
}

void ATrainChelles::EndPlay(const EEndPlayReason::Type Raison)
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
	RemettreToutesLesVoitures();
	for (const TWeakObjectPtr<AArretTrainChelles>& Panneau : PanneauxPoses)
	{
		if (Panneau.IsValid()) { Panneau->Destroy(); }
	}
	PanneauxPoses.Reset();
	Super::EndPlay(Raison);
}

void ATrainChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DeltaSeconds = FMath::Min(DeltaSeconds, 0.1f);
	if (!Reglages().bActif)
	{
		return;
	}
	if (EstServeur())
	{
		const float Maintenant = GetWorld()->GetTimeSeconds();
		if (!bTrajetConstruit && Maintenant >= ProchainEssaiTrajet && EssaisTrajet < 40)
		{
			// on attend les reponses de la Base Adresse Nationale (20 s au plus) et le reseau routier de la circulation
			if (RequetesEnCours <= 0 || Maintenant >= FinAttenteGeocodage)
			{
				ProchainEssaiTrajet = Maintenant + 1.f;
				++EssaisTrajet;
				bTrajetConstruit = ConstruireTrajet();
			}
		}
		if (bTrajetConstruit && LongueurTrajet > 0.f && ArretsInfos.Num() > 0)
		{
			TickServeur(DeltaSeconds);
		}
	}
	TickLocal(DeltaSeconds);
}

// =====================================================================================================
// Serveur : adresses -> positions (Base Adresse Nationale)
// =====================================================================================================

void ATrainChelles::LireArretsTrouves()
{
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *CheminFichier(TEXT("ArretsTrouves.txt"))))
	{
		return;
	}
	for (const FString& Ligne : Lignes)
	{
		if (Ligne.StartsWith(TEXT("#"))) { continue; }
		TArray<FString> M;
		Ligne.ParseIntoArray(M, TEXT("|"), false);
		if (M.Num() < 3) { continue; }
		for (FString& X : M) { X.TrimStartAndEndInline(); }
		AdressesTrouvees.Add(M[0].ToLower(), FVector2D(Nombre(M[1]), Nombre(M[2])));
		if (M.Num() > 3) { LibellesTrouves.Add(M[0].ToLower(), M[3]); }
	}
}

void ATrainChelles::EcrireArretsTrouves() const
{
	FString Texte = TEXT("# CHELLES 3D - adresses des arrets de train trouvees par la Base Adresse Nationale (rempli tout seul).\r\n")
		TEXT("# adresse | latitude | longitude | adresse officielle trouvee\r\n")
		TEXT("# Si un arret est mal place : supprimez sa ligne ici, corrigez l'adresse dans Train.txt, ou donnez X | Y dans Train.txt.\r\n");
	for (const TPair<FString, FVector2D>& Paire : AdressesTrouvees)
	{
		const FString* Libelle = LibellesTrouves.Find(Paire.Key);
		Texte += FString::Printf(TEXT("%s | %.7f | %.7f | %s\r\n"), *Paire.Key, Paire.Value.X, Paire.Value.Y, Libelle ? **Libelle : TEXT(""));
	}
	FFileHelper::SaveStringToFile(Texte, *CheminFichier(TEXT("ArretsTrouves.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void ATrainChelles::LancerGeocodage()
{
	if (bGeocodageLance) { return; }
	bGeocodageLance = true;
	LireArretsTrouves();
	FinAttenteGeocodage = GetWorld()->GetTimeSeconds() + 20.f;
	int32 ADemander = 0;
	for (const FArretEcrit& A : Reglages().Arrets)
	{
		if (A.bCoordonnees || AdressesTrouvees.Contains(A.Adresse.ToLower()))
		{
			continue;
		}
		DemanderAdresse(A.Adresse, 0);
		++ADemander;
	}
	if (ADemander > 0)
	{
		UE_LOG(LogTrainChelles, Log, TEXT("[Train] Recherche de %d adresse(s) dans la Base Adresse Nationale (Internet)..."), ADemander);
	}
}

void ATrainChelles::DemanderAdresse(const FString& Adresse, int32 Essai)
{
	// 0 = Geoplateforme de l'IGN (nouvelle adresse de la BAN), 1 = ancienne adresse api-adresse.data.gouv.fr
	const FString Recherche = FGenericPlatformHttp::UrlEncode(Adresse + TEXT(" 77500 Chelles"));
	const FString Url = (Essai == 0)
		? FString::Printf(TEXT("https://data.geopf.fr/geocodage/search?index=address&limit=1&q=%s"), *Recherche)
		: FString::Printf(TEXT("https://api-adresse.data.gouv.fr/search/?limit=1&q=%s"), *Recherche);
	auto Requete = FHttpModule::Get().CreateRequest();
	Requete->SetURL(Url);
	Requete->SetVerb(TEXT("GET"));
	Requete->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Requete->OnProcessRequestComplete().BindUObject(this, &ATrainChelles::ReponseAdresse, Adresse, Essai);
	if (Requete->ProcessRequest())
	{
		++RequetesEnCours;
	}
	else
	{
		UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Impossible d'interroger %s"), *Url);
	}
}

void ATrainChelles::ReponseAdresse(FHttpRequestPtr Requete, FHttpResponsePtr Reponse, bool bReussi, FString Adresse, int32 Essai)
{
	RequetesEnCours = FMath::Max(0, RequetesEnCours - 1);
	bool bTrouve = false;
	if (bReussi && Reponse.IsValid() && Reponse->GetResponseCode() == 200)
	{
		TSharedPtr<FJsonObject> Racine;
		const TSharedRef<TJsonReader<>> Lecteur = TJsonReaderFactory<>::Create(Reponse->GetContentAsString());
		const TArray<TSharedPtr<FJsonValue>>* Resultats = nullptr;
		if (FJsonSerializer::Deserialize(Lecteur, Racine) && Racine.IsValid() && Racine->TryGetArrayField(TEXT("features"), Resultats)
			&& Resultats && Resultats->Num() > 0)
		{
			const TSharedPtr<FJsonObject> Premier = (*Resultats)[0]->AsObject();
			const TSharedPtr<FJsonObject>* Geometrie = nullptr;
			const TSharedPtr<FJsonObject>* Proprietes = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Coordonnees = nullptr;
			if (Premier.IsValid() && Premier->TryGetObjectField(TEXT("geometry"), Geometrie) && Geometrie
				&& (*Geometrie)->TryGetArrayField(TEXT("coordinates"), Coordonnees) && Coordonnees && Coordonnees->Num() >= 2)
			{
				const double Lon = (*Coordonnees)[0]->AsNumber();
				const double Lat = (*Coordonnees)[1]->AsNumber();
				FString Libelle, Ville, CodeInsee;
				if (Premier->TryGetObjectField(TEXT("properties"), Proprietes) && Proprietes)
				{
					(*Proprietes)->TryGetStringField(TEXT("label"), Libelle);
					(*Proprietes)->TryGetStringField(TEXT("city"), Ville);
					(*Proprietes)->TryGetStringField(TEXT("citycode"), CodeInsee);
				}
				if (CodeInsee == TEXT("77108") || Ville.Contains(TEXT("Chelles")) || Libelle.Contains(TEXT("Chelles")))
				{
					AdressesTrouvees.Add(Adresse.ToLower(), FVector2D(static_cast<float>(Lat), static_cast<float>(Lon)));
					LibellesTrouves.Add(Adresse.ToLower(), Libelle);
					bTrouve = true;
					UE_LOG(LogTrainChelles, Log, TEXT("[Train] Adresse trouvee : %s -> %s (%.6f, %.6f)"), *Adresse, *Libelle, Lat, Lon);
				}
				else
				{
					UE_LOG(LogTrainChelles, Warning, TEXT("[Train] '%s' : resultat hors de Chelles (%s), ignore."), *Adresse, *Libelle);
				}
			}
		}
	}
	if (!bTrouve)
	{
		if (Essai == 0)
		{
			DemanderAdresse(Adresse, 1); // on essaie l'autre adresse de la BAN
		}
		else
		{
			UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Adresse introuvable : '%s' (pas d'Internet ? adresse mal ecrite ?). Cet arret est ignore."), *Adresse);
		}
	}
	if (RequetesEnCours == 0)
	{
		EcrireArretsTrouves();
	}
}

// =====================================================================================================
// Serveur : trajet
// =====================================================================================================

bool ATrainChelles::ConstruireTrajet()
{
	const FReglagesTrain& R = Reglages();
	UWorld* Monde = GetWorld();

	// 1. Le reseau routier de la circulation (il peut etre pret une ou deux secondes apres le debut)
	FReseau Reseau;
	Reseau.bMilieu = R.bMilieuDeLaRoute;
	if (!LireReseau(Monde, Reseau))
	{
		if (EssaisTrajet == 1 || EssaisTrajet % 10 == 0)
		{
			UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Pas encore de reseau routier (Circulation Spawner absent de la carte ?). Nouvel essai..."));
		}
		return false;
	}
	EssaisTrajet = 1000; // plus d'autre essai apres celui-ci
	const FProjection Projection = LireProjection(Monde);

	if (!bTrainPrepare)
	{
		PreparerTrain(); // il faut connaitre la longueur et la largeur du train
	}
	const float DemiLargeur = Voitures.Num() > 0 ? Voitures[0].Boite.Max.Y * R.Echelle : 150.f;

	// 2. Les arrets : ceux de Train.txt + ceux poses a la main dans la carte
	struct FCandidat
	{
		FString Nom;
		FVector2D P;
		bool bManuel = false;
		int32 Nombre = 1;
	};
	TArray<FCandidat> Candidats;
	for (const FArretEcrit& A : R.Arrets)
	{
		FCandidat C;
		C.Nom = A.Nom;
		if (A.bCoordonnees)
		{
			C.P = A.Position;
		}
		else if (const FVector2D* LatLon = AdressesTrouvees.Find(A.Adresse.ToLower()))
		{
			C.P = Projection.VersCarte(LatLon->X, LatLon->Y);
		}
		else
		{
			continue; // adresse pas trouvee (deja signale)
		}
		Candidats.Add(C);
	}
	for (TActorIterator<AArretTrainChelles> It(Monde); It; ++It)
	{
		if (!It->bAutomatique && It->bActif)
		{
			FCandidat C;
			C.Nom = It->NomArret;
			const FVector Ici = It->GetActorLocation() + It->GetActorForwardVector().GetSafeNormal2D() * 300.f;
			C.P = FVector2D(Ici.X, Ici.Y);
			C.bManuel = true;
			Candidats.Add(C);
		}
	}

	// 3. Arrets voisins regroupes en un seul
	if (R.RegrouperMetres > 0.f)
	{
		TArray<FCandidat> Groupes;
		TArray<bool> Pris;
		Pris.Init(false, Candidats.Num());
		for (int32 i = 0; i < Candidats.Num(); ++i)
		{
			if (Pris[i]) { continue; }
			Pris[i] = true;
			FCandidat G = Candidats[i];
			FVector2D Somme = G.P;
			for (int32 j = i + 1; j < Candidats.Num(); ++j)
			{
				if (!Pris[j] && FVector2D::Distance(Candidats[i].P, Candidats[j].P) < R.RegrouperMetres * 100.f)
				{
					Pris[j] = true;
					Somme += Candidats[j].P;
					++G.Nombre;
					G.bManuel &= Candidats[j].bManuel;
				}
			}
			if (G.Nombre > 1)
			{
				UE_LOG(LogTrainChelles, Log, TEXT("[Train] %d arrets proches regroupes : '%s'."), G.Nombre, *SansNumero(G.Nom));
				G.P = Somme / static_cast<float>(G.Nombre);
				G.Nom = SansNumero(G.Nom);
				G.bManuel = false;
			}
			Groupes.Add(G);
		}
		Candidats = MoveTemp(Groupes);
	}

	// 4. Chaque arret est rattache a la route la plus proche (deux sens possibles)
	struct FArretPlace
	{
		FString Nom;
		bool bManuel = false;
		TArray<FArretSurRoute, TInlineAllocator<2>> Sens;
	};
	TArray<FArretPlace> Places;
	for (const FCandidat& C : Candidats)
	{
		const FVector Ici(C.P.X, C.P.Y, 0.f);
		float Meilleure = TNumericLimits<float>::Max();
		FArretSurRoute A;
		for (int32 T = 0; T < Reseau.Troncons.Num(); ++T)
		{
			const FReseau::FTroncon& Tr = Reseau.Troncons[T];
			if (Tr.Longueur < 1.f) { continue; }
			const FVector P0 = Reseau.Point(T, 0.f);
			const FVector P1 = Reseau.Point(T, Tr.Longueur);
			const FVector Seg(P1.X - P0.X, P1.Y - P0.Y, 0.f);
			const float L2 = Seg.SizeSquared();
			if (L2 < 1.f) { continue; }
			const float U = FMath::Clamp(FVector::DotProduct(FVector(Ici.X - P0.X, Ici.Y - P0.Y, 0.f), Seg) / L2, 0.f, 1.f);
			const FVector P = FMath::Lerp(P0, P1, U);
			const float D = FVector::Dist2D(Ici, P);
			if (D < Meilleure)
			{
				Meilleure = D;
				A.Troncon = T;
				A.S = U * Tr.Longueur;
				A.Point = Reseau.Point(T, A.S);
			}
		}
		if (A.Troncon == INDEX_NONE || Meilleure > R.DistanceMaxArretRouteMetres * 100.f)
		{
			UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Arret '%s' ignore : a %.0f m de la route la plus proche (hors de la carte ?). Position X=%.0f Y=%.0f."),
				*C.Nom, Meilleure / 100.f, C.P.X, C.P.Y);
			continue;
		}
		FArretPlace Place;
		Place.Nom = C.Nom;
		Place.bManuel = C.bManuel;
		Place.Sens.Add(A);
		const int32 Inv = Reseau.Troncons[A.Troncon].Inverse;
		if (Inv != INDEX_NONE)
		{
			FArretSurRoute B;
			B.Troncon = Inv;
			B.S = FMath::Clamp(Reseau.Troncons[Inv].Longueur - A.S, 0.f, Reseau.Troncons[Inv].Longueur);
			B.Point = Reseau.Point(Inv, B.S);
			Place.Sens.Add(B);
		}
		UE_LOG(LogTrainChelles, Log, TEXT("[Train] Arret '%s' : a %.0f m de la route."), *C.Nom, Meilleure / 100.f);
		Places.Add(Place);
	}
	if (Places.Num() < 2)
	{
		UE_LOG(LogTrainChelles, Error, TEXT("[Train] Il faut au moins 2 arrets sur la carte (trouves : %d). Pas de train. Voir les lignes [Train] ci-dessus."), Places.Num());
		return false;
	}

	// 5. Ordre : le tour de la carte dans le sens des aiguilles d'une montre (ou l'ordre de Train.txt)
	const int32 N = Places.Num();
	if (R.bOrdreAuto)
	{
		FVector Centre = FVector::ZeroVector;
		for (const FArretPlace& P : Places) { Centre += P.Sens[0].Point; }
		Centre /= static_cast<float>(N);
		// X = est, Y = sud dans la carte : angle "vu du ciel, nord en haut" = atan2(-Y, X) ; aiguilles d'une montre = angle qui diminue
		Places.Sort([&Centre](const FArretPlace& A, const FArretPlace& B)
		{
			const float AA = FMath::Atan2(-(A.Sens[0].Point.Y - Centre.Y), A.Sens[0].Point.X - Centre.X);
			const float AB = FMath::Atan2(-(B.Sens[0].Point.Y - Centre.Y), B.Sens[0].Point.X - Centre.X);
			return AA > AB;
		});
	}

	// 6. Pour chaque arret, le sens de la rue qui donne la boucle la plus courte
	TMap<int32, TArray<FVector>> Chemins; // cle = ((k*2+ca)*2+cb)
	auto Morceau = [&](int32 k, int32 ca, int32 cb) -> const TArray<FVector>*
	{
		const int32 Cle = (k * 2 + ca) * 2 + cb;
		if (const TArray<FVector>* Deja = Chemins.Find(Cle)) { return Deja->Num() > 0 ? Deja : nullptr; }
		TArray<FVector>& Points = Chemins.Add(Cle);
		const FArretPlace& A = Places[k];
		const FArretPlace& B = Places[(k + 1) % N];
		if (!CheminEntre(Reseau, A.Sens[ca], B.Sens[cb], Points)) { Points.Reset(); return nullptr; }
		return &Points;
	};
	auto Cout = [&](int32 k, int32 ca, int32 cb) -> float
	{
		const TArray<FVector>* P = Morceau(k, ca, cb);
		return P ? LongueurPolyligne(*P) : 1.e12f;
	};
	TArray<int32> Choix;
	float MeilleurTotal = TNumericLimits<float>::Max();
	for (int32 c0 = 0; c0 < Places[0].Sens.Num(); ++c0)
	{
		TArray<TArray<float>> Meilleur;
		TArray<TArray<int32>> Parent;
		Meilleur.SetNum(N);
		Parent.SetNum(N);
		Meilleur[0].Init(1.e30f, 2);
		Meilleur[0][c0] = 0.f;
		Parent[0].Init(INDEX_NONE, 2);
		for (int32 k = 1; k < N; ++k)
		{
			Meilleur[k].Init(1.e30f, 2);
			Parent[k].Init(INDEX_NONE, 2);
			for (int32 c = 0; c < Places[k].Sens.Num(); ++c)
			{
				for (int32 cp = 0; cp < Places[k - 1].Sens.Num(); ++cp)
				{
					if (Meilleur[k - 1][cp] >= 1.e29f) { continue; }
					const float V = Meilleur[k - 1][cp] + Cout(k - 1, cp, c);
					if (V < Meilleur[k][c]) { Meilleur[k][c] = V; Parent[k][c] = cp; }
				}
			}
		}
		for (int32 c = 0; c < Places[N - 1].Sens.Num(); ++c)
		{
			if (Meilleur[N - 1][c] >= 1.e29f) { continue; }
			const float Total = Meilleur[N - 1][c] + Cout(N - 1, c, c0);
			if (Total < MeilleurTotal)
			{
				MeilleurTotal = Total;
				Choix.Init(0, N);
				Choix[N - 1] = c;
				for (int32 k = N - 1; k > 0; --k) { Choix[k - 1] = Parent[k][Choix[k]]; }
				Choix[0] = c0;
			}
		}
	}
	if (Choix.Num() != N)
	{
		Choix.Init(0, N);
	}

	// 7. Les morceaux de trajet d'un arret au suivant (boucle fermee)
	TArray<TArray<FVector>> Morceaux;
	Morceaux.SetNum(N);
	float LongueurTotale = 0.f;
	for (int32 k = 0; k < N; ++k)
	{
		const TArray<FVector>* P = Morceau(k, Choix[k], Choix[(k + 1) % N]);
		if (P)
		{
			Morceaux[k] = *P;
		}
		else
		{
			UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Pas de route de '%s' vers '%s' : ligne droite."), *Places[k].Nom, *Places[(k + 1) % N].Nom);
			Morceaux[k] = { Places[k].Sens[Choix[k]].Point, Places[(k + 1) % N].Sens[Choix[(k + 1) % N]].Point };
		}
		RetirerDoublons(Morceaux[k]);
		Arrondir(Morceaux[k], 3); // virages bien ronds pour un long train
		LongueurTotale += LongueurPolyligne(Morceaux[k]);
	}
	if (LongueurTotale < 1000.f)
	{
		UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Trajet trop court (%.0f m)."), LongueurTotale / 100.f);
		return false;
	}
	const float Pas = FMath::Max(200.f, LongueurTotale / (MaxPointsTrajet - N - 1));
	TArray<FVector_NetQuantize> Points;
	TArray<int32> IndexArrets;
	for (int32 k = 0; k < N; ++k)
	{
		TArray<FVector> Regulier;
		Reechantillonner(Morceaux[k], Pas, Regulier);
		IndexArrets.Add(Points.Num());
		for (int32 i = 0; i < Regulier.Num(); ++i)
		{
			if (i == Regulier.Num() - 1) { continue; } // = premier point du morceau suivant (ou du premier pour fermer la boucle)
			Points.Add(FVector_NetQuantize(Regulier[i]));
		}
	}
	if (Points.Num() > 1)
	{
		const FVector_NetQuantize Premier = Points[0]; // copie : Add() d'un element du meme tableau fait planter Unreal
		Points.Add(Premier); // boucle fermee
	}
	Trajet = MoveTemp(Points);
	OnRep_Trajet();
	if (LongueurTrajet <= 0.f)
	{
		return false;
	}

	// 8. Les arrets : la tete du train s'arrete pour que le MILIEU du train soit devant le panneau
	TArray<FArretTrainInfo> Infos;
	FString Liste;
	for (int32 k = 0; k < N; ++k)
	{
		const float D = Cumuls[IndexArrets[k]];
		FArretTrainInfo Info;
		Info.Nom = Places[k].Nom;
		Info.bManuel = Places[k].bManuel;
		Info.DistanceTete = Boucler(D + LongueurTrain * 0.5f);
		const FVector Sur = PointA(D);
		const FVector Dir = DirectionA(D);
		FVector Panneau = Sur + Droite(Dir) * (DemiLargeur + 230.f);
		float Z = Sur.Z;
		SolSous(Panneau, Sur.Z + 100.f, Z);
		Panneau.Z = Z;
		Info.Position = Panneau;
		Info.Lacet = (-Droite(Dir)).Rotation().Yaw; // le panneau regarde la voie
		Infos.Add(Info);
		Liste += (Liste.IsEmpty() ? TEXT("") : TEXT(" > ")) + Info.Nom;
	}
	ArretsInfos = MoveTemp(Infos);
	OnRep_Arrets();

	// 9. Le train attend au premier arret, portes ouvertes
	ArretCible = 0;
	Parcouru = ArretsInfos[0].DistanceTete;
	ParcouruServeur = Parcouru;
	ParcouruLocal = Parcouru;
	Vitesse = 0.f;
	Etat = 1;
	TempsALArret = 0.f;
	for (FVoitureTrain& V : Voitures) { V.bZInitialise = false; }
	PlacerTrain(Parcouru, 0.f, true);
	UE_LOG(LogTrainChelles, Log, TEXT("[Train] Ligne prete : %d arret(s), boucle de %.0f m, train de %.0f m (%s)."),
		N, LongueurTrajet / 100.f, LongueurTrain / 100.f, *Liste);
	return true;
}

void ATrainChelles::ListerJoueurs(TArray<APawn*>& OutJoueurs) const
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

bool ATrainChelles::JoueurDevantLeTrain() const
{
	if (Voitures.Num() == 0 || !Voitures[0].Racine.IsValid())
	{
		return false;
	}
	const FVoitureTrain& Tete = Voitures[0];
	const FTransform Repere = Tete.Racine->GetComponentTransform();
	const float E = FMath::Max(0.1f, Reglages().Echelle);
	TArray<APawn*> Joueurs;
	ListerJoueurs(Joueurs);
	for (APawn* Joueur : Joueurs)
	{
		const FVector L = Repere.InverseTransformPosition(Joueur->GetActorLocation());
		if (L.X > Tete.Boite.Max.X - 20.f && L.X < Tete.Boite.Max.X + 1000.f / E
			&& FMath::Abs(L.Y) < Tete.Boite.Max.Y + 60.f / E
			&& L.Z > -200.f / E && L.Z < Tete.Boite.Max.Z)
		{
			return true;
		}
	}
	return false;
}

void ATrainChelles::TickServeur(float DeltaSeconds)
{
	const int32 N = ArretsInfos.Num();
	const FReglagesTrain& R = Reglages();
	const float Maintenant = GetWorld()->GetTimeSeconds();
	ArretCible = FMath::Clamp(ArretCible, 0, N - 1);

	switch (Etat)
	{
	case 0: // roule vers le prochain arret
	{
		const float Cible = ArretsInfos[ArretCible].DistanceTete;
		const float Reste = Boucler(Cible - Parcouru);

		// on ralentit dans les virages : on regarde la route sous tout le train et 40 m devant
		float VMax = R.VitesseKmh * KmhVersCms;
		float AngleMax = 0.f;
		const FVector DTete = DirectionA(Parcouru);
		for (float D = -LongueurTrain; D <= 4000.f; D += 500.f)
		{
			const FVector DD = DirectionA(Parcouru + D);
			AngleMax = FMath::Max(AngleMax, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(DTete, DD), -1.f, 1.f))));
		}
		if (AngleMax > 60.f) { VMax = FMath::Min(VMax, 250.f); }
		else if (AngleMax > 25.f) { VMax = FMath::Min(VMax, 420.f); }

		float VCible = FMath::Min(VMax, FMath::Sqrt(2.f * Freinage * FMath::Max(0.f, Reste - 10.f)) + 30.f);
		float Frein = Freinage;
		if (JoueurDevantLeTrain())
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
			Etat = 1; // portes ouvertes
			UE_LOG(LogTrainChelles, Verbose, TEXT("[Train] Arret %s."), *ArretsInfos[ArretCible].Nom);
		}
		break;
	}
	case 1: // a l'arret, portes ouvertes
		TempsALArret += DeltaSeconds;
		DepartDans = FMath::Max(0.f, R.AttenteSecondes - TempsALArret);
		if (TempsALArret >= R.AttenteSecondes)
		{
			Etat = 2;
			FinFermeture = Maintenant + 3.f;
			DepartDans = 0.f;
		}
		break;
	case 2: // fermeture des portes puis depart
		if (Maintenant >= FinFermeture)
		{
			ArretCible = (ArretCible + 1) % N;
			Parcouru = Boucler(Parcouru + 1.f);
			Etat = 0;
		}
		break;
	default:
		Etat = 0;
		break;
	}
	ParcouruServeur = Parcouru;
	VitesseServeur = (Etat == 0) ? Vitesse : 0.f;
}

// =====================================================================================================
// Chaque machine : le train, les rails, les panneaux, les voitures, l'ecran
// =====================================================================================================

void ATrainChelles::OnRep_Trajet()
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
		for (FVoitureTrain& V : Voitures) { V.bZInitialise = false; }
	}
}

void ATrainChelles::OnRep_Arrets()
{
	PoserPanneaux();
}

float ATrainChelles::Boucler(float S) const
{
	if (LongueurTrajet <= 0.f)
	{
		return 0.f;
	}
	S = FMath::Fmod(S, LongueurTrajet);
	return S < 0.f ? S + LongueurTrajet : S;
}

FVector ATrainChelles::PointA(float S) const
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

FVector ATrainChelles::DirectionA(float S) const
{
	FVector D = PointA(S + 300.f) - PointA(S - 300.f);
	D.Z = 0.f;
	return D.IsNearlyZero() ? FVector::ForwardVector : D.GetSafeNormal();
}

bool ATrainChelles::SolSous(const FVector& P, float ZIndice, float& OutZ) const
{
	FHitResult Impact;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SolTrainChelles), false, this);
	for (const TWeakObjectPtr<AArretTrainChelles>& Panneau : PanneauxPoses)
	{
		if (Panneau.IsValid()) { Params.AddIgnoredActor(Panneau.Get()); }
	}
	if (GetWorld()->LineTraceSingleByObjectType(Impact, FVector(P.X, P.Y, ZIndice + 250.f), FVector(P.X, P.Y, ZIndice - 1200.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutZ = Impact.ImpactPoint.Z;
		return true;
	}
	return false;
}

void ATrainChelles::PreparerTrain()
{
	bTrainPrepare = true;
	const FReglagesTrain& R = Reglages();
	UStaticMesh* Loco = LoadObject<UStaticMesh>(nullptr, MeshLocomotive);
	UStaticMesh* Voiture = LoadObject<UStaticMesh>(nullptr, MeshVoiture);
	if (!Loco)
	{
		UE_LOG(LogTrainChelles, Error, TEXT("[Train] Modele du train introuvable : %s (le pack Intercity_Train est-il dans Content ?)."), MeshLocomotive);
		return;
	}
	if (!Voiture) { Voiture = Loco; }

	const int32 N = R.NombreDeVoitures;
	for (int32 i = 0; i < N; ++i)
	{
		const bool bQueue = (N >= 2 && i == N - 1);
		const bool bLoco = (i == 0) || bQueue;
		CreerVoiture(i, bLoco ? Loco : Voiture, bQueue, bLoco);
	}
	float Decalage = 0.f;
	for (FVoitureTrain& V : Voitures)
	{
		V.Decalage = Decalage;
		Decalage += V.Longueur + EspaceEntreVoitures * R.Echelle;
	}
	LongueurTrain = Voitures.Num() > 0 ? Voitures.Last().Decalage + Voitures.Last().Longueur : 0.f;
	int32 NbPortes = 0;
	for (const FVoitureTrain& V : Voitures) { NbPortes += V.Portes.Num(); }
	UE_LOG(LogTrainChelles, Log, TEXT("[Train] Train pret : %d voiture(s), %.1f m de long, %d porte(s), plancher a %.0f cm."),
		Voitures.Num(), LongueurTrain / 100.f, NbPortes, Voitures.Num() > 0 ? Voitures[0].Plancher * R.Echelle : 0.f);
}

void ATrainChelles::CreerVoiture(int32 Index, UStaticMesh* Corps, bool bRetournee, bool bLocomotive)
{
	const FReglagesTrain& R = Reglages();
	const float E = R.Echelle;
	FVoitureTrain V;

	USceneComponent* Racine = NewObject<USceneComponent>(this, *FString::Printf(TEXT("VoitureTrain_%d"), Index));
	Racine->SetMobility(EComponentMobility::Movable);
	Racine->SetupAttachment(RootComponent);
	Racine->SetUsingAbsoluteLocation(true);
	Racine->SetUsingAbsoluteRotation(true);
	Racine->SetUsingAbsoluteScale(true);
	Racine->RegisterComponent();
	Racine->SetWorldScale3D(FVector(E));
	V.Racine = Racine;

	// --- Le modele dans le repere de la voiture : X = avant du train, Y = droite, Z = 0 au niveau de la route
	const FBox BoiteMesh = Corps->GetBoundingBox();
	const bool bLongEnY = BoiteMesh.GetSize().Y > BoiteMesh.GetSize().X;
	float Lacet = bLongEnY ? -90.f : 0.f;
	// l'avant de la locomotive : un "socket" de phare / cabine s'il y en a un
	for (const UStaticMeshSocket* Socket : Corps->Sockets)
	{
		if (!Socket) { continue; }
		const FString Nom = Socket->SocketName.ToString();
		if (Nom.Contains(TEXT("Headlight")) || Nom.Contains(TEXT("Front")) || Nom.Contains(TEXT("Cab")))
		{
			const float Axe = bLongEnY ? Socket->RelativeLocation.Y - BoiteMesh.GetCenter().Y : Socket->RelativeLocation.X - BoiteMesh.GetCenter().X;
			if (Axe < 0.f) { Lacet += 180.f; }
			break;
		}
	}
	if (bRetournee) { Lacet += 180.f; }
	if (R.bInverserSens) { Lacet += 180.f; }
	const FRotator Rot(0.f, Lacet, 0.f);
	const FVector Centre = BoiteMesh.GetCenter();
	FVector Origine = -Rot.RotateVector(FVector(Centre.X, Centre.Y, 0.f));
	Origine.Z = R.HauteurRails / E;
	const FTransform RepereMesh(Rot, Origine);

	auto AjouterMesh = [this, Racine, Index](UStaticMesh* Mesh, const FTransform& Relatif, const TCHAR* Nom, bool bCollision) -> UStaticMeshComponent*
	{
		if (!Mesh) { return nullptr; }
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("%s_%d_%d"), Nom, Index, Racine->GetNumChildrenComponents()));
		C->SetMobility(EComponentMobility::Movable);
		C->SetupAttachment(Racine);
		C->SetStaticMesh(Mesh);
		C->SetRelativeTransform(Relatif);
		C->SetCanEverAffectNavigation(false);
		C->CanCharacterStepUpOn = ECB_No;
		if (bCollision)
		{
			// la carrosserie arrete les balles, mais pas les joueurs (des murs invisibles s'en chargent, avec les portes)
			C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			C->SetCollisionResponseToAllChannels(ECR_Block);
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		C->RegisterComponent();
		return C;
	};

	AjouterMesh(Corps, RepereMesh, TEXT("Caisse"), true);
	AjouterMesh(LoadObject<UStaticMesh>(nullptr, bLocomotive ? MeshInterieurLoco : MeshInterieurVoiture, nullptr, LOAD_NoWarn | LOAD_Quiet), RepereMesh, TEXT("Interieur"), false);
	if (R.bPassagers)
	{
		AjouterMesh(LoadObject<UStaticMesh>(nullptr, bLocomotive ? MeshPassagersLoco : MeshPassagersVoiture, nullptr, LOAD_NoWarn | LOAD_Quiet), RepereMesh, TEXT("Passagers"), false);
	}
	if (bLocomotive)
	{
		AjouterMesh(LoadObject<UStaticMesh>(nullptr, MeshToit, nullptr, LOAD_NoWarn | LOAD_Quiet), RepereMesh, TEXT("Toit"), false);
		if (R.bPassagers)
		{
			AjouterMesh(LoadObject<UStaticMesh>(nullptr, MeshConducteur, nullptr, LOAD_NoWarn | LOAD_Quiet), RepereMesh, TEXT("Conducteur"), false);
		}
	}

	V.Boite = BoiteMesh.TransformBy(RepereMesh);
	V.Longueur = V.Boite.GetSize().X * E;
	const float DemiLong = V.Boite.GetExtent().X;

	// --- Bogies (roues) : aux "sockets" Bogie du modele, sinon a 70 % de la demi-longueur
	TArray<float> PositionsBogies;
	for (const UStaticMeshSocket* Socket : Corps->Sockets)
	{
		if (Socket && Socket->SocketName.ToString().Contains(TEXT("Bogie")))
		{
			PositionsBogies.Add(RepereMesh.TransformPosition(Socket->RelativeLocation).X);
		}
	}
	if (PositionsBogies.Num() < 2)
	{
		PositionsBogies = { V.Boite.GetCenter().X + DemiLong * 0.7f, V.Boite.GetCenter().X - DemiLong * 0.7f };
	}
	float SommeEcart = 0.f;
	for (float X : PositionsBogies) { SommeEcart += FMath::Abs(X - V.Boite.GetCenter().X); }
	V.DemiEcartBogies = FMath::Max(200.f, SommeEcart / PositionsBogies.Num()) * E;
	if (UStaticMesh* Bogie = LoadObject<UStaticMesh>(nullptr, MeshBogie, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		const FBox BB = Bogie->GetBoundingBox();
		const FRotator RotB(0.f, BB.GetSize().Y > BB.GetSize().X ? -90.f : 0.f, 0.f);
		for (float X : PositionsBogies)
		{
			FVector Pos = FVector(X, 0.f, Origine.Z) - RotB.RotateVector(FVector(BB.GetCenter().X, BB.GetCenter().Y, 0.f));
			AjouterMesh(Bogie, FTransform(RotB, Pos), TEXT("Bogie"), false);
		}
	}

	// --- Portes : aux "sockets" Door du modele
	UStaticMesh* MeshDePorte = LoadObject<UStaticMesh>(nullptr, MeshPorte, nullptr, LOAD_NoWarn | LOAD_Quiet);
	struct FPorteBrute { FTransform Relatif; FBox Boite; };
	TArray<FPorteBrute> Brutes;
	for (const UStaticMeshSocket* Socket : Corps->Sockets)
	{
		if (!Socket || !Socket->SocketName.ToString().Contains(TEXT("Door"))) { continue; }
		FPorteBrute P;
		P.Relatif = FTransform(Socket->RelativeRotation, Socket->RelativeLocation, Socket->RelativeScale) * RepereMesh;
		P.Boite = MeshDePorte ? MeshDePorte->GetBoundingBox().TransformBy(P.Relatif) : FBox(P.Relatif.GetLocation() - FVector(65.f, 5.f, 0.f), P.Relatif.GetLocation() + FVector(65.f, 5.f, 200.f));
		Brutes.Add(P);
	}
	if (Brutes.Num() == 0)
	{
		// pas de socket : deux portes de chaque cote
		for (float Signe : { 0.5f, -0.5f })
		{
			for (float Cote : { 1.f, -1.f })
			{
				FPorteBrute P;
				const FVector C(V.Boite.GetCenter().X + DemiLong * Signe, V.Boite.Max.Y * Cote, V.Boite.Min.Z + V.Boite.GetSize().Z * 0.3f);
				P.Relatif = FTransform(C);
				P.Boite = FBox(C - FVector(65.f, 5.f, 0.f), C + FVector(65.f, 5.f, 200.f));
				Brutes.Add(P);
			}
		}
		MeshDePorte = nullptr;
		UE_LOG(LogTrainChelles, Log, TEXT("[Train] Voiture %d : pas de socket 'Door' dans %s, portes invisibles."), Index, *Corps->GetName());
	}

	// --- Plancher
	if (R.HauteurPlancher > 0.f)
	{
		V.Plancher = R.HauteurPlancher / E;
	}
	else
	{
		float BasPortes = TNumericLimits<float>::Max();
		for (const FPorteBrute& P : Brutes) { BasPortes = FMath::Min(BasPortes, P.Boite.Min.Z); }
		V.Plancher = FMath::Clamp(BasPortes, V.Boite.Min.Z + 20.f, V.Boite.Max.Z - 200.f);
	}

	// --- Murs invisibles : plancher, toit, avant, arriere, cotes ; ouvertures aux portes
	const FVector Min = V.Boite.Min;
	const FVector Max = V.Boite.Max;
	const float Ep = 8.f;
	const float Bas = V.Plancher;
	const float Haut = Max.Z - 10.f;
	const float CX = V.Boite.GetCenter().X;
	const float CY = V.Boite.GetCenter().Y;
	const float EX = V.Boite.GetExtent().X;
	const float EY = V.Boite.GetExtent().Y;
	int32 Numero = 0;
	auto CreerMur = [this, Racine, Index, &Numero](const FVector& C, const FVector& DemiTaille) -> UBoxComponent*
	{
		UBoxComponent* Boite = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("MurTrain_%d_%d"), Index, Numero++));
		Boite->SetMobility(EComponentMobility::Movable);
		Boite->SetupAttachment(Racine);
		Boite->SetBoxExtent(DemiTaille.ComponentMax(FVector(2.f)), false);
		Boite->SetRelativeLocation(C);
		Boite->SetCollisionProfileName(TEXT("BlockAllDynamic"));
		Boite->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Boite->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Boite->SetHiddenInGame(true);
		Boite->SetCanEverAffectNavigation(false);
		Boite->RegisterComponent();
		return Boite;
	};
	CreerMur(FVector(CX, CY, Bas - 12.f), FVector(EX - 2.f, EY - 2.f, 12.f));             // plancher
	CreerMur(FVector(CX, CY, Haut), FVector(EX, EY, Ep));                                  // toit
	CreerMur(FVector(Max.X - Ep, CY, (Bas + Haut) * 0.5f), FVector(Ep, EY, (Haut - Bas) * 0.5f)); // avant
	CreerMur(FVector(Min.X + Ep, CY, (Bas + Haut) * 0.5f), FVector(Ep, EY, (Haut - Bas) * 0.5f)); // arriere

	UMaterialInstanceDynamic* MateriauRampe = MateriauCouleur(this, FLinearColor(0.12f, 0.12f, 0.13f));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (float Cote : { 1.f, -1.f })
	{
		const float YMur = (Cote > 0.f) ? Max.Y - Ep : Min.Y + Ep;
		TArray<FVector2D> Ouvertures;
		for (const FPorteBrute& B : Brutes)
		{
			const bool bDeCeCote = (B.Boite.GetCenter().Y - CY) * Cote > 0.f;
			if (!bDeCeCote) { continue; }
			const bool bQuai = Cote > 0.f || R.bPortesDeuxCotes;
			const float Demi = FMath::Max(B.Boite.GetExtent().X, 45.f);
			FPorteTrain Porte;
			Porte.bCoteQuai = bQuai;
			if (MeshDePorte)
			{
				UStaticMeshComponent* C = AjouterMesh(MeshDePorte, B.Relatif, TEXT("Porte"), false);
				Porte.Composant = C;
				Porte.PositionFermee = B.Relatif.GetLocation();
				// la porte glisse le long de la caisse, vers le milieu de la voiture, et ressort un peu
				const float VersMilieu = (B.Boite.GetCenter().X > CX) ? -1.f : 1.f;
				Porte.Glissement = FVector(VersMilieu * Demi * 2.f * 0.95f, Cote * 7.f, 0.f);
			}
			if (bQuai)
			{
				Ouvertures.Add(FVector2D(B.Boite.GetCenter().X - Demi, B.Boite.GetCenter().X + Demi));
				Porte.Bloqueur = CreerMur(FVector(B.Boite.GetCenter().X, YMur, (Bas + Haut) * 0.5f), FVector(Demi, Ep, (Haut - Bas) * 0.5f));
				// marchepied : une rampe de la porte jusqu'a la route (visible seulement porte ouverte)
				if (Cube && Bas > 30.f)
				{
					const float Pente = FMath::DegreesToRadians(33.f);
					const float Horizontal = Bas / FMath::Tan(Pente);
					const float LongueurRampe = FMath::Sqrt(Horizontal * Horizontal + Bas * Bas);
					const FVector Descente(0.f, Cote * FMath::Cos(Pente), -FMath::Sin(Pente));
					const FRotator RotRampe = FRotationMatrix::MakeFromXY(FVector::ForwardVector, Descente).Rotator();
					const FVector CentreRampe(B.Boite.GetCenter().X, Max.Y * (Cote > 0.f ? 1.f : 0.f) + Min.Y * (Cote > 0.f ? 0.f : 1.f) + Cote * Horizontal * 0.5f, Bas * 0.5f);
					UStaticMeshComponent* Rampe = AjouterMesh(Cube, FTransform(RotRampe, CentreRampe, FVector(Demi * 1.6f / 100.f, LongueurRampe / 100.f, 0.06f)), TEXT("Rampe"), false);
					if (Rampe)
					{
						if (MateriauRampe) { Rampe->SetMaterial(0, MateriauRampe); }
						Rampe->SetCollisionProfileName(TEXT("BlockAllDynamic"));
						Rampe->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
						Rampe->CanCharacterStepUpOn = ECB_Yes;
						Rampe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						Rampe->SetVisibility(false);
						Porte.Rampe = Rampe;
					}
				}
			}
			V.Portes.Add(Porte);
		}
		Ouvertures.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
		float X = Min.X;
		for (const FVector2D& O : Ouvertures)
		{
			if (O.X > X + 5.f)
			{
				CreerMur(FVector((X + O.X) * 0.5f, YMur, (Bas + Haut) * 0.5f), FVector((O.X - X) * 0.5f, Ep, (Haut - Bas) * 0.5f));
			}
			X = FMath::Max(X, O.Y);
		}
		if (Max.X > X + 5.f)
		{
			CreerMur(FVector((X + Max.X) * 0.5f, YMur, (Bas + Haut) * 0.5f), FVector((Max.X - X) * 0.5f, Ep, (Haut - Bas) * 0.5f));
		}
	}
	UE_LOG(LogTrainChelles, Log, TEXT("[Train] Voiture %d (%s) : %.1f m, %d porte(s)%s."), Index, *Corps->GetName(), V.Longueur / 100.f, V.Portes.Num(),
		bRetournee ? TEXT(", retournee (locomotive de queue)") : TEXT(""));
	Voitures.Add(V);
}

void ATrainChelles::PlacerTrain(float STete, float DeltaSeconds, bool bTeleport)
{
	if (LongueurTrajet <= 0.f)
	{
		return;
	}
	for (FVoitureTrain& V : Voitures)
	{
		USceneComponent* Racine = V.Racine.Get();
		if (!Racine) { continue; }
		const float SCentre = STete - V.Decalage - V.Longueur * 0.5f;
		const FVector Avant = PointA(SCentre + V.DemiEcartBogies);
		const FVector Arriere = PointA(SCentre - V.DemiEcartBogies);
		FVector Direction = Avant - Arriere;
		Direction.Z = 0.f;
		if (Direction.IsNearlyZero()) { Direction = Racine->GetForwardVector(); }
		const float Lacet = Direction.Rotation().Yaw;

		float ZAv = Avant.Z, ZAr = Arriere.Z;
		const bool bAv = SolSous(Avant, Avant.Z, ZAv);
		const bool bAr = SolSous(Arriere, Arriere.Z, ZAr);
		if (!bAv && bAr) { ZAv = ZAr; }
		if (!bAr && bAv) { ZAr = ZAv; }
		const float ZSol = (ZAv + ZAr) * 0.5f;
		const float Tangage = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ZAv - ZAr, FVector::Dist2D(Avant, Arriere))), -8.f, 8.f);
		if (!V.bZInitialise || bTeleport || DeltaSeconds <= 0.f)
		{
			V.ZLisse = ZSol;
			V.bZInitialise = true;
		}
		else
		{
			V.ZLisse = FMath::FInterpTo(V.ZLisse, ZSol, DeltaSeconds, 6.f);
		}
		const FVector Milieu = (Avant + Arriere) * 0.5f;
		Racine->SetWorldLocationAndRotation(FVector(Milieu.X, Milieu.Y, V.ZLisse), FRotator(Tangage, Lacet, 0.f), false, nullptr,
			bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
	}
}

void ATrainChelles::AnimerPortes(float DeltaSeconds)
{
	const bool bOuvrir = (Etat == 1);
	const float Avant = OuverturePortes;
	OuverturePortes = FMath::FInterpConstantTo(OuverturePortes, bOuvrir ? 1.f : 0.f, DeltaSeconds, 1.f / 1.8f);
	const bool bOuvertesMaintenant = OuverturePortes > 0.6f;
	if (FMath::IsNearlyEqual(Avant, OuverturePortes) && bOuvertesMaintenant == bPortesOuvertesAffichees)
	{
		return;
	}
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, OuverturePortes, 2.f);
	for (FVoitureTrain& V : Voitures)
	{
		for (FPorteTrain& P : V.Portes)
		{
			if (!P.bCoteQuai) { continue; }
			if (UStaticMeshComponent* C = P.Composant.Get())
			{
				C->SetRelativeLocation(P.PositionFermee + P.Glissement * Alpha);
			}
			if (bOuvertesMaintenant != bPortesOuvertesAffichees)
			{
				if (UBoxComponent* B = P.Bloqueur.Get())
				{
					B->SetCollisionEnabled(bOuvertesMaintenant ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
				}
				if (UStaticMeshComponent* Rampe = P.Rampe.Get())
				{
					Rampe->SetVisibility(bOuvertesMaintenant);
					Rampe->SetCollisionEnabled(bOuvertesMaintenant ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
				}
			}
		}
	}
	bPortesOuvertesAffichees = bOuvertesMaintenant;
}

void ATrainChelles::PoserRails()
{
	const FReglagesTrain& R = Reglages();
	if (!R.bRails || LongueurTrajet <= 0.f || Rails)
	{
		return;
	}
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!Cube) { return; }
	auto CreerHism = [this, Cube](const TCHAR* Nom, const FLinearColor& Couleur, int32 Distance) -> UHierarchicalInstancedStaticMeshComponent*
	{
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Nom);
		H->SetMobility(EComponentMobility::Movable);
		H->SetupAttachment(RootComponent);
		H->SetUsingAbsoluteLocation(true);
		H->SetUsingAbsoluteRotation(true);
		H->SetUsingAbsoluteScale(true);
		H->SetStaticMesh(Cube);
		if (UMaterialInstanceDynamic* Mid = MateriauCouleur(this, Couleur)) { H->SetMaterial(0, Mid); }
		H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		H->SetCanEverAffectNavigation(false);
		H->SetCastShadow(false);
		H->SetCullDistances(0, Distance);
		H->RegisterComponent();
		return H;
	};
	Rails = CreerHism(TEXT("RailsTrain"), FLinearColor(0.32f, 0.30f, 0.28f), 40000);
	TraversesHism = R.bTraverses ? CreerHism(TEXT("TraversesTrain"), FLinearColor(0.18f, 0.11f, 0.06f), 15000) : nullptr;

	const float E = R.Echelle;
	const float Pas = 250.f;
	const float PasTraverse = 65.f * E;
	int32 NbRails = 0, NbTraverses = 0;
	for (float S = 0.f; S < LongueurTrajet; S += Pas)
	{
		const float Fin = FMath::Min(S + Pas, LongueurTrajet);
		const FVector A = PointA(S);
		const FVector B = PointA(Fin);
		FVector Dir = B - A;
		Dir.Z = 0.f;
		const float L = Dir.Size();
		if (L < 5.f) { continue; }
		Dir /= L;
		const FVector Milieu = (A + B) * 0.5f;
		float Z = Milieu.Z;
		SolSous(Milieu, Milieu.Z, Z);
		const FRotator Rot = Dir.Rotation();
		for (float Cote : { 1.f, -1.f })
		{
			const FVector P = FVector(Milieu.X, Milieu.Y, Z + 5.f * E) + Droite(Dir) * (Ecartement * 0.5f * E * Cote);
			Rails->AddInstance(FTransform(Rot, P, FVector((L + 4.f) / 100.f, 0.07f * E, 0.10f * E)));
			++NbRails;
		}
		if (TraversesHism)
		{
			for (float T = FMath::Fmod(S, PasTraverse) > 0.f ? PasTraverse - FMath::Fmod(S, PasTraverse) : 0.f; T < (Fin - S); T += PasTraverse)
			{
				const FVector P = A + (B - A) * (T / (Fin - S));
				TraversesHism->AddInstance(FTransform(Rot, FVector(P.X, P.Y, Z + 1.5f * E), FVector(0.24f * E, 2.5f * E, 0.04f * E)));
				++NbTraverses;
			}
		}
	}
	UE_LOG(LogTrainChelles, Log, TEXT("[Train] Rails poses : %d morceaux de rail, %d traverses."), NbRails, NbTraverses);
}

void ATrainChelles::PoserPanneaux()
{
	for (const TWeakObjectPtr<AArretTrainChelles>& Panneau : PanneauxPoses)
	{
		if (Panneau.IsValid()) { Panneau->Destroy(); }
	}
	PanneauxPoses.Reset();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return; // personne ne regarde
	}
	for (const FArretTrainInfo& Info : ArretsInfos)
	{
		if (Info.bManuel) { continue; }
		const FTransform Ou(FRotator(0.f, Info.Lacet, 0.f), Info.Position);
		AArretTrainChelles* Panneau = GetWorld()->SpawnActorDeferred<AArretTrainChelles>(AArretTrainChelles::StaticClass(), Ou, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Panneau) { continue; }
		Panneau->bAutomatique = true;
		Panneau->NomArret = Info.Nom;
		UGameplayStatics::FinishSpawningActor(Panneau, Ou);
		PanneauxPoses.Add(Panneau);
	}
}

void ATrainChelles::RetirerVoituresSurLePassage()
{
	UClass* ClasseVoiture = FindObject<UClass>(nullptr, TEXT("/Script/ChellesCirculation.VehiculeCirculation"));
	if (!ClasseVoiture || LongueurTrajet <= 0.f)
	{
		return;
	}
	// le couloir du train : de 15 m derriere la queue a 60 m devant la tete
	TArray<FVector> Couloir;
	for (float D = 6000.f; D >= -LongueurTrain - 1500.f; D -= 400.f)
	{
		Couloir.Add(PointA(ParcouruLocal + D));
	}
	const float DemiLargeur = (Voitures.Num() > 0 ? Voitures[0].Boite.Max.Y * Reglages().Echelle : 150.f) + 350.f;
	const float Rayon2 = DemiLargeur * DemiLargeur;

	TSet<TWeakObjectPtr<AActor>> DansLeCouloir;
	for (TActorIterator<AActor> It(GetWorld(), ClasseVoiture); It; ++It)
	{
		AActor* Voiture = *It;
		const FVector Ici = Voiture->GetActorLocation();
		bool bGene = false;
		for (const FVector& P : Couloir)
		{
			if (FVector::DistSquared2D(P, Ici) < Rayon2 && FMath::Abs(P.Z - Ici.Z) < 600.f)
			{
				bGene = true;
				break;
			}
		}
		if (!bGene) { continue; }
		DansLeCouloir.Add(Voiture);
		if (!Voiture->IsHidden())
		{
			Voiture->SetActorHiddenInGame(true);
			Voiture->SetActorEnableCollision(false);
			VoituresCachees.Add(Voiture);
		}
	}
	for (auto It = VoituresCachees.CreateIterator(); It; ++It)
	{
		AActor* Voiture = It->Get();
		if (!Voiture)
		{
			It.RemoveCurrent();
			continue;
		}
		if (!DansLeCouloir.Contains(*It))
		{
			Voiture->SetActorHiddenInGame(false);
			Voiture->SetActorEnableCollision(true);
			It.RemoveCurrent();
		}
	}
}

void ATrainChelles::RemettreToutesLesVoitures()
{
	for (const TWeakObjectPtr<AActor>& Voiture : VoituresCachees)
	{
		if (AActor* A = Voiture.Get())
		{
			A->SetActorHiddenInGame(false);
			A->SetActorEnableCollision(true);
		}
	}
	VoituresCachees.Reset();
}

bool ATrainChelles::EstDansLeTrain(const FVector& Position) const
{
	for (const FVoitureTrain& V : Voitures)
	{
		const USceneComponent* Racine = V.Racine.Get();
		if (!Racine) { continue; }
		const FVector L = Racine->GetComponentTransform().InverseTransformPosition(Position);
		if (L.X > V.Boite.Min.X && L.X < V.Boite.Max.X && L.Y > V.Boite.Min.Y && L.Y < V.Boite.Max.Y
			&& L.Z > V.Plancher - 40.f && L.Z < V.Boite.Max.Z)
		{
			return true;
		}
	}
	return false;
}

FString ATrainChelles::NomAffiche(int32 IndexArret) const
{
	return ArretsInfos.IsValidIndex(IndexArret) ? AvecAccents(ArretsInfos[IndexArret].Nom) : FString();
}

void ATrainChelles::TickLocal(float DeltaSeconds)
{
	UWorld* Monde = GetWorld();
	const FReglagesTrain& R = Reglages();
	if (LongueurTrajet <= 0.f)
	{
		return;
	}
	if (!bTrainPrepare)
	{
		PreparerTrain();
	}
	if (!Rails && R.bRails)
	{
		PoserRails();
	}

	// Le train suit son trajet
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
	PlacerTrain(ParcouruLocal, DeltaSeconds, false);
	AnimerPortes(DeltaSeconds);

	// Les voitures sur le passage
	const float Maintenant = Monde->GetTimeSeconds();
	if (R.bRetirerVoitures && Maintenant >= ProchainRetrait)
	{
		ProchainRetrait = Maintenant + 0.25f;
		RetirerVoituresSurLePassage();
	}

	// Le texte a l'ecran pour le joueur de cette machine
	TexteAide.Reset();
	if (!R.bMessages)
	{
		return;
	}
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
	if (!Joueur || ArretsInfos.Num() == 0 || Voitures.Num() == 0)
	{
		return;
	}
	const FVector Ici = Joueur->GetActorLocation();
	const bool bDedans = EstDansLeTrain(Ici);
	const FVector MilieuTrain = PointA(ParcouruLocal - LongueurTrain * 0.5f);
	const bool bPres = bDedans || FVector::Dist2D(Ici, MilieuTrain) < LongueurTrain * 0.5f + 1500.f;
	if (Etat == 1 && bPres)
	{
		TexteAide = FString::Printf(TEXT("TRAIN  -  %s  -  portes ouvertes, d\u00E9part dans %d s"), *NomAffiche(ArretCible), FMath::CeilToInt(DepartDans));
	}
	else if (Etat == 2 && bPres)
	{
		TexteAide = TEXT("TRAIN  -  Attention \u00E0 la fermeture des portes !");
	}
	else if (Etat == 0 && bDedans)
	{
		TexteAide = FString::Printf(TEXT("TRAIN  -  Prochain arr\u00EAt : %s"), *NomAffiche(ArretCible));
	}
	else if (!bDedans)
	{
		// pres d'un panneau : dans combien de temps passe le train ?
		for (int32 k = 0; k < ArretsInfos.Num(); ++k)
		{
			if (FVector::Dist2D(Ici, ArretsInfos[k].Position) > 1500.f) { continue; }
			if (Etat != 0 && k == ArretCible) { break; } // le train est la
			const float Distance = Boucler(ArretsInfos[k].DistanceTete - ParcouruLocal);
			int32 ArretsAvant = 0;
			for (int32 j = 0; j < ArretsInfos.Num(); ++j)
			{
				if (j == k) { continue; }
				const float Dj = Boucler(ArretsInfos[j].DistanceTete - ParcouruLocal);
				if (Dj > 1.f && Dj < Distance) { ++ArretsAvant; }
			}
			const float Secondes = Distance / FMath::Max(100.f, R.VitesseKmh * KmhVersCms * 0.6f)
				+ ArretsAvant * (R.AttenteSecondes + 6.f) + (Etat == 1 ? DepartDans : 0.f);
			const int32 Minutes = FMath::FloorToInt(Secondes / 60.f);
			TexteAide = (Minutes > 0)
				? FString::Printf(TEXT("TRAIN  -  Arr\u00EAt %s  -  prochain train dans %d min %02d s environ"), *NomAffiche(k), Minutes, FMath::FloorToInt(Secondes) % 60)
				: FString::Printf(TEXT("TRAIN  -  Arr\u00EAt %s  -  prochain train dans %d s environ"), *NomAffiche(k), FMath::Max(1, FMath::CeilToInt(Secondes)));
			break;
		}
	}
}

FText ATrainChelles::TexteEcran() const
{
	return FText::FromString(TexteAide);
}

void ATrainChelles::CreerEcran()
{
	UGameViewportClient* Vue = GetWorld()->GetGameViewport();
	if (!Vue)
	{
		return;
	}
	TWeakObjectPtr<ATrainChelles> Moi(this);
	Ecran = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 200.f))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(STextBlock)
			.Text_Lambda([Moi]() { return Moi.IsValid() ? Moi->TexteEcran() : FText::GetEmpty(); })
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
			.ColorAndOpacity(FLinearColor(0.35f, 1.f, 0.65f))
			.ShadowOffset(FVector2D(2.f, 2.f))
			.ShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f))
			.Justification(ETextJustify::Center)
		];
	Vue->AddViewportWidgetContent(Ecran.ToSharedRef(), 41);
}

// =====================================================================================================
// Console
// =====================================================================================================

void ATrainChelles::DecrireEtat() const
{
	static const TCHAR* Etats[] = { TEXT("roule"), TEXT("a l'arret, portes ouvertes"), TEXT("ferme ses portes") };
	UE_LOG(LogTrainChelles, Display, TEXT("[Train] %s | boucle %.0f m | train %.0f m | %d arret(s) | %s | vers %s | position %.0f m | %.0f km/h"),
		Reglages().bActif ? TEXT("actif") : TEXT("Actif=Non dans Train.txt"), LongueurTrajet / 100.f, LongueurTrain / 100.f, ArretsInfos.Num(),
		Etat < 3 ? Etats[Etat] : TEXT("?"), *NomAffiche(ArretCible), ParcouruLocal / 100.f, VitesseServeur / KmhVersCms);
	for (int32 k = 0; k < ArretsInfos.Num(); ++k)
	{
		UE_LOG(LogTrainChelles, Display, TEXT("[Train]   %d. %s  (X=%.0f Y=%.0f)"), k + 1, *ArretsInfos[k].Nom, ArretsInfos[k].Position.X, ArretsInfos[k].Position.Y);
	}
}

void ATrainChelles::EmmenerAuProchainArret(APawn* Joueur) const
{
	if (!Joueur || ArretsInfos.Num() == 0)
	{
		UE_LOG(LogTrainChelles, Warning, TEXT("[Train] Pas encore d'arret (la ligne se prepare, ou erreur : voir les lignes [Train])."));
		return;
	}
	const FArretTrainInfo& Info = ArretsInfos[FMath::Clamp(ArretCible, 0, ArretsInfos.Num() - 1)];
	const FVector Ou = FVector(Info.Position) + FRotator(0.f, Info.Lacet, 0.f).Vector() * -250.f + FVector(0.f, 0.f, 120.f);
	Joueur->TeleportTo(Ou, FRotator(0.f, Info.Lacet, 0.f));
	UE_LOG(LogTrainChelles, Display, TEXT("[Train] Vous etes a l'arret %s."), *Info.Nom);
}
