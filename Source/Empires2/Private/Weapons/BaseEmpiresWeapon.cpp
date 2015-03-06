// Fill out your copyright notice in the Description page of Project Settings.

#include "Empires2.h"
#include "Empires2Character.h"
#include "BaseEmpiresProjectile.h"
#include "BaseFiremode.h"
#include "BaseEmpiresWeapon.h"
#include "UnrealNetwork.h"
#include "WeaponFireType.h"
#include "SkeletalMeshTypes.h"
//Turns on COF Adjusted fire angles
static TAutoConsoleVariable<int32> CVarDisplayCofTraces(TEXT("emp.DesignDisplayCoFLines"), 0, 
	TEXT("Displays lines showing the CoF adjusted Firing Angle")/*, EConsoleVariableFlags::ECVF_Cheat*/);
	


ABaseEmpiresWeapon::ABaseEmpiresWeapon(const class FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{

	GunOffset = FVector(100.0f, 30.0f, 10.0f);
	ActiveFiremode = 0;
	bReplicates = true;
	bAlwaysRelevant = true;
	CollisionComponent = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Collision Capsue"));
	RootComponent = CollisionComponent;

	Mesh1P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("FirstPersonMesh"));
	Mesh1P->bOnlyOwnerSee = true;


	Mesh3P = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("ThirdPersonMesh"));
	Mesh3P->bOwnerNoSee = true;

	Mesh3P->AttachParent = RootComponent;
	Mesh1P->AttachParent = RootComponent;

	PrimaryActorTick.bCanEverTick = true;
}

//////////////////////GENERAL

void ABaseEmpiresWeapon::PostInitProperties()
{
	Super::PostInitProperties();

	//Construct the firemodes
	for (int32 i = 0; i < FiremodeData.Num(); i++)
	{
		if (FiremodeData[i].FiremodeClass == nullptr)
		{
			SCREENLOG(TEXT("[%s] Error constructing firemode %d, Firemode is null"), GetName(), i);
			continue;
		}
		UBaseFiremode* firemode = ConstructObject<UBaseFiremode>(FiremodeData[i].FiremodeClass, this);
		Firemodes.Add(firemode);

		
	}

	//Create the ammo pools
	CurrentClipPool.AddZeroed(AmmoPools.Num());
	RemainingAmmoPool.AddZeroed(AmmoPools.Num());
	Firetypes.AddZeroed(AmmoPools.Num());

	for (int32 i = 0; i < AmmoPools.Num(); i++)
	{
		CurrentClipPool[i] = AmmoPools[i].ClipSize;
		RemainingAmmoPool[i] = AmmoPools[i].MaxAmmo;

		//Create the firetypes
		if(AmmoPools[i].FireType) Firetypes[i] = NewObject<UWeaponFireType>(this, AmmoPools[i].FireType);
	}

}

void ABaseEmpiresWeapon::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseEmpiresWeapon, CurrentCoF);
	DOREPLIFETIME(ABaseEmpiresWeapon, ActiveFiremode);
	DOREPLIFETIME(ABaseEmpiresWeapon, ShotsFired);
	DOREPLIFETIME(ABaseEmpiresWeapon, OwningCharacter);
	DOREPLIFETIME(ABaseEmpiresWeapon, WeaponState);
}


void ABaseEmpiresWeapon::SetOwner(AEmpires2Character* Owner)
{
	Super::SetOwner(Owner);

	OwningCharacter = Owner;
}

void ABaseEmpiresWeapon::PlaySound(USoundBase* Sound)
{
	// try and play the sound if specified
	if (Sound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(OwningCharacter, Sound, OwningCharacter->GetActorLocation());
	}
}

void ABaseEmpiresWeapon::PlayAnimation(UAnimMontage* Animation)
{
	// try and play a animation
	if (Animation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = OwningCharacter->Get1PMesh()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(Animation, 1.f);
		}
	}
}


void ABaseEmpiresWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	//Tick the tickable firetypes
	for (int32 i = 0; i < Firetypes.Num(); i++)
	{
		if (Firetypes[i] && Firetypes[i]->bIsTicked) Firetypes[i]->Tick(DeltaSeconds);
	}

}




//////////////////////////EQUIPPING
void ABaseEmpiresWeapon::Equip()
{
	check(OwningCharacter);

	Mesh1P->SetVisibility(false);
	Mesh3P->SetVisibility(false);

	//Get the socket
	if (OwningCharacter->Role == ROLE_AutonomousProxy)
	{
		Mesh1P->AttachTo(OwningCharacter->Get1PMesh(), OwningCharacter->GetWeaponAttachSocket(), EAttachLocation::SnapToTarget, true);
		//this->AttachRootComponentTo(OwningCharacter->Get1PMesh(), OwningCharacter->GetWeaponAttachSocket(), EAttachLocation::SnapToTarget, true);
	}
	else
	{
		this->AttachRootComponentTo(OwningCharacter->GetMesh(), OwningCharacter->GetWeaponAttachSocket());
	}
	
	RegisterAllComponents();
	WeaponState = EWeaponState::Weapon_NotSelected; //TODO: Play the equipping sound/goto equipping state
}
void ABaseEmpiresWeapon::Unequip()
{
	this->DetachRootComponentFromParent();

	WeaponState = EWeaponState::Weapon_NotEquipped;
}


void ABaseEmpiresWeapon::DrawWeapon()
{
	//Todo: Play the equip animation and make the state drawing here
	WeaponState = EWeaponState::Weapon_Idle;
	Mesh1P->SetVisibility(true);
	Mesh3P->SetVisibility(true);

	if (Role == ROLE_Authority)
	{
		NotifyDrawn();
	}
}

void ABaseEmpiresWeapon::PutAwayWeapon()
{
	this->Mesh1P->SetVisibility(false);
	this->Mesh3P->SetVisibility(false);
	WeaponState = EWeaponState::Weapon_NotSelected;

	if (Role == ROLE_Authority)
	{
		NotifyPutAway();
	}
}

void ABaseEmpiresWeapon::NotifyPutAway_Implementation()
{
	this->Mesh1P->SetVisibility(false);
	this->Mesh3P->SetVisibility(false);
}


void ABaseEmpiresWeapon::NotifyDrawn_Implementation()
{
	Mesh1P->SetVisibility(true);
	Mesh3P->SetVisibility(true);

}





/////////////////////FIRE CONTROL

bool ABaseEmpiresWeapon::CanFire()
{
	UBaseFiremode* firemode = GetActiveFiremode();
	if (firemode == nullptr) return false; //No active firemode
	if (WeaponState != EWeaponState::Weapon_Idle) return false; //We can only fire if we are idle
	if (GetAmmoInClip() <= 0) return false; //If we have no ammo, we can't fire
	
	//TODO: Check if the firemode is capable of firing


	return true;
}

void ABaseEmpiresWeapon::BeginFire()
{
	if (!CanFire()) return; //Don't Fire if we can't fire

	if (Role < ROLE_Authority)
	{
		ServerStartFire();
	}

	if (GetAmmoInClip() <= 0) //If we are out of ammo, attempt to reload
	{
		Reload();
		return;
	}

	UBaseFiremode* firemode = GetActiveFiremode();
	check(firemode);
	firemode->BeginFire();

	WeaponState = EWeaponState::Weapon_Firing;
	ShotsFired = 0;

}


void ABaseEmpiresWeapon::ServerStartFire_Implementation()
{
	BeginFire();
}


bool ABaseEmpiresWeapon::ServerStartFire_Validate()
{
	return true;
}


