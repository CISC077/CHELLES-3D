// CHELLES 3D - Sauvegarde de l'XP du joueur (fichier Saved\SaveGames\ChellesXP.sav sur son PC)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "XPSauvegarde.generated.h"

UCLASS()
class CHELLESPOUVOIRS_API UXPSauvegarde : public USaveGame
{
	GENERATED_BODY()

public:
	/** XP totale gagnee. */
	UPROPERTY()
	int32 XP = 0;

	/** Temps de jeu total en secondes (pour le bonus de chaque heure). */
	UPROPERTY()
	double SecondesJouees = 0.0;

	/** Pouvoir choisi la derniere fois. */
	UPROPERTY()
	int32 Selection = 0;
};
