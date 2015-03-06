// Fill out your copyright notice in the Description page of Project Settings.

#include "Empires2.h"
#include "BaseEmpiresInventory.h"
#include "Net/UnrealNetwork.h"
#include "EmpBaseWeapon.h"




UBaseEmpiresInventory::UBaseEmpiresInventory(const class FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
	
}


void UBaseEmpiresInventory::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBaseEmpiresInventory, Pistol);
	DOREPLIFETIME(UBaseEmpiresInventory, Primary);
	DOREPLIFETIME(UBaseEmpiresInventory, Tertiary);
	DOREPLIFETIME(UBaseEmpiresInventory, Special);
	
}


void UBaseEmpiresInventory::ClearInventory()
{
	//Destroy each of the objects and set them to null
	if (Pistol)
	{
		Pistol->Destroy();
		Pistol = nullptr;
	}

	if (Primary)
	{
		Primary->Destroy();
		Primary = nullptr;
	}
	if (Tertiary)
	{
		Tertiary->Destroy();
		Tertiary = nullptr;
	}
	if (Special)
	{
		Special->Destroy();
		Special = nullptr;
	}
}

void UBaseEmpiresInventory::AddItem(EInfantryInventorySlots::Type Slot, AEmpBaseWeapon* Item)
{
	switch (Slot)
	{
	case EInfantryInventorySlots::Slot_Sidearm:
		Pistol = Item;
		break;

	case EInfantryInventorySlots::Slot_Primary:
		Primary = Item;
		break;

	case EInfantryInventorySlots::Slot_Tertiary:
		Tertiary = Item;
		break;

	case EInfantryInventorySlots::Slot_Ability:
		Special = Item;
		break;
		
	}
}

AEmpBaseWeapon* UBaseEmpiresInventory::GetItemInSlot(EInfantryInventorySlots::Type Slot)
{
	switch (Slot)
	{
	case EInfantryInventorySlots::Slot_Sidearm:
		return Pistol;

	case EInfantryInventorySlots::Slot_Primary:
		return Primary;

	case EInfantryInventorySlots::Slot_Tertiary:
		return Tertiary;

	case EInfantryInventorySlots::Slot_Ability:
		return Special;
	
	default:
		return nullptr;
	}
}

void UBaseEmpiresInventory::OnRep_Pistol()
{
	TRACE("Pistol Replicated: %u", Pistol);
	OnRep_Any();
}

void UBaseEmpiresInventory::OnRep_Primary()
{
	TRACE("Primary Replicated: %u", Primary);
	OnRep_Any();
}

void UBaseEmpiresInventory::OnRep_Tertiary()
{
	OnRep_Any();
}

void UBaseEmpiresInventory::OnRep_Special()
{
	
}

void UBaseEmpiresInventory::OnRep_Any()
{
	AEmpires2Character* Character = Cast<AEmpires2Character>(this->GetOwner());
	if (Character) Character->RefreshHeldWeapon();
}
