// CHELLES 3D - Modes de survie : lecture / ecriture des variables des Blueprints FPS_Game et des autres plugins
// par leur nom (sans les modifier et sans compiler avec eux).
#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace SurvieOutils
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

	inline bool LireNombre(const UObject* Objet, std::initializer_list<const TCHAR*> Noms, float& Valeur)
	{
		FNumericProperty* P = CastField<FNumericProperty>(Variable(Objet, Noms));
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

	/**
	 * Remplit les munitions de toutes les armes portees par le pion (variables FPS_Game BP_WeaponBase :
	 * AmmoInReserve / AmmoInReserve_Max, AmmoCurrent / AmmoMax). Retourne le nombre d'armes rechargees.
	 */
	inline int32 RemplirMunitions(AActor* Pion)
	{
		if (!Pion)
		{
			return 0;
		}
		TArray<AActor*> Attaches;
		Pion->GetAttachedActors(Attaches);
		int32 Rechargees = 0;
		for (AActor* Arme : Attaches)
		{
			float ReserveMax = 0.f;
			if (!LireNombre(Arme, { TEXT("AmmoInReserve_Max"), TEXT("AmmoInReserveMax") }, ReserveMax))
			{
				continue;
			}
			EcrireNombre(Arme, { TEXT("AmmoInReserve") }, ReserveMax);
			float ChargeurMax = 0.f;
			if (LireNombre(Arme, { TEXT("AmmoMax") }, ChargeurMax))
			{
				EcrireNombre(Arme, { TEXT("AmmoCurrent") }, ChargeurMax);
			}
			++Rechargees;
		}
		return Rechargees;
	}
}
