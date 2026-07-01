use super::{SourceDescriptor, SourceId, MAX_CHANNELS};
use std::collections::BTreeSet;

#[derive(Debug, Default)]
pub struct SourceRegistry {
    assigned: BTreeSet<SourceId>,
}

impl SourceRegistry {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn assign(&mut self, channels: &[Option<SourceId>]) {
        self.assigned.clear();
        for source in channels.iter().flatten() {
            self.assigned.insert(source.clone());
        }
    }

    pub fn is_used(&self, id: &SourceId) -> bool {
        self.assigned.contains(id)
    }

    pub fn mark_available<'a>(
        &self,
        sources: impl Iterator<Item = &'a SourceId>,
    ) -> Vec<SourceDescriptor> {
        sources
            .map(|id| SourceDescriptor {
                id: id.clone(),
                already_used: self.is_used(id),
                available: true,
            })
            .collect()
    }

    pub fn validate_unique(channels: &[Option<SourceId>]) -> Result<(), String> {
        let mut seen = BTreeSet::new();
        for (i, source) in channels.iter().enumerate().take(MAX_CHANNELS) {
            if let Some(id) = source {
                if !seen.insert(id.clone()) {
                    return Err(format!(
                        "Channel {} uses a source that is already assigned",
                        i + 1
                    ));
                }
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sources::SourceKind;

    #[test]
    fn rejects_duplicate_sources() {
        let id = SourceId {
            kind: SourceKind::Hardware,
            hardware_uid: "mic-1".into(),
            app_bundle_id: String::new(),
            window_id: 0,
            process_id: 0,
            display_name: "Mic".into(),
            subtitle: String::new(),
        };
        let channels = vec![Some(id.clone()), Some(id)];
        assert!(SourceRegistry::validate_unique(&channels).is_err());
    }
}
