// CHELLES 3D - Butin : marqueur "ping" (facon Apex) vu par tous les joueurs, et petits effets (explosions). UE 4.27.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PingButin.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class USceneComponent;

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESBUTIN_API APingButin : public AActor
{
	GENERATED_BODY()

public:
	APingButin();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** ChellesButin::PingIci / PingEnnemi / PingButin / PingVehicule / PingFrappe */
	UPROPERTY(Replicated)
	uint8 Genre = 0;

	/** Nom du joueur qui a marque. */
	UPROPERTY(Replicated)
	FString Auteur;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Racine;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Colonne;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Point;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* Texte;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* Lumiere;

	float Temps = 0.f;
};

/** Explosion de la frappe aerienne : creee par le serveur, l'effet et le son se jouent chez tous. */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESBUTIN_API AEffetButin : public AActor
{
	GENERATED_BODY()

public:
	AEffetButin();

protected:
	virtual void BeginPlay() override;
};
