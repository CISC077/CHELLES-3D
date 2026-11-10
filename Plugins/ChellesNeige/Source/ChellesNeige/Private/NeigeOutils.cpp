#include "NeigeOutils.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialParameterCollection.h"

bool UNeigeOutils::BrancherPositionMonde(UMaterial* Materiau, UMaterialExpression* Expression)
{
#if WITH_EDITORONLY_DATA
	if (!Materiau || !Expression)
	{
		return false;
	}
	Materiau->Modify();
	FExpressionInput& Entree = Materiau->WorldPositionOffset;
	Entree.Expression = Expression;
	Entree.OutputIndex = 0;
	TArray<FExpressionOutput>& Sorties = Expression->GetOutputs();
	if (Sorties.Num() > 0)
	{
		Entree.Mask = Sorties[0].Mask;
		Entree.MaskR = Sorties[0].MaskR;
		Entree.MaskG = Sorties[0].MaskG;
		Entree.MaskB = Sorties[0].MaskB;
		Entree.MaskA = Sorties[0].MaskA;
	}
	Materiau->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UNeigeOutils::AjouterScalaireCollection(UMaterialParameterCollection* Collection, FName Nom, float ValeurParDefaut)
{
#if WITH_EDITOR
	if (!Collection || Nom.IsNone())
	{
		return false;
	}
	Collection->Modify();
	for (FCollectionScalarParameter& P : Collection->ScalarParameters)
	{
		if (P.ParameterName == Nom)
		{
			P.DefaultValue = ValeurParDefaut;
			Collection->PostEditChange();
			Collection->MarkPackageDirty();
			return true;
		}
	}
	FCollectionScalarParameter Nouveau;
	Nouveau.ParameterName = Nom;
	Nouveau.DefaultValue = ValeurParDefaut;
	Collection->ScalarParameters.Add(Nouveau);
	Collection->PostEditChange();
	Collection->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UNeigeOutils::ReglerNoeudCollection(UMaterialExpression* Noeud, UMaterialParameterCollection* Collection, FName Nom)
{
#if WITH_EDITORONLY_DATA
	UMaterialExpressionCollectionParameter* N = Cast<UMaterialExpressionCollectionParameter>(Noeud);
	if (!N || !Collection)
	{
		return false;
	}
	const FGuid Id = Collection->GetParameterId(Nom);
	if (!Id.IsValid())
	{
		return false;
	}
	N->Modify();
	N->Collection = Collection;
	N->ParameterName = Nom;
	N->ParameterId = Id;
	return true;
#else
	return false;
#endif
}
