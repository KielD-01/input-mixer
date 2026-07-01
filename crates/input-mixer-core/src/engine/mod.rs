mod limiter;
mod mixer_engine;

pub use mixer_engine::MixerEngine;

pub fn peak_level(samples: &[f32]) -> f32 {
    samples
        .iter()
        .map(|s| s.abs())
        .fold(0.0_f32, |a, b| a.max(b))
}

pub fn apply_volume(samples: &mut [f32], volume: f32) {
    if (volume - 1.0).abs() > f32::EPSILON {
        for s in samples.iter_mut() {
            *s *= volume;
        }
    }
}

pub fn mix_add(target: &mut [f32], source: &[f32]) {
    let len = target.len().min(source.len());
    for i in 0..len {
        target[i] += source[i];
    }
}
