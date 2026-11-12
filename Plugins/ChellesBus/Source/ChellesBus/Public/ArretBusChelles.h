// CHELLES 3D - Arret de bus a poser dans la carte (Blueprint enfant : /Game/Chelles/Bus/BP_Arret_Bus).
// Pendant l'invasion zombie, le bus BP_Bus_sity fait le tour de TOUS les arrets poses dans la carte,
// en suivant les routes de la circulation. Il s'arrete devant chaque arret.
// Posez l'arret sur le trottoir, au bord de la route (le bus roule a droite : ses portes sont du cote du trottoir).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArretBusChelles.generated.h"

class UArrowComponent;
class UBillboardComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "Arret de bus (Chelles)"))
class CHELLESBUS_API AArretBusChelles : public AActor
{
	GENERATED_BODY()

public:
	AArretBusChelles();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Nom affiche sur le panneau. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de bus")
	FString NomArret = TEXT("Arret");

	/**
	 * Ordre de passage du bus (1, 2, 3...). 0 = automatique : le bus choisit lui-meme
	 * le trajet le plus court entre les arrets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de bus", meta = (ClampMin = "0"))
	int32 Ordre = 0;

	/** Decocher pour que le bus ignore cet arret. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de bus")
	bool bActif = true;

	/** Afficher le banc de l'abri (pack Bus_Station). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de bus")
	bool bAfficherBanc = true;

	/** Couleur du panneau. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arret de bus")
	FLinearColor CouleurPanneau = FLinearColor(0.02f, 0.18f, 0.75f);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	USceneComponent* Racine;

	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	UStaticMeshComponent* Poteau;

	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	UStaticMeshComponent* Panneau;

	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	UTextRenderComponent* TexteBus;

	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	UTextRenderComponent* TexteBusDos;

	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	UTextRenderComponent* TexteNom;

	UPROPERTY(VisibleAnywhere, Category = "Arret de bus")
	UStaticMeshComponent* Banc;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* Icone;

	UPROPERTY()
	UArrowComponent* Fleche;
#endif
};
