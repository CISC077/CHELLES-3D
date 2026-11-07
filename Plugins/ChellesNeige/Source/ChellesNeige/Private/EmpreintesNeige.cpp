#include "EmpreintesNeige.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "LandscapeProxy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"

static const float IntervalleReport = 0.1f;

AEmpreintesNeige::AEmpreintesNeige()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	USceneComponent* Racine = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Racine;
	SetActorHiddenInGame(true);
}

FVector2D AEmpreintesNeige::PositionJoueur(bool& bTrouve) const
{
	bTrouve = false;
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			FVector Lieu;
			FRotator Rot;
			PC->GetPlayerViewPoint(Lieu, Rot);
			if (APawn* P = PC->GetPawn())
			{
				Lieu = P->GetActorLocation();
			}
			bTrouve = true;
			return FVector2D(Lieu.X, Lieu.Y);
		}
	}
	return FVector2D::ZeroVector;
}

void AEmpreintesNeige::MettreAJourCollection()
{
	if (!Collection)
	{
		return;
	}
	UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("CentreX"), Centre.X);
	UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("CentreY"), Centre.Y);
	UKismetMaterialLibrary::SetScalarParameterValue(this, Collection, TEXT("Taille"), TailleZone);
}

void AEmpreintesNeige::BeginPlay()
{
	Super::BeginPlay();
	if (!Cible || !Tampon || !MatEmpreinte || !MatReport || !Collection || TailleZone <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("EmpreintesNeige : reglages incomplets, relancez Tools/Neige/ajouter_neige.py"));
		return;
	}
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Cible, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(this, Tampon, FLinearColor::Black);
	MidVersTampon = UMaterialInstanceDynamic::Create(MatReport, this);
	MidVersTampon->SetTextureParameterValue(TEXT("Source"), Cible);
	MidVersCible = UMaterialInstanceDynamic::Create(MatReport, this);
	MidVersCible->SetTextureParameterValue(TEXT("Source"), Tampon);
	MidVersCible->SetVectorParameterValue(TEXT("Decalage"), FLinearColor(0.f, 0.f, 0.f, 0.f));
	MidVersCible->SetScalarParameterValue(TEXT("Attenuation"), 1.f);

	bool bTrouve = false;
	const FVector2D J = PositionJoueur(bTrouve);
	const float Pixel = TailleZone / FMath::Max(1, Cible->SizeX);
	Centre = FVector2D(FMath::RoundToFloat(J.X / Pixel) * Pixel, FMath::RoundToFloat(J.Y / Pixel) * Pixel);
	MettreAJourCollection();
	bPret = true;
}

void AEmpreintesNeige::ReporterEtEffacer(const FVector2D& NouveauCentre, float Attenuation)
{
	const FVector2D Decalage = (NouveauCentre - Centre) / TailleZone;
	MidVersTampon->SetVectorParameterValue(TEXT("Decalage"), FLinearColor(Decalage.X, Decalage.Y, 0.f, 0.f));
	MidVersTampon->SetScalarParameterValue(TEXT("Attenuation"), Attenuation);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, Tampon, MidVersTampon);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, Cible, MidVersCible);
	Centre = NouveauCentre;
	MettreAJourCollection();
}

bool AEmpreintesNeige::SurLaNeige(ACharacter* Perso) const
{
	UCharacterMovementComponent* Mvt = Perso->GetCharacterMovement();
	if (!Mvt || !Mvt->IsMovingOnGround())
	{
		return false;
	}
	if (!bSeulementSurLandscape)
	{
		return true;
	}
	const AActor* Sol = Mvt->CurrentFloor.HitResult.GetActor();
	return Sol && Sol->IsA(ALandscapeProxy::StaticClass());
}

