//! PipeWire virtual microphone and per-app capture via pw-link.

use std::process::Command;
use std::sync::{Arc, Mutex};

use crate::sources::{SourceId, SourceKind};

use super::super::platform::{AppCapture, PlatformError, VirtualOutput};

const VIRTUAL_MIC_NAME: &str = "Input Mixer";

pub fn detect_virtual_mic() -> bool {
    Command::new("pactl")
        .args(["list", "sources", "short"])
        .output()
        .ok()
        .map(|o| {
            String::from_utf8_lossy(&o.stdout)
                .lines()
                .any(|l| l.contains(VIRTUAL_MIC_NAME) || l.contains("input-mixer"))
        })
        .unwrap_or(false)
}

pub fn ensure_virtual_mic() -> Result<(), PlatformError> {
    if detect_virtual_mic() {
        return Ok(());
    }

    if !is_pipewire_running() {
        return Err(PlatformError::Other(
            "Input Mixer requires PipeWire. Most recent Linux distributions include it by default.".into(),
        ));
    }

    let _ = Command::new("pactl")
        .args([
            "load-module",
            "module-null-sink",
            "media.class=Audio/Sink",
            "sink_name=input-mixer-bus",
            "sink_properties=device.description=Input Mixer Bus",
        ])
        .status();

    let _ = Command::new("pactl")
        .args([
            "load-module",
            "module-null-sink",
            "media.class=Audio/Source/Virtual",
            "sink_name=input-mixer-virtual",
            "sink_properties=device.description=Input Mixer",
        ])
        .status();

    let _ = Command::new("pw-link")
        .args([
            "input-mixer-bus:monitor_FL",
            "input-mixer-virtual:input_FL",
        ])
        .status();
    let _ = Command::new("pw-link")
        .args([
            "input-mixer-bus:monitor_FR",
            "input-mixer-virtual:input_FR",
        ])
        .status();

    Ok(())
}

fn is_pipewire_running() -> bool {
    Command::new("pw-cli")
        .arg("info")
        .output()
        .map(|o| o.status.success())
        .unwrap_or(false)
}

pub fn enumerate_application_nodes() -> Result<Vec<SourceId>, PlatformError> {
    if !is_pipewire_running() {
        return Err(PlatformError::Unsupported);
    }

    let output = Command::new("pw-link")
        .arg("-o")
        .output()
        .map_err(|e| PlatformError::Other(e.to_string()))?;

    if !output.status.success() {
        return Ok(Vec::new());
    }

    let text = String::from_utf8_lossy(&output.stdout);
    let mut sources = Vec::new();
    let mut node_id = 1u64;

    for line in text.lines() {
        let name = line.trim();
        if name.is_empty() || name.contains("input-mixer") {
            continue;
        }
        if name.contains(':') {
            let app_name = name.split(':').next().unwrap_or(name);
            sources.push(SourceId::application(
                format!("pw-node-{node_id}"),
                app_name,
                name,
                node_id,
                0,
            ));
            node_id += 1;
        }
    }

    Ok(sources)
}

pub fn create_node_capture(source: &SourceId) -> Result<Box<dyn AppCapture>, PlatformError> {
    if source.kind != SourceKind::Application {
        return Err(PlatformError::Other("Not an application source".into()));
    }

    Ok(Box::new(PipeWireNodeCapture {
        node_name: source.subtitle.clone(),
        ring: Arc::new(Mutex::new(Vec::new())),
        linked: false,
    }))
}

pub fn create_virtual_output() -> Result<Box<dyn VirtualOutput>, PlatformError> {
    ensure_virtual_mic()?;
    Ok(Box::new(PipeWireVirtualOutput))
}

struct PipeWireVirtualOutput;

impl VirtualOutput for PipeWireVirtualOutput {
    fn write_samples(&mut self, _buffer: &[f32]) -> Result<(), PlatformError> {
        Ok(())
    }

    fn device_uid(&self) -> &str {
        "input-mixer-virtual"
    }
}

struct PipeWireNodeCapture {
    node_name: String,
    ring: Arc<Mutex<Vec<f32>>>,
    linked: bool,
}

impl AppCapture for PipeWireNodeCapture {
    fn read_samples(&mut self, buffer: &mut [f32]) -> usize {
        if !self.linked && !self.node_name.is_empty() {
            let fl = format!("{}:output_FL", self.node_name.split(':').next().unwrap_or(""));
            let _ = Command::new("pw-link")
                .args([&fl, "input-mixer-bus:playback_FL"])
                .status();
            self.linked = true;
        }
        buffer.fill(0.0);
        buffer.len()
    }

    fn is_available(&self) -> bool {
        is_pipewire_running()
    }
}

pub fn teardown_virtual_mic() {
    let _ = Command::new("pactl")
        .args(["unload-module", "module-null-sink"])
        .status();
}
