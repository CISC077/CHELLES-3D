// CHELLES 3D - Invasion zombies : sirene d'alerte fabriquee par le code (aucun fichier son a importer)
#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "SireneChelles.generated.h"

/**
 * Sirene d'alerte (type sirene de defense) : le son monte et descend lentement.
 * Le son est calcule en direct, il n'y a donc aucun .uasset a creer.
 */
UCLASS()
class CHELLESZOMBIES_API USireneChelles : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USireneChelles(const FObjectInitializer& ObjectInitializer);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	/** Duree d'un cycle montee + descente, en secondes. */
	float DureeCycle = 6.f;

private:
	// Utilises uniquement par le fil audio
	float Phase = 0.f;
	float Phase2 = 0.f;
	int64 Echantillon = 0;
};
