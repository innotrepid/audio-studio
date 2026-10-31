using AudioStudio;

var root = Path.Combine(Path.GetTempPath(), "AudioStudio-CreatorStore-Tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    RoundTripPersistsProject(root);
    RecoveryRestoresLastValidGeneration(Path.Combine(root, "recovery"));
    InvalidPrimaryIsPreserved(Path.Combine(root, "invalid"));
    InvalidTakeReferencesAreRejected(Path.Combine(root, "validation"));
    DraftsRoundTripAndRecover(Path.Combine(root, "draft-recovery"));
    InvalidDraftPrimaryIsPreserved(Path.Combine(root, "draft-invalid"));
    CaptionTimingAndExportAreValidated(Path.Combine(root, "captions"));
    Console.WriteLine("Creator project, named-draft, and caption persistence tests passed (7 scenarios).");
}
finally
{
    Directory.Delete(root, recursive: true);
}

static void RoundTripPersistsProject(string root)
{
    var store = new CreatorProjectStore(root);
    var created = store.LoadOrCreate().Project;
    created.Title = "Studio session";
    created.Scripts[0].Sections[0].Text = "A saved script line.";
    store.Save(created);

    var loaded = new CreatorProjectStore(root).LoadOrCreate().Project;
    Assert(loaded.Title == "Studio session", "Project title did not round-trip.");
    Assert(loaded.Scripts[0].Sections[0].Text == "A saved script line.", "Script text did not round-trip.");
}

static void RecoveryRestoresLastValidGeneration(string root)
{
    Directory.CreateDirectory(root);
    var store = new CreatorProjectStore(root);
    var initial = store.LoadOrCreate().Project;
    initial.Title = "Newer generation";
    store.Save(initial);

    var primary = Path.Combine(root, "default-project.json");
    File.WriteAllText(primary, "{ malformed");
    var recovered = new CreatorProjectStore(root).LoadOrCreate();

    Assert(recovered.RecoveredFromBackup, "Recovery did not report backup restoration.");
    Assert(recovered.Project.Title == "My first project", "Recovery did not restore the last valid generation.");
    Assert(File.ReadAllText(primary).Contains("My first project", StringComparison.Ordinal), "Recovered primary was not restored.");
}

static void InvalidPrimaryIsPreserved(string root)
{
    Directory.CreateDirectory(root);
    var primary = Path.Combine(root, "default-project.json");
    const string malformed = "{ definitely not valid JSON";
    File.WriteAllText(primary, malformed);

    var threw = false;
    try { new CreatorProjectStore(root).LoadOrCreate(); }
    catch (InvalidDataException) { threw = true; }

    Assert(threw, "Malformed project without backup should fail safely.");
    Assert(File.ReadAllText(primary) == malformed, "Malformed source file should remain available for diagnosis.");
}

static void InvalidTakeReferencesAreRejected(string root)
{
    var store = new CreatorProjectStore(root);
    var project = store.LoadOrCreate().Project;
    project.Takes.Add(new CreatorProjectStore.TakeDocument
    {
        TakeId = Guid.NewGuid().ToString(), SourcePath = Path.Combine(root, "take.wav"),
        DurationMs = 1200, SampleRateHz = 48000, Channels = 1,
        ScriptId = "missing-script", SectionId = "missing-section"
    });

    var threw = false;
    try { store.Save(project); }
    catch (InvalidDataException) { threw = true; }
    Assert(threw, "A take referencing a missing script must be rejected.");
}

static void DraftsRoundTripAndRecover(string root)
{
    Directory.CreateDirectory(root);
    var projectRoot = Path.Combine(root, "project");
    var script = new CreatorProjectStore(projectRoot).LoadOrCreate().Project.Scripts[0];
    var store = new CreatorDraftStore(Path.Combine(root, "drafts"));
    var document = CreatorDraftStore.Create("First draft", script, null);
    document = CreatorDraftStore.Create("Second draft", script, document);
    store.Save(document);

    var changedScript = new CreatorProjectStore.ScriptDocument
    {
        ScriptId = script.ScriptId, Title = "Updated script",
        Sections = script.Sections
    };
    CreatorDraftStore.Update(document, document.ActiveDraftId, "Renamed second", changedScript);
    store.Save(document);

    var primary = Path.Combine(root, "drafts", "drafts.json");
    File.WriteAllText(primary, "{ malformed");
    var recovered = new CreatorDraftStore(Path.Combine(root, "drafts")).Load();

    Assert(recovered is not null, "A valid draft backup should be recovered.");
    Assert(recovered!.Drafts.Count == 2, "Recovery should preserve both named drafts.");
    Assert(recovered.Drafts.Any(d => d.Name == "Second draft"), "Recovery should restore the previous valid draft generation.");
    Assert(File.ReadAllText(primary).Contains("Second draft", StringComparison.Ordinal), "Recovered draft primary was not restored.");
}

static void InvalidDraftPrimaryIsPreserved(string root)
{
    Directory.CreateDirectory(root);
    var primary = Path.Combine(root, "drafts.json");
    const string malformed = "{ invalid draft JSON";
    File.WriteAllText(primary, malformed);

    var threw = false;
    try { new CreatorDraftStore(root).Load(); }
    catch (InvalidDataException) { threw = true; }

    Assert(threw, "Malformed named drafts without backup should fail safely.");
    Assert(File.ReadAllText(primary) == malformed, "Malformed named-draft source should be preserved.");
}

static void CaptionTimingAndExportAreValidated(string root)
{
    var store = new CreatorProjectStore(root);
    var project = store.LoadOrCreate().Project;
    project.Transcript = "One two three four five six seven";
    project.Captions = CreatorProjectStore.GenerateDraftCaptions(project.Transcript, 3);
    store.Save(project);

    var loaded = new CreatorProjectStore(root).LoadOrCreate().Project;
    Assert(loaded.Transcript == project.Transcript, "Transcript did not round-trip.");
    Assert(loaded.Captions.Count == 3, "Draft captions were not persisted.");
    Assert(CreatorProjectStore.ExportSrt(loaded.Captions).Contains("00:00:00,000 -->"), "SRT timestamps were not formatted.");
    Assert(CreatorProjectStore.ExportWebVtt(loaded.Captions).StartsWith("WEBVTT", StringComparison.Ordinal), "WebVTT header is missing.");

    loaded.Captions[0].EndMs = loaded.Captions[0].StartMs;
    var threw = false;
    try { store.Save(loaded); }
    catch (InvalidDataException) { threw = true; }
    Assert(threw, "Invalid caption timing must be rejected.");
}

static void Assert(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
