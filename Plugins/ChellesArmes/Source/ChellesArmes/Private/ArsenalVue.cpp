// CHELLES 3D - Arsenal : l'arme vue dans les mains du joueur
#include "ArsenalVue.h"
#include "ArsenalVisuel.h"
#include "SonArsenalChelles.h"

#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

using namespace ChellesArsenal;
using namespace ChellesArsenalVisuel;

AArsenalVue::AArsenalVue()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = false;
	SetActorEnableCollision(false);
	Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;
}

void AArsenalVue::Vider()
{
	if (Bourdon)
	{
		Bourdon->Stop();
		Bourdon = nullptr;
	}
	for (USceneComponent* P : Pieces)
	{
		if (P)
		{
			P->DestroyComponent();
		}
	}
	Pieces.Reset();
	if (PointBouche)
	{
		PointBouche->DestroyComponent();
		PointBouche = nullptr;
	}
	if (Pivot)
	{
		Pivot->DestroyComponent();
		Pivot = nullptr;
	}
	Lame = nullptr;
	Ogive = nullptr;
	Canons = nullptr;
	VitesseCanons = 0.f;
	Lueur = nullptr;
}

void AArsenalVue::EndPlay(const EEndPlayReason::Type Raison)
{
	Vider();
	Super::EndPlay(Raison);
}

