// CHELLES 3D - XP et pouvoirs : gagner de l'XP en jouant, debloquer et utiliser des pouvoirs (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "PouvoirsChelles.generated.h"

class APawn;
class APlayerController;
class APouvoirsRelais;
class UPrimitiveComponent;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class SPouvoirsHud;

/** CAMELEON : un personnage deguise (nouvelle apparence posee par-dessus, l'ancienne cachee). */
struct FDeguisementChelles
{
	FString Chemin;
	TWeakObjectPtr<USkeletalMeshComponent> Copie;
	TWeakObjectPtr<USkeletalMeshComponent> Corps;
	uint8 AncienTickAnim = 0;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> MorceauxCaches;
};

/**
 * Un seul par niveau, ajoute tout seul au lancement du jeu (Play) sur chaque machine.
 * - Sur la machine du joueur : compte le temps de jeu, donne l'XP (10 / min), la sauvegarde,
 *   affiche la barre d'XP et les pouvoirs, lit les touches J (utiliser) et Y (changer).
 * - Sur le serveur (ou en solo) : cree un relais par joueur et execute les pouvoirs.
 * - Partout : montre les effets (vol, invisible, gele, apparence) de tous les joueurs.
 */
UCLASS(meta = (DisplayName = "Pouvoirs et XP Chelles"))
class CHELLESPOUVOIRS_API APouvoirsChelles : public AActor
{
	GENERATED_BODY()

public:
	APouvoirsChelles();

	/** Touche pour utiliser le pouvoir choisi. */
	UPROPERTY(EditAnywhere, Category = "Pouvoirs")
	FKey ToucheUtiliser;

	/** Touche pour passer au pouvoir suivant. */
	UPROPERTY(EditAnywhere, Category = "Pouvoirs")
	FKey ToucheChanger;

	/** Afficher la barre d'XP et les pouvoirs en bas de l'ecran. */
	UPROPERTY(EditAnywhere, Category = "Pouvoirs")
	bool bAfficher = true;

	/** Message sur l'ecran du joueur. */
	void Message(const FString& Texte);

	/** Serveur : un joueur demande un pouvoir. */
	void ExecuterServeur(APouvoirsRelais* Relais, int32 Quel, FVector Origine, FRotator Visee);

	/** Pour tester : ajoute (ou retire) de l'XP au joueur de cette machine. Console : Chelles.XP 500 */
	void AjouterXPTest(int32 Quantite);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	APlayerController* GetPC() const;
	bool EstServeur() const;
	APouvoirsRelais* MonRelais() const;
	void LierClavier();
	void Utiliser();
	void Changer();

	// XP du joueur de cette machine
	void Charger();
	void Sauvegarder();
	void CompterTemps(float Secondes);
	void DonnerXP(int32 Quantite, bool bTempsDeJeu);
	bool EnJeu() const;

	// Serveur
	void GererRelais();
	bool PvpActif() const;
	bool ServeurCameleon(APouvoirsRelais* Relais, APawn* Pion);
	APawn* ViserCible(APawn* Lanceur, const FVector& Origine, const FVector& Direction) const;
	const TArray<FString>& ApparencesPour(USkeletalMeshComponent* Corps);

	// Effets vus sur cette machine
	void AppliquerEffets(float DeltaSeconds);
	bool AppliquerApparence(APawn* Pion, const FString& Chemin);
	void RetirerApparence(APawn* Pion);
	void Degeler(APawn* Pion);

	// Affichage
	void CreerHud();
	void RetirerHud();
	void MajHud();

	TSharedPtr<SPouvoirsHud> Hud;

	/** XP du joueur de cette machine (sauvegardee sur son PC). UPROPERTY : lue par le plugin ChellesHud. */
	UPROPERTY(Transient)
	int32 XPLocale = 0;
	double SecondesJouees = 0.0;
	float SecondesMinute = 0.f;
	/** Pouvoir choisi avec Y. UPROPERTY : lu par le plugin ChellesHud. */
	UPROPERTY(Transient)
	int32 Selection = 0;
	int32 NiveauAnnonce = 0;
	float ProchainEnvoi = 0.f;
	bool bCharge = false;
	bool bLieClavier = false;
	float ProchaineGestion = 0.f;
	float ProchaineSauvegarde = 0.f;

	FString Banniere;
	FString SousBanniere;
	float AgeBanniere = 100.f;
	FString Info;
	float AgeInfo = 100.f;

	/** Pions qui volent grace a ENVOL (pour les faire redescendre a la fin). */
	TSet<TWeakObjectPtr<APawn>> Volants;
	/** Morceaux caches par EFFET VERRE (pour les remontrer a la fin). */
	TSet<TWeakObjectPtr<UPrimitiveComponent>> MorceauxCaches;
	/** Pions geles et leur lumiere bleue. */
	TMap<TWeakObjectPtr<APawn>, TWeakObjectPtr<UPointLightComponent>> Geles;

	TMap<TWeakObjectPtr<APawn>, FDeguisementChelles> Deguises;

	/** CAMELEON : apparences compatibles, par squelette. */
	TMap<FString, TArray<FString>> ApparencesParSquelette;

	UPROPERTY(Transient)
	TArray<USkeletalMesh*> MeshesCharges;
};
