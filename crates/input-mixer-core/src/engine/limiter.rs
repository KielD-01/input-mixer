/// Soft-knee limiter to prevent clipping on the master bus.
pub fn limit(samples: &mut [f32], threshold: f32) {
    for s in samples.iter_mut() {
        let abs = s.abs();
        if abs > threshold {
            let sign = s.signum();
            let excess = abs - threshold;
            *s = sign * (threshold + excess / (1.0 + excess));
        }
    }
}
