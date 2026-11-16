// CHELLES 3D - Agent de police qui regle la circulation.
#include "AgentPolicier.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

namespace
{
	/** Cherche un os par son nom exact, puis "prefixe:Nom" (Mixamo), puis un nom qui finit par Nom. */
	int32 TrouverOs(const FReferenceSkeleton& Ref, const TArray<FString>& Noms)
	{
		for (int32 Passe = 0; Passe < 3; ++Passe)
		{
			for (const FString& Nom : Noms)
			{
				for (int32 i = 0; i < Ref.GetNum(); ++i)
				{
					const FString Os = Ref.GetBoneName(i).ToString();
					const bool bTrouve = (Passe == 0 && Os.Equals(Nom, ESearchCase::IgnoreCase))
						|| (Passe == 1 && Os.EndsWith(TEXT(":") + Nom, ESearchCase::IgnoreCase))
						|| (Passe == 2 && Os.EndsWith(Nom, ESearchCase::IgnoreCase));
					if (bTrouve)
					{
						return i;
					}
				}
			}
		}
		return INDEX_NONE;
	}

	FVector Direction(const FVector& V)
	{
		return V.GetSafeNormal();
	}
}

AAgentPolicier::AAgentPolicier()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(35.f, 90.f);
	// WorldDynamic (pas Pawn) : le joueur ne le traverse pas, mais les voitures de la circulation
	// (qui freinent devant les Pawns) ne s'arretent pas pour lui.
	Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Capsule->SetCanEverAffectNavigation(false);
	SetRootComponent(Capsule);
}

bool AAgentPolicier::Configurer(USkeletalMesh* Modele, float Taille, bool bRotationAuto, float CorrectionLacet,
	UAnimSequenceBase* InAnimRepos, UAnimSequenceBase* InAnimPassez, UAnimSequenceBase* InAnimStop)
{
	if (!Modele)
	{
		return false;
	}
	AnimRepos = InAnimRepos;
	AnimPassez = InAnimPassez ? InAnimPassez : InAnimRepos;
	AnimStop = InAnimStop ? InAnimStop : InAnimRepos;

	// ---- Poses de reference (espace du modele)
	const FReferenceSkeleton& Ref = Modele->GetRefSkeleton();
	const TArray<FTransform>& Locales = Ref.GetRefBonePose();
	TArray<FTransform> RefCS;
	RefCS.SetNum(Ref.GetNum());
	for (int32 i = 0; i < Ref.GetNum(); ++i)
	{
		const int32 Parent = Ref.GetParentIndex(i);
		RefCS[i] = (Parent == INDEX_NONE) ? Locales[i] : Locales[i] * RefCS[Parent];
	}

	auto Remplir = [&](FBras& B, const TArray<FString>& NomsBras, const TArray<FString>& NomsAvantBras, const TArray<FString>& NomsMain)
	{
		B.Bras = TrouverOs(Ref, NomsBras);
		B.AvantBras = TrouverOs(Ref, NomsAvantBras);
		B.Main = TrouverOs(Ref, NomsMain);
		if (!B.EstValide())
		{
			return;
		}
		B.Epaule = Ref.GetParentIndex(B.Bras);
		B.RefEpaule = (B.Epaule != INDEX_NONE) ? RefCS[B.Epaule] : FTransform::Identity;
		B.RefBras = RefCS[B.Bras];
		B.RefAvantBras = RefCS[B.AvantBras];
		B.RefMain = RefCS[B.Main];
	};
	Remplir(BrasGauche, { TEXT("LeftArm"), TEXT("upperarm_l") }, { TEXT("LeftForeArm"), TEXT("lowerarm_l") }, { TEXT("LeftHand"), TEXT("hand_l") });
	Remplir(BrasDroit, { TEXT("RightArm"), TEXT("upperarm_r") }, { TEXT("RightForeArm"), TEXT("lowerarm_r") }, { TEXT("RightHand"), TEXT("hand_r") });

	// ---- Devant du modele : la main gauche est a gauche, donc devant = haut ^ gauche
	float LacetModele = -90.f; // modeles Unreal habituels (regardent vers +Y)
	if (BrasGauche.EstValide() && BrasDroit.EstValide())
	{
		FVector VersGauche = BrasGauche.RefMain.GetLocation() - BrasDroit.RefMain.GetLocation();
		VersGauche.Z = 0.f;
		if (VersGauche.Normalize())
		{
			Gauche = VersGauche;
			Devant = FVector::CrossProduct(FVector::UpVector, Gauche).GetSafeNormal();
			LacetModele = -Devant.Rotation().Yaw;
		}
	}
	if (!bRotationAuto)
	{
		LacetModele = CorrectionLacet;
	}

	// ---- Taille : le modele est mis a la taille demandee, pieds au sol
	const FBox Boite = Modele->GetImportedBounds().GetBox();
	const float Hauteur = Boite.Max.Z - Boite.Min.Z;
	const float Echelle = (Hauteur > 1.f) ? FMath::Clamp(Taille / Hauteur, 0.005f, 200.f) : 1.f;
	const float DemiHauteur = FMath::Max(40.f, Taille * 0.5f);
	Capsule->SetCapsuleSize(FMath::Max(20.f, Taille * 0.2f), DemiHauteur);
	const FVector Decalage(0.f, 0.f, -DemiHauteur - Boite.Min.Z * Echelle);
	const FRotator Rotation(0.f, LacetModele, 0.f);

	USkinnedMeshComponent* Composant = nullptr;
	if (AnimRepos)
	{
		Corps = NewObject<USkeletalMeshComponent>(this, TEXT("Corps"));
		Corps->SetSkeletalMesh(Modele);
		Corps->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Corps->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Corps->bEnableUpdateRateOptimizations = true;
		Composant = Corps;
	}
	else
	{
		CorpsPose = NewObject<UPoseableMeshComponent>(this, TEXT("CorpsPose"));
		CorpsPose->SetSkeletalMesh(Modele);
		Composant = CorpsPose;
	}
	Composant->SetupAttachment(Capsule);
	Composant->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Composant->SetCanEverAffectNavigation(false);
	Composant->SetRelativeLocationAndRotation(Decalage, Rotation);
	Composant->SetRelativeScale3D(FVector(Echelle));
	Composant->SetCullDistance(30000.f);
	Composant->RegisterComponent();

	if (Corps)
	{
		JouerAnimation(EGestePolicier::Repos);
	}
	else
	{
		AppliquerGestesCalcules();
	}
	return true;
}

