// CHELLES 3D - Armes : petits outils pour les effets (particules et sons deja presents dans le projet)
#pragma once

#include "CoreMinimal.h"
#include <initializer_list>
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
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
	// Impacts (2026-11-18) : giclee sur un personnage, trou + poussiere sur un mur ou le sol
	CHELLES_EFFET(UParticleSystem, ImpactCorps, TEXT("/Game/FPS_Game/Particles/P_body_bullet_impact.P_body_bullet_impact"), TEXT("/Game/FPS_Game/Particles/P_body_bullet_NoStop.P_body_bullet_NoStop"))
	CHELLES_EFFET(UMaterialInterface, TrouImpact, TEXT("/Game/FPS_Game/Particles/Decal_Bullet_Impact_WithNormal.Decal_Bullet_Impact_WithNormal"), TEXT("/Game/FPS_Game/Particles/Decal_Bullet_Impact.Decal_Bullet_Impact"))
	CHELLES_EFFET(USoundBase, SonImpactMur, TEXT("/Game/FPS_Game/Sounds/SFX/Weapon_AssaultRifle/Mono/AssaultRifle_ImpactConcrete_Cue.AssaultRifle_ImpactConcrete_Cue"),
		TEXT("/Game/FPS_Game/EpicDemoContent/Sounds/SFX/Weapon_AssaultRifle/Mono/AssaultRifle_ImpactConcrete_Cue.AssaultRifle_ImpactConcrete_Cue"))

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

	/**
	 * Marque d'impact a l'endroit touche, vue sur cette machine :
	 *  - sur un personnage (bCorps) : giclee ;
	 *  - sur un mur / le sol : trou noirci (decal) + poussiere + petit bruit.
	 * Taille : 1 = balle, plus pour une boule de feu.
	 */
	inline void Impact(UWorld* Monde, const FVector& Point, const FVector& Normale, bool bCorps, float Taille = 1.f, bool bSon = true)
	{
		if (!Monde || Monde->GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
		const FVector N = Normale.IsNearlyZero() ? FVector::UpVector : Normale.GetSafeNormal();
		if (bCorps)
		{
			if (UParticleSystem* Giclee = ImpactCorps())
			{
				UGameplayStatics::SpawnEmitterAtLocation(Monde, Giclee, Point, N.Rotation(), FVector(FMath::Clamp(Taille, 0.6f, 2.f)), true);
			}
			return;
		}
		if (UMaterialInterface* Trou = TrouImpact())
		{
			// Le decal se projette le long de son axe X : on le tourne vers l'interieur de la surface
			const float Rayon = 9.f * FMath::Clamp(Taille, 0.6f, 6.f);
			FRotator Sens = (-N).Rotation();
			Sens.Roll = FMath::FRandRange(-180.f, 180.f);
			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(Monde, Trou, FVector(FMath::Max(12.f, Rayon * 0.8f), Rayon, Rayon), Point, Sens, 12.f))
			{
				Decal->SetFadeScreenSize(0.002f);
				Decal->SetFadeOut(9.f, 3.f, false);
			}
		}
		Bouffee(Monde, ImpactBalle(), Point + N * 2.f, FMath::Clamp(Taille, 0.6f, 3.f), 0.35f);
		if (bSon)
		{
			if (USoundBase* Son = SonImpactMur())
			{
				UGameplayStatics::PlaySoundAtLocation(Monde, Son, Point, 0.55f, FMath::FRandRange(0.9f, 1.1f));
			}
		}
	}

	/** Le coup a-t-il touche un personnage (ou un pion) ? */
	inline bool EstUnCorps(const AActor* Acteur)
	{
		return Acteur && Acteur->IsA(APawn::StaticClass());
	}
}
