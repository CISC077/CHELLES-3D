// CHELLES 3D - Caisses de munitions dispersees sur la carte (25/11/2026, agent Armements) - UE 4.27
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CaisseMunitions.generated.h"

class APawn;
class USphereComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UPrimitiveComponent;

/** Reglages lus dans Plugins\ChellesArmes\Arsenal.txt (section CAISSES DE MUNITIONS). */
struct FReglagesCaisses
{
	bool bActif = true;
	int32 Nombre = 40;
	float Attente = 60.f;         // secondes avant que la caisse revienne
	float Rayon = 40000.f;        // cm autour du point de depart des joueurs
	float Ecart = 2500.f;         // cm minimum entre deux caisses
	int32 Flammes = 75;           // jets de lance-flammes en plus
	int32 Mitrailleuse = 100;     // balles de mitrailleuse en plus
	FString Modele;               // Static Mesh facultatif (sinon caisse en bois)

	static const FReglagesCaisses& Lire(bool bRelire = false);
};

/**
 * Une caisse de munitions. Repliquee : le serveur la pose, tous les joueurs la voient.
 * Passer dessus = munitions pleines pour les armes du kit FPS_Game (reserve au maximum)
 * + munitions du lance-flammes et de la mitrailleuse (plugin ChellesArmes).
 * La caisse disparait puis revient apres Attente secondes.
 */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API ACaisseMunitions : public AActor
{
	GENERATED_BODY()

public:
	ACaisseMunitions();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Remplit les armes d'un personnage (sur la machine qui appelle). Renvoie le nombre d'armes remplies. */
	static int32 RemplirArmes(APawn* Pion);

private:
	UFUNCTION()
	void AuContact(UPrimitiveComponent* Composant, AActor* Autre, UPrimitiveComponent* AutreComposant, int32 Index, bool bBalayage, const FHitResult& Impact);

	UFUNCTION()
	void OnRep_Vide();

	void Revenir();
	void MontrerEtat();
	void Aviser(const FString& Texte) const;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* Zone;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Caisse;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Bande;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* Texte;

	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* Texte2;

	UPROPERTY(ReplicatedUsing = OnRep_Vide)
	bool bVide = false;

	FTimerHandle Minuteur;
	float DernierAvisLocal = -100.f;
};

/** Pose les caisses au debut de la partie (serveur, hote ou solo seulement). */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESARMES_API ACaissesMunitionsGestion : public AActor
{
	GENERATED_BODY()

public:
	ACaissesMunitionsGestion();
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Caisses Relire | Reposer | Nombre */
	void Commande(const TArray<FString>& Arguments);

private:
	int32 Poser();
	bool bPosees = false;
	int32 Essais = 0;
	float ProchainEssai = 5.f;
};