void ABaseEmpiresWeapon::EndFire()
{
	//If we are the client, send a server RPC to fire our weapon
	if (Role < ROLE_Authority)
	{
		ServerEndFire();
	}

	if (WeaponState != EWeaponState::Weapon_Firing) return; //Don't need to EndFire if we aren't firing.
	UBaseFiremode* firemode = GetActiveFiremode();
	check(firemode);
	firemode->EndFire();

	WeaponState = EWeaponState::Weapon_Idle;
}


void ABaseEmpiresWeapon::ServerEndFire_Implementation()
{
	EndFire();
}

bool ABaseEmpiresWeapon::ServerEndFire_Validate()
{
	return true;
}


void ABaseEmpiresWeapon::FireShot()
{
	check(OwningCharacter);

	if (Role < ROLE_Authority)
	{
		ServerFireShot();
	}

	//Get the current firemode's projectile
	FAmmoPool ammoPool = GetCurrentAmmoPool();
				
	const FRotator SpawnRotation = OwningCharacter->GetControlRotation();
	// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
	const FVector SpawnLocation = OwningCharacter->GetActorLocation() + SpawnRotation.RotateVector(GunOffset);

	FVector AimDirection = GetFireDirection();
	FVector CofDirection = AdjustByCof(AimDirection);

	if (CVarDisplayCofTraces.GetValueOnGameThread())
	{
		DrawDebugLine(GetWorld(), SpawnLocation, SpawnLocation + (AimDirection * 1000), FColor::Blue, true, -1.0f, 0, 1);		
		DrawDebugLine(GetWorld(), SpawnLocation, SpawnLocation + (CofDirection * 1000), FColor::Red, true, -1.0f, 0, 1);
	}
	FRotator ConeAdjustedAngle = CofDirection.Rotation();

	EmitShot(SpawnLocation, ConeAdjustedAngle);
		
	ConsumeAmmo(GetActiveFiremodeData().AmmoConsumedPerShot);
	ShotsFired++;

	if (OwningCharacter->IsLocallyControlled())
	{
		PlaySound(GetActiveWeaponAnimationSet().FireSound);
		PlayAnimation(GetActiveWeaponAnimationSet().FireAnimation);

		//Recoil the shot
		OwningCharacter->AddControllerPitchInput(-RollVerticalRecoil());
		OwningCharacter->AddControllerYawInput(RollHorizontalRecoil());

		ClientPlayWeaponEffect(SpawnLocation, ConeAdjustedAngle);

	}
	if (Role == ROLE_Authority)
	{
		NotifyClientShotFired(SpawnLocation, ConeAdjustedAngle);
	}


	//If we are out of ammo, stop firing and reload
	if (GetAmmoInClip() <= 0)
	{
		EndFire();
		Reload();
	}

}

//Emits a simulated bullet.  This does not play any effects.
void ABaseEmpiresWeapon::EmitShot(FVector StartPoint, FRotator Direction)
{

	UWeaponFireType* CurrentFiretype = GetCurrentFiretype();

	if (CurrentFiretype == nullptr)
	{
		//UE_LOG(EmpiresGameplay, Display, TEXT("Unable to fire Weapon %s, firemode %d because Firetype is null!"), GetName(), ActiveFiremode);
		return;
	}
	
	//Let the firetype emit a shot.  
	CurrentFiretype->EmitShot(StartPoint, Direction);
}

void ABaseEmpiresWeapon::NotifyClientShotFired_Implementation(FVector StartPoint, FRotator Direction)
{

	if (OwningCharacter->Role == ROLE_AutonomousProxy) return; //If we are locally controlled, we've already played the effect and simulated the shot

	//Play the effect
	ClientPlayWeaponEffect(StartPoint, Direction);

	//And simulate a shot
	EmitShot(StartPoint, Direction);
}




