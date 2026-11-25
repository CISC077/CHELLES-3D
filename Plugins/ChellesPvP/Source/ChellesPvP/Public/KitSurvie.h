// CHELLES 3D - PvP : le kit de survie qu'on jette a un ami a terre
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KitSurvie.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Petite boite verte a croix blanche, lancee par le serveur (physique, repliquee chez tous les joueurs).
 * Elle porte le tag d'acteur "ChellesKitSurvie" : tout acteur portant ce tag (par exemple un kit d'un autre
 * plugin) qui arrive pres d'un joueur a terre le reanime automatiquement (verifie par APvPChelles).
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESPVP_API AKitSurvie : public AActor
{
	GENERATED_BODY()

public:
	AKitSurvie();

	/** Serveur : lance le kit avec cette vitesse (cm/s). */
	void Lancer(const FVector& Vitesse);

	/** Serveur : remplace la boite par un modele du projet (chemin d'un StaticMesh, ex. /Game/Pack/SM_Kit). */
	void ChangerModele(const FString& Chemin);

	UPROPERTY(VisibleAnywhere, Category = "Chelles PvP")
	UStaticMeshComponent* Boite = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Chelles PvP")
	UStaticMeshComponent* CroixA = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Chelles PvP")
	UStaticMeshComponent* CroixB = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Chelles PvP")
	UPointLightComponent* Lueur = nullptr;

	/** Chemin du modele choisi dans PvP.txt (replique : chaque machine charge le meme). */
	UPROPERTY(ReplicatedUsing = QuandModeleRecu)
	FString CheminModele;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void QuandModeleRecu();

private:
	void Colorer(UStaticMeshComponent* Piece, const FLinearColor& Couleur);
};
