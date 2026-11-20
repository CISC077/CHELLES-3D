// CHELLES 3D - Arsenal : lecture de Plugins\ChellesArmes\Arsenal.txt
#include "ArsenalTypes.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ChellesArsenal
{
	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesArmes"), TEXT("Arsenal.txt"));
	}

	static bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	/** "255,40,20" (0-255) -> couleur. */
	static void LireCouleur(const FString& Texte, FLinearColor& Couleur)
	{
		TArray<FString> M;
		Texte.ParseIntoArray(M, TEXT(","), true);
		if (M.Num() < 3)
		{
			return;
		}
		Couleur = FLinearColor(
			FMath::Clamp(FCString::Atof(*M[0]) / 255.f, 0.f, 1.f),
			FMath::Clamp(FCString::Atof(*M[1]) / 255.f, 0.f, 1.f),
			FMath::Clamp(FCString::Atof(*M[2]) / 255.f, 0.f, 1.f));
	}

	void ChargerReglages(FReglagesArsenal& R)
	{
		R = FReglagesArsenal();
		TArray<FString> Lignes;
		const FString Chemin = CheminReglages();
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Log, TEXT("[Arsenal] %s absent : reglages par defaut."), *Chemin);
			return;
		}
		for (const FString& Brute : Lignes)
		{
			FString Ligne = Brute.TrimStartAndEnd();
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
			const float N = FCString::Atof(*Valeur);
			auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };

			if (Est(TEXT("Arsenal"))) R.bActif = EstOui(Valeur);
			else if (Est(TEXT("ToucheChanger"))) R.ToucheChanger = Valeur;
			else if (Est(TEXT("MateriauLueur"))) R.MateriauLueur = Valeur;
			else if (Est(TEXT("Volume"))) R.Volume = FMath::Clamp(N, 0.f, 4.f);

			else if (Est(TEXT("Blaster.Degats"))) R.BlasterDegats = FMath::Max(0.f, N);
			else if (Est(TEXT("Blaster.Cadence"))) R.BlasterCadence = FMath::Clamp(N, 0.05f, 5.f);
			else if (Est(TEXT("Blaster.Vitesse"))) R.BlasterVitesse = FMath::Clamp(N, 500.f, 60000.f);
			else if (Est(TEXT("Blaster.Couleur"))) LireCouleur(Valeur, R.BlasterCouleur);

			else if (Est(TEXT("Bazooka.Degats"))) R.BazookaDegats = FMath::Max(0.f, N);
			else if (Est(TEXT("Bazooka.Rayon"))) R.BazookaRayon = FMath::Clamp(N, 50.f, 5000.f);
			else if (Est(TEXT("Bazooka.Poussee"))) R.BazookaPoussee = FMath::Clamp(N, 0.f, 10000.f);
			else if (Est(TEXT("Bazooka.Cadence"))) R.BazookaCadence = FMath::Clamp(N, 0.2f, 30.f);
			else if (Est(TEXT("Bazooka.Vitesse"))) R.BazookaVitesse = FMath::Clamp(N, 300.f, 20000.f);
			else if (Est(TEXT("Bazooka.Virage"))) R.BazookaVirage = FMath::Clamp(N, 0.f, 720.f);
			else if (Est(TEXT("Bazooka.Duree"))) R.BazookaDuree = FMath::Clamp(N, 1.f, 30.f);
			else if (Est(TEXT("Bazooka.Guidee"))) R.bBazookaGuidee = EstOui(Valeur);

			else if (Est(TEXT("Sabre.Degats"))) R.SabreDegats = FMath::Max(0.f, N);
			else if (Est(TEXT("Sabre.Portee"))) R.SabrePortee = FMath::Clamp(N, 50.f, 1000.f);
			else if (Est(TEXT("Sabre.Cadence"))) R.SabreCadence = FMath::Clamp(N, 0.1f, 5.f);
			else if (Est(TEXT("Sabre.TrancheZombies"))) R.bSabreTueZombies = EstOui(Valeur);
			else if (Est(TEXT("Sabre.Couleur"))) LireCouleur(Valeur, R.SabreCouleur);

			else if (Est(TEXT("Grenade.Attente"))) R.GrenadeAttente = FMath::Clamp(N, 0.5f, 30.f);
			else if (Est(TEXT("Grenade.RayonAppat"))) R.GrenadeRayonAppat = FMath::Clamp(N, 100.f, 20000.f);
			else if (Est(TEXT("Grenade.Degats"))) R.GrenadeDegats = FMath::Max(0.f, N);
			else if (Est(TEXT("Grenade.Rayon"))) R.GrenadeRayon = FMath::Clamp(N, 50.f, 5000.f);
			else if (Est(TEXT("Grenade.Poussee"))) R.GrenadePoussee = FMath::Clamp(N, 0.f, 10000.f);
			else if (Est(TEXT("Grenade.Cadence"))) R.GrenadeCadence = FMath::Clamp(N, 0.2f, 30.f);
			else if (Est(TEXT("Grenade.Vitesse"))) R.GrenadeVitesse = FMath::Clamp(N, 200.f, 6000.f);

			else if (Est(TEXT("LanceFlammes.Degats"))) R.FlammesDegats = FMath::Max(0.f, N);
			else if (Est(TEXT("LanceFlammes.Cadence"))) R.FlammesCadence = FMath::Clamp(N, 0.05f, 2.f);
			else if (Est(TEXT("LanceFlammes.Portee"))) R.FlammesPortee = FMath::Clamp(N, 100.f, 4000.f);
			else if (Est(TEXT("LanceFlammes.Angle"))) R.FlammesAngle = FMath::Clamp(N, 3.f, 60.f);
			else if (Est(TEXT("LanceFlammes.XP"))) R.FlammesXP = FMath::Max(0, FMath::RoundToInt(N));
			else if (Est(TEXT("LanceFlammes.Charge"))) R.FlammesCharge = FMath::Clamp(FMath::RoundToInt(N), 1, 100000);

			else if (Est(TEXT("Mitrailleuse.Degats"))) R.MitrailleuseDegats = FMath::Max(0.f, N);
			else if (Est(TEXT("Mitrailleuse.Cadence"))) R.MitrailleuseCadence = FMath::Clamp(N, 0.03f, 2.f);
			else if (Est(TEXT("Mitrailleuse.Vitesse"))) R.MitrailleuseVitesse = FMath::Clamp(N, 500.f, 60000.f);
			else if (Est(TEXT("Mitrailleuse.XP"))) R.MitrailleuseXP = FMath::Max(0, FMath::RoundToInt(N));
			else if (Est(TEXT("Mitrailleuse.Charge"))) R.MitrailleuseCharge = FMath::Clamp(FMath::RoundToInt(N), 1, 100000);
			else if (Est(TEXT("Mitrailleuse.Couleur"))) LireCouleur(Valeur, R.MitrailleuseCouleur);

			else if (Est(TEXT("Rateliers"))) R.bRateliers = EstOui(Valeur);
			else if (Est(TEXT("Rateliers.Max"))) R.RateliersMax = FMath::Clamp(FMath::RoundToInt(N), 0, 60);
			else if (Est(TEXT("Rateliers.Attente"))) R.RatelierAttente = FMath::Clamp(N, 5.f, 3600.f);
			else if (Est(TEXT("Rateliers.Touche"))) R.ToucheRatelier = Valeur;
			else if (Est(TEXT("Rateliers.ModeleLanceFlammes"))) R.ModeleFlammes = Valeur;
			else if (Est(TEXT("Rateliers.ModeleMitrailleuse"))) R.ModeleMitrailleuse = Valeur;
			else if (Est(TEXT("Rateliers.Bois"))) R.BoisCadre = Valeur;
			else if (Est(TEXT("ArmesLourdesSansRatelier"))) R.bLourdesSansRatelier = EstOui(Valeur);

			else if (Est(TEXT("Puissance.ParNiveau"))) R.PuissanceParNiveau = FMath::Clamp(N, 0.f, 2.f);
		}
		UE_LOG(LogTemp, Log, TEXT("[Arsenal] Reglages lus (%s). Touche %s = changer d'arme."), *Chemin, *R.ToucheChanger);
	}
}
