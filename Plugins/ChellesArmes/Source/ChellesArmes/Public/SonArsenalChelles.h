// CHELLES 3D - Arsenal : sons fabriques par le code (aucun fichier son a importer)
#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "SonArsenalChelles.generated.h"

class UAudioComponent;
class USceneComponent;

namespace ChellesArsenalSons
{
	enum EGenre : uint8
	{
		Piou = 0,       // tir du blaster (son de science-fiction qui descend)
		Impact,         // le trait du blaster touche
		Lancement,      // depart de la fusee
		Reacteur,       // fusee en vol (boucle)
		Boum,           // explosion (si le son du StarterContent est absent)
		Allumage,       // le sabre s'allume
		Bourdonnement,  // sabre allume (boucle)
		CoupSabre,      // le sabre fend l'air
		Bip,            // grenade qui attire les zombies
		Lancer          // grenade lancee
	};
}

/**
 * Un son calcule en direct (comme la sirene des zombies). Un objet par son joue.
 * Les sons "boucle" (Reacteur, Bourdonnement) durent jusqu'a ce qu'on les arrete.
 */
UCLASS()
class CHELLESARMES_API USonArsenalChelles : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	USonArsenalChelles(const FObjectInitializer& ObjectInitializer);

	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;

	/** Joue un son a un endroit (ou accroche a un composant). Renvoie le composant audio (peut etre nul). */
	static UAudioComponent* Jouer(UObject* Contexte, uint8 Genre, const FVector& Ou, float Volume = 1.f, float Hauteur = 1.f,
		USceneComponent* AccrocherA = nullptr);

	/** Duree du son en secondes (0 = boucle). */
	static float DureeDe(uint8 Genre);

	uint8 Genre = 0;
	float Hauteur = 1.f;

private:
	float Bruit();

	// Utilises uniquement par le fil audio
	int64 Echantillon = 0;
	float Phase = 0.f;
	float Phase2 = 0.f;
	float Phase3 = 0.f;
	float Filtre = 0.f;
	float Filtre2 = 0.f;
	uint32 Graine = 22222u;
};
