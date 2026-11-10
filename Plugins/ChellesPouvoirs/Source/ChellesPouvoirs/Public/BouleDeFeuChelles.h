// CHELLES 3D - Pouvoir BOULE DE FEU : le projectile (cree par le serveur, vu par tous)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BouleDeFeuChelles.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESPOUVOIRS_API ABouleDeFeuChelles : public AActor
{
	GENERATED_BODY()

public:
	ABouleDeFeuChelles();

	UPROPERTY(VisibleAnywhere, Category = "Boule de feu")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Boule de feu")
	UStaticMeshComponent* Boule;

	UPROPERTY(VisibleAnywhere, Category = "Boule de feu")
	UPointLightComponent* Lumiere;

	UPROPERTY(VisibleAnywhere, Category = "Boule de feu")
	UProjectileMovementComponent* Mouvement;

	/** Degats au centre de l'explosion. */
	float Degats = 45.f;
	/** Rayon de l'explosion (cm). */
	float Rayon = 350.f;
	/** Faux : les autres joueurs ne prennent pas de degats (PVP coupe par l'hote). */
	bool bToucherJoueurs = true;

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

private:
	UFUNCTION()
	void Touche(UPrimitiveComponent* MonComposant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact);

	void Exploser();

	bool bExplosee = false;
};
