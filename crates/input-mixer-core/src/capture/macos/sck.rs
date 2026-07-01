//! ScreenCaptureKit-based application enumeration and capture.
//!
//! Uses a minimal Objective-C shim (`sck_shim.mm`) for APIs not yet in objc2.

use std::sync::{Arc, Mutex};

use crate::sources::{SourceId, SourceKind};

use super::super::platform::{AppCapture, PlatformError};

extern "C" {
    fn input_mixer_has_microphone_access() -> i32;
    fn input_mixer_is_microphone_access_denied() -> i32;
    fn input_mixer_should_request_microphone_access() -> i32;
    fn input_mixer_set_microphone_prompted(prompted: i32);
    fn input_mixer_request_microphone_access() -> i32;
    fn input_mixer_sck_has_screen_capture_access() -> i32;
    fn input_mixer_sck_screen_capture_access_state() -> i32;
    fn input_mixer_sck_refresh_screen_capture_access();
    fn input_mixer_sck_is_adhoc_build() -> i32;
    fn input_mixer_should_request_screen_capture_access() -> i32;
    fn input_mixer_sck_request_screen_capture_access() -> i32;

    fn input_mixer_sck_enumerate_apps(
        names: *mut *mut *mut std::ffi::c_char,
        bundle_ids: *mut *mut *mut std::ffi::c_char,
        subtitles: *mut *mut *mut std::ffi::c_char,
        window_ids: *mut *mut u64,
        count: *mut usize,
    ) -> i32;

    fn input_mixer_sck_start_capture(
        bundle_id: *const std::ffi::c_char,
        window_id: u64,
        callback: extern "C" fn(*const f32, usize, *mut std::ffi::c_void),
        ctx: *mut std::ffi::c_void,
    ) -> i32;

    fn input_mixer_sck_stop_capture();
}

pub fn has_microphone_access() -> bool {
    unsafe { input_mixer_has_microphone_access() != 0 }
}

pub fn is_microphone_access_denied() -> bool {
    unsafe { input_mixer_is_microphone_access_denied() != 0 }
}

pub fn should_request_microphone_access() -> bool {
    unsafe { input_mixer_should_request_microphone_access() != 0 }
}

pub fn set_microphone_prompted(prompted: bool) {
    unsafe {
        input_mixer_set_microphone_prompted(if prompted { 1 } else { 0 });
    }
}

pub fn request_microphone_access() -> bool {
    unsafe { input_mixer_request_microphone_access() != 0 }
}

pub fn has_screen_capture_access() -> bool {
    unsafe { input_mixer_sck_has_screen_capture_access() != 0 }
}

/// 0 = granted, 1 = not determined, 2 = needs relaunch, 3 = adhoc rebuild needs re-grant
pub fn screen_capture_access_state() -> i32 {
    unsafe { input_mixer_sck_screen_capture_access_state() }
}

pub fn refresh_screen_capture_access() {
    unsafe {
        input_mixer_sck_refresh_screen_capture_access();
    }
}

pub fn is_adhoc_build() -> bool {
    unsafe { input_mixer_sck_is_adhoc_build() != 0 }
}

pub fn should_request_screen_capture_access() -> bool {
    unsafe { input_mixer_should_request_screen_capture_access() != 0 }
}

pub fn request_screen_capture_access() -> bool {
    unsafe { input_mixer_sck_request_screen_capture_access() != 0 }
}

pub fn enumerate_application_sources() -> Result<Vec<SourceId>, PlatformError> {
    if has_screen_capture_access() {
        if let Ok(sources) = enumerate_via_sck() {
            if !sources.is_empty() {
                return Ok(sources);
            }
        }
    }
    Ok(fallback_enumerate())
}

fn enumerate_via_sck() -> Result<Vec<SourceId>, PlatformError> {
    let mut names: *mut *mut std::ffi::c_char = std::ptr::null_mut();
    let mut bundle_ids: *mut *mut std::ffi::c_char = std::ptr::null_mut();
    let mut subtitles: *mut *mut std::ffi::c_char = std::ptr::null_mut();
    let mut window_ids: *mut u64 = std::ptr::null_mut();
    let mut count: usize = 0;

    let rc = unsafe {
        input_mixer_sck_enumerate_apps(
            &mut names,
            &mut bundle_ids,
            &mut subtitles,
            &mut window_ids,
            &mut count,
        )
    };

    if rc != 0 || count == 0 {
        return Ok(Vec::new());
    }

    let mut sources = Vec::with_capacity(count);
    for i in 0..count {
        unsafe {
            let name = std::ffi::CStr::from_ptr(*names.add(i))
                .to_string_lossy()
                .into_owned();
            let bundle = std::ffi::CStr::from_ptr(*bundle_ids.add(i))
                .to_string_lossy()
                .into_owned();
            let subtitle = std::ffi::CStr::from_ptr(*subtitles.add(i))
                .to_string_lossy()
                .into_owned();
            let window_id = *window_ids.add(i);
            sources.push(SourceId::application(
                bundle,
                name,
                subtitle,
                window_id,
                0,
            ));
        }
    }

    Ok(sources)
}

