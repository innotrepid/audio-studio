using System.Text.Json;

namespace AudioStudio;

/// <summary>Stores named script snapshots separately from the project and its recorded takes.</summary>
internal sealed class CreatorDraftStore
{
    internal sealed class DraftDocument
    {
        public int SchemaVersion { get; set; } = 1;
        public string ActiveDraftId { get; set; } = string.Empty;
        public List<Draft> Drafts { get; set; } = new();
    }

    internal sealed class Draft
    {
        public string DraftId { get; set; } = Guid.NewGuid().ToString();
        public string Name { get; set; } = string.Empty;
        public string CreatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public string UpdatedUtc { get; set; } = DateTime.UtcNow.ToString("O");
        public CreatorProjectStore.ScriptDocument Script { get; set; } = new();
    }

    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        WriteIndented = true
    };
    private readonly string _path;
    private string BackupPath => _path + ".bak";
    private const int MaxBytes = 1_048_576;

    internal CreatorDraftStore(string? rootDirectory = null)
    {
        var folder = rootDirectory is null
            ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "AudioStudio", "Creator")
            : Path.GetFullPath(rootDirectory);
        Directory.CreateDirectory(folder);
        _path = Path.Combine(folder, "drafts.json");
    }

    internal DraftDocument? Load()
    {
        if (!File.Exists(_path) && !File.Exists(BackupPath)) return null;

        Exception? primaryError = null;
        if (File.Exists(_path))
        {
            try { return ReadAndValidate(_path); }
            catch (Exception error) { primaryError = error; }
        }

        if (File.Exists(BackupPath))
        {
            try
            {
                var recovered = ReadAndValidate(BackupPath);
                RestorePrimaryWithoutRotatingBackup(recovered);
                return recovered;
            }
            catch (Exception backupError)
            {
                throw new InvalidDataException(
                    $"The named drafts and their recovery copy are invalid. Both files were preserved. Primary: {primaryError?.Message ?? "missing"}. Backup: {backupError.Message}",
                    backupError);
            }
        }

        throw new InvalidDataException(
            $"The saved named drafts are invalid and were preserved: {primaryError?.Message ?? "unknown error"}",
            primaryError);
    }

    private DraftDocument ReadAndValidate(string path)
    {
        if (new FileInfo(path).Length > MaxBytes)
            throw new InvalidDataException("Draft data exceeds the 1 MB safety limit.");
        var document = JsonSerializer.Deserialize<DraftDocument>(File.ReadAllBytes(path), Options)
            ?? throw new InvalidDataException("Draft document is empty.");
        Validate(document);
        return document;
    }

    private void RestorePrimaryWithoutRotatingBackup(DraftDocument document)
    {
        var bytes = JsonSerializer.SerializeToUtf8Bytes(document, Options);
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
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    internal void Save(DraftDocument document)
    {
        Validate(document);
        var bytes = JsonSerializer.SerializeToUtf8Bytes(document, Options);
        if (bytes.Length > MaxBytes) throw new InvalidDataException("Draft data exceeds the 1 MB safety limit.");
        var temporary = _path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            using (var stream = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                stream.Write(bytes);
                stream.Flush(flushToDisk: true);
            }
            if (File.Exists(_path)) File.Replace(temporary, _path, BackupPath, ignoreMetadataErrors: true);
            else File.Move(temporary, _path);
        }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }

    internal static DraftDocument Create(string name, CreatorProjectStore.ScriptDocument script, DraftDocument? current)
    {
        var clean = name.Trim();
        if (clean.Length is < 1 or > 200) throw new InvalidDataException("Draft name must contain 1–200 characters.");
        var now = DateTime.UtcNow.ToString("O");
        var draft = new Draft { Name = clean, CreatedUtc = now, UpdatedUtc = now, Script = Clone(script) };
        var result = new DraftDocument
        {
            Drafts = (current?.Drafts ?? new List<Draft>()).Append(draft).ToList(),
            ActiveDraftId = draft.DraftId
        };
        Validate(result);
        return result;
    }

    internal static DraftDocument Update(DraftDocument document, string id, string name, CreatorProjectStore.ScriptDocument script)
    {
        var clean = name.Trim();
        if (clean.Length is < 1 or > 200) throw new InvalidDataException("Draft name must contain 1–200 characters.");
        var draft = document.Drafts.FirstOrDefault(item => item.DraftId == id)
            ?? throw new InvalidDataException("Selected draft no longer exists.");
        draft.Name = clean;
        draft.Script = Clone(script);
        draft.UpdatedUtc = DateTime.UtcNow.ToString("O");
        Validate(document);
        return document;
    }

    internal static DraftDocument Delete(DraftDocument document, string id)
    {
        if (document.Drafts.Count <= 1) throw new InvalidDataException("The last draft cannot be deleted.");
        var index = document.Drafts.FindIndex(item => item.DraftId == id);
        if (index < 0) throw new InvalidDataException("Selected draft no longer exists.");
        document.Drafts.RemoveAt(index);
        if (document.ActiveDraftId == id) document.ActiveDraftId = document.Drafts[Math.Min(index, document.Drafts.Count - 1)].DraftId;
        Validate(document);
        return document;
    }

    private static CreatorProjectStore.ScriptDocument Clone(CreatorProjectStore.ScriptDocument script) =>
        JsonSerializer.Deserialize<CreatorProjectStore.ScriptDocument>(JsonSerializer.SerializeToUtf8Bytes(script, Options), Options)
        ?? throw new InvalidDataException("Script snapshot could not be copied.");

    private static void Validate(DraftDocument document)
    {
        if (document.SchemaVersion != 1 || document.Drafts.Count is < 1 or > 100)
            throw new InvalidDataException("Draft schema or count is invalid.");
        if (document.Drafts.Any(d => string.IsNullOrWhiteSpace(d.DraftId) ||
            string.IsNullOrWhiteSpace(d.Name) || d.Name.Length > 200 ||
            string.IsNullOrWhiteSpace(d.Script.ScriptId) || d.Script.Sections.Count == 0))
            throw new InvalidDataException("One or more named drafts are invalid.");
        if (document.Drafts.Select(d => d.DraftId).Distinct(StringComparer.Ordinal).Count() != document.Drafts.Count ||
            !document.Drafts.Any(d => d.DraftId == document.ActiveDraftId))
            throw new InvalidDataException("Draft IDs or active draft reference are invalid.");
    }
}
