// CHELLES 3D - Conducteur ejecte qui s'enfuit en courant (pur decor, local a chaque machine, disparait vite).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FuyardChelles.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimationAsset;

UCLASS(NotPlaceable, Transient)
class CHELLESVOLVOITURE_API AFuyardChelles : public AActor
{
	GENERATED_BODY()

public:
	AFuyardChelles();

	void Lancer(USkeletalMesh* Mesh, UAnimationAsset* Course);

	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Chelles")
	USkeletalMeshComponent* Corps;

	float Vitesse = 480.f; // cm/s
};