fn fallback_enumerate() -> Vec<SourceId> {
    // App names only — no window titles without ScreenCaptureKit.
    let output = std::process::Command::new("osascript")
        .arg("-e")
        .arg("tell application \"System Events\" to get name of every application process whose background only is false")
        .output();

    match output {
        Ok(out) if out.status.success() => {
            let text = String::from_utf8_lossy(&out.stdout);
            text.split(", ")
                .filter(|s| !s.is_empty())
                .filter(|name| !name.eq_ignore_ascii_case("Input Mixer"))
                .map(|name| {
                    let trimmed = name.trim();
                    let bundle_id = resolve_bundle_id_for_app(trimmed);
                    SourceId::application(
                        bundle_id,
                        trimmed,
                        "",
                        0,
                        0,
                    )
                })
                .collect()
        }
        _ => Vec::new(),
    }
}

fn resolve_bundle_id_for_app(name: &str) -> String {
    let escaped = name.replace('\\', "\\\\").replace('"', "\\\"");
    let script = format!("id of app \"{escaped}\"");
    let output = std::process::Command::new("osascript")
        .arg("-e")
        .arg(script)
        .output();

    match output {
        Ok(out) if out.status.success() => {
            let id = String::from_utf8_lossy(&out.stdout).trim().to_string();
            if !id.is_empty() && !id.eq_ignore_ascii_case("missing value") {
                return id;
            }
        }
        _ => {}
    }

    name.to_lowercase().replace(' ', "")
}

pub fn create_capture(source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError> {
    if source.kind != SourceKind::Application {
        return Err(PlatformError::Other("Not an application source".into()));
    }

    if !has_screen_capture_access() {
        eprintln!(
            "[InputMixer] Screen Recording permission required to capture \"{}\"",
            source.display_name
        );
        return Ok(Box::new(UnavailableAppCapture::permission_denied()));
    }

    let ring = Arc::new(Mutex::new(Vec::<f32>::new()));
    let ring_cb = ring.clone();

    let bundle = std::ffi::CString::new(source.app_bundle_id.as_str())
        .map_err(|e| PlatformError::Other(e.to_string()))?;

    extern "C" fn on_audio(data: *const f32, len: usize, ctx: *mut std::ffi::c_void) {
        if data.is_null() || ctx.is_null() {
            return;
        }
        let ring = unsafe { &*(ctx as *const Mutex<Vec<f32>>) };
        let slice = unsafe { std::slice::from_raw_parts(data, len) };
        ring.lock().unwrap().extend_from_slice(slice);
    }

    let rc = unsafe {
        input_mixer_sck_start_capture(
            bundle.as_ptr(),
            source.window_id,
            on_audio,
            &*ring_cb as *const Mutex<Vec<f32>> as *mut std::ffi::c_void,
        )
    };

    if rc != 0 {
        eprintln!(
            "[InputMixer] Failed to start capture for \"{}\" (bundle={}, window_id={})",
            source.display_name, source.app_bundle_id, source.window_id
        );
        return Ok(Box::new(UnavailableAppCapture::capture_failed()));
    }

    Ok(Box::new(SckAppCapture {
        ring,
        available: true,
    }))
}

struct SckAppCapture {
    ring: Arc<Mutex<Vec<f32>>>,
    available: bool,
}

impl AppCapture for SckAppCapture {
    fn read_samples(&mut self, buffer: &mut [f32]) -> usize {
        let mut ring = self.ring.lock().unwrap();
        let len = buffer.len().min(ring.len());
        buffer[..len].copy_from_slice(&ring[..len]);
        ring.drain(..len);
        len
    }

    fn is_available(&self) -> bool {
        self.available
    }
}

impl Drop for SckAppCapture {
    fn drop(&mut self) {
        unsafe {
            input_mixer_sck_stop_capture();
        }
    }
}

/// Capture placeholder when permission is missing or SCK start fails.
struct UnavailableAppCapture {
    available: bool,
}

impl UnavailableAppCapture {
    fn permission_denied() -> Self {
        Self { available: false }
    }

    fn capture_failed() -> Self {
        Self { available: false }
    }
}

impl AppCapture for UnavailableAppCapture {
    fn read_samples(&mut self, buffer: &mut [f32]) -> usize {
        buffer.fill(0.0);
        0
    }

    fn is_available(&self) -> bool {
        self.available
    }
}
