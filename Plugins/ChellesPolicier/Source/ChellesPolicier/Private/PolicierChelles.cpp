// CHELLES 3D - Agents de police aux ronds-points et aux grands carrefours.
#include "PolicierChelles.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogChellesPolicier, Log, All);

namespace
{
	constexpr float KmhVersCms = 27.7778f;

	FString DossierPlugin()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ChellesPolicier"));
		return Plugin.IsValid() ? Plugin->GetBaseDir() : FPaths::ProjectPluginsDir() / TEXT("ChellesPolicier");
	}

	/** Copie du reseau routier de Chelles Circulation (lu par reflexion). */
	struct FNoeudLu
	{
		FVector Position = FVector::ZeroVector;
		int32 Voisins = 0;
	};

	struct FTronconLu
	{
		int32 De = INDEX_NONE;
		int32 Vers = INDEX_NONE;
		float VitesseMax = 0.f;
		bool bRondPoint = false;
	};

	bool LireReseau(AActor* Circulation, TArray<FNoeudLu>& OutNoeuds, TArray<FTronconLu>& OutTroncons)
	{
		if (!IsValid(Circulation))
		{
			return false;
		}
		UClass* Classe = Circulation->GetClass();
		FArrayProperty* PNoeuds = FindFProperty<FArrayProperty>(Classe, TEXT("Noeuds"));
		FArrayProperty* PTroncons = FindFProperty<FArrayProperty>(Classe, TEXT("Troncons"));
		FStructProperty* SNoeud = PNoeuds ? CastField<FStructProperty>(PNoeuds->Inner) : nullptr;
		FStructProperty* STroncon = PTroncons ? CastField<FStructProperty>(PTroncons->Inner) : nullptr;
		if (!SNoeud || !STroncon)
		{
			return false;
		}
		FStructProperty* PPosition = FindFProperty<FStructProperty>(SNoeud->Struct, TEXT("Position"));
		FIntProperty* PVoisins = FindFProperty<FIntProperty>(SNoeud->Struct, TEXT("NombreDeVoisins"));
		FIntProperty* PDe = FindFProperty<FIntProperty>(STroncon->Struct, TEXT("De"));
		FIntProperty* PVers = FindFProperty<FIntProperty>(STroncon->Struct, TEXT("Vers"));
		FFloatProperty* PVitesse = FindFProperty<FFloatProperty>(STroncon->Struct, TEXT("VitesseMax"));
		FBoolProperty* PRond = FindFProperty<FBoolProperty>(STroncon->Struct, TEXT("bRondPoint"));
		if (!PPosition || !PVoisins || !PDe || !PVers)
		{
			return false;
		}

		FScriptArrayHelper TabNoeuds(PNoeuds, PNoeuds->ContainerPtrToValuePtr<void>(Circulation));
		OutNoeuds.SetNum(TabNoeuds.Num());
		for (int32 i = 0; i < TabNoeuds.Num(); ++i)
		{
			uint8* Element = TabNoeuds.GetRawPtr(i);
			OutNoeuds[i].Position = *PPosition->ContainerPtrToValuePtr<FVector>(Element);
			OutNoeuds[i].Voisins = PVoisins->GetPropertyValue_InContainer(Element);
		}

		FScriptArrayHelper TabTroncons(PTroncons, PTroncons->ContainerPtrToValuePtr<void>(Circulation));
		OutTroncons.SetNum(TabTroncons.Num());
		for (int32 i = 0; i < TabTroncons.Num(); ++i)
		{
			uint8* Element = TabTroncons.GetRawPtr(i);
			OutTroncons[i].De = PDe->GetPropertyValue_InContainer(Element);
			OutTroncons[i].Vers = PVers->GetPropertyValue_InContainer(Element);
			OutTroncons[i].VitesseMax = PVitesse ? PVitesse->GetPropertyValue_InContainer(Element) : 0.f;
			OutTroncons[i].bRondPoint = PRond ? PRond->GetPropertyValue_InContainer(Element) : false;
		}
		return true;
	}

	/** Groupes de points (union-find). */
	struct FGroupes
	{
		TArray<int32> Parent;
		explicit FGroupes(int32 N)
		{
			Parent.SetNum(N);
			for (int32 i = 0; i < N; ++i) { Parent[i] = i; }
		}
		int32 Racine(int32 i)
		{
			while (Parent[i] != i)
			{
				Parent[i] = Parent[Parent[i]];
				i = Parent[i];
			}
			return i;
		}
		void Unir(int32 A, int32 B)
		{
			A = Racine(A);
			B = Racine(B);
			if (A != B) { Parent[B] = A; }
		}
	};

	float Distance2D(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
	}
}

