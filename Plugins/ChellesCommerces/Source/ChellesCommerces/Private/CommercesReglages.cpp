// CHELLES 3D - Commerces : lecture de Commerces.txt et articles par defaut
#include "CommercesTypes.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	using namespace ChellesCommerces;

	bool CommercesEstOui(const FString& Texte)
	{
		return Texte.Equals(TEXT("Oui"), ESearchCase::IgnoreCase) || Texte.Equals(TEXT("1")) || Texte.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| Texte.Equals(TEXT("Yes"), ESearchCase::IgnoreCase);
	}

	FArticle Article(const TCHAR* Nom, const TCHAR* Description, int32 Prix, uint8 Effet, float Valeur, float Duree)
	{
		FArticle A;
		A.Nom = Nom;
		A.Description = Description;
		A.Prix = Prix;
		A.Effet = Effet;
		A.Valeur = Valeur;
		A.Duree = Duree;
		return A;
	}

	void CommercesParDefaut(FReglagesCommerces& R)
	{
		R.Commerces.Reset();

		FCommerce B;
		B.Cle = TEXT("Boulangerie");
		B.Nom = TEXT("BOULANGERIE");
		B.Tag = FName(TEXT("ChellesComptoir_Boulangerie"));
		B.Couleur = FLinearColor(1.f, 0.74f, 0.27f);
		B.Articles.Add(Article(TEXT("Croissant"), TEXT("+25 vie"), 10, EffetVie, 25.f, 0.f));
		B.Articles.Add(Article(TEXT("Pain au chocolat"), TEXT("+50 vie"), 20, EffetVie, 50.f, 0.f));
		B.Articles.Add(Article(TEXT("Baguette tradition"), TEXT("Vie au maximum"), 35, EffetVie, 999.f, 0.f));
		B.Articles.Add(Article(TEXT("G\u00E2teau du quartier"), TEXT("+3 vie par seconde pendant 30 s"), 45, EffetRegen, 3.f, 30.f));
		R.Commerces.Add(B);

		FCommerce T;
		T.Cle = TEXT("Tabac");
		T.Nom = TEXT("TABAC - PRESSE");
		T.Tag = FName(TEXT("ChellesComptoir_Tabac"));
		T.Couleur = FLinearColor(1.f, 0.28f, 0.2f);
		T.Articles.Add(Article(TEXT("Le Journal de Chelles"), TEXT("+40 XP"), 15, EffetXP, 40.f, 0.f));
		T.Articles.Add(Article(TEXT("Carte \u00E0 gratter"), TEXT("Gagne jusqu'\u00E0 150 pi\u00E8ces... ou rien !"), 25, EffetGrattage, 150.f, 0.f));
		T.Articles.Add(Article(TEXT("Recharge de munitions"), TEXT("+150 munitions armes lourdes"), 40, EffetMunitions, 150.f, 0.f));
		T.Articles.Add(Article(TEXT("Magazine des h\u00E9ros"), TEXT("+120 XP"), 40, EffetXP, 120.f, 0.f));
		R.Commerces.Add(T);

		FCommerce Ba;
		Ba.Cle = TEXT("Bar");
		Ba.Nom = TEXT("CHELLES BAR");
		Ba.Tag = FName(TEXT("ChellesBarComptoir"));
		Ba.Couleur = FLinearColor(1.f, 0.24f, 0.67f);
		Ba.Articles.Add(Article(TEXT("Caf\u00E9 serr\u00E9"), TEXT("Course x1,4 pendant 45 s"), 15, EffetVitesse, 1.4f, 45.f));
		Ba.Articles.Add(Article(TEXT("Soda \u00E9nergie"), TEXT("Saut x1,6 pendant 45 s"), 20, EffetSaut, 1.6f, 45.f));
		Ba.Articles.Add(Article(TEXT("Diabolo menthe"), TEXT("+40 vie"), 15, EffetVie, 40.f, 0.f));
		Ba.Articles.Add(Article(TEXT("Cocktail maison (sans alcool)"), TEXT("Bouclier : 50 % des d\u00E9g\u00E2ts rendus pendant 60 s"), 60, EffetBouclier, 50.f, 60.f));
		R.Commerces.Add(Ba);
	}

	FCommerce* TrouverCommerce(FReglagesCommerces& R, const FString& Cle)
	{
		for (FCommerce& C : R.Commerces)
		{
			if (C.Cle.Equals(Cle, ESearchCase::IgnoreCase))
			{
				return &C;
			}
		}
		return nullptr;
	}
}

