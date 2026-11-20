// CHELLES 3D - Butin (2026-11-20) : reglages communs (Plugins\ChellesButin\Butin.txt), UE 4.27.
#pragma once

#include "CoreMinimal.h"

namespace ChellesButin
{
	enum ERarete : uint8
	{
		Commun = 0,
		Rare,
		Legendaire
	};

	enum EGenrePing : uint8
	{
		PingIci = 0,
		PingEnnemi,
		PingButin,
		PingVehicule,
		PingFrappe
	};

	struct FReglagesButin
	{
		// ---- Coffres (Fortnite / Apex)
		bool bCoffres = true;
		int32 CoffresMax = 8;            // en tout dans la carte
		int32 CoffresParJoueur = 3;      // autour de chaque joueur
		float DistanceMin = 2500.f;      // cm
		float DistanceMax = 12000.f;     // cm
		float DistanceRetrait = 25000.f; // cm : plus loin de tous les joueurs = retire
		float Attente = 15.f;            // s entre deux nouveaux coffres
		FString ToucheOuvrir = TEXT("E");
		int32 ChanceRare = 22;           // %
		int32 ChanceLegendaire = 8;      // %
		int32 XPCommun = 40;
		int32 XPRare = 100;
		int32 XPLegendaire = 250;
		int32 BallesMitrailleuse = 150;
		int32 JetsFlammes = 100;
		FString ModeleCoffre = TEXT("/Game/Low_Poly_Viking_World/Models/Props/Treasure_Chest/SM_Treasure_Chest_01_A.SM_Treasure_Chest_01_A");
		FString ModeleOuvert = TEXT("/Game/Low_Poly_Viking_World/Models/Props/Currency/SM_Coin_Treasure_Chest_01.SM_Coin_Treasure_Chest_01");
		FString SonCoffre = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_LapComplete_Cue.UI_LapComplete_Cue");

		// ---- Ping (Apex)
		bool bPing = true;
		FString TouchePing = TEXT("J");
		float DureePing = 8.f;
		FString SonPing = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_Countdown_Cue.UI_Countdown_Cue");

		// ---- Series d'eliminations (Call of Duty)
		bool bSeries = true;
		int32 SerieRadar = 3;
		int32 SerieFrappe = 5;
		int32 SerieXP = 7;
		float DureeRadar = 20.f;
		float DistanceRadar = 10000.f;   // cm
		FString ToucheFrappe = TEXT("Four");
		int32 FrappeBombes = 6;
		float FrappeDegats = 160.f;
		float FrappeRayon = 650.f;
		int32 XPSerie = 150;
		FString SonSerie = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_CountdownFinish_Cue.UI_CountdownFinish_Cue");
	};

	/** Lit Plugins\ChellesButin\Butin.txt (les cles absentes gardent leur valeur par defaut). */
	void ChargerReglagesButin(FReglagesButin& R);

	const TCHAR* NomRarete(uint8 Rarete);
	FLinearColor CouleurRarete(uint8 Rarete);
	const TCHAR* TextePing(uint8 Genre);
	FLinearColor CouleurPing(uint8 Genre);
}
