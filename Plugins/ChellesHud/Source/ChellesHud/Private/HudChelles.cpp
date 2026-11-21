// CHELLES 3D - HUD du joueur : lit la vie, l'XP, le pouvoir, l'invasion et la mini-carte, puis met a jour le dessin
#include "HudChelles.h"
#include "SHudChelles.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectHash.h"
#include "Misc/App.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"

bool GHudChellesAffiche = true;

// ---------------------------------------------------------------------------
// Reglages de la mini-carte (console, touche ²)
// ---------------------------------------------------------------------------
static int32 GMiniCarteMode = 2;
static FAutoConsoleVariableRef CVarMiniCarte(
	TEXT("Chelles.MiniCarte"), GMiniCarteMode,
	TEXT("Mini-carte : 0 = cachee, 1 = points seuls (plus leger), 2 = avec le quartier vu du ciel."));

static float GMiniCarteZoom = 100.f;
static FAutoConsoleVariableRef CVarMiniCarteZoom(
	TEXT("Chelles.MiniCarteZoom"), GMiniCarteZoom,
	TEXT("Mini-carte : distance du joueur au bord gauche ou droit, en metres (20 a 500, 100 par defaut)."));

static int32 GMiniCarteItineraire = 1;
static FAutoConsoleVariableRef CVarMiniCarteItineraire(
	TEXT("Chelles.MiniCarteItineraire"), GMiniCarteItineraire,
	TEXT("Mini-carte : 1 = trajet cyan + distance en km vers la mission en cours, sinon vers le teleporteur le plus proche ; 0 = rien."));

static int32 GMiniCarteCoins = 1;
static FAutoConsoleVariableRef CVarMiniCarteCoins(
	TEXT("Chelles.MiniCarteCoins"), GMiniCarteCoins,
	TEXT("Mini-carte : 1 = vue du ciel a coins arrondis (par defaut), 0 = vue du ciel carree (si l'image n'apparait pas)."));

static int32 GCacherBulleAide = 1;
static FAutoConsoleVariableRef CVarCacherBulleAide(
	TEXT("Chelles.CacherBulleAide"), GCacherBulleAide,
	TEXT("1 = cache la bulle d'aide 'Show Controls Tutorial / Show Simple Controls Guide' du pack SuperheroFlight (par defaut), 0 = la laisser."));

static float GMiniCarteNord = -90.f;
static FAutoConsoleVariableRef CVarMiniCarteNord(
	TEXT("Chelles.MiniCarteNord"), GMiniCarteNord,
	TEXT("Mini-carte : direction du nord en degres dans Unreal (-90 = vers -Y, 0 = vers +X)."));

