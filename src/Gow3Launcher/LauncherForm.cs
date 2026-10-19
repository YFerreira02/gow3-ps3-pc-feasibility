using System.Diagnostics;
using System.Buffers.Binary;
using System.Text;
using System.Text.Json;

namespace Gow3Launcher;

internal sealed class LauncherSettings
{
    public string Rpcs3Path { get; set; } = "";
    public string GameFolder { get; set; } = "";
    public bool DirectBoot { get; set; } = true;
}

internal sealed class LauncherForm : Form
{
    private const string ExpectedTitleId = "BCES00510";
    private readonly TextBox _rpcs3Path = new() { Dock = DockStyle.Fill };
    private readonly TextBox _gamePath = new() { Dock = DockStyle.Fill };
    private readonly CheckBox _directBoot = new() { Text = "Iniciar el juego directamente ( --no-gui )", Checked = true, AutoSize = true };
    private readonly Label _status = new() { AutoSize = true, Text = "Selecciona RPCS3 y una carpeta PS3_GAME extraída." };
    private readonly TextBox _output = new() { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Dock = DockStyle.Fill };
    private readonly string _settingsPath;

    public LauncherForm()
    {
        Text = "GOW III PS3 · Windows launcher (experimental)";
        MinimumSize = new Size(760, 500);
        Size = new Size(900, 610);
        StartPosition = FormStartPosition.CenterScreen;

        var appData = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GOW3PS3PC");
        Directory.CreateDirectory(appData);
        _settingsPath = Path.Combine(appData, "launcher.json");
        LoadSettings();
        BuildLayout();
        RefreshReadiness();
    }

    private void BuildLayout()
    {
        var root = new TableLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(16), ColumnCount = 1, RowCount = 5 };
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Controls.Add(root);

        var heading = new Label
        {
            Text = "God of War III · PS3",
            Font = new Font(Font.FontFamily, 18, FontStyle.Bold),
            AutoSize = true,
            Padding = new Padding(0, 0, 0, 5)
        };
        root.Controls.Add(heading, 0, 0);

        var note = new Label
        {
            Text = "Prototipo de launcher para Windows 11. La ejecución del juego depende de RPCS3; esto aún no es un port nativo ni promete más FPS. Vulkan 1.2 se comprueba aparte.",
            AutoSize = true,
            MaximumSize = new Size(820, 0),
            Padding = new Padding(0, 0, 0, 12)
        };
        root.Controls.Add(note, 0, 1);

