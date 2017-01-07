#pragma once

namespace PanzerChasm
{

namespace Sound
{

struct SoundId
{
	enum : unsigned char
	{
		MenuScroll= 2u,
		MenuOn= 3u,
		MenuChange= 4u,
		MenuSelect= 5u,
		Jump= 12u,
		JumpNo= 13u,
		Step= 14u,
		StepRun= 15u,
		FirstWeaponFire= 21u,
		FirstRocketHit= 30u,
		FirstWeaponPickup=  51u,

		ItemUp= 60u,
		Message= 61u,
		Health= 62u,
		GetKey= 63u,
		BreakGlass0= 68u,
		BreakGlass1= 69u,
		BreakGlass2= 70u,
		MineOn= 72u,
	};
};

struct MonsterSoundId
{
	enum : unsigned char
	{
		Alarmed= 0u,
		RemoteAttack= 1u,
		MeleeAttack= 2u,
		Pain= 4u,
		Death= 6u,
	};
};

// Player have differnet from other monsters sounds set.
struct PlayerMonsterSoundId
{
	enum : unsigned char
	{
		Pain0= 0u,
		Pain1= 1u,
		Death= 6u,
	};
};

} // namespace Sound

} // namespace PanzerChasm
