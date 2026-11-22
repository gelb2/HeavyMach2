# HeavyMach 2 validation

[한국어](../../public/validation.md) · [Documentation](index.md)

## Public releases and device checks

| Environment | Release and checks |
| --- | --- |
| iPhone and iPad | Build v0.4 and sign it with your own development team. The regular app was installed and relaunched on an iPhone Air and an iPad Air, and the save files matched. The player confirmed entry into White Mountain on the physical iPhone. [Install and save checks](../../save-transfer.md#호스트별-확인-범위) · [White Mountain device record](../../white-mountain-dual-release-20261120.md) |
| Android | The regular APK is v0.4 and the test APK is v0.1. The regular app on a Galaxy kept the existing slots after updating and opened the recovery-code entry screen. The regular app on an emulator applied the code, backed up the existing saves and retained the restored slots after relaunch. [Regular APK](https://github.com/gelb2/HeavyMach2/releases/tag/v0.4) · [Save checks](../../save-transfer.md#호스트별-확인-범위) |
| PWA | The public build is v0.3. On November 21, 2026, v0.2 on a Galaxy resumed in landscape and continued the background music after returning from the Home Screen. On November 22, v0.3 launched full screen from the Home Screen icon and stayed full screen after returning. The Mac Chrome installation reopened offline and retained its saves. [v0.2 record](../../pwa-lifecycle-audit-20261121.md#갤럭시-설치-앱) · [v0.3 record](../../pwa-lifecycle-audit-20261121.md#갤럭시-v03-설치-앱-확인) |
| Mac | The public v0.1 ZIP was opened on an Apple silicon Mac mini. The signature, portrait window, new slot and trip from the base to the shop and back were checked. [Public app check](../../public/release-policy.md#v01-공개-배포-확인) |

## Public source build

A fresh public clone prepared 1,576 campaign files after verifying the pinned package. All files matched the prepared release resources. The core passed 29 host tests. [Build steps](build-and-install.md#clone-and-prepare-the-resources)

| Platform | What was checked |
| --- | --- |
| iPhone and iPad | Installed regular and test Release builds in the simulators. Debug UI tests covered iPhone route changes and double-tap dash, and iPad movement, mission and inventory panels, pause and resume. The Release test app traversed all 14 White Mountain portal routes on each simulator. |
| Android | Built both ARM64 and x86-64 Debug editions. Ran the regular app in an ARM64 emulator, created a slot, entered Ruin Field and moved. The test app passed 27 White Mountain, boss, shop and air-strike checks. |
| PWA | Installed both web editions in Mac Chrome and reopened them offline with saves intact. |
| Tauri Mac | Verified the signatures of both local bundles. The regular app entered the field, moved and retained the slot after relaunch. The test edition reached the base using a separate slot. |

[Full clean-clone record](../../public/validation.md#공개-클론에서-빌드와-실행)

## What still needs a device check

The real iPad White Mountain command and portal checks, extended phone play, the opposite landscape rotation on Galaxy, and installed Safari offline restart remain open. The final campaign comparison will run the original HD port and both restoration editions on the same iPad. [The remaining checks](../../public/validation.md#남은-검증) · [Feature evidence](../../feature-verification.md)
