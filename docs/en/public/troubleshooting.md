# HeavyMach 2 troubleshooting

I reported symptoms during hands-on play. This page records the conditions Codex reproduced, the causes, and the fixes. Each entry identifies the checks made on a physical device, in the test app, in a simulator, or on the host. [한국어](../../public/troubleshooting.md) · [Development history](development-history.md) · [Current platform checks](../../canonical.md#플랫폼과-검증-현황)

## 1. Double-tap dash fails after changing the route on iPhone

I reported that the dash sometimes failed when I drew a new route and double-tapped on iPhone. Repeating the input on an iPhone simulator showed that UIKit sent a move event after a small change in touch coordinates even when both positions mapped to the same canvas point. This event cleared the first tap.

Codex filtered those move events when they mapped to the same canvas point and were about to reach battlefield input. Menu and HUD move events still follow their existing order. Each iPhone and iPad simulator ran five single-point route changes and five multi-point route changes. All ten attempts per simulator triggered one dash and used 100 SP. The failure rate with repeated input on a physical device remains unmeasured. [Input reproduction and simulator results](../../iphone-module-audit-20261118.md#수정과-시뮬레이터-검증)

## 2. Sudden zoom changes while moving and background jitter in Skyway

I reported abrupt changes in battlefield zoom on iPhone and slight background jitter after leaving Wasteland Base for Skyway. As the moving mech approached the standard HUD, the camera applied a new minimum zoom immediately. In Skyway, small numerical differences caused horizontal clamping to flicker when the background and viewport were almost the same width.

Codex made the HUD adjustment ease toward the target zoom and centered the background when there was no horizontal room to pan. Across 37,440 ticks on 78 maps, iPhone zoom changes greater than 0.01 per tick fell from 19 to zero. In 300 iPhone simulator frames at Skyway, the maximum horizontal background movement fell from 3.170 pt to 0.000031 pt. The two comparison frames on iPad matched pixel for pixel before and after the change. The same route still needs a continuous frame capture on a physical iPhone. [Measurements, fix, and checks](../../iphone-module-audit-20261118.md#수정과-시뮬레이터-검증) · [Route into Skyway](../../iphone-module-audit-20261118.md#wasteland-base에서-skyway로-나간-뒤의-떨림)

## 3. The iPhone shop shows empty slots but declines a laser purchase

I reported that the shop declined a laser purchase while the inventory appeared to have room. All 75 slots were occupied in the save copy. The screen split each 25-slot page into groups of 20 and five, yet drew a 20-slot background for the second group. Screens two, four, and six each showed 15 empty-looking slots that did not exist in the save.

Codex adjusted the shop and equipment backgrounds to the number of actual slots. With the same save copy on the host, an LS-102 purchase failed when the inventory was full and succeeded after one slot was cleared. Captures of all six shop and equipment screens on the iPhone simulator showed 20, five, 20, five, 20, and five slots. The updated build still needs a check on a physical device. [Save copy and screen checks](../../iphone-inventory-last-page-20261120.md)

### The base background shows through the iPhone shop

I asked for new captures from the original iPhone app to compare the sizes of icons, detail panels, and trade buttons, as well as pagination. The base artwork showed through the existing iPhone shop. An edited version of the hangar artwork supplies the empty shop cells and panels for the shop and equipment screens. The shared UI still handles the actual slots and buttons. Simulator checks compared the covered screen and pixels across six existing iPad shops. [Original shop size, background, and buttons](../../iphone-shop-reference-coordinates-20261119.md)

## 4. Laser Air-Strike is missing from the iPad shop

I was looking for Laser Air-Strike `02006` in the special attacks tab. Its item level in the restoration data was 30, so the shop only listed it at levels 24 through 50. My save was at level 22.

Codex restored the original item's level of zero so it follows the same all-level availability rule as the other air strikes. The iPhone and iPad test apps at level 22 confirmed the listing, BUY button, 30 Cr payment, and quick-slot assignment. On the battlefield, the strike consumed the item, spawned ten projectiles, reduced enemy HP, and played its assigned effect. [Shop, payment, and combat checks](../../campaign-and-ipad-followup-20261120.md#특수공격-탭의-레이저-공습)

## 5. Snowfield bosses appear at the portal

I pointed out that seeing the boss as soon as I entered a snowfield felt different from other battlefields. The bosses were moved farther into their maps. The existing spawner now waits until the player has accepted the mission and is within 240 game units of the boss position. The base exit labels also use the bold italic Trebuchet used by existing portals.

The iPhone and iPad test apps each ran all 14 portal directions and four boss missions. Bosses stayed hidden on arrival and appeared on approach. The checks covered combat, defeat, and saving the rewards. I confirmed entry into White Mountain in the regular iPhone app. [Portals, fonts, bosses, and regular app checks](../../white-mountain-dual-release-20261120.md)

## 6. Tapping Shop in a Safari tab exits the base

On an iPhone 17 Pro simulator in landscape, the Safari tab provided a canvas of 874 × 338. Tapping Shop in the first base opened the slot screen. A shared C++ check at the same size showed that the Exit touch area, moved inside the viewport, overlapped the center of Shop. Exit received the touch first. At the same device classification and width and a height of 402, the Shop button received touches at all six bases.

I asked for the installed PWA to be checked from its icon. In the first base of the test edition launched from the iPhone simulator Home Screen, Shop and Exit each responded as expected. The regular edition also completed a trip to the shop and back. The 874 × 338 Safari tab overlap and repeated checks on a physical device remain open. [Shared-code reproduction coordinates](../../pwa-wasm-audit-20261120.md#현재-범위의-화면-검사) · [Home Screen launch and touch checks](../../pwa-device-validation-20261120.md#iphone-홈-화면에서-실행한-치트판)

## 7. Tauri Mac shows a black or distorted screen

I saw a corrupted or black screen while testing the Tauri Mac app. In the web runtime used by both PWA and Tauri, resizing with the save transfer dialog open or recovering the WebGL context could leave the view undrawn while gameplay was paused. Codex changed the runtime to redraw under those conditions while gameplay remains paused.

In the Desert base equipment shop, a small original panel was combined with shared HD button coordinates, leaving black space and misaligned slots. The original hangar artwork and shared HD inventory panel aligned the artwork with the item drop coordinates. Chromium checks covered window resizing and WebGL recovery. On a Mac mini, checks covered entering the equipment shop, moving an item, and returning to base. A regular build from a fresh clone entered the base and field, moved, and kept its save after relaunch. The test build reached the base using a separate slot. Trips through the facilities in the portrait test build and Mac audio still need checks. [Public clone run](validation.md#public-source-build) · [Reproduction and fix records](../../todo.md#공유-복구코드와-웹mac-후속-작업--2026-11-20) · [Tauri portrait window](../../pwa-device-validation-20261120.md#tauri-mac-세로-창)

## 8. The Galaxy PWA returns from the Home Screen in portrait

After returning to the installed public v0.1 app from the Home Screen, the battlefield changed to 384 × 749 in portrait. The install manifest had no orientation. Updating the web cache alone did not refresh the orientation of the app managed by Chrome.

Version 0.2 sets the phone installation orientation to `landscape`. If the OS supplies a portrait viewport, the game pauses and asks the player to rotate the phone. After updating the installed app on the Galaxy, the check confirmed Android's `SENSOR_LANDSCAPE` setting and a 755 × 354 landscape view on return. The opposite landscape rotation still needs a check on the physical device. [Reproduction and Galaxy installation update](../../pwa-lifecycle-audit-20261121.md#갤럭시-설치-앱)

## 9. PWA music does not resume after returning from the Home Screen

Leaving for the Home Screen used to stop the music source. Returning created a new one. The web music adapter now keeps the source and playback position, suspends the audio context, and resumes it on return. A scene change that requests a different track creates a new source.

We left a live battlefield in the public v0.2 PWA on a Galaxy and returned from the Home Screen. The same source resumed playback from its existing position. Sound effects use the existing shared OpenAL path. [Music code and check](../../pwa-lifecycle-audit-20261121.md#원인과-수정)

## 10. A fresh public clone cannot build without campaign resources

The public checkout contained the core, and the campaign files and Apple icons needed by the four platforms still had to be prepared. A new command downloads packages from a pinned public PWA build, verifies the SHA-256 hashes, and prepares the resources and icons.

A fresh public clone prepared 1,576 campaign files. They matched the existing release resources byte for byte, and all four platform builds succeeded. [Resource preparation steps](build-and-install.md#clone-and-prepare-the-resources) · [Runtime checks](validation.md#public-source-build)
