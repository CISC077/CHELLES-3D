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
		return Aucune;
	}

	struct FReglagesArsenal
	{
		bool bActif = true;
		FString ToucheChanger = TEXT("N");
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
	};

	FString CheminReglages();
	void ChargerReglages(FReglagesArsenal& R);
}
