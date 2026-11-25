// CHELLES 3D - Agent 5 "Testeur" : parcours automatique (voir TesteurChelles.h)
#include "TesteurChelles.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "RHI.h"

DEFINE_LOG_CATEGORY_STATIC(LogChellesTesteur, Log, All);

// ============================================================================
//  Compteur d'erreurs du journal : on sait a quelle etape chaque erreur arrive.
// ============================================================================
namespace ChellesTesteurJournal
{
	class FCompteur : public FOutputDevice
	{
	public:
		FCriticalSection Verrou;
		int32 Erreurs = 0;
		int32 Avertissements = 0;
		TArray<FString> Derniers; // dernieres erreurs (pour l'etape en cours)

		virtual void Serialize(const TCHAR* Texte, ELogVerbosity::Type Verbosite, const FName& Categorie) override
		{
			const ELogVerbosity::Type Niveau = (ELogVerbosity::Type)(Verbosite & ELogVerbosity::VerbosityMask);
			if (Niveau > ELogVerbosity::Warning || Categorie == TEXT("LogChellesTesteur"))
			{
				return;
			}
			FScopeLock Garde(&Verrou);
			if (Niveau == ELogVerbosity::Warning)
			{
				++Avertissements;
				return;
			}
			++Erreurs;
			if (Derniers.Num() < 5)
			{
				Derniers.Add(FString::Printf(TEXT("%s: %s"), *Categorie.ToString(), Texte).Left(300));
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }
	};

	static FCompteur* GCompteur = nullptr;

	static void Brancher()
	{
		if (!GCompteur && GLog)
		{
			GCompteur = new FCompteur();
			GLog->AddOutputDevice(GCompteur);
		}
	}

	static void Debrancher()
	{
		if (GCompteur && GLog)
		{
			GLog->RemoveOutputDevice(GCompteur);
		}
		// pas de delete : un autre fil peut encore ecrire au meme instant (fuite volontaire, une seule fois)
	}

	static void Lire(int32& Erreurs, int32& Avertissements, TArray<FString>* PrendreExemples = nullptr)
	{
		if (!GCompteur)
		{
			Erreurs = Avertissements = 0;
			return;
		}
		FScopeLock Garde(&GCompteur->Verrou);
		Erreurs = GCompteur->Erreurs;
		Avertissements = GCompteur->Avertissements;
		if (PrendreExemples)
		{
			*PrendreExemples = MoveTemp(GCompteur->Derniers);
			GCompteur->Derniers.Reset();
		}
	}
}

// ============================================================================

ATesteurChelles::ATesteurChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true; // les menus mettent le jeu en pause : le test continue
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetCanBeDamaged(false);
}

void ATesteurChelles::BeginPlay()
{
	Super::BeginPlay();
	SetTickableWhenPaused(true);
	ChellesTesteurJournal::Brancher();
	LireReglages();
	DebutTest = FPlatformTime::Seconds();
	DebutPhase = DebutTest;
	ChargementSecondes = DebutTest - GStartTime;
	UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] Debut du test automatique (carte chargee en %.0f s)."), ChargementSecondes);
	EcrireResultats(false);
}

void ATesteurChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (Phase != ETestPhase::Fin)
	{
		// la carte se ferme avant la fin (changement de carte, plantage d'un Blueprint qui recharge...)
		if (Etapes.IsValidIndex(EtapeEnCours))
		{
			Etapes[EtapeEnCours].Etat = TEXT("probleme");
			Etapes[EtapeEnCours].Problemes.Add(TEXT("La carte s'est fermee pendant cette etape."));
		}
		EcrireResultats(false);
	}
	ChellesTesteurJournal::Debrancher();
	Super::EndPlay(Raison);
}

