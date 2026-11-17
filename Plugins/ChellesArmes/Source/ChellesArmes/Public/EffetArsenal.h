// CHELLES 3D - Arsenal : petit effet visible par tous (coup de sabre, etincelles)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EffetArsenal.generated.h"

namespace ChellesEffetArsenal
{
	enum EGenre : uint8
	{
		CoupSabre = 0,  // le sabre fend l'air (son pour les autres joueurs)
		Etincelles      // le sabre touche quelqu'un
	};
}

UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API AEffetArsenal : public AActor
{
	GENERATED_BODY()

public:
	AEffetArsenal();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Serveur : cree l'effet (il disparait tout seul). */
	static void Creer(UWorld* Monde, uint8 Genre, const FVector& Ou, const FLinearColor& Couleur, APawn* Auteur);

	UPROPERTY(Replicated)
	uint8 Genre = 0;

	UPROPERTY(Replicated)
	FColor Couleur = FColor::White;

protected:
	virtual void BeginPlay() override;
};