namespace
{
	/** Cherche une variable (C++ ou Blueprint) par son nom, sans tenir compte des majuscules. */
	FProperty* VariableHud(const UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* Nom : Noms)
		{
			for (TFieldIterator<FProperty> It(Objet->GetClass()); It; ++It)
			{
				if (It->GetName().Equals(Nom, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	bool LireNombreHud(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(VariableHud(Objet, Noms));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? (float)P->GetFloatingPointPropertyValue(Adresse) : (float)P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	bool LireBooleenHud(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(VariableHud(Objet, Noms));
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	/** Lit une variable FVector (ou FVector_NetQuantize) par son nom. */
	bool LireVecteurHud(const UObject* Objet, const TCHAR* Nom, FVector& Valeur)
	{
		FStructProperty* P = CastField<FStructProperty>(VariableHud(Objet, { Nom }));
		// FVector ou FVector_NetQuantize (meme contenu : X, Y, Z)
		if (!P || !P->Struct || !(P->Struct->IsChildOf(TBaseStructure<FVector>::Get()) || P->Struct->GetName().StartsWith(TEXT("Vector")))
			|| P->Struct->GetStructureSize() < (int32)sizeof(FVector))
		{
			return false;
		}
		Valeur = *P->ContainerPtrToValuePtr<FVector>(Objet);
		return true;
	}

	/** Lit un tableau de nombres (ex. FinEffet du relais des pouvoirs). */
	bool LireTableauHud(const UObject* Objet, const TCHAR* Nom, TArray<float>& Valeurs)
	{
		FArrayProperty* P = CastField<FArrayProperty>(VariableHud(Objet, { Nom }));
		FNumericProperty* Dedans = P ? CastField<FNumericProperty>(P->Inner) : nullptr;
		if (!Dedans || !Dedans->IsFloatingPoint())
		{
			return false;
		}
		FScriptArrayHelper Tableau(P, P->ContainerPtrToValuePtr<void>(Objet));
		Valeurs.Reset();
		for (int32 i = 0; i < Tableau.Num(); ++i)
		{
			Valeurs.Add((float)Dedans->GetFloatingPointPropertyValue(Tableau.GetRawPtr(i)));
		}
		return true;
	}

	/** Lit un tableau d'acteurs (ex. Zombies de l'invasion). */
	void LireActeursHud(const UObject* Objet, const TCHAR* Nom, TArray<AActor*>& Acteurs)
	{
		FArrayProperty* P = CastField<FArrayProperty>(VariableHud(Objet, { Nom }));
		FObjectPropertyBase* Dedans = P ? CastField<FObjectPropertyBase>(P->Inner) : nullptr;
		if (!Dedans)
		{
			return;
		}
		FScriptArrayHelper Tableau(P, P->ContainerPtrToValuePtr<void>(Objet));
		for (int32 i = 0; i < Tableau.Num(); ++i)
		{
			if (AActor* A = Cast<AActor>(Dedans->GetObjectPropertyValue(Tableau.GetRawPtr(i))))
			{
				Acteurs.Add(A);
			}
		}
	}

	/** Classe C++ d'un autre plugin, trouvee par son nom (nullptr si le plugin n'est pas charge). */
	UClass* ClasseHud(const TCHAR* Nom)
	{
		return FindObject<UClass>(ANY_PACKAGE, Nom);
	}
}

AHudChelles::AHudChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bReplicates = false;
	SetActorHiddenInGame(true);
}

void AHudChelles::BeginPlay()
{
	Super::BeginPlay();
	CreerHud();
	UE_LOG(LogTemp, Log, TEXT("[HUD] HUD Chelles pret (vie, XP, pouvoir, zombies). Console : Chelles.Hud 0 / 1."));
}

void AHudChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	RetirerHud();
	RetirerVueDuCiel();
	Super::EndPlay(Raison);
}

APlayerController* AHudChelles::GetPC() const
{
	UWorld* W = GetWorld();
	return W ? W->GetFirstPlayerController() : nullptr;
}

void AHudChelles::CreerHud()
{
	if (Hud.IsValid() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UWorld* W = GetWorld();
	UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr;
	if (!Vue)
	{
		return;
	}
	Hud = SNew(SHudChelles);
	// Couche 3 : sous le panneau des pouvoirs (4) et sous les menus
	Vue->AddViewportWidgetContent(Hud.ToSharedRef(), 3);
}

void AHudChelles::RetirerHud()
{
	if (!Hud.IsValid())
	{
		return;
	}
	// la mini-carte ne doit plus montrer l'image de la vue du ciel
	Hud->BrushCarte.SetResourceObject(nullptr);
	Hud->Etat.bImageCarte = false;
	UWorld* W = GetWorld();
	if (UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Hud.ToSharedRef());
	}
	Hud.Reset();
}

void AHudChelles::ChercherActeurs()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	if (!Pouvoirs.IsValid())
	{
		if (UClass* C = ClasseHud(TEXT("PouvoirsChelles")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				Pouvoirs = *It;
				break;
			}
		}
	}
	APlayerController* PC = GetPC();
	if (PC && (!Relais.IsValid() || Relais->GetOwner() != PC))
	{
		Relais = nullptr;
		if (UClass* C = ClasseHud(TEXT("PouvoirsRelais")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				if (It->GetOwner() == PC)
				{
					Relais = *It;
					break;
				}
			}
		}
	}
	if (!Missions.IsValid())
	{
		if (UClass* C = ClasseHud(TEXT("MissionsChelles")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				Missions = *It;
				break;
			}
		}
	}
	if (!Invasion.IsValid())
	{
		if (UClass* C = ClasseHud(TEXT("InvasionZombies")))
		{
			for (TActorIterator<AActor> It(W, C); It; ++It)
			{
				Invasion = *It;
				break;
			}
		}
	}
}

void AHudChelles::LireVie()
{
	FEtatHudChelles& E = Hud->Etat;
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;

	float Vie = 0.f;
	float Max = 0.f;
	E.bVie = Pion && LireNombreHud(Pion, { TEXT("HealthCurrent"), TEXT("Health"), TEXT("CurrentHealth"), TEXT("Vie") }, Vie);
	if (!E.bVie)
	{
		VieAvant = -1.f;
		PionAvant = Pion;
		return;
	}
	if (!LireNombreHud(Pion, { TEXT("HealthMax"), TEXT("MaxHealth"), TEXT("VieMax") }, Max) || Max <= 0.f)
	{
		Max = FMath::Max(100.f, Vie);
	}
	bool bMort = false;
	LireBooleenHud(Pion, { TEXT("IsDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort);

	// Coup recu : la vie baisse sur le meme personnage
	if (PionAvant.Get() == Pion && VieAvant >= 0.f && Vie < VieAvant - 0.01f)
	{
		E.AgeCoup = 0.f;
	}
	VieAvant = Vie;
	PionAvant = Pion;

	E.Vie = Vie;
	E.VieMax = Max;
	E.bMort = bMort || Vie <= 0.f;
}

void AHudChelles::LirePouvoirs()
{
	FEtatHudChelles& E = Hud->Etat;
	AActor* Gestion = Pouvoirs.Get();
	AActor* Mien = Relais.Get();
	E.bPouvoirs = Gestion != nullptr || Mien != nullptr;
	if (!E.bPouvoirs)
	{
		return;
	}

	// XP : celle de ce PC (a jour tout de suite) si on peut la lire, sinon celle connue du serveur
	float Nombre = 0.f;
	if (Gestion && LireNombreHud(Gestion, { TEXT("XPLocale") }, Nombre))
	{
		E.XP = FMath::RoundToInt(Nombre);
	}
	else if (Mien && LireNombreHud(Mien, { TEXT("XP") }, Nombre))
	{
		E.XP = FMath::RoundToInt(Nombre);
	}
	E.Selection = (Gestion && LireNombreHud(Gestion, { TEXT("Selection") }, Nombre)) ? FMath::RoundToInt(Nombre) : -1;

	// Effet et recharge : heures de fin (horloge du serveur) copiees dans le relais
	for (int32 i = 0; i < ChellesHudDonnees::NombrePouvoirs; ++i)
	{
		E.Effet[i] = 0.f;
		E.Recharge[i] = 0.f;
	}
	if (!Mien)
	{
		return;
	}
	UWorld* W = GetWorld();
	const AGameStateBase* EtatJeu = W ? W->GetGameState() : nullptr;
	const float Maintenant = EtatJeu ? EtatJeu->GetServerWorldTimeSeconds() : (W ? W->GetTimeSeconds() : 0.f);
	TArray<float> Fins;
	if (LireTableauHud(Mien, TEXT("FinEffet"), Fins))
	{
		for (int32 i = 0; i < FMath::Min(Fins.Num(), ChellesHudDonnees::NombrePouvoirs); ++i)
		{
			// CAMELEON n'a pas de duree : pas de compte a rebours
			E.Effet[i] = (i == 3) ? 0.f : FMath::Max(0.f, Fins[i] - Maintenant);
		}
	}
	if (LireTableauHud(Mien, TEXT("FinRecharge"), Fins))
	{
		for (int32 i = 0; i < FMath::Min(Fins.Num(), ChellesHudDonnees::NombrePouvoirs); ++i)
		{
			E.Recharge[i] = FMath::Max(0.f, Fins[i] - Maintenant);
		}
	}
}

void AHudChelles::LireZombies()
{
	FEtatHudChelles& E = Hud->Etat;
	AActor* Chef = Invasion.Get();
	E.bZombies = Chef != nullptr;
	if (!Chef)
	{
		E.bInvasion = false;
		return;
	}
	bool bEnCours = false;
	float Nombre = 0.f;
	LireBooleenHud(Chef, { TEXT("bInvasionEnCours") }, bEnCours);
	E.bInvasion = bEnCours;
	E.ZombiesRestants = LireNombreHud(Chef, { TEXT("ZombiesRestants") }, Nombre) ? FMath::RoundToInt(Nombre) : 0;
	E.SecondesInvasion = LireNombreHud(Chef, { TEXT("SecondesRestantes") }, Nombre) ? Nombre : 0.f;
}

// ---------------------------------------------------------------------------
// Mini-carte
// ---------------------------------------------------------------------------

namespace
{
	enum : uint8
	{
		GenreTeleporteur = 1,
		GenreZombie = 2,
		GenreCiel = 4
	};

	/** Devine ce qu'est une classe d'apres son nom et celui de ses parents (BP_Teleporter_C, BP_AI_CharZombie_C, BP_GoodSky_C...). */
	uint8 DevinerGenre(UClass* Classe)
	{
		uint8 Genre = 0;
		const bool bPion = Classe->IsChildOf(APawn::StaticClass());
		for (UClass* K = Classe; K && K != AActor::StaticClass(); K = K->GetSuperClass())
		{
			const FString Nom = K->GetName();
			if (Nom.Contains(TEXT("Teleport")))
			{
				Genre |= GenreTeleporteur;
			}
			if (bPion && Nom.Contains(TEXT("Zombie")))
			{
				Genre |= GenreZombie;
			}
			if (!bPion && (Nom.Contains(TEXT("Sky")) || Nom.Contains(TEXT("Cloud")) || Nom.Contains(TEXT("Nuage"))
				|| Nom.Contains(TEXT("Ciel")) || Nom.Contains(TEXT("Atmos"))))
			{
				Genre |= GenreCiel;
			}
		}
		// un teleporteur n'est jamais compte comme zombie ou comme ciel
		if (Genre & GenreTeleporteur)
		{
			Genre = GenreTeleporteur;
		}
		return Genre;
	}
}

void AHudChelles::PreparerVueDuCiel()
{
	if (VueDuCiel || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// L'image est gardee jusqu'a la fin de la partie (meme si on passe en mode 1), pour que l'affichage ne la perde jamais
	if (!ImageCarte)
	{
		ImageCarte = NewObject<UTextureRenderTarget2D>(this);
		ImageCarte->ClearColor = FLinearColor(0.02f, 0.05f, 0.1f, 1.f);
		// couleurs lineaires (la couleur de base des materiaux, sans lumiere : la carte reste lisible la nuit)
		// meme forme que la carte (plus large que haute)
		ImageCarte->InitCustomFormat(ChellesCarte::ImageX, ChellesCarte::ImageY, PF_B8G8R8A8, true);
		ImageCarte->UpdateResourceImmediate(true);
	}

	VueDuCiel = NewObject<USceneCaptureComponent2D>(this);
	VueDuCiel->ProjectionType = ECameraProjectionMode::Orthographic;
	VueDuCiel->OrthoWidth = 2.f * 100.f * FMath::Clamp(GMiniCarteZoom, 20.f, 500.f);
	VueDuCiel->CaptureSource = SCS_BaseColor;
	VueDuCiel->bCaptureEveryFrame = false;
	VueDuCiel->bCaptureOnMovement = false;
	VueDuCiel->TextureTarget = ImageCarte;
	// Rien d'inutile sur la vue du ciel : pas de personnages (ils sont dessines en points), ni neige, ni brouillard, ni ombres
	VueDuCiel->ShowFlags.SetSkeletalMeshes(false);
	VueDuCiel->ShowFlags.SetParticles(false);
	VueDuCiel->ShowFlags.SetTranslucency(false);
	VueDuCiel->ShowFlags.SetFog(false);
	VueDuCiel->ShowFlags.SetVolumetricFog(false);
	VueDuCiel->ShowFlags.SetAtmosphere(false);
	VueDuCiel->ShowFlags.SetDynamicShadows(false);
	VueDuCiel->ShowFlags.SetInstancedGrass(false);
	VueDuCiel->ShowFlags.SetMotionBlur(false);
	VueDuCiel->ShowFlags.SetBloom(false);
	VueDuCiel->RegisterComponent();
	ProchainePointsCarte = 0.f; // relance la recherche du ciel a cacher tout de suite

	if (Hud.IsValid())
	{
		Hud->BrushCarte.SetResourceObject(ImageCarte);
		Hud->BrushCarte.ImageSize = FVector2D(ChellesCarte::ImageX, ChellesCarte::ImageY);
	}
	UE_LOG(LogTemp, Log, TEXT("[HUD] Mini-carte : vue du ciel prete (Chelles.MiniCarte 1 pour l'alleger, 0 pour la cacher)."));
}

void AHudChelles::RetirerVueDuCiel()
{
	if (Hud.IsValid())
	{
		Hud->BrushCarte.SetResourceObject(nullptr);
		Hud->Etat.bImageCarte = false;
	}
	if (VueDuCiel)
	{
		VueDuCiel->TextureTarget = nullptr;
		VueDuCiel->DestroyComponent();
		VueDuCiel = nullptr;
	}
}

void AHudChelles::ChercherPointsCarte()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		return;
	}
	Teleporteurs.Reset();
	ZombiesVus.Reset();
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->IsPendingKillPending())
		{
			continue;
		}
		UClass* Classe = A->GetClass();
		uint8* Connu = GenreClasse.Find(Classe);
		const uint8 Genre = Connu ? *Connu : GenreClasse.Add(Classe, DevinerGenre(Classe));
		if (Genre & GenreTeleporteur)
		{
			Teleporteurs.Add(A);
		}
		else if (Genre & GenreZombie)
		{
			ZombiesVus.Add(A);
		}
		if ((Genre & GenreCiel) && VueDuCiel && !VueDuCiel->HiddenActors.Contains(A))
		{
			VueDuCiel->HiddenActors.Add(A);
		}
	}
}

void AHudChelles::LireCarte()
{
	FEtatHudChelles& E = Hud->Etat;
	E.PointsCarte.Reset();
	E.bCarte = GMiniCarteMode > 0;
	if (!E.bCarte)
	{
		RetirerVueDuCiel();
		return;
	}
	APlayerController* PC = GetPC();
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	UWorld* W = GetWorld();
	if (!Pion || !W)
	{
		E.bCarte = false;
		return;
	}

	const float Horloge = W->GetRealTimeSeconds();
	if (Horloge >= ProchainePointsCarte)
	{
		ProchainePointsCarte = Horloge + 2.f;
		ChercherPointsCarte();
	}

	// La carte tourne avec la camera : ce qui est devant le joueur est en haut
	const FVector Centre = Pion->GetActorLocation();
	const float Cap = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : PC->GetControlRotation().Yaw;
	const float CapRad = FMath::DegreesToRadians(Cap);
	const FVector2D Devant(FMath::Cos(CapRad), FMath::Sin(CapRad));
	const FVector2D Droite(-Devant.Y, Devant.X);
	const float RayonMetres = FMath::Clamp(GMiniCarteZoom, 20.f, 500.f);
	const float Rayon = RayonMetres * 100.f;
	E.CarteRayonMetres = RayonMetres;

	auto VersCarte = [&](const FVector2D& Direction) -> FVector2D
	{
		return FVector2D(FVector2D::DotProduct(Direction, Droite), -FVector2D::DotProduct(Direction, Devant));
	};
	auto Ajouter = [&](const AActor* A, FPointCarteChelles::EType Type, bool bGarderAuBord)
	{
		const FVector L = A->GetActorLocation();
		FPointCarteChelles Point;
		Point.Type = Type;
		Point.Pos = VersCarte(FVector2D(L.X - Centre.X, L.Y - Centre.Y)) / Rayon;
		Point.bGarderAuBord = bGarderAuBord;
		// trop loin et sans interet au bord : inutile de l'envoyer au dessin
		if (!bGarderAuBord && Point.Pos.SizeSquared() > 4.f)
		{
			return;
		}
		E.PointsCarte.Add(Point);
	};

	const float NordRad = FMath::DegreesToRadians(GMiniCarteNord);
	E.CarteNord = VersCarte(FVector2D(FMath::Cos(NordRad), FMath::Sin(NordRad)));

	// Teleporteurs : toujours visibles, colles au bord s'ils sont loin
	for (const TWeakObjectPtr<AActor>& T : Teleporteurs)
	{
		if (T.IsValid())
		{
			Ajouter(T.Get(), FPointCarteChelles::Teleporteur, true);
		}
	}

	// Autres joueurs (multijoueur)
	if (const AGameStateBase* EtatJeu = W->GetGameState())
	{
		for (const APlayerState* Joueur : EtatJeu->PlayerArray)
		{
			const APawn* Autre = Joueur ? Joueur->GetPawn() : nullptr;
			if (Autre && Autre != Pion && !Autre->IsHidden())
			{
				Ajouter(Autre, FPointCarteChelles::Joueur, true);
			}
		}
	}

	// Zombies : ceux de l'invasion (a jour tout de suite) + ceux trouves dans le niveau
	TArray<AActor*> Zombies;
	if (AActor* Chef = Invasion.Get())
	{
		LireActeursHud(Chef, TEXT("Zombies"), Zombies);
	}
	for (const TWeakObjectPtr<AActor>& Z : ZombiesVus)
	{
		if (Z.IsValid())
		{
			Zombies.AddUnique(Z.Get());
		}
	}
	for (AActor* Z : Zombies)
	{
		if (!Z || Z->IsPendingKillPending() || Z->IsHidden() || Z == Pion)
		{
			continue;
		}
		Ajouter(Z, FPointCarteChelles::Zombie, false);
	}

	// Itineraire cyan + distance en km
	E.bCoinsArrondis = GMiniCarteCoins != 0;
	if (GMiniCarteItineraire && Horloge >= ProchainItineraire)
	{
		ProchainItineraire = Horloge + 0.5f;
		CalculerItineraire(Centre);
	}
	E.bDestination = GMiniCarteItineraire && bDestination && TrajetMonde.Num() >= 2;
	E.Itineraire.Reset();
	if (E.bDestination)
	{
		float Longueur = 0.f;
		for (int32 i = 0; i < TrajetMonde.Num(); ++i)
		{
			// le trajet part toujours de la position actuelle du joueur
			const FVector P = (i == 0) ? Centre : TrajetMonde[i];
			if (i > 0)
			{
				const FVector Avant = (i == 1) ? Centre : TrajetMonde[i - 1];
				Longueur += FVector::Dist2D(Avant, P);
			}
			E.Itineraire.Add(VersCarte(FVector2D(P.X - Centre.X, P.Y - Centre.Y)) / Rayon);
		}
		const FVector Fin = TrajetMonde.Last();
		E.Destination = VersCarte(FVector2D(Fin.X - Centre.X, Fin.Y - Centre.Y)) / Rayon;
		E.DistanceKm = Longueur / 100000.f;
		E.NomDestination = NomDestination;
	}

	// Vue du ciel (mode 2) : une photo vue de haut toutes les 0,1 s ; le joueur est en bas de la carte, on voit surtout devant
	if (GMiniCarteMode < 2)
	{
		RetirerVueDuCiel();
		return;
	}
	PreparerVueDuCiel();
	if (!VueDuCiel || !ImageCarte)
	{
		return;
	}
	if (Hud->BrushCarte.GetResourceObject() != ImageCarte)
	{
		Hud->BrushCarte.SetResourceObject(ImageCarte);
		Hud->BrushCarte.ImageSize = FVector2D(ChellesCarte::ImageX, ChellesCarte::ImageY);
	}
	if (Horloge >= ProchaineVueDuCiel)
	{
		ProchaineVueDuCiel = Horloge + 0.1f;
		VueDuCiel->OrthoWidth = 2.f * Rayon;
		// 80 m au-dessus, regard vers le sol ; le haut de l'image = devant le joueur.
		// Le joueur est dessine a PlaceJoueur de la hauteur : le centre de l'image est donc un peu devant lui.
		const float Avance = (ChellesCarte::PlaceJoueur - 0.5f) * ChellesCarte::Hauteur / (ChellesCarte::Largeur * 0.5f) * Rayon;
		const FVector CentreImage = Centre + FVector(Devant.X, Devant.Y, 0.f) * Avance;
		VueDuCiel->SetWorldLocationAndRotation(CentreImage + FVector(0.f, 0.f, 8000.f), FRotator(-90.f, Cap, 0.f));
		VueDuCiel->CaptureSceneDeferred();
		E.bImageCarte = true;
	}
}

void AHudChelles::CalculerItineraire(const FVector& Depart)
{
	bDestination = false;
	UWorld* W = GetWorld();
	if (!W)
	{
		TrajetMonde.Reset();
		return;
	}

	// 1) La mission en cours (plugin ChellesMissions), si elle donne un endroit ou aller
	FVector Cible = FVector::ZeroVector;
	bool bConnue = false;
	if (AActor* M = Missions.Get())
	{
		LireBooleenHud(M, { TEXT("bCibleConnue") }, bConnue);
		bConnue = bConnue && LireVecteurHud(M, TEXT("Cible"), Cible) && !Cible.IsNearlyZero();
	}
	if (bConnue)
	{
		NomDestination = TEXT("MISSION");
	}
	else
	{
		// 2) Sinon le teleporteur le plus proche (en ignorant celui sur lequel on est deja)
		float Meilleure = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& T : Teleporteurs)
		{
			if (!T.IsValid())
			{
				continue;
			}
			const float D = FVector::DistSquared2D(T->GetActorLocation(), Depart);
			if (D > FMath::Square(1500.f) && D < Meilleure)
			{
				Meilleure = D;
				Cible = T->GetActorLocation();
				bConnue = true;
			}
		}
		NomDestination = TEXT("T\u00C9L\u00C9PORTEUR");
	}
	if (!bConnue)
	{
		TrajetMonde.Reset();
		return;
	}

	// Trajet par les rues (NavMesh) si possible, sinon en ligne droite
	TrajetMonde.Reset();
	if (UNavigationPath* Chemin = UNavigationSystemV1::FindPathToLocationSynchronously(W, Depart, Cible, GetPC() ? GetPC()->GetPawn() : nullptr))
	{
		if (Chemin->IsValid() && Chemin->PathPoints.Num() >= 2)
		{
			TrajetMonde = Chemin->PathPoints;
			// chemin incomplet : on finit en ligne droite jusqu'a la destination
			if (FVector::DistSquared2D(TrajetMonde.Last(), Cible) > FMath::Square(300.f))
			{
				TrajetMonde.Add(Cible);
			}
		}
	}
	if (TrajetMonde.Num() < 2)
	{
		TrajetMonde.Reset();
		TrajetMonde.Add(Depart);
		TrajetMonde.Add(Cible);
	}
	bDestination = true;
}

void AHudChelles::CacherBulleAide()
{
	if (!GCacherBulleAide)
	{
		return;
	}
	// La bulle est dans WBP_MainHUD, que BP_Player (SuperheroFlight) ajoute a l'ecran : ce widget ne contient que des aides
	// (bulle T / Tab, guide des commandes de vol, video du tutoriel). On le cache, sans modifier aucun Blueprint du pack.
	// Si le jeu le remontre (touche T ou F1), il est recache dans la demi-seconde.
	TArray<UObject*> Widgets;
	GetObjectsOfClass(UUserWidget::StaticClass(), Widgets, true);
	for (UObject* Objet : Widgets)
	{
		UUserWidget* Widget = Cast<UUserWidget>(Objet);
		if (!Widget || Widget->IsPendingKill() || Widget->HasAnyFlags(RF_ClassDefaultObject) || Widget->GetWorld() != GetWorld())
		{
			continue;
		}
		const UClass* Classe = Widget->GetClass();
		const bool bBulle = Classe->GetPathName().Contains(TEXT("/SuperheroFlight/"))
			&& (Classe->GetName().StartsWith(TEXT("WBP_MainHUD")) || Classe->GetName().StartsWith(TEXT("WBP_Tutorial_Box")));
		if (bBulle && Widget->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
			UE_LOG(LogTemp, Log, TEXT("[HUD] Bulle d'aide SuperheroFlight cachee (%s). Console : Chelles.CacherBulleAide 0 pour la garder."), *Widget->GetName());
		}
	}
}

void AHudChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Hud.IsValid())
	{
		CreerHud();
		if (!Hud.IsValid())
		{
			return;
		}
	}
	UWorld* W = GetWorld();
	const float Reel = float(FApp::GetDeltaTime());
	FEtatHudChelles& E = Hud->Etat;
	E.Horloge += Reel;
	E.AgeCoup += Reel;

	// Bulle d'aide du pack de vol, toutes les 0,5 s, meme quand le HUD est cache
	if (W && W->GetRealTimeSeconds() >= ProchaineBulleAide)
	{
		ProchaineBulleAide = W->GetRealTimeSeconds() + 0.5f;
		CacherBulleAide();
	}

	// Pas de HUD pendant le menu, la pause, le chat ou les options (souris visible), ni sans personnage
	APlayerController* PC = GetPC();
	E.bVisible = bAfficher && GHudChellesAffiche && PC && PC->GetPawn() && !PC->IsPaused() && !PC->bShowMouseCursor;
	if (!E.bVisible)
	{
		return;
	}

	const float Horloge = W ? W->GetRealTimeSeconds() : 0.f;
	if (Horloge >= ProchaineRecherche)
	{
		ProchaineRecherche = Horloge + 1.f;
		ChercherActeurs();
	}
	LireVie();
	LirePouvoirs();
	LireZombies();
	LireCarte();
}
