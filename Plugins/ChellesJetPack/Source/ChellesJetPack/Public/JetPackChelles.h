// CHELLES 3D - JetPack a ramasser : le joueur qui le touche le porte sur le dos et peut voler (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "JetPackChelles.generated.h"

class ACharacter;
class APlayerController;
class UAudioComponent;
class UMaterialInterface;
class UParticleSystem;
class UParticleSystemComponent;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundBase;
class USphereComponent;
class SWidget;
class STextBlock;

/**
 * JetPack pose dans la carte (Blueprints BP_JetPack_* dans FPS_Game/Blueprints/Bonus).
 *
 * - Au sol : il tourne doucement et brille de sa couleur.
 * - Un joueur passe dessus : il le prend sur le dos (le serveur decide, tout le monde le voit).
 * - Espace : decoller / monter.  Ctrl ou C : descendre (et atterrir pres du sol).
 *   Z Q S D : avancer comme d'habitude.  Maj : turbo.  N : lacher le JetPack.
 * - Si le joueur meurt ou quitte la partie, le JetPack tombe a sa place,
 *   puis revient a son point de depart s'il n'est pas repris.
 */
UCLASS(Blueprintable, meta = (DisplayName = "JetPack Chelles"))
class CHELLESJETPACK_API AJetPackChelles : public AActor
{
	GENERATED_BODY()

public:
	AJetPackChelles();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------ apparence
	/** Modele 3D du JetPack (pack Jet Packs Bundle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	USkeletalMesh* ModeleJetPack = nullptr;

	/** Materiau a colorer (une version "Mask" du pack). Vide = materiau d'origine du modele. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	UMaterialInterface* MateriauCouleur = nullptr;

	/** Couleur du JetPack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	FLinearColor Couleur = FLinearColor(1.f, 0.05f, 0.05f, 1.f);

	/** Nom affiche au joueur (ROUGE, BLEU...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	FString NomCouleur = TEXT("ROUGE");

	/** Parametres de couleur du materiau qui recoivent Couleur (remplis par le script d'installation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	TArray<FName> ParametresCouleur;

	/** Parametres qui recoivent une version plus sombre de Couleur (2e teinte). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	TArray<FName> ParametresCouleurSombre;

	/** Flammes des reacteurs (pack : Jet_Exhaust_PS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	UParticleSystem* Flammes = nullptr;

	/** Bruit du moteur en vol (pack : Jet_Engine_Loop_Cue). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	USoundBase* SonMoteur = nullptr;

	/** Os du personnage ou le JetPack est accroche. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Dos")
	FName OsDuDos = TEXT("spine_03");

	/** Decalage du JetPack par rapport a l'os (rempli par le script d'apres la demo du pack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Dos")
	FVector DecalageDos = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Dos")
	FRotator RotationDos = FRotator::ZeroRotator;

	/** Taille du JetPack quand il est pose au sol (plus gros = plus facile a voir). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Apparence")
	float TailleAuSol = 1.6f;

	// ------------------------------------------------------------ vol
	/** Vitesse de vol (cm/s). 1500 = 54 km/h. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Vol")
	float VitesseVol = 1500.f;

	/** Vitesse avec Maj (turbo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Vol")
	float VitesseTurbo = 3000.f;

	/** Altitude maximale au-dessus du point de depart du JetPack (cm). 30000 = 300 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Vol")
	float PlafondVol = 30000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Touches")
	FKey ToucheMonter = EKeys::SpaceBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Touches")
	FKey ToucheDescendre = EKeys::LeftControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Touches")
	FKey ToucheDescendre2 = EKeys::C;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Touches")
	FKey ToucheTurbo = EKeys::LeftShift;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Touches")
	FKey ToucheLacher = EKeys::N;

	/** Secondes avant que le JetPack lache revienne a son point de depart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "JetPack|Vol")
	float DelaiRetour = 90.f;

	// ------------------------------------------------------------ etat (copie chez tous les joueurs)
	/** Personnage qui porte le JetPack (vide = au sol). */
	UPROPERTY(ReplicatedUsing = OnRep_Porteur, BlueprintReadOnly, Category = "JetPack")
	ACharacter* Porteur = nullptr;

