// CHELLES 3D - Courses de rue : rues de la circulation + fabrication des parcours (voir CoursesRues.h)
#include "CoursesRues.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "UObject/UnrealType.h"

namespace
{
	bool EstSpawner(const AActor* A)
	{
		for (const UClass* C = A ? A->GetClass() : nullptr; C; C = C->GetSuperClass())
		{
			if (C->GetName() == TEXT("CirculationSpawner"))
			{
				return true;
			}
		}
		return false;
	}

	float Dist2D(const FVector& A, const FVector& B)
	{
		return FVector::Dist2D(A, B);
	}
}

bool CoursesRues::Lire(UWorld* Monde, FGrapheRues& Graphe, FString& Pourquoi)
{
	Graphe = FGrapheRues();
	if (!Monde)
	{
		Pourquoi = TEXT("pas de monde");
		return false;
	}
	AActor* Spawner = nullptr;
	for (TActorIterator<AActor> It(Monde); It; ++It)
	{
		if (EstSpawner(*It))
		{
			Spawner = *It;
			break;
		}
	}
	if (!Spawner)
	{
		Pourquoi = TEXT("pas de Circulation Spawner dans la carte (plugin ChellesCirculation)");
		return false;
	}

	FArrayProperty* PropNoeuds = FindFProperty<FArrayProperty>(Spawner->GetClass(), TEXT("Noeuds"));
	FArrayProperty* PropTroncons = FindFProperty<FArrayProperty>(Spawner->GetClass(), TEXT("Troncons"));
	FStructProperty* StructNoeud = PropNoeuds ? CastField<FStructProperty>(PropNoeuds->Inner) : nullptr;
	FStructProperty* StructTroncon = PropTroncons ? CastField<FStructProperty>(PropTroncons->Inner) : nullptr;
	if (!StructNoeud || !StructTroncon || !StructNoeud->Struct || !StructTroncon->Struct)
	{
		Pourquoi = TEXT("variables Noeuds / Troncons introuvables dans le Circulation Spawner");
		return false;
	}
	FStructProperty* PropPosition = FindFProperty<FStructProperty>(StructNoeud->Struct, TEXT("Position"));
	FIntProperty* PropDe = FindFProperty<FIntProperty>(StructTroncon->Struct, TEXT("De"));
	FIntProperty* PropVers = FindFProperty<FIntProperty>(StructTroncon->Struct, TEXT("Vers"));
	if (!PropPosition || PropPosition->Struct != TBaseStructure<FVector>::Get() || !PropDe || !PropVers)
	{
		Pourquoi = TEXT("Position / De / Vers introuvables dans les rues de la circulation");
		return false;
	}

	FScriptArrayHelper Noeuds(PropNoeuds, PropNoeuds->ContainerPtrToValuePtr<void>(Spawner));
	FScriptArrayHelper Troncons(PropTroncons, PropTroncons->ContainerPtrToValuePtr<void>(Spawner));
	if (Noeuds.Num() < 10 || Troncons.Num() < 10)
	{
		Pourquoi = FString::Printf(TEXT("reseau de la circulation pas encore construit (%d points)"), Noeuds.Num());
		return false;
	}

	Graphe.Points.SetNum(Noeuds.Num());
	Graphe.Voisins.SetNum(Noeuds.Num());
	for (int32 i = 0; i < Noeuds.Num(); ++i)
	{
		Graphe.Points[i] = *PropPosition->ContainerPtrToValuePtr<FVector>(Noeuds.GetRawPtr(i));
	}
	for (int32 i = 0; i < Troncons.Num(); ++i)
	{
		const uint8* T = Troncons.GetRawPtr(i);
		const int32 De = *PropDe->ContainerPtrToValuePtr<int32>(T);
		const int32 Vers = *PropVers->ContainerPtrToValuePtr<int32>(T);
		if (De == Vers || !Graphe.Points.IsValidIndex(De) || !Graphe.Points.IsValidIndex(Vers))
		{
			continue;
		}
		Graphe.Voisins[De].AddUnique(Vers);
		Graphe.Voisins[Vers].AddUnique(De);
	}
	// Ordre fixe : le meme parcours sur tous les PC
	for (TArray<int32>& V : Graphe.Voisins)
	{
		V.Sort();
	}
	return true;
}

