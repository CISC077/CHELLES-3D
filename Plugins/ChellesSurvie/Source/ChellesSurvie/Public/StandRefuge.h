// CHELLES 3D - Dernier Refuge : un point d'achat (arme, munitions) ou une barriere payante
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StandRefuge.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class ETypeStand : uint8
{
	Arme,
	Munitions,
	Barriere
};

/**
 * Pose par le serveur au debut d'une partie Dernier Refuge, retire a la fin.
 * Un bloc colore avec son prix ecrit au-dessus ; le joueur s'approche et appuie sur E.
 * Barriere : un mur qui bloque une rue tant que personne ne l'a payee.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESSURVIE_API AStandRefuge : public AActor
{
	GENERATED_BODY()

public:
	AStandRefuge();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Serveur : regle le stand juste apres son apparition. */
	void Regler(ETypeStand NouveauType, const FString& NouveauLibelle, int32 NouveauPrix, const FString& Classe);

	/** Texte affiche au joueur qui s'approche ("[E] Fusil - 1200 pts"). */
	FString Invite() const;
	/** Distance (cm) au bord du bloc a laquelle on peut acheter. */
	float Portee() const;

	UPROPERTY(Replicated)
	ETypeStand Type = ETypeStand::Arme;

	UPROPERTY(Replicated)
	FString Libelle;

	UPROPERTY(Replicated)
	int32 Prix = 0;

	/** Barriere payee : elle disparait. */
	UPROPERTY(ReplicatedUsing = OnRep_Ouvert)
	bool bOuvert = false;

	/** Serveur seulement : Blueprint de l'arme vendue. */
	FString ClasseArme;

	/** Serveur : ouvre la barriere pour tout le monde. */
	void Ouvrir();

protected:
	UFUNCTION()
	void OnRep_Ouvert();

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Bloc = nullptr;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* Texte = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* Couleur = nullptr;

private:
	void MettreEnForme();
	bool bEnForme = false;
};
