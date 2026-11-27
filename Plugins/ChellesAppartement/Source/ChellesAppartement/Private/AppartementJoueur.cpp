// CHELLES 3D - L'appartement d'un joueur (voir AppartementChelles.h)
#include "AppartementChelles.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

using namespace ChellesAppart;

namespace
{
	// Dimensions de la piece (cm). Le sol est a Z = 0, le centre de la piece en (0, 0).
	const float Longueur = 3000.f; // X : de la porte (-1500) au garage (+1500)
	const float Largeur = 1600.f;  // Y : garde-robe (-800) / trophees (+800)
	const float HauteurPiece = 450.f;
	const float Epaisseur = 40.f;
	const FVector Arrivee(-1150.f, 0.f, 0.f);
	const float DebutGarage = 600.f;

	const TCHAR* NomsTrophees[NombreTrophees] = {
		TEXT("CHASSEUR DE ZOMBIES"), TEXT("HEROS DU QUARTIER"), TEXT("SURVIVANT DU REFUGE"),
		TEXT("EXPERIENCE"), TEXT("FIDELE DE CHELLES"), TEXT("COLLECTIONNEUR") };
	const TCHAR* UnitesTrophees[NombreTrophees] = {
		TEXT("zombies"), TEXT("missions"), TEXT("vague"), TEXT("XP"), TEXT("h de jeu"), TEXT("objets ranges") };
	const TCHAR* NomsRangs[4] = { TEXT("A GAGNER"), TEXT("BRONZE"), TEXT("ARGENT"), TEXT("OR") };

	FLinearColor CouleurRang(int32 R)
	{
		switch (R)
		{
		case 1: return FLinearColor(0.55f, 0.25f, 0.07f);
		case 2: return FLinearColor(0.65f, 0.67f, 0.72f);
		case 3: return FLinearColor(1.00f, 0.62f, 0.05f);
		default: return FLinearColor(0.05f, 0.05f, 0.06f);
		}
	}

	FColor CouleurTexteRang(int32 R)
	{
		switch (R)
		{
		case 1: return FColor(230, 140, 70);
		case 2: return FColor(220, 225, 235);
		case 3: return FColor(255, 200, 40);
		default: return FColor(140, 140, 150);
		}
	}

	/** Meme regle que les autres sauvegardes CHELLES (ChellesPouvoirs SauvegardeJoueur.h) : un fichier par joueur. */
	FString AppEmplacement(const UObject* Contexte, const TCHAR* Base)
	{
		FString Nom = Base;
		FString Joueur;
		if (FParse::Value(FCommandLine::Get(), TEXT("Joueur="), Joueur))
		{
			FString Propre;
			for (const TCHAR C : Joueur)
			{
				if (FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-'))
				{
					Propre.AppendChar(C);
				}
			}
			if (!Propre.IsEmpty())
			{
				return Nom + TEXT("_") + Propre.Left(32);
			}
		}
		const UWorld* Monde = Contexte ? Contexte->GetWorld() : nullptr;
		if (Monde && Monde->WorldType == EWorldType::PIE)
		{
			const int32 Instance = Monde->GetOutermost()->PIEInstanceID;
			if (Instance > 0)
			{
				Nom += FString::Printf(TEXT("_PIE%d"), Instance + 1);
			}
		}
		return Nom;
	}

	/** Lit un nombre (int, float, double, octet...) par son nom dans un objet d'un autre plugin. */
	bool AppLireNombre(const UObject* Objet, const TCHAR* Nom, double& Sortie)
	{
		if (!Objet)
		{
			return false;
		}
		FProperty* P = Objet->GetClass()->FindPropertyByName(FName(Nom));
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (const FNumericProperty* N = CastField<FNumericProperty>(P))
		{
			Sortie = N->IsFloatingPoint() ? N->GetFloatingPointPropertyValue(Adresse) : double(N->GetSignedIntPropertyValue(Adresse));
			return true;
		}
		if (const FEnumProperty* E = CastField<FEnumProperty>(P))
		{
			Sortie = double(E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Adresse));
			return true;
		}
		return false;
	}

	bool EstTransitoire(const UObject* O)
	{
		return !O || O->GetOutermost() == GetTransientPackage() || O->GetPathName().StartsWith(TEXT("/Engine/Transient"));
	}

	/** Nom lisible : SK_Wraith_Skin2 -> WRAITH SKIN2, BP_VoitureSport_C -> VOITURESPORT */
	FString NomLisible(FString N)
	{
		for (const TCHAR* Debut : { TEXT("SK_"), TEXT("SKM_"), TEXT("SM_"), TEXT("BP_"), TEXT("Bp_"), TEXT("B_") })
		{
			N.RemoveFromStart(Debut, ESearchCase::CaseSensitive);
		}
		N.RemoveFromEnd(TEXT("_C"));
		N = N.Replace(TEXT("_"), TEXT(" ")).ToUpper();
		return N.Left(22);
	}

	bool EstVehicule(const APawn* Pion)
	{
		if (!Pion)
		{
			return false;
		}
		if (!Pion->IsA<ACharacter>())
		{
			return true;
		}
		const FString Classe = Pion->GetClass()->GetName();
		return Classe.Contains(TEXT("Horse")) || Classe.Contains(TEXT("Cheval"));
	}
}

FString FPieceAppartement::Cle() const
{
	FString C;
	for (const FMorceauAppartement& M : Morceaux)
	{
		C += M.Maillage;
		if (Morceaux.Num() > 0 && &M == &Morceaux[0])
		{
			for (const FString& Mat : M.Materiaux)
			{
				C += TEXT("|") + Mat;
			}
		}
		C += TEXT(";");
	}
	return C;
}

// ===================================================================================
//  Base
// ===================================================================================

AAppartementJoueur::AAppartementJoueur()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	bReplicates = true;
	bOnlyRelevantToOwner = true; // personne d'autre ne recoit l'appartement
	bAlwaysRelevant = false;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
	NetUpdateFrequency = 5.f;
	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;
}

void AAppartementJoueur::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAppartementJoueur, Numero);
	DOREPLIFETIME(AAppartementJoueur, bDedans);
}

bool AAppartementJoueur::EstLocal() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	return PC && PC->IsLocalController();
}

void AAppartementJoueur::BeginPlay()
{
	Super::BeginPlay();
	Placer();
	Construire();
	if (EstLocal())
	{
		ChargerSauvegarde();
		if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
		{
			AddTickPrerequisiteActor(PC); // la touche F1 est lue apres le clavier
		}
	}
	MettreAJourVisibilite();
}

void AAppartementJoueur::EndPlay(const EEndPlayReason::Type Raison)
{
	if (EstLocal() && Sauvegarde)
	{
		EcrireSauvegarde(); // les derniers zombies tues ne sont pas perdus
	}
	if (Bandeau.IsValid() && VueBandeau.IsValid())
	{
		VueBandeau->RemoveViewportWidgetContent(Bandeau.ToSharedRef());
	}
	Bandeau.Reset();
	if (Chargement.IsValid())
	{
		Chargement->CancelHandle();
		Chargement.Reset();
	}
	Super::EndPlay(Raison);
}

