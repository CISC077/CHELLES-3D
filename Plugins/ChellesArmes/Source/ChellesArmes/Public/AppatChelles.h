// CHELLES 3D - Arsenal : appat de la grenade a zombies (serveur seulement)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AppatChelles.generated.h"

/**
 * Petit "pion" invisible accroche a la grenade pendant qu'elle bipe. Il porte le tag ChellesAppat :
 * le plugin ChellesZombies fait courir vers lui les zombies a moins de RayonAppat cm, au lieu du joueur.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AAppatChelles : public APawn
{
	GENERATED_BODY()

public:
	AAppatChelles();

	/** Distance (cm) a laquelle les zombies sont attires. Lu par ChellesZombies par son nom. */
	UPROPERTY(VisibleAnywhere, Category = "Appat")
	float RayonAppat = 3000.f;
};
