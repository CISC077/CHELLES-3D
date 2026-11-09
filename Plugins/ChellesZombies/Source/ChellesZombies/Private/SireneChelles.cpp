// CHELLES 3D - Invasion zombies : sirene d'alerte fabriquee par le code
#include "SireneChelles.h"

namespace
{
	const int32 FrequenceSirene = 44100;
}

USireneChelles::USireneChelles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(FrequenceSirene);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Default;
}

int32 USireneChelles::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Sortie = reinterpret_cast<int16*>(OutAudio.GetData());

	const float Cycle = FMath::Max(1.f, DureeCycle);
	const float DeuxPi = 2.f * PI;
	for (int32 i = 0; i < NumSamples; ++i)
	{
		// Temps en secondes (on repart de 0 toutes les heures pour garder la precision)
		const float T = (float)(Echantillon % ((int64)FrequenceSirene * 3600)) / (float)FrequenceSirene;
		++Echantillon;

		// 0 -> 1 -> 0 sur un cycle (montee lente, descente lente)
		const float Avancement = FMath::Fmod(T, Cycle) / Cycle;
		const float Courbe = 0.5f - 0.5f * FMath::Cos(DeuxPi * Avancement);
		const float Hauteur = 260.f + 560.f * Courbe; // de 260 Hz a 820 Hz

		Phase += DeuxPi * Hauteur / (float)FrequenceSirene;
		Phase2 += DeuxPi * Hauteur * 1.5f / (float)FrequenceSirene; // quinte : son plus "metallique"
		if (Phase > DeuxPi) Phase -= DeuxPi;
		if (Phase2 > DeuxPi) Phase2 -= DeuxPi;

		// Onde un peu ecrasee (entre sinus et carre) comme une vraie sirene
		const float Ecrase = FMath::Clamp(FMath::Sin(Phase) * 1.8f, -1.f, 1.f);
		float Valeur = 0.62f * Ecrase + 0.22f * FMath::Sin(Phase2);

		// Fondu d'entree d'une demi-seconde
		const float Entree = FMath::Clamp(T / 0.5f, 0.f, 1.f);
		Valeur *= 0.55f * Entree;

		Sortie[i] = (int16)FMath::Clamp((int32)(Valeur * 32767.f), -32767, 32767);
	}
	return NumSamples;
}
