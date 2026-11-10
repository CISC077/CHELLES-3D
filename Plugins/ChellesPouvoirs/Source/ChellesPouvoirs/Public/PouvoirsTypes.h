// CHELLES 3D - XP et pouvoirs : les paliers et les pouvoirs (a modifier ici pour changer le rythme)
#pragma once

#include "CoreMinimal.h"

namespace ChellesPouvoirs
{
	/** Les pouvoirs, dans l'ordre ou ils se debloquent. */
	enum EPouvoir : uint8
	{
		Envol = 0,
		BouleDeFeu,
		Verre,
		Cameleon,
		Gel,
		NombrePouvoirs
	};

	/** XP gagnee par minute de jeu (hors pause et menu). */
	static constexpr int32 XPParMinute = 10;
	/** Bonus a chaque heure de jeu complete. */
	static constexpr int32 BonusParHeure = 50;

	struct FPalier
	{
		const TCHAR* Titre;
		int32 XP;
	};

	/** Rythme RAPIDE choisi par MERIDJA le 09/11/2026 : tout est debloque en 2 h environ. */
	inline const TArray<FPalier>& Paliers()
	{
		static const TArray<FPalier> Liste =
		{
			{ TEXT("HABITANT"), 0 },
			{ TEXT("VOISIN"), 100 },                       // 10 min
			{ TEXT("GARDIEN DU QUARTIER"), 250 },          // 25 min
			{ TEXT("FANT\u00D4ME DE CHELLES"), 500 },      // 50 min
			{ TEXT("CAM\u00C9L\u00C9ON"), 800 },           // 1 h 20
			{ TEXT("L\u00C9GENDE DE CHELLES"), 1200 }      // 2 h
		};
		return Liste;
	}

	/** Niveau (1 = Habitant) pour une quantite d'XP. */
	inline int32 NiveauPourXP(int32 XP)
	{
		int32 Niveau = 1;
		for (int32 i = 0; i < Paliers().Num(); ++i)
		{
			if (XP >= Paliers()[i].XP)
			{
				Niveau = i + 1;
			}
		}
		return Niveau;
	}

	struct FInfoPouvoir
	{
		const TCHAR* Nom;
		const TCHAR* Aide;
		/** Niveau qui debloque le pouvoir (2 = Voisin...). */
		int32 Niveau;
		/** Duree de l'effet en secondes (0 = instantane). */
		float Duree;
		/** Attente apres la fin de l'effet avant de pouvoir le relancer. */
		float Recharge;
		FLinearColor Couleur;
	};

	inline const FInfoPouvoir& Pouvoir(int32 Index)
	{
		static const FInfoPouvoir Liste[NombrePouvoirs] =
		{
			{ TEXT("ENVOL"), TEXT("Tu voles 20 s : ESPACE monte, CTRL descend"), 2, 20.f, 45.f, FLinearColor(0.25f, 0.85f, 1.f) },
			{ TEXT("BOULE DE FEU"), TEXT("Lance une boule de feu l\u00E0 o\u00F9 tu vises"), 3, 0.f, 3.f, FLinearColor(1.f, 0.45f, 0.08f) },
			{ TEXT("EFFET VERRE"), TEXT("Invisible pour les autres pendant 10 s"), 4, 10.f, 60.f, FLinearColor(0.7f, 1.f, 0.95f) },
			{ TEXT("CAM\u00C9L\u00C9ON"), TEXT("Prends l'apparence d'un autre (J encore = redevenir toi)"), 5, 0.f, 4.f, FLinearColor(0.45f, 1.f, 0.35f) },
			{ TEXT("GEL"), TEXT("G\u00E8le 4 s le joueur ou le passant vis\u00E9 (30 m)"), 6, 4.f, 30.f, FLinearColor(0.55f, 0.75f, 1.f) }
		};
		return Liste[FMath::Clamp(Index, 0, int32(NombrePouvoirs) - 1)];
	}

	inline bool EstDebloque(int32 Index, int32 XP)
	{
		return NiveauPourXP(XP) >= Pouvoir(Index).Niveau;
	}
}