APolicierChelles::APolicierChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
	SetCanBeDamaged(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

void APolicierChelles::BeginPlay()
{
	Super::BeginPlay();
	LireReglages();
	ChargerModeleEtAnimations();
	if (!bActif)
	{
		UE_LOG(LogChellesPolicier, Log, TEXT("Policiers desactives (Actif=0 dans Policier.txt)."));
	}
}

void APolicierChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerAgents();
	Super::EndPlay(Raison);
}

void APolicierChelles::RetirerAgents()
{
	for (FPostePolicier& Poste : Postes)
	{
		if (IsValid(Poste.Agent))
		{
			Poste.Agent->Destroy();
		}
		Poste.Agent = nullptr;
	}
}

// ---------------------------------------------------------------------------- Reglages

void APolicierChelles::LireReglages()
{
	// Valeurs par defaut (si une ligne manque dans Policier.txt)
	bActif = true;
	CheminModele = TEXT("/Game/Personnages/Mondes/police_officer.police_officer");
	Taille = 180.f;
	bRondsPoints = true;
	bGrandsCarrefours = true;
	BranchesMinimum = 4;
	BranchesPrincipales = 1;
	VitesseRuePrincipaleKmh = 45.f;
	EcartCarrefour = 750.f;
	DistanceApparition = 25000.f;
	AgentsMaximum = 12;
	DossierAnimations = TEXT("/Game/Personnages/Mondes/Policier_Animations");
	CheminAnimRepos.Reset();
	CheminAnimPassez.Reset();
	CheminAnimStop.Reset();
	bRotationAuto = true;
	CorrectionRotation = 0.f;

	FString Texte;
	const FString Chemin = DossierPlugin() / TEXT("Policier.txt");
	if (!FFileHelper::LoadFileToString(Texte, *Chemin))
	{
		UE_LOG(LogChellesPolicier, Warning, TEXT("Policier.txt introuvable (%s) : reglages par defaut."), *Chemin);
		return;
	}
	TArray<FString> Lignes;
	Texte.ParseIntoArrayLines(Lignes);
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
		const bool bOui = Valeur == TEXT("1") || Valeur.Equals(TEXT("oui"), ESearchCase::IgnoreCase) || Valeur.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		const float Nombre = FCString::Atof(*Valeur);
		auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };

		if (Est(TEXT("Actif"))) { bActif = bOui; }
		else if (Est(TEXT("Modele"))) { if (!Valeur.IsEmpty()) { CheminModele = Valeur; } }
		else if (Est(TEXT("Taille"))) { Taille = FMath::Clamp(Nombre, 50.f, 400.f); }
		else if (Est(TEXT("RondsPoints"))) { bRondsPoints = bOui; }
		else if (Est(TEXT("GrandsCarrefours"))) { bGrandsCarrefours = bOui; }
		else if (Est(TEXT("BranchesMinimum"))) { BranchesMinimum = FMath::Clamp(FMath::RoundToInt(Nombre), 3, 8); }
		else if (Est(TEXT("BranchesPrincipales"))) { BranchesPrincipales = FMath::Clamp(FMath::RoundToInt(Nombre), 0, 8); }
		else if (Est(TEXT("VitesseRuePrincipaleKmh"))) { VitesseRuePrincipaleKmh = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("EcartCarrefour"))) { EcartCarrefour = FMath::Clamp(Nombre, 200.f, 3000.f); }
		else if (Est(TEXT("DistanceApparition"))) { DistanceApparition = FMath::Clamp(Nombre, 2000.f, 200000.f); }
		else if (Est(TEXT("AgentsMaximum"))) { AgentsMaximum = FMath::Clamp(FMath::RoundToInt(Nombre), 0, 100); }
		else if (Est(TEXT("DossierAnimations"))) { DossierAnimations = Valeur; }
		else if (Est(TEXT("AnimRepos"))) { CheminAnimRepos = Valeur; }
		else if (Est(TEXT("AnimPassez"))) { CheminAnimPassez = Valeur; }
		else if (Est(TEXT("AnimStop"))) { CheminAnimStop = Valeur; }
		else if (Est(TEXT("CorrectionRotation")))
		{
			bRotationAuto = Valeur.IsEmpty();
			CorrectionRotation = Nombre;
		}
	}
}