        var paths = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 3, RowCount = 2, AutoSize = true, Padding = new Padding(0, 0, 0, 8) };
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        paths.Controls.Add(new Label { Text = "RPCS3.exe", AutoSize = true, Anchor = AnchorStyles.Left, Padding = new Padding(0, 7, 10, 0) }, 0, 0);
        paths.Controls.Add(_rpcs3Path, 1, 0);
        var selectRpcs3 = new Button { Text = "Buscar…", AutoSize = true };
        selectRpcs3.Click += (_, _) => ChooseRpcs3();
        paths.Controls.Add(selectRpcs3, 2, 0);
        paths.Controls.Add(new Label { Text = "Juego extraído", AutoSize = true, Anchor = AnchorStyles.Left, Padding = new Padding(0, 7, 10, 0) }, 0, 1);
        paths.Controls.Add(_gamePath, 1, 1);
        var selectGame = new Button { Text = "Buscar carpeta…", AutoSize = true };
        selectGame.Click += (_, _) => ChooseGameFolder();
        paths.Controls.Add(selectGame, 2, 1);
        _rpcs3Path.TextChanged += (_, _) => { SaveSettings(); RefreshReadiness(); };
        _gamePath.TextChanged += (_, _) => { SaveSettings(); RefreshReadiness(); };
        root.Controls.Add(paths, 0, 2);

        var actions = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Padding = new Padding(0, 4, 0, 10) };
        _directBoot.CheckedChanged += (_, _) => SaveSettings();
        actions.Controls.Add(_directBoot);
        var launch = new Button { Text = "Iniciar God of War III", AutoSize = true, Padding = new Padding(10, 4, 10, 4) };
        launch.Click += (_, _) => LaunchGame();
        actions.Controls.Add(launch);
        var vulkan = new Button { Text = "Comprobar Vulkan 1.2", AutoSize = true, Padding = new Padding(10, 4, 10, 4) };
        vulkan.Click += async (_, _) => await RunVulkanProbeAsync(vulkan);
        actions.Controls.Add(vulkan);
        root.Controls.Add(actions, 0, 3);

        var bottom = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2 };
        bottom.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        bottom.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        _status.Padding = new Padding(0, 4, 0, 8);
        bottom.Controls.Add(_status, 0, 0);
        _output.Font = new Font("Consolas", 9);
        bottom.Controls.Add(_output, 0, 1);
        root.Controls.Add(bottom, 0, 4);
    }

    private void LoadSettings()
    {
        try
        {
            if (!File.Exists(_settingsPath)) return;
            var settings = JsonSerializer.Deserialize<LauncherSettings>(File.ReadAllText(_settingsPath));
            if (settings is null) return;
            _rpcs3Path.Text = settings.Rpcs3Path;
            _gamePath.Text = settings.GameFolder;
            _directBoot.Checked = settings.DirectBoot;
        }
        catch (Exception ex)
        {
            AppendLog($"No se pudo leer la configuración; se usarán valores vacíos. {ex.Message}");
        }
    }

    private void SaveSettings()
    {
        try
        {
            var settings = new LauncherSettings { Rpcs3Path = _rpcs3Path.Text.Trim(), GameFolder = _gamePath.Text.Trim(), DirectBoot = _directBoot.Checked };
            File.WriteAllText(_settingsPath, JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch (Exception ex) { AppendLog($"No se guardó la configuración: {ex.Message}"); }
    }

    private void ChooseRpcs3()
    {
        using var dialog = new OpenFileDialog { Title = "Selecciona RPCS3", Filter = "RPCS3 (rpcs3.exe)|rpcs3.exe|Ejecutables (*.exe)|*.exe" };
        if (dialog.ShowDialog(this) == DialogResult.OK) _rpcs3Path.Text = dialog.FileName;
    }

    private void ChooseGameFolder()
    {
        using var dialog = new FolderBrowserDialog { Description = "Selecciona la carpeta extraída que contiene PS3_GAME." };
        if (Directory.Exists(_gamePath.Text)) dialog.SelectedPath = _gamePath.Text;
        if (dialog.ShowDialog(this) == DialogResult.OK) _gamePath.Text = dialog.SelectedPath;
    }

    private string? GetPs3GameDirectory()
    {
        var selected = _gamePath.Text.Trim().Trim('"');
        if (!Directory.Exists(selected)) return null;
        var ps3Game = Path.GetFileName(selected).Equals("PS3_GAME", StringComparison.OrdinalIgnoreCase)
            ? selected
            : Path.Combine(selected, "PS3_GAME");
        return Directory.Exists(ps3Game) ? ps3Game : null;
    }

    private string? GetEbootPath()
    {
        var ps3Game = GetPs3GameDirectory();
        if (ps3Game is null) return null;
        var eboot = Path.Combine(ps3Game, "USRDIR", "EBOOT.BIN");
        return File.Exists(eboot) ? eboot : null;
    }

    private static string? ReadTitleId(string ps3GameDirectory)
    {
        var path = Path.Combine(ps3GameDirectory, "PARAM.SFO");
        if (!File.Exists(path)) return null;
        var info = new FileInfo(path);
        if (info.Length < 20 || info.Length > 4 * 1024 * 1024) return null;
        var data = File.ReadAllBytes(path);
        if (BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(0, 4)) != 0x46535000) return null;
        var keyTable = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(8, 4));
        var dataTable = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(12, 4));
        var entryCount = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(16, 4));
        if (entryCount > 512 || keyTable >= data.Length || dataTable >= data.Length || 20UL + entryCount * 16UL > (ulong)data.Length) return null;

        for (uint i = 0; i < entryCount; i++)
        {
            var entry = 20 + (int)(i * 16);
            var keyOffset = BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(entry, 2));
            var valueLength = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(entry + 4, 4));
            var valueOffset = BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(entry + 12, 4));
            var keyStart = (ulong)keyTable + keyOffset;
            var valueStart = (ulong)dataTable + valueOffset;
            if (keyStart >= (ulong)data.Length || valueStart > (ulong)data.Length || valueLength > (ulong)data.Length - valueStart) continue;
            var keyEnd = Array.IndexOf(data, (byte)0, (int)keyStart, (int)((ulong)data.Length - keyStart));
            if (keyEnd < 0) continue;
            var key = Encoding.ASCII.GetString(data, (int)keyStart, keyEnd - (int)keyStart);
            if (key is not ("TITLE_ID" or "GAME_ID")) continue;
            return Encoding.ASCII.GetString(data, (int)valueStart, (int)valueLength).TrimEnd('\0', ' ', '\r', '\n');
        }
        return null;
    }

    private void RefreshReadiness()
    {
        var emulatorReady = File.Exists(_rpcs3Path.Text.Trim().Trim('"'));
        var ps3Game = GetPs3GameDirectory();
        var eboot = GetEbootPath();
        var titleId = ps3Game is null ? null : ReadTitleId(ps3Game);
        var gameStatus = eboot is null
            ? "Juego: falta PS3_GAME\\USRDIR\\EBOOT.BIN"
            : titleId is null
                ? "Juego: falta PARAM.SFO legible"
                : titleId.Equals(ExpectedTitleId, StringComparison.OrdinalIgnoreCase)
                    ? $"God of War III: {titleId}"
                    : $"ID distinta: {titleId} (se espera {ExpectedTitleId})";
        _status.Text = (emulatorReady ? "RPCS3: encontrado" : "RPCS3: selecciona rpcs3.exe") + "  ·  " +
            gameStatus;
    }

    private void LaunchGame()
    {
        var emulator = _rpcs3Path.Text.Trim().Trim('"');
        var eboot = GetEbootPath();
        if (!File.Exists(emulator))
        {
            MessageBox.Show(this, "Selecciona un rpcs3.exe válido.", "Falta RPCS3", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        if (eboot is null)
        {
            MessageBox.Show(this, "Selecciona una carpeta extraída que contenga PS3_GAME\\USRDIR\\EBOOT.BIN. Esta versión no abre ISO directamente.", "Carpeta de juego no válida", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        var ps3Game = GetPs3GameDirectory();
        var titleId = ps3Game is null ? null : ReadTitleId(ps3Game);
        if (!ExpectedTitleId.Equals(titleId, StringComparison.OrdinalIgnoreCase))
        {
            MessageBox.Show(this, $"El PARAM.SFO no identifica esta edición como {ExpectedTitleId}. El launcher evita iniciar un título distinto o una carpeta incompleta.", "Juego no validado", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        var start = new ProcessStartInfo { FileName = emulator, WorkingDirectory = Path.GetDirectoryName(emulator)!, UseShellExecute = false };
        if (_directBoot.Checked) start.ArgumentList.Add("--no-gui");
        start.ArgumentList.Add(eboot);
        try
        {
            var process = Process.Start(start);
            if (process is null) throw new InvalidOperationException("Windows no creó el proceso RPCS3.");
            AppendLog($"Iniciado RPCS3 (PID {process.Id}) con el EBOOT seleccionado.");
            AppendLog("El primer arranque puede compilar cachés. FPS y compatibilidad no están garantizados.");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"No se pudo iniciar RPCS3: {ex.Message}", "Error de inicio", MessageBoxButtons.OK, MessageBoxIcon.Error);
            AppendLog($"Error de inicio: {ex}");
        }
    }

    private async Task RunVulkanProbeAsync(Button button)
    {
        var script = Path.Combine(AppContext.BaseDirectory, "tools", "Test-Vulkan12.ps1");
        if (!File.Exists(script)) script = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "tools", "Test-Vulkan12.ps1"));
        if (!File.Exists(script))
        {
            MessageBox.Show(this, "No se encuentra tools\\Test-Vulkan12.ps1. Ejecuta esta app desde el repositorio o instala la carpeta tools junto a ella.", "Falta el probe", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        button.Enabled = false;
        AppendLog("Comprobando Vulkan 1.2 en el adaptador Vega 8…");
        try
        {
            var start = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };
            start.ArgumentList.Add("-NoProfile");
            start.ArgumentList.Add("-ExecutionPolicy");
            start.ArgumentList.Add("Bypass");
            start.ArgumentList.Add("-File");
            start.ArgumentList.Add(script);
            start.ArgumentList.Add("-PreferDevice");
            start.ArgumentList.Add("Vega 8");
            using var process = Process.Start(start) ?? throw new InvalidOperationException("No se inició PowerShell.");
            var stdout = process.StandardOutput.ReadToEndAsync();
            var stderr = process.StandardError.ReadToEndAsync();
            await process.WaitForExitAsync();
            var output = await stdout;
            var error = await stderr;
            AppendLog(output.Trim());
            if (!string.IsNullOrWhiteSpace(error)) AppendLog(error.Trim());
            AppendLog($"Probe terminado con código {process.ExitCode}.");
        }
        catch (Exception ex) { AppendLog($"No se pudo ejecutar el probe Vulkan: {ex.Message}"); }
        finally { button.Enabled = true; }
    }

    private void AppendLog(string message)
    {
        if (InvokeRequired) { BeginInvoke(() => AppendLog(message)); return; }
        _output.AppendText($"[{DateTime.Now:HH:mm:ss}] {message}{Environment.NewLine}");
    }
}
