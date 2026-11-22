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
	/** XP de combat (2026-11-18) : chaque coup qui touche un ennemi (bandit, soldat, zombie...). */
	static constexpr int32 XPParTouche = 1;
	/** XP de combat : ennemi elimine (le dernier joueur qui l'a touche la recoit). */
	static constexpr int32 XPParEnnemiTue = 10;

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

	/**
	 * Reglages lus dans Plugins\ChellesPouvoirs\Pouvoirs.txt au lancement (2026-11-22, demande MERIDJA :
	 * tous les joueurs, soldat compris, ont tous les pouvoirs et peuvent tous voler).
	 */
	struct FReglagesPouvoirs
	{
		/** Vrai : les 5 pouvoirs sont ouverts des le depart pour tout le monde (l'XP et les niveaux restent comptes). */
		bool bTousLesPouvoirs = true;
		/** Vrai : sauter puis rappuyer sur ESPACE en l'air = ENVOL (soldat et tous les personnages a pied). */
		bool bVolPourTous = true;
		/** ENVOL : secondes de vol (0 = sans limite, on vole jusqu'a se poser). */
		float DureeEnvol = 0.f;
		/** ENVOL : attente apres le vol avant de pouvoir revoler. */
		float RechargeEnvol = 2.f;
	};

	inline FReglagesPouvoirs& Reglages()
	{
		static FReglagesPouvoirs R;
		return R;
	}

	/** Duree enregistree pour un vol "sans limite" (1 heure : on se pose bien avant). */
	static constexpr float DureeVolSansLimite = 3600.f;

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
		static FInfoPouvoir Liste[NombrePouvoirs] =
		{
			{ TEXT("ENVOL"), TEXT("Tu voles : ESPACE monte, CTRL descend, pose-toi (ou J) pour atterrir"), 2, 20.f, 45.f, FLinearColor(0.25f, 0.85f, 1.f) },
			{ TEXT("BOULE DE FEU"), TEXT("Lance une boule de feu l\u00E0 o\u00F9 tu vises"), 3, 0.f, 3.f, FLinearColor(1.f, 0.45f, 0.08f) },
			{ TEXT("EFFET VERRE"), TEXT("Invisible pour les autres pendant 10 s"), 4, 10.f, 60.f, FLinearColor(0.7f, 1.f, 0.95f) },
			{ TEXT("CAM\u00C9L\u00C9ON"), TEXT("Prends l'apparence d'un autre (J encore = redevenir toi)"), 5, 0.f, 4.f, FLinearColor(0.45f, 1.f, 0.35f) },
			{ TEXT("GEL"), TEXT("G\u00E8le 4 s le joueur ou le passant vis\u00E9 (30 m)"), 6, 4.f, 30.f, FLinearColor(0.55f, 0.75f, 1.f) }
		};
		// ENVOL : duree et recharge viennent de Pouvoirs.txt
		Liste[Envol].Duree = Reglages().DureeEnvol > 0.f ? Reglages().DureeEnvol : DureeVolSansLimite;
		Liste[Envol].Recharge = FMath::Max(0.f, Reglages().RechargeEnvol);
		return Liste[FMath::Clamp(Index, 0, int32(NombrePouvoirs) - 1)];
	}

	inline bool EstDebloque(int32 Index, int32 XP)
	{
		if (Reglages().bTousLesPouvoirs)
		{
			return true; // 2026-11-22 : tout le monde a tous les pouvoirs des le depart
		}
		return NiveauPourXP(XP) >= Pouvoir(Index).Niveau;
	}
}
