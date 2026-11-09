// CHELLES 3D - XP et pouvoirs : un relais par joueur, cree par le serveur (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PouvoirsRelais.generated.h"

class APawn;
class APouvoirsChelles;

/**
 * Chaque joueur a son relais (proprietaire = son PlayerController), en solo comme en multi.
 * Le joueur annonce son XP au serveur et lui demande d'utiliser un pouvoir.
 * Le serveur verifie le niveau et la recharge, puis remplit les variables copiees chez tous :
 * ainsi tout le monde voit la meme boule de feu, le meme joueur invisible ou gele.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESPOUVOIRS_API APouvoirsRelais : public AActor
{
	GENERATED_BODY()

public:
	APouvoirsRelais();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Le joueur donne son XP (sauvegardee sur son PC) au serveur. */
	UFUNCTION(Server, Reliable)
	void ServeurXP(int32 NouvelleXP);

	/** Le joueur demande d'utiliser un pouvoir, en visant depuis Origine dans la direction Visee. */
	UFUNCTION(Server, Reliable)
	void ServeurPouvoir(uint8 Quel, FVector Origine, FRotator Visee);

	/** Le serveur affiche un message au joueur (sur son ecran seulement). */
	UFUNCTION(Client, Reliable)
	void ClientAvis(const FString& Texte);

	/** XP du joueur (connue de tous, pour afficher son niveau). */
	UPROPERTY(Replicated)
	int32 XP = 0;

	/** Personnage joue en ce moment par ce joueur. */
	UPROPERTY(Replicated)
	APawn* Pion = nullptr;

	/** Heure (horloge du serveur) de fin de l'effet de chaque pouvoir. */
	UPROPERTY(Replicated)
	TArray<float> FinEffet;

	/** Heure (horloge du serveur) a partir de laquelle chaque pouvoir est de nouveau pret. */
	UPROPERTY(Replicated)
	TArray<float> FinRecharge;

	/** Joueur ou passant gele par ce joueur (pendant l'effet GEL). */
	UPROPERTY(Replicated)
	AActor* CibleGel = nullptr;

	/** Apparence choisie avec CAMELEON (chemin d'un SkeletalMesh), vide = apparence d'origine. */
	UPROPERTY(Replicated)
	FString Deguisement;

	/** Serveur seulement : numero de l'apparence CAMELEON en cours (0 = origine). */
	int32 IndexDeguisement = 0;

	/** Heure commune a toutes les machines (horloge du serveur). */
	float Maintenant() const;
	bool EffetActif(int32 Quel) const;
	float RestantEffet(int32 Quel) const;
	float RestantRecharge(int32 Quel) const;

private:
	APouvoirsChelles* TrouverGestionnaire() const;
};