void AArsenalVue::Construire(uint8 NouvelleArme, const FReglagesArsenal& R)
{
	Vider();
	Arme = NouvelleArme;
	Rechargement = 1.f;
	DebutTir = -100.f;
	if (Arme == Aucune)
	{
		return;
	}

	Pivot = NewObject<USceneComponent>(this);
	Pivot->SetupAttachment(Racine);
	Pivot->RegisterComponent();
	PointBouche = NewObject<USceneComponent>(this);
	PointBouche->SetupAttachment(Pivot);
	PointBouche->RegisterComponent();

	auto Ajouter = [this](UStaticMeshComponent* C) -> UStaticMeshComponent*
	{
		if (C)
		{
			Pieces.Add(C);
		}
		return C;
	};
	auto Lumiere = [this](const FVector& Ou, const FLinearColor& Couleur, float Force, float Portee) -> UPointLightComponent*
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(Pivot);
		L->SetRelativeLocation(Ou);
		L->SetLightColor(Couleur);
		L->SetIntensity(Force);
		L->SetAttenuationRadius(Portee);
		L->SetCastShadows(false);
		L->RegisterComponent();
		Pieces.Add(L);
		return L;
	};

	const FLinearColor Noir(0.02f, 0.02f, 0.025f);
	const FLinearColor Gris(0.18f, 0.18f, 0.2f);
	const FLinearColor Metal(0.45f, 0.45f, 0.48f);

	switch (Arme)
	{
	case Blaster:
	{
		// Pistolet blaster de science-fiction, tenu a droite
		PositionRepos = FVector(34.f, 13.f, -14.f);
		PoseRepos = FRotator::ZeroRotator;
		const FLinearColor C = R.BlasterCouleur;
		Ajouter(Piece(Pivot, Cube, FVector(0.f, 0.f, 0.f), FVector(24.f, 4.5f, 6.5f), Gris, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(16.f, 0.f, 1.5f), FVector(16.f, 2.4f, 2.4f), Noir, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(9.f, 0.f, 1.5f), FVector(2.5f, 3.4f, 3.4f), C, true));
		Ajouter(Piece(Pivot, Cylindre, FVector(13.f, 0.f, 1.5f), FVector(1.2f, 3.2f, 3.2f), C, true));
		Ajouter(Piece(Pivot, Cube, FVector(-2.f, 0.f, 5.f), FVector(9.f, 1.8f, 2.4f), Noir, false));
		Ajouter(Piece(Pivot, Cube, FVector(-7.f, 0.f, -7.f), FVector(4.5f, 3.5f, 11.f), Noir, false, FRotator(14.f, 0.f, 0.f)));
		Ajouter(Piece(Pivot, Cube, FVector(3.f, 0.f, -5.5f), FVector(3.f, 2.6f, 6.f), C, true));
		PointBouche->SetRelativeLocation(FVector(25.f, 0.f, 1.5f));
		Lueur = Lumiere(FVector(14.f, 0.f, 1.5f), C, 250.f, 60.f);
		break;
	}
	case Bazooka:
	{
		// Tube sur l'epaule droite, ogive rouge chargee a l'avant
		PositionRepos = FVector(22.f, 24.f, -13.f);
		PoseRepos = FRotator::ZeroRotator;
		const FLinearColor Olive(0.16f, 0.2f, 0.08f);
		Ajouter(Piece(Pivot, Cylindre, FVector(12.f, 0.f, 0.f), FVector(100.f, 13.f, 13.f), Olive, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(60.f, 0.f, 0.f), FVector(6.f, 15.f, 15.f), Noir, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(-10.f, 0.f, 0.f), FVector(6.f, 15.f, 15.f), Noir, false));
		Ajouter(Piece(Pivot, Cube, FVector(6.f, 0.f, -11.f), FVector(3.f, 3.f, 12.f), Noir, false));
		Ajouter(Piece(Pivot, Cube, FVector(20.f, -6.f, 9.f), FVector(10.f, 2.f, 5.f), Noir, false));
		Ajouter(Piece(Pivot, Cube, FVector(24.f, -6.f, 12.f), FVector(2.f, 1.f, 1.f), FLinearColor(1.f, 0.1f, 0.05f), true));
		Ogive = Ajouter(Piece(Pivot, Cone, FVector(68.f, 0.f, 0.f), FVector(14.f, 10.f, 10.f), FLinearColor(0.8f, 0.05f, 0.03f), false));
		PointBouche->SetRelativeLocation(FVector(75.f, 0.f, 0.f));
		break;
	}
	case Sabre:
	{
		// Poignee metal + lame lumineuse, tenue en garde a droite
		PositionRepos = FVector(38.f, 20.f, -26.f);
		PoseRepos = FRotator(-40.f, 0.f, 25.f);
		const FLinearColor C = R.SabreCouleur;
		Ajouter(Piece(Pivot, Cylindre, FVector(0.f, 0.f, 0.f), FVector(4.f, 4.f, 26.f), Metal, false, FRotator::ZeroRotator, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(0.f, 0.f, -4.f), FVector(4.4f, 4.4f, 6.f), Noir, false, FRotator::ZeroRotator, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(0.f, 0.f, 13.f), FVector(5.5f, 5.5f, 2.5f), Noir, false, FRotator::ZeroRotator, false));
		Ajouter(Piece(Pivot, Cube, FVector(2.2f, 0.f, 4.f), FVector(1.f, 1.2f, 2.f), FLinearColor(1.f, 0.1f, 0.05f), true));
		LongueurLame = 100.f;
		Lame = Ajouter(Piece(Pivot, Cylindre, FVector(0.f, 0.f, 14.f), FVector(3.f, 3.f, 1.f), C, true, FRotator::ZeroRotator, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(0.f, 0.f, 14.f + LongueurLame * 0.5f), FVector(1.4f, 1.4f, LongueurLame), FLinearColor::White, true,
			FRotator::ZeroRotator, false));
		Pieces.Last()->SetVisibility(false); // coeur blanc, affiche une fois la lame sortie
		PointBouche->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
		Lueur = Lumiere(FVector(0.f, 0.f, 60.f), C, 0.f, 260.f);
		DebutAllumage = Temps;
		USonArsenalChelles::Jouer(this, ChellesArsenalSons::Allumage, GetActorLocation(), 0.8f * R.Volume);
		Bourdon = USonArsenalChelles::Jouer(this, ChellesArsenalSons::Bourdonnement, GetActorLocation(), 0.35f * R.Volume, 1.f, Pivot);
		break;
	}
	case Grenade:
	{
		PositionRepos = FVector(34.f, 19.f, -17.f);
		PoseRepos = FRotator::ZeroRotator;
		Ajouter(Piece(Pivot, ChellesArsenalVisuel::Sphere, FVector::ZeroVector, FVector(9.f), FLinearColor(0.08f, 0.18f, 0.06f), false));
		Ajouter(Piece(Pivot, Cylindre, FVector::ZeroVector, FVector(9.5f, 9.5f, 2.f), FLinearColor(1.f, 0.08f, 0.05f), true, FRotator::ZeroRotator, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(0.f, 0.f, 5.5f), FVector(2.f, 2.f, 3.f), Metal, false, FRotator::ZeroRotator, false));
		Ajouter(Piece(Pivot, Cube, FVector(2.f, 0.f, 6.f), FVector(5.f, 1.f, 1.f), Metal, false));
		PointBouche->SetRelativeLocation(FVector(8.f, 0.f, 0.f));
		break;
	}
	case LanceFlammes:
	{
		// Lance-flammes : reservoir rouge sous le bras, long tube, buse noire et petite flamme pilote bleue
		PositionRepos = FVector(30.f, 18.f, -20.f);
		PoseRepos = FRotator::ZeroRotator;
		const FLinearColor Rouge(0.45f, 0.04f, 0.02f);
		Ajouter(Piece(Pivot, Cylindre, FVector(-6.f, 4.f, -8.f), FVector(28.f, 9.f, 9.f), Rouge, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(-6.f, 4.f, -8.f), FVector(3.f, 9.6f, 9.6f), Noir, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(18.f, 0.f, 2.f), FVector(48.f, 3.2f, 3.2f), Metal, false));
		Ajouter(Piece(Pivot, Cylindre, FVector(44.f, 0.f, 2.f), FVector(8.f, 5.f, 5.f), Noir, false));
		Ajouter(Piece(Pivot, Cube, FVector(4.f, 0.f, -6.f), FVector(4.f, 3.f, 10.f), Noir, false, FRotator(12.f, 0.f, 0.f)));
		Ajouter(Piece(Pivot, Cylindre, FVector(26.f, 0.f, -2.f), FVector(10.f, 1.2f, 1.2f), Noir, false));
		Ajouter(Piece(Pivot, ChellesArsenalVisuel::Sphere, FVector(49.f, 0.f, -2.f), FVector(1.6f), FLinearColor(0.15f, 0.4f, 1.f), true));
		PointBouche->SetRelativeLocation(FVector(50.f, 0.f, 2.f));
		Lueur = Lumiere(FVector(52.f, 0.f, 2.f), FLinearColor(1.f, 0.45f, 0.1f), 0.f, 400.f);
		break;
	}
	case Mitrailleuse:
	{
		// Mitrailleuse lourde a canons tournants
		PositionRepos = FVector(30.f, 18.f, -19.f);
		PoseRepos = FRotator::ZeroRotator;
		const FLinearColor C = R.MitrailleuseCouleur;
		Ajouter(Piece(Pivot, Cube, FVector(0.f, 0.f, 0.f), FVector(26.f, 9.f, 10.f), Gris, false));
		Ajouter(Piece(Pivot, Cube, FVector(-4.f, 0.f, -9.f), FVector(10.f, 7.f, 8.f), Noir, false));
		Ajouter(Piece(Pivot, Cube, FVector(4.f, 0.f, 8.f), FVector(16.f, 2.f, 3.f), Noir, false));
		Canons = NewObject<USceneComponent>(this);
		Canons->SetupAttachment(Pivot);
		Canons->SetRelativeLocation(FVector(14.f, 0.f, 0.f));
		Canons->RegisterComponent();
		Pieces.Add(Canons);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = FMath::DegreesToRadians(60.f * i);
			Ajouter(Piece(Canons, Cylindre, FVector(18.f, 3.f * FMath::Cos(A), 3.f * FMath::Sin(A)), FVector(36.f, 1.5f, 1.5f), Noir, false));
		}
		Ajouter(Piece(Canons, Cylindre, FVector(30.f, 0.f, 0.f), FVector(2.f, 8.f, 8.f), Metal, false));
		Ajouter(Piece(Pivot, Cube, FVector(8.f, 5.f, 0.f), FVector(6.f, 1.f, 3.f), C, true));
		PointBouche->SetRelativeLocation(FVector(52.f, 0.f, 0.f));
		Lueur = Lumiere(FVector(52.f, 0.f, 0.f), C, 0.f, 120.f);
		break;
	}
	default:
		break;
	}
	Pivot->SetRelativeLocationAndRotation(PositionRepos, PoseRepos);
}

