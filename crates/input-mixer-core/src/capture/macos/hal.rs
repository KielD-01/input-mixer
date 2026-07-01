//! CoreAudio HAL device listing without opening input AudioUnits (avoids mic TCC prompts).

use crate::sources::{OutputDeviceDescriptor, SourceId};

extern "C" {
    fn input_mixer_hal_enumerate_input_devices(
        names: *mut *mut *mut std::ffi::c_char,
        uids: *mut *mut *mut std::ffi::c_char,
        count: *mut usize,
    ) -> i32;

    fn input_mixer_hal_enumerate_output_devices(
        names: *mut *mut *mut std::ffi::c_char,
        uids: *mut *mut *mut std::ffi::c_char,
        count: *mut usize,
    ) -> i32;

    fn input_mixer_hal_free_device_list(arr: *mut *mut std::ffi::c_char, count: usize);

    fn input_mixer_hal_input_name_for_uid(uid: *const std::ffi::c_char, out_name: *mut *mut std::ffi::c_char)
        -> i32;

    fn input_mixer_hal_free_string(s: *mut std::ffi::c_char);
}

struct DeviceList {
    names: *mut *mut std::ffi::c_char,
    uids: *mut *mut std::ffi::c_char,
    count: usize,
}

impl DeviceList {
    fn enumerate(enumerate_fn: unsafe extern "C" fn(*mut *mut *mut std::ffi::c_char, *mut *mut *mut std::ffi::c_char, *mut usize) -> i32) -> Option<Self> {
        let mut names: *mut *mut std::ffi::c_char = std::ptr::null_mut();
        let mut uids: *mut *mut std::ffi::c_char = std::ptr::null_mut();
        let mut count: usize = 0;

        let rc = unsafe { enumerate_fn(&mut names, &mut uids, &mut count) };
        if rc != 0 {
            return None;
        }

        Some(Self { names, uids, count })
    }
}

impl Drop for DeviceList {
    fn drop(&mut self) {
        unsafe {
            if !self.names.is_null() {
                input_mixer_hal_free_device_list(self.names, self.count);
            }
            if !self.uids.is_null() {
                input_mixer_hal_free_device_list(self.uids, self.count);
            }
        }
    }
}

pub fn enumerate_input_devices() -> Vec<SourceId> {
    let Some(list) = DeviceList::enumerate(input_mixer_hal_enumerate_input_devices) else {
        return Vec::new();
    };

    let mut sources = Vec::with_capacity(list.count);
    for i in 0..list.count {
        unsafe {
            let name = std::ffi::CStr::from_ptr(*list.names.add(i))
                .to_string_lossy()
                .into_owned();
            if name.is_empty() {
                continue;
            }
            let hal_uid = std::ffi::CStr::from_ptr(*list.uids.add(i))
                .to_string_lossy()
                .into_owned();
            let uid = format!("hw-in:{}", hal_uid);
            sources.push(SourceId::hardware(uid, name));
        }
    }
    sources
}

pub fn enumerate_output_devices() -> Vec<OutputDeviceDescriptor> {
    let Some(list) = DeviceList::enumerate(input_mixer_hal_enumerate_output_devices) else {
        return Vec::new();
    };

    let mut outputs = Vec::with_capacity(list.count);
    for i in 0..list.count {
        unsafe {
            let name = std::ffi::CStr::from_ptr(*list.names.add(i))
                .to_string_lossy()
                .into_owned();
            if name.is_empty() {
                continue;
            }
            outputs.push(OutputDeviceDescriptor {
                uid: format!("hw-out:{}", name),
                name,
            });
        }
    }
    outputs
}

pub fn input_device_names() -> Vec<String> {
    enumerate_input_devices()
        .into_iter()
        .map(|s| s.display_name)
        .collect()
}

pub fn input_name_for_uid(uid: &str) -> Option<String> {
    let c_uid = std::ffi::CString::new(uid).ok()?;
    let mut out: *mut std::ffi::c_char = std::ptr::null_mut();
    let rc = unsafe { input_mixer_hal_input_name_for_uid(c_uid.as_ptr(), &mut out) };
    if rc != 0 || out.is_null() {
        return None;
    }
    let name = unsafe {
        let s = std::ffi::CStr::from_ptr(out).to_string_lossy().into_owned();
        input_mixer_hal_free_string(out);
        s
    };
    if name.is_empty() {
        None
    } else {
        Some(name)
    }
}
