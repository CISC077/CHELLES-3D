// CHELLES 3D - Butin : un relais par joueur, cree par le serveur (existe aussi en solo). UE 4.27.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "RelaisButin.generated.h"

class ACoffreButin;
class AButinChelles;
class APingButin;
class APawn;

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESBUTIN_API ARelaisButin : public AActor
{
	GENERATED_BODY()

public:
	ARelaisButin();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void ServeurOuvrir(ACoffreButin* Coffre);

	UFUNCTION(Server, Reliable)
	void ServeurPing(FVector_NetQuantize Point, uint8 Genre);

	UFUNCTION(Server, Reliable)
	void ServeurFrappe(FVector_NetQuantize Point);

	/** Grand message au centre de l'ecran de ce joueur (Couleur : 0 blanc, 1 bleu, 2 violet, 3 or, 4 rouge). */
	UFUNCTION(Client, Reliable)
	void ClientAnnonce(const FString& Texte, uint8 Couleur, bool bSon);

	/** XP gagnee : donnee sur la machine du joueur (plugin ChellesPouvoirs, sauvegardee chez lui). */
	UFUNCTION(Client, Reliable)
	void ClientXP(int32 Quantite, const FString& Raison);

	/** Eliminations a la suite sans mourir. */
	UPROPERTY(Replicated)
	int32 Serie = 0;

	UPROPERTY(Replicated)
	bool bRadar = false;

	UPROPERTY(Replicated)
	bool bFrappePrete = false;

	// ---- Serveur seulement
	TWeakObjectPtr<APawn> DernierPion;
	TWeakObjectPtr<APingButin> DernierPing;
	float FinRadar = 0.f;
	float ProchainPing = 0.f;

private:
	AButinChelles* Gestionnaire() const;
};
