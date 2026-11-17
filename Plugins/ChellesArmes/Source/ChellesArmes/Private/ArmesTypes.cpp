// CHELLES 3D - Armes : lecture / ecriture de Plugins\ChellesArmes\Armes.txt
#include "ArmesTypes.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace ChellesArmes
{
	FString CheminReglages()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesArmes"), TEXT("Armes.txt"));
	}

	static bool EstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	void ChargerReglages(FReglagesArmes& R)
	{
		R = FReglagesArmes();
		TArray<FString> Lignes;
		const FString Chemin = CheminReglages();
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Armes] %s absent : aucune apparence changee, tirs speciaux par defaut."), *Chemin);
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

			if (Cle.Equals(TEXT("Apparences"), ESearchCase::IgnoreCase)) { R.bApparences = EstOui(Valeur); continue; }
			if (Cle.Equals(TEXT("TirsSpeciaux"), ESearchCase::IgnoreCase)) { R.bTirsSpeciaux = EstOui(Valeur); continue; }
			if (Cle.Equals(TEXT("GarderAccessoires"), ESearchCase::IgnoreCase)) { R.bGarderAccessoires = EstOui(Valeur); continue; }
			if (Cle.Equals(TEXT("ToucheChanger"), ESearchCase::IgnoreCase)) { R.ToucheChanger = Valeur; continue; }
			if (Cle.Equals(TEXT("Heros"), ESearchCase::IgnoreCase))
			{
				// Heros=SK_Pantera | SK_AutreHeros  (vide = aucun heros ne tire sans arme)
				R.Heros.Reset();
				Valeur.ParseIntoArray(R.Heros, TEXT("|"), true);
				for (FString& H : R.Heros)
				{
					H.TrimStartAndEndInline();
				}
				R.Heros.RemoveAll([](const FString& H) { return H.IsEmpty(); });
				continue;
			}
			if (Cle.Equals(TEXT("ToucheFlash"), ESearchCase::IgnoreCase)) { R.ToucheFlash = Valeur; continue; }
			if (Cle.Equals(TEXT("HerosFlash"), ESearchCase::IgnoreCase))
			{
				// HerosFlash=SK_SpiderMan_Vol | SK_Pantera ...  (vide = personne n'a le flash electrique)
				R.HerosFlash.Reset();
				Valeur.ParseIntoArray(R.HerosFlash, TEXT("|"), true);
				for (FString& H : R.HerosFlash)
				{
					H.TrimStartAndEndInline();
				}
				R.HerosFlash.RemoveAll([](const FString& H) { return H.IsEmpty(); });
				continue;
			}
			if (Cle.Equals(TEXT("TirHeros"), ESearchCase::IgnoreCase))
			{
				const uint8 T = TirDepuisTexte(Valeur);
				R.TirHeros = (T == Aucun) ? Laser : T;
				continue;
			}

			FString Avant, Apres;
			if (!Cle.Split(TEXT("."), &Avant, &Apres))
			{
				continue;
			}
			if (Avant.Equals(TEXT("Arme"), ESearchCase::IgnoreCase))
			{
				TArray<FString> Chemins;
				Valeur.ParseIntoArray(Chemins, TEXT("|"), true);
				for (FString& C : Chemins)
				{
					C.TrimStartAndEndInline();
				}
				// "Aucune" ou vide = garder l'apparence d'origine
				Chemins.RemoveAll([](const FString& C) { return C.IsEmpty() || C.Equals(TEXT("Aucune"), ESearchCase::IgnoreCase); });
				if (Chemins.Num() > 0)
				{
					R.Apparences.Add(Apres, Chemins);
				}
				else
				{
					R.Apparences.Remove(Apres);
				}
			}
			else if (Avant.Equals(TEXT("Reglage"), ESearchCase::IgnoreCase))
			{
				FReglageApparence Reg;
				Reg.DepuisTexte(Valeur);
				R.Reglages.Add(Apres, Reg);
			}
			else if (Avant.Equals(TEXT("Tir"), ESearchCase::IgnoreCase))
			{
				R.TirsParDefaut.Add(Apres, TirDepuisTexte(Valeur));
			}
			else
			{
				const uint8 Tir = TirDepuisTexte(Avant);
				if (Tir == Aucun)
				{
					continue;
				}
				FCaracTir& C = R.Tirs[Tir];
				const float Nombre = FCString::Atof(*Valeur);
				if (Apres.Equals(TEXT("Degats"), ESearchCase::IgnoreCase)) C.Degats = FMath::Max(0.f, Nombre);
				else if (Apres.Equals(TEXT("Cadence"), ESearchCase::IgnoreCase)) C.Cadence = FMath::Clamp(Nombre, 0.05f, 10.f);
				else if (Apres.Equals(TEXT("Vitesse"), ESearchCase::IgnoreCase)) C.Vitesse = FMath::Clamp(Nombre, 300.f, 100000.f);
				else if (Apres.Equals(TEXT("DegatsExplosion"), ESearchCase::IgnoreCase)) C.DegatsZone = FMath::Max(0.f, Nombre);
				else if (Apres.Equals(TEXT("Rayon"), ESearchCase::IgnoreCase)) C.Rayon = FMath::Clamp(Nombre, 0.f, 5000.f);
				else if (Apres.Equals(TEXT("DureeGel"), ESearchCase::IgnoreCase)) C.DureeGel = FMath::Clamp(Nombre, 0.f, 30.f);
				else if (Apres.Equals(TEXT("Rebonds"), ESearchCase::IgnoreCase)) C.Rebonds = FMath::Clamp(FCString::Atoi(*Valeur), 0, 10);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[Armes] Reglages lus : %d apparences, %d tirs speciaux par defaut."), R.Apparences.Num(), R.TirsParDefaut.Num());
	}

	bool EcrireLigne(const FString& Cle, const FString& Valeur)
	{
		const FString Chemin = CheminReglages();
		TArray<FString> Lignes;
		FFileHelper::LoadFileToStringArray(Lignes, *Chemin);

		const FString Nouvelle = Cle + TEXT("=") + Valeur;
		bool bRemplacee = false;
		for (FString& L : Lignes)
		{
			FString K, V;
			if (!L.TrimStart().StartsWith(TEXT("#")) && L.Split(TEXT("="), &K, &V) && K.TrimStartAndEnd().Equals(Cle, ESearchCase::IgnoreCase))
			{
				L = Nouvelle;
				bRemplacee = true;
				break;
			}
		}
		if (!bRemplacee)
		{
			Lignes.Add(Nouvelle);
		}
		const FString Texte = FString::Join(Lignes, TEXT("\r\n")) + TEXT("\r\n");
		return FFileHelper::SaveStringToFile(Texte, *Chemin, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}
