#include "ChellesVegetationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"

AChellesVegetationActor::AChellesVegetationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	RootComponent = Instances;
	// Movable comme le reste du quartier (pas de lightmap a construire, eclairage jour/nuit dynamique)
	Instances->SetMobility(EComponentMobility::Movable);
	// La vegetation ne touche pas au NavMesh : pas de recalcul de navigation au lancement
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetGenerateOverlapEvents(false);
	Instances->bAffectDistanceFieldLighting = false;
}

int32 AChellesVegetationActor::Remplir(UStaticMesh* Modele, const TArray<FTransform>& Transforms, float CullDebutCm,
                                       float CullFinCm, bool bOmbres, bool bCollision)
{
	if (!Instances || !Modele)
	{
		return 0;
	}
	SetActorTransform(FTransform::Identity);

	Instances->ClearInstances();
	Instances->SetStaticMesh(Modele);
	Instances->InstanceStartCullDistance = FMath::Max(0, FMath::RoundToInt(CullDebutCm));
	Instances->InstanceEndCullDistance = FMath::Max(0, FMath::RoundToInt(CullFinCm));
	Instances->SetCastShadow(bOmbres);
	Instances->bCastDynamicShadow = bOmbres;
	Instances->SetCanEverAffectNavigation(false);
	if (bCollision)
	{
		Instances->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Instances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		Instances->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Instances->AddInstances(Transforms, false);
	Instances->BuildTreeIfOutdated(false, true);
	Instances->MarkRenderStateDirty();
	Modify();
	return Instances->GetInstanceCount();
}

int32 AChellesVegetationActor::NombreDeCopies() const
{
	return Instances ? Instances->GetInstanceCount() : 0;
}
