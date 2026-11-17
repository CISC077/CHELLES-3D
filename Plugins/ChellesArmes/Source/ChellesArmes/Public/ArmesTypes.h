// CHELLES 3D - Armes : types de tirs speciaux et reglages lus dans Plugins\ChellesArmes\Armes.txt
#pragma once

#include "CoreMinimal.h"

namespace ChellesArmes
{
	/** Tirs speciaux, tires EN PLUS du tir normal de l'arme. */
	enum ETir : uint8
	{
		Aucun = 0,
		Laser,
		Feu,
		Glace,
		Foudre,
		NombreTirs
	};

	inline const TCHAR* NomTir(uint8 Tir)
	{
		switch (Tir)
		{
		case Laser:  return TEXT("LASER");
		case Feu:    return TEXT("BOULE DE FEU");
		case Glace:  return TEXT("GLACIAL");
		case Foudre: return TEXT("FOUDRE");
		default:     return TEXT("AUCUN");
		}
	}

	inline uint8 TirDepuisTexte(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		if (T.StartsWith(TEXT("Laser"), ESearchCase::IgnoreCase)) return Laser;
		if (T.StartsWith(TEXT("Feu"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Boule"), ESearchCase::IgnoreCase)) return Feu;
		if (T.StartsWith(TEXT("Glac"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Glace"), ESearchCase::IgnoreCase)) return Glace;
		if (T.StartsWith(TEXT("Foudre"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Elec"), ESearchCase::IgnoreCase)) return Foudre;
		return Aucun;
	}

	/** Reglage a la main d'une apparence (dans le repere de l'arme d'origine). */
	struct FReglageApparence
	{
		float Avant = 0.f;    // cm vers le bout du canon
		float Haut = 0.f;     // cm vers le haut
		float Cote = 0.f;     // cm vers la droite
		float Tangage = 0.f;  // degres : leve / baisse le canon
		float Lacet = 0.f;    // degres : tourne a gauche / droite
		float Roulis = 0.f;   // degres : tourne autour du canon (180 = remettre a l'endroit)
		float Echelle = 1.f;  // multiplie la taille calculee automatiquement

		FString VersTexte() const
		{
			return FString::Printf(TEXT("%g,%g,%g,%g,%g,%g,%g"), Avant, Haut, Cote, Tangage, Lacet, Roulis, Echelle);
		}

		void DepuisTexte(const FString& Texte)
		{
			TArray<FString> Morceaux;
			Texte.ParseIntoArray(Morceaux, TEXT(","), true);
			float* Champs[] = { &Avant, &Haut, &Cote, &Tangage, &Lacet, &Roulis, &Echelle };
			for (int32 i = 0; i < Morceaux.Num() && i < 7; ++i)
			{
				*Champs[i] = FCString::Atof(*Morceaux[i].TrimStartAndEnd());
			}
			if (Echelle <= 0.01f)
			{
				Echelle = 1.f;
			}
		}
	};

	/** Caracteristiques d'un tir special (modifiables dans Armes.txt). */
	struct FCaracTir
	{
		float Degats = 20.f;          // degats au point touche (tete = mort pour un zombie, corps = 33 %)
		float Cadence = 0.3f;         // secondes minimum entre deux tirs speciaux de ce type
		float Vitesse = 6000.f;       // cm/s
		float DegatsZone = 0.f;       // BOULE DE FEU : degats de l'explosion
		float Rayon = 0.f;            // BOULE DE FEU : rayon de l'explosion (cm) / FOUDRE : rayon de la chaine
		float DureeGel = 0.f;         // GLACIAL : secondes de gel
		int32 Rebonds = 0;            // FOUDRE : nombre d'ennemis voisins touches en plus
	};

	struct FReglagesArmes
	{
		bool bApparences = true;
		bool bTirsSpeciaux = true;
		bool bGarderAccessoires = false;
		FString ToucheChanger = TEXT("O");

		/** Heros sans arme (ex. PANTERA, 2026-11-11) : clic gauche = tir special depuis les mains.
		 *  On les reconnait au nom de leur corps (SkeletalMesh). Ligne Heros= dans Armes.txt. */
		TArray<FString> Heros = { TEXT("SK_Pantera") };
		/** Tir special de depart des heros (le joueur change avec la touche O). */
		uint8 TirHeros = Laser;

		/** FLASH ELECTRIQUE (2026-11-11) : touche qui lance un double eclair depuis les mains,
		 *  pour les heros dont le corps porte un de ces noms (a pied avec les armes du soldat, ou en vol). */
		FString ToucheFlash = TEXT("X");
		TArray<FString> HerosFlash = { TEXT("SK_SpiderMan_Vol"), TEXT("SK_SpiderMan_UE4"), TEXT("SK_SpiderFemelle"), TEXT("SK_Pantera") };

		/** Cle = nom du Blueprint de l'arme sans _C (ex : BPC_WeaponRifle). Valeur = chemins (le 1er qui existe est pris). */
		TMap<FString, TArray<FString>> Apparences;
		TMap<FString, FReglageApparence> Reglages;
		/** Tir special donne par defaut a une arme (le joueur peut changer avec la touche O). */
		TMap<FString, uint8> TirsParDefaut;

		FCaracTir Tirs[NombreTirs];

		FReglagesArmes()
		{
			Tirs[Laser].Degats = 25.f;  Tirs[Laser].Cadence = 0.15f; Tirs[Laser].Vitesse = 30000.f;
			Tirs[Feu].Degats = 40.f;    Tirs[Feu].Cadence = 0.7f;    Tirs[Feu].Vitesse = 2800.f; Tirs[Feu].DegatsZone = 35.f; Tirs[Feu].Rayon = 300.f;
			Tirs[Glace].Degats = 15.f;  Tirs[Glace].Cadence = 0.4f;  Tirs[Glace].Vitesse = 7000.f; Tirs[Glace].DureeGel = 3.f;
			Tirs[Foudre].Degats = 25.f; Tirs[Foudre].Cadence = 0.5f; Tirs[Foudre].Vitesse = 18000.f; Tirs[Foudre].Rayon = 700.f; Tirs[Foudre].Rebonds = 3;
		}
	};

	/** Chemin du fichier de reglages. */
	FString CheminReglages();

	/** Lit Armes.txt (garde les valeurs par defaut pour ce qui manque). */
	void ChargerReglages(FReglagesArmes& R);

	/** Ecrit (ou remplace) une ligne Cle=Valeur dans Armes.txt, sans toucher aux autres lignes. */
	bool EcrireLigne(const FString& Cle, const FString& Valeur);
}