// ---------------------------------------------------------------------------
//  Reglages
// ---------------------------------------------------------------------------
void ATesteurChelles::LireReglages()
{
	Modes = { TEXT("Soldat"), TEXT("Spider-Male"), TEXT("Spider-Femelle"), TEXT("PANTERA"), TEXT("FLASH ECLAIR"), TEXT("FLAMME FIRE"),
		TEXT("SuperPowers"), TEXT("DYNAMIC COMBAT"), TEXT("Motoneige"), TEXT("VOITURE"), TEXT("HORSE") };
	ModeFinal = TEXT("Soldat");

	const FString Fichier = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesTesteur"), TEXT("Testeur.txt"));
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
	{
		UE_LOG(LogChellesTesteur, Warning, TEXT("[Testeur] %s introuvable : reglages par defaut."), *Fichier);
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")))
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
		const bool bOui = Valeur.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Valeur == TEXT("1");
		if (Cle.Equals(TEXT("Modes"), ESearchCase::IgnoreCase))
		{
			Modes.Reset();
			TArray<FString> Morceaux;
			Valeur.ParseIntoArray(Morceaux, TEXT(","), true);
			for (FString& M : Morceaux)
			{
				M.TrimStartAndEndInline();
				if (!M.IsEmpty())
				{
					Modes.Add(M);
				}
			}
		}
		else if (Cle.Equals(TEXT("Grille"), ESearchCase::IgnoreCase))          { TailleGrille = FMath::Clamp(FCString::Atoi(*Valeur), 0, 6); }
		else if (Cle.Equals(TEXT("SecondesParZone"), ESearchCase::IgnoreCase)) { SecondesParZone = FMath::Clamp(FCString::Atof(*Valeur), 3.f, 60.f); }
		else if (Cle.Equals(TEXT("SecondesParMode"), ESearchCase::IgnoreCase)) { SecondesParMode = FMath::Clamp(FCString::Atof(*Valeur), 3.f, 60.f); }
		else if (Cle.Equals(TEXT("SecondesMarche"), ESearchCase::IgnoreCase))  { SecondesMarche = FMath::Clamp(FCString::Atof(*Valeur), 2.f, 30.f); }
		else if (Cle.Equals(TEXT("AttenteDepart"), ESearchCase::IgnoreCase))   { AttenteDepart = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 120.f); }
		else if (Cle.Equals(TEXT("FPSFaible"), ESearchCase::IgnoreCase))       { FPSFaible = FMath::Clamp(FCString::Atof(*Valeur), 5.f, 200.f); }
		else if (Cle.Equals(TEXT("Menus"), ESearchCase::IgnoreCase))           { bMenus = bOui; }
		else if (Cle.Equals(TEXT("Marche"), ESearchCase::IgnoreCase))          { bMarche = bOui; }
		else if (Cle.Equals(TEXT("ModeFinal"), ESearchCase::IgnoreCase))       { ModeFinal = Valeur; }
		else if (Cle.Equals(TEXT("Point"), ESearchCase::IgnoreCase))
		{
			// Point = Nom ; X ; Y ; Z   (coordonnees prises dans l'editeur, Z facultatif)
			TArray<FString> Morceaux;
			Valeur.ParseIntoArray(Morceaux, TEXT(";"), false);
			if (Morceaux.Num() >= 3)
			{
				FPointTest P;
				P.Nom = Morceaux[0].TrimStartAndEnd();
				P.Lieu = FVector(FCString::Atof(*Morceaux[1]), FCString::Atof(*Morceaux[2]), Morceaux.Num() >= 4 ? FCString::Atof(*Morceaux[3]) : 0.f);
				PointsEnPlus.Add(P);
			}
		}
	}
}

// ---------------------------------------------------------------------------
//  Aides
// ---------------------------------------------------------------------------
APlayerController* ATesteurChelles::PC() const
{
	UWorld* Monde = GetWorld();
	return Monde ? Monde->GetFirstPlayerController() : nullptr;
}

APawn* ATesteurChelles::Pion() const
{
	APlayerController* Joueur = PC();
	return Joueur ? Joueur->GetPawn() : nullptr;
}

bool ATesteurChelles::CommandeExiste(const FString& Nom)
{
	return IConsoleManager::Get().FindConsoleObject(*Nom) != nullptr;
}

