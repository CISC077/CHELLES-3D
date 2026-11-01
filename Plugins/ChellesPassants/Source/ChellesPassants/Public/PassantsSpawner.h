// A poser dans le niveau : fait apparaitre des passants qui se promenent autour de lui.
// Pour tout retirer : supprimer cet acteur du niveau. Rien d'autre n'est modifie.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PassantsSpawner.generated.h"

class APassantCharacter;
class UAnimSequence;
class USphereComponent;
class UBillboardComponent;
class USkeletalMesh;
class UAnimInstance;

/** Un "modele" de passant : un personnage et ses animations. */
USTRUCT()
struct FTypePassant
{
	GENERATED_BODY()

	UPROPERTY() USkeletalMesh* Apparence = nullptr;
	UPROPERTY() UAnimSequence* Marche = nullptr;
	UPROPERTY() UAnimSequence* Repos = nullptr;
};

UCLASS(meta = (DisplayName = "Passants Spawner (Chelles)"))
class CHELLESPASSANTS_API APassantsSpawner : public AActor
{
	GENERATED_BODY()

public:
	APassantsSpawner();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Decocher pour desactiver les passants sans supprimer l'acteur. */
	UPROPERTY(EditAnywhere, Category = "Passants")
	bool bActif = true;

	/** Nombre de passants dans la zone. */
	UPROPERTY(EditAnywhere, Category = "Passants", meta = (ClampMin = "1", ClampMax = "300"))
	int32 NombreDePassants = 40;

	/** Rayon de la zone de promenade en cm (20000 = 200 metres). La sphere verte la montre dans l'editeur. */
	UPROPERTY(EditAnywhere, Category = "Passants", meta = (ClampMin = "500"))
	float RayonDeZone = 20000.f;

	/** Cherche tout seul les personnages (habilles) et leurs animations dans le dossier Content. */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	bool bChercherPersonnagesAutomatiquement = true;

	/** Dossier ou chercher les personnages ("/Game" = tout le dossier Content). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages", meta = (EditCondition = "bChercherPersonnagesAutomatiquement"))
	FString DossierDeRecherche = TEXT("/Game");

	/** Garder aussi le mannequin gris d'Unreal (sans vetements) ? Par defaut il est ignore s'il y a d'autres personnages. */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	bool bGarderMannequinSansVetements = false;

	/** Personnages a utiliser (Skeletal Mesh). Optionnel : s'il est rempli, la recherche automatique est ignoree. */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	TArray<USkeletalMesh*> Personnages;

	/** Animation de marche pour les personnages ci-dessus (optionnel). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	UAnimSequence* AnimationMarche = nullptr;

	/** Animation d'attente pour les personnages ci-dessus (optionnel). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	UAnimSequence* AnimationRepos = nullptr;

	/** Ou bien un Animation Blueprint (remplace les deux animations ci-dessus). */
	UPROPERTY(EditAnywhere, Category = "Passants|Personnages")
	TSubclassOf<UAnimInstance> AnimationBlueprint;

	/** Vitesse de marche minimale (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float VitesseMin = 100.f;

	/** Vitesse de marche maximale (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float VitesseMax = 160.f;

	/** Pause minimale entre deux trajets (secondes). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float PauseMin = 1.f;

	/** Pause maximale entre deux trajets (secondes). */
	UPROPERTY(EditAnywhere, Category = "Passants|Comportement")
	float PauseMax = 6.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Passants")
	USphereComponent* Zone;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* Icone;
#endif

	UPROPERTY(Transient)
	TArray<FTypePassant> Types;

	UPROPERTY(Transient)
	TArray<APassantCharacter*> Passants;

	FTimerHandle MinuteurRemplacement;

	void PreparerTypes();
	void ChercherPersonnagesDansContent();
	bool TrouverPointDApparition(FVector& OutPoint) const;
	APassantCharacter* FaireApparaitreUnPassant();
	void RemplacerLesDisparus();
	void Annoncer(const FString& Texte, const FColor& Couleur) const;
};
