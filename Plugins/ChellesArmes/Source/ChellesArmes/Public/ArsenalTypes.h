// CHELLES 3D - Arsenal (2026-11-17) : blaster, bazooka, sabre lumineux, grenade a zombies.
// Reglages lus dans Plugins\ChellesArmes\Arsenal.txt (modifiable au Bloc-notes, sans recompiler).
#pragma once

#include "CoreMinimal.h"

namespace ChellesArsenal
{
	/** Armes de l'arsenal. Aucune = l'arme normale du kit FPS_Game est en main. */
	enum EArme : uint8
	{
		Aucune = 0,
		Blaster,
		Bazooka,
		Sabre,
		Grenade,
		LanceFlammes,   // arme lourde (2026-11-20) : ramassee sur un ratelier, debloquee avec l'XP
		Mitrailleuse,   // arme lourde (2026-11-20) : ramassee sur un ratelier
		NombreArmes
	};

	inline const TCHAR* NomArme(uint8 Arme)
	{
		switch (Arme)
		{
		case Blaster: return TEXT("BLASTER");
		case Bazooka: return TEXT("BAZOOKA");
		case Sabre:   return TEXT("SABRE LUMINEUX");
		case Grenade: return TEXT("GRENADE A ZOMBIES");
		case LanceFlammes: return TEXT("LANCE-FLAMMES");
		case Mitrailleuse: return TEXT("MITRAILLEUSE LOURDE");
		default:      return TEXT("ARME NORMALE");
		}
	}

	inline uint8 ArmeDepuisTexte(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd();
		if (T.StartsWith(TEXT("Blas"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Laser"), ESearchCase::IgnoreCase)) return Blaster;
		if (T.StartsWith(TEXT("Baz"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Fus"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Miss"), ESearchCase::IgnoreCase)) return Bazooka;
		if (T.StartsWith(TEXT("Sab"), ESearchCase::IgnoreCase)) return Sabre;
		if (T.StartsWith(TEXT("Gren"), ESearchCase::IgnoreCase)) return Grenade;
		if (T.StartsWith(TEXT("Lance"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Flam"), ESearchCase::IgnoreCase)) return LanceFlammes;
		if (T.StartsWith(TEXT("Mitr"), ESearchCase::IgnoreCase) || T.StartsWith(TEXT("Mini"), ESearchCase::IgnoreCase)) return Mitrailleuse;
		return Aucune;
	}

	struct FReglagesArsenal
	{
		bool bActif = true;
		FString ToucheChanger = TEXT("Three");
		/** Materiau lumineux (cree par Tools\Arsenal\creer_materiau_lueur.py). Absent = couleurs simples + lumiere. */
		FString MateriauLueur = TEXT("/Game/ChellesArsenal/M_LueurChelles.M_LueurChelles");
		float Volume = 1.f;

		// BLASTER : rafale de traits lumineux avec le son "piou"
		float BlasterDegats = 30.f;
		float BlasterCadence = 0.16f;
		float BlasterVitesse = 9000.f;
		FLinearColor BlasterCouleur = FLinearColor(1.f, 0.06f, 0.04f);

		// BAZOOKA : fusee qui suit le viseur (la souris) puis explose
		float BazookaDegats = 150.f;
		float BazookaRayon = 650.f;
		float BazookaPoussee = 1400.f;
		float BazookaCadence = 1.4f;
		float BazookaVitesse = 2600.f;
		float BazookaVirage = 150.f;   // degres par seconde
		float BazookaDuree = 8.f;      // secondes avant explosion en l'air
		bool bBazookaGuidee = true;

		// SABRE LUMINEUX : coup au corps-a-corps
		float SabreDegats = 90.f;
		float SabrePortee = 240.f;
		float SabreCadence = 0.35f;
		bool bSabreTueZombies = true;  // un coup = zombie tranche
		FLinearColor SabreCouleur = FLinearColor(0.1f, 1.f, 0.2f);

		// GRENADE A ZOMBIES : attire les zombies puis explose
		float GrenadeAttente = 5.f;      // secondes pendant lesquelles elle attire les zombies
		float GrenadeRayonAppat = 3000.f; // cm
		float GrenadeDegats = 150.f;
		float GrenadeRayon = 700.f;
		float GrenadePoussee = 1300.f;
		float GrenadeCadence = 2.f;
		float GrenadeVitesse = 1500.f;

		// LANCE-FLAMMES (arme lourde) : jet de feu continu devant soi
		float FlammesDegats = 9.f;       // par jet (10 jets par seconde)
		float FlammesCadence = 0.1f;
		float FlammesPortee = 900.f;     // cm
		float FlammesAngle = 18.f;       // demi-ouverture du jet (degres)
		int32 FlammesXP = 250;           // XP necessaire (250 = GARDIEN DU QUARTIER)
		int32 FlammesCharge = 150;       // jets par ratelier (15 secondes de feu)

		// MITRAILLEUSE LOURDE (arme lourde) : rafale tres rapide de balles tracantes
		float MitrailleuseDegats = 20.f;
		float MitrailleuseCadence = 0.07f;
		float MitrailleuseVitesse = 15000.f;
		int32 MitrailleuseXP = 0;
		int32 MitrailleuseCharge = 200;  // balles par ratelier
		FLinearColor MitrailleuseCouleur = FLinearColor(1.f, 0.8f, 0.15f);

		// RATELIERS : armes lourdes accrochees aux murs dans un cadre en bois (au-dessus des socles d'armes)
		bool bRateliers = true;
		int32 RateliersMax = 10;
		float RatelierAttente = 90.f;    // secondes avant que l'arme revienne sur le ratelier
		FString ToucheRatelier = TEXT("E");
		FString ModeleFlammes = TEXT("/Game/AAAFantasticWeapons/Meshes/SKWeapRL.SKWeapRL");
		FString ModeleMitrailleuse = TEXT("/Game/AAAFantasticWeapons/Meshes/SKWeapSMG.SKWeapSMG");
		FString BoisCadre = TEXT("/Game/StarterContent/Materials/M_Wood_Pine.M_Wood_Pine");
		/** Oui = armes lourdes toujours dans la touche 3, sans ratelier (pour tester). */
		bool bLourdesSansRatelier = false;

		// PUISSANCE : les degats de l'arsenal montent avec le niveau d'XP (plugin ChellesPouvoirs)
		float PuissanceParNiveau = 0.1f; // +10 % par niveau (niveau 6 = +50 %)
	};

	/** Niveau (1 a 6) pour une quantite d'XP : memes paliers que le plugin ChellesPouvoirs. */
	inline int32 NiveauArsenal(int32 XP)
	{
		static const int32 Paliers[] = { 0, 100, 250, 500, 800, 1200 };
		int32 Niveau = 1;
		for (int32 i = 0; i < 6; ++i)
		{
			if (XP >= Paliers[i])
			{
				Niveau = i + 1;
			}
		}
		return Niveau;
	}

	inline bool EstArmeLourde(uint8 Arme)
	{
		return Arme == LanceFlammes || Arme == Mitrailleuse;
	}

	FString CheminReglages();
	void ChargerReglages(FReglagesArsenal& R);
}
