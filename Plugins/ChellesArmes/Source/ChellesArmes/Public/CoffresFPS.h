// CHELLES 3D - Coffres d'armes du kit FPS_Game (25/11/2026, agent Armements) - UE 4.27
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoffresFPS.generated.h"

class APawn;
class APlayerController;

/**
 * Filet de securite pour les coffres d'armes du kit FPS_Game (BP_Chest, Replicated_Chest, BP_Loot_Chest).
 *
 * Normalement le soldat ouvre le coffre avec E (action "Use" -> interface BPI_Interact -> evenement
 * BPI_Interaction du coffre). Quand ce chemin ne marche plus (perso du lobby, collision, touche prise
 * par un autre menu...), cet acteur appelle lui-meme l'evenement BPI_Interaction du coffre le plus proche
 * quand on appuie sur E a moins de 3 m. Le DoOnce du coffre empeche toute double ouverture.
 * Aucun Blueprint n'est modifie. Journal : lignes [CoffresFPS]. Console : Chelles.CoffresFPS [Liste|Ouvrir].
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API ACoffresFPS : public AActor
{
	GENERATED_BODY()

public:
	ACoffresFPS();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Liste (journal de tous les coffres) | Ouvrir (ouvre le plus proche, meme loin) */
	void Commande(const TArray<FString>& Arguments);

private:
	void Recenser();
	APlayerController* GetPC() const;
	AActor* PlusProche(const APawn* Pion, float DistanceMax) const;
	bool Ouvrir(AActor* Coffre, APawn* Pion, const TCHAR* Pourquoi);

	TArray<TWeakObjectPtr<AActor>> Coffres;
	TSet<TWeakObjectPtr<AActor>> Ouverts;
	float ProchainRecensement = 0.f;
	bool bJournalFait = false;
};
