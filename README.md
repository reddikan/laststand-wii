# LAST STAND

A bullet-heaven roguelite survival shooter for the Nintendo Wii (homebrew), played with the Classic Controller in Dolphin or on a real Wii.

Hold the line against an endless horde across five cursed biomes, each darker and creepier than the last. Every biome is ten one-minute waves that keep getting nastier; between waves you spend your bounty at the trading post on outrageous guns and on yourself. At 10:00 the biome's boss crawls out. Put it down and ride on.

## Controls (Classic Controller)

| Button | Does |
| --- | --- |
| Left stick / D-Pad | move |
| Right stick | aim (let go and it auto-aims at the nearest monster - or the boss) |
| **A / B / X / Y** | switch to the gun on that button |
| **R or ZR** | fire the gun in your hand (or turn on AUTO FIRE in the menus) |
| L or ZL | dash - you can't be hit mid-dash |
| + | pause |
| - | 16:9 / letterboxed 4:3 |
| HOME | exit |

You carry up to four guns, one per button. The one in your hand fires when you pull the trigger; the other three float around you and fire on their own at a slower rate (the **Backup Rig** upgrade speeds them up).

## The guns

| Gun | What it does | Upgrades (up to level 8) |
| --- | --- | --- |
| **Hellfire Revolvers** | fast twin six-shooters | pierce more, *fan the hammer* at level 4 |
| **Dragon's Breath** | a shotgun full of fire that sets the horde alight | more pellets, leaves the ground burning |
| **Gatling Storm** | spins up, then hoses everything down (heavy to carry) | faster, piercing rounds, twin barrels |
| **Dynamite Bundle** | lobbed bundles, big booms | cluster bombs, mega blasts, two bundles at once |
| **Tesla Coil** | lightning that jumps from monster to monster | more jumps every level |
| **Sun Lance** | a beam that pierces everything in a line | triple beam, then five |

Shoot the **powder kegs** for chain reactions. **Gold elites** drop chests (a free gun level). Loot: **whiskey** heals, the **magnet** pulls in all the coin, the **TNT crate** blows up the whole screen, and the **purple skull** is a FRENZY - every gun at full tilt. Fall the first time and you get a **Last Stand**: five seconds of fury, then you get back up.

## The trading post

After every wave: new guns (pick the button for each), gun levels, and upgrades for yourself - Vitality, Armor, Boots, Magnet, Powder (damage), Trigger (fire rate), Luck, Backup Rig, and a swig of Whiskey.

## The armory

Dying keeps your **souls**. Spend them on new gunslingers - the Drifter (revolvers, crits), the Preacher (Dragon's Breath, tougher), Granny Gatling (gatling, armored) and Dynamite Doc (dynamite) - on gunslinger levels, and on perks for every run (starting cash, an extra Last Stand, more coin, faster backups). Beating a boss unlocks the next biome as a starting point.

## The biomes

1. **Dust Gulch** - sundown in the badlands. Skeleton bandits, buzzards, cactus golems, bone slingers. Boss: **the Bone Sheriff**.
2. **Rotwood Bayou** - murky swamp, drizzle, fireflies. Bog toads, swamp crows, moss brutes, plague lanterns. Boss: **the Bog Mother**.
3. **Gravewatch** - a graveyard in a thunderstorm. Zombies, bone swarms, coffin knights (shielded in front), grave wraiths. Boss: **the Undertaker**.
4. **Ashen Hollow** - a forest still burning. Charred husks, fire imps that explode, ash hulks, plague doctors. Boss: **the Plague Prophet**.
5. **Hellmouth** - the end of the road. Shadow crawlers, eye wisps, flesh hulks, void priests. Boss: **the Maw**.

## Saving

Souls, gunslingers, perks, unlocked biomes and settings save to `sd:/apps/laststand/save.dat`. In Dolphin this needs the virtual SD card: *Config > Wii > SD Card > Insert SD Card* (on by default in recent versions). The title screen says whether saving is working.

## Building

With devkitPro installed: `make` builds `boot.dol` (open it in Dolphin) and an `apps/laststand/` folder for a real Wii's SD card.

Pushed to GitHub, the Actions workflow builds the game in the devkitPro container and publishes it as `laststand_v1.dol` on the **v1** release.

## Tests

`sh tools/run_tests.sh` builds a native test harness (any C compiler; Pillow for PNG export) that walks the menus, the shop and the save, renders screenshots of every biome and boss through a software copy of the GPU path (including the lighting pass), and plays whole runs with bots of different skill to check the balance and the frame cost. `sh tools/run_tests.sh reel 3` renders a preview video of biome 3 with its music and sound (needs ffmpeg).

## How it is made

- Everything is drawn as coloured triangles by the GPU: flat shapes with chunky dark outlines for the cast, built at run time - no image files.
- Lighting is a coarse light grid rebuilt every frame from the gunslinger's lantern, muzzle flashes, tracers, explosions, burning ground, lightning arcs, the boss and glowing scenery (candles, lava, eyes), drawn as a 2x multiply over the scene so lights can over-brighten as well as darken; an additive pass adds the glows, the eyes in the dark, beams and sparks. Each biome's ambient light is darker than the last.
- Screen shake, slow motion, white flashes and a zoom punch when things get big.
- Every sound is synthesized at start-up, including a gravel-voiced announcer from a tiny formant speech synthesizer.
- The music is composed and synthesized at start-up: a whistled title theme, a galloping fight track with a metal layer that fades in as the chaos builds, an organ-and-drums boss track and a saloon piano for the shop. Each deeper biome plays it a little lower and darker.
