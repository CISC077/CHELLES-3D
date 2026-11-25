// CHELLES 3D - Ecran de chargement : lecture de Astuces.txt
#include "ReglagesChargement.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FReglagesChargement& FReglagesChargement::Get()
{
	static FReglagesChargement Instance;
	return Instance;
}

void FReglagesChargement::AstucesParDefaut()
{
	struct FDefaut { const TCHAR* Touche; const TCHAR* Texte; };
	static const FDefaut Liste[] =
	{
		{ TEXT("P"), TEXT("Utilise le POUVOIR de ton personnage.") },
		{ TEXT("J"), TEXT("Danse ! Chaque appui lance la danse suivante. Bouge pour arrêter.") },
		{ TEXT("I"), TEXT("Ouvre l'inventaire ARMEMENTS pour choisir ton arme.") },
		{ TEXT("²"), TEXT("Ouvre la console (la touche juste sous Échap).") },
		{ TEXT("F11"), TEXT("Passe en plein écran, ou reviens en fenêtre.") },
		{ TEXT("TAB"), TEXT("La roue des armes : change d'arme en un clin d'œil.") },
		{ TEXT("ESPACE"), TEXT("Tout le monde vole : appuie sur ESPACE pour t'envoler.") },
		{ TEXT("ÉCHAP"), TEXT("Menu PAUSE : options, habillage et quitter.") },
		{ TEXT("B"), TEXT("Chat global : parle avec les autres joueurs.") },
		{ TEXT("E"), TEXT("Ouvre les coffres, les portes du bus et les râteliers d'armes.") },
		{ TEXT("H"), TEXT("Klaxonne quand tu conduis.") },
		{ TEXT("MOLETTE"), TEXT("Clic molette : signale un objet à tes amis.") },
		{ TEXT(""), TEXT("Toutes les 2 heures, les zombies envahissent Chelles : trouve un abri !") },
		{ TEXT(""), TEXT("Plus tu joues, plus tu gagnes d'XP et de nouveaux pouvoirs.") },
		{ TEXT(""), TEXT("Change de personnage et de tenue dans le lobby HABILLAGE.") },
	};
	Astuces.Reset();
	for (const FDefaut& D : Liste)
	{
		FAstuceChelles A;
		A.Touche = D.Touche;
		A.Texte = D.Texte;
		Astuces.Add(A);
	}
}

void FReglagesChargement::Charger()
{
	DureeMini = 2.5f;
	DureeMaxi = 10.f;
	SecondesParAstuce = 4.f;
	bChargementCarte = true;
	bDebutPartie = true;
	AstucesParDefaut();

	FString Chemin;
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ChellesChargement")))
	{
		Chemin = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Astuces.txt"));
	}
	FString Contenu;
	if (Chemin.IsEmpty() || !FFileHelper::LoadFileToString(Contenu, *Chemin))
	{
		return; // jeu empaquete sans le fichier : astuces par defaut
	}

	TArray<FString> Lignes;
	Contenu.ParseIntoArrayLines(Lignes, false);
	TArray<FAstuceChelles> Lues;
	bool bDansAstuces = false;
	for (FString Ligne : Lignes)
	{
		Ligne.TrimStartAndEndInline();
		if (Ligne.IsEmpty() || Ligne.StartsWith(TEXT("#")) || Ligne.StartsWith(TEXT(";")))
		{
			continue;
		}
		if (Ligne.StartsWith(TEXT("[")))
		{
			bDansAstuces = Ligne.Equals(TEXT("[Astuces]"), ESearchCase::IgnoreCase);
			continue;
		}
		if (bDansAstuces)
		{
			FAstuceChelles A;
			FString Gauche, Droite;
			if (Ligne.Split(TEXT("|"), &Gauche, &Droite))
			{
				A.Touche = Gauche.TrimStartAndEnd();
				A.Texte = Droite.TrimStartAndEnd();
			}
			else
			{
				A.Texte = Ligne;
			}
			if (!A.Texte.IsEmpty())
			{
				Lues.Add(A);
			}
			continue;
		}
		FString Cle, Valeur;
		if (!Ligne.Split(TEXT("="), &Cle, &Valeur))
		{
			continue;
		}
		Cle.TrimStartAndEndInline();
		Valeur.TrimStartAndEndInline();
		Valeur.ReplaceInline(TEXT(","), TEXT("."));
		const bool bOui = !(Valeur == TEXT("0") || Valeur.Equals(TEXT("non"), ESearchCase::IgnoreCase));
		if (Cle.Equals(TEXT("DureeMini"), ESearchCase::IgnoreCase))
		{
			DureeMini = FMath::Clamp(FCString::Atof(*Valeur), 0.f, 30.f);
		}
		else if (Cle.Equals(TEXT("DureeMaxi"), ESearchCase::IgnoreCase))
		{
			DureeMaxi = FMath::Clamp(FCString::Atof(*Valeur), 1.f, 120.f);
		}
		else if (Cle.Equals(TEXT("SecondesParAstuce"), ESearchCase::IgnoreCase))
		{
			SecondesParAstuce = FMath::Clamp(FCString::Atof(*Valeur), 1.5f, 60.f);
		}
		else if (Cle.Equals(TEXT("ChargementCarte"), ESearchCase::IgnoreCase))
		{
			bChargementCarte = bOui;
		}
		else if (Cle.Equals(TEXT("DebutPartie"), ESearchCase::IgnoreCase))
		{
			bDebutPartie = bOui;
		}
	}
	DureeMaxi = FMath::Max(DureeMaxi, DureeMini);
	if (Lues.Num() > 0)
	{
		Astuces = MoveTemp(Lues);
	}
}
