// CHELLES 3D - Butin : lecture de Butin.txt
#include "ButinTypes.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	bool ButinEstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}
}

namespace ChellesButin
{
	void ChargerReglagesButin(FReglagesButin& R)
	{
		const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesButin"), TEXT("Butin.txt"));
		TArray<FString> Lignes;
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Butin] %s introuvable : reglages par defaut."), *Chemin);
			return;
		}
		for (FString Ligne : Lignes)
		{
			Ligne.TrimStartAndEndInline();
			if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
			{
				continue;
			}
			FString Cle, Valeur;
			if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
			{
				continue;
			}
			Cle.TrimStartAndEndInline();
			Valeur.TrimStartAndEndInline();
			const float N = FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));
			const int32 I = FMath::RoundToInt(N);
			auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };

			if (Est(TEXT("Coffres"))) R.bCoffres = ButinEstOui(Valeur);
			else if (Est(TEXT("Coffres.Max"))) R.CoffresMax = FMath::Clamp(I, 0, 40);
			else if (Est(TEXT("Coffres.ParJoueur"))) R.CoffresParJoueur = FMath::Clamp(I, 0, 10);
			else if (Est(TEXT("Coffres.DistanceMin"))) R.DistanceMin = FMath::Clamp(N, 5.f, 500.f) * 100.f;
			else if (Est(TEXT("Coffres.DistanceMax"))) R.DistanceMax = FMath::Clamp(N, 10.f, 1000.f) * 100.f;
			else if (Est(TEXT("Coffres.DistanceRetrait"))) R.DistanceRetrait = FMath::Clamp(N, 50.f, 2000.f) * 100.f;
			else if (Est(TEXT("Coffres.Attente"))) R.Attente = FMath::Clamp(N, 1.f, 600.f);
			else if (Est(TEXT("Coffres.Touche"))) R.ToucheOuvrir = Valeur;
			else if (Est(TEXT("Coffres.ChanceRare"))) R.ChanceRare = FMath::Clamp(I, 0, 100);
			else if (Est(TEXT("Coffres.ChanceLegendaire"))) R.ChanceLegendaire = FMath::Clamp(I, 0, 100);
			else if (Est(TEXT("Coffres.XPCommun"))) R.XPCommun = FMath::Clamp(I, 0, 5000);
			else if (Est(TEXT("Coffres.XPRare"))) R.XPRare = FMath::Clamp(I, 0, 5000);
			else if (Est(TEXT("Coffres.XPLegendaire"))) R.XPLegendaire = FMath::Clamp(I, 0, 5000);
			else if (Est(TEXT("Coffres.BallesMitrailleuse"))) R.BallesMitrailleuse = FMath::Clamp(I, 0, 2000);
			else if (Est(TEXT("Coffres.JetsLanceFlammes"))) R.JetsFlammes = FMath::Clamp(I, 0, 2000);
			else if (Est(TEXT("Coffres.Modele"))) R.ModeleCoffre = Valeur;
			else if (Est(TEXT("Coffres.ModeleOuvert"))) R.ModeleOuvert = Valeur;
			else if (Est(TEXT("Coffres.Son"))) R.SonCoffre = Valeur;
			else if (Est(TEXT("Ping"))) R.bPing = ButinEstOui(Valeur);
			else if (Est(TEXT("Ping.Touche"))) R.TouchePing = Valeur;
			else if (Est(TEXT("Ping.Duree"))) R.DureePing = FMath::Clamp(N, 1.f, 60.f);
			else if (Est(TEXT("Ping.Son"))) R.SonPing = Valeur;
			else if (Est(TEXT("Series"))) R.bSeries = ButinEstOui(Valeur);
			else if (Est(TEXT("Series.Radar"))) R.SerieRadar = FMath::Clamp(I, 0, 100);
			else if (Est(TEXT("Series.Frappe"))) R.SerieFrappe = FMath::Clamp(I, 0, 100);
			else if (Est(TEXT("Series.XP"))) R.SerieXP = FMath::Clamp(I, 0, 100);
			else if (Est(TEXT("Series.DureeRadar"))) R.DureeRadar = FMath::Clamp(N, 1.f, 120.f);
			else if (Est(TEXT("Series.DistanceRadar"))) R.DistanceRadar = FMath::Clamp(N, 10.f, 500.f) * 100.f;
			else if (Est(TEXT("Series.ToucheFrappe"))) R.ToucheFrappe = Valeur;
			else if (Est(TEXT("Series.Bombes"))) R.FrappeBombes = FMath::Clamp(I, 1, 20);
			else if (Est(TEXT("Series.DegatsBombe"))) R.FrappeDegats = FMath::Clamp(N, 1.f, 5000.f);
			else if (Est(TEXT("Series.RayonBombe"))) R.FrappeRayon = FMath::Clamp(N, 1.f, 30.f) * 100.f;
			else if (Est(TEXT("Series.XPBonus"))) R.XPSerie = FMath::Clamp(I, 0, 5000);
			else if (Est(TEXT("Series.Son"))) R.SonSerie = Valeur;
		}
	}

	const TCHAR* NomRarete(uint8 Rarete)
	{
		switch (Rarete)
		{
		case Rare: return TEXT("COFFRE RARE");
		case Legendaire: return TEXT("COFFRE LEGENDAIRE");
		default: return TEXT("COFFRE");
		}
	}

	FLinearColor CouleurRarete(uint8 Rarete)
	{
		switch (Rarete)
		{
		case Rare: return FLinearColor(0.65f, 0.2f, 1.f);
		case Legendaire: return FLinearColor(1.f, 0.7f, 0.1f);
		default: return FLinearColor(0.2f, 0.55f, 1.f);
		}
	}

	const TCHAR* TextePing(uint8 Genre)
	{
		switch (Genre)
		{
		case PingEnnemi: return TEXT("ENNEMI !");
		case PingButin: return TEXT("BUTIN");
		case PingVehicule: return TEXT("VEHICULE");
		case PingFrappe: return TEXT("FRAPPE AERIENNE !");
		default: return TEXT("ICI");
		}
	}

	FLinearColor CouleurPing(uint8 Genre)
	{
		switch (Genre)
		{
		case PingEnnemi: return FLinearColor(1.f, 0.15f, 0.1f);
		case PingButin: return FLinearColor(1.f, 0.75f, 0.1f);
		case PingVehicule: return FLinearColor(0.2f, 1.f, 0.4f);
		case PingFrappe: return FLinearColor(1.f, 0.35f, 0.f);
		default: return FLinearColor(0.2f, 0.85f, 1.f);
		}
	}
}
