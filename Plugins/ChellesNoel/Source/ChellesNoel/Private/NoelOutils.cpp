#include "NoelOutils.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionBlendMaterialAttributes.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialParameterCollection.h"

bool UNoelOutils::AjouterScalaireCollection(UMaterialParameterCollection* Collection, FName Nom, float ValeurParDefaut)
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

#if WITH_EDITOR
namespace
{
	using FCarte = TMap<UMaterialExpression*, UMaterialExpression*>;

	/** Copie un lien (expression + sortie + masque). */
	void CopierLien(FExpressionInput& Vers, const FExpressionInput& De)
	{
		Vers.Expression = De.Expression;
		Vers.OutputIndex = De.OutputIndex;
		Vers.Mask = De.Mask;
		Vers.MaskR = De.MaskR;
		Vers.MaskG = De.MaskG;
		Vers.MaskB = De.MaskB;
		Vers.MaskA = De.MaskA;
	}

	/** Copie un lien du materiau de neige, en le faisant pointer vers la copie du noeud. */
	void CopierLienNeige(FExpressionInput& Vers, const FExpressionInput& De, const FCarte& Carte)
	{
		CopierLien(Vers, De);
		if (De.Expression)
		{
			UMaterialExpression* const* Copie = Carte.Find(De.Expression);
			Vers.Expression = Copie ? *Copie : nullptr;
		}
	}

	/** Branche la sortie 0 de Noeud. */
	void Relier(FExpressionInput& Entree, UMaterialExpression* Noeud)
	{
		Entree.Expression = Noeud;
		Entree.OutputIndex = 0;
		Entree.Mask = 0;
		Entree.MaskR = 0;
		Entree.MaskG = 0;
		Entree.MaskB = 0;
		Entree.MaskA = 0;
	}

	template <class T>
	T* Creer(UMaterial* Materiau, int32 X, int32 Y)
	{
		T* Noeud = NewObject<T>(Materiau, NAME_None, RF_Transactional);
		Noeud->Material = Materiau;
		Noeud->MaterialExpressionEditorX = X;
		Noeud->MaterialExpressionEditorY = Y;
		Materiau->Expressions.Add(Noeud);
		return Noeud;
	}

	UMaterialExpression* NormaleVersLeHaut(UMaterial* Materiau, int32 X, int32 Y)
	{
		UMaterialExpressionConstant3Vector* C = Creer<UMaterialExpressionConstant3Vector>(Materiau, X, Y);
		C->Constant = FLinearColor(0.f, 0.f, 1.f);
		return C;
	}
}
#endif

