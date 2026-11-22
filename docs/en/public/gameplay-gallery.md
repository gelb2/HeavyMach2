# HeavyMach 2 gameplay screenshots and clips

[한국어](../../public/gameplay-gallery.md) · [How to play](quick-play.md) · [What the restoration adds](features.md)

## iPad gameplay recordings

In the first GIF, the player sets a destination between ponds and fights nearby tanks. In the second, the player draws a path along the coast and trades fire with enemies. Each is an uninterrupted eight seconds from the player's November 12, 2026 recording. [Timestamps and hashes](../../public/media/player-captures-20261121/sources.json)

<img src="../../public/media/player-captures-20261121/ipad-field-combat.gif" alt="An iPad portrait view of a mech moving between ponds and fighting tanks" width="378">

<img src="../../public/media/player-captures-20261121/ipad-combat-route.gif" alt="A green movement path and incoming fire on a coastal battlefield on iPad" width="378">

## iPhone Command Center and battlefield

The player captured these screens on an iPhone in landscape on November 14, 2026. They show the Command Center missions, the battlefield near the Skyway Base portal, and the system menu over the battlefield.

![A new mission notice and three active missions in the iPhone Command Center](../../public/media/player-captures-20261121/iphone-command.png)

![The Skyway Base portal, movement target, and HUD on an iPhone battlefield](../../public/media/player-captures-20261121/iphone-skyway-base.png)

![The iPhone system menu offers a return to base and save and quit](../../public/media/player-captures-20261121/iphone-system-menu.png)

## Movement paths and the shop on iPhone

The player changes the movement path in Ruin Field. Green dots and lines mark the new route. The Source app was recorded on an iPhone Air simulator using OS touch input. The clip uses seconds 36.8 to 42.9, rotated to landscape. [MP4, 6.1 seconds](../../public/media/gameplay-20261121/iphone-route.mp4) · [Input reproduction and fix](../../iphone-module-audit-20261118.md)

The iPhone simulator screenshot below shows goods, inventory, and page buttons in the Wasteland shop. [Comparison with original shop coordinates](../../iphone-shop-reference-coordinates-20261119.md)

![The iPhone Wasteland shop shows goods on the left and inventory with page buttons on the right](../../public/media/gameplay-20261121/iphone-shop.png)

## Portrait combat on iPad