FVector AArsenalVue::Bouche() const
{
	return PointBouche ? PointBouche->GetComponentLocation() : GetActorLocation();
}

void AArsenalVue::Tirer()
{
	DebutTir = Temps;
	if (Arme == Bazooka || Arme == Grenade)
	{
		Rechargement = 0.f;
	}
}

void AArsenalVue::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;
	if (!Pivot)
	{
		return;
	}
	const float T = Temps - DebutTir;
	FVector Position = PositionRepos;
	FRotator Pose = PoseRepos;

	switch (Arme)
	{
	case Blaster:
		// Petit recul a chaque tir + la bague s'illumine
		if (T < 0.12f)
		{
			const float K = 1.f - T / 0.12f;
			Position.X -= 2.5f * K;
			Pose.Pitch += 4.f * K;
			if (Lueur)
			{
				Lueur->SetIntensity(250.f + 1500.f * K);
			}
		}
		break;

	case Bazooka:
		if (T < 0.3f)
		{
			const float K = 1.f - T / 0.3f;
			Position.X -= 8.f * K;
			Pose.Pitch += 6.f * K;
		}
		// L'ogive revient apres le rechargement
		Rechargement = FMath::Min(1.f, Rechargement + DeltaSeconds / 1.2f);
		if (Ogive)
		{
			Ogive->SetVisibility(Rechargement >= 1.f);
		}
		break;

	case Grenade:
		// La grenade disparait (lancee) puis une nouvelle remonte dans la main
		Rechargement = FMath::Min(1.f, Rechargement + DeltaSeconds / 1.5f);
		if (Rechargement < 0.5f)
		{
			Position.Z -= 40.f;
		}
		else
		{
			Position.Z -= 40.f * (1.f - (Rechargement - 0.5f) * 2.f);
		}
		break;

	case Sabre:
	{
		// La lame sort de la poignee en 0,25 s
		const float Sortie = FMath::Clamp((Temps - DebutAllumage) / 0.25f, 0.f, 1.f);
		if (Lame)
		{
			const float L = FMath::Max(1.f, LongueurLame * Sortie);
			Lame->SetRelativeLocation(FVector(0.f, 0.f, 14.f + L * 0.5f));
			Lame->SetRelativeScale3D(FVector(0.03f, 0.03f, L / 100.f));
		}
		if (Sortie >= 1.f && Pieces.Num() > 0)
		{
			for (USceneComponent* P : Pieces)
			{
				if (P && !P->IsVisible() && P != Ogive)
				{
					P->SetVisibility(true);
				}
			}
		}
		if (Lueur)
		{
			// Leger scintillement de la lame
			Lueur->SetIntensity(Sortie * (1800.f + 250.f * FMath::Sin(Temps * 37.f)));
		}
		// Coup : la lame part en diagonale puis revient en garde
		const FRotator Frappe(-120.f, -15.f, -55.f);
		if (T < 0.13f)
		{
			Pose = FMath::Lerp(PoseRepos, Frappe, FMath::InterpEaseOut(0.f, 1.f, T / 0.13f, 2.f));
			Position += FVector(6.f, -10.f, 4.f) * (T / 0.13f);
		}
		else if (T < 0.35f)
		{
			const float K = (T - 0.13f) / 0.22f;
			Pose = FMath::Lerp(Frappe, PoseRepos, FMath::InterpEaseInOut(0.f, 1.f, K, 2.f));
			Position += FVector(6.f, -10.f, 4.f) * (1.f - K);
		}
		break;
	}
	case LanceFlammes:
		// La buse tremble et s'illumine pendant le jet
		if (T < 0.15f)
		{
			const float K = 1.f - T / 0.15f;
			Position += FMath::VRand() * 0.6f * K;
			if (Lueur)
			{
				Lueur->SetIntensity(3500.f * K + 400.f * FMath::Sin(Temps * 50.f) * K);
			}
		}
		else if (Lueur)
		{
			Lueur->SetIntensity(0.f);
		}
		break;

	case Mitrailleuse:
		// Les canons tournent tant qu'on tire, puis ralentissent
		VitesseCanons = (T < 0.15f) ? FMath::Min(1440.f, VitesseCanons + 4000.f * DeltaSeconds) : FMath::Max(0.f, VitesseCanons - 900.f * DeltaSeconds);
		if (Canons)
		{
			Canons->AddLocalRotation(FRotator(0.f, 0.f, VitesseCanons * DeltaSeconds));
		}
		if (T < 0.07f)
		{
			const float K = 1.f - T / 0.07f;
			Position.X -= 1.8f * K;
			Pose.Pitch += 1.5f * K;
			if (Lueur)
			{
				Lueur->SetIntensity(1200.f * K);
			}
		}
		else if (Lueur)
		{
			Lueur->SetIntensity(0.f);
		}
		break;

	default:
		break;
	}
	Pivot->SetRelativeLocationAndRotation(Position, Pose);
}
