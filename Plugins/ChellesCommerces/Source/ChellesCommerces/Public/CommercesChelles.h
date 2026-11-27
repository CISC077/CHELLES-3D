// CHELLES 3D - Commerces (2026-11-26, demande Rachid "commerces ouverts"), UE 4.27.
//
// Un gestionnaire par machine (ajoute tout seul au Play, rien a poser dans la carte) :
//  - PIECES : monnaie du jeu, gagnee en tuant des zombies / ennemis et avec l'XP (plugin ChellesPouvoirs).
//    Elles sont sauvegardees sur le PC du joueur (Saved\SaveGames\ChellesCommerces.sav) et affichees a droite ;
//  - BOUTIQUES : en arrivant au comptoir de la BOULANGERIE, du TABAC-PRESSE (Tools\Commerces) ou du CHELLES BAR
//    (Tools\Bar), la boutique s'ouvre toute seule : on clique sur un article pour l'acheter avec ses pieces
//    (vie, regeneration, course, saut, bouclier, XP, munitions, carte a gratter). Aucune touche a apprendre ;
//  - en multijoueur, le serveur donne les bonus (vie, vitesse...) ; chaque joueur garde ses pieces chez lui.
// Ne modifie aucun autre plugin ni Blueprint : XP, munitions et vie sont lues / donnees par leur nom.
// Reglages : Plugins\ChellesCommerces\Commerces.txt (Bloc-notes, pas besoin de recompiler).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "CommercesTypes.h"
#include "CommercesChelles.generated.h"

class APawn;
class ACharacter;
class APlayerController;
class AController;
class ARelaisCommerces;
class SWidget;
class UDamageType;

/** Pieces du joueur, sauvegardees sur sa machine. */
UCLASS()
class CHELLESCOMMERCES_API UPiecesChelles : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 Pieces = 0;

	UPROPERTY()
	int32 TotalGagne = 0;

	UPROPERTY()
	int32 TotalDepense = 0;

	/** 27/11 : CADEAU DU JOUR - dernier jour ou le cadeau a ete donne (AAAA-MM-JJ) et jours de suite. */
	UPROPERTY()
	FString DernierCadeau;

	UPROPERTY()
	int32 SerieJours = 0;
};

/** Un relais par joueur, cree par le serveur (existe aussi en solo). */
UCLASS(NotBlueprintable, NotPlaceable)
class CHELLESCOMMERCES_API ARelaisCommerces : public AActor
{
	GENERATED_BODY()

public:
	ARelaisCommerces();

	UFUNCTION(Server, Reliable)
	void ServeurAcheter(uint8 Commerce, uint8 Article);

	/** Reponse du serveur a un achat : le joueur retire ses pieces seulement si bOk. */
	UFUNCTION(Client, Reliable)
	void ClientResultat(uint8 Commerce, uint8 Article, bool bOk, const FString& Message);

	/** Pieces gagnees (zombie tue, carte a gratter...) : ajoutees sur la machine du joueur. */
	UFUNCTION(Client, Reliable)
	void ClientPieces(int32 Quantite, const FString& Raison);

	/** Course / saut : la machine du joueur doit aussi le savoir (mouvement predit). */
	UFUNCTION(Client, Reliable)
	void ClientMouvement(uint8 Effet, float Multiplicateur, float Duree);

private:
	class ACommercesChelles* Gestionnaire() const;
};

/** Bonus en cours (serveur). */
struct FEffetCommerce
{
	TWeakObjectPtr<APlayerController> PC;
	uint8 Effet = 0;
	float Valeur = 0.f;
	float Fin = 0.f;
};

/** Course / saut modifies (serveur et machine du joueur). */
struct FMouvementCommerce
{
	TWeakObjectPtr<ACharacter> Perso;
	uint8 Effet = 0;
	float Original = 0.f;
	float Fin = 0.f;
};

/** Ennemi touche par un joueur (serveur, pour donner des pieces a l'elimination). */
struct FCibleCommerce
{
	TWeakObjectPtr<AController> Tireur;
	float Heure = 0.f;
	bool bZombie = false;
};

/** Comptoir trouve dans la carte. */
struct FComptoirCommerce
{
	TWeakObjectPtr<AActor> Acteur;
	int32 Commerce = 0;
};

UCLASS(NotPlaceable, Transient)
class CHELLESCOMMERCES_API ACommercesChelles : public AActor
{
	GENERATED_BODY()

public:
	ACommercesChelles();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console (touche au-dessus de Tab) : Chelles.Commerces [Info|Relire|Pieces N|Ouvrir Boulangerie/Tabac/Bar|Fermer] */
	void Commande(const TArray<FString>& Arguments);

	// ---- Serveur (appele par le relais du joueur)
	void AcheterServeur(ARelaisCommerces* R, uint8 Commerce, uint8 Article);
	void MouvementLocal(uint8 Effet, float Multiplicateur, float Duree);

