// CHELLES 3D - Armes : petits outils pour les effets (particules et sons deja presents dans le projet)
#pragma once

#include "CoreMinimal.h"
#include <initializer_list>
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace ChellesArmesEffets
{
	/** Charge le premier objet qui existe parmi les chemins donnes (rien si aucun : l'effet est simplement saute). */
	template <typename T>
	T* Charger(std::initializer_list<const TCHAR*> Chemins)
	{
		for (const TCHAR* Chemin : Chemins)
		{
			if (T* Objet = LoadObject<T>(nullptr, Chemin, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return Objet;
			}
		}
		return nullptr;
	}

	/** Garde en memoire le resultat (un chemin absent n'est cherche qu'une fois). */
	template <typename T>
	struct FMemo
	{
		TWeakObjectPtr<T> Objet;
		bool bAbsent = false;

		T* Obtenir(std::initializer_list<const TCHAR*> Chemins)
		{
			if (!Objet.IsValid() && !bAbsent)
			{
				Objet = Charger<T>(Chemins);
				bAbsent = !Objet.IsValid();
			}
			return Objet.Get();
		}
	};

#define CHELLES_EFFET(Type, Nom, ...) \
	inline Type* Nom() { static FMemo<Type> Memo; return Memo.Obtenir({ __VA_ARGS__ }); }

	CHELLES_EFFET(UParticleSystem, Flammes, TEXT("/Game/FPS_Game/Particles/P_Fire.P_Fire"), TEXT("/Game/StarterContent/Particles/P_Fire.P_Fire"))
	CHELLES_EFFET(UParticleSystem, Explosion, TEXT("/Game/FPS_Game/Particles/P_Explosion.P_Explosion"), TEXT("/Game/StarterContent/Particles/P_Explosion.P_Explosion"))
	CHELLES_EFFET(UParticleSystem, Etincelles, TEXT("/Game/FPS_Game/Particles/P_Sparks.P_Sparks"), TEXT("/Game/StarterContent/Particles/P_Sparks.P_Sparks"))
	CHELLES_EFFET(UParticleSystem, Vapeur, TEXT("/Game/FPS_Game/Particles/P_Steam_Lit.P_Steam_Lit"), TEXT("/Game/StarterContent/Particles/P_Steam_Lit.P_Steam_Lit"))
	CHELLES_EFFET(UParticleSystem, ImpactBalle, TEXT("/Game/FPS_Game/Particles/P_AssaultRifle_IH_SparksOnly.P_AssaultRifle_IH_SparksOnly"), TEXT("/Game/FPS_Game/Particles/P_Sparks.P_Sparks"))
	CHELLES_EFFET(USoundBase, SonExplosion, TEXT("/Game/StarterContent/Audio/Explosion_Cue.Explosion_Cue"), TEXT("/Game/StarterContent/Audio/Explosion01.Explosion01"))
	CHELLES_EFFET(USoundBase, SonLaser, TEXT("/Game/FPS_Game/Sounds/SFX/Weapon_AssaultRifle/SFX_Laser_Cue.SFX_Laser_Cue"), TEXT("/Game/FPS_Game/Assets/Weapons/Laser/SFX_Laser_Cue.SFX_Laser_Cue"))

#undef CHELLES_EFFET

	/**
	 * Fait apparaitre une particule a un endroit et l'arrete apres Duree secondes
	 * (certaines particules tournent en boucle et ne s'arreteraient jamais seules).
	 */
	inline void Bouffee(UWorld* Monde, UParticleSystem* Modele, const FVector& Ou, float Taille, float Duree)
	{
		if (!Monde || !Modele || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		UParticleSystemComponent* P = UGameplayStatics::SpawnEmitterAtLocation(Monde, Modele, Ou, FRotator::ZeroRotator, FVector(Taille), true);
		if (!P)
		{
			return;
		}
		TWeakObjectPtr<UParticleSystemComponent> Faible(P);
		FTimerHandle Minuteur;
		Monde->GetTimerManager().SetTimer(Minuteur, FTimerDelegate::CreateLambda([Faible]()
		{
			if (Faible.IsValid())
			{
				Faible->DeactivateSystem();
			}
		}), Duree, false);
	}
}
