#pragma once

#include <optional>
#include <vector>

#include "RustBridge.h"

namespace input_mixer
{

/** True when assigned and candidate refer to the same capture source (exact or legacy/fuzzy). */
bool sourcesEquivalent(const CxxSourceDescriptor& assigned, const CxxSourceDescriptor& candidate);

/** Find the best current scan match for a persisted or legacy source descriptor. */
std::optional<CxxSourceDescriptor> findMatchingSourceDescriptor(
    const CxxSourceDescriptor& assigned,
    const std::vector<SourceItem>& hardware,
    const std::vector<SourceItem>& applications);

/** Resolve assigned sources to current scan IDs where possible. */
void resolveChannelSources(std::vector<ChannelState>& channels,
                           const std::vector<SourceItem>& hardware,
                           const std::vector<SourceItem>& applications);

} // namespace input_mixer