bool AAgentPolicier::ConfigurerFixe(UStaticMesh* Modele, float Taille, float Lacet, const FRotator& Redressement)
{
	if (!Modele)
	{
		return false;
	}
	// Rotation du modele : d'abord redresse (s'il est couche), puis tourne pour regarder devant
	const FRotator Rotation(Redressement.Pitch, Lacet, Redressement.Roll);
	const FBox Boite = Modele->GetBoundingBox().TransformBy(FTransform(Rotation));
	const float Hauteur = Boite.Max.Z - Boite.Min.Z;
	const float Echelle = (Hauteur > 1.f) ? FMath::Clamp(Taille / Hauteur, 0.005f, 200.f) : 1.f;
	const float DemiHauteur = FMath::Max(40.f, Taille * 0.5f);
	Capsule->SetCapsuleSize(FMath::Max(20.f, Taille * 0.2f), DemiHauteur);
	// Pieds au sol, centre sous la capsule
	const FVector Centre = Boite.GetCenter();
	const FVector Decalage(-Centre.X * Echelle, -Centre.Y * Echelle, -DemiHauteur - Boite.Min.Z * Echelle);

	CorpsFixe = NewObject<UStaticMeshComponent>(this, TEXT("CorpsFixe"));
	CorpsFixe->SetMobility(EComponentMobility::Movable);
	CorpsFixe->SetStaticMesh(Modele);
	CorpsFixe->SetupAttachment(Capsule);
	// Jamais de physique : sinon le modele tombe et se couche par terre
	CorpsFixe->SetSimulatePhysics(false);
	CorpsFixe->SetEnableGravity(false);
	CorpsFixe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CorpsFixe->SetCanEverAffectNavigation(false);
	CorpsFixe->SetRelativeLocationAndRotation(Decalage, Rotation);
	CorpsFixe->SetRelativeScale3D(FVector(Echelle));
	CorpsFixe->SetCullDistance(30000.f);
	CorpsFixe->RegisterComponent();
	bPoseFaite = true;
	return true;
}

void AAgentPolicier::DonnerOrdre(EGestePolicier NouveauGeste, float InLacetVise, float CotePassage)
{
	LacetVise = InLacetVise;
	if (NouveauGeste != Geste)
	{
		Geste = NouveauGeste;
		DebutGeste = Temps;
		Cote = CotePassage >= 0.f ? 1.f : -1.f;
		JouerAnimation(Geste);
	}
}

void AAgentPolicier::JouerAnimation(EGestePolicier NouveauGeste)
{
	if (!Corps)
	{
		return;
	}
	UAnimSequenceBase* Anim = AnimRepos;
	if (NouveauGeste == EGestePolicier::Passez) { Anim = AnimPassez; }
	if (NouveauGeste == EGestePolicier::Stop) { Anim = AnimStop; }
	if (!Anim || (GesteAffiche == NouveauGeste && Corps->GetAnimationMode() == EAnimationMode::AnimationSingleNode && bPoseFaite))
	{
		return;
	}
	Corps->PlayAnimation(Anim, true);
	GesteAffiche = NouveauGeste;
	bPoseFaite = true;
}

void AAgentPolicier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Temps += DeltaSeconds;

	// Se tourne doucement vers la voiture a regler
	const FRotator Actuelle = GetActorRotation();
	const float Lacet = FMath::FixedTurn(Actuelle.Yaw, LacetVise, 220.f * DeltaSeconds);
	if (!FMath::IsNearlyEqual(FRotator::NormalizeAxis(Lacet - Actuelle.Yaw), 0.f, 0.01f))
	{
		SetActorRotation(FRotator(0.f, Lacet, 0.f));
	}

	if (CorpsPose && (WasRecentlyRendered(0.3f) || !bPoseFaite))
	{
		AppliquerGestesCalcules();
	}
}

