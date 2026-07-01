#include "SourceMatching.h"

#include "CxxString.h"

namespace input_mixer
{
namespace
{

juce::String normalizedName(juce::String value)
{
    return value.trim().toLowerCase();
}

juce::String bundleSlug(juce::String value)
{
    return value.toLowerCase().removeCharacters("._- ");
}

bool windowIdsCompatible(uint64_t assignedWindowId, uint64_t candidateWindowId)
{
    return assignedWindowId == 0 || candidateWindowId == 0 || assignedWindowId == candidateWindowId;
}

bool hardwareSourcesEquivalent(const CxxSourceDescriptor& assigned,
                               const CxxSourceDescriptor& candidate)
{
    if (! assigned.hardware_uid.empty() && assigned.hardware_uid == candidate.hardware_uid)
        return true;

    const auto assignedName = normalizedName(toJuceString(assigned.display_name));
    const auto candidateName = normalizedName(toJuceString(candidate.display_name));

    if (! assignedName.isEmpty() && assignedName == candidateName)
        return true;

    const auto assignedUid = toJuceString(assigned.hardware_uid);
    const auto candidateUid = toJuceString(candidate.hardware_uid);

    if (! assignedUid.isEmpty() && assignedUid == candidateUid)
        return true;

    if (! assignedUid.isEmpty() && ! candidateName.isEmpty() && assignedUid.endsWithIgnoreCase(candidateName))
        return true;

    if (! candidateUid.isEmpty() && ! assignedName.isEmpty() && candidateUid.endsWithIgnoreCase(assignedName))
        return true;

    return false;
}

bool applicationSourcesEquivalent(const CxxSourceDescriptor& assigned,
                                  const CxxSourceDescriptor& candidate)
{
    if (! windowIdsCompatible(assigned.window_id, candidate.window_id))
        return false;

    if (! assigned.app_bundle_id.empty() && assigned.app_bundle_id == candidate.app_bundle_id)
        return true;

    const auto assignedName = normalizedName(toJuceString(assigned.display_name));
    const auto candidateName = normalizedName(toJuceString(candidate.display_name));

    if (! assignedName.isEmpty() && assignedName == candidateName)
        return true;

    const auto assignedSlug = bundleSlug(toJuceString(assigned.app_bundle_id));
    const auto candidateSlug = bundleSlug(toJuceString(candidate.app_bundle_id));

    if (! assignedSlug.isEmpty() && ! candidateSlug.isEmpty())
    {
        if (assignedSlug == candidateSlug || candidateSlug.contains(assignedSlug)
            || assignedSlug.contains(candidateSlug))
            return true;
    }

    const auto nameSlug = bundleSlug(toJuceString(assigned.display_name));

    if (! nameSlug.isEmpty() && ! candidateSlug.isEmpty()
        && (candidateSlug.contains(nameSlug) || nameSlug.contains(candidateSlug)))
        return true;

    return false;
}

} // namespace

bool sourcesEquivalent(const CxxSourceDescriptor& assigned, const CxxSourceDescriptor& candidate)
{
    if (assigned.kind != candidate.kind)
        return false;

    if (assigned.kind == 0)
        return hardwareSourcesEquivalent(assigned, candidate);

    if (assigned.kind == 1)
        return applicationSourcesEquivalent(assigned, candidate);

    return false;
}

std::optional<CxxSourceDescriptor> findMatchingSourceDescriptor(
    const CxxSourceDescriptor& assigned,
    const std::vector<SourceItem>& hardware,
    const std::vector<SourceItem>& applications)
{
    for (const auto& item : hardware)
    {
        if (sourcesEquivalent(assigned, item.descriptor))
            return item.descriptor;
    }

    for (const auto& item : applications)
    {
        if (sourcesEquivalent(assigned, item.descriptor))
            return item.descriptor;
    }

    return std::nullopt;
}

void resolveChannelSources(std::vector<ChannelState>& channels,
                           const std::vector<SourceItem>& hardware,
                           const std::vector<SourceItem>& applications)
{
    for (auto& channel : channels)
    {
        if (! channel.hasSource)
            continue;

        if (const auto resolved = findMatchingSourceDescriptor(channel.source, hardware, applications))
            channel.source = *resolved;
    }
}

} // namespace input_mixer
