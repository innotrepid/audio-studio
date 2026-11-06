#!/usr/bin/env python3
"""Add phraseReplacementRequests to Creator project stores (Phase 5 step 1)."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def ensure_android() -> None:
    path = ROOT / "platforms/android/app/src/main/java/com/innotrepid/audiostudio/CreatorProjectStore.kt"
    text = path.read_text(encoding="utf-8")
    if "PhraseReplacementRequest" in text and "phraseReplacementRequests" in text:
        print("Android phrase replacement store already present")
        return

    # data class before Project
    if "data class PhraseReplacementRequest" not in text:
        text = text.replace(
            "    data class Project(",
            """    data class PhraseReplacementRequest(
        val requestId: String,
        val phraseText: String,
        val charStart: Int? = null,
        val charEnd: Int? = null,
        val sourceTakeId: String? = null,
        /** unverified | manual | rejected — no audio range until verified */
        val alignmentStatus: String = "unverified",
        val rangeStartMs: Long? = null,
        val rangeEndMs: Long? = null,
        val replacementTakeId: String? = null,
        val assemblyStatus: String = "none",
        val createdUtc: String,
        val updatedUtc: String
    )

    data class Project(""",
            1,
        )

    # Project field — after productionMarkers line
    if "phraseReplacementRequests" not in text:
        # handle optional trailing fields from other applies
        if "val productionMarkers: List<ProductionMarker> = emptyList()\n    )" in text:
            text = text.replace(
                "val productionMarkers: List<ProductionMarker> = emptyList()\n    )",
                "val productionMarkers: List<ProductionMarker> = emptyList(),\n"
                "        val phraseReplacementRequests: List<PhraseReplacementRequest> = emptyList()\n    )",
                1,
            )
        elif "val productionMarkers: List<ProductionMarker> = emptyList()," in text:
            # already has more fields after markers
            text = text.replace(
                "val productionMarkers: List<ProductionMarker> = emptyList(),",
                "val productionMarkers: List<ProductionMarker> = emptyList(),\n"
                "        val phraseReplacementRequests: List<PhraseReplacementRequest> = emptyList(),",
                1,
            )
        else:
            raise SystemExit("Cannot find productionMarkers field on Android Project")

    # encode — after productionMarkers put
    if '"phraseReplacementRequests"' not in text:
        marker = '.put("productionMarkers", JSONArray().apply { project.productionMarkers.forEach'
        idx = text.find(marker)
        if idx < 0:
            raise SystemExit("Cannot find productionMarkers encode")
        # find end of that .put chain line(s) - next line after the productionMarkers put closes
        # Insert new put after the productionMarkers put expression ends
        # Search for closing of productionMarkers forEach
        end = text.find(")\n", idx)
        # better: find ".put(\"productionMarkers\"" block end by matching forEach close
        chunk = text[idx:]
        # find first ")\n            .put" or ")\n" after productionMarkers - the put ends with })\n or similar
        close = chunk.find("})\n")
        if close < 0:
            close = chunk.find("})\r\n")
        if close < 0:
            raise SystemExit("Cannot end productionMarkers put")
        insert_at = idx + close + len("})\n")
        put_block = (
            '            .put("phraseReplacementRequests", JSONArray().apply {
'
            '                project.phraseReplacementRequests.forEach { req ->
'
            '                    put(JSONObject()
'
            '                        .put("requestId", req.requestId)
'
            '                        .put("phraseText", req.phraseText)
'
            '                        .put("charStart", req.charStart ?: JSONObject.NULL)
'
            '                        .put("charEnd", req.charEnd ?: JSONObject.NULL)
'
            '                        .put("sourceTakeId", req.sourceTakeId ?: JSONObject.NULL)
'
            '                        .put("alignmentStatus", req.alignmentStatus)
'
            '                        .put("rangeStartMs", req.rangeStartMs ?: JSONObject.NULL)
'
            '                        .put("rangeEndMs", req.rangeEndMs ?: JSONObject.NULL)
'
            '                        .put("replacementTakeId", req.replacementTakeId ?: JSONObject.NULL)
'
            '                        .put("assemblyStatus", req.assemblyStatus)
'
            '                        .put("createdUtc", req.createdUtc)
'
            '                        .put("updatedUtc", req.updatedUtc)
'
            '                    )
'
            '                }
'
            '            })\n'
        )
        text = text[:insert_at] + put_block + text[insert_at:]

    # decode before return Project(
    if "phraseReplacementRequests =" not in text:
        text = text.replace(
            "        return Project(\n",
            """        val phraseJson = root.optJSONArray("phraseReplacementRequests") ?: JSONArray()
        val phraseRequests = (0 until phraseJson.length()).map { index ->
            val item = phraseJson.getJSONObject(index)
            PhraseReplacementRequest(
                requestId = item.getString("requestId"),
                phraseText = item.getString("phraseText"),
                charStart = if (item.isNull("charStart")) null else item.optInt("charStart"),
                charEnd = if (item.isNull("charEnd")) null else item.optInt("charEnd"),
                sourceTakeId = item.optString("sourceTakeId").takeIf { it.isNotBlank() && it != "null" },
                alignmentStatus = item.optString("alignmentStatus", "unverified").ifBlank { "unverified" },
                rangeStartMs = if (item.isNull("rangeStartMs")) null else item.optLong("rangeStartMs"),
                rangeEndMs = if (item.isNull("rangeEndMs")) null else item.optLong("rangeEndMs"),
                replacementTakeId = item.optString("replacementTakeId").takeIf { it.isNotBlank() && it != "null" },
                assemblyStatus = item.optString("assemblyStatus", "none").ifBlank { "none" },
                createdUtc = item.optString("createdUtc", Instant.now().toString()),
                updatedUtc = item.optString("updatedUtc", Instant.now().toString())
            )
        }
        return Project(
""",
            1,
        )
        # add to Project constructor call
        if "phraseReplacementRequests = phraseRequests" not in text:
            text = text.replace(
                "            productionMarkers = markers\n        ).also(::validate)",
                "            productionMarkers = markers,\n"
                "            phraseReplacementRequests = phraseRequests\n"
                "        ).also(::validate)",
                1,
            )

    # validate
    if "phraseReplacementRequests" in text and "Phrase replacement requestId" not in text:
        # append validation near end of validate function - after production markers loop if any
        anchor = "        require(project.transcript.length <= maxSectionTextLength)"
        if anchor not in text:
            anchor = "        project.captions.forEach"
        insert = (
            "        val allowedAlign = setOf(\"unverified\", \"manual\", \"rejected\")\n"
            "        val allowedAssembly = setOf(\"none\", \"preview\", \"approved\", \"discarded\")\n"
            "        project.phraseReplacementRequests.forEach { req ->\n"
            "            require(req.requestId.isNotBlank()) { \"Phrase replacement requestId is required.\" }\n"
            "            require(req.phraseText.isNotBlank()) { \"Phrase text must not be blank.\" }\n"
            "            require(req.phraseText.length <= maxSectionTextLength) { \"Phrase text exceeds limit.\" }\n"
            "            require(req.alignmentStatus in allowedAlign) { \"Invalid alignmentStatus.\" }\n"
            "            require(req.assemblyStatus in allowedAssembly) { \"Invalid assemblyStatus.\" }\n"
            "            if (req.charStart != null && req.charEnd != null) {\n"
            "                require(req.charStart >= 0 && req.charEnd >= req.charStart) { \"Invalid phrase character range.\" }\n"
            "            }\n"
            "            req.sourceTakeId?.let { tid ->\n"
            "                require(project.takes.any { it.optString(\"takeId\") == tid }) {\n"
            "                    \"Phrase sourceTakeId must reference a project take.\"\n"
            "                }\n"
            "            }\n"
            "        }\n"
        )
        if anchor in text:
            text = text.replace(anchor, insert + "\n        " + anchor.lstrip(), 1)

    path.write_text(text, encoding="utf-8")
    print("Android phrase replacement store applied")


def ensure_windows() -> None:
    path = ROOT / "platforms/windows/AudioStudio/CreatorProjectStore.cs"
    text = path.read_text(encoding="utf-8")
    if "PhraseReplacementRequestDocument" in text:
        print("Windows phrase replacement store already present")
        return

    # Insert class after ProductionMarkerDocument or before ProjectDocument end
    doc = '''
    internal sealed class PhraseReplacementRequestDocument
    {
        public string RequestId { get; set; } = Guid.NewGuid().ToString();
        public string PhraseText { get; set; } = string.Empty;
        public int? CharStart { get; set; }
        public int? CharEnd { get; set; }
        public string? SourceTakeId { get; set; }
        /// <summary>unverified | manual | rejected</summary>
        public string AlignmentStatus { get; set; } = "unverified";
        public long? RangeStartMs { get; set; }
        public long? RangeEndMs { get; set; }
        public string? ReplacementTakeId { get; set; }
        /// <summary>none | preview | approved | discarded</summary>
        public string AssemblyStatus { get; set; } = "none";
        public string CreatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public string UpdatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
    }
'''
    if "class PhraseReplacementRequestDocument" not in text:
        # after ProductionMarkerDocument class closing - find ProductionMarkerDocument
        anchor = "    internal sealed class ProductionMarkerDocument"
        if anchor not in text:
            anchor = "    internal sealed class CaptionDocument"
        # insert before ProjectDocument
        text = text.replace(
            "    internal sealed class ProjectDocument",
            doc + "\n    internal sealed class ProjectDocument",
            1,
        )

    if "PhraseReplacementRequests" not in text:
        text = text.replace(
            "        public List<ProductionMarkerDocument> ProductionMarkers { get; set; } = new();\n",
            "        public List<ProductionMarkerDocument> ProductionMarkers { get; set; } = new();\n"
            "        public List<PhraseReplacementRequestDocument> PhraseReplacementRequests { get; set; } = new();\n",
            1,
        )

    # Validate in Validate method
    if "Phrase replacement requestId" not in text and "PhraseReplacementRequests" in text:
        # find a good place - after captions validation
        needle = "        foreach (var caption in project.Captions"
        if needle not in text:
            needle = "        foreach (var marker in project.ProductionMarkers"
        insert = (
            "        var allowedAlign = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { \"unverified\", \"manual\", \"rejected\" };\n"
            "        var allowedAssembly = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { \"none\", \"preview\", \"approved\", \"discarded\" };\n"
            "        foreach (var req in project.PhraseReplacementRequests ?? new List<PhraseReplacementRequestDocument>())\n"
            "        {\n"
            "            if (string.IsNullOrWhiteSpace(req.RequestId))\n"
            "                throw new InvalidDataException(\"Phrase replacement requestId is required.\");\n"
            "            if (string.IsNullOrWhiteSpace(req.PhraseText))\n"
            "                throw new InvalidDataException(\"Phrase text must not be blank.\");\n"
            "            if (!allowedAlign.Contains(req.AlignmentStatus ?? \"\"))\n"
            "                throw new InvalidDataException(\"Invalid alignmentStatus.\");\n"
            "            if (!allowedAssembly.Contains(req.AssemblyStatus ?? \"\"))\n"
            "                throw new InvalidDataException(\"Invalid assemblyStatus.\");\n"
            "            if (req.CharStart is int cs && req.CharEnd is int ce && (cs < 0 || ce < cs))\n"
            "                throw new InvalidDataException(\"Invalid phrase character range.\");\n"
            "            if (!string.IsNullOrWhiteSpace(req.SourceTakeId) &&\n"
            "                (project.Takes is null || !project.Takes.Any(t => t.TakeId == req.SourceTakeId)))\n"
            "                throw new InvalidDataException(\"Phrase sourceTakeId must reference a project take.\");\n"
            "        }\n\n"
        )
        if needle in text:
            text = text.replace(needle, insert + needle, 1)
        else:
            print("WARN: could not inject Windows validation loop")

    path.write_text(text, encoding="utf-8")
    print("Windows phrase replacement store applied")


def main() -> int:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target in ("all", "android"):
        ensure_android()
    if target in ("all", "windows"):
        ensure_windows()
    print("ci_apply_phrase_replacement_store:", target, "OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
