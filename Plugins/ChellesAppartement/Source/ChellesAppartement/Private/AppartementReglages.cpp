// CHELLES 3D - Lecture de Appartement.txt (voir AppartementChelles.h)
#include "AppartementChelles.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FReglagesAppartement GReglagesAppart;
	bool GReglagesAppartLus = false;

	const TCHAR* GNomsTrophees[ChellesAppart::NombreTrophees] = {
		TEXT("Zombies"), TEXT("Missions"), TEXT("Vague"), TEXT("XP"), TEXT("Heures"), TEXT("Collection") };

	bool AppOui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("1")) || V.Equals(TEXT("Vrai"), ESearchCase::IgnoreCase) || V.Equals(TEXT("True"), ESearchCase::IgnoreCase);
	}

	TArray<FKey> AppTouches(const FString& V)
	{
		TArray<FKey> Sortie;
		TArray<FString> Morceaux;
		V.ParseIntoArray(Morceaux, TEXT(","), true);
		for (FString M : Morceaux)
		{
			M.TrimStartAndEndInline();
			const FKey K{ FName(*M) };
			if (K.IsValid())
			{
				Sortie.Add(K);
			}
			else if (!M.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Appartement] Touche inconnue : %s"), *M);
			}
		}
		return Sortie;
	}

	/** "0.3, 0.2, 0.1" ou "(R=0.3,G=0.2,B=0.1)" */
	bool AppCouleur(const FString& V, FLinearColor& Sortie)
	{
		if (V.Contains(TEXT("R=")))
		{
			return Sortie.InitFromString(V);
		}
		TArray<FString> M;
		V.ParseIntoArray(M, TEXT(","), true);
		if (M.Num() < 3)
		{
			return false;
		}
		Sortie = FLinearColor(FCString::Atof(*M[0]), FCString::Atof(*M[1]), FCString::Atof(*M[2]), 1.f);
		return true;
	}

	/** "Motoneige|MOTONEIGE, VOITURE|VOITURE" */
	TArray<FDepartAppartement> AppDeparts(const FString& V)
	{
		TArray<FDepartAppartement> Sortie;
		TArray<FString> Morceaux;
		V.ParseIntoArray(Morceaux, TEXT(","), true);
		for (FString M : Morceaux)
		{
			M.TrimStartAndEndInline();
			FDepartAppartement D;
			if (!M.Split(TEXT("|"), &D.Mode, &D.Nom))
			{
				D.Mode = M;
				D.Nom = M.ToUpper();
			}
			D.Mode.TrimStartAndEndInline();
			D.Nom.TrimStartAndEndInline();
			if (!D.Mode.IsEmpty())
			{
				Sortie.Add(D);
			}
			if (Sortie.Num() >= 5)
			{
				break;
			}
		}
		return Sortie;
	}

	void AppDefauts(FReglagesAppartement& R)
	{
		R = FReglagesAppartement();
		R.Touches = AppTouches(TEXT("F1"));
		R.Departs = AppDeparts(TEXT("Motoneige|EN MOTONEIGE, VOITURE|EN VOITURE, HORSE|A CHEVAL"));
		const int32 Defaut[ChellesAppart::NombreTrophees][3] = {
			{ 10, 100, 500 },   // zombies tues
			{ 1, 5, 20 },       // missions reussies
			{ 5, 10, 20 },      // meilleure vague du Refuge
			{ 1000, 5000, 20000 }, // XP
			{ 1, 5, 20 },       // heures de jeu
			{ 3, 8, 15 } };     // skins + vehicules ranges
		FMemory::Memcpy(R.Seuils, Defaut, sizeof(Defaut));
		R.SonTrophee = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_LapComplete_Cue.UI_LapComplete_Cue");
	}
}

const FReglagesAppartement& ReglagesAppartement()
{
	if (!GReglagesAppartLus)
	{
		RechargerReglagesAppartement();
	}
	return GReglagesAppart;
}

