#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChellesVegetationActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Un lot de plantes (ou de pierres, barrieres...) qui utilisent toutes le meme modele 3D.
 * Toutes les copies sont dessinees en une seule fois par la carte graphique (HISM) :
 * 3 000 arbres coutent a peu pres comme quelques dizaines d'objets normaux.
 * Les copies lointaines disparaissent (distance de coupure) et les modeles utilisent leurs LOD.
 * Pose par Tools/VegetationEau/vegetation_eau.py (ne pas editer a la main).
 */
UCLASS()
class CHELLESVEGETATION_API AChellesVegetationActor : public AActor
{
	GENERATED_BODY()

public:
	AChellesVegetationActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Chelles")
	UHierarchicalInstancedStaticMeshComponent* Instances;

	/** Categorie (arbre, buisson, haie, roseau, fleur, pierre, barriere) : pour le journal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chelles")
	FString Categorie;

	/**
	 * Remplace toutes les copies par celles de Transforms (coordonnees du monde, l'acteur reste a l'origine).
	 * CullDebutCm / CullFinCm : les copies s'effacent en fondu entre ces deux distances (0 = jamais).
	 * Renvoie le nombre de copies posees.
	 */
	UFUNCTION(BlueprintCallable, Category = "Chelles")
	int32 Remplir(UStaticMesh* Modele, const TArray<FTransform>& Transforms, float CullDebutCm, float CullFinCm,
	              bool bOmbres, bool bCollision);

	UFUNCTION(BlueprintCallable, Category = "Chelles")
	int32 NombreDeCopies() const;
};