	/** Le porteur est en train de voler (flammes + bruit chez tout le monde). */
	UPROPERTY(ReplicatedUsing = OnRep_EnVol, BlueprintReadOnly, Category = "JetPack")
	bool bEnVol = false;

	UPROPERTY(Replicated)
	bool bTurbo = false;

	/** Place du JetPack quand il est au sol (le serveur la donne a tous). */
	UPROPERTY(ReplicatedUsing = OnRep_Pose)
	FVector PosePosition = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_Pose)
	FRotator PoseRotation = FRotator::ZeroRotator;

	/** Le joueur demande au serveur de decoller / atterrir. */
	UFUNCTION(Server, Reliable)
	void ServeurVoler(bool bVoler);

	UFUNCTION(Server, Reliable)
	void ServeurTurbo(bool bActif);

	/** Le joueur demande au serveur de lacher le JetPack. */
	UFUNCTION(Server, Reliable)
	void ServeurLacher();

	/** Le JetPack porte par ce personnage (ou nullptr). */
	static AJetPackChelles* PortePar(const ACharacter* Perso);

protected:
	UPROPERTY(VisibleAnywhere, Category = "JetPack")
	USphereComponent* Zone;

	UPROPERTY(VisibleAnywhere, Category = "JetPack")
	USkeletalMeshComponent* Modele;

	UPROPERTY(VisibleAnywhere, Category = "JetPack")
	UPointLightComponent* Lueur;

	UPROPERTY(Transient)
	UAudioComponent* Moteur = nullptr;

	UPROPERTY(Transient)
	TArray<UParticleSystemComponent*> Reacteurs;

	UFUNCTION()
	void OnRep_Porteur();

	UFUNCTION()
	void OnRep_EnVol();

	UFUNCTION()
	void OnRep_Pose();

private:
	void AppliquerApparence();
	void CreerEffets();
	void MettreAJourEffets();
	void AppliquerPorte(); // accroche / decroche selon Porteur, sur chaque machine

	// serveur
	void ServeurCherche(float Dt);
	void ServeurSurveillePorteur(float Dt);
	void Prendre(ACharacter* Perso);
	void Lacher(bool bRetourDepart);
	void RevenirAuDepart();

	// les deux (serveur + joueur qui le porte)
	void ImposerVol(float Dt);
	void FinDuVol(ACharacter* Perso);

	// joueur qui le porte
	void CommandesLocales(float Dt);
	APlayerController* PCLocal() const;
	bool ProcheDuSol(const ACharacter* Perso, float Distance) const;

	// message a l'ecran
	void Afficher(const FString& Texte, float Duree);
	void MettreAJourMessage(float Dt);
	void RetirerMessage();

	FTransform Depart;
	bool bDepartConnu = false;
	float Horloge = 0.f;
	float ProchaineRecherche = 0.f;
	float FinRetour = -1.f;              // heure du retour automatique (serveur)
	TWeakObjectPtr<APlayerController> PCDuPorteur;
	TWeakObjectPtr<ACharacter> DernierPorteur;
	float FinInterditDernier = 0.f;      // le joueur qui vient de le lacher ne le reprend pas tout de suite
	float SansPion = 0.f;
	FVector DernierePosition = FVector::ZeroVector;

	// vitesse d'origine du porteur, remise a l'atterrissage
	TWeakObjectPtr<ACharacter> PersoModifie;
	float VitesseOrigine = -1.f;
	float FreinOrigine = -1.f;

	bool bEtaitMoi = false;  // le joueur de cette machine portait ce JetPack
	bool bPoseConnue = false;
	bool bVolLocal = false;  // ce que le joueur local croit (avant la reponse du serveur)
	bool bTurboLocal = false;
	float AttenteDecollage = 0.f;

	TSharedPtr<SWidget> Bandeau;
	TSharedPtr<STextBlock> TexteBandeau;
	float FinMessage = 0.f;
	FString MessagePermanent;
};
