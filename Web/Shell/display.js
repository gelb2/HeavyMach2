export const installedDisplayQuery = '(display-mode: standalone), (display-mode: fullscreen)';

// Take a stable device class once. Rotation, split screen and resizing change
// only the viewport and safe insets; a narrow Mac window stays desktop.
export function screenProfile(navigator, screen, coarse, desktopHost) {
  if (desktopHost) return 'desktop';
  const ua = navigator.userAgent ?? '';
  if (/ipad/i.test(ua) || (/macintosh/i.test(ua) && navigator.maxTouchPoints > 1)) return 'tablet';
  if (/iphone|ipod/i.test(ua)) return 'phone';
  const shortest = Math.min(screen.width, screen.height);
  const usableScreen = Number.isFinite(shortest) && shortest > 0;
  if (/android/i.test(ua)) return usableScreen ? (shortest >= 600 ? 'tablet' : 'phone') : null;
  if (/windows nt/i.test(ua)) return 'desktop';
  if (coarse && navigator.maxTouchPoints > 0 && usableScreen) return shortest >= 600 ? 'tablet' : 'phone';
  if (/macintosh|cros|x11|linux (?:x86_64|aarch64)/i.test(ua)) return 'desktop';
  return null;
}

// In a full-width iPad Home Screen app, WebKit reports the full window in
// outerHeight and takes 32 CSS pixels from the top of the document for the
// system status bar. Keep the game in window coordinates, anchored below at
// the same bottom edge. Other window sizes use their drawable bounds.
export function ipadHomeScreenTopOffset(viewport, outer, screen) {
  const statusBarHeight = 32;
  if (outer.width !== screen.width || outer.height !== screen.height ||
      viewport.width !== outer.width || viewport.height <= viewport.width) return 0;
  return outer.height - viewport.height === statusBarHeight ? statusBarHeight : 0;
}

// The browser can ignore the manifest orientation. Never start or resize the
// game in that orientation. Native macOS and the desktop browser preview
// keep their own window policies.
export function requiresRotation(profile, landscape, desktop, installed) {
  if (profile === 'phone') return !landscape;
  return landscape && !desktop && (profile === 'tablet' || installed);
}
