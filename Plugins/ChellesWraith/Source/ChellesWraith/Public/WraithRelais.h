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

	/** Tenue choisie avec K pour le heros de la roue (Spider-Male, PANTERA...). */
	UFUNCTION(Server, Reliable)
	void ServeurTenue(int32 NouvelleTenue);

	/** Joueur a qui appartient ce relais (pour retrouver son personnage sur chaque machine). */
	UPROPERTY(Replicated)
	APlayerState* Joueur = nullptr;

	/** Numero du skin choisi (ligne Skin= de Wraith.txt, -1 = pas encore choisi). */
	UPROPERTY(Replicated)
	int32 Skin = -1;

	/** Numero de la tenue du heros joue (ligne Tenue= de Wraith.txt, -1 = tenue d'origine). */
	UPROPERTY(Replicated)
	int32 Tenue = -1;

	/** LENA, SOFIA, LUCIA... : le joueur danse (touche des danses) ; les autres joueurs la voient danser. */
	UFUNCTION(Server, Reliable)
	void ServeurDanse(bool bDanser);

	UPROPERTY(Replicated)
	bool bDanse = false;

	/** Numero de la danse choisie (chaque appui sur J = la suivante) : les autres joueurs voient la meme. */
	UFUNCTION(Server, Reliable)
	void ServeurNumeroDanse(int32 Numero);
	UPROPERTY(Replicated)
	int32 NumeroDanse = 0;
};
