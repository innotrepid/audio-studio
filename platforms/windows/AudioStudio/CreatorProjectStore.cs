using System.Text.Json;
using System.Text.Json.Serialization;

namespace AudioStudio;

internal sealed class CreatorProjectStore
{
    internal sealed class ProjectDocument
    {
        public int SchemaVersion { get; set; } = 1;
        public string ProjectId { get; set; } = Guid.NewGuid().ToString();
        public string Title { get; set; } = "My first project";
        public string CreatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public string UpdatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public List<ScriptDocument> Scripts { get; set; } = new();
        public List<TakeDocument> Takes { get; set; } = new();
        public string Transcript { get; set; } = string.Empty;
        public List<CaptionDocument> Captions { get; set; } = new();
        public List<ProductionMarkerDocument> ProductionMarkers { get; set; } = new();
    }

    internal sealed class ScriptDocument
    {
        public string ScriptId { get; set; } = Guid.NewGuid().ToString();
        public string Title { get; set; } = "First script";
        public string CreatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public string UpdatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public List<SectionDocument> Sections { get; set; } = new();
    }

    internal sealed class SectionDocument
    {
        public string SectionId { get; set; } = Guid.NewGuid().ToString();
        public string Title { get; set; } = "Opening";
        public string Text { get; set; } = string.Empty;
        public int Order { get; set; }
        public string Notes { get; set; } = string.Empty;
        public List<string> Cues { get; set; } = new();
    }

    internal sealed class ProductionMarkerDocument
    {
        public string MarkerId { get; set; } = Guid.NewGuid().ToString();
        public string SectionId { get; set; } = string.Empty;
        public string Type { get; set; } = "Retake";
        public string Note { get; set; } = string.Empty;
        public string CreatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
    }

    internal sealed class CaptionDocument
    {
        public string CaptionId { get; set; } = Guid.NewGuid().ToString();
        public long StartMs { get; set; }
        public long EndMs { get; set; }
        public string Text { get; set; } = string.Empty;
    }

    internal sealed class TakeDocument
    {
        public string TakeId { get; set; } = string.Empty;
        public string? SourceUri { get; set; }
        public string? SourcePath { get; set; }
        public string? CreatedUtc { get; set; }
        public long? DurationMs { get; set; }
        public int? SampleRateHz { get; set; }
        public int? Channels { get; set; }
        public string? ScriptId { get; set; }
        public string? SectionId { get; set; }
        public string? Status { get; set; }
    }