	// ---- Machine du joueur
	void ResultatLocal(uint8 Commerce, uint8 Article, bool bOk, const FString& Message);
	void GagnerPieces(int32 Quantite, const FString& Raison);

	// ---- Lu par l'ecran (Slate)
	int32 Pieces() const { return Sauvegarde ? Sauvegarde->Pieces : 0; }
	const ChellesCommerces::FReglagesCommerces& LireReglages() const { return Reglages; }
	int32 BoutiqueOuverte() const { return Ouverte; }
	bool AchatEnCours() const { return bAchatEnCours; }
	FString TexteMessage() const { return Message; }
	FLinearColor CouleurMessage() const { return CouleurMsg; }
	FString TexteGain() const { return Gain; }
	float AgeDuGain() const { return AgeGain; }
	FString TexteBonus() const;

	/** Clic sur un article de la boutique ouverte. */
	void Acheter(int32 Article);
	/** Bouton FERMER (la boutique se rouvre quand on s'eloigne puis revient au comptoir). */
	void FermerBoutique(bool bParBouton);

	UFUNCTION()
	void EnnemiTouche(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur);

	UFUNCTION()
	void JoueurTouche(AActor* Touche, float Degats, const UDamageType* TypeDegats, AController* Auteur, AActor* Causeur);

private:
	// ---- Reseau
	APlayerController* PCLocal() const;
	ARelaisCommerces* RelaisDe(APlayerController* PC) const;
	void GererRelais();

	// ---- Serveur : effets
	bool Soigner(APawn* Pion, float Quantite) const;
	int32 AjouterMunitions(APlayerController* PC, const TCHAR* Propriete, int32 Quantite) const;
	void AppliquerMouvement(ACharacter* Perso, uint8 Effet, float Multiplicateur, float Duree);
	void GererEffets(float Dt);
	void GererMouvements();

	// ---- Serveur : pieces par elimination
	void SuivreEnnemis();
	void VerifierMorts(float Maintenant);
	static bool EstEnnemi(const AActor* Acteur);
	static bool EstMort(const AActor* Acteur);

	// ---- Joueur local
	void JoueurLocal(float Dt);
	void ChercherComptoirs();
	void OuvrirBoutique(int32 Commerce);
	void SuivreXP(float Dt);
	void DonnerXPLocale(int32 Quantite, const FString& Raison);
	void Charger();
	void Sauver();
	FString NomSauvegarde() const;
	void Son(const FString& Chemin, float Volume) const;
	void CreerEcran();
	void RetirerEcran();
	void AfficherMessage(const FString& Texte, const FLinearColor& Couleur);
	/** 27/11 : cadeau de pieces une fois par jour, plus gros chaque jour de suite (bForcer = test console). */
	void CadeauDuJour(bool bForcer);
	/** 27/11 : samedi et dimanche, pieces gagnees x Reglages.MultiWeekEnd. */
	bool EstWeekEndBonus() const;

	ChellesCommerces::FReglagesCommerces Reglages;

	UPROPERTY(Transient)
	UPiecesChelles* Sauvegarde = nullptr;

	// ---- Etat serveur
	TMap<TWeakObjectPtr<AActor>, FCibleCommerce> Cibles;
	TArray<FEffetCommerce> Effets;
	float ProchainRelais = 0.f;
	float ProchainSuivi = 1.f;

	// ---- Etat serveur et local
	TArray<FMouvementCommerce> Mouvements;

	// ---- Etat local
	TArray<FComptoirCommerce> Comptoirs;
	float ProchainComptoirs = 2.f;
	int32 RecherchesComptoirs = 0;
	float ProchainProche = 0.f;
	int32 Ouverte = INDEX_NONE;
	int32 FermeeIci = INDEX_NONE;   // boutique fermee au bouton : pas rouverte tant qu'on reste au comptoir
	bool bAchatEnCours = false;
	float DebutAchat = 0.f;
	bool bModifie = false;
	float ProchaineSauvegarde = 5.f;
	int32 DernierXP = -1;
	float AttenteXP = 8.f;
	float ResteXP = 0.f;
	float ResteMinute = 0.f;
	FString Message;
	FLinearColor CouleurMsg = FLinearColor::White;
	FString Gain;
	float AgeGain = 100.f;
	bool bOuvertureForcee = false;   // ouverte par la console : ne se ferme pas en s'eloignant
	TWeakObjectPtr<AActor> ActeurPouvoirs;
	float ProchainXP = 1.f;
	float ProchainRegen = 1.f;
	TArray<TPair<FString, float>> BonusAffiches; // nom, heure de fin
	TSharedPtr<SWidget> Ecran;
	TSharedPtr<SWidget> Fenetre;
	bool bCurseurAvant = false;
	bool bCadeauVerifie = false;
	float AttenteCadeau = 8.f;   // s apres le Play : l'ecran des pieces est deja la

	TSharedRef<SWidget> CreerFenetre(int32 Commerce);
};
