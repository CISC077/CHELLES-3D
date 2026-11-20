// CHELLES 3D - Arsenal : projectiles (trait de blaster, fusee de bazooka, grenade a zombies). Crees par le serveur, vus par tous.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "ProjectileArsenal.generated.h"

class AAppatChelles;
class AArsenalRelais;
class UAudioComponent;
class UPointLightComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * BLASTER : trait lumineux rapide, son "piou".
 * BAZOOKA : fusee qui suit le point vise par le joueur (sa souris), traine de fumee, explose et projette.
 * GRENADE : rebondit, se pose, bipe et attire les zombies (appat) pendant quelques secondes, puis explose.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AProjectileArsenal : public AActor
{
	GENERATED_BODY()

public:
	AProjectileArsenal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, Category = "Arsenal")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Arsenal")
	UProjectileMovementComponent* Mouvement;

	UPROPERTY(VisibleAnywhere, Category = "Arsenal")
	UPointLightComponent* Lumiere;

	/** ChellesArsenal::EArme (copie chez tous). */
	UPROPERTY(Replicated)
	uint8 Arme = 1;

	UPROPERTY(Replicated)
	float Vitesse = 6000.f;

	UPROPERTY(Replicated)
	FColor Couleur = FColor::Red;

	/** Grenade posee : elle bipe et attire les zombies. */
	UPROPERTY(ReplicatedUsing = OnRep_Armee)
	bool bArmee = false;

	UPROPERTY(Replicated)
	float Attente = 5.f;

	// --- Serveur seulement ---
	float Degats = 20.f;
	float Rayon = 0.f;
	float Poussee = 0.f;
	float Virage = 0.f;
	float DureeVie = 8.f;
	float RayonAppat = 3000.f;
	bool bToucherJoueurs = true;
	TWeakObjectPtr<AArsenalRelais> Relais; // bazooka guide : le relais connait le point vise

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;

private:
	UFUNCTION()
	void Touche(UPrimitiveComponent* MonComposant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact);

	UFUNCTION()
	void Arrete(const FHitResult& Impact);

	UFUNCTION()
	void OnRep_Armee();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastImpact(FVector_NetQuantize Point, FVector_NetQuantizeNormal Normale, bool bCorps);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplosion(FVector_NetQuantize Point, float Taille);

	void Armer();
	void Exploser(const FVector& Point);
	void Finir();
	void Allure();

	bool bFini = false;
	float Age = 0.f;
	float AgeArmee = 0.f;
	float ProchaineFumee = 0.f;
	float ProchainBip = 0.f;
	float Flash = 0.f;
	float LumiereNormale = 0.f;

	UPROPERTY()
	TArray<UStaticMeshComponent*> Pieces;

	UPROPERTY()
	UAudioComponent* SonVol = nullptr;

	TWeakObjectPtr<AAppatChelles> Appat;
};
