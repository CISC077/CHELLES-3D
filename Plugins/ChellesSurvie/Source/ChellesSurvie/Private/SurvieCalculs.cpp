// CHELLES 3D - Modes de survie : calculs purs (courbe des vagues, points, catalogue)
#include "SurvieCalculs.h"

#include "Misc/Paths.h"

// ===================================================================================
//  Lecture des fichiers de reglages
// ===================================================================================

bool FLigneReglage::Decouper(const FString& LigneBrute, FString& Cle, FString& Valeur)
{
	FString Ligne = LigneBrute;
	Ligne.TrimStartAndEndInline();
	if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
	{
		return false;
	}
	if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
	{
		return false;
	}
	Cle.TrimStartAndEndInline();
	Valeur.TrimStartAndEndInline();
	return !Cle.IsEmpty();
}

float FLigneReglage::Nombre(const FString& Valeur)
{
	// Accepte la virgule francaise : 0,5 = 0.5
	return FCString::Atof(*Valeur.Replace(TEXT(","), TEXT(".")));
}

bool FLigneReglage::EstOui(const FString& Valeur)
{
	return Valeur.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Valeur.Equals(TEXT("1"))
		|| Valeur.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Valeur.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
}

// ===================================================================================
//  Courbe des vagues
// ===================================================================================

void FCourbeVagues::Lire(const TArray<FString>& Lignes)
{
	for (const FString& Brute : Lignes)
	{
		FString Cle, Valeur;
		if (!FLigneReglage::Decouper(Brute, Cle, Valeur))
		{
			continue;
		}
		const float N = FLigneReglage::Nombre(Valeur);
		if (Cle == TEXT("ZombiesVague1")) ZombiesVague1 = FMath::Clamp((int32)N, 1, 300);
		else if (Cle == TEXT("ZombiesEnPlusParVague")) ZombiesEnPlusParVague = FMath::Clamp(N, 0.f, 100.f);
		else if (Cle == TEXT("ZombiesEnPlusParJoueur")) EnPlusParJoueur = FMath::Clamp(N, 0.f, 5.f);
		else if (Cle == TEXT("ZombiesMax")) ZombiesMax = FMath::Clamp((int32)N, 1, 300);
		else if (Cle == TEXT("VieVague1")) VieVague1 = FMath::Clamp(N, 1.f, 10000.f);
		else if (Cle == TEXT("VieEnPlusParVague")) VieEnPlusParVague = FMath::Clamp(N, 0.f, 1000.f);
		else if (Cle == TEXT("VieMax")) VieMax = FMath::Clamp(N, 1.f, 10000.f);
		else if (Cle == TEXT("VitesseVague1")) VitesseVague1 = FMath::Clamp(N, 50.f, 2000.f);
		else if (Cle == TEXT("VitesseEnPlusParVague")) VitesseEnPlusParVague = FMath::Clamp(N, 0.f, 500.f);
		else if (Cle == TEXT("VitesseMax")) VitesseMax = FMath::Clamp(N, 50.f, 2000.f);
		else if (Cle == TEXT("CostaudsAPartirDeLaVague")) CostaudsAPartirDe = FMath::Clamp((int32)N, 1, 1000);
		else if (Cle == TEXT("PartCostaudsDepart")) PartCostaudsDepart = FMath::Clamp(N, 0.f, 1.f);
		else if (Cle == TEXT("PartCostaudsEnPlusParVague")) PartCostaudsEnPlusParVague = FMath::Clamp(N, 0.f, 1.f);
		else if (Cle == TEXT("PartCostaudsMax")) PartCostaudsMax = FMath::Clamp(N, 0.f, 1.f);
		else if (Cle == TEXT("VieCostaudFois")) VieCostaudFois = FMath::Clamp(N, 1.f, 20.f);
		else if (Cle == TEXT("VagueCoureursToutesLes")) CoureursToutesLes = FMath::Clamp((int32)N, 0, 1000);
		else if (Cle == TEXT("CoureursVitesseFois")) CoureursVitesseFois = FMath::Clamp(N, 1.f, 3.f);
		else if (Cle == TEXT("PauseEntreVaguesSecondes")) PauseEntreVaguesSecondes = FMath::Clamp(N, 3.f, 300.f);
	}
	VieMax = FMath::Max(VieMax, VieVague1);
	VitesseMax = FMath::Max(VitesseMax, VitesseVague1);
	ZombiesMax = FMath::Max(ZombiesMax, ZombiesVague1);
}

FVagueCalculee FCourbeVagues::Calculer(int32 Vague, int32 NombreJoueurs) const
{
	FVagueCalculee R;
	R.Numero = FMath::Max(1, Vague);
	const int32 Montee = R.Numero - 1; // vague 1 = valeurs de base
	const int32 JoueursEnPlus = FMath::Max(0, NombreJoueurs - 1);

	const float Base = (float)ZombiesVague1 + ZombiesEnPlusParVague * Montee;
	const float AvecJoueurs = Base * (1.f + EnPlusParJoueur * JoueursEnPlus);
	R.Nombre = FMath::Clamp(FMath::RoundToInt(AvecJoueurs), 1, ZombiesMax);

	R.Vie = FMath::Min(VieMax, VieVague1 + VieEnPlusParVague * Montee);
	R.Vitesse = FMath::Min(VitesseMax, VitesseVague1 + VitesseEnPlusParVague * Montee);

	if (R.Numero >= CostaudsAPartirDe)
	{
		R.PartCostauds = FMath::Min(PartCostaudsMax, PartCostaudsDepart + PartCostaudsEnPlusParVague * (R.Numero - CostaudsAPartirDe));
	}
	R.VieCostaud = R.Vie * VieCostaudFois;

	R.bCoureurs = CoureursToutesLes > 0 && R.Numero % CoureursToutesLes == 0;
	if (R.bCoureurs)
	{
		// Les coureurs peuvent depasser le plafond de vitesse normal : c'est le but de la vague
		R.Vitesse = R.Vitesse * CoureursVitesseFois;
	}
	return R;
}