void ABaseEmpiresWeapon::ClientPlayWeaponEffect(FVector StartPoint, FRotator Direction)
{
	//Spawn the muzzleflash effect
	FWeaponAnimationSet AnimSet = GetActiveWeaponAnimationSet();

	if (AnimSet.MuzzleFlash)
	{
		USkeletalMeshComponent* Component = Mesh3P;
		if (OwningCharacter->IsLocallyControlled())
		{
			Component = Mesh1P;
		}

		UGameplayStatics::SpawnEmitterAttached(AnimSet.MuzzleFlash,
			Component,
			FName("Muzzle"),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			true);
	}


	//Spawn the bullet projectile particle effect

	if (AnimSet.BulletEffect)
	{
		const USkeletalMeshSocket* Muzzle = ViewModel->FindSocket("Muzzle");
		FVector SpawnLocation = StartPoint;
		if (Muzzle)
		{
			SpawnLocation = Muzzle->RelativeLocation;
		}
		UGameplayStatics::SpawnEmitterAtLocation(this, AnimSet.BulletEffect, SpawnLocation);
	}
}




void ABaseEmpiresWeapon::ServerFireShot_Implementation()
{
	FireShot();
}

bool ABaseEmpiresWeapon::ServerFireShot_Validate()
{
	return true;
}

FVector ABaseEmpiresWeapon::GetFireDirection()
{
	FVector Vec;
	FRotator Rot;
	OwningCharacter->GetController()->GetPlayerViewPoint(Vec, Rot);
	return Rot.Vector();
}

///////////////////////////////////// FIREMODES
FWeaponData ABaseEmpiresWeapon::GetActiveFiremodeData()
{

	return FiremodeData[ActiveFiremode];
}


UBaseFiremode* ABaseEmpiresWeapon::GetActiveFiremode()
{
	return Firemodes[ActiveFiremode];
}


FAmmoPool ABaseEmpiresWeapon::GetCurrentAmmoPool()
{
	return AmmoPools[FiremodeData[ActiveFiremode].AmmoPoolIndex];
}

UWeaponFireType* ABaseEmpiresWeapon::GetCurrentFiretype()
{
	return Firetypes[FiremodeData[ActiveFiremode].AmmoPoolIndex];
}



void ABaseEmpiresWeapon::NextFiremode()
{
	if (GetActiveFiremode()->IsFiring()) return; //Don't change modes if we are shooting
	if (Firemodes.Num() == 1) return; //Don't change firemode if we only have one firemode

	//Play the animation and sound for changing the firemode.
	PlaySound(GetActiveWeaponAnimationSet().ChangeFiremodeSound);
	PlayAnimation(GetActiveWeaponAnimationSet().ChangeFiremodeAnimation);

	ActiveFiremode++;
	if (ActiveFiremode >= Firemodes.Num())
	{
		ActiveFiremode = 0;
	}
}

UBaseFiremode* ABaseEmpiresWeapon::GetFiremode(int32 Firemode)
{
	if (Firemode == CurrentFiremode)
	{
		return GetActiveFiremode();
	}
	else
	{
		check(Firemode >= 0 && Firemode < Firemodes.Num());
		return Firemodes[Firemode];
	}
}

FWeaponData ABaseEmpiresWeapon::GetFiremodeData(int32 Firemode)
{
	if (Firemode == CurrentFiremode)
	{
		return GetActiveFiremodeData();
	}
	else
	{
		check(Firemode >= 0 && Firemode < FiremodeData.Num());
		return FiremodeData[Firemode];
	}
}

////////////////////////////////// AMMO

FAmmoPool ABaseEmpiresWeapon::GetAmmoPool(int32 FromAmmoPool)
{
	if (FromAmmoPool == CurrentAmmopool)
	{
		return GetCurrentAmmoPool();
	}
	else
	{
		check(FromAmmoPool >= 0 && FromAmmoPool < AmmoPools.Num());
		return AmmoPools[FromAmmoPool];
	}
}