void APolicierChelles::ChargerModeleEtAnimations()
{
	AnimRepos = AnimPassez = AnimStop = nullptr;
	Modele = LoadObject<USkeletalMesh>(nullptr, *CheminModele);
	if (!Modele)
	{
		UE_LOG(LogChellesPolicier, Warning, TEXT("Modele introuvable : %s"), *CheminModele);
		return;
	}
	USkeleton* Squelette = Modele->Skeleton;
	auto Charger = [Squelette](const FString& Chemin) -> UAnimSequenceBase*
	{
		if (Chemin.IsEmpty())
		{
			return nullptr;
		}
		UAnimSequenceBase* Anim = LoadObject<UAnimSequenceBase>(nullptr, *Chemin);
		return (Anim && Anim->GetSkeleton() == Squelette) ? Anim : nullptr;
	};
	AnimRepos = Charger(CheminAnimRepos);
	AnimPassez = Charger(CheminAnimPassez);
	AnimStop = Charger(CheminAnimStop);

	if (DossierAnimations.IsEmpty() || (AnimRepos && AnimPassez && AnimStop))
	{
		return;
	}
	// Recherche automatique par le nom dans le dossier des animations
	IAssetRegistry& Registre = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Liste;
	Registre.GetAssetsByPath(FName(*DossierAnimations), Liste, true);
	for (const FAssetData& Donnee : Liste)
	{
		if (Donnee.AssetClass != UAnimSequence::StaticClass()->GetFName())
		{
			continue;
		}
		const FString Nom = Donnee.AssetName.ToString().ToLower();
		auto Contient = [&Nom](const TArray<const TCHAR*>& Mots)
		{
			for (const TCHAR* Mot : Mots)
			{
				if (Nom.Contains(Mot)) { return true; }
			}
			return false;
		};
		UAnimSequenceBase** Cible = nullptr;
		if (!AnimStop && Contient({ TEXT("stop"), TEXT("halt"), TEXT("arret") })) { Cible = &AnimStop; }
		else if (!AnimPassez && Contient({ TEXT("passez"), TEXT("wav"), TEXT("direct"), TEXT("signal"), TEXT("circul"), TEXT("point"), TEXT("traffic") })) { Cible = &AnimPassez; }
		else if (!AnimRepos && Contient({ TEXT("repos"), TEXT("idle"), TEXT("attente"), TEXT("breath") })) { Cible = &AnimRepos; }
		if (!Cible)
		{
			continue;
		}
		UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Donnee.GetAsset());
		if (Anim && Anim->GetSkeleton() == Squelette)
		{
			*Cible = Anim;
		}
		else if (Anim)
		{
			UE_LOG(LogChellesPolicier, Warning, TEXT("%s n'utilise pas le squelette du policier : ignoree."), *Donnee.AssetName.ToString());
		}
	}
}

// ---------------------------------------------------------------------------- Boucle

void APolicierChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActif || !Modele)
	{
		return;
	}
	UWorld* Monde = GetWorld();
	const float Maintenant = Monde->GetTimeSeconds();

	if (!bPostesCalcules)
	{
		if (Maintenant < ProchaineRecherche)
		{
			return;
		}
		ProchaineRecherche = Maintenant + 1.f;
		if (!IsValid(Circulation))
		{
			Circulation = TrouverCirculation();
		}
		if (!Circulation)
		{
			if (Maintenant > 15.f && DernierMessage.IsEmpty())
			{
				Annoncer(TEXT("Policiers : pas de circulation dans ce niveau (Circulation Spawner absent)."), FColor::Orange);
			}
			return;
		}
		// On attend que le reseau routier soit pret (construit au lancement par la circulation),
		// puis une seconde de plus pour que les altitudes soient mesurees.
		TArray<FNoeudLu> Noeuds;
		TArray<FTronconLu> Troncons;
		if (!LireReseau(Circulation, Noeuds, Troncons) || Troncons.Num() == 0)
		{
			if (Maintenant > 20.f && DernierMessage.IsEmpty())
			{
				Annoncer(TEXT("Policiers : le reseau routier de la circulation est vide."), FColor::Orange);
			}
			return;
		}
		if (PremierReseauVu < 0.f)
		{
			PremierReseauVu = Maintenant;
			return;
		}
		bPostesCalcules = true;
		CalculerPostes();
		EcrireRapport();
		Annoncer(FString::Printf(TEXT("Policiers : %d rond(s)-point(s) et %d grand(s) carrefour(s) surveilles (%s)."),
			NombreRondsPoints, NombreCarrefours, AnimRepos ? TEXT("animations") : TEXT("gestes calcules")), FColor::Green);
		ProchaineApparition = 0.f;
	}

	FVector Joueur;
	if (Maintenant >= ProchaineApparition && PositionJoueur(Joueur))
	{
		ProchaineApparition = Maintenant + 1.f;
		MettreAJourAgents(Joueur);
	}
	if (Maintenant >= ProchainOrdre)
	{
		ProchainOrdre = Maintenant + 0.2f;
		LireVoitures(Maintenant);
		DonnerLesOrdres(Maintenant);
	}
}