void AAppartementJoueur::Placer()
{
	const FReglagesAppartement& R = ReglagesAppartement();
	SetActorLocationAndRotation(R.Position + FVector(0.f, Numero * R.Ecart, 0.f), FRotator::ZeroRotator);
}

void AAppartementJoueur::QuandNumero()
{
	Placer();
}

void AAppartementJoueur::QuandDedans()
{
	MettreAJourVisibilite();
	if (!EstLocal())
	{
		return;
	}
	if (bDedans)
	{
		RemplirTout();
		Son(ReglagesAppartement().SonPorte);
		const FString Touche = ReglagesAppartement().Touches.Num() > 0 ? ReglagesAppartement().Touches[0].GetDisplayName().ToString() : TEXT("F1");
		Message(FString::Printf(TEXT("Bienvenue chez toi !  %s ou la porte SORTIE pour revenir dans le quartier."), *Touche));
	}
	else
	{
		ViderExpositions(); // on libere la memoire des mannequins et des vehicules
		Son(ReglagesAppartement().SonPorte);
		if (!ModeApresSortie.IsEmpty())
		{
			const FString Mode = ModeApresSortie;
			ModeApresSortie.Reset();
			FTimerHandle Minuteur;
			TWeakObjectPtr<AAppartementJoueur> Moi(this);
			GetWorldTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Moi, Mode]()
			{
				if (Moi.IsValid() && GEngine && Moi->GetWorld())
				{
					GEngine->Exec(Moi->GetWorld(), *FString::Printf(TEXT("Chelles.Raccourcis Mode %s"), *Mode));
				}
			}), 0.4f, false);
		}
	}
}

void AAppartementJoueur::MettreAJourVisibilite()
{
	// Visible seulement chez son proprietaire, et seulement quand il est dedans (les murs gardent leurs collisions)
	SetActorHiddenInGame(!(EstLocal() && bDedans));
}

// ===================================================================================
//  Construction de la piece (formes de base d'Unreal + couleurs)
// ===================================================================================

UMaterialInstanceDynamic* AAppartementJoueur::Peinture(const FLinearColor& Couleur)
{
	const uint32 Cle = Couleur.ToFColor(false).DWColor();
	if (UMaterialInstanceDynamic** Deja = Peintures.Find(Cle))
	{
		return *Deja;
	}
	static UMaterialInterface* Base = nullptr;
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!Base)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this);
	M->SetVectorParameterValue(TEXT("Color"), Couleur);
	Peintures.Add(Cle, M);
	return M;
}

UStaticMeshComponent* AAppartementJoueur::Bloc(UStaticMesh* Forme, const FVector& Centre, const FVector& Taille, const FLinearColor& Couleur,
	bool bCollision, const FRotator& Rotation, USceneComponent* Parent)
{
	if (!Forme)
	{
		return nullptr;
	}
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
	C->SetupAttachment(Parent ? Parent : Racine);
	C->SetStaticMesh(Forme);
	C->SetRelativeLocationAndRotation(Centre, Rotation);
	C->SetRelativeScale3D(Taille / 100.f); // les formes de base font 1 m
	C->SetMaterial(0, Peinture(Couleur));
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bCollision)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}
	C->SetCanEverAffectNavigation(false);
	C->bCastDynamicShadow = bCollision;
	C->RegisterComponent();
	return C;
}

UTextRenderComponent* AAppartementJoueur::Texte(const FString& Contenu, const FVector& Lieu, float Yaw, float Taille, const FColor& Couleur,
	USceneComponent* Parent)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(Parent ? Parent : Racine);
	T->SetRelativeLocationAndRotation(Lieu, FRotator(0.f, Yaw, 0.f));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetWorldSize(Taille);
	T->SetTextRenderColor(Couleur);
	T->SetText(FText::FromString(Contenu));
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->CastShadow = false;
	T->RegisterComponent();
	return T;
}

UPointLightComponent* AAppartementJoueur::Lampe(const FVector& Lieu, float Intensite, const FLinearColor& Couleur, float Rayon)
{
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetupAttachment(Racine);
	L->SetMobility(EComponentMobility::Movable);
	L->SetRelativeLocation(Lieu);
	L->SetIntensity(Intensite);
	L->SetLightColor(Couleur);
	L->SetAttenuationRadius(Rayon);
	L->SetCastShadows(false);
	L->RegisterComponent();
	return L;
}

void AAppartementJoueur::AjouterDalle(uint8 Type, const FVector2D& Centre, const FVector2D& Taille, const FString& Mode, const FString& Titre,
	const FLinearColor& Couleur)
{
	FDalle D;
	D.Type = Type;
	D.Centre = Centre;
	D.Taille = Taille;
	D.Mode = Mode;
	Dalles.Add(D);

	// Dalle lumineuse + bord blanc + panneau qui flotte au-dessus (lisible depuis l'entree)
	Bloc(Cube, FVector(Centre, 1.f), FVector(Taille.X + 16.f, Taille.Y + 16.f, 1.f), FLinearColor::White, false);
	Bloc(Cube, FVector(Centre, 2.f), FVector(Taille, 2.f), Couleur, false);
	Lampe(FVector(Centre, 60.f), 1200.f, Couleur, 280.f);
	Texte(Titre, FVector(Centre.X, Centre.Y, 175.f), 180.f, 26.f, Couleur.ToFColor(true));
}