void ABaseEmpiresWeapon::ConsumeAmmo(int32 HowMuch, int32 FromAmmoPool)
{
	int32 AmmoPoolidx = FromAmmoPool == CurrentAmmopool ? GetActiveFiremodeData().AmmoPoolIndex : FromAmmoPool;

	CurrentClipPool[AmmoPoolidx] -= HowMuch;
}

int32 ABaseEmpiresWeapon::GetAmmoInClip(int32 FromAmmoPool)
{
	int32 AmmoPoolidx = FromAmmoPool == CurrentAmmopool ? GetActiveFiremodeData().AmmoPoolIndex : FromAmmoPool;

	return CurrentClipPool[AmmoPoolidx];
}

int32 ABaseEmpiresWeapon::GetTotalAmmo(int32 FromAmmoPool)
{
	int32 AmmoPoolidx = FromAmmoPool == CurrentAmmopool ? GetActiveFiremodeData().AmmoPoolIndex : FromAmmoPool;
	return RemainingAmmoPool[AmmoPoolidx];
}

void ABaseEmpiresWeapon::AddAmmo(int32 Ammount, int32 ToAmmoPool)
{
	int32 AmmoPoolidx = ToAmmoPool == CurrentAmmopool ? GetActiveFiremodeData().AmmoPoolIndex : ToAmmoPool;
	FAmmoPool AmmoPool = GetAmmoPool(ToAmmoPool);


	RemainingAmmoPool[AmmoPoolidx] += Ammount;

	if (RemainingAmmoPool[AmmoPoolidx] > AmmoPool.MaxAmmo)
	{
		RemainingAmmoPool[AmmoPoolidx] = AmmoPool.MaxAmmo;
	}
}

void ABaseEmpiresWeapon::Reload()
{
	if (GetTotalAmmo() <= 0) return; //We don't have any ammo to reload

	SCREENLOG(TEXT("Starting Reload"));

	//Get the current reload animation
	UAnimMontage* ReloadAnim = GetActiveWeaponAnimationSet().ReloadAnimation;
	float ReloadTime = GetAmmoPool().ReloadTime;

	if (ReloadAnim == nullptr)
	{
		PlayAnimation(ReloadAnim);
	}

	FTimerHandle Handle;

	GetWorld()->GetTimerManager().SetTimer(Handle, this, &ABaseEmpiresWeapon::DoReload, ReloadTime, false);

	WeaponState = EWeaponState::Weapon_Reloading;
}

void ABaseEmpiresWeapon::DoReload()
{
	SCREENLOG(TEXT("Ending Reload"));

	int Idx = GetActiveFiremodeData().AmmoPoolIndex;


	if (RemainingAmmoPool[Idx] < AmmoPools[Idx].ClipSize) //We don't have enough ammo to fill out a full clip
	{
		CurrentClipPool[Idx] = RemainingAmmoPool[Idx]; //So set the ammo to whatever is left
		RemainingAmmoPool[Idx] = 0;
	}
	else //Otherwise, max out our clip
	{
		//Figure out how many bullets we need
		int32 bulletsNeeded = AmmoPools[Idx].ClipSize - CurrentClipPool[Idx];

		//Max out the clip size
		CurrentClipPool[Idx] = AmmoPools[Idx].ClipSize;
		//And take the diff from the pool
		RemainingAmmoPool[Idx] -= bulletsNeeded;
	}
	WeaponState = EWeaponState::Weapon_Idle;
	
}

