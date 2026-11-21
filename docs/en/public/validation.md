# HeavyMach 2 validation

[한국어](../../public/validation.md) · [Documentation](index.md)

## Public source build

A fresh public clone prepared 1,576 campaign files after verifying the pinned package. All files matched the prepared release resources. The core passed 29 host tests. [Build steps](build-and-install.md#clone-and-prepare-the-resources)

| Platform | What was checked |
| --- | --- |
| iPhone and iPad | Installed regular and test Release builds in the simulators. Debug UI tests covered iPhone route changes and double-tap dash, and iPad movement, mission and inventory panels, pause and resume. The Release test app traversed all 14 White Mountain portal routes on each simulator. |
| Android | Built both ARM64 and x86-64 Debug editions. Ran the regular app in an ARM64 emulator, created a slot, entered Ruin Field and moved. The test app passed 27 White Mountain, boss, shop and air-strike checks. |
| PWA | Installed both web editions in Mac Chrome and reopened them offline with saves intact. On a Galaxy, the published v0.2 installed PWA returned from the Home Screen in landscape and resumed background music. |
| Tauri Mac | Verified the signatures of both local bundles. The regular app entered the field, moved and retained the slot after relaunch. The test edition reached the base using a separate slot. |

[Full clean-clone record](../../public/validation.md#공개-클론에서-빌드와-실행) · [Galaxy PWA lifecycle checks](../../pwa-lifecycle-audit-20261121.md)

## What still needs a device check

The real iPad White Mountain command and portal checks, extended phone play, the opposite landscape rotation on Galaxy, and installed Safari offline restart remain open. The original HD comparison still needs a final pass over the campaign. [The remaining checks](../../public/validation.md#남은-검증) · [Feature evidence](../../feature-verification.md)
