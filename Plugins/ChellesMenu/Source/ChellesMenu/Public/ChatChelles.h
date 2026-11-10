// CHELLES 3D - Chat entre joueurs (multijoueur LAN)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChatChelles.generated.h"

class APlayerController;

/**
 * Ajoute par le serveur sur le PlayerController de chaque joueur.
 * Le joueur envoie son message au serveur, qui le renvoie a tous les joueurs.
 */
UCLASS(ClassGroup = (Chelles))
class CHELLESMENU_API UChatChelles : public UActorComponent
{
	GENERATED_BODY()

public:
	UChatChelles();

	/** Envoie un message a tous les joueurs de la partie (appele sur le PC du joueur). */
	void Envoyer(const FString& Texte);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServeurEnvoyer(const FString& Texte);

	UFUNCTION(Client, Reliable)
	void ClientRecevoir(const FString& Auteur, const FString& Texte);

	/** Cote serveur : donne un chat a chaque joueur connecte qui n'en a pas encore. */
	static void EquiperJoueurs(UWorld* Monde);

	static UChatChelles* DuJoueur(APlayerController* PC);
};