AActor* APolicierChelles::TrouverCirculation() const
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (UClass* Classe = It->GetClass(); Classe; Classe = Classe->GetSuperClass())
		{
			if (Classe->GetFName() == TEXT("CirculationSpawner"))
			{
				return *It;
			}
		}
	}
	return nullptr;
}

bool APolicierChelles::PositionJoueur(FVector& OutPosition) const
{
	APlayerController* Controleur = GetWorld()->GetFirstPlayerController();
	if (!Controleur)
	{
		return false;
	}
	FRotator Vue;
	Controleur->GetPlayerViewPoint(OutPosition, Vue);
	return true;
}

// ---------------------------------------------------------------------------- Postes

bool APolicierChelles::TrouverSol(const FVector& Point, float& OutZ) const
{
	FHitResult Impact;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PolicierSol), true);
	Params.AddIgnoredActor(this);
	const FVector Debut = Point + FVector(0.f, 0.f, 350.f);
	const FVector Fin = Point - FVector(0.f, 0.f, 1500.f);
	if (GetWorld()->LineTraceSingleByObjectType(Impact, Debut, Fin, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutZ = Impact.ImpactPoint.Z;
		return true;
	}
	return false;
}

bool APolicierChelles::PlaceLibre(const FVector& Centre, const FVector& Point) const
{
	UWorld* Monde = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PolicierPlace), false);
	Params.AddIgnoredActor(this);
	const FCollisionObjectQueryParams Statique(ECC_WorldStatic);
	// Rien entre le centre du carrefour et l'agent (pas de mur : il est bien dans la rue)
	if (Monde->LineTraceTestByObjectType(Centre + FVector(0.f, 0.f, 120.f), Point + FVector(0.f, 0.f, 120.f), Statique, Params))
	{
		return false;
	}
	// Et assez de place pour lui (pas dans un lampadaire, un arbre ou une facade)
	return !Monde->OverlapAnyTestByObjectType(Point + FVector(0.f, 0.f, 115.f), FQuat::Identity, Statique,
		FCollisionShape::MakeCapsule(30.f, 70.f), Params);
}

