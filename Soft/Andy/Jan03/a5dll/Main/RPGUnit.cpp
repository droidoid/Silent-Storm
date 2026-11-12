#include "StdAfx.h"
#include "RPGUnit.h"
#include "RPGItemSet.h"
#include "A5Script.h"
#include "..\Misc\RandomGen.h"
#include "..\MiscDll\LogStream.h"
#include "..\DBFormat\DataFormat.h"
#include "..\DBFormat\DataRPG.h"
#include "rpgPerk.h"
#include "rpgPerkConstants.h"
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NRPG
{
const int N_DEFAULT_WEAPON_ID = 1;  // MeleeWeapon - "Unarmed"
const float LN161 = 0.476234179f;
int GetSkillByCap( int nCap, float fXP )
{
	if ( fXP <= 0 )
		return 0;
	const float C = 1.f / ( 20.f * LN161 / float(nCap) );
	return log( 1.f + 0.02f * fXP ) * C;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float GetXPBySkill( int nCap, int nLvl )
{
	if ( nLvl <= 0 )
		return 0;
	const float C = 1.f / ( 20.f * LN161 / float(nCap) );
	return ( pow( 2.7296f, float(nLvl) / C ) - 1.f ) / 0.02f;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CDynamicSkill
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDynamicSkill::SetNewMaxValue( int nNewValue ) 
{ 
	nValue += nNewValue - nMaxValue; 
	nMaxValue = nNewValue; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDynamicSkill::SetNewBaseValue( int nNewValue ) 
{ 
	SetNewMaxValue( nMaxValue + nNewValue - nBaseValue );
	nBaseValue = nNewValue; 
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDynamicSkill::Upgrade( float fAddToProgress )
{
	fProgress += fAddToProgress;
	int nModif = int(fProgress);
	if ( nModif > 0 )
	{
		fProgress -= float(nModif);
		Modify( nModif );
		if ( nValue > fMultiplier * nMaxValue )
			nValue = fMultiplier * nMaxValue;
		return true;
	}
	return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CDynamicSkill::Multiply( float fValue ) 
{ 
	fMultiplier *= fValue; 
	if ( nValue > fMultiplier * nMaxValue )
		nValue = fMultiplier * nMaxValue;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// CUnit
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit::CUnit(): fXP( 0 ), nHealedVP( 0 ), nCheats( 0 ), nDeathVP( 0 ), bUnconscious( false )
{
	skills.resize(NDb::SKILL_TYPE_NUMBERS);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetSkillCap( NDb::ESkillType eSkill, float fXP )
{
	return GetSkillByCap( pClass->skills[eSkill], fXP );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
float CUnit::GetXPForSkill( NDb::ESkillType eSkill, int nLvl )
{
	if ( !pClass )
		return 0;
	return GetXPBySkill( pClass->skills[eSkill], nLvl );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CUnit::CUnit( NDb::CRPGPers *_pPers, NDb::CComplexHead *_pHead, bool _bHero ):
	pPers( _pPers ), pHead( _pHead ), nHealedVP( 0 ), nCheats( 0 ), 
	fXP( 0 ), skills( NDb::SKILL_TYPE_NUMBERS ), wsName( L"[UNKNOWN]" ),
	nDeathVP( 0 ), bUnconscious( false ), bHero( _bHero )
{
	if ( !IsValid( pHead ) )
		pHead = pPers->pHead;
	if ( IsValid( pPers->pName ) )
		wsName = pPers->pName->szStr;

	pPerksTree = CreatePerksTree( pPers->pClass->nPerkTreeID );
	nRPGPersID = pPers->nRPGPersID;
	pInventory = CreateInventory(this);
	NDb::CRPGBaseValue &baseValues = *pPers->pBaseValue;
	skills[NDb::ST_STR] = new CDynamicSkill( baseValues.skills[NDb::ST_STR], baseValues.skills[NDb::ST_STR] );
	skills[NDb::ST_DEX] = new CDynamicSkill( baseValues.skills[NDb::ST_DEX], baseValues.skills[NDb::ST_DEX] );
	skills[NDb::ST_INT] = new CDynamicSkill( baseValues.skills[NDb::ST_INT], baseValues.skills[NDb::ST_INT] );
	fXP = baseValues.nBaseXP;

	pClass = pPers->pClass;
	SRand rand;
	pModel = pPers->pModel->CreateModel( &rand );

	if ( IsValid( pPers->pPanzerklein ) ) 
		pInventory->SetPanzerklein( pPers->pPanzerklein, 0 );
	pDefaultWeapon = dynamic_cast<CMeleeWeaponItem*>( CreateMeleeWeaponItem( NDb::GetMeleeWeapon( N_DEFAULT_WEAPON_ID ) ) );

	//pInventory->Equip( NDb::SLOT_WEAPON, CreateWeaponItem( pPers->pWeapon ) );
	vector< CPtr<IInventoryItem> > items;
	CPtr<IInventoryItem> pMainWeapon;
	if ( IsValid( pPers->pWeapon ) )
	{
		pMainWeapon = CreateWeaponItem( pPers->pWeapon );
		if ( IsValid( pMainWeapon ) )
			pInventory->Equip( NDb::SLOT_1, pMainWeapon );			
	}
	if ( IsValid( pPers->pPanzerklein ) )
	{
		NDb::CPanzerklein *pPK = pPers->pPanzerklein;
		CPtr<IInventoryItem> pSecondWeapon;
	//	pSecondWeapon = CInventoryItem( pPK->pLeftHandItem );
		if ( IsValid( pSecondWeapon ) )
			pInventory->Equip( NDb::SLOT_2, pSecondWeapon );			
	}
	for ( vector<NDb::SItemAssign>::const_iterator it = pPers->items.begin(); it != pPers->items.end(); ++it )
	{
		const NDb::SItemAssign &assign = *it;
		for( int i = 0; i < assign.nQuantity; ++i )
		{
			if ( IsValid( assign.pItem ) )
			{
				CPtr<NRPG::IInventoryItem> pItem;
				if ( !IsValid( assign.pAmmo ) )
					pItem = CreateItem( assign.pItem );
				else
					if ( CDynamicCast<NDb::CRPGClip> pRPGClip( assign.pItem ) )
						pItem = CreateClipItem( pRPGClip, assign.pAmmo );

				if ( IsValid( pItem ) )
					pInventory->Place( CTPoint<int>( -1, -1 ), pItem );
			}
			else
				ASSERT( 0 && "wrong item was assigned to unit" );
		}
	}

	for ( int i = 0; i < NDb::ST_STR; ++i )
		skills[i] = new CDynamicSkill( baseValues.skills.skills[i], GetSkillBaseStatValue( NDb::ESkillType(i) ) );
	if ( pClass )
	{
		for ( int i = NDb::ST_MELEE; i < NDb::ST_STR; ++i )
		{
			if ( baseValues.skills.skills[i] == 0 )
			{
				Skills(i).SetXPPart( GetSkillCap( NDb::ESkillType(i), fXP ) );
				Skills(i).Reset();
			}
		}
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const wstring& CUnit::GetName() const
{
	return wsName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::SetName( const wstring &_wsName )
{
	wsName = _wsName;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::UpdateSkills()
{
	for ( int i = 0; i < NDb::ST_STR; ++i )
	{
		skills[i]->SetNewBaseValue( GetSkillBaseStatValue( NDb::ESkillType(i) ) );
		skills[i]->Reset();
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::AddXP( float fXPToAdd )
{
	if ( !IsValid( pPers ) || !IsValid( pPers->pName ) )
		return;
	//
	int nOldLevel = Skills( NDb::ST_LEVEL );
	csRPG << CC_YELLOW << pPers->pName->szStr << " gained " << fXPToAdd << " EXP" << endl;
	int nLvlEXP = GetXPForSkill( NDb::ST_LEVEL, Skills(NDb::ST_LEVEL) );
	int nLvlUPEXP = GetXPForSkill( NDb::ST_LEVEL, Skills(NDb::ST_LEVEL) + 1 );
	fXP += fXPToAdd;
	Skills(NDb::ST_LEVEL).SetProgress( float(fXP - nLvlEXP) / float(nLvlUPEXP - nLvlEXP) );
	if ( fXP >= nLvlUPEXP )
	{
		int nNewLevel = Skills( NDb::ST_LEVEL );
		if ( IsValid( pPerksTree ) )
			pPerksTree->AddPerkPoints( nNewLevel - nOldLevel );
		csRPG << CC_YELLOW << pPers->pName->szStr << " level up ( " << Skills(NDb::ST_LEVEL) << "-th level )" << endl;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::UseSkill( int eSkill, const int nAddValue )
{
	CDynamicSkill &skill = Skills(eSkill);
	float sSkillDiff = GetSkillCap( NDb::ESkillType(eSkill), fXP ) - ( skill.GetXPPart() - nAddValue );
	float fAdd = Clamp( (1.f - skill.GetProgress()) * sSkillDiff / 20.f, 0.005f, 0.1f );
	bool bRes = skill.Upgrade(fAdd);
	if ( eSkill >= NDb::ST_STR )
		UpdateSkills();
	return bRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetSkillBaseStatValue( const int eSkill )
{
	switch ( eSkill )
	{
		case NDb::ST_MELEE:
			return Skills(NDb::ST_STR) + 2 * Skills(NDb::ST_DEX);
		case NDb::ST_SHOOTING:
			return 20 + Skills(NDb::ST_DEX);
		case NDb::ST_THROWING:
			return 2 * Skills(NDb::ST_STR) + Skills(NDb::ST_DEX);
		case NDb::ST_BURST:
			return 2 * Skills(NDb::ST_STR) + Skills(NDb::ST_DEX);
		case NDb::ST_SNIPE:
			return 2 * Skills(NDb::ST_DEX) + Skills(NDb::ST_INT);
		//
		case NDb::ST_STEALTH:
			return 5 + 2 * Skills(NDb::ST_DEX);
		case NDb::ST_SPOT:
			return 5 + 2 * Skills(NDb::ST_INT);
		case NDb::ST_MEDICINE:
			return Skills(NDb::ST_DEX) + 2 * Skills(NDb::ST_INT);
		case NDb::ST_ENGINEERING:
			return Skills(NDb::ST_DEX) + 2 * Skills(NDb::ST_INT);
		//
		case NDb::ST_VP:
			return 50 + 10 * Skills(NDb::ST_STR);
		case NDb::ST_INTERRUPT:
			return 25; //CRAP
		case NDb::ST_AP:
			return 40 + 2 * Skills(NDb::ST_DEX);
		case NDb::ST_IC:
			return 5;
		case NDb::ST_LEVEL:
			return 0;
		default:
			ASSERT( 0 && "Unknow skill" );
			break;
	}
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CRPGPers* CUnit::GetPers() const
{
	return pPers;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CComplexHead* CUnit::GetHead() const
{
	return pHead;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::EWeaponType CUnit::GetWeaponType() const
{
	if ( CDynamicCast<NRPG::CMineDetectorItem> pMD( pInventory->GetActive() ) )
		return NDb::WT_MINE_DETECTOR;

	CWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
		return pWeapon->GetWeaponType();

	CMeleeWeaponItem *pMeleeWeapon = GetMeleeWeaponItem();
	if ( pMeleeWeapon )
		return pMeleeWeapon->GetWeaponType();

	return NDb::WT_DEFAULT;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::CAnimWeaponType* CUnit::GetDBAnimWeapon() const
{
	CWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
		return pWeapon->GetDBWeapon()->pAnimWeaponType;
	CMeleeWeaponItem *pMeleeWeapon = GetMeleeWeaponItem();
	if ( pMeleeWeapon )
		return pMeleeWeapon->GetDBMeleeWeapon()->pAnimWeaponType;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NDb::ESkillType CUnit::GetWeaponSkill() const
{
	CWeaponItem *pWeapon = GetWeaponItem();
	if ( pWeapon )
		return pWeapon->GetSkillIndex();
	return NDb::ST_MELEE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetWeaponAP( CWeaponItem *_pWeapon ) const
{
	int nRes = 0;
	CPtr<CWeaponItem> pWeapon = _pWeapon;
	if ( !IsValid( pWeapon ) )
		pWeapon = GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		nRes = pWeapon->GetShootAP();
		NDb::EShootMode ShotMode = pWeapon->GetShootMode();
		if ( ShotMode == NDb::SM_Aimed || ShotMode == NDb::SM_Careful )
			nRes += pWeapon->GetDBWeapon()->nTargetingAP;		
		//
		float fDelta = 0;
		if ( ShotMode == NDb::SM_Snap )
			HasPerk( N_PERK_CHEAP_SNAP_SHOT, &fDelta );
		else if ( ShotMode == NDb::SM_Aimed )
			HasPerk( N_PERK_CHEAP_AIMED_SHOT, &fDelta );
		else if ( ShotMode == NDb::SM_ShortBurst )
			HasPerk( N_PERK_CHEAP_SHORT_BURST, &fDelta );
		nRes -= nRes * fDelta;
	}
	return nRes;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetWeaponBurstAP( CWeaponItem *_pWeapon ) const
{
	CPtr<CWeaponItem> pWeapon = _pWeapon;
	if ( !IsValid( pWeapon ) )
		pWeapon = GetWeaponItem();
	if ( IsValid( pWeapon ) )
	{
		if ( 0 == pWeapon->GetDBWeapon()->nRoF )
		{
			ASSERT( 0 );
			return 0;
		}
		return skills[NDb::ST_AP]->GetMaxValue() / pWeapon->GetDBWeapon()->nRoF;
	}
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetWeaponReloadAP( CWeaponItem *_pWeapon ) const
{
	CPtr<CWeaponItem> pWeapon = _pWeapon;
	if ( !IsValid( pWeapon ) )
		pWeapon = GetWeaponItem();
	if ( IsValid( pWeapon ) )
		return pWeapon->GetReloadAP();
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CWeaponItem* CUnit::GetWeaponItem() const
{
	if ( pCannonItem )
		return pCannonItem;
	if ( CDynamicCast<NRPG::CWeaponItem> pWeapon( pInventory->GetActive() ) )
		return pWeapon;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
CMeleeWeaponItem* CUnit::GetMeleeWeaponItem() const
{
	IInventoryItem *pItem = pInventory->GetActive();
	if ( !pItem )
		return pDefaultWeapon;
	if ( CDynamicCast<NRPG::CMeleeWeaponItem> pWeapon( pItem ) )
		return pWeapon;
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::IsDead()
{
	return Skills( NDb::ST_VP ) <= nDeathVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::Kill()
{
	Skills(NDb::ST_VP ).SetValue( -0xFFF );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::CalcDeathVP( float _fDeathCoeff )
{
	nDeathVP = -_fDeathCoeff * Skills( NDb::ST_VP ).GetMaxValue();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::SetXPLevel( int nLevel )
{
	if ( !pClass )
		return;
	int nOldLevel = Skills( NDb::ST_LEVEL );
	float fXP = GetXPForSkill( NDb::ST_LEVEL, nLevel );
	for ( int i = NDb::ST_MELEE; i < NDb::SKILL_TYPE_NUMBERS; ++i )
	{
		Skills(i).SetXPPart( GetSkillCap( NDb::ESkillType( i ), fXP ) );
		Skills(i).Reset();
	}
	UpdateSkills();
	csRPG << CC_GREY << "Unit [ " << CC_YELLOW << GetName() << 
		CC_GREY << " ] level changed: " << Skills( NDb::ST_LEVEL ) << "-th level" << endl;
	int nNewLevel = Skills( NDb::ST_LEVEL );
	if ( IsValid( pPerksTree ) )
		pPerksTree->AddPerkPoints( nNewLevel - nOldLevel );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::IsCheatEnabled( int nCheat )
{
	return ( ( nCheats & nCheat ) > 0 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::SetCheat( int nCheat, bool bState )
{
	if ( bState )
		nCheats |= nCheat;
	else
		nCheats &= ( ~nCheat );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
NRPG::IFirstAidItem* CUnit::GetFirstAidItem() const
{
	NRPG::IInventoryItem *pItem = pInventory->GetActive();
	if ( pItem == 0 )
		return 0;
	return CDynamicCast<NRPG::IFirstAidItem>( pItem );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
const float fVPScale = (float)N_MAX_VP / N_MAX_SKILL;
const float fDCScale = (float)N_MAX_DC / N_MAX_SKILL;
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::CreateFirstAid( SFirstAid *pRes, int nHealVP, int nSkill ) const
{
	pRes->nMaxVP = fVPScale * nSkill;
	pRes->fdVP = nHealVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::CreateFirstAid( SFirstAid *pRes, int nMaxSpentAP, float fKitCapacity, 
	IFirstAidItem *pItem, CUnit *pTarget, int *pRequiredAP )
{
	SFirstAid tmp;
	int nSkill = *skills[ NDb::ST_MEDICINE ] + pItem->GetDBFirstAid()->nSkillModifier;
	CreateFirstAid( &tmp, 0, nSkill );
	int nMaxHealed = pTarget->Skills(NDb::ST_VP).GetMaxValue() - pTarget->Skills(NDb::ST_VP);
	int nVPToHeal = Max( 0, tmp.nMaxVP - pTarget->nHealedVP );
	nVPToHeal = Min( nVPToHeal, nMaxHealed );
	if ( nVPToHeal == 0 )
		return false;
	int nRequiredAP = 60.f / nSkill * nVPToHeal;
	if ( nRequiredAP > nMaxSpentAP )
	{
		nRequiredAP = nMaxSpentAP;
		nVPToHeal = Float2Int( nRequiredAP * nSkill / 60.0f );
	}
	*pRequiredAP = nRequiredAP;
	CreateFirstAid( pRes, nVPToHeal, nSkill );
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
int CUnit::GetFirstAidDC( IFirstAidItem *pItem )
{
	int nSkill = *skills[ NDb::ST_MEDICINE ];
	if ( IsValid(pItem) ) 
		nSkill += pItem->GetDBFirstAid()->nSkillModifier;
	return fDCScale * nSkill;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::Heal( const SFirstAid &fa )
{
	int nTotalVP = Skills( NDb::ST_VP ) + nHealedVP;
	const int nMaxVP = Min( Skills( NDb::ST_VP ).GetMaxValue(), Skills( NDb::ST_VP ) + fa.nMaxVP );
	const int ndVP = Max( 0, Min( Float2Int( fa.fdVP ), nMaxVP - nTotalVP ) );
	nHealedVP += ndVP;
	//
	csRPG << "<font size=16pt>";
	csRPG << CC_RED << "\tBandage:" << CC_GREY << endl;
	csRPG << "\tMaxVP=" << fa.nMaxVP << " \t dVP=" << Float2Int( fa.fdVP ) << endl;
	csRPG << "<font size=16pt>";
	csRPG << "\t" << GetName() << ": \tVP += " << ndVP << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnit::RegenerateVP( const SFirstAid &fa )
{
	Skills( NDb::ST_VP ).SetValue( Max( 1, 
		Min( Skills( NDb::ST_VP ).GetMaxValue(), ( int )( Skills( NDb::ST_VP ) + fa.fdVP ) ) ) );
	nHealedVP = Min( nHealedVP, Skills( NDb::ST_VP ).GetMaxValue() - Skills( NDb::ST_VP ) );
	//
	csRPG << "<font size=16pt>";
	csRPG << CC_RED << "\tHeal:" << CC_GREY << endl;
	csRPG << "\t" << GetName() << ": VP = " << Skills( NDb::ST_VP ) << endl;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::CanHeal( CUnit *pTarget ) const
{
	ASSERT( IsValid( pTarget ) );
	if ( !IsValid( pTarget ) )
		return false;
	//
	return int( fVPScale * *skills[ NDb::ST_MEDICINE ] ) > pTarget->nHealedVP;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUnit::HasPerk( int nPerkID, float *pParam1, float *pParam2, float *pParam3 ) const
{
	return GetPerksTree()->HasPerk( nPerkID, pParam1, pParam2, pParam3 );
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace
using namespace NRPG;
REGISTER_SAVELOAD_CLASS( 0x24051150, CUnit );
REGISTER_SAVELOAD_CLASS( 0x24051151, CDynamicSkill );
REGISTER_SAVELOAD_CLASS( 0x24051152, CSkillModifier );
