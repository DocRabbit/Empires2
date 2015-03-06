// Fill out your copyright notice in the Description page of Project Settings.

#include "Empires2.h"
#include "EmpBaseWeapon.h"
#include "FullAutoFiremode.h"


UFullAutoFiremode::UFullAutoFiremode()
	: Super()
{

}


void UFullAutoFiremode::BeginFire()
{
	Super::BeginFire();

	//Start a timer based off the first shot time
	float FirstShotTime = GetWeapon()->GetActiveFiremodeData().FirstShotFireDelay;
	if (FirstShotTime != 0)
	{
		FTimerHandle handle;
		GetWorld()->GetTimerManager().SetTimer(handle, this, &UFullAutoFiremode::HandleFire, FirstShotTime, false);
	}
	else
	{
		HandleFire();
	}
}

void UFullAutoFiremode::HandleFire()
{
	if (!IsFiring()) return; //If we aren't firing anymore, don't fire a shot

	GetWeapon()->FireShot();


	//get the time until the next shot
	float FireTime = (1.0f / GetWeapon()->GetActiveFiremodeData().RoundsPerMinute) * 60.0f;

	FTimerHandle handle;
	GetWorld()->GetTimerManager().SetTimer(handle, this, &UFullAutoFiremode::HandleFire, FireTime, false);

}