// CHELLES 3D - Armes : projectile d'un tir special (cree par le serveur, vu par tous)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TirSpecialChelles.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

/**
 * Un seul projectile pour les 4 tirs speciaux, son Type decide de l'allure et de l'effet :
 *  LASER  : trait rouge tres rapide.
 *  FEU    : boule de feu qui explose (degats autour).
 *  GLACE  : eclat bleu qui gele la cible quelques secondes.
 *  FOUDRE : eclair qui saute sur les ennemis voisins.
 * Les degats passent par ApplyPointDamage : le plugin Zombies garde ses regles (tete = mort, corps = 33 %).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API ATirSpecialChelles : public AActor
{
	GENERATED_BODY()

public:
	ATirSpecialChelles();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Tir special")
	USphereComponent* Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Tir special")
	UStaticMeshComponent* Forme;

	UPROPERTY(VisibleAnywhere, Category = "Tir special")
	UPointLightComponent* Lumiere;

	UPROPERTY(VisibleAnywhere, Category = "Tir special")
	UProjectileMovementComponent* Mouvement;

	/** ChellesArmes::ETir (copie chez tous). */
	UPROPERTY(Replicated)
	uint8 Type = 1;

	/** Vitesse (copiee chez tous pour que le projectile avance pareil partout). */
	UPROPERTY(Replicated)
	float Vitesse = 6000.f;

	// --- Serveur seulement ---
	float Degats = 20.f;
	float DegatsZone = 0.f;
	float Rayon = 0.f;
	float DureeGel = 3.f;
	int32 Rebonds = 0;
	/** Faux : les autres joueurs ne prennent pas de degats (PVP coupe par l'hote). */
	bool bToucherJoueurs = true;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void Touche(UPrimitiveComponent* MonComposant, AActor* Autre, UPrimitiveComponent* AutreComposant, FVector Impulsion, const FHitResult& Impact);

	/** Effet visuel et sonore de l'impact, chez tout le monde. Points = cibles de la foudre. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastImpact(FVector_NetQuantize Point, const TArray<FVector_NetQuantize>& Points);

	void Exploser(AActor* Autre, const FHitResult& Impact);
	bool PeutBlesser(AActor* Cible) const;
	void Blesser(AActor* Cible, float Quantite, const FHitResult& Impact, const FVector& Direction);
	void Geler(APawn* Cible);

	bool bFini = false;
};