    internal sealed record LoadResult(ProjectDocument Project, bool RecoveredFromBackup);

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        WriteIndented = true
    };

    private readonly string _path;
    private string BackupPath => _path + ".bak";
    private const int MaxDocumentBytes = 1_048_576;
    private const int MaxTitleLength = 200;
    private const int MaxSectionTextLength = 100_000;

    internal CreatorProjectStore(string? rootDirectory = null)
    {
        var folder = rootDirectory is null
            ? Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "AudioStudio", "Projects")
            : Path.GetFullPath(rootDirectory);
        Directory.CreateDirectory(folder);
        _path = Path.Combine(folder, "default-project.json");
    }

    internal LoadResult LoadOrCreate()
    {
        if (!File.Exists(_path) && !File.Exists(BackupPath))
        {
            var project = CreateDefault();
            Save(project);
            return new LoadResult(project, false);
        }

        Exception? primaryError = null;
        if (File.Exists(_path))
        {
            try
            {
                return new LoadResult(ReadAndValidate(_path), false);
            }
            catch (Exception error)
            {
                primaryError = error;
            }
        }

        if (File.Exists(BackupPath))
        {
            try
            {
                var recovered = ReadAndValidate(BackupPath);
                RestorePrimaryWithoutRotatingBackup(recovered);
                return new LoadResult(recovered, true);
            }
            catch (Exception backupError)
            {
                throw new InvalidDataException(
                    $"The Creator project and its recovery copy are invalid. Both files were preserved. Primary: {primaryError?.Message ?? "missing"}. Backup: {backupError.Message}",
                    backupError);
            }
        }

        throw new InvalidDataException(
            $"The saved Creator project is invalid and was preserved: {primaryError?.Message ?? "unknown error"}",
            primaryError);
    }

    internal void Save(ProjectDocument project)
    {
        project.UpdatedUtc = DateTime.UtcNow.ToString("O");
        Validate(project);
        var bytes = JsonSerializer.SerializeToUtf8Bytes(project, JsonOptions);
        if (bytes.Length > MaxDocumentBytes)
            throw new InvalidDataException("Project file exceeds the 1 MB safety limit.");

        var directory = Path.GetDirectoryName(_path)!;
        var temporary = Path.Combine(directory, $"default-project.{Guid.NewGuid():N}.tmp");
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                stream.Write(bytes);
                stream.Flush(flushToDisk: true);
            }

            if (File.Exists(_path))
                File.Replace(temporary, _path, BackupPath, ignoreMetadataErrors: true);
            else
                File.Move(temporary, _path);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    private ProjectDocument ReadAndValidate(string path)
    {
        var info = new FileInfo(path);
        if (info.Length > MaxDocumentBytes)
            throw new InvalidDataException("Project file exceeds the 1 MB safety limit.");
        var project = JsonSerializer.Deserialize<ProjectDocument>(File.ReadAllBytes(path), JsonOptions)
            ?? throw new InvalidDataException("Project document is empty.");
        Validate(project);
        return project;
    }

    private void RestorePrimaryWithoutRotatingBackup(ProjectDocument project)
    {
        var bytes = JsonSerializer.SerializeToUtf8Bytes(project, JsonOptions);
        var temporary = _path + ".recovery.tmp";
        try
        {
            using (var stream = new FileStream(temporary, FileMode.Create, FileAccess.Write, FileShare.None))
            {
                stream.Write(bytes);
                stream.Flush(flushToDisk: true);
            }
            File.Move(temporary, _path, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary))
                File.Delete(temporary);
        }
    }

    private static ProjectDocument CreateDefault()
    {
        var now = DateTime.UtcNow.ToString("O");
        return new ProjectDocument
        {
            CreatedUtc = now,
            UpdatedUtc = now,
            Scripts =
            {
                new ScriptDocument
                {
                    CreatedUtc = now,
                    UpdatedUtc = now,
                    Sections = { new SectionDocument() }
                }
            }
        };
    }

    internal string ExportJson(ProjectDocument project)
    {
        Validate(project);
        var bytes = JsonSerializer.SerializeToUtf8Bytes(project, JsonOptions);
        if (bytes.Length > MaxDocumentBytes) throw new InvalidDataException("Project file exceeds the 1 MB safety limit.");
        return System.Text.Encoding.UTF8.GetString(bytes);
    }

    internal ProjectDocument ImportJson(string json)
    {
        if (System.Text.Encoding.UTF8.GetByteCount(json) > MaxDocumentBytes)
            throw new InvalidDataException("Project file exceeds the 1 MB safety limit.");
        var project = JsonSerializer.Deserialize<ProjectDocument>(json, JsonOptions)
            ?? throw new InvalidDataException("Project document is empty.");
        Validate(project);
        return project;
    }

    /// <summary>Creates estimated caption windows from reading pace, not audio-aligned timestamps.</summary>
    internal static List<CaptionDocument> GenerateDraftCaptions(string transcript, int maxWords, int wordsPerMinute = 150)
    {
        if (maxWords is < 3 or > 30) throw new ArgumentOutOfRangeException(nameof(maxWords), "Words per caption must be between 3 and 30.");
        if (wordsPerMinute is < 30 or > 300) throw new ArgumentOutOfRangeException(nameof(wordsPerMinute), "Reading pace must be between 30 and 300 words per minute.");
        var words = (transcript ?? string.Empty).Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
        long cursor = 0;
        return words.Chunk(maxWords).Select(chunk =>
        {
            var duration = Math.Max(1_000L, (long)Math.Ceiling(chunk.Length * 60_000.0 / wordsPerMinute));
            var caption = new CaptionDocument { StartMs = cursor, EndMs = cursor + duration, Text = string.Join(" ", chunk) };
            cursor += duration;
            return caption;
        }).ToList();
    }

    internal static string ExportSrt(IEnumerable<CaptionDocument> captions)
    {
        var ordered = captions.OrderBy(caption => caption.StartMs).ToList();
        return ordered.Count == 0 ? string.Empty : string.Join(Environment.NewLine + Environment.NewLine,
            ordered.Select((caption, index) =>
                (index + 1).ToString(System.Globalization.CultureInfo.InvariantCulture) + Environment.NewLine +
                FormatCaptionTime(caption.StartMs, ',') + " --> " + FormatCaptionTime(caption.EndMs, ',') +
                Environment.NewLine + caption.Text)) + Environment.NewLine;
    }

    internal static string ExportWebVtt(IEnumerable<CaptionDocument> captions)
    {
        var ordered = captions.OrderBy(caption => caption.StartMs).ToList();
        return "WEBVTT" + Environment.NewLine + Environment.NewLine +
            (ordered.Count == 0 ? string.Empty : string.Join(Environment.NewLine + Environment.NewLine,
                ordered.Select(caption => FormatCaptionTime(caption.StartMs, '.') + " --> " +
                    FormatCaptionTime(caption.EndMs, '.') + Environment.NewLine + caption.Text)) + Environment.NewLine);
    }

    private static string FormatCaptionTime(long milliseconds, char separator)
    {
        var hours = milliseconds / 3_600_000;
        var minutes = milliseconds / 60_000 % 60;
        var seconds = milliseconds / 1_000 % 60;
        var millis = milliseconds % 1_000;
        return $"{hours:00}:{minutes:00}:{seconds:00}{separator}{millis:000}";
    }

    internal static string ExportNarration(ScriptDocument script) =>
        string.Join(Environment.NewLine + Environment.NewLine,
            script.Sections.OrderBy(section => section.Order).Select(section => section.Text ?? string.Empty));

    internal static string ExportProductionCopy(ScriptDocument script) =>
        script.Title + Environment.NewLine + Environment.NewLine +
        string.Join(Environment.NewLine + Environment.NewLine,
            script.Sections.OrderBy(section => section.Order).Select(section =>
                section.Title + Environment.NewLine +
                (section.Cues.Count == 0 ? string.Empty : "[" + string.Join("] [", section.Cues) + "]" + Environment.NewLine) +
                (string.IsNullOrWhiteSpace(section.Notes) ? string.Empty : "Notes: " + section.Notes + Environment.NewLine) +
                section.Text));

    // Starter outlines use editable prompts rather than generated narration.
    internal static ScriptDocument CreateTemplate(string title, string template)
    {
        var now = DateTime.UtcNow.ToString("O");
        var outline = template.Trim().ToLowerInvariant() switch
        {
            "youtube" or "video intro" => new[] { ("Hook", "[HOOK]\\nOpen with the viewer's problem or curiosity."), ("Intro", "[INTRO]\\nIntroduce the topic and promise."), ("Main points", "[MAIN POINT]\\nDeliver the key information."), ("Outro", "[OUTRO]\\nSummarize and invite the next action.") },
            "podcast" => new[] { ("Cold open", "[INTRO]\\nStart with a compelling moment."), ("Welcome", "Welcome listeners and introduce the episode."), ("Discussion", "[MAIN POINT]\\nEpisode discussion and stories."), ("Wrap-up", "[OUTRO]\\nRecap, credits, and next episode.") },
            "tutorial" => new[] { ("Outcome", "[INTRO]\\nShow what the viewer will learn."), ("Prerequisites", "List tools or knowledge needed."), ("Steps", "[MAIN POINT]\\nExplain the process step by step."), ("Recap", "[OUTRO]\\nReview the result and next steps.") },
            "product promo" or "promo" => new[] { ("Problem", "[INTRO]\\nIdentify the audience's pain point."), ("Solution", "[MAIN POINT]\\nIntroduce the product and benefit."), ("Proof", "Add a specific, verifiable proof point."), ("Call to action", "[OUTRO]\\nState one clear next action.") },
            _ => throw new ArgumentException("Choose YouTube/video intro, podcast, tutorial, or product promo.", nameof(template))
        };
        return new ScriptDocument
        {
            ScriptId = Guid.NewGuid().ToString(),
            Title = string.IsNullOrWhiteSpace(title) ? template : title.Trim(),
            CreatedUtc = now,
            UpdatedUtc = now,
            Sections = outline.Select((item, index) => new SectionDocument
            {
                SectionId = Guid.NewGuid().ToString(), Title = item.Item1,
                Text = item.Item2, Order = index
            }).ToList()
        };
    }

    private static void Validate(ProjectDocument project)
    {
        if (project.SchemaVersion != 1)
            throw new InvalidDataException($"Unsupported project schema version: {project.SchemaVersion}.");
        if (string.IsNullOrWhiteSpace(project.ProjectId))
            throw new InvalidDataException("Project ID is required.");
        RequireTitle(project.Title, "Project");
        if (project.Scripts is null || project.Scripts.Count == 0)
            throw new InvalidDataException("At least one script is required.");

        var ids = new HashSet<string>(StringComparer.Ordinal) { project.ProjectId };
        foreach (var script in project.Scripts)
        {
            if (string.IsNullOrWhiteSpace(script.ScriptId) || !ids.Add(script.ScriptId))
                throw new InvalidDataException("Script IDs must be present and unique.");
            RequireTitle(script.Title, "Script");
            if (script.Sections is null || script.Sections.Count == 0)
                throw new InvalidDataException("Each script must contain at least one section.");
            foreach (var section in script.Sections)
            {
                if (string.IsNullOrWhiteSpace(section.SectionId) || !ids.Add(section.SectionId))
                    throw new InvalidDataException("Section IDs must be present and unique.");
                RequireTitle(section.Title, "Section");
                if (section.Order < 0)
                    throw new InvalidDataException("Section order cannot be negative.");
                if ((section.Text?.Length ?? 0) > MaxSectionTextLength)
                    throw new InvalidDataException("Section text exceeds the 100,000 character limit.");
            }
        }

        if ((project.Transcript?.Length ?? 0) > MaxSectionTextLength)
            throw new InvalidDataException("Transcript exceeds the 100,000 character limit.");
        if (project.Captions is null || project.Captions.Count > 10_000)
            throw new InvalidDataException("Caption count exceeds the 10,000 item limit.");
        foreach (var caption in project.Captions)
        {
            if (string.IsNullOrWhiteSpace(caption.CaptionId) || !ids.Add(caption.CaptionId))
                throw new InvalidDataException("Caption IDs must be present and unique.");
            if (caption.StartMs < 0 || caption.EndMs <= caption.StartMs)
                throw new InvalidDataException("Caption end time must be after its start time.");
            if (string.IsNullOrWhiteSpace(caption.Text) || caption.Text.Length > 1_000)
                throw new InvalidDataException("Caption text must contain 1–1,000 characters.");
        }

        foreach (var take in project.Takes ?? new List<TakeDocument>())
        {
            if (string.IsNullOrWhiteSpace(take.TakeId) || !ids.Add(take.TakeId))
                throw new InvalidDataException("Take IDs must be present and unique.");
            if (string.IsNullOrWhiteSpace(take.SourceUri) && string.IsNullOrWhiteSpace(take.SourcePath))
                throw new InvalidDataException("A take must reference its existing audio file.");
            if (take.DurationMs is null or <= 0 || take.SampleRateHz is null or <= 0 ||
                take.Channels is not (1 or 2))
                throw new InvalidDataException("A take must contain valid verified audio metadata.");
            if (take.ScriptId is not null && !project.Scripts.Any(script => script.ScriptId == take.ScriptId))
                throw new InvalidDataException("A take references a missing script.");
            if (take.SectionId is not null)
            {
                if (take.ScriptId is null)
                    throw new InvalidDataException("A section-linked take must also reference its script.");
                var referencedScript = project.Scripts.First(script => script.ScriptId == take.ScriptId);
                if (!referencedScript.Sections.Any(section => section.SectionId == take.SectionId))
                    throw new InvalidDataException("A take section must exist inside its referenced script.");
            }
        }
    }

    private static void RequireTitle(string? title, string entity)
    {
        if (string.IsNullOrWhiteSpace(title) || title.Length > MaxTitleLength)
            throw new InvalidDataException($"{entity} title must contain 1–200 characters.");
    }
}
