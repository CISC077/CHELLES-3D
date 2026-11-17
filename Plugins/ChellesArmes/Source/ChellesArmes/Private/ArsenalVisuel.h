// CHELLES 3D - Arsenal : formes simples (moteur) et materiaux colores pour dessiner les armes et les tirs
#pragma once

#include "CoreMinimal.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;

namespace ChellesArsenalVisuel
{
	enum EForme : uint8 { Cube, Cylindre, Sphere, Cone };

	/** Chemin du materiau lumineux (Arsenal.txt, MateriauLueur=). */
	void DefinirLueur(const FString& Chemin);

	UStaticMesh* Forme(uint8 Quelle);

	/** Materiau de couleur. bLueur = qui brille (si le materiau lumineux existe), sinon couleur simple. */
	UMaterialInstanceDynamic* Materiau(UObject* Proprietaire, const FLinearColor& Couleur, bool bLueur);

	/**
	 * Ajoute une piece (forme simple) a un composant.
	 * Taille en cm (X = longueur vers l'avant). Les cylindres et cones sont couches vers l'avant (+X).
	 */
	UStaticMeshComponent* Piece(USceneComponent* Parent, uint8 Quelle, const FVector& Position, const FVector& TailleCm,
		const FLinearColor& Couleur, bool bLueur, const FRotator& Rotation = FRotator::ZeroRotator, bool bCoucher = true);
}
