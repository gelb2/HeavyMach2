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
