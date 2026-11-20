// CHELLES 3D - Butin : un coffre lumineux pose au sol (facon Fortnite / Apex), vu par tous. UE 4.27.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoffreButin.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class USceneComponent;

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESBUTIN_API ACoffreButin : public AActor
{
	GENERATED_BODY()

public:
	ACoffreButin();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** ChellesButin::Commun / Rare / Legendaire */
	UPROPERTY(ReplicatedUsing = OnRep_Etat)
	uint8 Rarete = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Etat)
	bool bOuvert = false;

	/** Serveur : ouvre le coffre (il disparait quelques secondes apres). */
	void Ouvrir();

	FVector Centre() const;

	/** Serveur : secondes passees loin de tous les joueurs. */
	float TempsLoin = 0.f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Etat();

private:
	void Habiller();
	void Colorer(UStaticMeshComponent* C, const FLinearColor& Couleur);

	UPROPERTY(VisibleAnywhere)
	USceneComponent* Racine;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Modele;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Colonne;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* Lumiere;

	float Temps = 0.f;
	bool bHabilleOuvert = false;
	bool bHabille = false;
	bool bSonJoue = false;
};
