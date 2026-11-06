#!/usr/bin/env python3
"""Wire take association to the active Creator script/section."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    original = text

    old = '''            val script = project.scripts.first()
            val section = script.sections.first()
            val take = JSONObject()
'''
    new = '''            val script = project.scripts.firstOrNull { candidate ->
                !selectedCreatorScriptId.isNullOrBlank() && candidate.scriptId == selectedCreatorScriptId
            } ?: project.scripts.firstOrNull()
                ?: return "Project link not saved · the Creator project has no scripts."
            val orderedSections = script.sections.sortedBy { it.order }
            val section = orderedSections.firstOrNull { candidate ->
                !selectedCreatorSectionId.isNullOrBlank() && candidate.sectionId == selectedCreatorSectionId
            } ?: orderedSections.firstOrNull()
                ?: return "Project link not saved · script \u201c${script.title}\u201d has no sections."
            val take = JSONObject()
'''
    if "orderedSections.firstOrNull" in text and "associateVerifiedTake" in text:
        # already patched if we see selectedCreatorSectionId near take JSON
        block = text[text.find("private fun associateVerifiedTake") : text.find("private fun associateVerifiedTake") + 1200]
        if "selectedCreatorSectionId" in block:
            print("Android section-linked takes already present")
            return

    if old not in text:
        raise SystemExit("Cannot find Android associateVerifiedTake first-section defaults")
    text = text.replace(old, new, 1)

    # Prefer selectedCreatorSectionId class field; promote if still local-only
    if "private var selectedCreatorSectionId" not in text:
        anchor = "    private var selectedCreatorScriptId: String? = null\n"
        if anchor in text:
            text = text.replace(
                anchor,
                anchor + "    private var selectedCreatorSectionId: String? = null\n",
                1,
            )
            text = text.replace(
                "        var selectedCreatorSectionId: String? = null\n",
                "        // selectedCreatorSectionId is a class field\n",
                1,
            )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Wrote Android section-linked takes")
    else:
        print("Android section-linked takes unchanged")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")
    original = text

    old = '''            var script = _creatorProject.Scripts[0];
            var section = script.Sections[0];
            _creatorProject.Takes ??= new List<CreatorProjectStore.TakeDocument>();
'''
    new = '''            var script = SelectedCreatorScript
                ?? _creatorProject.Scripts.FirstOrDefault()
                ?? throw new InvalidOperationException("The Creator project has no scripts.");
            var ordered = script.Sections.OrderBy(section => section.Order).ToList();
            if (ordered.Count == 0)
                return $"Project link not saved · script \u201c{script.Title}\u201d has no sections. The verified WAV is preserved.";
            var sectionIndex = CreatorSectionComboBox?.SelectedIndex ?? 0;
            if (sectionIndex < 0 || sectionIndex >= ordered.Count)
                sectionIndex = 0;
            // Prefer SelectedCreatorSectionId when reading-position/section-nav CI injected it.
            var preferredSectionId = (string?)null;
            try { preferredSectionId = SelectedCreatorSectionId; } catch { /* property may be absent */ }
            var section = ordered.FirstOrDefault(item =>
                    !string.IsNullOrWhiteSpace(preferredSectionId)
                    && string.Equals(item.SectionId, preferredSectionId, StringComparison.Ordinal))
                ?? ordered[sectionIndex];
            _creatorProject.Takes ??= new List<CreatorProjectStore.TakeDocument>();
'''

    block_start = text.find("private string AssociateVerifiedTake")
    if block_start < 0:
        raise SystemExit("Cannot find AssociateVerifiedTake")
    block = text[block_start : block_start + 1600]
    if "SelectedCreatorScript" in block and "ordered" in block:
        print("Windows section-linked takes already present")
        return

    if old not in text:
        raise SystemExit("Cannot find Windows AssociateVerifiedTake first-section defaults")
    text = text.replace(old, new, 1)

    # Ensure SelectedCreatorSectionId exists for preferred match
    if "private string? SelectedCreatorSectionId" not in text and "string? SelectedCreatorSectionId" not in text:
        prop = '''
    private string? SelectedCreatorSectionId
    {
        get
        {
            var script = SelectedCreatorScript;
            if (script?.Sections is null || script.Sections.Count == 0)
                return null;
            var ordered = script.Sections.OrderBy(section => section.Order).ToList();
            var index = CreatorSectionComboBox?.SelectedIndex ?? 0;
            if (index < 0 || index >= ordered.Count)
                index = 0;
            return ordered[index].SectionId;
        }
    }

'''
        text = text.replace(
            "    private string AssociateVerifiedTake(string takePath)",
            prop + "    private string AssociateVerifiedTake(string takePath)",
            1,
        )
        print("Injected SelectedCreatorSectionId for take association")

    if text != original:
        path.write_text(text, encoding="utf-8")
        print("Wrote Windows section-linked takes")
    else:
        print("Windows section-linked takes unchanged")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_section_linked_takes:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
