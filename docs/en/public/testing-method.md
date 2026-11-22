# How HeavyMach 2 was tested

[한국어](../../public/testing-method.md) · [Development history](development-history.md) · [Validation results](validation.md) · [Troubleshooting](troubleshooting.md)

## Comparison with the original game

We examined tables, artwork, functions, and running screens in the regular and HD editions. The regular edition was the primary reference for iPhone menu layouts. The HD edition was the primary reference for the campaign and shared rules. With the same save and inputs, the Command Center awarded 80 Cr and then 100 Cr in both the original HD and the restoration. The complete campaign and final comparison against the original HD remain open. [Command Center comparison](../../command-detail-restoration-20261111.md) · [Evidence by feature](../../feature-verification.md)

## Starting conditions in the test edition

The test edition sets levels, equipment, missions, and positions for repeatable checks. Once set up, movement, weapon fire, hits, purchases, and saves run through the shared core. The main check ran 126 conditions on each iPhone and iPad simulator. Separate runs checked 14 White Mountain portal routes, four boss missions, and 29 equipment conditions. [Build the test app and find its results in Korean](../../public/build-and-install.md#치트-앱-실행과-결과) · [Snowfield results](../../white-mountain-implementation-20261119.md)

## Regular app and actual input

During hands-on play, the player found jump failures after changing a route, Skyway jitter, empty-looking shop slots, and the missing laser air strike. After the fixes, the iPhone check used OS touch input to redraw the route and double-tap. On iPad, it opened a save, moved on the battlefield, opened the mission and inventory panels, and paused and resumed. The records identify regular-device play and automated simulator checks separately. [iPhone reproduction and fix](../../iphone-module-audit-20261118.md) · [Public clone checks](validation.md#public-source-build) · [Player captures](gameplay-gallery.md)

## Installation and performance

A browser tab, an installed PWA, and a Tauri window run in different conditions. On a Galaxy, the installed app was launched from its icon, sent to the Home Screen, and reopened to check landscape and music resumption. The app installed from Mac Chrome also reopened offline and retained its saves. Separate checks observed whether artwork or sound resources were repeatedly recreated. Long sessions to measure frame rate and heat on physical devices, offline restart in an installed Safari app, and the opposite landscape rotation remain open. [Galaxy PWA record](../../pwa-lifecycle-audit-20261121.md) · [Asset and output checks](../../code-document-fixes-20261121.md)
