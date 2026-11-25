// CHELLES 3D - Lecture de Accessoires.txt (voir AccessoiresChelles.h)
#include "AccessoiresChelles.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FReglagesAccessoires GReglagesAccessoires;
	bool GReglagesAccessoiresLus = false;

	const TCHAR* GAccNoms[ChellesAccessoires::Nombre] = { TEXT("Lampe"), TEXT("Jumelles"), TEXT("Grappin"), TEXT("Kit") };

	bool AccOui(const FString& V)
	{
		return V.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || V.Equals(TEXT("Yes"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("1")) || V.Equals(TEXT("Vrai"), ESearchCase::IgnoreCase) || V.Equals(TEXT("True"), ESearchCase::IgnoreCase)
			|| V.Equals(TEXT("Depart"), ESearchCase::IgnoreCase);
	}

	/** "Seven, E_AccentGrave" -> les touches valides */
	TArray<FKey> AccLireTouches(const FString& V)
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
				UE_LOG(LogTemp, Warning, TEXT("[Accessoires] Touche inconnue : %s"), *M);
			}
		}
		return Sortie;
	}

	void AccDefauts(FReglagesAccessoires& R)
	{
		R = FReglagesAccessoires();
		// 7 8 9 0 : QWERTY + AZERTY (sans Maj)
		const TCHAR* Defaut[ChellesAccessoires::Nombre] = {
			TEXT("Seven, E_AccentGrave"),
			TEXT("Eight, Underscore"),
			TEXT("Nine, C_Cedille"),
			TEXT("Zero, A_AccentGrave") };
		for (int32 i = 0; i < ChellesAccessoires::Nombre; ++i)
		{
			R.Touches[i] = AccLireTouches(Defaut[i]);
		}
		const FString Dossier = TEXT("/Game/CraftResourcesIcons/Textures/");
		R.Icones[ChellesAccessoires::Lampe] = Dossier + TEXT("Tex_tools_08.Tex_tools_08");
		R.Icones[ChellesAccessoires::Jumelles] = Dossier + TEXT("Tex_spare_parts_03.Tex_spare_parts_03");
		R.Icones[ChellesAccessoires::Grappin] = Dossier + TEXT("Tex_tools_05.Tex_tools_05");
		R.Icones[ChellesAccessoires::Kit] = Dossier + TEXT("Tex_container_06.Tex_container_06");
		R.MenusInventaire = { TEXT("SMenuArmements"), TEXT("W_WeaponInventory"), TEXT("WBP_Inventory") };
		R.SonGain = TEXT("/Game/Sounds/InterfaceAudio/Mono/UI_LapComplete_Cue.UI_LapComplete_Cue");
	}
}

const FReglagesAccessoires& ReglagesAccessoires()
{
	if (!GReglagesAccessoiresLus)
	{
		RechargerReglagesAccessoires();
	}
	return GReglagesAccessoires;
}

