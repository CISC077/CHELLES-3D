// CHELLES 3D - empreintes de pas dans la neige du Landscape (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EmpreintesNeige.generated.h"

class ACharacter;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UTextureRenderTarget2D;

/**
 * A poser une fois dans le niveau (le script Tools/Neige/ajouter_neige.py le fait).
 * Quand un personnage (joueur, passants, soldats) marche sur le Landscape, une empreinte est
 * "imprimee" dans une texture (Cible) qui suit le joueur ; le materiau du Landscape la lit.
 * Les empreintes s'effacent doucement (DureeDisparition).
 */
UCLASS(meta = (DisplayName = "Empreintes dans la neige"))
class CHELLESNEIGE_API AEmpreintesNeige : public AActor
{
	GENERATED_BODY()

public:
	AEmpreintesNeige();

	/** Texture lue par le materiau du Landscape (RT_Empreintes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	UTextureRenderTarget2D* Cible = nullptr;

	/** Texture de travail, meme taille (RT_Empreintes_Tampon). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	UTextureRenderTarget2D* Tampon = nullptr;

	/** Forme d'une empreinte (M_Empreinte_Pas, additif). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	UMaterialInterface* MatEmpreinte = nullptr;

	/** Decalage + effacement (M_Empreinte_Report). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	UMaterialInterface* MatReport = nullptr;

	/** Collection lue par le materiau du sol (MPC_Empreintes : CentreX, CentreY, Taille). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	UMaterialParameterCollection* Collection = nullptr;

	/** Cote de la zone qui garde les empreintes autour du joueur (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	float TailleZone = 4000.f;

	/** Temps pour qu'une empreinte disparaisse (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	float DureeDisparition = 45.f;

	/** Distance entre deux pas (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	float LongueurPas = 70.f;

	/** Longueur et largeur d'une empreinte (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	FVector2D TailleEmpreinte = FVector2D(30.f, 13.f);

	/** Ecart entre pied gauche et pied droit (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	float EcartPieds = 12.f;

	/** Aussi les passants et les soldats (sinon seulement le joueur). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	bool bTousLesPersonnages = true;

	/** Seulement sur le Landscape (pas sur les routes, trottoirs, toits). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Empreintes")
	bool bSeulementSurLandscape = true;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	struct FPas
	{
		FVector DernierPas = FVector::ZeroVector;
		bool bPiedGauche = false;
		bool bInit = false;
	};

	FVector2D PositionJoueur(bool& bTrouve) const;
	void MettreAJourCollection();
	void ReporterEtEffacer(const FVector2D& NouveauCentre, float Attenuation);
	bool SurLaNeige(ACharacter* Perso) const;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MidVersTampon = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MidVersCible = nullptr;

	TMap<TWeakObjectPtr<ACharacter>, FPas> Pas;
	FVector2D Centre = FVector2D::ZeroVector;
	float Minuteur = 0.f;
	bool bPret = false;
};
