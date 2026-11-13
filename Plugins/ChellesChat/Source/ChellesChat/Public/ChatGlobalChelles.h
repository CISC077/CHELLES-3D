// CHELLES 3D - Chat global en jeu : fenetre a gauche de l'ecran, touche B
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChatGlobalChelles.generated.h"

class APlayerController;
class SChatGlobal;

/**
 * Un seul par niveau, ajoute tout seul au lancement du jeu (Play) sur chaque PC qui affiche le jeu.
 * Il ne touche a rien d'autre : il affiche la fenetre du chat global et ecoute la touche B.
 * La connexion elle-meme (FConnexionChatGlobal) est partagee par tout le jeu.
 * Console (touche au-dessus de Tab) : Chelles.ChatGlobal pour voir l'etat, Chelles.ChatGlobalPseudo Nom pour changer de pseudo.
 */
UCLASS(NotPlaceable, Transient, meta = (DisplayName = "Chat global Chelles"))
class CHELLESCHAT_API AChatGlobalChelles : public AActor
{
	GENERATED_BODY()

public:
	AChatGlobalChelles();

	APlayerController* GetPC() const;

	/** Touche B : ouvre la fenetre et met le curseur dans la zone de saisie. */
	void OuvrirFenetre();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void CreerFenetre();
	void RetirerFenetre();
	void BrancherTouche();

	TSharedPtr<SChatGlobal> Fenetre;
	TWeakObjectPtr<APlayerController> PCBranche;
	bool bToucheLiee = false;
};
