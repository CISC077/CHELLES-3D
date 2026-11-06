// Un passant : personnage qui se promene seul dans une zone de la ville.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "PassantCharacter.generated.h"

class UAnimSequence;
class UStaticMeshComponent;
class UMeshComponent;
class USkeletalMesh;
class UAnimInstance;

UCLASS(Blueprintable)
class CHELLESPASSANTS_API APassantCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APassantCharacter();

	/** Donne au passant son apparence, ses animations et sa zone de promenade. */
	void Configurer(USkeletalMesh* Apparence, TSubclassOf<UAnimInstance> AnimBlueprint,
		UAnimSequence* Marche, UAnimSequence* Repos,
		float Vitesse, const FVector& Centre, float Rayon, float PauseMin, float PauseMax,
		float DistanceAffichage = 0.f);

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Le sol sous le point (X, Y) est-il un trottoir ? On regarde autour de la hauteur HauteurPieds.
	 * Un trottoir = un acteur ou un Static Mesh dont le nom contient MotCle (ex. "Trottoir").
	 * OutSol recoit le point du sol touche.
	 */
	static bool EstUnTrottoir(const UWorld* Monde, const FVector& Point, float HauteurPieds,
		const FString& MotCle, const AActor* AIgnorer, FVector* OutSol = nullptr);

	/** Hauteur des pieds (bas de la capsule). */
	float HauteurDesPieds() const;

	/** Centre de la zone ou le passant a le droit de se promener. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passant")
	FVector CentreZone = FVector::ZeroVector;

	/** Rayon de la zone (en cm). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passant")
	float RayonZone = 10000.f;

	/** Pause minimale entre deux trajets (secondes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	float PauseMin = 2.f;

	/** Pause maximale entre deux trajets (secondes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	float PauseMax = 8.f;

	/** Ne marche que sur les trottoirs (jamais sur les routes, les toits, les jumpers...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	bool bTrottoirsSeulement = false;

	/** Mot present dans le nom des trottoirs (acteur ou Static Mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passant")
	FString MotCleTrottoir = TEXT("Trottoir");

protected:
	/** Silhouette simple affichee seulement si aucun personnage n'a ete trouve. */
	UPROPERTY(VisibleAnywhere, Category = "Passant")
	UStaticMeshComponent* SilhouetteCorps;

	UPROPERTY(VisibleAnywhere, Category = "Passant")
	UStaticMeshComponent* SilhouetteTete;

	UPROPERTY()
	UAnimSequence* AnimMarche = nullptr;

	UPROPERTY()
	UAnimSequence* AnimRepos = nullptr;

	/** Vitesse a laquelle l'animation de marche est jouee a vitesse normale (cm/s). */
	float VitesseReferenceMarche = 150.f;

	/** 0 = aucune anim jouee, 1 = repos, 2 = marche */
	int32 EtatAnim = 0;

	void ColorerVetements(UMeshComponent* Composant);
};
