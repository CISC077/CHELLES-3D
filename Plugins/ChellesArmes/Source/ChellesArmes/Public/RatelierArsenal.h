// CHELLES 3D - Ratelier (2026-11-20) : une arme lourde accrochee au mur d'un batiment, dans un simple cadre en bois.
// Pose par le serveur au lancement (au-dessus des socles d'armes de Tools\Bonus), vu par tous.
// Le joueur s'approche et appuie sur E : il prend l'arme (LANCE-FLAMMES ou MITRAILLEUSE LOURDE) avec ses munitions.
// L'arme revient sur le ratelier apres un moment (Rateliers.Attente dans Arsenal.txt).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RatelierArsenal.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UTextRenderComponent;
class USceneComponent;

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API ARatelierArsenal : public AActor
{
	GENERATED_BODY()

public:
	ARatelierArsenal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Arme accrochee (ChellesArsenal::LanceFlammes ou Mitrailleuse). */
	UPROPERTY(ReplicatedUsing = OnRep_Etat)
	uint8 Arme = 0;

	/** L'arme a ete prise : le cadre est vide jusqu'a son retour. */
	UPROPERTY(ReplicatedUsing = OnRep_Etat)
	bool bVide = false;

	/** Serveur : l'arme revient dans Attente secondes. */
	void Vider(float Attente);

	/** Centre du ratelier (pour la distance de prise). */
	FVector Centre() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Etat();

private:
	void Habiller();

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Racine;

	UPROPERTY()
	TArray<UStaticMeshComponent*> Cadre;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* ModeleArme;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* ModeleSecours;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* Etiquette;

	float Retour = 0.f;
	uint8 ArmeHabillee = 255;
};