FString UNoelOutils::MelangerNeige(UMaterial* Cible, UMaterial* SourceNeige, UMaterialParameterCollection* Collection, FName Parametre)
{
#if WITH_EDITOR
	if (!Cible || !SourceNeige || !Collection)
	{
		return TEXT("materiau ou collection manquant");
	}
	if (Cible == SourceNeige)
	{
		return TEXT("la cible et la neige sont le meme materiau");
	}
	const FGuid IdParametre = Collection->GetParameterId(Parametre);
	if (!IdParametre.IsValid())
	{
		return FString::Printf(TEXT("parametre %s absent de %s"), *Parametre.ToString(), *Collection->GetName());
	}
	if (Cible->bUseMaterialAttributes && !Cible->MaterialAttributes.Expression)
	{
		return TEXT("le materiau d'origine n'a rien de branche sur Material Attributes");
	}

	Cible->PreEditChange(nullptr);
	Cible->Modify();

	// les noeuds ajoutes sont ranges a gauche et en dessous des noeuds existants
	int32 MinX = 0;
	int32 MaxY = 0;
	for (const UMaterialExpression* E : Cible->Expressions)
	{
		if (E)
		{
			MinX = FMath::Min(MinX, E->MaterialExpressionEditorX);
			MaxY = FMath::Max(MaxY, E->MaterialExpressionEditorY);
		}
	}
	const int32 DecalX = MinX - 600;
	const int32 DecalY = MaxY + 800;

	// 1. copie de tous les noeuds de la neige
	FCarte Carte;
	for (UMaterialExpression* E : SourceNeige->Expressions)
	{
		if (!E)
		{
			continue;
		}
		UMaterialExpression* Copie = DuplicateObject<UMaterialExpression>(E, Cible);
		Copie->SetFlags(RF_Transactional);
		Copie->Material = Cible;
		Copie->MaterialExpressionEditorX += DecalX;
		Copie->MaterialExpressionEditorY += DecalY;
		if (Copie->HasAParameterName())
		{
			// noms distincts de ceux du sol d'origine
			Copie->SetParameterName(FName(*(TEXT("Neige_") + Copie->GetParameterName().ToString())));
			Copie->UpdateParameterGuid(true, false);
		}
		Cible->Expressions.Add(Copie);
		Carte.Add(E, Copie);
	}
	// les liens entre les copies pointent encore vers le materiau de neige : on les corrige
	for (TPair<UMaterialExpression*, UMaterialExpression*>& Paire : Carte)
	{
		for (FExpressionInput* Entree : Paire.Value->GetInputs())
		{
			if (Entree && Entree->Expression)
			{
				UMaterialExpression* const* Copie = Carte.Find(Entree->Expression);
				Entree->Expression = Copie ? *Copie : nullptr;
			}
		}
	}

	// 2. "combien de neige" = Parametre (0 ou 1) x sol a peu pres plat
	const int32 X0 = MinX - 400;
	UMaterialExpressionCollectionParameter* Saison = Creer<UMaterialExpressionCollectionParameter>(Cible, X0 - 600, DecalY - 400);
	Saison->Collection = Collection;
	Saison->ParameterName = Parametre;
	Saison->ParameterId = IdParametre;
	UMaterialExpressionVertexNormalWS* Normale = Creer<UMaterialExpressionVertexNormalWS>(Cible, X0 - 600, DecalY - 250);
	UMaterialExpressionCustom* Couverture = Creer<UMaterialExpressionCustom>(Cible, X0 - 300, DecalY - 350);
	Couverture->Description = TEXT("Couverture_Neige_Noel");
	Couverture->OutputType = CMOT_Float1;
	Couverture->Code = TEXT("return saturate(Saison) * saturate((N.z - 0.55) * 4.0);");
	Couverture->Inputs.Empty();
	FCustomInput EntreeSaison;
	EntreeSaison.InputName = TEXT("Saison");
	Couverture->Inputs.Add(EntreeSaison);
	FCustomInput EntreeNormale;
	EntreeNormale.InputName = TEXT("N");
	Couverture->Inputs.Add(EntreeNormale);
	Relier(Couverture->Inputs[0].Input, Saison);
	Relier(Couverture->Inputs[1].Input, Normale);

	// 3. melange
	if (Cible->bUseMaterialAttributes)
	{
		UMaterialExpressionMakeMaterialAttributes* Neige = Creer<UMaterialExpressionMakeMaterialAttributes>(Cible, X0, DecalY);
		CopierLienNeige(Neige->BaseColor, SourceNeige->BaseColor, Carte);
		CopierLienNeige(Neige->Roughness, SourceNeige->Roughness, Carte);
		CopierLienNeige(Neige->Normal, SourceNeige->Normal, Carte);
		if (!Neige->Normal.Expression)
		{
			Relier(Neige->Normal, NormaleVersLeHaut(Cible, X0 - 300, DecalY + 200));
		}
		UMaterialExpressionBlendMaterialAttributes* Melange = Creer<UMaterialExpressionBlendMaterialAttributes>(Cible, X0 + 400, 0);
		CopierLien(Melange->A, Cible->MaterialAttributes);
		Relier(Melange->B, Neige);
		Relier(Melange->Alpha, Couverture);
		Melange->PixelAttributeBlendType = EMaterialAttributeBlend::Blend;
		Melange->VertexAttributeBlendType = EMaterialAttributeBlend::UseA;
		Relier(Cible->MaterialAttributes, Melange);
	}
	else
	{
		// Sortie : entree du materiau d'origine ; DeLaNeige : la meme entree dans M_Sol_Neige (ou rien)
		auto Melanger = [&](FExpressionInput& Sortie, const FExpressionInput* DeLaNeige,
			float ParDefautOrigine, float ParDefautNeige, bool bNormale, int32 Y)
		{
			UMaterialExpressionLinearInterpolate* Lerp = Creer<UMaterialExpressionLinearInterpolate>(Cible, X0, Y);
			if (Sortie.Expression)
			{
				CopierLien(Lerp->A, Sortie);
			}
			else if (bNormale)
			{
				Relier(Lerp->A, NormaleVersLeHaut(Cible, X0 - 300, Y));
			}
			else
			{
				Lerp->ConstA = ParDefautOrigine;
			}
			if (DeLaNeige && DeLaNeige->Expression)
			{
				CopierLienNeige(Lerp->B, *DeLaNeige, Carte);
			}
			if (!Lerp->B.Expression)
			{
				if (bNormale)
				{
					Relier(Lerp->B, NormaleVersLeHaut(Cible, X0 - 300, Y + 100));
				}
				else
				{
					Lerp->ConstB = ParDefautNeige;
				}
			}
			Relier(Lerp->Alpha, Couverture);
			Relier(Sortie, Lerp);
		};
		Melanger(Cible->BaseColor, &SourceNeige->BaseColor, 0.f, 0.85f, false, 0);
		Cible->BaseColor.UseConstant = false;
		Melanger(Cible->Roughness, &SourceNeige->Roughness, 0.5f, 0.6f, false, 200);
		Cible->Roughness.UseConstant = false;
		Melanger(Cible->Normal, &SourceNeige->Normal, 0.f, 0.f, true, 400);
		Melanger(Cible->Metallic, nullptr, 0.f, 0.f, false, 600);
		Cible->Metallic.UseConstant = false;
	}

	Cible->PostEditChange();
	Cible->MarkPackageDirty();
	return FString();
#else
	return TEXT("seulement dans l'editeur");
#endif
}
