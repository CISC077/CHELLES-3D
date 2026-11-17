// CHELLES 3D - Arsenal : sons fabriques par le code
#include "SonArsenalChelles.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"
#include "UObject/Package.h"

using namespace ChellesArsenalSons;

namespace
{
	const int32 Frequence = 44100;

	/** Le son baisse avec la distance (sinon on l'entendrait partout dans le quartier). */
	USoundAttenuation* Attenuation()
	{
		static TWeakObjectPtr<USoundAttenuation> Memo;
		if (!Memo.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AttenuationArsenalChelles"));
			A->AddToRoot();
			A->Attenuation.bAttenuate = true;
			A->Attenuation.bSpatialize = true;
			A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			A->Attenuation.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
			A->Attenuation.FalloffDistance = 4500.f;
			Memo = A;
		}
		return Memo.Get();
	}

	float Enveloppe(float T, float Attaque, float Chute)
	{
		return FMath::Clamp(T / FMath::Max(Attaque, 0.0001f), 0.f, 1.f) * FMath::Exp(-T * Chute);
	}
}

USonArsenalChelles::USonArsenalChelles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetSampleRate(Frequence);
	NumChannels = 1;
	Duration = INDEFINITELY_LOOPING_DURATION;
	bLooping = false;
	SoundGroup = SOUNDGROUP_Default;
}

float USonArsenalChelles::DureeDe(uint8 G)
{
	switch (G)
	{
	case Piou:      return 0.38f;
	case Impact:    return 0.2f;
	case Lancement: return 0.9f;
	case Boum:      return 1.8f;
	case Allumage:  return 0.7f;
	case CoupSabre: return 0.4f;
	case Bip:       return 0.14f;
	case Lancer:    return 0.3f;
	default:        return 0.f; // boucle
	}
}

float USonArsenalChelles::Bruit()
{
	Graine = Graine * 1664525u + 1013904223u;
	return ((Graine >> 9) & 0x7FFF) / 16383.5f - 1.f;
}