bool CoursesRues::Fabriquer(const FGrapheRues& G, int32 Graine, float LongueurCm, float EcartCm,
	const TArray<FVector>& DepartsPris, float EcartDepartsCm, TArray<FVector>& Points, float& LongueurReelleCm)
{
	Points.Reset();
	LongueurReelleCm = 0.f;

	TArray<int32> Carrefours;
	for (int32 i = 0; i < G.Num(); ++i)
	{
		if (G.Voisins[i].Num() >= 3)
		{
			Carrefours.Add(i);
		}
	}
	if (Carrefours.Num() == 0)
	{
		return false;
	}

	FRandomStream Hasard(Graine);
	TArray<int32> Meilleur;
	TArray<float> MeilleurCumul;
	for (int32 Essai = 0; Essai < 400; ++Essai)
	{
		const int32 Debut = Carrefours[Hasard.RandRange(0, Carrefours.Num() - 1)];
		bool bTropPres = false;
		for (const FVector& Pris : DepartsPris)
		{
			if (Dist2D(Pris, G.Points[Debut]) < EcartDepartsCm)
			{
				bTropPres = true;
				break;
			}
		}
		if (bTropPres)
		{
			continue;
		}

		TArray<int32> Chemin;
		TArray<float> Cumul;
		TSet<int32> Vus;
		Chemin.Add(Debut);
		Cumul.Add(0.f);
		Vus.Add(Debut);
		while (Cumul.Last() < LongueurCm)
		{
			const int32 Ici = Chemin.Last();
			const int32 Avant = Chemin.Num() >= 2 ? Chemin[Chemin.Num() - 2] : INDEX_NONE;
			const FVector DirAvant = Avant != INDEX_NONE ? (G.Points[Ici] - G.Points[Avant]).GetSafeNormal2D() : FVector::ZeroVector;

			TArray<int32> Options;
			TArray<float> Poids;
			float Somme = 0.f;
			for (const int32 V : G.Voisins[Ici])
			{
				if (Vus.Contains(V))
				{
					continue;
				}
				const FVector Dir = (G.Points[V] - G.Points[Ici]).GetSafeNormal2D();
				const float Alignement = Avant != INDEX_NONE ? FVector::DotProduct(DirAvant, Dir) : 1.f;
				if (Alignement < -0.2f)
				{
					continue; // pas de demi-tour
				}
				// Ne pas revenir pres d'un endroit deja parcouru (sinon les portes se melangent)
				bool bRecoupe = false;
				for (int32 j = 0; j < Chemin.Num(); ++j)
				{
					if (Cumul.Last() - Cumul[j] > 6000.f && Dist2D(G.Points[Chemin[j]], G.Points[V]) < 3000.f)
					{
						bRecoupe = true;
						break;
					}
				}
				if (bRecoupe)
				{
					continue;
				}
				const float P = 0.25f + FMath::Max(0.f, Alignement);
				Options.Add(V);
				Poids.Add(P);
				Somme += P;
			}
			if (Options.Num() == 0)
			{
				break;
			}
			float Tirage = Hasard.FRand() * Somme;
			int32 Choisi = Options.Last();
			for (int32 k = 0; k < Options.Num(); ++k)
			{
				Tirage -= Poids[k];
				if (Tirage <= 0.f)
				{
					Choisi = Options[k];
					break;
				}
			}
			Cumul.Add(Cumul.Last() + Dist2D(G.Points[Ici], G.Points[Choisi])); // en 2D : meme resultat sur tous les PC, quelle que soit l'altitude mesuree
			Chemin.Add(Choisi);
			Vus.Add(Choisi);
		}

		if (Cumul.Last() >= LongueurCm * 0.9f)
		{
			Meilleur = Chemin;
			MeilleurCumul = Cumul;
			break;
		}
		if (Meilleur.Num() == 0 || Cumul.Last() > MeilleurCumul.Last())
		{
			Meilleur = Chemin;
			MeilleurCumul = Cumul;
		}
	}
	if (Meilleur.Num() < 2 || MeilleurCumul.Last() < FMath::Min(LongueurCm * 0.5f, 30000.f))
	{
		return false;
	}

	// Une porte tous les EcartCm le long du chemin, la derniere a la fin
	const float Total = MeilleurCumul.Last();
	Points.Add(G.Points[Meilleur[0]]);
	const float Pas = FMath::Max(3000.f, EcartCm);
	float Suivante = Pas;
	for (int32 i = 1; i < Meilleur.Num(); ++i)
	{
		while (Suivante <= MeilleurCumul[i] && Suivante < Total - Pas * 0.4f)
		{
			const float Longueur = MeilleurCumul[i] - MeilleurCumul[i - 1];
			const float Alpha = Longueur > 1.f ? (Suivante - MeilleurCumul[i - 1]) / Longueur : 1.f;
			Points.Add(FMath::Lerp(G.Points[Meilleur[i - 1]], G.Points[Meilleur[i]], Alpha));
			Suivante += Pas;
		}
	}
	Points.Add(G.Points[Meilleur.Last()]);
	LongueurReelleCm = Total;
	return Points.Num() >= 3;
}