void AAgentPolicier::AppliquerGestesCalcules()
{
	if (!CorpsPose || CorpsPose->BoneSpaceTransforms.Num() == 0)
	{
		return;
	}
	const FVector Haut = FVector::UpVector;
	const FVector Droite = -Gauche;

	// Repos : bras le long du corps
	FVector BrasG = Direction(-Haut * 0.96f + Gauche * 0.2f + Devant * 0.04f);
	FVector AvantG = Direction(-Haut * 0.93f + Devant * 0.3f + Gauche * 0.05f);
	FVector BrasD = Direction(-Haut * 0.96f + Droite * 0.2f + Devant * 0.04f);
	FVector AvantD = Direction(-Haut * 0.93f + Devant * 0.3f + Droite * 0.05f);

	const float T = Temps - DebutGeste;
	const float Montee = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(T / 0.4f, 0.f, 1.f));

	if (Geste == EGestePolicier::Stop)
	{
		// Bras droit tendu vers la voiture, avant-bras leve : main ouverte, "on s'arrete"
		const FVector B = Direction(Devant * 0.75f + Haut * 0.6f + Droite * 0.12f);
		const FVector A = Direction(Haut * 0.95f + Devant * 0.3f);
		BrasD = Direction(FMath::Lerp(BrasD, B, Montee));
		AvantD = Direction(FMath::Lerp(AvantD, A, Montee));
	}
	else if (Geste == EGestePolicier::Passez)
	{
		// Le bras montre la voiture puis balaie vers le cote ou elle doit aller : "passez"
		const bool bDroite = Cote >= 0.f;
		const FVector Cote3D = bDroite ? Droite : Gauche;
		const float Balancement = 0.5f - 0.5f * FMath::Cos(T * 2.f * PI * 0.65f);
		const float Angle = FMath::DegreesToRadians(10.f + 75.f * Balancement);
		const float AngleAvant = Angle + FMath::DegreesToRadians(20.f);
		const FVector B = Direction(Devant * FMath::Cos(Angle) + Cote3D * FMath::Sin(Angle) + Haut * 0.12f);
		const FVector A = Direction(Devant * FMath::Cos(AngleAvant) + Cote3D * FMath::Sin(AngleAvant) + Haut * 0.2f);
		FVector& BrasActif = bDroite ? BrasD : BrasG;
		FVector& AvantActif = bDroite ? AvantD : AvantG;
		BrasActif = Direction(FMath::Lerp(BrasActif, B, Montee));
		AvantActif = Direction(FMath::Lerp(AvantActif, A, Montee));
	}

	PlacerBras(BrasGauche, BrasG, AvantG);
	PlacerBras(BrasDroit, BrasD, AvantD);
	CorpsPose->RefreshBoneTransforms();
	bPoseFaite = true;
}

void AAgentPolicier::PlacerBras(const FBras& B, const FVector& DirBras, const FVector& DirAvantBras)
{
	if (!B.EstValide() || !CorpsPose)
	{
		return;
	}
	TArray<FTransform>& Locales = CorpsPose->BoneSpaceTransforms;
	if (!Locales.IsValidIndex(B.Bras) || !Locales.IsValidIndex(B.AvantBras))
	{
		return;
	}
	const FVector RefDirBras = (B.RefAvantBras.GetLocation() - B.RefBras.GetLocation()).GetSafeNormal();
	const FVector RefDirAvant = (B.RefMain.GetLocation() - B.RefAvantBras.GetLocation()).GetSafeNormal();
	if (RefDirBras.IsNearlyZero() || RefDirAvant.IsNearlyZero())
	{
		return;
	}

	// Bras : on tourne la pose de reference pour qu'il pointe dans DirBras
	const FQuat Rotation1 = FQuat::FindBetweenNormals(RefDirBras, DirBras);
	FTransform NouveauBras = B.RefBras;
	NouveauBras.SetRotation(Rotation1 * B.RefBras.GetRotation());

	// Avant-bras : suit le bras, puis tourne pour pointer dans DirAvantBras
	const FTransform AvantBrasLocalRef = B.RefAvantBras.GetRelativeTransform(B.RefBras);
	FTransform NouvelAvantBras = AvantBrasLocalRef * NouveauBras;
	const FQuat Rotation2 = FQuat::FindBetweenNormals(Rotation1.RotateVector(RefDirAvant), DirAvantBras);
	NouvelAvantBras.SetRotation(Rotation2 * NouvelAvantBras.GetRotation());

	Locales[B.Bras] = NouveauBras.GetRelativeTransform(B.RefEpaule);
	Locales[B.AvantBras] = NouvelAvantBras.GetRelativeTransform(NouveauBras);
}
