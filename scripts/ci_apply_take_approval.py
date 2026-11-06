#!/usr/bin/env python3
"""Wire take approval states into Creator Production on Android and Windows."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/MainActivity.kt"
    text = path.read_text(encoding="utf-8")
    if "refreshTakeApprovals" in text and "TAKE APPROVAL" in text:
        # heal older apply that used bare Space(this)
        if "takeApprovalList.addView(Space(this)" in text:
            text = text.replace(
                "takeApprovalList.addView(Space(this)",
                "takeApprovalList.addView(android.widget.Space(this)",
            )
            path.write_text(text, encoding="utf-8")
            print("Android take approval healed Space reference")
        else:
            print("Android take approval already present")
        return

    list_decl = "        val productionMarkerList = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }\n"
    if "val takeApprovalList" not in text:
        if list_decl not in text:
            raise SystemExit("Cannot find productionMarkerList declaration")
        text = text.replace(
            list_decl,
            list_decl + "        val takeApprovalList = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }\n",
            1,
        )

    helper = '''
        fun cycleTakeStatus(current: String?): String {
            val order = listOf("recorded", "review", "approved", "rejected")
            val idx = order.indexOf((current ?: "recorded").lowercase())
            return order[(maxOf(idx, 0) + 1) % order.size]
        }

        fun refreshTakeApprovals() {
            takeApprovalList.removeAllViews()
            val takes = activeCreatorProject?.takes.orEmpty()
            if (takes.isEmpty()) {
                takeApprovalList.addView(TextView(this).apply {
                    text = "No linked takes yet. Record a studio take to review it here."
                    setTextColor(Color.rgb(174, 185, 204))
                    setPadding(0, dp(8), 0, dp(8))
                })
                return
            }
            takes.forEachIndexed { index, take ->
                val takeId = take.optString("takeId")
                val status = take.optString("status", "recorded").ifBlank { "recorded" }
                val sectionId = take.optString("sectionId")
                val row = LinearLayout(this).apply {
                    orientation = LinearLayout.VERTICAL
                    setPadding(dp(10), dp(8), dp(10), dp(8))
                    background = android.graphics.drawable.GradientDrawable().apply {
                        setColor(Color.rgb(27, 34, 48))
                        cornerRadius = dp(8).toFloat()
                    }
                }
                row.addView(TextView(this).apply {
                    text = "Take ${index + 1} \u00b7 $status"
                    textSize = 13f
                    setTextColor(Color.WHITE)
                })
                if (sectionId.isNotBlank()) {
                    row.addView(TextView(this).apply {
                        text = "Section: ${sectionId.take(12)}"
                        textSize = 11f
                        setTextColor(Color.rgb(160, 190, 255))
                    })
                }
                row.addView(exchangeButton("Cycle status", "recorded \u2192 review \u2192 approved \u2192 rejected") {
                    try {
                        val project = activeCreatorProject ?: return@exchangeButton
                        val store = creatorProjectStore ?: return@exchangeButton
                        val next = cycleTakeStatus(status)
                        val updated = project.takes.map { item ->
                            if (item.optString("takeId") == takeId) {
                                org.json.JSONObject(item.toString()).put("status", next)
                            } else item
                        }
                        activeCreatorProject = store.save(project.copy(takes = updated))
                        creatorTakesSummaryText?.text = "Linked takes: ${activeCreatorProject?.takes?.size ?: 0}"
                        productionStatus.text = "Take marked $next."
                        refreshTakeApprovals()
                    } catch (error: Exception) {
                        productionStatus.text = "Could not update take: ${error.message}"
                    }
                })
                takeApprovalList.addView(row, matchWidth())
                takeApprovalList.addView(android.widget.Space(this).apply { minimumHeight = dp(6) })
            }
        }

'''
    if "fun cycleTakeStatus" not in text:
        anchor = "        fun refreshProductionMarkers()"
        if anchor not in text:
            raise SystemExit("Cannot find refreshProductionMarkers")
        text = text.replace(anchor, helper + anchor, 1)

    panel = '''
        creatorProductionPanel.addView(TextView(this).apply {
            text = "TAKE APPROVAL"
            textSize = 11f
            setTypeface(null, android.graphics.Typeface.BOLD)
            setTextColor(Color.rgb(160, 190, 255))
            setPadding(0, dp(16), 0, dp(4))
        })
        creatorProductionPanel.addView(TextView(this).apply {
            text = "Cycle each linked take: recorded \u2192 review \u2192 approved \u2192 rejected. Audio files are never changed."
            textSize = 12f
            setTextColor(Color.rgb(174, 185, 204))
            setPadding(0, 0, 0, dp(8))
        })
        creatorProductionPanel.addView(takeApprovalList, matchWidth())
        refreshTakeApprovals()
'''
    if "TAKE APPROVAL" not in text:
        mark = "        creatorProductionPanel.addView(productionMarkerList,matchWidth())"
        if mark not in text:
            mark = "        creatorProductionPanel.addView(productionMarkerList, matchWidth())"
        if mark not in text:
            raise SystemExit("Cannot find productionMarkerList panel add")
        text = text.replace(mark, mark + panel, 1)

    path.write_text(text, encoding="utf-8")
    print("Android take approval wired")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/MainWindow.xaml.cs"
    text = path.read_text(encoding="utf-8")

    # Heal: ensure non-empty marker path also appends take rows
    if "AppendTakeApprovalRows" in text:
        # After marker foreach, ensure Append is always invoked once at end of refresh
        bad_early = (
            "            CreatorProductionMarkerListPanel.Children.Add(new TextBlock { Text = \"No production markers yet.\" });\n"
            "            AppendTakeApprovalRows();\n"
            "            return;\n"
        )
        good_early = (
            "            CreatorProductionMarkerListPanel.Children.Add(new TextBlock { Text = \"No production markers yet.\" });\n"
        )
        if bad_early in text:
            text = text.replace(bad_early, good_early, 1)
            print("Removed early-return after empty markers so takes always append")

        # Ensure single Append at end of RefreshCreatorProductionMarkers
        start = text.find("private void RefreshCreatorProductionMarkers")
        end = text.find("private static string CycleTakeStatus", start)
        if end < 0:
            end = text.find("private void AppendTakeApprovalRows", start)
        if start > 0 and end > start:
            region = text[start:end]
            if region.count("AppendTakeApprovalRows();") == 0:
                old = region.rstrip()
                if old.endswith("}"):
                    new_region = old[:-1] + "\n        AppendTakeApprovalRows();\n    }\n"
                    text = text[:start] + new_region + text[end:]
                    print("Injected AppendTakeApprovalRows at end of marker refresh")
            elif region.count("AppendTakeApprovalRows();") > 1:
                # leave first only at end
                print("Append already present in refresh")

        path.write_text(text, encoding="utf-8")
        print("Windows take approval healed/present")
        return

    methods = r'''
    private static string CycleTakeStatus(string? current)
    {
        var order = new[] { "recorded", "review", "approved", "rejected" };
        var normalized = string.IsNullOrWhiteSpace(current) ? "recorded" : current.Trim().ToLowerInvariant();
        var idx = Array.IndexOf(order, normalized);
        if (idx < 0) idx = 0;
        return order[(idx + 1) % order.Length];
    }

    private void AppendTakeApprovalRows()
    {
        if (CreatorProductionMarkerListPanel is null) return;
        CreatorProductionMarkerListPanel.Children.Add(new TextBlock
        {
            Text = "Take approval",
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            Margin = new Thickness(0, 16, 0, 4),
        });
        CreatorProductionMarkerListPanel.Children.Add(new TextBlock
        {
            Text = "Cycle recorded \u2192 review \u2192 approved \u2192 rejected. Audio files are never changed.",
            Opacity = 0.8,
            Margin = new Thickness(0, 0, 0, 8),
        });

        var takes = _creatorProject?.Takes ?? new List<CreatorProjectStore.TakeDocument>();
        if (takes.Count == 0)
        {
            CreatorProductionMarkerListPanel.Children.Add(new TextBlock
            {
                Text = "No linked takes yet. Record a studio take to review it here.",
                Opacity = 0.75,
            });
            return;
        }

        for (var index = 0; index < takes.Count; index++)
        {
            var take = takes[index];
            var status = string.IsNullOrWhiteSpace(take.Status) ? "recorded" : take.Status!;
            var row = new StackPanel { Spacing = 4, Margin = new Thickness(0, 0, 0, 8) };
            row.Children.Add(new TextBlock { Text = $"Take {index + 1} \u00b7 {status}" });
            if (!string.IsNullOrWhiteSpace(take.SectionId))
                row.Children.Add(new TextBlock { Text = $"Section: {take.SectionId[..Math.Min(12, take.SectionId.Length)]}", Opacity = 0.75, FontSize = 12 });

            var button = new Button { Content = "Cycle status", Tag = take.TakeId };
            button.Click += (_, _) =>
            {
                if (_creatorProject is null || _creatorStore is null) return;
                var target = _creatorProject.Takes.FirstOrDefault(item => item.TakeId == take.TakeId);
                if (target is null) return;
                try
                {
                    target.Status = CycleTakeStatus(target.Status);
                    _creatorStore.Save(_creatorProject);
                    if (CreatorTakesSummaryText is not null)
                        CreatorTakesSummaryText.Text = $"Linked takes: {_creatorProject.Takes.Count}";
                    if (CreatorProductionStatusText is not null)
                        CreatorProductionStatusText.Text = $"Take marked {target.Status}.";
                    RefreshCreatorProductionMarkers();
                }
                catch (Exception error)
                {
                    if (CreatorProductionStatusText is not null)
                        CreatorProductionStatusText.Text = "Could not update take: " + error.Message;
                }
            };
            row.Children.Add(button);
            CreatorProductionMarkerListPanel.Children.Add(row);
        }
    }

'''

    if "private static string CycleTakeStatus" not in text:
        anchor = "    private void CreatorProductionAdd_Click"
        if anchor not in text:
            raise SystemExit("Cannot find CreatorProductionAdd_Click")
        text = text.replace(anchor, methods + "\n    " + anchor.lstrip(), 1)

    # Empty markers: do not return early without takes section
    text = text.replace(
        """            CreatorProductionMarkerListPanel.Children.Add(new TextBlock { Text = "No production markers yet." });
            return;
""",
        """            CreatorProductionMarkerListPanel.Children.Add(new TextBlock { Text = "No production markers yet." });
""",
        1,
    )

    start = text.find("private void RefreshCreatorProductionMarkers")
    end = text.find("private static string CycleTakeStatus", start)
    if end < 0:
        end = text.find("private void CreatorProductionAdd_Click", start)
    if start > 0 and end > start:
        region = text[start:end]
        if "AppendTakeApprovalRows();" not in region:
            old = region.rstrip()
            if old.endswith("}"):
                new_region = old[:-1] + "\n        AppendTakeApprovalRows();\n    }\n"
                text = text[:start] + new_region + text[end:]
                print("Injected AppendTakeApprovalRows at end of marker refresh")

    path.write_text(text, encoding="utf-8")
    print("Windows take approval wired")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_take_approval:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