void AAppartementJoueur::Construire()
{
	if (bConstruit)
	{
		return;
	}
	bConstruit = true;
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cylindre = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!Cube)
	{
		UE_LOG(LogTemp, Error, TEXT("[Appartement] /Engine/BasicShapes/Cube introuvable : appartement non construit."));
		return;
	}
	const FReglagesAppartement& R = ReglagesAppartement();
	const float L2 = Longueur * 0.5f;
	const float W2 = Largeur * 0.5f;
	const float E = Epaisseur;
	const FLinearColor Blanc(0.9f, 0.9f, 0.88f);
	const FLinearColor Bois(0.20f, 0.10f, 0.04f);
	const FLinearColor Plafond(0.95f, 0.95f, 0.93f);

	// ---- Sol, plafond, murs ----
	Bloc(Cube, FVector(0.f, 0.f, -E * 0.5f), FVector(Longueur + 2 * E, Largeur + 2 * E, E), R.CouleurSol);
	Bloc(Cube, FVector(0.f, 0.f, HauteurPiece + E * 0.5f), FVector(Longueur + 2 * E, Largeur + 2 * E, E), Plafond);
	Bloc(Cube, FVector(-L2 - E * 0.5f, 0.f, HauteurPiece * 0.5f), FVector(E, Largeur, HauteurPiece), R.CouleurMurs);
	Bloc(Cube, FVector(L2 + E * 0.5f, 0.f, HauteurPiece * 0.5f), FVector(E, Largeur, HauteurPiece), R.CouleurMurs);
	Bloc(Cube, FVector(0.f, -W2 - E * 0.5f, HauteurPiece * 0.5f), FVector(Longueur, E, HauteurPiece), R.CouleurMurs);
	Bloc(Cube, FVector(0.f, W2 + E * 0.5f, HauteurPiece * 0.5f), FVector(Longueur, E, HauteurPiece), R.CouleurMurs);

	// Plinthes sombres et bande neon en haut des murs
	Bloc(Cube, FVector(0.f, -W2 + 2.f, 10.f), FVector(Longueur, 4.f, 20.f), Bois, false);
	Bloc(Cube, FVector(0.f, W2 - 2.f, 10.f), FVector(Longueur, 4.f, 20.f), Bois, false);
	Bloc(Cube, FVector(0.f, -W2 + 2.f, HauteurPiece - 25.f), FVector(Longueur, 4.f, 8.f), R.CouleurNeon, false);
	Bloc(Cube, FVector(0.f, W2 - 2.f, HauteurPiece - 25.f), FVector(Longueur, 4.f, 8.f), R.CouleurNeon, false);
	Bloc(Cube, FVector(-L2 + 2.f, 0.f, HauteurPiece - 25.f), FVector(4.f, Largeur, 8.f), R.CouleurNeon, false);
	Bloc(Cube, FVector(L2 - 2.f, 0.f, HauteurPiece - 25.f), FVector(4.f, Largeur, 8.f), R.CouleurNeon, false);

	// ---- Entree : porte + titre ----
	Bloc(Cube, FVector(-L2 + 3.f, 0.f, 130.f), FVector(6.f, 170.f, 260.f), Bois, false);
	Bloc(Cube, FVector(-L2 + 7.f, 60.f, 125.f), FVector(4.f, 12.f, 12.f), FLinearColor(0.9f, 0.7f, 0.2f), false);
	Texte(R.Titre, FVector(-L2 + 10.f, 0.f, 360.f), 0.f, 60.f, FColor(255, 255, 255));
	Texte(TEXT("SORTIE"), FVector(-L2 + 10.f, 0.f, 290.f), 0.f, 34.f, FColor(255, 90, 90));
	AjouterDalle(DalleSortie, FVector2D(-L2 + 110.f, 0.f), FVector2D(150.f, 200.f), FString(), TEXT("SORTIE"), FLinearColor(1.f, 0.15f, 0.15f));

	// ---- Salon : tapis, table basse, plantes ----
	Bloc(Cube, FVector(-375.f, 0.f, 1.f), FVector(520.f, 360.f, 2.f), FLinearColor(0.45f, 0.06f, 0.10f), false);
	Bloc(Cylindre, FVector(-375.f, 0.f, 20.f), FVector(20.f, 20.f, 40.f), Bois);
	Bloc(Cylindre, FVector(-375.f, 0.f, 43.f), FVector(130.f, 130.f, 6.f), Bois);
	const FVector2D Plantes[] = { FVector2D(-L2 + 70.f, -W2 + 70.f), FVector2D(-L2 + 70.f, W2 - 70.f), FVector2D(DebutGarage - 120.f, -W2 + 70.f), FVector2D(DebutGarage - 120.f, W2 - 70.f) };
	for (const FVector2D& P : Plantes)
	{
		Bloc(Cylindre, FVector(P, 30.f), FVector(60.f, 60.f, 60.f), FLinearColor(0.75f, 0.35f, 0.15f));
		Bloc(Sphere, FVector(P, 115.f), FVector(110.f, 110.f, 120.f), FLinearColor(0.05f, 0.35f, 0.08f), false);
	}

	// ---- Garde-robe (mur de gauche) ----
	Texte(TEXT("GARDE-ROBE"), FVector(-375.f, -W2 + 8.f, 365.f), 90.f, 60.f, FColor(255, 255, 255));
	Texte(TEXT("tes skins, ranges a chaque visite"), FVector(-375.f, -W2 + 8.f, 320.f), 90.f, 22.f, FColor(220, 220, 220));
	const int32 NbHabits = R.PlacesGardeRobe;
	const float PasHabits = 1300.f / FMath::Max(1, NbHabits);
	for (int32 i = 0; i < NbHabits; ++i)
	{
		const float X = -1050.f + PasHabits * (i + 0.5f);
		Bloc(Cylindre, FVector(X, -W2 + 190.f, 15.f), FVector(160.f, 160.f, 30.f), Blanc);
		Bloc(Cylindre, FVector(X, -W2 + 190.f, 31.f), FVector(150.f, 150.f, 2.f), R.CouleurNeon, false);
		Lampe(FVector(X, -W2 + 300.f, 330.f), 1500.f, FLinearColor(1.f, 0.95f, 0.85f), 450.f);
	}
	AjouterDalle(DalleHabillage, FVector2D(-375.f, -W2 + 430.f), FVector2D(220.f, 150.f), FString(), TEXT("HABILLAGE"), FLinearColor(0.85f, 0.25f, 1.f));

	// ---- Vitrine des trophees (mur de droite) ----
	Texte(TEXT("TROPHEES"), FVector(-375.f, W2 - 8.f, 365.f), -90.f, 60.f, FColor(255, 210, 60));
	Bloc(Cube, FVector(-375.f, W2 - 15.f, 250.f), FVector(1350.f, 10.f, 8.f), FLinearColor(1.f, 0.62f, 0.05f), false);
	for (int32 t = 0; t < NombreTrophees; ++t)
	{
		// Les socles sont construits partout (le serveur connait leurs collisions), les coupes seulement chez le joueur
		const float X = -1050.f + (1300.f / NombreTrophees) * (t + 0.5f);
		Bloc(Cube, FVector(X, W2 - 170.f, 50.f), FVector(130.f, 130.f, 100.f), FLinearColor(0.08f, 0.05f, 0.03f));
		Bloc(Cube, FVector(X, W2 - 170.f, 101.f), FVector(134.f, 134.f, 3.f), FLinearColor(1.f, 0.62f, 0.05f), false);
		Lampe(FVector(X, W2 - 330.f, 330.f), 1500.f, FLinearColor(1.f, 0.9f, 0.7f), 450.f);
	}

	// ---- Garage (au fond) ----
	Bloc(Cube, FVector((DebutGarage + L2) * 0.5f, 0.f, 1.f), FVector(L2 - DebutGarage, Largeur, 2.f), R.CouleurGarage, false);
	Bloc(Cube, FVector(DebutGarage, 0.f, 2.5f), FVector(14.f, Largeur, 1.f), FLinearColor(1.f, 0.8f, 0.f), false);
	Texte(TEXT("GARAGE"), FVector(L2 - 8.f, 0.f, 370.f), 180.f, 70.f, FColor(255, 255, 255));
	Texte(TEXT("tes vehicules, ranges quand tu les conduis"), FVector(L2 - 8.f, 0.f, 320.f), 180.f, 22.f, FColor(220, 220, 220));
	const int32 NbPlaces = R.PlacesGarage;
	const float LargePlace = Largeur / NbPlaces;
	for (int32 i = 1; i < NbPlaces; ++i)
	{
		Bloc(Cube, FVector((DebutGarage + L2) * 0.5f + 60.f, -W2 + LargePlace * i, 2.5f), FVector(L2 - DebutGarage - 120.f, 10.f, 1.f), FLinearColor(1.f, 0.8f, 0.f), false);
	}
	Lampe(FVector(1050.f, 0.f, HauteurPiece - 40.f), R.Lumiere, FLinearColor(0.85f, 0.92f, 1.f), 1300.f);
	const int32 NbDeparts = R.Departs.Num();
	for (int32 i = 0; i < NbDeparts; ++i)
	{
		const float Y = NbDeparts == 1 ? 0.f : FMath::Lerp(-450.f, 450.f, float(i) / float(NbDeparts - 1));
		AjouterDalle(DalleDepart, FVector2D(DebutGarage - 140.f, Y), FVector2D(180.f, 150.f), R.Departs[i].Mode,
			FString::Printf(TEXT("PARTIR %s"), *R.Departs[i].Nom), FLinearColor(0.1f, 1.f, 0.35f));
	}

	// ---- Lumieres de la piece ----
	Lampe(FVector(-1100.f, 0.f, HauteurPiece - 40.f), R.Lumiere, FLinearColor(1.f, 0.9f, 0.75f), 1300.f);
	Lampe(FVector(-375.f, 0.f, HauteurPiece - 40.f), R.Lumiere, FLinearColor(1.f, 0.9f, 0.75f), 1300.f);
	Lampe(FVector(300.f, 0.f, HauteurPiece - 40.f), R.Lumiere, FLinearColor(1.f, 0.9f, 0.75f), 1300.f);

	// Ce qui change a chaque visite (mannequins, vehicules, coupes) est pose sous ce composant
	Expositions = NewObject<USceneComponent>(this, TEXT("Expositions"));
	Expositions->SetupAttachment(Racine);
	Expositions->RegisterComponent();
}

