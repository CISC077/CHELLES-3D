// CHELLES 3D - Cadeaux de Noel ramasses par le joueur (fichier Saved\SaveGames\ChellesNoel.sav sur son PC)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CadeauxSauvegarde.generated.h"

UCLASS()
class CHELLESNOEL_API UCadeauxSauvegarde : public USaveGame
{
	GENERATED_BODY()

public:
	/** Jour des cadeaux deja ramasses, "AAAA-MM-JJ" (les cadeaux changent de place chaque jour). */
	UPROPERTY()
	FString Jour;

	/** Numeros des cadeaux ramasses ce jour-la (ils ne reapparaissent pas si on relance le jeu). */
	UPROPERTY()
	TArray<int32> Ramasses;

	/** Total de cadeaux ramasses depuis toujours. */
	UPROPERTY()
	int32 Total = 0;
};