namespace ChellesCommerces
{
	uint8 EffetDepuisTexte(const FString& Texte)
	{
		const FString T = Texte.TrimStartAndEnd().ToUpper();
		if (T == TEXT("VIE")) return EffetVie;
		if (T == TEXT("REGEN") || T == TEXT("REGENERATION")) return EffetRegen;
		if (T == TEXT("VITESSE") || T == TEXT("COURSE")) return EffetVitesse;
		if (T == TEXT("SAUT")) return EffetSaut;
		if (T == TEXT("BOUCLIER")) return EffetBouclier;
		if (T == TEXT("XP")) return EffetXP;
		if (T == TEXT("MUNITIONS")) return EffetMunitions;
		if (T == TEXT("GRATTAGE") || T == TEXT("GRATTER")) return EffetGrattage;
		return EffetInconnu;
	}

	FString TexteEffet(const FArticle& A)
	{
		switch (A.Effet)
		{
		case EffetVie: return A.Valeur >= 999.f ? FString(TEXT("Vie au maximum")) : FString::Printf(TEXT("+%d vie"), FMath::RoundToInt(A.Valeur));
		case EffetRegen: return FString::Printf(TEXT("+%d vie/s pendant %d s"), FMath::RoundToInt(A.Valeur), FMath::RoundToInt(A.Duree));
		case EffetVitesse: return FString::Printf(TEXT("Course x%.1f pendant %d s"), A.Valeur, FMath::RoundToInt(A.Duree));
		case EffetSaut: return FString::Printf(TEXT("Saut x%.1f pendant %d s"), A.Valeur, FMath::RoundToInt(A.Duree));
		case EffetBouclier: return FString::Printf(TEXT("Bouclier %d %% pendant %d s"), FMath::RoundToInt(A.Valeur), FMath::RoundToInt(A.Duree));
		case EffetXP: return FString::Printf(TEXT("+%d XP"), FMath::RoundToInt(A.Valeur));
		case EffetMunitions: return FString::Printf(TEXT("+%d munitions"), FMath::RoundToInt(A.Valeur));
		case EffetGrattage: return FString::Printf(TEXT("Jusqu'\u00E0 %d pi\u00E8ces"), FMath::RoundToInt(A.Valeur));
		default: return FString();
		}
	}