void RechargerReglagesAppartement()
{
	GReglagesAppartLus = true;
	FReglagesAppartement& R = GReglagesAppart;
	AppDefauts(R);

	const FString Fichier = FPaths::ProjectPluginsDir() / TEXT("ChellesAppartement/Appartement.txt");
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Appartement] %s introuvable : reglages par defaut"), *Fichier);
		return;
	}
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		FString Cle, V;
		if (!Ligne.Split(TEXT("="), &Cle, &V))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		V.TrimStartAndEndInline();
		auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };
		const float Nombre = FCString::Atof(*V);

		if (Est(TEXT("Actif"))) { R.bActif = AppOui(V); }
		else if (Est(TEXT("Touche")))
		{
			const TArray<FKey> T = AppTouches(V);
			if (T.Num() > 0)
			{
				R.Touches = T;
			}
		}
		else if (Est(TEXT("Hauteur"))) { R.Position.Z = Nombre; }
		else if (Est(TEXT("PositionX"))) { R.Position.X = Nombre; }
		else if (Est(TEXT("PositionY"))) { R.Position.Y = Nombre; }
		else if (Est(TEXT("Ecart"))) { R.Ecart = FMath::Max(3500.f, Nombre); }
		else if (Est(TEXT("Titre"))) { R.Titre = V.ToUpper(); }
		else if (Est(TEXT("AnimerMannequins"))) { R.bAnimerMannequins = AppOui(V); }
		else if (Est(TEXT("PlacesGardeRobe"))) { R.PlacesGardeRobe = FMath::Clamp(FCString::Atoi(*V), 1, 8); }
		else if (Est(TEXT("PlacesGarage"))) { R.PlacesGarage = FMath::Clamp(FCString::Atoi(*V), 1, 5); }
		else if (Est(TEXT("TailleMaxVehicule"))) { R.TailleMaxVehicule = FMath::Clamp(Nombre, 200.f, 850.f); }
		else if (Est(TEXT("Departs"))) { R.Departs = AppDeparts(V); }
		else if (Est(TEXT("Lumiere"))) { R.Lumiere = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("Messages"))) { R.bMessages = AppOui(V); }
		else if (Est(TEXT("SonTrophee"))) { R.SonTrophee = V; }
		else if (Est(TEXT("SonPorte"))) { R.SonPorte = V; }
		else if (Est(TEXT("CouleurSol"))) { AppCouleur(V, R.CouleurSol); }
		else if (Est(TEXT("CouleurMurs"))) { AppCouleur(V, R.CouleurMurs); }
		else if (Est(TEXT("CouleurGarage"))) { AppCouleur(V, R.CouleurGarage); }
		else if (Est(TEXT("CouleurNeon"))) { AppCouleur(V, R.CouleurNeon); }
		else
		{
			bool bTrouve = false;
			for (int32 i = 0; i < ChellesAppart::NombreTrophees && !bTrouve; ++i)
			{
				if (Cle.Equals(FString(TEXT("Trophee.")) + GNomsTrophees[i], ESearchCase::IgnoreCase))
				{
					TArray<FString> M;
					V.ParseIntoArray(M, TEXT(","), true);
					for (int32 j = 0; j < 3 && j < M.Num(); ++j)
					{
						R.Seuils[i][j] = FMath::Max(1, FCString::Atoi(*M[j].TrimStartAndEnd()));
					}
					// toujours croissants : bronze < argent < or
					R.Seuils[i][1] = FMath::Max(R.Seuils[i][1], R.Seuils[i][0] + 1);
					R.Seuils[i][2] = FMath::Max(R.Seuils[i][2], R.Seuils[i][1] + 1);
					bTrouve = true;
				}
			}
			if (!bTrouve)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Appartement] Ligne inconnue dans Appartement.txt : %s"), *Ligne);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Appartement] Reglages lus (%d touche(s), %d departs, %s)."),
		R.Touches.Num(), R.Departs.Num(), R.bActif ? TEXT("actif") : TEXT("coupe"));
}