bool ATesteurChelles::Commande(const FString& Ligne, FEtapeTest& Etape)
{
	FString Nom = Ligne;
	FString Reste;
	Ligne.Split(TEXT(" "), &Nom, &Reste);
	if (!CommandeExiste(Nom))
	{
		Etape.Etat = TEXT("non teste");
		Etape.Remarques.Add(FString::Printf(TEXT("La commande %s n'existe pas (plugin absent ou pas compile)."), *Nom));
		return false;
	}
	UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] Commande : %s"), *Ligne);
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), *Ligne);
	}
	return true;
}

bool ATesteurChelles::Sol(const FVector& Point, FVector& Trouve) const
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return false;
	}
	FHitResult Impact;
	FCollisionQueryParams Params(FName(TEXT("ChellesTesteur")), true);
	if (APawn* P = Pion())
	{
		Params.AddIgnoredActor(P);
	}
	const FVector Haut(Point.X, Point.Y, 60000.f);
	const FVector Bas(Point.X, Point.Y, -60000.f);
	if (Monde->LineTraceSingleByChannel(Impact, Haut, Bas, ECC_Visibility, Params))
	{
		Trouve = Impact.ImpactPoint;
		return true;
	}
	return false;
}

void ATesteurChelles::Teleporter(const FVector& Lieu)
{
	APawn* P = Pion();
	if (!P)
	{
		return;
	}
	P->SetActorLocation(Lieu + FVector(0.f, 0.f, 150.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (ACharacter* Perso = Cast<ACharacter>(P))
	{
		if (UCharacterMovementComponent* Mouvement = Perso->GetCharacterMovement())
		{
			Mouvement->StopMovementImmediately();
		}
	}
}

void ATesteurChelles::TourneCamera(float Delta, float Duree)
{
	APlayerController* Joueur = PC();
	if (!Joueur || Duree <= 0.f)
	{
		return;
	}
	FRotator R = Joueur->GetControlRotation();
	R.Pitch = -8.f;
	R.Yaw += 360.f / Duree * Delta; // un tour complet pendant l'etape
	Joueur->SetControlRotation(R);
}

void ATesteurChelles::VerifierChute(FEtapeTest& Etape, float ZDepart)
{
	APawn* P = Pion();
	if (!P)
	{
		Etape.Problemes.Add(TEXT("Le personnage a disparu (plus de joueur controle)."));
		return;
	}
	const float Z = P->GetActorLocation().Z;
	const AWorldSettings* Reglages = GetWorld() ? GetWorld()->GetWorldSettings() : nullptr;
	if ((Reglages && Reglages->bEnableWorldBoundsChecks && Z < Reglages->KillZ + 500.f) || Z < ZDepart - 3000.f)
	{
		Etape.Problemes.Add(FString::Printf(TEXT("Le joueur est tombe sous la carte (hauteur %.0f m, depart %.0f m)."), Z / 100.f, ZDepart / 100.f));
	}
}

// ---------------------------------------------------------------------------
//  Construction du parcours
// ---------------------------------------------------------------------------
void ATesteurChelles::ConstruireEtapes()
{
	Etapes.Reset();
	// On commence toujours avec le premier personnage de la liste (le soldat) : les zones se visitent a pied.
	if (Modes.Num() > 0 && CommandeExiste(TEXT("Chelles.Raccourcis")) && GEngine)
	{
		GEngine->Exec(GetWorld(), *FString::Printf(TEXT("Chelles.Raccourcis Mode %s"), *Modes[0]));
	}
	AjouterZones();
	if (bMarche)
	{
		AjouterMarche();
	}
	if (bMenus)
	{
		AjouterMenus();
	}
	AjouterModes();
	bEtapesConstruites = true;
	UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] %d etapes au programme."), Etapes.Num());
}

void ATesteurChelles::AjouterZones()
{
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	TArray<FPointTest> Lieux;

	if (TailleGrille > 0)
	{
		// Etendue de la carte : positions des objets du decor, sans les 5 % les plus eloignes de chaque cote
		// (ciel, montagnes au loin...).
		TArray<float> Xs, Ys;
		for (TActorIterator<AStaticMeshActor> It(Monde); It; ++It)
		{
			const FVector L = It->GetActorLocation();
			Xs.Add(L.X);
			Ys.Add(L.Y);
		}
		if (Xs.Num() >= 20)
		{
			Xs.Sort();
			Ys.Sort();
			const int32 Bas = Xs.Num() * 5 / 100;
			const int32 Haut = Xs.Num() - 1 - Bas;
			const float X0 = Xs[Bas], X1 = Xs[Haut], Y0 = Ys[Bas], Y1 = Ys[Haut];
			// Noms simples : dans Unreal, X = avant (on l'appelle "nord"), Y = droite ("est")
			static const TCHAR* NomsLignes[] = { TEXT("sud"), TEXT("centre"), TEXT("nord") };
			static const TCHAR* NomsColonnes[] = { TEXT("ouest"), TEXT("centre"), TEXT("est") };
			int32 Numero = 0;
			for (int32 i = 0; i < TailleGrille; ++i)
			{
				for (int32 j = 0; j < TailleGrille; ++j)
				{
					++Numero;
					const float Fx = (i + 0.5f) / TailleGrille;
					const float Fy = (j + 0.5f) / TailleGrille;
					FPointTest P;
					P.Lieu = FVector(FMath::Lerp(X0, X1, Fx), FMath::Lerp(Y0, Y1, Fy), 0.f);
					if (TailleGrille == 3)
					{
						const FString Ligne = NomsLignes[i];
						const FString Colonne = NomsColonnes[j];
						const FString Coin = (Ligne == TEXT("centre") && Colonne == TEXT("centre")) ? TEXT("centre")
							: (Ligne == TEXT("centre") ? Colonne : (Colonne == TEXT("centre") ? Ligne : Ligne + TEXT("-") + Colonne));
						P.Nom = FString::Printf(TEXT("Zone %d (%s)"), Numero, *Coin);
					}
					else
					{
						P.Nom = FString::Printf(TEXT("Zone %d"), Numero);
					}
					Lieux.Add(P);
				}
			}
		}
	}
	Lieux.Append(PointsEnPlus);

	for (const FPointTest& Lieu : Lieux)
	{
		FEtapeTest E;
		E.Nom = Lieu.Nom;
		E.Categorie = TEXT("Zone");
		E.Duree = SecondesParZone;
		const FVector Cible = Lieu.Lieu;
		const int32 Index = Etapes.Num();
		E.Debut = [this, Cible, Index]()
		{
			FEtapeTest& Moi = Etapes[Index];
			FVector Point = Cible;
			if (Cible.Z == 0.f && !Sol(Cible, Point))
			{
				Moi.Etat = TEXT("non teste");
				Moi.Remarques.Add(TEXT("Aucun sol trouve a cet endroit (zone vide de la carte)."));
				return;
			}
			Moi.Position = Point;
			Moi.bPosition = true;
			Teleporter(Point);
			ZDepartEtape = Point.Z;
		};
		E.Pendant = [this](float Delta)
		{
			TourneCamera(Delta, SecondesParZone);
		};
		E.Fin = [this, Index]()
		{
			VerifierChute(Etapes[Index], ZDepartEtape);
		};
		Etapes.Add(MoveTemp(E));
	}
}

void ATesteurChelles::AjouterMarche()
{
	FEtapeTest E;
	E.Nom = TEXT("Marcher tout droit");
	E.Categorie = TEXT("Marche");
	E.Duree = SecondesMarche;
	const int32 Index = Etapes.Num();
	E.Debut = [this]()
	{
		APawn* P = Pion();
		DepartMarche = P ? P->GetActorLocation() : FVector::ZeroVector;
		ZDepartEtape = DepartMarche.Z;
	};
	E.Pendant = [this](float)
	{
		if (APawn* P = Pion())
		{
			if (APlayerController* Joueur = PC())
			{
				const FRotator Direction(0.f, Joueur->GetControlRotation().Yaw, 0.f);
				P->AddMovementInput(Direction.Vector(), 1.f);
			}
		}
	};
	E.Fin = [this, Index]()
	{
		FEtapeTest& Moi = Etapes[Index];
		APawn* P = Pion();
		if (!P)
		{
			Moi.Problemes.Add(TEXT("Le personnage a disparu pendant la marche."));
			return;
		}
		const float Distance = FVector::Dist2D(P->GetActorLocation(), DepartMarche) / 100.f;
		Moi.Remarques.Add(FString::Printf(TEXT("Distance parcourue : %.1f m."), Distance));
		if (Distance < 1.f)
		{
			Moi.Problemes.Add(TEXT("Le personnage n'a presque pas avance (mur devant lui ou commandes bloquees)."));
		}
		VerifierChute(Moi, ZDepartEtape);
	};
	Etapes.Add(MoveTemp(E));
}

void ATesteurChelles::AjouterMenus()
{
	struct FMenu { const TCHAR* Nom; const TCHAR* Ouvrir; const TCHAR* Fermer; };
	static const FMenu Menus[] =
	{
		{ TEXT("Menu ARMEMENTS (touche I)"), TEXT("Chelles.Armements Ouvrir"), TEXT("Chelles.Armements Fermer") },
		{ TEXT("Menu HABILLAGE (lobby)"),    TEXT("Chelles.Habillage"),        TEXT("Chelles.Menu Reprendre") },
		{ TEXT("Menu PAUSE (Echap)"),        TEXT("Chelles.Menu Pause"),       TEXT("Chelles.Menu Reprendre") },
	};
	for (const FMenu& M : Menus)
	{
		FEtapeTest E;
		E.Nom = M.Nom;
		E.Categorie = TEXT("Menu");
		E.Duree = 4.f;
		const FString Ouvrir = M.Ouvrir;
		const FString Fermer = M.Fermer;
		const int32 Index = Etapes.Num();
		E.Debut = [this, Ouvrir, Index]()
		{
			Commande(Ouvrir, Etapes[Index]);
		};
		E.Fin = [this, Fermer, Index]()
		{
			FEtapeTest& Moi = Etapes[Index];
			if (Moi.Etat == TEXT("non teste"))
			{
				return;
			}
			Commande(Fermer, Moi);
		};
		Etapes.Add(MoveTemp(E));

		// Petite etape de controle : le jeu doit repartir apres la fermeture du menu.
		FEtapeTest Controle;
		Controle.Nom = FString::Printf(TEXT("Retour au jeu apres %s"), M.Nom);
		Controle.Categorie = TEXT("Menu");
		Controle.Duree = 1.5f;
		const int32 IndexControle = Etapes.Num();
		Controle.Fin = [this, IndexControle, Index]()
		{
			FEtapeTest& Moi = Etapes[IndexControle];
			if (Etapes[Index].Etat == TEXT("non teste"))
			{
				Moi.Etat = TEXT("non teste");
				return;
			}
			if (UGameplayStatics::IsGamePaused(this))
			{
				Moi.Problemes.Add(TEXT("Le jeu reste en pause apres la fermeture du menu."));
				// on debloque quand meme pour la suite du test
				if (APlayerController* Joueur = PC())
				{
					Joueur->SetPause(false);
				}
			}
			if (APlayerController* Joueur = PC())
			{
				Joueur->SetInputMode(FInputModeGameOnly());
				Joueur->bShowMouseCursor = false;
			}
		};
		Etapes.Add(MoveTemp(Controle));
	}
}

void ATesteurChelles::AjouterModes()
{
	TArray<FString> Liste = Modes;
	if (!ModeFinal.IsEmpty())
	{
		Liste.Add(ModeFinal); // on revient au depart a la fin (verifie que le retour marche aussi)
	}
	for (int32 i = 0; i < Liste.Num(); ++i)
	{
		const FString Mode = Liste[i];
		FEtapeTest E;
		E.Nom = (i == Liste.Num() - 1 && !ModeFinal.IsEmpty()) ? FString::Printf(TEXT("Retour en %s"), *Mode) : Mode;
		E.Categorie = TEXT("Personnage");
		E.Duree = SecondesParMode;
		const int32 Index = Etapes.Num();
		E.Debut = [this, Mode, Index]()
		{
			FEtapeTest& Moi = Etapes[Index];
			APawn* P = Pion();
			PionAvant = P ? P->GetClass()->GetName() : FString();
			ZDepartEtape = P ? P->GetActorLocation().Z : 0.f;
			Commande(FString::Printf(TEXT("Chelles.Raccourcis Mode %s"), *Mode), Moi);
		};
		E.Pendant = [this](float Delta)
		{
			TourneCamera(Delta, SecondesParMode * 2.f);
		};
		E.Fin = [this, Index, i]()
		{
			FEtapeTest& Moi = Etapes[Index];
			if (Moi.Etat == TEXT("non teste"))
			{
				return;
			}
			APawn* P = Pion();
			if (!P)
			{
				Moi.Problemes.Add(TEXT("Plus de personnage controle apres le changement (ecran vide ou camera perdue)."));
				return;
			}
			if (i > 0 && P->GetClass()->GetName() == PionAvant)
			{
				Moi.Remarques.Add(TEXT("Le personnage n'a pas change de type (mode deja actif, indisponible, ou simple changement d'apparence)."));
			}
			VerifierChute(Moi, ZDepartEtape);
		};
		Etapes.Add(MoveTemp(E));
	}
}

// ---------------------------------------------------------------------------
//  Deroulement
// ---------------------------------------------------------------------------
void ATesteurChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Maintenant = FPlatformTime::Seconds();
	const float Delta = FMath::Max((float)FApp::GetDeltaTime(), 0.0001f); // vrai temps de l'image (pause et ralenti compris)


	switch (Phase)
	{
	case ETestPhase::AttenteJoueur:
	{
		// Le menu de depart passe tout seul en SOLO avec -ChellesTest (ChellesMenu) : on attend le joueur.
		APawn* P = Pion();
		if (P && !UGameplayStatics::IsGamePaused(this))
		{
			UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] Joueur pret (%s). Prechauffage %.0f s."), *P->GetClass()->GetName(), AttenteDepart);
			Phase = ETestPhase::Prechauffage;
			DebutPhase = Maintenant;
		}
		else if (Maintenant - DebutPhase > 30.0 && Maintenant >= ProchainSolo && CommandeExiste(TEXT("Chelles.Menu")) && GEngine)
		{
			ProchainSolo = Maintenant + 10.0;
			GEngine->Exec(GetWorld(), TEXT("Chelles.Menu Solo")); // le menu ne s'est pas ferme seul : on insiste
		}
		else if (Maintenant - DebutPhase > 180.0)
		{
			UE_LOG(LogChellesTesteur, Error, TEXT("[Testeur] Aucun joueur apres 3 minutes : test arrete."));
			FEtapeTest E;
			E.Nom = TEXT("Arrivee du joueur sur la carte");
			E.Categorie = TEXT("Depart");
			E.Etat = TEXT("probleme");
			E.Problemes.Add(TEXT("Aucun joueur controlable apres 3 minutes (menu bloque ou personnage pas cree)."));
			Etapes.Add(E);
			Quitter();
		}
		return;
	}
	case ETestPhase::Prechauffage:
		if (Maintenant - DebutPhase >= AttenteDepart)
		{
			ConstruireEtapes();
			Phase = ETestPhase::Etapes;
			CommencerEtape(0);
		}
		return;
	case ETestPhase::Fin:
		return;
	case ETestPhase::Etapes:
		break;
	}

	if (!Etapes.IsValidIndex(EtapeEnCours))
	{
		Quitter();
		return;
	}
	FEtapeTest& E = Etapes[EtapeEnCours];
	const double Ecoule = Maintenant - DebutEtape;

	// Mesure apres 1 seconde de stabilisation (la teleportation charge le decor d'un coup)
	if (Ecoule > 1.0 && E.Etat != TEXT("non teste"))
	{
		++E.Images;
		E.Temps += Delta;
		E.PireImageMs = FMath::Max(E.PireImageMs, Delta * 1000.f);
		if (Delta > 0.1f)
		{
			++E.Saccades;
		}
		++ImagesGlobal;
		TempsGlobal += Delta;
		++FenetreImages;
		FenetreTemps += Delta;
		if (FenetreTemps >= 0.5)
		{
			const FPlatformMemoryStats Memoire = FPlatformMemory::GetStats();
			MemoireMax = FMath::Max<uint64>(MemoireMax, Memoire.UsedPhysical);
			const float FPS = FenetreImages / FenetreTemps;
			E.FPSMin = (E.FPSMin <= 0.f) ? FPS : FMath::Min(E.FPSMin, FPS);
			FPSMinGlobal = (FPSMinGlobal <= 0.f) ? FPS : FMath::Min(FPSMinGlobal, FPS);
			FenetreImages = 0;
			FenetreTemps = 0.0;
		}
	}

	if (E.Pendant && E.Etat != TEXT("non teste"))
	{
		E.Pendant(Delta);
	}
	if (Ecoule >= E.Duree || E.Etat == TEXT("non teste"))
	{
		TerminerEtape();
		if (Etapes.IsValidIndex(EtapeEnCours + 1))
		{
			CommencerEtape(EtapeEnCours + 1);
		}
		else
		{
			Quitter();
		}
	}
}

