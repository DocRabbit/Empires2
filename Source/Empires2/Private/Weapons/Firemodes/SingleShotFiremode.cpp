// Fill out your copyright notice in the Description page of Project Settings.

#include "Empires2.h"
#include "EmpBaseWeapon.h"
#include "SingleShotFiremode.h"


USingleShotFiremode::USingleShotFiremode()
	: Super()
{

}

void USingleShotFiremode::BeginFire()
{
	Super::BeginFire();

	//Start a timer based off the first shot time
	float FirstShotTime = GetWeapon()->GetActiveFiremodeData().FirstShotFireDelay;
	if (FirstShotTime != 0)
	{
		FTimerHandle handle;
		GetWorld()->GetTimerManager().SetTimer(handle, this, &USingleShotFiremode::HandleFire, FirstShotTime, false);
	}
	else
	{
		HandleFire();
	}
	
}



void USingleShotFiremode::HandleFire()
{
	GetWeapon()->FireShot();
}