FVector ABaseEmpiresWeapon::AdjustByCof(FVector Aim)
{
	//Get the current cone of fire

	FWeaponRecoilData recoilData = GetCurrentRecoilData();

	/* CONE OF FIRE CALC
	BaseCof is the default cone of fire for the first shot.
	BloomModifier is how much the cone of fire blooms per shot.
	MaxCoF is the maximum value that the cone can get to.

	Cone Angle is calculated as:

	ActualCoF = BaseCof	+ (BloomModifier * NumberOfShots)

	Cone of Fire will always be BaseCoF < ActualCoF < MaxCoF
	*/

	float BaseCof = 0;
	float BloomModifier = 0;
	float MaxCof = recoilData.MaxStand;


	//TODO: Get movementstate
	//TODO: Get Standing/Crouching/Prone
	//TODO: Get ADS or Hip
	if (OwningCharacter->IsMoving())
	{
		BaseCof = recoilData.StandHipMove;
		BloomModifier = recoilData.StandingHipBloom;
	}
	else
	{
		BaseCof = recoilData.StandHipStill;
	}


	//Add cof bloom
	float AdjustedBloom = FMath::Min((BaseCof + (BloomModifier * ShotsFired)), MaxCof);


	return FMath::VRandCone(Aim, FMath::DegreesToRadians(AdjustedBloom / 2));

}
float ABaseEmpiresWeapon::RollVerticalRecoil()
{

	FWeaponRecoilData recoilData = GetCurrentRecoilData();
	
	float minRecoil = recoilData.VerticalRecoilMin;
	float maxRecoil = recoilData.VerticalRecoilMax;


	float recoilVal = FMath::FRandRange(minRecoil, maxRecoil);

	float recoilMultiplier = 1;
	
	//TODO: Get stance
	recoilMultiplier = recoilData.StandingRecoilMultiplier;

	if (ShotsFired == 0) recoilMultiplier += recoilData.FirstShotRecoilMultiplier;

	return recoilVal * recoilMultiplier;
}
float ABaseEmpiresWeapon::RollHorizontalRecoil()
{
	

	FWeaponRecoilData recoilData = GetCurrentRecoilData();
	//If Left and right recoil are both false, we don't horizontal recoil
	if (!recoilData.LeftRecoils && !recoilData.RightRecoils) return 0;


	float minRecoil = recoilData.HorizontalRecoilMin;
	float maxRecoil = recoilData.HorizontalRecoilMax;
	bool recoilRight = true;


	//Figure out if we are recoiling right or left
	if (recoilData.LeftRecoils && recoilData.RightRecoils) //If they are both true, pick a random recoil direction
	{
		recoilRight = (FMath::Rand() % 2) == 0;
	}
	else if (recoilData.LeftRecoils && !recoilData.RightRecoils) //If left is true and right is false, always recoil left
	{
		recoilRight = false;
	}
	//By default we recoil right

	float recoilVal = FMath::FRandRange(minRecoil, maxRecoil);

	//Scale the recoil direction by the biases
	if (recoilRight)
	{
		recoilVal *= recoilData.RightRecoilBias;
	}
	else
	{
		recoilVal *= recoilData.LeftRecoilBias;
		recoilVal = -recoilVal; //Invert the recoil direction
	}


	float recoilMultiplier = 1;

	//TODO: Get stance
	recoilMultiplier = recoilData.StandingRecoilMultiplier;

	if (ShotsFired == 0) recoilMultiplier += recoilData.FirstShotRecoilMultiplier;

	return recoilVal * recoilMultiplier;
}

/////////////////////////REPLICATION

void  ABaseEmpiresWeapon::OnRep_Reload()
{
	//TODO: Simulate Weaponing
}

void ABaseEmpiresWeapon::DealDamage(AEmpires2Character* Target)
{
	float damage = GetAmmoPool().Damage;

	FPointDamageEvent damageEvent;
	damageEvent.Damage = damage;
	//Fill in the rest of the damage event

	Target->TakeDamage(damage, damageEvent, OwningCharacter->GetController(), OwningCharacter);
}


void ABaseEmpiresWeapon::OnRep_WeaponState()
{
	
}

void ABaseEmpiresWeapon::OnBulletHit(const FHitResult& Hit)
{
	AEmpires2Character* Character = Cast<AEmpires2Character>(Hit.GetActor());
	if (Character)
	{
		DealDamage(Character);
	}

	BPOnBulletHit(Hit);
}