void ATesteurChelles::CommencerEtape(int32 Index)
{
	EtapeEnCours = Index;
	DebutEtape = FPlatformTime::Seconds();
	FenetreImages = 0;
	FenetreTemps = 0.0;
	FEtapeTest& E = Etapes[Index];
	E.Etat = TEXT("en cours");
	TArray<FString> Ignore;
	ChellesTesteurJournal::Lire(ErreursDebut, AvertissementsDebut, &Ignore);
	UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] Etape %d/%d : %s"), Index + 1, Etapes.Num(), *E.Nom);
	if (E.Debut)
	{
		E.Debut();
	}
	EcrireResultats(false); // en cas de plantage, on saura a quelle etape
}

void ATesteurChelles::TerminerEtape()
{
	if (!Etapes.IsValidIndex(EtapeEnCours))
	{
		return;
	}
	FEtapeTest& E = Etapes[EtapeEnCours];
	if (E.Fin && E.Etat != TEXT("non teste"))
	{
		E.Fin();
	}
	int32 Erreurs = 0, Avertissements = 0;
	ChellesTesteurJournal::Lire(Erreurs, Avertissements, &E.ExemplesErreurs);
	E.Erreurs = Erreurs - ErreursDebut;
	E.Avertissements = Avertissements - AvertissementsDebut;
	if (APawn* P = Pion())
	{
		E.Pion = P->GetClass()->GetName();
	}
	if (E.Etat != TEXT("non teste"))
	{
		const float Moyenne = E.Temps > 0.0 ? E.Images / E.Temps : 0.f;
		if (E.Images > 0 && Moyenne < FPSFaible && E.Categorie != TEXT("Menu"))
		{
			E.Problemes.Add(FString::Printf(TEXT("Le jeu rame ici : %.0f images par seconde en moyenne (bon = %.0f ou plus)."), Moyenne, FPSFaible));
		}
		if (E.Saccades >= 3)
		{
			E.Problemes.Add(FString::Printf(TEXT("%d saccades (images bloquees plus de 0,1 s, pire : %.0f ms)."), E.Saccades, E.PireImageMs));
		}
		E.Etat = E.Problemes.Num() > 0 ? TEXT("probleme") : TEXT("ok");
	}
	UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] -> %s : %s, %.0f FPS, %d erreur(s)"), *E.Nom, *E.Etat, E.Temps > 0.0 ? E.Images / E.Temps : 0.0, E.Erreurs);
}

