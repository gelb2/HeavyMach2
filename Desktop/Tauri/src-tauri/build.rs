fn main() {
    #[cfg(feature = "desktop")]
    tauri_build::try_build(tauri_build::Attributes::new().app_manifest(
        tauri_build::AppManifest::new().commands(&["store_read", "store_write", "finish_close"]),
    ))
    .expect("valid HeavyMach 2 desktop configuration");
}
