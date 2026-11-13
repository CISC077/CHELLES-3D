// CHELLES 3D - Chat global en jeu : fenetre a gauche de l'ecran, touche B
#include "ChatGlobalChelles.h"

#include "ConnexionChatGlobal.h"
#include "SChatGlobal.h"

#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

AChatGlobalChelles::AChatGlobalChelles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickInterval = 0.5f;
	bReplicates = false;
	SetActorHiddenInGame(true);
}

void AChatGlobalChelles::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FConnexionChatGlobal::Get().PartieCommence();
	CreerFenetre();
	BrancherTouche();
	UE_LOG(LogTemp, Log, TEXT("[ChatGlobal] Pret : touche B en jeu, option CHAT GLOBAL dans OPTIONS > JEU."));
}

void AChatGlobalChelles::EndPlay(const EEndPlayReason::Type Raison)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		RetirerFenetre();
		// Fin du Play dans l'editeur ou jeu ferme : on coupe tout de suite ; changement de carte : on attend la suivante
		if (FConnexionChatGlobal* Connexion = FConnexionChatGlobal::Trouver())
		{
			Connexion->PartieTerminee(Raison == EEndPlayReason::EndPlayInEditor || Raison == EEndPlayReason::Quit);
		}
	}
	Super::EndPlay(Raison);
}

void AChatGlobalChelles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	CreerFenetre();
	BrancherTouche();
}

APlayerController* AChatGlobalChelles::GetPC() const
{
	UWorld* W = GetWorld();
	return W ? W->GetFirstPlayerController() : nullptr;
}

void AChatGlobalChelles::BrancherTouche()
{
	APlayerController* PC = GetPC();
	if (!PC || !PC->IsLocalController() || PCBranche.Get() == PC || bToucheLiee)
	{
		return;
	}
	if (APlayerController* Ancien = PCBranche.Get())
	{
		DisableInput(Ancien);
	}
	EnableInput(PC);
	if (InputComponent)
	{
		// B est libre dans le projet (aucune autre touche, ni dans DefaultInput.ini ni dans les plugins Chelles)
		FInputKeyBinding& Liaison = InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AChatGlobalChelles::OuvrirFenetre);
		Liaison.bConsumeInput = false;
		PCBranche = PC;
		bToucheLiee = true;
	}
}

void AChatGlobalChelles::CreerFenetre()
{
	if (Fenetre.IsValid())
	{
		return;
	}
	UWorld* W = GetWorld();
	UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr;
	if (!Vue)
	{
		return;
	}
	Fenetre = SNew(SChatGlobal).Proprietaire(this);
	// Couche 890 : sous le panneau ENTREE du menu (900) et sous les menus (1000)
	Vue->AddViewportWidgetContent(Fenetre.ToSharedRef(), 890);
}

void AChatGlobalChelles::RetirerFenetre()
{
	if (!Fenetre.IsValid())
	{
		return;
	}
	Fenetre->Fermer();
	UWorld* W = GetWorld();
	if (UGameViewportClient* Vue = W ? W->GetGameViewport() : nullptr)
	{
		Vue->RemoveViewportWidgetContent(Fenetre.ToSharedRef());
	}
	Fenetre.Reset();
}

void AChatGlobalChelles::OuvrirFenetre()
{
	CreerFenetre();
	if (Fenetre.IsValid() && !Fenetre->EstOuverte())
	{
		Fenetre->Ouvrir();
	}
}