// ===================================================================================
//  Entrer / sortir
// ===================================================================================

void AAppartementJoueur::EntrerOuSortir()
{
	UWorld* Monde = GetWorld();
	if (!Monde || !EstLocal() || Monde->GetRealTimeSeconds() < ProchainEssai)
	{
		return;
	}
	ProchainEssai = Monde->GetRealTimeSeconds() + 1.f;
	if (bDedans)
	{
		Sortir(FString());
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	if (EstVehicule(Pion))
	{
		Message(TEXT("Descends du vehicule pour rentrer chez toi (il sera range dans ton garage)."));
		return;
	}
	// Le skin porte en ce moment est range dans la garde-robe
	FPieceAppartement Habit;
	if (Sauvegarde && Capturer(Pion, false, Habit))
	{
		if (Ranger(Sauvegarde->GardeRobe, Habit, ReglagesAppartement().PlacesGardeRobe))
		{
			++Sauvegarde->Collection;
		}
	}
	if (Sauvegarde)
	{
		++Sauvegarde->Visites;
		EcrireSauvegarde();
	}
	ServeurEntrer();
}

void AAppartementJoueur::Sortir(const FString& ModeApres)
{
	if (!bDedans)
	{
		return;
	}
	ModeApresSortie = ModeApres;
	ServeurSortir();
}

void AAppartementJoueur::ServeurEntrer_Implementation()
{
	if (bDedans || !bConstruit)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ACharacter* Perso = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
	if (!Perso || EstVehicule(Perso))
	{
		return;
	}
	RetourLieu = Perso->GetActorLocation();
	RetourRotation = PC->GetControlRotation();
	RetourRotation.Pitch = 0.f;
	RetourRotation.Roll = 0.f;
	UCharacterMovementComponent* Marche = Perso->GetCharacterMovement();
	RetourMarche = Marche ? uint8(Marche->MovementMode) : uint8(MOVE_Walking);

	const float DemiHauteur = Perso->GetCapsuleComponent() ? Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 100.f;
	const FVector Lieu = GetActorTransform().TransformPosition(Arrivee + FVector(0.f, 0.f, DemiHauteur + 10.f));
	const FRotator Regard(0.f, GetActorRotation().Yaw, 0.f);
	if (!Perso->TeleportTo(Lieu, Regard, false, false) && !Perso->TeleportTo(Lieu, Regard, false, true))
	{
		ClientMessage(TEXT("Impossible d'entrer dans l'appartement pour le moment."));
		return;
	}
	if (Marche)
	{
		Marche->StopMovementImmediately();
		Marche->SetMovementMode(MOVE_Falling); // il se pose sur le parquet
	}
	PC->ClientSetRotation(Regard, true);
	PionDedans = Perso;
	bDedans = true;
	UE_LOG(LogTemp, Log, TEXT("[Appartement] %s rentre chez lui (appartement %d)."), *PC->GetName(), Numero);
	if (GetNetMode() != NM_Client)
	{
		QuandDedans(); // pas de replication vers soi-meme
	}
}

void AAppartementJoueur::ServeurSortir_Implementation()
{
	if (!bDedans)
	{
		return;
	}
	APawn* Pion = PionDedans.Get();
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (Pion && PC && Pion == PC->GetPawn())
	{
		if (!Pion->TeleportTo(RetourLieu, RetourRotation, false, false))
		{
			Pion->TeleportTo(RetourLieu, RetourRotation, false, true);
		}
		if (ACharacter* Perso = Cast<ACharacter>(Pion))
		{
			if (UCharacterMovementComponent* Marche = Perso->GetCharacterMovement())
			{
				Marche->StopMovementImmediately();
				// En vol (JetPack, ENVOL) on repart en vol, sinon on retombe sur le trottoir
				Marche->SetMovementMode(RetourMarche == uint8(MOVE_Flying) ? MOVE_Flying : MOVE_Falling);
			}
		}
		PC->ClientSetRotation(RetourRotation, true);
	}
	PionDedans.Reset();
	bDedans = false;
	if (GetNetMode() != NM_Client)
	{
		QuandDedans();
	}
}

void AAppartementJoueur::ClientMessage_Implementation(const FString& Texte)
{
	Message(Texte);
}

// ===================================================================================
//  Chaque image
// ===================================================================================

void AAppartementJoueur::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* Monde = GetWorld();
	if (!Monde)
	{
		return;
	}
	if (HasAuthority())
	{
		TickServeur();
	}
	if (Bandeau.IsValid() && Monde->GetRealTimeSeconds() > FinBandeau)
	{
		if (VueBandeau.IsValid())
		{
			VueBandeau->RemoveViewportWidgetContent(Bandeau.ToSharedRef());
		}
		Bandeau.Reset();
	}
	if (!EstLocal())
	{
		return;
	}
	if (!Sauvegarde)
	{
		ChargerSauvegarde(); // en multijoueur, le proprietaire peut arriver apres BeginPlay
		MettreAJourVisibilite();
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->PlayerInput && ReglagesAppartement().bActif)
	{
		const bool bCtrl = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
		for (const FKey& K : ReglagesAppartement().Touches)
		{
			if (!bCtrl && PC->WasInputKeyJustPressed(K))
			{
				EntrerOuSortir();
				break;
			}
		}
	}
	if (bDedans)
	{
		VerifierDalles();
	}
	else
	{
		SurveillerVehicule();
	}
}