bool APolicierChelles::CalculerPostes()
{
	RetirerAgents();
	Postes.Reset();
	NombreRondsPoints = 0;
	NombreCarrefours = 0;

	TArray<FNoeudLu> Noeuds;
	TArray<FTronconLu> Troncons;
	if (!LireReseau(Circulation, Noeuds, Troncons))
	{
		return false;
	}
	const int32 NombreNoeuds = Noeuds.Num();
	auto Valide = [NombreNoeuds](int32 i) { return i >= 0 && i < NombreNoeuds; };

	// ---- 1. Ronds-points : les troncons "rond-point" relies entre eux forment un anneau
	FGroupes Anneaux(NombreNoeuds);
	TArray<bool> SurAnneau;
	SurAnneau.Init(false, NombreNoeuds);
	for (const FTronconLu& T : Troncons)
	{
		if (T.bRondPoint && Valide(T.De) && Valide(T.Vers))
		{
			Anneaux.Unir(T.De, T.Vers);
			SurAnneau[T.De] = SurAnneau[T.Vers] = true;
		}
	}
	TMap<int32, TArray<int32>> Groupes;
	for (int32 i = 0; i < NombreNoeuds; ++i)
	{
		if (SurAnneau[i])
		{
			Groupes.FindOrAdd(Anneaux.Racine(i)).Add(i);
		}
	}
	struct FRond { FVector Centre; float Rayon; };
	TArray<FRond> Ronds;
	for (const TPair<int32, TArray<int32>>& Groupe : Groupes)
	{
		if (Groupe.Value.Num() < 3)
		{
			continue;
		}
		FVector Centre = FVector::ZeroVector;
		for (int32 i : Groupe.Value) { Centre += Noeuds[i].Position; }
		Centre /= Groupe.Value.Num();
		float Rayon = 0.f;
		for (int32 i : Groupe.Value) { Rayon += Distance2D(Noeuds[i].Position, Centre); }
		Rayon /= Groupe.Value.Num();
		if (Rayon < 250.f || Rayon > 8000.f)
		{
			continue;
		}
		Ronds.Add({ Centre, Rayon });

		if (bRondsPoints)
		{
			FPostePolicier Poste;
			Poste.bRondPoint = true;
			Poste.Centre = Centre;
			Poste.Position = Centre;
			Poste.Rayon = Rayon + 1500.f;
			float Z;
			if (TrouverSol(Centre, Z)) { Poste.Position.Z = Z; }
			for (const FTronconLu& T : Troncons)
			{
				if (!T.bRondPoint && Valide(T.De) && Valide(T.Vers) && SurAnneau[T.Vers] && !SurAnneau[T.De]
					&& Anneaux.Racine(T.Vers) == Groupe.Key)
				{
					++Poste.Branches; // rues qui entrent dans le rond-point
				}
			}
			Postes.Add(Poste);
			++NombreRondsPoints;
		}
	}

	// ---- 2. Grands carrefours
	if (bGrandsCarrefours)
	{
		TArray<int32> Candidats;
		for (int32 i = 0; i < NombreNoeuds; ++i)
		{
			if (Noeuds[i].Voisins < 3 || SurAnneau[i])
			{
				continue;
			}
			bool bPresDUnRond = false;
			for (const FRond& Rond : Ronds)
			{
				if (Distance2D(Noeuds[i].Position, Rond.Centre) < Rond.Rayon + 2500.f) { bPresDUnRond = true; break; }
			}
			if (!bPresDUnRond)
			{
				Candidats.Add(i);
			}
		}
		// Les points tres proches (rues a deux chaussees) forment un seul carrefour
		FGroupes Fusion(NombreNoeuds);
		for (int32 a = 0; a < Candidats.Num(); ++a)
		{
			for (int32 b = a + 1; b < Candidats.Num(); ++b)
			{
				const FVector& PA = Noeuds[Candidats[a]].Position;
				const FVector& PB = Noeuds[Candidats[b]].Position;
				if (Distance2D(PA, PB) < 1500.f && FMath::Abs(PA.Z - PB.Z) < 500.f)
				{
					Fusion.Unir(Candidats[a], Candidats[b]);
				}
			}
		}
		TArray<int32> GroupeDuNoeud;
		GroupeDuNoeud.Init(INDEX_NONE, NombreNoeuds);
		TArray<TArray<int32>> Carrefours;
		TMap<int32, int32> IndexDuGroupe;
		for (int32 i : Candidats)
		{
			const int32 Racine = Fusion.Racine(i);
			int32* Index = IndexDuGroupe.Find(Racine);
			if (!Index)
			{
				Index = &IndexDuGroupe.Add(Racine, Carrefours.Num());
				Carrefours.AddDefaulted();
			}
			Carrefours[*Index].Add(i);
			GroupeDuNoeud[i] = *Index;
		}
		// Rues qui partent de chaque carrefour : (angle, vitesse)
		struct FBranche { float Angle; float Vitesse; int32 Exterieur; };
		TArray<TArray<FBranche>> Branches;
		Branches.SetNum(Carrefours.Num());
		TArray<FVector> Centres;
		Centres.SetNum(Carrefours.Num());
		for (int32 c = 0; c < Carrefours.Num(); ++c)
		{
			FVector Centre = FVector::ZeroVector;
			for (int32 i : Carrefours[c]) { Centre += Noeuds[i].Position; }
			Centres[c] = Centre / Carrefours[c].Num();
		}
		for (const FTronconLu& T : Troncons)
		{
			if (!Valide(T.De) || !Valide(T.Vers))
			{
				continue;
			}
			const int32 GA = GroupeDuNoeud[T.De];
			const int32 GB = GroupeDuNoeud[T.Vers];
			if (GA == GB)
			{
				continue;
			}
			if (GA != INDEX_NONE)
			{
				const FVector D = Noeuds[T.Vers].Position - Centres[GA];
				Branches[GA].Add({ FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), T.VitesseMax, T.Vers });
			}
			if (GB != INDEX_NONE)
			{
				const FVector D = Noeuds[T.De].Position - Centres[GB];
				Branches[GB].Add({ FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), T.VitesseMax, T.De });
			}
		}

		const float SeuilPrincipal = VitesseRuePrincipaleKmh * KmhVersCms;
		for (int32 c = 0; c < Carrefours.Num(); ++c)
		{
			TArray<FBranche>& Liste = Branches[c];
			if (Liste.Num() < 3)
			{
				continue;
			}
			Liste.Sort([](const FBranche& A, const FBranche& B) { return A.Angle < B.Angle; });
			// Regroupe les directions a moins de 30 degres (les deux sens d'une meme rue)
			TArray<float> Angles;
			TArray<float> Vitesses;
			for (const FBranche& Br : Liste)
			{
				if (Angles.Num() > 0 && FMath::Abs(FMath::FindDeltaAngleDegrees(Angles.Last(), Br.Angle)) < 30.f)
				{
					Vitesses.Last() = FMath::Max(Vitesses.Last(), Br.Vitesse);
					continue;
				}
				Angles.Add(Br.Angle);
				Vitesses.Add(Br.Vitesse);
			}
			if (Angles.Num() > 1 && FMath::Abs(FMath::FindDeltaAngleDegrees(Angles.Last(), Angles[0] + 360.f)) < 30.f)
			{
				Vitesses[0] = FMath::Max(Vitesses[0], Vitesses.Last());
				Angles.Pop();
				Vitesses.Pop();
			}
			int32 Principales = 0;
			for (float V : Vitesses) { if (V >= SeuilPrincipal - 1.f) { ++Principales; } }
			if (Angles.Num() < BranchesMinimum || Principales < BranchesPrincipales)
			{
				continue;
			}

			// L'agent se tient au coin du trottoir : dans le plus grand angle entre deux rues
			struct FEcart { float Bissectrice; float Ouverture; };
			TArray<FEcart> Ecarts;
			for (int32 k = 0; k < Angles.Num(); ++k)
			{
				const float A = Angles[k];
				const float B = Angles[(k + 1) % Angles.Num()];
				float Ouverture = B - A;
				if (Ouverture <= 0.f) { Ouverture += 360.f; }
				Ecarts.Add({ A + Ouverture * 0.5f, Ouverture });
			}
			Ecarts.Sort([](const FEcart& A, const FEcart& B) { return A.Ouverture > B.Ouverture; });

			const FVector Centre = Centres[c];
			FVector Choisi = FVector::ZeroVector;
			bool bTrouve = false;
			for (const FEcart& E : Ecarts)
			{
				const FVector Dir(FMath::Cos(FMath::DegreesToRadians(E.Bissectrice)), FMath::Sin(FMath::DegreesToRadians(E.Bissectrice)), 0.f);
				for (float Facteur : { 1.f, 0.8f, 0.62f })
				{
					FVector Point = Centre + Dir * EcartCarrefour * Facteur;
					float Z;
					if (!TrouverSol(Point, Z))
					{
						continue;
					}
					Point.Z = Z;
					if (PlaceLibre(Centre, Point))
					{
						Choisi = Point;
						bTrouve = true;
						break;
					}
				}
				if (bTrouve) { break; }
			}
			if (!bTrouve)
			{
				UE_LOG(LogChellesPolicier, Log, TEXT("Carrefour %s : pas de place libre pour l'agent."), *Centre.ToCompactString());
				continue;
			}
			float RayonCarrefour = 0.f;
			for (int32 i : Carrefours[c]) { RayonCarrefour = FMath::Max(RayonCarrefour, Distance2D(Noeuds[i].Position, Centre)); }

			FPostePolicier Poste;
			Poste.Centre = Centre;
			Poste.Position = Choisi;
			Poste.Rayon = FMath::Max(1500.f, RayonCarrefour + 1200.f);
			Poste.Branches = Angles.Num();
			Postes.Add(Poste);
			++NombreCarrefours;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------- Agents

void APolicierChelles::MettreAJourAgents(const FVector& Joueur)
{
	// Les postes les plus proches du joueur recoivent un agent
	TArray<TPair<float, int32>> Proches;
	for (int32 i = 0; i < Postes.Num(); ++i)
	{
		const float D = Distance2D(Joueur, Postes[i].Position);
		const float Limite = IsValid(Postes[i].Agent) ? DistanceApparition * 1.15f : DistanceApparition;
		if (D < Limite)
		{
			Proches.Emplace(D, i);
		}
	}
	Proches.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	TArray<bool> Garder;
	Garder.Init(false, Postes.Num());
	for (int32 k = 0; k < Proches.Num() && k < AgentsMaximum; ++k)
	{
		Garder[Proches[k].Value] = true;
	}

	for (int32 i = 0; i < Postes.Num(); ++i)
	{
		FPostePolicier& Poste = Postes[i];
		if (!Garder[i])
		{
			if (IsValid(Poste.Agent)) { Poste.Agent->Destroy(); }
			Poste.Agent = nullptr;
			continue;
		}
		if (IsValid(Poste.Agent))
		{
			continue;
		}
		const float Lacet = Poste.bRondPoint ? FMath::FRandRange(-180.f, 180.f) : (Poste.Centre - Poste.Position).Rotation().Yaw;
		const FTransform Transformation(FRotator(0.f, Lacet, 0.f), Poste.Position + FVector(0.f, 0.f, Taille * 0.5f + 2.f));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		AAgentPolicier* Agent = GetWorld()->SpawnActor<AAgentPolicier>(AAgentPolicier::StaticClass(), Transformation, Params);
		if (!Agent)
		{
			continue;
		}
		if (!Agent->Configurer(Modele, Taille, bRotationAuto, CorrectionRotation, AnimRepos, AnimPassez, AnimStop))
		{
			Agent->Destroy();
			continue;
		}
		Agent->DonnerOrdre(EGestePolicier::Repos, Lacet, 1.f);
		Poste.Agent = Agent;
	}
}

void APolicierChelles::LireVoitures(float Maintenant)
{
	Voitures.Reset();
	bool bUnAgent = false;
	for (const FPostePolicier& Poste : Postes)
	{
		if (IsValid(Poste.Agent)) { bUnAgent = true; break; }
	}
	if (!bUnAgent || !IsValid(Circulation))
	{
		PositionsPrecedentes.Reset();
		DerniereLectureVoitures = -1.f;
		return;
	}
	FArrayProperty* Propriete = FindFProperty<FArrayProperty>(Circulation->GetClass(), TEXT("Vehicules"));
	FObjectProperty* Interieur = Propriete ? CastField<FObjectProperty>(Propriete->Inner) : nullptr;
	if (!Interieur)
	{
		return;
	}
	const float Dt = DerniereLectureVoitures > 0.f ? Maintenant - DerniereLectureVoitures : 0.f;
	DerniereLectureVoitures = Maintenant;

	FScriptArrayHelper Tableau(Propriete, Propriete->ContainerPtrToValuePtr<void>(Circulation));
	TMap<TWeakObjectPtr<AActor>, FVector> Nouvelles;
	Nouvelles.Reserve(Tableau.Num());
	for (int32 i = 0; i < Tableau.Num(); ++i)
	{
		AActor* Voiture = Cast<AActor>(Interieur->GetObjectPropertyValue(Tableau.GetRawPtr(i)));
		if (!IsValid(Voiture) || Voiture->IsHidden())
		{
			continue;
		}
		FVoitureVue Vue;
		Vue.Position = Voiture->GetActorLocation();
		if (const FVector* Avant = PositionsPrecedentes.Find(Voiture))
		{
			if (Dt > 0.01f)
			{
				Vue.Vitesse = (Vue.Position - *Avant) / Dt;
				if (Vue.Vitesse.Size() > 4500.f)
				{
					Vue.Vitesse = FVector::ZeroVector; // voiture deplacee d'un coup (recyclee)
				}
			}
		}
		Nouvelles.Add(Voiture, Vue.Position);
		Voitures.Add(Vue);
	}
	PositionsPrecedentes = MoveTemp(Nouvelles);
}

void APolicierChelles::DonnerLesOrdres(float Maintenant)
{
	for (FPostePolicier& Poste : Postes)
	{
		AAgentPolicier* Agent = Poste.Agent;
		if (!IsValid(Agent))
		{
			continue;
		}
		const FVector PosAgent = Agent->GetActorLocation();
		int32 QuiPasse = INDEX_NONE;
		int32 QuiAttend = INDEX_NONE;
		float MeilleurPasse = BIG_NUMBER;
		float MeilleurAttend = BIG_NUMBER;
		for (int32 v = 0; v < Voitures.Num(); ++v)
		{
			const FVoitureVue& Voiture = Voitures[v];
			FVector VersCentre = Poste.Centre - Voiture.Position;
			VersCentre.Z = 0.f;
			const float D = VersCentre.Size();
			if (D > Poste.Rayon + 1500.f || FMath::Abs(Voiture.Position.Z - Poste.Centre.Z) > 800.f)
			{
				continue;
			}
			const float Vitesse = Voiture.Vitesse.Size2D();
			if (Vitesse > 200.f)
			{
				const bool bArrive = (Voiture.Vitesse | VersCentre) > 0.f;
				if ((bArrive || D < Poste.Rayon) && D < MeilleurPasse)
				{
					MeilleurPasse = D;
					QuiPasse = v;
				}
			}
			else if (Vitesse < 80.f && D > 200.f && D < Poste.Rayon + 1200.f && D < MeilleurAttend)
			{
				MeilleurAttend = D;
				QuiAttend = v;
			}
		}

		EGestePolicier Geste = EGestePolicier::Repos;
		float Lacet = Agent->GetActorRotation().Yaw;
		float Cote = 1.f;
		if (QuiPasse != INDEX_NONE)
		{
			// Il regarde la voiture qui arrive et lui fait signe de passer
			Geste = EGestePolicier::Passez;
			Lacet = (Voitures[QuiPasse].Position - PosAgent).Rotation().Yaw;
			const FVector Droite = FRotator(0.f, Lacet, 0.f).RotateVector(FVector::RightVector);
			Cote = ((Voitures[QuiPasse].Vitesse | Droite) >= 0.f) ? 1.f : -1.f;
		}
		else if (QuiAttend != INDEX_NONE)
		{
			// Une voiture attend son tour : main levee vers elle
			Geste = EGestePolicier::Stop;
			Lacet = (Voitures[QuiAttend].Position - PosAgent).Rotation().Yaw;
		}
		else if (!Poste.bRondPoint)
		{
			Lacet = (Poste.Centre - PosAgent).Rotation().Yaw;
		}

		// Pas de changement de geste toutes les 0,2 s (sauf pour faire passer une voiture)
		if (Geste != Agent->GesteActuel())
		{
			if (Maintenant < Poste.ProchainChangement && Geste != EGestePolicier::Passez)
			{
				continue;
			}
			Poste.ProchainChangement = Maintenant + 1.5f;
		}
		Agent->DonnerOrdre(Geste, Lacet, Cote);
	}
}

// ---------------------------------------------------------------------------- Infos

void APolicierChelles::Annoncer(const FString& Texte, const FColor& Couleur)
{
	DernierMessage = Texte;
	UE_LOG(LogChellesPolicier, Log, TEXT("%s"), *Texte);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.f, Couleur, Texte);
	}
}