void ATesteurChelles::Quitter()
{
	if (Phase == ETestPhase::Fin)
	{
		return;
	}
	Phase = ETestPhase::Fin;
	UE_LOG(LogChellesTesteur, Display, TEXT("[Testeur] FIN du test, fermeture du jeu."));
	EcrireResultats(true);
	// On laisse 2 secondes au journal pour s'ecrire, puis on ferme le jeu
	FTimerHandle Minuterie;
	GetWorldTimerManager().SetTimer(Minuterie, FTimerDelegate::CreateLambda([]()
	{
		FPlatformMisc::RequestExit(false);
	}), 2.f, false);
	// filet de securite si les minuteries sont en pause
	if (UGameplayStatics::IsGamePaused(this))
	{
		FPlatformMisc::RequestExit(false);
	}
}

void ATesteurChelles::EcrireResultats(bool bTermine) const
{
	TSharedRef<FJsonObject> Racine = MakeShared<FJsonObject>();
	Racine->SetBoolField(TEXT("termine"), bTermine);
	Racine->SetStringField(TEXT("date"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
	Racine->SetStringField(TEXT("carte"), GetWorld() ? GetWorld()->GetMapName() : FString());
	Racine->SetNumberField(TEXT("chargement_secondes"), ChargementSecondes);
	Racine->SetNumberField(TEXT("duree_test_secondes"), FPlatformTime::Seconds() - DebutTest);
	Racine->SetNumberField(TEXT("fps_moyen"), TempsGlobal > 0.0 ? ImagesGlobal / TempsGlobal : 0.0);
	Racine->SetNumberField(TEXT("fps_min"), FPSMinGlobal);
	Racine->SetNumberField(TEXT("memoire_max_mo"), (double)(MemoireMax / (1024ull * 1024ull)));
	Racine->SetStringField(TEXT("carte_graphique"), GRHIAdapterName);
	Racine->SetNumberField(TEXT("fps_faible"), FPSFaible);
	Racine->SetStringField(TEXT("etape_en_cours"), Etapes.IsValidIndex(EtapeEnCours) ? Etapes[EtapeEnCours].Nom : FString());

	TArray<TSharedPtr<FJsonValue>> Liste;
	for (const FEtapeTest& E : Etapes)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("nom"), E.Nom);
		O->SetStringField(TEXT("categorie"), E.Categorie);
		O->SetStringField(TEXT("etat"), E.Etat);
		O->SetNumberField(TEXT("fps_moyen"), E.Temps > 0.0 ? E.Images / E.Temps : 0.0);
		O->SetNumberField(TEXT("fps_min"), E.FPSMin);
		O->SetNumberField(TEXT("pire_image_ms"), E.PireImageMs);
		O->SetNumberField(TEXT("saccades"), E.Saccades);
		O->SetNumberField(TEXT("erreurs"), E.Erreurs);
		O->SetNumberField(TEXT("avertissements"), E.Avertissements);
		O->SetStringField(TEXT("personnage"), E.Pion);
		if (E.bPosition)
		{
			O->SetStringField(TEXT("position"), FString::Printf(TEXT("X=%.0f Y=%.0f Z=%.0f"), E.Position.X, E.Position.Y, E.Position.Z));
		}
		auto Tableau = [](const TArray<FString>& Textes)
		{
			TArray<TSharedPtr<FJsonValue>> T;
			for (const FString& S : Textes)
			{
				T.Add(MakeShared<FJsonValueString>(S));
			}
			return T;
		};
		O->SetArrayField(TEXT("problemes"), Tableau(E.Problemes));
		O->SetArrayField(TEXT("remarques"), Tableau(E.Remarques));
		O->SetArrayField(TEXT("exemples_erreurs"), Tableau(E.ExemplesErreurs));
		Liste.Add(MakeShared<FJsonValueObject>(O));
	}
	Racine->SetArrayField(TEXT("etapes"), Liste);

	FString Texte;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Ecrivain = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Texte);
	FJsonSerializer::Serialize(Racine, Ecrivain);
	const FString Fichier = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Testeur"), TEXT("resultats.json"));
	FFileHelper::SaveStringToFile(Texte, *Fichier, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
