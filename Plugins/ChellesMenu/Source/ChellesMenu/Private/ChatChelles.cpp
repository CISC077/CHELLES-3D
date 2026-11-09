// CHELLES 3D - Chat entre joueurs (multijoueur LAN)
#include "ChatChelles.h"

#include "SChatChelles.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UChatChelles::UChatChelles()
{
	SetIsReplicatedByDefault(true);
}

void UChatChelles::Envoyer(const FString& Texte)
{
	if (!Texte.TrimStartAndEnd().IsEmpty())
	{
		ServeurEnvoyer(Texte.Left(200));
	}
}

bool UChatChelles::ServeurEnvoyer_Validate(const FString& Texte)
{
	return Texte.Len() <= 500;
}

void UChatChelles::ServeurEnvoyer_Implementation(const FString& Texte)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UWorld* Monde = GetWorld();
	if (!PC || !Monde)
	{
		return;
	}
	FString Propre = Texte.Left(200).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	if (Propre.IsEmpty())
	{
		return;
	}
	const FString Nom = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString(TEXT("Joueur"));
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		if (UChatChelles* Chat = DuJoueur(It->Get()))
		{
			Chat->ClientRecevoir(Nom, Propre);
		}
	}
}

void UChatChelles::ClientRecevoir_Implementation(const FString& Auteur, const FString& Texte)
{
	ChellesChat::AjouterMessageJoueur(Auteur, Texte);
}

void UChatChelles::EquiperJoueurs(UWorld* Monde)
{
	if (!Monde || Monde->GetNetMode() == NM_Client)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = Monde->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !DuJoueur(PC))
		{
			UChatChelles* Chat = NewObject<UChatChelles>(PC, TEXT("ChatChelles"));
			Chat->SetIsReplicated(true);
			Chat->RegisterComponent();
		}
	}
}

UChatChelles* UChatChelles::DuJoueur(APlayerController* PC)
{
	return PC ? PC->FindComponentByClass<UChatChelles>() : nullptr;
}