void APolicierChelles::EcrireRapport() const
{
	FString Texte;
	Texte += FString::Printf(TEXT("Policiers - rapport du %s\r\n"), *FDateTime::Now().ToString(TEXT("%d/%m/%Y %H:%M")));
	Texte += FString::Printf(TEXT("Modele : %s (%s)\r\n"), *CheminModele, Modele ? TEXT("trouve") : TEXT("INTROUVABLE"));
	Texte += FString::Printf(TEXT("Animations : repos=%s passez=%s stop=%s -> %s\r\n"),
		AnimRepos ? *AnimRepos->GetName() : TEXT("-"), AnimPassez ? *AnimPassez->GetName() : TEXT("-"),
		AnimStop ? *AnimStop->GetName() : TEXT("-"), AnimRepos ? TEXT("mode animations") : TEXT("gestes calcules"));
	Texte += FString::Printf(TEXT("Ronds-points : %d   Grands carrefours : %d   (agents max %d, a moins de %.0f m)\r\n\r\n"),
		NombreRondsPoints, NombreCarrefours, AgentsMaximum, DistanceApparition / 100.f);
	for (int32 i = 0; i < Postes.Num(); ++i)
	{
		const FPostePolicier& P = Postes[i];
		Texte += FString::Printf(TEXT("%3d  %-12s  branches %d  centre (%.0f, %.0f, %.0f)  agent (%.0f, %.0f, %.0f)\r\n"), i,
			P.bRondPoint ? TEXT("rond-point") : TEXT("carrefour"), P.Branches,
			P.Centre.X, P.Centre.Y, P.Centre.Z, P.Position.X, P.Position.Y, P.Position.Z);
	}
	FFileHelper::SaveStringToFile(Texte, *(DossierPlugin() / TEXT("Policier_Rapport.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
}

void APolicierChelles::Commande(const TArray<FString>& Arguments)
{
	const FString Action = Arguments.Num() > 0 ? Arguments[0].ToLower() : TEXT("info");
	if (Action == TEXT("relire"))
	{
		RetirerAgents();
		Postes.Reset();
		LireReglages();
		ChargerModeleEtAnimations();
		bPostesCalcules = false;
		PremierReseauVu = -1.f;
		ProchaineRecherche = 0.f;
		Annoncer(TEXT("Policiers : Policier.txt relu."), FColor::Green);
		return;
	}
	if (Action == TEXT("montrer"))
	{
		for (const FPostePolicier& P : Postes)
		{
			const FColor Couleur = P.bRondPoint ? FColor::Cyan : FColor::Yellow;
			DrawDebugSphere(GetWorld(), P.Centre + FVector(0.f, 0.f, 200.f), P.Rayon, 16, Couleur, false, 30.f);
			DrawDebugLine(GetWorld(), P.Position, P.Position + FVector(0.f, 0.f, 5000.f), Couleur, false, 30.f, 0, 40.f);
		}
		Annoncer(FString::Printf(TEXT("Policiers : %d postes affiches 30 s (bleu = rond-point, jaune = carrefour)."), Postes.Num()), FColor::Green);
		return;
	}
	int32 Agents = 0;
	for (const FPostePolicier& P : Postes) { if (IsValid(P.Agent)) { ++Agents; } }
	if (Action == TEXT("liste"))
	{
		EcrireRapport();
		Annoncer(TEXT("Policiers : liste ecrite dans Plugins/ChellesPolicier/Policier_Rapport.txt"), FColor::Green);
		return;
	}
	Annoncer(FString::Printf(TEXT("Policiers : %s, %d ronds-points, %d carrefours, %d agent(s) en ce moment, %s."),
		bActif ? TEXT("actifs") : TEXT("desactives"), NombreRondsPoints, NombreCarrefours, Agents,
		AnimRepos ? TEXT("animations") : TEXT("gestes calcules")), FColor::Green);
}
