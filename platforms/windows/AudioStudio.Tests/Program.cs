using AudioStudio;

var root = Path.Combine(Path.GetTempPath(), "AudioStudio-CreatorStore-Tests-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
try
{
    RoundTripPersistsProject(root);
    RecoveryRestoresLastValidGeneration(Path.Combine(root, "recovery"));
    InvalidPrimaryIsPreserved(Path.Combine(root, "invalid"));
    InvalidTakeReferencesAreRejected(Path.Combine(root, "validation"));
    Console.WriteLine("Creator project persistence tests passed (4 scenarios).");
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
    try
    {
        new CreatorProjectStore(root).LoadOrCreate();
    }
    catch (InvalidDataException)
    {
        threw = true;
    }

    Assert(threw, "Malformed project without backup should fail safely.");
    Assert(File.ReadAllText(primary) == malformed, "Malformed source file should remain available for diagnosis.");
}

static void InvalidTakeReferencesAreRejected(string root)
{
    var store = new CreatorProjectStore(root);
    var project = store.LoadOrCreate().Project;
    project.Takes.Add(new CreatorProjectStore.TakeDocument
    {
        TakeId = Guid.NewGuid().ToString(),
        SourcePath = Path.Combine(root, "take.wav"),
        DurationMs = 1200,
        SampleRateHz = 48000,
        Channels = 1,
        ScriptId = "missing-script",
        SectionId = "missing-section"
    });

    var threw = false;
    try
    {
        store.Save(project);
    }
    catch (InvalidDataException)
    {
        threw = true;
    }
    Assert(threw, "A take referencing a missing script must be rejected.");
}

static void Assert(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}
