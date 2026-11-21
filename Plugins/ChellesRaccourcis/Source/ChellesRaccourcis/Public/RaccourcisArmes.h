// CHELLES 3D - Raccourcis armes F1 a F12 (2026-11-21, etape 1 de la refonte "menu armes")
//
//  F1 a F10 prennent directement une arme, sans ouvrir de menu :
//    - "Case1" a "Case4" = les cases d'armes du soldat FPS_Game (dans l'ordre ou elles ont ete ramassees),
//    - "Blaster", "Bazooka", "Sabre", "Grenade", "LanceFlammes", "Mitrailleuse" = armes de l'arsenal ChellesArmes.
//  F11 et F12 sont reservees pour plus tard ("Reserve").
//  Le choix de chaque touche se regle dans Plugins\ChellesRaccourcis\Raccourcis.txt (sans recompiler).
//
//  Rien n'est modifie dans les Blueprints : le plugin fait ce que fait la roue d'armes du pack
//  (W_CircleWeaponSelect) : SelectedWeaponIndex = case, PlayLowerMontage, EquipWeapon.
//  Les touches ne marchent qu'a pied avec le soldat (ou un heros qui a les armes du soldat) ;
//  en vehicule, a cheval ou en DYNAMIC COMBAT (qui utilise F2-F6 pour lui), elles ne font rien.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RaccourcisArmes.generated.h"

class APawn;
class APlayerController;
class SWidget;

/** Ce que fait une touche. */
struct FRaccourciArme
{
	FKey Touche;
	/** Texte lu dans Raccourcis.txt : Case1..Case8, Blaster, Bazooka, Sabre, Grenade, LanceFlammes, Mitrailleuse, Aucune, Reserve. */
	FString Action;
};

UCLASS(NotPlaceable, Transient)
class CHELLESRACCOURCIS_API ARaccourcisArmes : public AActor
{
	GENERATED_BODY()

public:
	ARaccourcisArmes();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Raison) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Console : Chelles.Raccourcis [Info|Relire|0|1|F1..F12] */
	void Commande(const TArray<FString>& Arguments);

private:
	void ChargerReglages();
	APlayerController* GetPC() const;
	void Appuyer(int32 Numero);

	/** Le pion a-t-il les armes du soldat FPS_Game (SelectedWeaponIndex + EquipWeapon) ? */
	static bool EstSoldat(APawn* Pion);
	bool ChoisirCase(APawn* Pion, int32 Index);
	bool ChoisirArsenal(APawn* Pion, const FString& Nom);
	bool ArsenalEnMain() const;
	bool LancerCommande(const FString& Commande, const TArray<FString>& Arguments) const;
	static FString NomArme(UObject* Arme);
	void DecrireSoldat(APawn* Pion);

	// ---- Message a l'ecran ----
	void Message(const FString& Texte);
	void CreerEcran();
	void RetirerEcran();

	bool bActif = true;
	bool bAnimation = true;
	bool bMessages = true;
	TArray<FRaccourciArme> Raccourcis;

	TWeakObjectPtr<APawn> PionDecrit;
	FString TexteMessage;
	float FinMessage = 0.f;
	TSharedPtr<SWidget> Ecran;
};
