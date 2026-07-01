fn main() {
    #[cfg(target_os = "macos")]
    {
        cc::Build::new()
            .file("src/capture/macos/sck_shim.mm")
            .file("src/capture/macos/aggregate_device.mm")
            .file("src/capture/macos/hal_devices.mm")
            .flag("-fobjc-arc")
            .compile("input_mixer_sck");

        println!("cargo:rustc-link-lib=framework=Foundation");
        println!("cargo:rustc-link-lib=framework=CoreFoundation");
        println!("cargo:rustc-link-lib=framework=CoreMedia");
        println!("cargo:rustc-link-lib=framework=CoreAudio");
        println!("cargo:rustc-link-lib=framework=ScreenCaptureKit");
        println!("cargo:rustc-link-lib=framework=AVFoundation");
        println!("cargo:rustc-link-lib=framework=CoreGraphics");
        println!("cargo:rustc-link-lib=framework=Security");
    }

    let mut build = cxx_build::bridge("src/ffi.rs");
    build.std("c++17");

    #[cfg(target_os = "windows")]
    {
        println!("cargo:rustc-link-lib=ole32");
        println!("cargo:rustc-link-lib=uuid");
    }

    build.compile("input-mixer-core-ffi");

    println!("cargo:rerun-if-changed=src/ffi.rs");
    println!("cargo:rerun-if-changed=src/capture/macos/sck_shim.mm");
    println!("cargo:rerun-if-changed=src/capture/macos/aggregate_device.mm");
    println!("cargo:rerun-if-changed=src/capture/macos/hal_devices.mm");
}