void AEmpreintesNeige::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPret)
	{
		return;
	}

	// 1. toutes les 0,1 s : effacement progressif + la zone suit le joueur (decalage d'un nombre entier de pixels)
	Minuteur += DeltaSeconds;
	if (Minuteur >= IntervalleReport)
	{
		const float Pas01 = Minuteur;
		Minuteur = 0.f;
		const float Attenuation = FMath::Pow(0.02f, Pas01 / FMath::Max(1.f, DureeDisparition));
		FVector2D NouveauCentre = Centre;
		bool bTrouve = false;
		const FVector2D J = PositionJoueur(bTrouve);
		if (bTrouve && (FMath::Abs(J.X - Centre.X) > TailleZone * 0.1f || FMath::Abs(J.Y - Centre.Y) > TailleZone * 0.1f))
		{
			const float Pixel = TailleZone / FMath::Max(1, Cible->SizeX);
			NouveauCentre = FVector2D(FMath::RoundToFloat(J.X / Pixel) * Pixel, FMath::RoundToFloat(J.Y / Pixel) * Pixel);
		}
		ReporterEtEffacer(NouveauCentre, Attenuation);
	}

	// 2. un pas tous les LongueurPas cm pour chaque personnage qui marche sur la neige
	struct FTampon { FVector2D Pos; float Angle; };
	TArray<FTampon> ATamponner;
	APawn* Joueur = nullptr;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		Joueur = PC->GetPawn();
	}
	const float Demi = TailleZone * 0.5f;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Perso = *It;
		if (!Perso || Perso->IsHidden() || (!bTousLesPersonnages && Perso != Joueur))
		{
			continue;
		}
		const FVector Lieu = Perso->GetActorLocation();
		if (FMath::Abs(Lieu.X - Centre.X) > Demi || FMath::Abs(Lieu.Y - Centre.Y) > Demi)
		{
			continue;
		}
		FPas& Etat = Pas.FindOrAdd(Perso);
		if (!Etat.bInit)
		{
			Etat.DernierPas = Lieu;
			Etat.bInit = true;
			continue;
		}
		if (!SurLaNeige(Perso))
		{
			Etat.DernierPas = Lieu;
			continue;
		}
		const FVector Depl = FVector(Lieu.X - Etat.DernierPas.X, Lieu.Y - Etat.DernierPas.Y, 0.f);
		const float Dist = Depl.Size();
		if (Dist > 400.f)
		{
			Etat.DernierPas = Lieu;  // teleporte : pas de trainee
			continue;
		}
		if (Dist < LongueurPas)
		{
			continue;
		}
		const FVector Avant = Depl / Dist;
		const FVector Droite(-Avant.Y, Avant.X, 0.f);
		Etat.bPiedGauche = !Etat.bPiedGauche;
		const FVector Pied = Lieu + Droite * (Etat.bPiedGauche ? -EcartPieds : EcartPieds);
		ATamponner.Add({ FVector2D(Pied.X, Pied.Y), FMath::RadiansToDegrees(FMath::Atan2(Avant.Y, Avant.X)) });
		Etat.DernierPas = Lieu;
	}
	for (auto It = Pas.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	if (ATamponner.Num() == 0)
	{
		return;
	}

	UCanvas* Canvas = nullptr;
	FVector2D TailleRT;
	FDrawToRenderTargetContext Contexte;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Cible, Canvas, TailleRT, Contexte);
	if (Canvas)
	{
		const FVector2D TaillePix = TailleEmpreinte / TailleZone * TailleRT;
		for (const FTampon& T : ATamponner)
		{
			const FVector2D UV = (T.Pos - Centre) / TailleZone + FVector2D(0.5f, 0.5f);
			const FVector2D Coin = UV * TailleRT - TaillePix * 0.5f;
			Canvas->K2_DrawMaterial(MatEmpreinte, Coin, TaillePix, FVector2D(0.f, 0.f), FVector2D(1.f, 1.f),
				T.Angle, FVector2D(0.5f, 0.5f));
		}
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Contexte);
}