void RechargerReglagesAccessoires()
{
	GReglagesAccessoiresLus = true;
	FReglagesAccessoires& R = GReglagesAccessoires;
	AccDefauts(R);

	const FString Fichier = FPaths::ProjectPluginsDir() / TEXT("ChellesAccessoires/Accessoires.txt");
	TArray<FString> Lignes;
	if (!FFileHelper::LoadFileToStringArray(Lignes, *Fichier))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Accessoires] %s introuvable : reglages par defaut"), *Fichier);
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
		auto Est = [&Cle](const TCHAR* Nom) { return Cle.Equals(Nom, ESearchCase::IgnoreCase); };
		const float Nombre = FCString::Atof(*Valeur);

		if (Est(TEXT("Actif"))) { R.bActif = AccOui(Valeur); continue; }

		bool bTrouve = false;
		for (int32 i = 0; i < ChellesAccessoires::Nombre && !bTrouve; ++i)
		{
			const FString Base = GAccNoms[i];
			if (Cle.Equals(Base + TEXT(".Touche"), ESearchCase::IgnoreCase)) { R.Touches[i] = AccLireTouches(Valeur); bTrouve = true; }
			else if (Cle.Equals(Base + TEXT(".Depart"), ESearchCase::IgnoreCase)) { R.bDepart[i] = AccOui(Valeur); bTrouve = true; }
			else if (Cle.Equals(Base + TEXT(".Icone"), ESearchCase::IgnoreCase)) { R.Icones[i] = Valeur; bTrouve = true; }
		}
		if (bTrouve)
		{
			continue;
		}
		if (Est(TEXT("Lampe.Intensite"))) { R.LampeIntensite = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("Lampe.Angle"))) { R.LampeAngle = FMath::Clamp(Nombre, 5.f, 80.f); }
		else if (Est(TEXT("Lampe.Portee"))) { R.LampePortee = FMath::Max(100.f, Nombre * 100.f); }
		else if (Est(TEXT("Lampe.Ombres"))) { R.bLampeOmbres = AccOui(Valeur); }
		else if (Est(TEXT("Lampe.Couleur")))
		{
			FColor C = FColor::FromHex(Valeur);
			R.LampeCouleur = FLinearColor::FromSRGBColor(C);
		}
		else if (Est(TEXT("Jumelles.Zoom1"))) { R.ZoomJumelles1 = FMath::Clamp(Nombre, 1.5f, 20.f); }
		else if (Est(TEXT("Jumelles.Zoom2"))) { R.ZoomJumelles2 = FMath::Clamp(Nombre, 1.5f, 30.f); }
		else if (Est(TEXT("Jumelles.Cache"))) { R.bCacheJumelles = AccOui(Valeur); }
		else if (Est(TEXT("Grappin.Portee"))) { R.GrappinPortee = FMath::Clamp(Nombre, 5.f, 300.f); }
		else if (Est(TEXT("Grappin.Vitesse"))) { R.GrappinVitesse = FMath::Clamp(Nombre * 100.f, 500.f, 10000.f); }
		else if (Est(TEXT("Grappin.Attente"))) { R.GrappinAttente = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("Kit.Vie"))) { R.SoinVie = FMath::Max(1.f, Nombre); }
		else if (Est(TEXT("Kit.Attente"))) { R.SoinAttente = FMath::Max(0.f, Nombre); }
		else if (Est(TEXT("Kit.AuDepart"))) { R.KitsDepart = FMath::Max(0, FMath::RoundToInt(Nombre)); }
		else if (Est(TEXT("Kit.Max"))) { R.KitsMax = FMath::Max(1, FMath::RoundToInt(Nombre)); }
		else if (Est(TEXT("Butin"))) { R.bButin = AccOui(Valeur); }
		else if (Est(TEXT("Butin.Distance"))) { R.DistanceCoffre = FMath::Max(1.f, Nombre); }
		else if (Est(TEXT("Butin.ChanceKitCommun"))) { R.ChanceKit[0] = FMath::RoundToInt(Nombre); }
		else if (Est(TEXT("Butin.ChanceKitRare"))) { R.ChanceKit[1] = FMath::RoundToInt(Nombre); }
		else if (Est(TEXT("Butin.ChanceKitLegendaire"))) { R.ChanceKit[2] = FMath::RoundToInt(Nombre); }
		else if (Est(TEXT("Butin.KitsCommun"))) { R.KitsParCoffre[0] = FMath::Max(0, FMath::RoundToInt(Nombre)); }
		else if (Est(TEXT("Butin.KitsRare"))) { R.KitsParCoffre[1] = FMath::Max(0, FMath::RoundToInt(Nombre)); }
		else if (Est(TEXT("Butin.KitsLegendaire"))) { R.KitsParCoffre[2] = FMath::Max(0, FMath::RoundToInt(Nombre)); }
		else if (Est(TEXT("Butin.ChanceAccessoireCommun"))) { R.ChanceAccessoire[0] = FMath::RoundToInt(Nombre); }
		else if (Est(TEXT("Butin.ChanceAccessoireRare"))) { R.ChanceAccessoire[1] = FMath::RoundToInt(Nombre); }
		else if (Est(TEXT("Butin.ChanceAccessoireLegendaire"))) { R.ChanceAccessoire[2] = FMath::RoundToInt(Nombre); }
		else if (Est(TEXT("Case"))) { R.PlaceCase = Valeur; }
		else if (Est(TEXT("MenusInventaire")))
		{
			R.MenusInventaire.Reset();
			Valeur.ParseIntoArray(R.MenusInventaire, TEXT(","), true);
			for (FString& M : R.MenusInventaire)
			{
				M.TrimStartAndEndInline();
			}
		}
		else if (Est(TEXT("Son.Gain"))) { R.SonGain = Valeur; }
		else if (Est(TEXT("Son.Soin"))) { R.SonSoin = Valeur; }
	}
	R.KitsDepart = FMath::Min(R.KitsDepart, R.KitsMax);
	UE_LOG(LogTemp, Log, TEXT("[Accessoires] Reglages lus (%s)"), *Fichier);
}