void AAppartementJoueur::TickServeur()
{
	if (!bDedans)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pion = PionDedans.Get();
	APawn* Actuel = PC ? PC->GetPawn() : nullptr;
	if (Actuel && Actuel != Pion)
	{
		// Nouveau personnage (habillage, roue Tab) apparu dans l'appartement : c'est toujours lui qui est chez lui
		const FVector L = GetActorTransform().InverseTransformPosition(Actuel->GetActorLocation());
		if (FMath::Abs(L.X) < Longueur && FMath::Abs(L.Y) < Largeur && L.Z > -800.f && L.Z < HauteurPiece + 800.f)
		{
			PionDedans = Actuel;
			Pion = Actuel;
		}
	}
	if (!Pion || !PC || Actuel != Pion)
	{
		// Mort, changement de personnage ou de vehicule (roue Tab) : il n'est plus chez lui
		PionDedans.Reset();
		bDedans = false;
		if (GetNetMode() != NM_Client)
		{
			QuandDedans();
		}
		return;
	}
	// Tombe sous le parquet (ne devrait pas arriver) : retour a l'entree
	const FVector Local = GetActorTransform().InverseTransformPosition(Pion->GetActorLocation());
	if (Local.Z < -800.f || Local.Z > HauteurPiece + 800.f || FMath::Abs(Local.X) > Longueur || FMath::Abs(Local.Y) > Largeur)
	{
		const float DemiHauteur = Cast<ACharacter>(Pion) && Cast<ACharacter>(Pion)->GetCapsuleComponent()
			? Cast<ACharacter>(Pion)->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 100.f;
		Pion->TeleportTo(GetActorTransform().TransformPosition(Arrivee + FVector(0.f, 0.f, DemiHauteur + 10.f)), FRotator::ZeroRotator, false, true);
	}
}

void AAppartementJoueur::VerifierDalles()
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (!Pion)
	{
		return;
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(Pion->GetActorLocation());
	for (FDalle& D : Dalles)
	{
		const bool bSur = FMath::Abs(Local.X - D.Centre.X) < D.Taille.X * 0.5f && FMath::Abs(Local.Y - D.Centre.Y) < D.Taille.Y * 0.5f
			&& Local.Z < 300.f;
		if (!bSur)
		{
			D.bArmee = true; // on peut la reutiliser quand on en est sorti
			continue;
		}
		if (!D.bArmee)
		{
			continue;
		}
		D.bArmee = false;
		switch (D.Type)
		{
		case DalleSortie:
			Sortir(FString());
			break;
		case DalleHabillage:
			if (GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("Chelles.Habillage"));
			}
			break;
		case DalleDepart:
			Sortir(D.Mode);
			break;
		default:
			break;
		}
		return;
	}
}

void AAppartementJoueur::SurveillerVehicule()
{
	UWorld* Monde = GetWorld();
	if (Monde->GetTimeSeconds() < ProchaineVerif || !Sauvegarde)
	{
		return;
	}
	ProchaineVerif = Monde->GetTimeSeconds() + 1.f;
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pion = PC ? PC->GetPawn() : nullptr;
	if (Pion != DernierPion.Get())
	{
		DernierPion = Pion;
		DepuisNouveauPion = 0.f;
		bPionNote = false;
		return;
	}
	DepuisNouveauPion += 1.f;
	// 3 secondes dans le meme vehicule : on le range dans le garage (ses pieces sont en place)
	if (!Pion || bPionNote || DepuisNouveauPion < 3.f || !EstVehicule(Pion))
	{
		return;
	}
	bPionNote = true;
	FPieceAppartement Vehicule;
	if (Capturer(Pion, true, Vehicule))
	{
		if (Ranger(Sauvegarde->Garage, Vehicule, ReglagesAppartement().PlacesGarage))
		{
			++Sauvegarde->Collection;
			Message(FString::Printf(TEXT("%s range dans ton garage (touche %s pour aller le voir)."), *Vehicule.Nom,
				ReglagesAppartement().Touches.Num() > 0 ? *ReglagesAppartement().Touches[0].GetDisplayName().ToString() : TEXT("F1")));
		}
		EcrireSauvegarde();
		AnnoncerTrophees(true);
	}
}

// ===================================================================================
//  Garde-robe et garage
// ===================================================================================

