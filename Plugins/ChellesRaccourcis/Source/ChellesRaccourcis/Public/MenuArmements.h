// CHELLES 3D - Menu ARMEMENTS facon "lobby" (touche I, 2026-11-24, demande MERIDJA)
//
//  Grand menu plein ecran, style jeu de tir moderne (bleu, cartes colorees par rarete) :
//   - en haut : onglets ARMEMENTS / PERSONNAGE ;
//   - a gauche : recherche + categories (Tout, Coffre, Pistolets, Fusils...) ;
//   - au centre : grille de cartes (couleur = rarete, etoiles), tri, nombre d'objets, cases de l'inventaire ;
//   - a droite : fiche de l'arme choisie (i) ou filtres (cases a cocher) ;
//   - en bas : EQUIPER / AJOUTER AU COFFRE / LACHER, et QUITTER.
//
//  Meme liste d'armes que la roue TAB (tableau DT_Armes, voir RoueArmes.h) : rien a remplir en plus.
//  Le COFFRE = tes armes preferees (enregistrees dans GameUserSettings.ini, section ChellesArmements).
//  Reglages : section MENU ARMEMENTS de Plugins\ChellesRaccourcis\Raccourcis.txt. Console : Chelles.Armements
#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RoueArmes.h"
#include "MenuArmements.generated.h"

class APlayerController;
class SMenuArmements;

UCLASS(NotPlaceable, Transient)
class CHELLESRACCOURCIS_API AMenuArmements : public AActor
{
	GENERATED_BODY()

public:
	AMenuArmements();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Armements [Ouvrir|Fermer|Relire|0|1] */
	void Commande(const TArray<FString>& Arguments);

	void Ouvrir();
	void Fermer();
	void Basculer();
	bool EstOuvert() const { return bOuvert; }

private:
	void ChargerReglages();
	void LierClavier();
	APlayerController* GetPC() const;
	ARoueArmes* Roue() const;
	void ModeInterface();
	/** Relit les armes (en main, ramassees...) et met le menu a jour. */
	void Rafraichir();
	/** Rafraichit encore un peu plus tard (le changement d'arme du soldat prend un instant). */
	void RafraichirBientot();

	// Actions du menu (Index = case dans la liste complete)
	void Equiper(int32 Index);
	void BasculerCoffre(int32 Index);
	void Lacher(int32 Index);
	void Onglet(int32 Index);

	/** L'ancien menu d'inventaire du kit FPS (W_WeaponInventory_UI) : on le ferme s'il s'ouvre aussi sur I. */
	bool FermerInventaireFPS();

	void LireCoffre();
	void EcrireCoffre();
	void Message(const FString& Texte) const;

	// ---- Reglages (Raccourcis.txt, section MENU ARMEMENTS) ----
	bool bActif = true;
	FKey Touche;
	bool bRemplacerInventaire = true;

	// ---- Etat ----
	bool bOuvert = false;
	bool bLieClavier = false;
	TWeakObjectPtr<APlayerController> PCLie;
	uint64 ImageBasculee = 0;
	float SurveillerJusqua = 0.f;
	TArray<float> Rafraichissements;
	TSharedPtr<SMenuArmements> Widget;

	/** Ce que represente chaque carte (meme ordre que la liste du menu). */
	TArray<FCibleRoueArmes> Cibles;

	/** Images des cartes gardees en memoire tant que le menu les montre. */
	UPROPERTY()
	TArray<UTexture2D*> Images;

	/** Noms des armes rangees dans le COFFRE. */
	TSet<FString> LeCoffre;
};
