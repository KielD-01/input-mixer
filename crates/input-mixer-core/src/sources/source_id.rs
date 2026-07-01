use std::cmp::Ordering;
use std::fmt;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum SourceKind {
    Hardware,
    Application,
}

impl SourceKind {
    pub fn from_u8(v: u8) -> Self {
        match v {
            1 => Self::Application,
            _ => Self::Hardware,
        }
    }

    pub fn as_u8(self) -> u8 {
        match self {
            Self::Hardware => 0,
            Self::Application => 1,
        }
    }
}

/// Stable identity for deduplication across channels.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct SourceId {
    pub kind: SourceKind,
    pub hardware_uid: String,
    pub app_bundle_id: String,
    pub window_id: u64,
    pub process_id: u32,
    pub display_name: String,
    pub subtitle: String,
}

impl SourceId {
    pub fn hardware(uid: impl Into<String>, name: impl Into<String>) -> Self {
        let name = name.into();
        Self {
            kind: SourceKind::Hardware,
            hardware_uid: uid.into(),
            app_bundle_id: String::new(),
            window_id: 0,
            process_id: 0,
            display_name: name.clone(),
            subtitle: String::new(),
        }
    }

    pub fn application(
        bundle_id: impl Into<String>,
        name: impl Into<String>,
        subtitle: impl Into<String>,
        window_id: u64,
        process_id: u32,
    ) -> Self {
        Self {
            kind: SourceKind::Application,
            hardware_uid: String::new(),
            app_bundle_id: bundle_id.into(),
            window_id,
            process_id,
            display_name: name.into(),
            subtitle: subtitle.into(),
        }
    }
}

impl PartialOrd for SourceId {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for SourceId {
    fn cmp(&self, other: &Self) -> Ordering {
        (
            self.kind.as_u8(),
            self.hardware_uid.as_str(),
            self.app_bundle_id.as_str(),
            self.window_id,
            self.process_id,
        )
            .cmp(&(
                other.kind.as_u8(),
                other.hardware_uid.as_str(),
                other.app_bundle_id.as_str(),
                other.window_id,
                other.process_id,
            ))
    }
}

impl fmt::Display for SourceId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.subtitle.is_empty() {
            write!(f, "{}", self.display_name)
        } else {
            write!(f, "{} — {}", self.display_name, self.subtitle)
        }
    }
}

#[derive(Debug, Clone)]
pub struct SourceDescriptor {
    pub id: SourceId,
    pub already_used: bool,
    pub available: bool,
}

#[derive(Debug, Clone)]
pub struct OutputDeviceDescriptor {
    pub uid: String,
    pub name: String,
}

#[derive(Debug, Clone)]
pub struct ChannelConfig {
    pub source: Option<SourceId>,
    pub volume: f32,
    pub muted: bool,
}

impl Default for ChannelConfig {
    fn default() -> Self {
        Self {
            source: None,
            volume: 1.0,
            muted: false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct MasterConfig {
    pub volume: f32,
    pub muted: bool,
}

impl Default for MasterConfig {
    fn default() -> Self {
        Self {
            volume: 1.0,
            muted: false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct MonitorConfig {
    pub enabled: bool,
    pub output_device_uid: Option<String>,
    pub volume: f32,
}

impl Default for MonitorConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            output_device_uid: None,
            volume: 0.75,
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct MeterSnapshot {
    pub channel_input_peaks: Vec<f32>,
    pub channel_output_peaks: Vec<f32>,
    pub channel_unavailable: Vec<bool>,
    pub master_peak: f32,
    pub monitor_peak: f32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EngineStatus {
    Stopped,
    Ready,
    Running,
    DriverMissing,
    PermissionNeeded,
    Error,
}

impl EngineStatus {
    pub fn as_u8(self) -> u8 {
        match self {
            Self::Stopped => 0,
            Self::Ready => 1,
            Self::Running => 2,
            Self::DriverMissing => 3,
            Self::PermissionNeeded => 4,
            Self::Error => 5,
        }
    }

    pub fn from_u8(v: u8) -> Self {
        match v {
            1 => Self::Ready,
            2 => Self::Running,
            3 => Self::DriverMissing,
            4 => Self::PermissionNeeded,
            5 => Self::Error,
            _ => Self::Stopped,
        }
    }
}

pub const MAX_CHANNELS: usize = 6;
pub const SAMPLE_RATE: u32 = 48_000;
pub const CHANNELS: usize = 2;