bool AAppartementJoueur::Capturer(APawn* Pion, bool bVehicule, FPieceAppartement& Sortie) const
{
	if (!Pion)
	{
		return false;
	}
	// Reference : les pieds du personnage, le centre du vehicule
	FTransform Reference = Pion->GetActorTransform();
	Reference.SetScale3D(FVector::OneVector);
	ACharacter* Perso = Cast<ACharacter>(Pion);
	if (Perso && Perso->GetCapsuleComponent())
	{
		Reference.AddToTranslation(FVector(0.f, 0.f, -Perso->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	}
	USkeletalMeshComponent* Corps = Perso ? Perso->GetMesh() : nullptr;

	TInlineComponentArray<UMeshComponent*> Maillages(Pion);
	TArray<UMeshComponent*> Gardes;
	for (UMeshComponent* M : Maillages)
	{
		if (!M || !M->IsRegistered() || !M->IsVisible() || M->bHiddenInGame || M->bOnlyOwnerSee)
		{
			continue; // cache, ou bras de la vue a la 1re personne
		}
		if (USkeletalMeshComponent* S = Cast<USkeletalMeshComponent>(M))
		{
			if (S->SkeletalMesh && !EstTransitoire(S->SkeletalMesh))
			{
				Gardes.Add(M);
			}
		}
		else if (UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(M))
		{
			// Pour un personnage, les objets statiques sont souvent des armes : seulement pour les vehicules
			if (bVehicule && SM->GetStaticMesh() && !EstTransitoire(SM->GetStaticMesh()) && !SM->IsA<UTextRenderComponent>())
			{
				Gardes.Add(M);
			}
		}
	}
	if (Gardes.Num() == 0)
	{
		return false;
	}
	// Le corps en premier (il porte l'animation), puis les plus gros morceaux
	Gardes.Sort([Corps](const UMeshComponent& A, const UMeshComponent& B)
	{
		if (&A == Corps) return true;
		if (&B == Corps) return false;
		return A.Bounds.SphereRadius > B.Bounds.SphereRadius;
	});
	if (!Gardes[0]->IsA<USkeletalMeshComponent>() && !bVehicule)
	{
		return false;
	}
	Gardes.SetNum(FMath::Min(Gardes.Num(), 12));

	FPieceAppartement P;
	for (UMeshComponent* M : Gardes)
	{
		FMorceauAppartement Mo;
		if (USkeletalMeshComponent* S = Cast<USkeletalMeshComponent>(M))
		{
			Mo.Maillage = S->SkeletalMesh->GetPathName();
			Mo.bSquelette = true;
			if (P.Morceaux.Num() == 0 && !bVehicule && S->GetAnimationMode() == EAnimationMode::AnimationBlueprint && S->GetAnimClass())
			{
				Mo.Anim = S->GetAnimClass()->GetPathName();
			}
		}
		else if (UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(M))
		{
			Mo.Maillage = SM->GetStaticMesh()->GetPathName();
			Mo.bSquelette = false;
		}
		for (int32 i = 0; i < M->GetNumMaterials(); ++i)
		{
			UMaterialInterface* Mat = M->GetMaterial(i);
			if (UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(Mat))
			{
				Mat = Dyn->Parent; // les materiaux "dynamiques" n'existent qu'en jeu : on garde leur parent
			}
			Mo.Materiaux.Add(Mat && !EstTransitoire(Mat) ? Mat->GetPathName() : FString());
		}
		Mo.Place = M->GetComponentTransform().GetRelativeTransform(Reference);
		P.Morceaux.Add(Mo);
	}
	const FString Base = bVehicule ? Pion->GetClass()->GetName() : FPaths::GetBaseFilename(P.Morceaux[0].Maillage);
	P.Nom = NomLisible(Base);
	Sortie = P;
	return true;
}

bool AAppartementJoueur::Ranger(TArray<FPieceAppartement>& Liste, const FPieceAppartement& Piece, int32 Max)
{
	const FString Cle = Piece.Cle();
	const int32 Deja = Liste.IndexOfByPredicate([&Cle](const FPieceAppartement& P) { return P.Cle() == Cle; });
	if (Deja != INDEX_NONE)
	{
		// Deja range : il remonte a la premiere place
		const FPieceAppartement Copie = Liste[Deja];
		Liste.RemoveAt(Deja);
		Liste.Insert(Copie, 0);
		return false;
	}
	Liste.Insert(Piece, 0);
	if (Liste.Num() > Max)
	{
		Liste.SetNum(Max); // le plus ancien laisse sa place
	}
	return true;
}

void AAppartementJoueur::ViderExpositions()
{
	if (Chargement.IsValid())
	{
		Chargement->CancelHandle();
		Chargement.Reset();
	}
	if (Expositions)
	{
		TArray<USceneComponent*> Enfants;
		Expositions->GetChildrenComponents(true, Enfants);
		for (USceneComponent* C : Enfants)
		{
			if (C)
			{
				C->DestroyComponent();
			}
		}
	}
	SoclesGardeRobe.Reset();
	SoclesGarage.Reset();
}

void AAppartementJoueur::RemplirTout()
{
	if (!Sauvegarde || !Expositions || !bConstruit)
	{
		return;
	}
	ViderExpositions();
	const FReglagesAppartement& R = ReglagesAppartement();
	const float W2 = Largeur * 0.5f;

	// Une place par skin (meme ordre que les socles de Construire)
	const float PasHabits = 1300.f / FMath::Max(1, R.PlacesGardeRobe);
	for (int32 i = 0; i < R.PlacesGardeRobe; ++i)
	{
		const float X = -1050.f + PasHabits * (i + 0.5f);
		USceneComponent* S = NewObject<USceneComponent>(this);
		S->SetupAttachment(Expositions);
		S->SetRelativeLocationAndRotation(FVector(X, -W2 + 190.f, 32.f), FRotator(0.f, 90.f, 0.f)); // regarde la piece
		S->RegisterComponent();
		SoclesGardeRobe.Add(S);
		const FString Nom = Sauvegarde->GardeRobe.IsValidIndex(i) ? Sauvegarde->GardeRobe[i].Nom : FString(TEXT("PLACE LIBRE"));
		Texte(Nom, FVector(X, -W2 + 285.f, 12.f), 90.f, 16.f, FColor(255, 255, 255), Expositions);
	}
	// Une place par vehicule
	const float LargePlace = Largeur / R.PlacesGarage;
	for (int32 i = 0; i < R.PlacesGarage; ++i)
	{
		const float Y = -W2 + LargePlace * (i + 0.5f);
		USceneComponent* S = NewObject<USceneComponent>(this);
		S->SetupAttachment(Expositions);
		S->SetRelativeLocationAndRotation(FVector(1100.f, Y, 3.f), FRotator(0.f, 180.f, 0.f)); // le capot vers la piece
		S->RegisterComponent();
		SoclesGarage.Add(S);
		const FString Nom = Sauvegarde->Garage.IsValidIndex(i) ? Sauvegarde->Garage[i].Nom : FString(TEXT("PLACE LIBRE"));
		Texte(Nom, FVector(Longueur * 0.5f - 8.f, Y, 230.f), 180.f, 24.f, FColor(255, 220, 120), Expositions);
	}

	// Chargement en arriere-plan de tous les maillages (pas de gel de l'image)
	TArray<FSoftObjectPath> ACharger;
	auto Ajouter = [&ACharger](const TArray<FPieceAppartement>& Liste, int32 Max)
	{
		for (int32 i = 0; i < Liste.Num() && i < Max; ++i)
		{
			for (const FMorceauAppartement& M : Liste[i].Morceaux)
			{
				ACharger.AddUnique(FSoftObjectPath(M.Maillage));
				for (const FString& Mat : M.Materiaux)
				{
					if (!Mat.IsEmpty())
					{
						ACharger.AddUnique(FSoftObjectPath(Mat));
					}
				}
				if (!M.Anim.IsEmpty())
				{
					ACharger.AddUnique(FSoftObjectPath(M.Anim));
				}
			}
		}
	};
	Ajouter(Sauvegarde->GardeRobe, R.PlacesGardeRobe);
	Ajouter(Sauvegarde->Garage, R.PlacesGarage);

	TWeakObjectPtr<AAppartementJoueur> Moi(this);
	auto Exposer_Tout = [Moi]()
	{
		AAppartementJoueur* A = Moi.Get();
		if (!A || !A->bDedans || !A->Sauvegarde)
		{
			return;
		}
		const FReglagesAppartement& Re = ReglagesAppartement();
		for (int32 i = 0; i < A->SoclesGardeRobe.Num() && i < A->Sauvegarde->GardeRobe.Num(); ++i)
		{
			A->Exposer(A->Sauvegarde->GardeRobe[i], A->SoclesGardeRobe[i], FVector(150.f, 150.f, 300.f), Re.bAnimerMannequins);
		}
		const float Place = Largeur / FMath::Max(1, Re.PlacesGarage);
		for (int32 i = 0; i < A->SoclesGarage.Num() && i < A->Sauvegarde->Garage.Num(); ++i)
		{
			A->Exposer(A->Sauvegarde->Garage[i], A->SoclesGarage[i], FVector(Re.TailleMaxVehicule, Place - 40.f, 380.f), false);
		}
	};
	if (ACharger.Num() > 0)
	{
		Chargement = Chargeur.RequestAsyncLoad(ACharger, FStreamableDelegate::CreateLambda(Exposer_Tout));
	}
	LireXP();
	RemplirVitrine();
	AnnoncerTrophees(false);
}

void AAppartementJoueur::Exposer(const FPieceAppartement& Piece, USceneComponent* Socle, const FVector& TailleMax, bool bAnimer)
{
	if (!Socle || Piece.Morceaux.Num() == 0)
	{
		return;
	}
	USceneComponent* Pivot = NewObject<USceneComponent>(this);
	Pivot->SetupAttachment(Socle);
	Pivot->RegisterComponent();

	FBox Boite(ForceInit);
	USkeletalMeshComponent* Principal = nullptr;
	for (const FMorceauAppartement& M : Piece.Morceaux)
	{
		UMeshComponent* C = nullptr;
		if (M.bSquelette)
		{
			USkeletalMesh* Maillage = Cast<USkeletalMesh>(FSoftObjectPath(M.Maillage).ResolveObject());
			if (!Maillage)
			{
				continue;
			}
			USkeletalMeshComponent* S = NewObject<USkeletalMeshComponent>(this);
			S->SetupAttachment(Pivot);
			S->SetSkeletalMesh(Maillage);
			S->SetRelativeTransform(M.Place);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			S->SetSimulatePhysics(false);
			S->bEnablePhysicsOnDedicatedServer = false;
			UClass* ClasseAnim = M.Anim.IsEmpty() ? nullptr : Cast<UClass>(FSoftObjectPath(M.Anim).ResolveObject());
			if (!Principal)
			{
				Principal = S;
				if (bAnimer && ClasseAnim)
				{
					S->SetAnimationMode(EAnimationMode::AnimationBlueprint);
					S->SetAnimInstanceClass(ClasseAnim);
				}
				else
				{
					S->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				}
			}
			S->RegisterComponent();
			if (Principal != S && Principal && S->SkeletalMesh->GetSkeleton() == Principal->SkeletalMesh->GetSkeleton())
			{
				S->SetMasterPoseComponent(Principal); // tete, cheveux, habits : suivent le corps
			}
			Boite += Maillage->GetBounds().GetBox().TransformBy(M.Place);
			C = S;
		}
		else
		{
			UStaticMesh* Maillage = Cast<UStaticMesh>(FSoftObjectPath(M.Maillage).ResolveObject());
			if (!Maillage)
			{
				continue;
			}
			UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(this);
			S->SetupAttachment(Pivot);
			S->SetStaticMesh(Maillage);
			S->SetMobility(EComponentMobility::Movable);
			S->SetRelativeTransform(M.Place);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			S->RegisterComponent();
			Boite += Maillage->GetBoundingBox().TransformBy(M.Place);
			C = S;
		}
		for (int32 i = 0; i < M.Materiaux.Num(); ++i)
		{
			if (UMaterialInterface* Mat = M.Materiaux[i].IsEmpty() ? nullptr : Cast<UMaterialInterface>(FSoftObjectPath(M.Materiaux[i]).ResolveObject()))
			{
				C->SetMaterial(i, Mat);
			}
		}
		C->SetCanEverAffectNavigation(false);
	}
	if (!Boite.IsValid)
	{
		Pivot->DestroyComponent();
		return;
	}
	// Mis a l'echelle pour tenir sur sa place, pose sur le socle, centre
	const FVector Taille = Boite.GetSize();
	float Echelle = 1.f;
	for (int32 a = 0; a < 3; ++a)
	{
		if (Taille[a] > 1.f)
		{
			Echelle = FMath::Min(Echelle, TailleMax[a] / Taille[a]);
		}
	}
	Echelle = FMath::Max(Echelle, 0.05f);
	const FVector Centre = Boite.GetCenter();
	Pivot->SetRelativeScale3D(FVector(Echelle));
	Pivot->SetRelativeLocation(FVector(-Centre.X * Echelle, -Centre.Y * Echelle, -Boite.Min.Z * Echelle));
}

// ===================================================================================
//  Trophees
// ===================================================================================

void AAppartementJoueur::LireXP()
{
	// XP et temps de jeu : sauvegarde de ChellesPouvoirs (ChellesXP.sav), lue par le nom de ses champs
	const FString Nom = AppEmplacement(this, TEXT("ChellesXP"));
	USaveGame* Fichier = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(Nom, 0))
	{
		Fichier = UGameplayStatics::LoadGameFromSlot(Nom, 0);
	}
	if (!Fichier && UGameplayStatics::DoesSaveGameExist(Nom + TEXT("_Secours"), 0))
	{
		Fichier = UGameplayStatics::LoadGameFromSlot(Nom + TEXT("_Secours"), 0);
	}
	double V = 0.0;
	if (AppLireNombre(Fichier, TEXT("XP"), V))
	{
		XPTotale = int32(V);
	}
	if (AppLireNombre(Fichier, TEXT("SecondesJouees"), V))
	{
		SecondesJouees = V;
	}
}

int32 AAppartementJoueur::Valeur(int32 Trophee) const
{
	if (!Sauvegarde)
	{
		return 0;
	}
	switch (Trophee)
	{
	case Zombies: return Sauvegarde->ZombiesTues;
	case Missions: return Sauvegarde->MissionsReussies;
	case Vague: return Sauvegarde->MeilleureVague;
	case XP: return XPTotale;
	case Heures: return int32(SecondesJouees / 3600.0);
	case Collection: return Sauvegarde->Collection;
	default: return 0;
	}
}

int32 AAppartementJoueur::Rang(int32 Trophee) const
{
	const int32 V = Valeur(Trophee);
	int32 R = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		if (V >= ReglagesAppartement().Seuils[Trophee][i])
		{
			R = i + 1;
		}
	}
	return R;
}

