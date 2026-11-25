// CHELLES 3D - PvP : lecture / ecriture des variables des Blueprints FPS_Game et des autres plugins
// par leur nom (sans les modifier et sans compiler avec eux).
#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

namespace PvPOutils
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
		FProperty* Brute = Variable(Objet, Noms);
		FNumericProperty* P = CastField<FNumericProperty>(Brute);
		if (!P)
		{
			// Une enumeration (ex. Mode de ChellesSurvie) : on lit le nombre qui est dessous
			if (FEnumProperty* E = CastField<FEnumProperty>(Brute))
			{
				P = E->GetUnderlyingProperty();
				if (P)
				{
					const void* AdresseEnum = E->ContainerPtrToValuePtr<void>(Objet);
					Valeur = (float)P->GetSignedIntPropertyValue(AdresseEnum);
					return true;
				}
			}
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

	/** Le premier acteur du monde dont la classe porte ce nom (ex. "SurvieChelles"). */
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

	// ---- Vie des personnages FPS_Game (BP_CharacterBase : HealthCurrent / HealthMax / IsDead) ----

	inline bool AUneVie(const UObject* Pion)
	{
		return Variable(Pion, { TEXT("HealthCurrent"), TEXT("Health") }) != nullptr;
	}

	inline float Vie(const UObject* Pion)
	{
		float V = 0.f;
		LireNombre(Pion, { TEXT("HealthCurrent"), TEXT("Health") }, V);
		return V;
	}

	inline float VieMax(const UObject* Pion)
	{
		float V = 100.f;
		LireNombre(Pion, { TEXT("HealthMax"), TEXT("MaxHealth") }, V);
		return V > 0.f ? V : 100.f;
	}

	inline void DonnerVie(UObject* Pion, float Valeur)
	{
		EcrireNombre(Pion, { TEXT("HealthCurrent"), TEXT("Health") }, Valeur);
	}

	inline bool EstMort(const UObject* Pion)
	{
		bool bMort = false;
		LireBooleen(Pion, { TEXT("IsDead"), TEXT("isDead"), TEXT("bIsDead"), TEXT("Dead") }, bMort);
		return bMort;
	}
}
