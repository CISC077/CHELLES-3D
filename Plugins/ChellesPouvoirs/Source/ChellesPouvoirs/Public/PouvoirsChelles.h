// CHELLES 3D - XP et pouvoirs : gagner de l'XP en jouant, debloquer et utiliser des pouvoirs (UE 4.27)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "PouvoirsChelles.generated.h"

class AController;
class APawn;
class APlayerController;
class UDamageType;
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

	/**
	 * XP gagnee autrement qu'en jouant (ex. cadeau de Noel ramasse, plugin ChellesNoel).
	 * UFUNCTION : appelee par son nom depuis les autres plugins, sans dependance de compilation.
	 * A appeler seulement sur la machine du joueur.
	 */
	UFUNCTION()
	void GagnerXPBonus(int32 Quantite, const FString& Raison);

	/**
	 * XP de combat (2026-11-18), sur la machine du joueur : envoyee par le serveur quand il touche
	 * (Quantite petite, cumulee puis sauvegardee toutes les 2 s) ou elimine un ennemi (bTue).
	 */
	void GagnerXPCombat(int32 Quantite, bool bTue);

	/** Serveur : un ennemi suivi a pris des degats (branche sur son OnTakeAnyDamage). */
	UFUNCTION()
	void EnnemiBlesse(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur);

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

	// XP de combat (serveur) : on suit les ennemis, on retient qui les a touches, on voit quand ils meurent
	void SuivreEnnemis();
	void VerifierMorts(float Horloge);
	bool EstEnnemi(AActor* Acteur) const;
	static bool EstMort(AActor* Acteur);
	void EnvoyerXPCombat(AController* Tireur, int32 Quantite, bool bTue);

	struct FCibleXP
	{
		TWeakObjectPtr<AController> Tireur;
		float Heure = 0.f;
	};
	TMap<TWeakObjectPtr<AActor>, FCibleXP> CiblesTouchees;
	TMap<TWeakObjectPtr<AController>, float> DernierXPTouche;
	float ProchainSuivi = 0.f;
	float ProchaineVerif = 0.f;

	// XP de combat (machine du joueur) : les petits gains sont cumules puis sauvegardes d'un coup
	int32 XPCombatEnAttente = 0;
	float ProchainBilanCombat = 0.f;

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
	/** Nom du fichier de sauvegarde de ce joueur (ChellesXP, ChellesXP_PIE2, ChellesXP_Karim...). */
	FString NomSauvegarde;
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
