// CHELLES 3D - Missions : lecture / ecriture des variables des autres plugins (ChellesZombies, ChellesTrain,
// ChellesSurvie, ChellesPouvoirs) PAR LEUR NOM, sans les modifier et sans compiler avec eux.
#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/NetSerialization.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"

namespace MissionsOutils
{
	inline FProperty* Variable(const UObject* Objet, std::initializer_list<const TCHAR*> Noms)
	{
		if (!Objet)
		{
			return nullptr;
		}
		for (const TCHAR* NomCherche : Noms)
		{
			for (TFieldIterator<FProperty> It(Objet->GetClass()); It; ++It)
			{
				if (It->GetName().Equals(NomCherche, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	/** Nombre (float, int, octet) ou enum : rend sa valeur en float. */
	inline bool LireNombre(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FProperty* Brut = Variable(Objet, Noms);
		if (FEnumProperty* E = CastField<FEnumProperty>(Brut))
		{
			Brut = E->GetUnderlyingProperty();
			if (FNumericProperty* Sous = CastField<FNumericProperty>(Brut))
			{
				Valeur = (float)Sous->GetSignedIntPropertyValue(E->ContainerPtrToValuePtr<void>(Objet));
				return true;
			}
			return false;
		}
		FNumericProperty* P = CastField<FNumericProperty>(Brut);
		if (!P)
		{
			return false;
		}
		const void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		Valeur = P->IsFloatingPoint() ? (float)P->GetFloatingPointPropertyValue(Adresse) : (float)P->GetSignedIntPropertyValue(Adresse);
		return true;
	}

	inline bool EcrireNombre(UObject* Objet, std::initializer_list<const TCHAR*> Noms, float Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(Variable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		void* Adresse = P->ContainerPtrToValuePtr<void>(Objet);
		if (P->IsFloatingPoint())
		{
			P->SetFloatingPointPropertyValue(Adresse, (double)Valeur);
		}
		else
		{
			P->SetIntPropertyValue(Adresse, (int64)FMath::RoundToInt(Valeur));
		}
		return true;
	}

	inline bool LireBooleen(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool& Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(Variable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		Valeur = P->GetPropertyValue_InContainer(Objet);
		return true;
	}

	inline bool EcrireBooleen(UObject* Objet, std::initializer_list<const TCHAR*> Noms, bool Valeur)
	{
		FBoolProperty* P = CastField<FBoolProperty>(Variable(Objet, Noms));
		if (!P)
		{
			return false;
		}
		P->SetPropertyValue_InContainer(Objet, Valeur);
		return true;
	}

	inline bool EcrireTexte(UObject* Objet, const TCHAR* NomVariable, const FString& Valeur)
	{
		FStrProperty* P = CastField<FStrProperty>(Variable(Objet, { NomVariable }));
		if (!P)
		{
			return false;
		}
		*P->ContainerPtrToValuePtr<FString>(Objet) = Valeur;
		return true;
	}

	inline bool EcrireVecteur(UObject* Objet, const TCHAR* NomVariable, const FVector& Valeur)
	{
		FStructProperty* P = CastField<FStructProperty>(Variable(Objet, { NomVariable }));
		if (!P || P->Struct != TBaseStructure<FVector>::Get())
		{
			return false;
		}
		*P->ContainerPtrToValuePtr<FVector>(Objet) = Valeur;
		return true;
	}

	/** Appelle une fonction sans parametre par son nom (ex. LancerVagueCommandee de ChellesZombies). */
	inline bool Appeler(UObject* Objet, const TCHAR* NomFonction)
	{
		UFunction* F = Objet ? Objet->FindFunction(FName(NomFonction)) : nullptr;
		if (!F || F->ParmsSize > 0)
		{
			return false;
		}
		Objet->ProcessEvent(F, nullptr);
		return true;
	}

	/** Le premier acteur du monde dont la classe porte ce nom (ex. "InvasionZombies"). */
	inline AActor* ActeurDeClasse(UWorld* Monde, const TCHAR* NomClasse)
	{
		if (!Monde)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(Monde); It; ++It)
		{
			if (It->GetClass()->GetName() == NomClasse)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Lit un tableau d'acteurs (ex. Zombies de InvasionZombies). */
	inline void LireActeurs(const UObject* Objet, const TCHAR* NomVariable, TArray<AActor*>& Resultat)
	{
		FArrayProperty* P = CastField<FArrayProperty>(Variable(Objet, { NomVariable }));
		FObjectPropertyBase* Interieur = P ? CastField<FObjectPropertyBase>(P->Inner) : nullptr;
		if (!Interieur)
		{
			return;
		}
		FScriptArrayHelper Tableau(P, P->ContainerPtrToValuePtr<void>(Objet));
		for (int32 i = 0; i < Tableau.Num(); ++i)
		{
			if (AActor* A = Cast<AActor>(Interieur->GetObjectPropertyValue(Tableau.GetRawPtr(i))))
			{
				Resultat.Add(A);
			}
		}
	}

	/** Lit le trajet du TRAMWAY (Trajet de ATrainChelles : TArray<FVector_NetQuantize>). */
	inline bool LirePoints(const UObject* Objet, const TCHAR* NomVariable, TArray<FVector>& Resultat)
	{
		FArrayProperty* P = CastField<FArrayProperty>(Variable(Objet, { NomVariable }));
		FStructProperty* Interieur = P ? CastField<FStructProperty>(P->Inner) : nullptr;
		if (!Interieur || !Interieur->Struct)
		{
			return false;
		}
		const FName NomStruct = Interieur->Struct->GetFName();
		FScriptArrayHelper Tableau(P, P->ContainerPtrToValuePtr<void>(Objet));
		Resultat.Reset(Tableau.Num());
		if (NomStruct == TEXT("Vector_NetQuantize") || NomStruct == TEXT("Vector"))
		{
			// FVector_NetQuantize herite de FVector sans rien ajouter : meme place en memoire
			for (int32 i = 0; i < Tableau.Num(); ++i)
			{
				Resultat.Add(*reinterpret_cast<const FVector*>(Tableau.GetRawPtr(i)));
			}
			return true;
		}
		return false;
	}

	/** Lit un champ texte d'un element d'un tableau de structures (ex. ArretsInfos[i].Nom du tramway). */
	inline FString LireTexteDansTableau(const UObject* Objet, const TCHAR* NomTableau, int32 Index, const TCHAR* NomChamp)
	{
		FArrayProperty* P = CastField<FArrayProperty>(Variable(Objet, { NomTableau }));
		FStructProperty* Interieur = P ? CastField<FStructProperty>(P->Inner) : nullptr;
		if (!Interieur || !Interieur->Struct)
		{
			return FString();
		}
		FStrProperty* Champ = FindFProperty<FStrProperty>(Interieur->Struct, NomChamp);
		FScriptArrayHelper Tableau(P, P->ContainerPtrToValuePtr<void>(Objet));
		if (!Champ || !Tableau.IsValidIndex(Index))
		{
			return FString();
		}
		return *Champ->ContainerPtrToValuePtr<FString>(Tableau.GetRawPtr(Index));
	}

	/** Un zombie de ChellesZombies encore debout (pas mort, pas en train de s'effondrer). */
	inline bool ZombieDebout(AActor* A)
	{
		ACharacter* Z = Cast<ACharacter>(A);
		if (!IsValid(Z) || !Z->GetController())
		{
			return false; // un zombie mort est detache de son controleur
		}
		bool bMort = false;
		if (LireBooleen(Z, { TEXT("isDead"), TEXT("IsDead") }, bMort) && bMort)
		{
			return false;
		}
		return true;
	}
}
