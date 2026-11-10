// CHELLES 3D - Roue des modes en multijoueur : un relais par joueur, cree par le serveur (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoueRelais.generated.h"

class APawn;
class ARoueModes;

/**
 * En multijoueur, chaque joueur a son relais (proprietaire = son PlayerController).
 * La roue du joueur (sur son PC) demande un mode au serveur par ce relais ; le serveur fait
 * apparaitre le personnage choisi, le donne au joueur, et tout le monde voit son costume.
 * En solo, ce relais n'existe pas : la roue marche comme avant.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESROUE_API ARoueRelais : public AActor
{
	GENERATED_BODY()

public:
	ARoueRelais();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Le joueur demande au serveur de passer au mode Index de la roue. */
	UFUNCTION(Server, Reliable)
	void ServeurChoisir(int32 Index);

	/** Le serveur affiche un message au joueur (sur son ecran seulement). */
	UFUNCTION(Client, Reliable)
	void ClientAvis(const FString& Texte);

	/** Mode joue par ce joueur (index de la roue, -1 = mode de depart). Copie chez tous. */
	UPROPERTY(Replicated)
	int32 Mode = -1;

	/** Personnage a habiller (costume du mode Mode), vu par tous les joueurs. */
	UPROPERTY(Replicated)
	APawn* PionHabille = nullptr;

	/** Soldat de depart cache pendant qu'on joue un autre mode (copie chez tous : pas de collision). */
	UPROPERTY(Replicated)
	APawn* PionCache = nullptr;

	/** Serveur seulement : le personnage donne au joueur au dernier changement. */
	UPROPERTY(Transient)
	APawn* PionActif = nullptr;

	/** Serveur seulement : classe du soldat de depart du joueur (pour y revenir). */
	UPROPERTY(Transient)
	UClass* ClasseSoldat = nullptr;

private:
	ARoueModes* TrouverRoue() const;
};