void AAppartementJoueur::RemplirVitrine()
{
	if (!Expositions)
	{
		return;
	}
	const FReglagesAppartement& Re = ReglagesAppartement();
	const float W2 = Largeur * 0.5f;
	const float Pas = 1300.f / NombreTrophees;
	for (int32 t = 0; t < NombreTrophees; ++t)
	{
		const float X = -1050.f + Pas * (t + 0.5f);
		const float Y = W2 - 170.f;
		const int32 R = Rang(t);
		const FLinearColor Couleur = CouleurRang(R);

		if (R > 0)
		{
			// Coupe : pied, tige, coupe (cone a l'envers), anses ; une etoile pour l'or
			const float H = 70.f + 20.f * R;
			Bloc(Cube, FVector(X, Y, 110.f), FVector(50.f, 50.f, 14.f), Couleur, false, FRotator::ZeroRotator, Expositions);
			Bloc(Cylindre, FVector(X, Y, 117.f + H * 0.2f), FVector(12.f, 12.f, H * 0.4f), Couleur, false, FRotator::ZeroRotator, Expositions);
			Bloc(Cone, FVector(X, Y, 117.f + H * 0.4f + H * 0.3f), FVector(55.f + 8.f * R, 55.f + 8.f * R, H * 0.6f), Couleur, false, FRotator(180.f, 0.f, 0.f), Expositions);
			Bloc(Sphere, FVector(X, Y - 36.f - 4.f * R, 117.f + H * 0.75f), FVector(16.f, 16.f, 26.f), Couleur, false, FRotator::ZeroRotator, Expositions);
			Bloc(Sphere, FVector(X, Y + 36.f + 4.f * R, 117.f + H * 0.75f), FVector(16.f, 16.f, 26.f), Couleur, false, FRotator::ZeroRotator, Expositions);
			if (R == 3)
			{
				Bloc(Sphere, FVector(X, Y, 117.f + H + 18.f), FVector(22.f, 22.f, 22.f), FLinearColor(1.f, 1.f, 0.6f), false, FRotator::ZeroRotator, Expositions);
			}
		}
		else
		{
			Texte(TEXT("?"), FVector(X, Y - 66.f, 170.f), -90.f, 90.f, FColor(90, 90, 100), Expositions);
		}
		// Etiquettes sur le devant du socle
		const FString Ligne1 = NomsTrophees[t];
		FString Ligne2;
		const int32 V = Valeur(t);
		if (R < 3)
		{
			Ligne2 = FString::Printf(TEXT("%s : %d / %d %s"), NomsRangs[R], V, Re.Seuils[t][R], UnitesTrophees[t]);
		}
		else
		{
			Ligne2 = FString::Printf(TEXT("OR : %d %s"), V, UnitesTrophees[t]);
		}
		Texte(Ligne1, FVector(X, Y - 67.f, 70.f), -90.f, 13.f, FColor(255, 255, 255), Expositions);
		Texte(Ligne2, FVector(X, Y - 67.f, 45.f), -90.f, 11.f, CouleurTexteRang(R), Expositions);
	}
}