int32 USonArsenalChelles::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Sortie = reinterpret_cast<int16*>(OutAudio.GetData());
	const float DeuxPi = 2.f * PI;
	const float Duree = DureeDe(Genre);
	const float H = FMath::Clamp(Hauteur, 0.25f, 4.f);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float T = (float)(Echantillon % ((int64)Frequence * 3600)) / (float)Frequence;
		++Echantillon;
		if (Duree > 0.f && T > Duree)
		{
			continue; // fini : silence (le composant est arrete juste apres)
		}
		float V = 0.f;
		switch (Genre)
		{
		case Piou:
		{
			// Le "piou" : une note qui tombe tres vite de l'aigu vers le grave, avec un ressort metallique
			const float F = H * (240.f + 1900.f * FMath::Exp(-T * 13.f));
			Phase += DeuxPi * F / Frequence;
			Phase2 += DeuxPi * F * 1.505f / Frequence;
			Phase3 += DeuxPi * (F * 0.5f + 35.f * FMath::Sin(T * 90.f)) / Frequence;
			const float Carre = FMath::Clamp(FMath::Sin(Phase) * 3.f, -1.f, 1.f);
			V = (0.45f * Carre + 0.3f * FMath::Sin(Phase2) + 0.25f * FMath::Sin(Phase3)) * Enveloppe(T, 0.003f, 8.f);
			break;
		}
		case Impact:
		{
			Phase += DeuxPi * 2200.f * H / Frequence;
			V = (0.6f * Bruit() * FMath::Exp(-T * 28.f) + 0.4f * FMath::Sin(Phase) * FMath::Exp(-T * 40.f));
			break;
		}
		case Lancement:
		{
			// Souffle puissant qui s'eteint
			Filtre += 0.12f * (Bruit() - Filtre);
			Phase += DeuxPi * (70.f + 40.f * FMath::Exp(-T * 6.f)) / Frequence;
			V = (0.9f * Filtre * 2.2f + 0.35f * FMath::Sin(Phase)) * Enveloppe(T, 0.01f, 3.2f);
			break;
		}
		case Reacteur:
		{
			Filtre += 0.08f * (Bruit() - Filtre);
			Phase += DeuxPi * 55.f / Frequence;
			const float Vibre = 0.85f + 0.15f * FMath::Sin(T * DeuxPi * 9.f);
			V = (1.8f * Filtre + 0.2f * FMath::Sin(Phase)) * Vibre * 0.6f * FMath::Clamp(T / 0.2f, 0.f, 1.f);
			break;
		}
		case Boum:
		{
			// Craquement puis grondement grave
			const float Coupure = 0.02f + 0.3f * FMath::Exp(-T * 6.f);
			Filtre += Coupure * (Bruit() - Filtre);
			Filtre2 += 0.02f * (Filtre - Filtre2);
			Phase += DeuxPi * (38.f + 60.f * FMath::Exp(-T * 5.f)) / Frequence;
			V = (1.6f * Filtre + 2.5f * Filtre2 + 0.6f * FMath::Sin(Phase)) * Enveloppe(T, 0.002f, 2.4f);
			break;
		}
		case Allumage:
		{
			// Le bourdonnement monte en puissance et en hauteur
			const float F = H * (55.f + 50.f * FMath::Clamp(T / 0.45f, 0.f, 1.f));
			Phase += DeuxPi * F / Frequence;
			Phase2 += DeuxPi * F * 1.01f / Frequence;
			Filtre += 0.25f * (Bruit() - Filtre);
			const float Scie = 2.f * (FMath::Fmod(Phase, DeuxPi) / DeuxPi) - 1.f;
			V = (0.45f * Scie + 0.35f * FMath::Sin(Phase2) + 0.25f * Filtre * FMath::Exp(-T * 6.f)) * FMath::Clamp(T / 0.08f, 0.f, 1.f)
				* (T > 0.55f ? FMath::Clamp((0.7f - T) / 0.15f, 0.f, 1.f) : 1.f);
			break;
		}
		case Bourdonnement:
		{
			// Le ronronnement du sabre (deux notes graves tres proches qui battent)
			Phase += DeuxPi * 90.f * H / Frequence;
			Phase2 += DeuxPi * 91.3f * H / Frequence;
			Phase3 += DeuxPi * 180.f * H / Frequence;
			const float Scie = 2.f * (FMath::Fmod(Phase, DeuxPi) / DeuxPi) - 1.f;
			Filtre += 0.18f * (Scie - Filtre);
			V = (0.5f * Filtre + 0.3f * FMath::Sin(Phase2) + 0.1f * FMath::Sin(Phase3)) * 0.5f * FMath::Clamp(T / 0.15f, 0.f, 1.f);
			break;
		}
		case CoupSabre:
		{
			// "Vvvoum" : le bourdonnement monte puis redescend pendant que la lame fend l'air
			const float Bosse = FMath::Sin(PI * FMath::Clamp(T / 0.35f, 0.f, 1.f));
			const float F = H * (95.f + 85.f * Bosse);
			Phase += DeuxPi * F / Frequence;
			Phase2 += DeuxPi * F * 1.5f / Frequence;
			Filtre += 0.1f * (Bruit() - Filtre);
			const float Scie = 2.f * (FMath::Fmod(Phase, DeuxPi) / DeuxPi) - 1.f;
			V = (0.5f * Scie + 0.25f * FMath::Sin(Phase2) + 0.9f * Filtre) * Bosse;
			break;
		}
		case Bip:
		{
			Phase += DeuxPi * 1500.f * H / Frequence;
			V = 0.5f * FMath::Sin(Phase) * FMath::Clamp((0.14f - T) / 0.02f, 0.f, 1.f) * FMath::Clamp(T / 0.005f, 0.f, 1.f);
			break;
		}
		case Lancer:
		{
			Filtre += 0.2f * (Bruit() - Filtre);
			V = 1.2f * Filtre * FMath::Sin(PI * FMath::Clamp(T / 0.3f, 0.f, 1.f));
			break;
		}
		default:
			break;
		}
		// Phases ramenees entre 0 et 2 pi (garde la precision quand une boucle dure longtemps)
		if (Phase > DeuxPi) Phase = FMath::Fmod(Phase, DeuxPi);
		if (Phase2 > DeuxPi) Phase2 = FMath::Fmod(Phase2, DeuxPi);
		if (Phase3 > DeuxPi) Phase3 = FMath::Fmod(Phase3, DeuxPi);
		Sortie[i] = (int16)FMath::Clamp((int32)(V * 0.8f * 32767.f), -32767, 32767);
	}
	return NumSamples;
}

UAudioComponent* USonArsenalChelles::Jouer(UObject* Contexte, uint8 G, const FVector& Ou, float Volume, float NouvelleHauteur,
	USceneComponent* AccrocherA)
{
	UWorld* Monde = Contexte ? Contexte->GetWorld() : nullptr;
	if (!Monde || Monde->GetNetMode() == NM_DedicatedServer || Volume <= 0.f)
	{
		return nullptr;
	}
	USonArsenalChelles* Son = NewObject<USonArsenalChelles>(GetTransientPackage());
	Son->Genre = G;
	Son->Hauteur = NouvelleHauteur;
	const float Duree = DureeDe(G);
	if (Duree > 0.f)
	{
		Son->Duration = Duree + 0.1f;
	}

	UAudioComponent* Audio = AccrocherA
		? UGameplayStatics::SpawnSoundAttached(Son, AccrocherA, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
			Volume, 1.f, 0.f, Attenuation(), nullptr, true)
		: UGameplayStatics::SpawnSoundAtLocation(Contexte, Son, Ou, FRotator::ZeroRotator, Volume, 1.f, 0.f, Attenuation(), nullptr, true);

	if (Audio && Duree > 0.f)
	{
		// Un son "fabrique" ne s'arrete pas tout seul : on l'arrete a la fin
		TWeakObjectPtr<UAudioComponent> Faible(Audio);
		FTimerHandle Minuteur;
		Monde->GetTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
		{
			if (Faible.IsValid())
			{
				Faible->Stop();
			}
		}), Duree + 0.05f, false);
	}
	return Audio;
}
