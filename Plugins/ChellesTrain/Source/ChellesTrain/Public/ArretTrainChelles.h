// CHELLES 3D - Arret de train (panneau "TRAIN" + nom + banc).
// Les arrets ecrits dans Plugins\ChellesTrain\Train.txt sont poses TOUT SEULS au lancement du jeu.
// On peut aussi en poser un a la main dans la carte (Place Actors > "Arret de train (Chelles)") :
// le train passera aussi par lui. La fleche bleue doit pointer vers la route.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArretTrainChelles.generated.h"

class UArrowComponent;
class UBillboardComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Arret de train (Chelles)"))
class CHELLESTRAIN_API AArretTrainChelles : public AActor
{
	GENERATED_BODY()

public:
	AArretTrainChelles();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Nom affiche sur le panneau. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de train")
	FString NomArret = TEXT("Arret");

	/** Decocher pour que le train ignore cet arret (pose a la main). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de train")
	bool bActif = true;

	/** Afficher le banc (pack Bus_Station). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de train")
	bool bAfficherBanc = true;

	/** Couleur du panneau. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de train")
	FLinearColor CouleurPanneau = FLinearColor(0.0f, 0.42f, 0.30f);

	/** Pose automatiquement par le train (a partir de Train.txt) : pas un arret "a la main". */
	bool bAutomatique = false;

	/** Change le nom affiche (arrets poses au lancement du jeu). */
	void DefinirNom(const FString& Nom);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	USceneComponent* Racine;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UStaticMeshComponent* Poteau;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UStaticMeshComponent* Panneau;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UStaticMeshComponent* Quai;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UTextRenderComponent* TexteTrain;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UTextRenderComponent* TexteTrainDos;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UTextRenderComponent* TexteNom;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UTextRenderComponent* TexteNomDos;

	UPROPERTY(VisibleAnywhere, Category = "Arret de train")
	UStaticMeshComponent* Banc;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* Icone;

	UPROPERTY()
	UArrowComponent* Fleche;
#endif

	void MettreAJourApparence();
};