void AAppartementJoueur::AnnoncerTrophees(bool bMessage)
{
	if (!Sauvegarde)
	{
		return;
	}
	Sauvegarde->RangsAnnonces.SetNum(NombreTrophees);
	bool bChange = false;
	for (int32 t = 0; t < NombreTrophees; ++t)
	{
		const int32 R = Rang(t);
		if (R > Sauvegarde->RangsAnnonces[t])
		{
			Sauvegarde->RangsAnnonces[t] = R;
			bChange = true;
			if (bMessage)
			{
				Message(FString::Printf(TEXT("NOUVEAU TROPHÉE : %s (%s) ! Va le voir dans ton appartement."), NomsTrophees[t], NomsRangs[R]));
				Son(ReglagesAppartement().SonTrophee);
			}
		}
	}
	if (bChange)
	{
		EcrireSauvegarde();
	}
}

void AAppartementJoueur::ServeurCompter(uint8 Quoi, int32 V)
{
	if (Quoi == CompteVague)
	{
		if (V <= DerniereVagueEnvoyee)
		{
			return;
		}
		DerniereVagueEnvoyee = V;
	}
	ClientCompter(Quoi, V);
}

void AAppartementJoueur::ClientCompter_Implementation(uint8 Quoi, int32 V)
{
	if (!Sauvegarde)
	{
		ChargerSauvegarde();
	}
	if (!Sauvegarde)
	{
		return;
	}
	switch (Quoi)
	{
	case CompteZombie: Sauvegarde->ZombiesTues += FMath::Max(1, V); break;
	case CompteMission: Sauvegarde->MissionsReussies += FMath::Max(1, V); break;
	case CompteVague: Sauvegarde->MeilleureVague = FMath::Max(Sauvegarde->MeilleureVague, V); break;
	default: return;
	}
	// Ecrit tout de suite pour une mission / une vague, toutes les 10 kills pour les zombies (moins d'acces disque)
	if (Quoi != CompteZombie || Sauvegarde->ZombiesTues % 10 == 0)
	{
		LireXP();
		AnnoncerTrophees(true);
		EcrireSauvegarde();
	}
}

// ===================================================================================
//  Sauvegarde, messages, sons
// ===================================================================================

void AAppartementJoueur::ChargerSauvegarde()
{
	NomSauvegarde = AppEmplacement(this, TEXT("ChellesAppartement"));
	for (const FString& Nom : { NomSauvegarde, NomSauvegarde + TEXT("_Secours") })
	{
		if (UGameplayStatics::DoesSaveGameExist(Nom, 0))
		{
			Sauvegarde = Cast<UAppartementSauvegarde>(UGameplayStatics::LoadGameFromSlot(Nom, 0));
			if (Sauvegarde)
			{
				break;
			}
		}
	}
	if (!Sauvegarde)
	{
		Sauvegarde = Cast<UAppartementSauvegarde>(UGameplayStatics::CreateSaveGameObject(UAppartementSauvegarde::StaticClass()));
	}
	LireXP();
	if (Sauvegarde && Sauvegarde->RangsAnnonces.Num() == 0)
	{
		AnnoncerTrophees(false); // premiere fois : pas d'annonce pour ce qui etait deja gagne avant
	}
}

void AAppartementJoueur::EcrireSauvegarde()
{
	if (!Sauvegarde || NomSauvegarde.IsEmpty())
	{
		return;
	}
	if (UGameplayStatics::SaveGameToSlot(Sauvegarde, NomSauvegarde, 0))
	{
		UGameplayStatics::SaveGameToSlot(Sauvegarde, NomSauvegarde + TEXT("_Secours"), 0);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Appartement] Impossible d'ecrire %s.sav"), *NomSauvegarde);
	}
}

void AAppartementJoueur::Message(const FString& Contenu)
{
	UE_LOG(LogTemp, Log, TEXT("[Appartement] %s"), *Contenu);
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	ULocalPlayer* Joueur = PC ? Cast<ULocalPlayer>(PC->Player) : nullptr;
	UGameViewportClient* Vue = Joueur ? Joueur->ViewportClient : nullptr;
	if (!ReglagesAppartement().bMessages || !Vue || !GetWorld())
	{
		return;
	}
	if (Bandeau.IsValid() && VueBandeau.IsValid())
	{
		VueBandeau->RemoveViewportWidgetContent(Bandeau.ToSharedRef());
	}
	Bandeau = SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 190.f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.65f))
			.Padding(FMargin(22.f, 10.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Contenu))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.25f))
				.Justification(ETextJustify::Center)
			]
		];
	Vue->AddViewportWidgetContent(Bandeau.ToSharedRef(), 60);
	VueBandeau = Vue;
	FinBandeau = GetWorld()->GetRealTimeSeconds() + 5.f;
}

void AAppartementJoueur::Son(const FString& Chemin)
{
	if (Chemin.IsEmpty())
	{
		return;
	}
	if (USoundBase* S = Cast<USoundBase>(FSoftObjectPath(Chemin).TryLoad()))
	{
		UGameplayStatics::PlaySound2D(this, S);
	}
}

// ===================================================================================
//  Console
// ===================================================================================

FString AAppartementJoueur::Resume() const
{
	FString S = FString::Printf(TEXT("Appartement %d, %s."), Numero, bDedans ? TEXT("dedans") : TEXT("dehors"));
	if (Sauvegarde)
	{
		S += FString::Printf(TEXT(" Garde-robe %d, garage %d, zombies %d, missions %d, vague %d, XP %d, %d h, collection %d, visites %d."),
			Sauvegarde->GardeRobe.Num(), Sauvegarde->Garage.Num(), Sauvegarde->ZombiesTues, Sauvegarde->MissionsReussies,
			Sauvegarde->MeilleureVague, XPTotale, int32(SecondesJouees / 3600.0), Sauvegarde->Collection, Sauvegarde->Visites);
		for (const FPieceAppartement& P : Sauvegarde->GardeRobe)
		{
			S += FString::Printf(TEXT("\n  skin %s (%d morceaux)"), *P.Nom, P.Morceaux.Num());
		}
		for (const FPieceAppartement& P : Sauvegarde->Garage)
		{
			S += FString::Printf(TEXT("\n  vehicule %s (%d morceaux)"), *P.Nom, P.Morceaux.Num());
		}
	}
	return S;
}

void AAppartementJoueur::ViderRangements()
{
	if (!Sauvegarde)
	{
		return;
	}
	Sauvegarde->GardeRobe.Reset();
	Sauvegarde->Garage.Reset();
	EcrireSauvegarde();
	if (bDedans)
	{
		RemplirTout();
	}
	Message(TEXT("Garde-robe et garage vides (les trophees sont gardes)."));
}

void AAppartementJoueur::TrophesDeTest()
{
	if (!Sauvegarde)
	{
		return;
	}
	Sauvegarde->ZombiesTues = FMath::Max(Sauvegarde->ZombiesTues, 120);
	Sauvegarde->MissionsReussies = FMath::Max(Sauvegarde->MissionsReussies, 1);
	Sauvegarde->MeilleureVague = FMath::Max(Sauvegarde->MeilleureVague, 21);
	AnnoncerTrophees(true);
	EcrireSauvegarde();
	if (bDedans)
	{
		RemplirTout();
	}
}
