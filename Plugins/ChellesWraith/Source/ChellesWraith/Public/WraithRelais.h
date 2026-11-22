// CHELLES 3D - Wraith : un relais par joueur, cree par le serveur (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WraithRelais.generated.h"

class APlayerState;

/**
 * En multijoueur, chaque joueur a son relais (proprietaire = son PlayerController).
 * Il porte le skin choisi avec la touche K : tous les joueurs voient la meme couleur.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESWRAITH_API AWraithRelais : public AActor
{
	GENERATED_BODY()

public:
	AWraithRelais();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Le joueur a appuye sur K : le serveur enregistre son skin. */
	UFUNCTION(Server, Reliable)
	void ServeurSkin(int32 NouveauSkin);

	/** Joueur a qui appartient ce relais (pour retrouver son personnage sur chaque machine). */
	UPROPERTY(Replicated)
	APlayerState* Joueur = nullptr;

	/** Numero du skin choisi (ligne Skin= de Wraith.txt, -1 = pas encore choisi). */
	UPROPERTY(Replicated)
	int32 Skin = -1;
};
