// CHELLES 3D - Voitures de police en multijoueur : un relais par joueur, cree par le serveur (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RelaisPolice.generated.h"

class APawn;
class APoliceChelles;

/**
 * En multijoueur, chaque joueur a son relais (proprietaire = son PlayerController). Le joueur demande au serveur
 * de poser / retirer une voiture (administrateur), de monter / descendre, ou d'allumer la sirene.
 * En solo ce relais n'existe pas.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESPOLICE_API ARelaisPolice : public AActor
{
	GENERATED_BODY()

public:
	ARelaisPolice();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void ServeurPoser(FVector Lieu, float Yaw);

	UFUNCTION(Server, Reliable)
	void ServeurRetirer(FVector Point);

	UFUNCTION(Server, Reliable)
	void ServeurMonter(APawn* VoitureVisee);

	UFUNCTION(Server, Reliable)
	void ServeurDescendre();

	UFUNCTION(Server, Reliable)
	void ServeurSirene(bool bActive);

	/** Message affiche sur l'ecran de ce joueur seulement. */
	UFUNCTION(Client, Reliable)
	void ClientAvis(const FString& Texte, float Duree);

	/** Voiture de police conduite par ce joueur en ce moment (montee avec E ou par la roue TAB). Copie chez tous. */
	UPROPERTY(Replicated)
	APawn* Voiture = nullptr;

	/** Soldat cache pendant qu'on conduit une voiture montee avec E (copie chez tous : cache, sans collision). */
	UPROPERTY(Replicated)
	APawn* PietonCache = nullptr;

	/** Sirene et gyrophares voulus par ce joueur (touche L). */
	UPROPERTY(Replicated)
	bool bSirene = true;

	/** Ce joueur est l'administrateur (peut poser / retirer des voitures). */
	UPROPERTY(Replicated)
	bool bAdmin = false;

	/** Serveur seulement : voiture dans laquelle ce joueur est monte avec E. */
	UPROPERTY(Transient)
	APawn* VoitureMontee = nullptr;

private:
	APoliceChelles* Gestionnaire() const;
};