The regular app was checked on an 11-inch iPad Pro simulator using a new profile, with trips between the base, Command Center, and field. The clip shows the portrait view and standard HUD in Ruin Field. [MP4, 12 seconds](../../public/media/gameplay-20261121/ipad-field.mp4) · [HUD and input checks by model](../../campaign-and-ipad-followup-20261120.md#ipad-모델별-hud)

<img src="../../public/media/gameplay-20261121/ipad-field.png" alt="A long bridge, tanks, and the standard HUD in portrait on an iPad" width="475">

## White Mountain base and snowfields

The **White Mt. Base** portal is in the upper-right corner of the Desert 01 screenshot from the test build on a 13-inch iPad Pro (M4) simulator. The base exit menu and Snow Pass arrival come from the test build on an iPhone simulator. At the base, **Desert 01** returns to the desert and **Snow Pass** leads into the first snowfield. [Portal and physical-device record](../../white-mountain-dual-release-20261120.md)

<img src="../../public/media/white-mountain-gameplay/desert-01-portal.webp" alt="The White Mt. Base entry portal at upper right in Desert 01" width="450">

![The Desert 01 and Snow Pass buttons at White Mt. Base](../../public/media/white-mountain-gameplay/white-mountain-base.webp)

![Arrival in Snow Pass with the return portal at left](../../public/media/white-mountain-gameplay/snow-pass.webp)

Snow Fort has newly placed tanks and turrets and collision boundaries around the rocks. The map data records aircraft placement. The shared-core tests checked aircraft fire and player damage. [Snowfield enemy placement](../../campaign-and-ipad-followup-20261120.md#설원-적-배치-결과) · [Weapon fire and damage checks](../../white-mountain-implementation-20261119.md#단계별-실행-결과)

![The fortress, rocks, player mech, and nearby enemies in Snow Fort](../../public/media/white-mountain-gameplay/snow-fort.webp)

The same iPhone simulator test run captured arrival in Snow Field, Snow Ridge, and Snow Valley. Snow Basin appears in the boss fight below.

![The Snow Field arrival point and tanks deployed on the field](../../public/media/white-mountain-gameplay/snow-field.webp)

![The Snow Ridge arrival point and ridge battlefield](../../public/media/white-mountain-gameplay/snow-ridge.webp)

![The Snow Valley arrival point and valley battlefield](../../public/media/white-mountain-gameplay/snow-valley.webp)

In the test build on a 13-inch iPad Pro (M4) simulator, the player approached E_Plane_01 in Snow Pass. The orange aircraft just to the right of center flashes as it fires its first shot.

<img src="../../public/media/white-mountain-gameplay/snow-pass-aircraft-fire.webp" alt="The orange E_Plane_01 aircraft fires its first shot in Snow Pass" width="450">

## White Mountain boss fight

This iPhone 17 Pro simulator screenshot came from the test build after accepting the Snow Basin boss mission and entering the field. Boss 05's health bar and firing turrets are visible. [Mission, spawn, defeat, and reward checks](../../white-mountain-implementation-20261119.md#단계별-실행-결과)

![Boss 05 fires in the snowfield and hits the player mech. The Snow Pass return portal is on the left.](../../public/media/gameplay-20261121/snow-boss.png)

The next three screenshots came from the test build on an iPad simulator. Accept the Boss 05 mission at the Command Center, fight it inside Snow Basin, then report the victory to collect the reward.

<img src="../../public/media/white-mountain-gameplay/boss-05-mission.webp" alt="The Boss 05 mission is available at the White Mountain Command Center" width="245"> <img src="../../public/media/white-mountain-gameplay/boss-05-combat.webp" alt="Boss 05's health bar during the fight in Snow Basin" width="245"> <img src="../../public/media/white-mountain-gameplay/boss-05-reward.webp" alt="The White Mountain Command Center shows the reward after the victory is reported" width="245">

## Sniper rifles and LS weapons

The test build on an iPad simulator shows the Sn-IGn rifle (10501) in the shop, then Sn-IGn and LS-series weapon 10311 equipped and firing in separate Ruin Field runs. The runs checked equipment, firing, and damage. The data records the Sn-IGn shop listing and Boss 05 drop, and the Tank_L02 drop for 10311 in regular play. [Shop and drop data](../../../Resources/Campaign/items.json) · [Weapon checks](../../white-mountain-implementation-20261119.md#단계별-실행-결과)

<img src="../../public/media/white-mountain-gameplay/sniper-shop.webp" alt="Sn-IGn selected in the shop" width="245"> <img src="../../public/media/white-mountain-gameplay/sniper-combat.webp" alt="Firing the equipped Sn-IGn in Ruin Field" width="245"> <img src="../../public/media/white-mountain-gameplay/ls-combat.webp" alt="Firing the equipped LS 10311 in Ruin Field" width="245">

In a separate test run on a 13-inch iPad Pro (M4) simulator, the player defeated E_Tank_02 in combat in Ruin Field. The test used a fixed random seed to reproduce a rare result from the public drop table. The two frames show LS-D (10311) dropping and the acquired weapon selected in the inventory.

<img src="../../public/media/white-mountain-gameplay/ls-10311-drop.webp" alt="LS-D drops as loot when E_Tank_02 is defeated in Ruin Field" width="360"> <img src="../../public/media/white-mountain-gameplay/ls-10311-inventory.webp" alt="The acquired LS-D is selected in the inventory" width="360">

## Laser air strike and Item Picker BOTs

In the test build on an iPad simulator, the level 22 shop sold **Laser Air-Strike** for 30 Cr in the special attacks tab. The next two screenshots show it assigned to a quick slot and used on a target in Ruin Field. [Air-strike scaling and purchase check](../../campaign-and-ipad-followup-20261120.md#공습-레벨-보정)

<img src="../../public/media/white-mountain-gameplay/laser-airstrike-buy.webp" alt="The Laser Air-Strike purchase dialog lists 30 Cr for one strike" width="245"> <img src="../../public/media/white-mountain-gameplay/laser-airstrike-quick-slot.webp" alt="Laser Air-Strike assigned to a Ruin Field quick slot" width="245"> <img src="../../public/media/white-mountain-gameplay/laser-airstrike-active.webp" alt="The laser air strike begins after being used from the quick slot" width="245">

All four frames come from one Ruin Field run in the test build on a 13-inch iPad Pro (M4) simulator. They show Item Picker D equipped in the first BOT slot, the BOT approaching enemy loot, the field just after delivery, and the inventory after delivery. The inventory balance increased by 3 Cr. In earlier runs, Picker E and F also delivered one item each. [Item Picker runtime record](../../white-mountain-implementation-20261119.md#단계별-실행-결과)

<img src="../../public/media/white-mountain-gameplay/picker-d-equipped.webp" alt="Item Picker D equipped in the first BOT slot" width="360"> <img src="../../public/media/white-mountain-gameplay/picker-d-approach.webp" alt="Item Picker D approaches enemy loot in Ruin Field" width="360">

<img src="../../public/media/white-mountain-gameplay/picker-d-collection.webp" alt="Ruin Field just after Item Picker D delivered the loot" width="360"> <img src="../../public/media/white-mountain-gameplay/picker-d-inventory-after.webp" alt="The inventory shows the balance increased by 3 Cr after delivery" width="360">

## Regular Galaxy app and the installed Mac PWA

On the regular Galaxy app, a fresh test slot entered Ruin Field and a tap on the terrain moved the mech. The screenshot shows the movement target in front of the mech and the HUD over the field. [Regular and test Galaxy app checks](../../../Android/검증-20261120.md)

![A green movement target in front of the mech in the Galaxy landscape view](../../public/media/gameplay-20261121/galaxy-field.png)

The test PWA installed from Mac Chrome was checked in Ruin Field as well. The browser output buffer confirmed machine-gun, missile, impact, and explosion sounds. [Installed PWA audio check](../../code-document-fixes-20261121.md#실행-근거)

![The player mech and enemy tanks fight in Ruin Field in the Mac Chrome app](../../public/media/gameplay-20261121/pwa-combat.png)

Both MP4 clips come from silent simulator recordings. [Recording paths, timestamps, and hashes](../../public/media/gameplay-20261121/sources.json)