	void ChargerReglages(FReglagesCommerces& R)
	{
		CommercesParDefaut(R);
		const FString Chemin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("ChellesCommerces"), TEXT("Commerces.txt"));
		TArray<FString> Lignes;
		if (!FFileHelper::LoadFileToStringArray(Lignes, *Chemin))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Commerces] %s introuvable : reglages par defaut."), *Chemin);
			return;
		}
		TSet<FString> Remplaces; // commerces dont les articles viennent du fichier
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

			if (Est(TEXT("Pieces.Depart"))) R.PiecesDepart = FMath::Clamp(I, 0, 100000);
			else if (Est(TEXT("Pieces.ParZombie"))) R.ParZombie = FMath::Clamp(I, 0, 1000);
			else if (Est(TEXT("Pieces.ParEnnemi"))) R.ParEnnemi = FMath::Clamp(I, 0, 1000);
			else if (Est(TEXT("Pieces.ParXP"))) R.ParXP = FMath::Clamp(N, 0.f, 100.f);
			else if (Est(TEXT("Pieces.ParMinute"))) R.ParMinute = FMath::Clamp(N, 0.f, 1000.f);
			else if (Est(TEXT("Pieces.Afficher"))) R.bAfficherPieces = CommercesEstOui(Valeur);
			else if (Est(TEXT("CadeauJour.Actif"))) R.bCadeauJour = CommercesEstOui(Valeur);
			else if (Est(TEXT("CadeauJour.Base"))) R.CadeauBase = FMath::Clamp(I, 0, 100000);
			else if (Est(TEXT("CadeauJour.ParJour"))) R.CadeauParJour = FMath::Clamp(I, 0, 100000);
			else if (Est(TEXT("CadeauJour.Max"))) R.CadeauMax = FMath::Clamp(I, 0, 100000);
			else if (Est(TEXT("WeekEnd.Multiplicateur"))) R.MultiWeekEnd = FMath::Clamp(N, 1.f, 10.f);
			else if (Est(TEXT("Boutique.Distance"))) R.Distance = FMath::Clamp(N, 0.5f, 20.f) * 100.f;
			else if (Est(TEXT("Son.Achat"))) R.SonAchat = Valeur;
			else if (Est(TEXT("Son.Pieces"))) R.SonPieces = Valeur;
			else if (Cle.StartsWith(TEXT("Commerce."), ESearchCase::IgnoreCase))
			{
				// Commerce.Boulangerie.Nom=... / Commerce.Boulangerie.Tag=...
				TArray<FString> Morceaux;
				Cle.ParseIntoArray(Morceaux, TEXT("."));
				if (Morceaux.Num() == 3)
				{
					if (FCommerce* C = TrouverCommerce(R, Morceaux[1]))
					{
						if (Morceaux[2].Equals(TEXT("Nom"), ESearchCase::IgnoreCase)) C->Nom = Valeur;
						else if (Morceaux[2].Equals(TEXT("Tag"), ESearchCase::IgnoreCase)) C->Tag = FName(*Valeur);
					}
				}
			}
			else if (Est(TEXT("Article")))
			{
				// Article = Commerce | Nom | Prix | EFFET | Valeur | Duree
				TArray<FString> Champs;
				Valeur.ParseIntoArray(Champs, TEXT("|"), false);
				for (FString& Ch : Champs)
				{
					Ch.TrimStartAndEndInline();
				}
				if (Champs.Num() < 5)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Commerces] ligne ignoree (il faut 5 ou 6 morceaux separes par |) : %s"), *Ligne);
					continue;
				}
				FCommerce* C = TrouverCommerce(R, Champs[0]);
				if (!C)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Commerces] commerce inconnu \"%s\" (Boulangerie, Tabac ou Bar)."), *Champs[0]);
					continue;
				}
				FArticle A;
				A.Nom = Champs[1];
				A.Prix = FMath::Clamp(FCString::Atoi(*Champs[2]), 0, 100000);
				A.Effet = EffetDepuisTexte(Champs[3]);
				A.Valeur = FCString::Atof(*Champs[4].Replace(TEXT(","), TEXT(".")));
				A.Duree = Champs.Num() > 5 ? FMath::Clamp(FCString::Atof(*Champs[5].Replace(TEXT(","), TEXT("."))), 0.f, 3600.f) : 0.f;
				if (A.Effet == EffetInconnu)
				{
					UE_LOG(LogTemp, Warning, TEXT("[Commerces] effet inconnu \"%s\" (VIE, REGEN, VITESSE, SAUT, BOUCLIER, XP, MUNITIONS, GRATTAGE)."), *Champs[3]);
					continue;
				}
				if (A.Effet == EffetVitesse || A.Effet == EffetSaut)
				{
					A.Valeur = FMath::Clamp(A.Valeur, 0.5f, 3.f);
				}
				A.Description = TexteEffet(A);
				if (!Remplaces.Contains(C->Cle))
				{
					Remplaces.Add(C->Cle);
					C->Articles.Reset();
				}
				if (C->Articles.Num() < 8)
				{
					C->Articles.Add(A);
				}
			}
		}
	}
}
