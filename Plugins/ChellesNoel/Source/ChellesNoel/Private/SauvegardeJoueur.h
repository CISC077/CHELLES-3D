// CHELLES 3D - Sauvegarde de la progression du joueur (UE 4.27)
// Fichier identique dans ChellesPouvoirs et ChellesNoel (un plugin ne peut pas dependre d'un autre) :
// si vous le changez, changez-le aux deux endroits.
//
// - Une sauvegarde PAR JOUEUR, sur le PC de ce joueur (Saved\SaveGames\<Nom>.sav).
// - Deux joueurs sur le meme PC (test multi dans l'editeur, ou deux jeux lances avec -Joueur=Nom)
//   ont chacun leur fichier : ChellesXP.sav, ChellesXP_PIE2.sav, ChellesXP_Karim.sav...
// - Copie de secours <Nom>_Secours.sav : si le jeu plante pendant l'ecriture, rien n'est perdu.
#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"

namespace ChellesSauvegarde
{
	/** Nom du fichier de sauvegarde de CE joueur (sans .sav). */
	inline FString Emplacement(const UObject* Contexte, const TCHAR* Base)
	{
		FString Nom = Base;

		// Jeu lance avec  -Joueur=Karim  : sauvegarde a son nom
		FString Joueur;
		if (FParse::Value(FCommandLine::Get(), TEXT("Joueur="), Joueur))
		{
			FString Propre;
			for (const TCHAR C : Joueur)
			{
				if (FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-'))
				{
					Propre.AppendChar(C);
				}
			}
			if (!Propre.IsEmpty())
			{
				return Nom + TEXT("_") + Propre.Left(32);
			}
		}

		// Test multi dans l'editeur : la 1re fenetre garde la vraie sauvegarde, les autres ont la leur
		const UWorld* Monde = Contexte ? Contexte->GetWorld() : nullptr;
		if (Monde && Monde->WorldType == EWorldType::PIE)
		{
			const int32 Instance = Monde->GetOutermost()->PIEInstanceID;
			if (Instance > 0)
			{
				Nom += FString::Printf(TEXT("_PIE%d"), Instance + 1);
			}
		}
		return Nom;
	}

	/** Ecrit la sauvegarde puis sa copie de secours. */
	inline bool Ecrire(USaveGame* Donnees, const FString& Nom)
	{
		if (!Donnees || Nom.IsEmpty())
		{
			return false;
		}
		const bool bOk = UGameplayStatics::SaveGameToSlot(Donnees, Nom, 0);
		if (bOk)
		{
			UGameplayStatics::SaveGameToSlot(Donnees, Nom + TEXT("_Secours"), 0);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Sauvegarde] impossible d'ecrire %s.sav (disque plein ou dossier protege ?)"), *Nom);
		}
		return bOk;
	}

	/** Lit la sauvegarde ; si elle est absente ou abimee, prend la copie de secours. */
	template <typename T>
	T* Lire(const FString& Nom)
	{
		if (UGameplayStatics::DoesSaveGameExist(Nom, 0))
		{
			if (T* Donnees = Cast<T>(UGameplayStatics::LoadGameFromSlot(Nom, 0)))
			{
				return Donnees;
			}
			UE_LOG(LogTemp, Warning, TEXT("[Sauvegarde] %s.sav abime : lecture de la copie de secours"), *Nom);
		}
		const FString Secours = Nom + TEXT("_Secours");
		if (UGameplayStatics::DoesSaveGameExist(Secours, 0))
		{
			return Cast<T>(UGameplayStatics::LoadGameFromSlot(Secours, 0));
		}
		return nullptr;
	}
}
