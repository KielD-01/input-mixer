use std::cell::UnsafeCell;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

/// Lock-free single-producer / single-consumer ring for monitor PCM.
///
/// Producer: engine thread. Consumer: CPAL output callback.
pub struct MonitorRingBuffer {
    buffer: Box<[UnsafeCell<f32>]>,
    capacity: usize,
    write_pos: AtomicUsize,
    read_pos: AtomicUsize,
}

impl MonitorRingBuffer {
    pub fn new(capacity: usize) -> Arc<Self> {
        let capacity = capacity.next_power_of_two().max(2);
        let buffer = (0..capacity)
            .map(|_| UnsafeCell::new(0.0_f32))
            .collect::<Vec<_>>()
            .into_boxed_slice();
        Arc::new(Self {
            buffer,
            capacity,
            write_pos: AtomicUsize::new(0),
            read_pos: AtomicUsize::new(0),
        })
    }

    pub fn free(&self) -> usize {
        let w = self.write_pos.load(Ordering::Relaxed);
        let r = self.read_pos.load(Ordering::Acquire);
        self.capacity - (w - r)
    }

    pub fn occupied(&self) -> usize {
        let w = self.write_pos.load(Ordering::Acquire);
        let r = self.read_pos.load(Ordering::Relaxed);
        w - r
    }

    /// Returns the number of samples written (may be less than `data.len()` when full).
    pub fn push_slice(&self, data: &[f32]) -> usize {
        let w = self.write_pos.load(Ordering::Relaxed);
        let r = self.read_pos.load(Ordering::Acquire);
        let available = self.capacity - (w - r);
        let to_write = data.len().min(available);
        let mask = self.capacity - 1;
        for (i, &sample) in data[..to_write].iter().enumerate() {
            // SAFETY: SPSC — only the producer writes these slots before advancing write_pos.
            unsafe {
                *self.buffer[(w + i) & mask].get() = sample;
            }
        }
        self.write_pos.store(w + to_write, Ordering::Release);
        to_write
    }

    /// Returns the number of samples read into `out`.
    pub fn pop_slice(&self, out: &mut [f32]) -> usize {
        let w = self.write_pos.load(Ordering::Acquire);
        let r = self.read_pos.load(Ordering::Relaxed);
        let available = w - r;
        let to_read = out.len().min(available);
        let mask = self.capacity - 1;
        for i in 0..to_read {
            // SAFETY: SPSC — only the consumer reads these slots before advancing read_pos.
            out[i] = unsafe { *self.buffer[(r + i) & mask].get() };
        }
        self.read_pos.store(r + to_read, Ordering::Release);
        to_read
    }
}

unsafe impl Send for MonitorRingBuffer {}
unsafe impl Sync for MonitorRingBuffer {}