// ===================================================================================
//  Economie
// ===================================================================================

void FEconomieRefuge::Lire(const TArray<FString>& Lignes)
{
	for (const FString& Brute : Lignes)
	{
		FString Cle, Valeur;
		if (!FLigneReglage::Decouper(Brute, Cle, Valeur))
		{
			continue;
		}
		const float N = FLigneReglage::Nombre(Valeur);
		if (Cle == TEXT("PointsDepart")) PointsDepart = FMath::Clamp((int32)N, 0, 1000000);
		else if (Cle == TEXT("PointsParTouche")) PointsParTouche = FMath::Clamp((int32)N, 0, 10000);
		else if (Cle == TEXT("PointsParKill")) PointsParKill = FMath::Clamp((int32)N, 0, 10000);
		else if (Cle == TEXT("PointsBonusTete")) PointsBonusTete = FMath::Clamp((int32)N, 0, 10000);
		else if (Cle == TEXT("PointsRelever")) PointsRelever = FMath::Clamp((int32)N, 0, 10000);
		else if (Cle == TEXT("PerteATerrePourcent")) PerteATerre = FMath::Clamp(N, 0.f, 100.f) / 100.f;
		else if (Cle == TEXT("XPParKill")) XPParKill = FMath::Clamp((int32)N, 0, 1000);
		else if (Cle == TEXT("XPBonusTete")) XPBonusTete = FMath::Clamp((int32)N, 0, 1000);
	}
}

int32 FEconomieRefuge::PointsPourCoup(bool bMort, bool bTete) const
{
	if (!bMort)
	{
		return PointsParTouche;
	}
	return PointsParKill + (bTete ? PointsBonusTete : 0);
}

int32 FEconomieRefuge::XPPourKill(bool bTete) const
{
	return XPParKill + (bTete ? XPBonusTete : 0);
}

int32 FEconomieRefuge::PerteQuandATerre(int32 Points) const
{
	return FMath::Max(0, FMath::FloorToInt(Points * PerteATerre));
}

bool FEconomieRefuge::Payer(int32& Points, int32 Prix)
{
	if (Prix < 0 || Points < Prix)
	{
		return false;
	}
	Points -= Prix;
	return true;
}

// ===================================================================================
//  Catalogue
// ===================================================================================

void FCatalogueRefuge::Lire(const TArray<FString>& Lignes)
{
	for (const FString& Brute : Lignes)
	{
		FString Cle, Valeur;
		if (!FLigneReglage::Decouper(Brute, Cle, Valeur))
		{
			continue;
		}
		if (Cle.StartsWith(TEXT("Arme.")))
		{
			FString Classe, Prix;
			if (!Valeur.Split(TEXT("|"), &Classe, &Prix))
			{
				continue;
			}
			FArticleRefuge Article;
			Article.Nom = Cle.Mid(5).TrimStartAndEnd();
			Article.Classe = Classe.TrimStartAndEnd();
			Article.Prix = FMath::Max(0, (int32)FLigneReglage::Nombre(Prix.TrimStartAndEnd()));
			if (Article.Nom.IsEmpty() || Article.Classe.IsEmpty())
			{
				continue;
			}
			// Une arme deja listee est remplacee (la derniere ligne gagne)
			Armes.RemoveAll([&Article](const FArticleRefuge& A) { return A.Nom.Equals(Article.Nom, ESearchCase::IgnoreCase); });
			Armes.Add(Article);
		}
		else if (Cle == TEXT("PrixMunitions")) PrixMunitions = FMath::Max(0, (int32)FLigneReglage::Nombre(Valeur));
		else if (Cle == TEXT("PrixBarriere")) PrixBarriere = FMath::Max(0, (int32)FLigneReglage::Nombre(Valeur));
	}
}

const FArticleRefuge* FCatalogueRefuge::Trouver(const FString& Nom) const
{
	return Armes.FindByPredicate([&Nom](const FArticleRefuge& A) { return A.Nom.Equals(Nom, ESearchCase::IgnoreCase); });
}

FString FCatalogueRefuge::CheminClasse(const FString& NomOuChemin)
{
	if (NomOuChemin.StartsWith(TEXT("/")))
	{
		if (NomOuChemin.Contains(TEXT(".")))
		{
			return NomOuChemin.EndsWith(TEXT("_C")) ? NomOuChemin : NomOuChemin + TEXT("_C");
		}
		const FString Nom = FPaths::GetBaseFilename(NomOuChemin);
		return FString::Printf(TEXT("%s.%s_C"), *NomOuChemin, *Nom);
	}
	return FString::Printf(TEXT("/Game/FPS_Game/Blueprints/Weapons/%s.%s_C"), *NomOuChemin, *NomOuChemin);
}
