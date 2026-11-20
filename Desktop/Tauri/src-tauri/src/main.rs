use hm2_desktop::storage::{self, Key};
use std::{
    path::PathBuf,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
};
use tauri::{Manager, RunEvent, State, Url, WebviewUrl, WebviewWindow, WindowEvent};

#[derive(Default)]
struct AppState {
    storage: Arc<Mutex<()>>,
    exiting: AtomicBool,
}

fn packaged_url(url: &Url) -> bool {
    url.scheme() == "tauri" && url.host_str() == Some("localhost")
}

fn storage_dir(window: &WebviewWindow) -> Result<PathBuf, String> {
    if window.label() != "main" || !window.url().is_ok_and(|url| packaged_url(&url)) {
        return Err("The packaged game window is required".into());
    }
    window
        .app_handle()
        .path()
        .app_data_dir()
        .map(|path| path.join("progress"))
        .map_err(|error| error.to_string())
}

#[tauri::command]
async fn store_read(
    window: WebviewWindow,
    state: State<'_, AppState>,
    key: String,
) -> Result<Option<Vec<u8>>, String> {
    let dir = storage_dir(&window)?;
    let key = Key::parse(&key).map_err(|error| error.to_string())?;
    let lock = Arc::clone(&state.storage);
    tauri::async_runtime::spawn_blocking(move || {
        let _guard = lock.lock().map_err(|error| error.to_string())?;
        storage::read(&dir, key).map_err(|error| error.to_string())
    })
    .await
    .map_err(|error| error.to_string())?
}

#[tauri::command]
async fn store_write(
    window: WebviewWindow,
    state: State<'_, AppState>,
    key: String,
    bytes: Vec<u8>,
) -> Result<(), String> {
    let dir = storage_dir(&window)?;
    let key = Key::parse(&key).map_err(|error| error.to_string())?;
    let lock = Arc::clone(&state.storage);
    tauri::async_runtime::spawn_blocking(move || {
        let _guard = lock.lock().map_err(|error| error.to_string())?;
        storage::write(&dir, key, &bytes).map_err(|error| error.to_string())
    })
    .await
    .map_err(|error| error.to_string())?
}

#[tauri::command]
fn finish_close(window: WebviewWindow, state: State<'_, AppState>) -> Result<(), String> {
    storage_dir(&window)?;
    state.exiting.store(true, Ordering::Release);
    window.app_handle().exit(0);
    Ok(())
}

fn close_after_storage(window: &WebviewWindow) {
    if let Err(error) = window.eval("window.HM2Desktop.close().catch(console.error)") {
        eprintln!("Could not request a clean close: {error}");
    }
}

fn main() {
    let window_size = if std::env::args_os().any(|arg| arg == "--portrait-check") {
        (820.0, 900.0)
    } else {
        (1200.0, 800.0)
    };
    let app = tauri::Builder::default()
        .plugin(tauri_plugin_single_instance::init(|app, _, _| {
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_focus();
            }
        }))
        .manage(AppState::default())
        .invoke_handler(tauri::generate_handler![
            store_read,
            store_write,
            finish_close
        ])
        .setup(move |app| {
            let window =
                tauri::WebviewWindowBuilder::new(app, "main", WebviewUrl::App("index.html".into()))
                    .title(
                        app.config()
                            .product_name
                            .as_deref()
                            .unwrap_or("HeavyMach 2 Desktop"),
                    )
                    .inner_size(window_size.0, window_size.1)
                    .min_inner_size(800.0, 540.0)
                    .on_navigation(packaged_url)
                    .on_new_window(|_, _| tauri::webview::NewWindowResponse::Deny)
                    .initialization_script(include_str!("../../frontend/desktop-host.js"))
                    .build()?;
            let handle = window.clone();
            window.on_window_event(move |event| {
                if let WindowEvent::CloseRequested { api, .. } = event {
                    api.prevent_close();
                    close_after_storage(&handle);
                }
            });
            Ok(())
        })
        .build(tauri::generate_context!())
        .expect("open HeavyMach 2 Desktop");
    app.run(|app, event| {
        if let RunEvent::ExitRequested { api, .. } = event {
            if !app.state::<AppState>().exiting.load(Ordering::Acquire) {
                if let Some(window) = app.get_webview_window("main") {
                    api.prevent_exit();
                    close_after_storage(&window);
                }
            }
        }
    });
}
