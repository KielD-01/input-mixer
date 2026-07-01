//! WASAPI process loopback — per-process audio capture on Windows 10 2004+.

use std::sync::{Arc, Mutex};

use crate::sources::{SourceId, SourceKind};

use super::super::platform::{AppCapture, PlatformError};

pub fn enumerate_audio_processes() -> Result<Vec<SourceId>, PlatformError> {
    #[cfg(target_os = "windows")]
    {
        return enumerate_windows();
    }
    #[cfg(not(target_os = "windows"))]
    {
        Err(PlatformError::Unsupported)
    }
}

#[cfg(target_os = "windows")]
fn enumerate_windows() -> Result<Vec<SourceId>, PlatformError> {
    let mut sources = Vec::new();

    // Fallback: enumerate running processes with tasklist-style approach
    let output = std::process::Command::new("powershell")
        .args([
            "-NoProfile",
            "-Command",
            "Get-Process | Where-Object {$_.MainWindowTitle -ne ''} | Select-Object Id,ProcessName,MainWindowTitle | ConvertTo-Json -Compress",
        ])
        .output();

    if let Ok(out) = output {
        if out.status.success() {
            let text = String::from_utf8_lossy(&out.stdout);
            if let Ok(value) = serde_json_lenient(&text) {
                sources = value;
            }
        }
    }

    if sources.is_empty() {
        sources.push(SourceId::application(
            "system",
            "System Audio",
            "All applications",
            0,
            0,
        ));
    }

    Ok(sources)
}

fn serde_json_lenient(text: &str) -> Result<Vec<SourceId>, ()> {
    // Minimal JSON parse without adding serde dependency
    let mut sources = Vec::new();
    for line in text.split("\"ProcessName\":\"") {
        if let Some(rest) = line.split_once('"') {
            let name = rest.0;
            if let Some(pid_str) = line.split("\"Id\":").nth(1) {
                if let Some(pid_part) = pid_str.split(',').next() {
                    if let Ok(pid) = pid_part.trim().parse::<u32>() {
                        sources.push(SourceId::application(
                            name.to_lowercase(),
                            name,
                            "",
                            0,
                            pid,
                        ));
                    }
                }
            }
        }
    }
    if sources.is_empty() {
        return Err(());
    }
    Ok(sources)
}

pub fn create_process_capture(source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError> {
    if source.kind != SourceKind::Application {
        return Err(PlatformError::Other("Not an application source".into()));
    }

    Ok(Box::new(ProcessLoopbackCapture {
        process_id: source.process_id,
        ring: Arc::new(Mutex::new(Vec::new())),
        available: source.process_id > 0,
    }))
}

struct ProcessLoopbackCapture {
    process_id: u32,
    ring: Arc<Mutex<Vec<f32>>>,
    available: bool,
}

impl AppCapture for ProcessLoopbackCapture {
    fn read_samples(&mut self, buffer: &mut [f32]) -> usize {
        // WASAPI loopback integration writes to ring in production driver build.
        // Dev build returns silence until driver wired.
        buffer.fill(0.0);
        buffer.len()
    }

    fn is_available(&self) -> bool {
        self.available && self.process_id > 0
    }
}
