# HeavyMach 2 Quick Play

[한국어](../../public/quick-play.md) · [Documentation](index.md) · [Build and install](build-and-install.md)

Open the regular app. For the PWA, launch it from its installed icon after the initial download. Play on a phone in landscape or on an iPad in portrait. [Install the game](build-and-install.md#play-the-published-builds)

## Start or continue

Tap the title and select a slot. Choose an empty slot and enter a name of one to ten characters for a new game. Choose an existing slot and press **OK** to continue at the saved base or field. **DEL** deletes the selected slot.

At the base, tap a facility to see its description. Tap it again to enter. In the command center, open a mission and press **ACCEPT**. The shop buys and sells items, and the equipment screen shows what the mech carries. Select the exit to enter the field.

## Move and fight

| Input | Action |
| --- | --- |
| Tap the ground | Move to that point. |
| Drag across the ground | Draw a route through several waypoints. |
| Tap an enemy | Select it as a target. Equipped weapons fire when the target and range permit. |
| Drag from a selected enemy to another enemy | Select another target. |
| Double-tap the ground | Dash when the booster and current state allow it. |
| Tap a left or right weapon icon | Open its weapon selection list. |
| Tap a quick-slot item | Use it according to its rules. Tap a target in the field if the item requires one. |

To save your progress and quit, open **MENU** on the battlefield and choose the save-and-exit option. HUD placement is available from the field menu. Drag pieces to new positions and apply the layout. Choose the default layout and apply it to restore the original positions. [HUD and gameplay captures](../../public/gameplay-gallery.md)

If `OUT OF RANGE` stays on screen, check your selected target and the equipped weapon’s range. A [player report](../../todo.md) describes the warning staying visible within range.

## Move progress to another device

Select **TRANSFER** (**저장 이동** in Korean) on the slot screen and copy the full recovery code. If the receiving app already has progress, back it up with its own code first. Open its save-transfer menu, paste the code, and confirm the replacement. The code transfers all three slots, including missions, equipment and experience. HUD placement stays with the app that stored it.

Browser saves are tied to the site address. Copy a code from the old address before moving to another one. [Recovery formats and device checks](../../save-transfer.md)

## Start at level 22 with a recovery code

The [level 22 recovery code](../../public/saves/level-22.txt) starts you at Ruin Field Base in slot 1, `XP SAMPLE`, with **366,785 EXP**, **38,417 Cr**, and starting equipment. Mission progress is 0%. Slots 2 and 3 are empty.

1. Select **TRANSFER** (**저장 이동** in Korean) on the slot screen and copy your current code somewhere outside the game. Importing the sample replaces all three slots.
2. Copy the entire line from the sample file, starting with `HM2-1-`, and paste it into the save transfer menu. Check the code and confirm the slot replacement.
3. Select `XP SAMPLE` in slot 1 and press **OK**.

The regular v0.2 PWA installed through Chrome on Mac restored a fresh slot from level 1 and 0 EXP to level 22 and 366,785 EXP. Both values persisted after an offline relaunch, and the restored slot opened at Ruin Field Base. [Save values and backup check](../../save-transfer.md#경험치-복원-검사)
