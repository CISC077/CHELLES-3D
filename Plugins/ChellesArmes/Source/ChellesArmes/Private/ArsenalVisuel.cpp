// CHELLES 3D - Arsenal : formes simples et materiaux colores
#include "ArsenalVisuel.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace ChellesArsenalVisuel
{
	static FString CheminLueur;
	static TWeakObjectPtr<UMaterialInterface> MemoLueur;
	static bool bLueurCherchee = false;

	void DefinirLueur(const FString& Chemin)
	{
		if (Chemin != CheminLueur)
		{
			CheminLueur = Chemin;
			MemoLueur.Reset();
			bLueurCherchee = false;
		}
	}

	static UMaterialInterface* Lueur()
	{
		if (!bLueurCherchee)
		{
			bLueurCherchee = true;
			if (!CheminLueur.IsEmpty())
			{
				MemoLueur = LoadObject<UMaterialInterface>(nullptr, *CheminLueur, nullptr, LOAD_NoWarn | LOAD_Quiet);
			}
			UE_LOG(LogTemp, Log, TEXT("[Arsenal] Materiau lumineux %s : %s"), *CheminLueur,
				MemoLueur.IsValid() ? TEXT("trouve") : TEXT("absent (couleurs simples, lancez Tools\\Arsenal\\creer_materiau_lueur.py pour des lames qui brillent)"));
		}
		return MemoLueur.Get();
	}

	static UMaterialInterface* Simple()
	{
		static TWeakObjectPtr<UMaterialInterface> Memo;
		if (!Memo.IsValid())
		{
			Memo = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return Memo.Get();
	}

	UStaticMesh* Forme(uint8 Quelle)
	{
		static TWeakObjectPtr<UStaticMesh> Memo[4];
		static const TCHAR* const Chemins[4] = {
			TEXT("/Engine/BasicShapes/Cube.Cube"),
			TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
			TEXT("/Engine/BasicShapes/Sphere.Sphere"),
			TEXT("/Engine/BasicShapes/Cone.Cone") };
		if (Quelle > Cone)
		{
			Quelle = Cube;
		}
		if (!Memo[Quelle].IsValid())
		{
			Memo[Quelle] = LoadObject<UStaticMesh>(nullptr, Chemins[Quelle], nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return Memo[Quelle].Get();
	}

	UMaterialInstanceDynamic* Materiau(UObject* Proprietaire, const FLinearColor& Couleur, bool bLueur)
	{
		UMaterialInterface* Base = bLueur ? Lueur() : nullptr;
		const bool bVraieLueur = Base != nullptr;
		if (!Base)
		{
			Base = Simple();
		}
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, Proprietaire);
		// Sans materiau lumineux, on eclaircit un peu la couleur pour qu'elle "flashe" quand meme
		const FLinearColor C = (bLueur && !bVraieLueur) ? (Couleur * 0.6f + FLinearColor(0.4f, 0.4f, 0.4f)) : Couleur;
		Mat->SetVectorParameterValue(TEXT("Color"), C);
		Mat->SetVectorParameterValue(TEXT("Couleur"), C);
		return Mat;
	}

	UStaticMeshComponent* Piece(USceneComponent* Parent, uint8 Quelle, const FVector& Position, const FVector& TailleCm,
		const FLinearColor& Couleur, bool bLueur, const FRotator& Rotation, bool bCoucher)
	{
		AActor* Proprietaire = Parent ? Parent->GetOwner() : nullptr;
		UStaticMesh* Maillage = Forme(Quelle);
		if (!Proprietaire || !Maillage)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Proprietaire);
		C->SetStaticMesh(Maillage);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->CanCharacterStepUpOn = ECB_No;
		C->SetCastShadow(false);
		C->SetupAttachment(Parent);

		// Les formes du moteur font 100 cm. Cylindre et cone sont dresses selon Z : on les couche vers +X.
		FVector Echelle = TailleCm / 100.f;
		FRotator Rot = Rotation;
		if (bCoucher && (Quelle == Cylindre || Quelle == Cone))
		{
			// Z local -> avant : longueur (X demande) sur Z, epaisseurs sur X/Y
			Echelle = FVector(TailleCm.Z, TailleCm.Y, TailleCm.X) / 100.f;
			Rot = (FQuat(Rotation) * FQuat(FRotator(-90.f, 0.f, 0.f))).Rotator();
		}
		C->SetRelativeLocationAndRotation(Position, Rot);
		C->SetRelativeScale3D(Echelle);
		if (UMaterialInstanceDynamic* Mat = Materiau(C, Couleur, bLueur))
		{
			C->SetMaterial(0, Mat);
		}
		C->RegisterComponent();
		return C;
	}
}
