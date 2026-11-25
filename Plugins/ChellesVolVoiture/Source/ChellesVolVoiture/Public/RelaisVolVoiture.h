// CHELLES 3D - Vol de voiture : un relais par joueur, cree par le serveur (UE 4.27).
// Il existe aussi en solo (les demandes "Serveur" s'executent alors tout de suite sur la machine).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RelaisVolVoiture.generated.h"

class APawn;
class AVolVoitureChelles;

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESVOLVOITURE_API ARelaisVolVoiture : public AActor
{
	GENERATED_BODY()

public:
	ARelaisVolVoiture();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Voler une voiture de la circulation (elle est locale a chaque machine : on envoie sa place et son modele). */
	UFUNCTION(Server, Reliable)
	void ServeurVolerCirculation(FVector Lieu, float Yaw, const FString& Modele);

	/** Monter dans une voiture garee ou deja volee (copiee par le reseau). */
	UFUNCTION(Server, Reliable)
	void ServeurMonter(APawn* VoitureVisee);

	UFUNCTION(Server, Reliable)
	void ServeurDescendre();

	/** 25/11 : action du pack DV_Car_Horo (phares, saut americain, reinitialiser, clignotants, warnings) sur la voiture conduite. */
	UFUNCTION(Server, Reliable)
	void ServeurActionVoiture(uint8 Action);

	/** Rejoue l'action chez tout le monde (serveur compris) pour que tous voient les phares, le saut... */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastActionVoiture(APawn* VoitureVisee, uint8 Action);

	/** Message affiche sur l'ecran de ce joueur seulement. */
	UFUNCTION(Client, Reliable)
	void ClientAvis(const FString& Texte, float Duree);

	/** Niveau RECHERCHE (0 a 5 etoiles). */
	UPROPERTY(Replicated)
	int32 Etoiles = 0;

	/** Voiture volee conduite en ce moment par ce joueur. */
	UPROPERTY(Replicated)
	APawn* Voiture = nullptr;

	/** Personnage a pied cache pendant qu'on conduit (cache et sans collision chez tous). */
	UPROPERTY(Replicated)
	APawn* PietonCache = nullptr;

	// ---- Serveur seulement
	float TempsSansPolice = 0.f;   // s passees loin de toute voiture de police (les etoiles baissent)
	float TempsCoince = 0.f;       // s passees arrete tout pres de la police (ARRETE !)
	float ProchainRenfort = 0.f;

private:
	AVolVoitureChelles* Gestionnaire() const;
};
