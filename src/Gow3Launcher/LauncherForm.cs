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
    public int ResolutionScalePercent { get; set; } = 50;
}

internal sealed class LauncherForm : Form
{
    private const string ExpectedTitleId = "BCES00510";
    private readonly TextBox _rpcs3Path = new() { Dock = DockStyle.Fill };
    private readonly TextBox _gamePath = new() { Dock = DockStyle.Fill };
    private readonly CheckBox _directBoot = new() { Text = "Iniciar el juego directamente ( --no-gui )", Checked = true, AutoSize = true };
    private readonly TrackBar _resolutionScale = new() { Minimum = 25, Maximum = 100, SmallChange = 1, LargeChange = 25, TickFrequency = 25, Value = 50, Width = 180, Height = 38 };
    private readonly Label _resolutionLabel = new() { AutoSize = true, Padding = new Padding(0, 8, 8, 0) };
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
            Text = "Prototipo de launcher para Windows 11. Puedes seleccionar la carpeta del juego o pasar una ISO directamente a RPCS3 sin copiarla ni extraerla. Esto aún no es un port nativo ni promete más FPS; Vulkan 1.2 se comprueba aparte.",
            AutoSize = true,
            MaximumSize = new Size(820, 0),
            Padding = new Padding(0, 0, 0, 12)
        };
        root.Controls.Add(note, 0, 1);

        var paths = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 4, RowCount = 2, AutoSize = true, Padding = new Padding(0, 0, 0, 8) };
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        paths.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        paths.Controls.Add(new Label { Text = "RPCS3.exe", AutoSize = true, Anchor = AnchorStyles.Left, Padding = new Padding(0, 7, 10, 0) }, 0, 0);
        paths.Controls.Add(_rpcs3Path, 1, 0);
        var selectRpcs3 = new Button { Text = "Buscar…", AutoSize = true };
        selectRpcs3.Click += (_, _) => ChooseRpcs3();
        paths.Controls.Add(selectRpcs3, 2, 0);
        paths.Controls.Add(new Label { Text = "Carpeta / ISO", AutoSize = true, Anchor = AnchorStyles.Left, Padding = new Padding(0, 7, 10, 0) }, 0, 1);
        paths.Controls.Add(_gamePath, 1, 1);
        var selectGame = new Button { Text = "Buscar carpeta…", AutoSize = true };
        selectGame.Click += (_, _) => ChooseGameFolder();
        paths.Controls.Add(selectGame, 2, 1);
        var selectIso = new Button { Text = "Buscar ISO…", AutoSize = true };
        selectIso.Click += (_, _) => ChooseIsoFile();
        paths.Controls.Add(selectIso, 3, 1);
        _rpcs3Path.TextChanged += (_, _) => { SaveSettings(); RefreshReadiness(); };
        _gamePath.TextChanged += (_, _) => { SaveSettings(); RefreshReadiness(); };
        root.Controls.Add(paths, 0, 2);

        var actions = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, WrapContents = true, Padding = new Padding(0, 4, 0, 10) };
        _directBoot.CheckedChanged += (_, _) => SaveSettings();
        actions.Controls.Add(_directBoot);
        actions.Controls.Add(new Label { Text = "Escala interna:", AutoSize = true, Padding = new Padding(10, 8, 0, 0) });
        _resolutionScale.ValueChanged += (_, _) => { UpdateResolutionLabel(); SaveSettings(); };
        actions.Controls.Add(_resolutionScale);
        actions.Controls.Add(_resolutionLabel);
        UpdateResolutionLabel();
        var profile = new Button { Text = "Guardar perfil Vulkan/Vega 8", AutoSize = true, Padding = new Padding(10, 4, 10, 4) };
        profile.Click += (_, _) => SaveGameProfile();
        actions.Controls.Add(profile);
        var launch = new Button { Text = "Iniciar God of War III", AutoSize = true, Padding = new Padding(10, 4, 10, 4) };
        launch.Click += (_, _) => LaunchGame();
        actions.Controls.Add(launch);
        var vulkan = new Button { Text = "Comprobar Vulkan 1.2", AutoSize = true, Padding = new Padding(10, 4, 10, 4) };
        vulkan.Click += async (_, _) => await RunVulkanProbeAsync(vulkan);
        actions.Controls.Add(vulkan);
        var installFirmware = new Button { Text = "Instalar firmware PS3…", AutoSize = true, Padding = new Padding(10, 4, 10, 4) };
        installFirmware.Click += async (_, _) => await InstallFirmwareAsync(installFirmware);
        actions.Controls.Add(installFirmware);
        var firmwareHelp = new LinkLabel { Text = "Firmware oficial Sony", AutoSize = true, Padding = new Padding(8, 8, 0, 0), LinkColor = SystemColors.HotTrack };
        firmwareHelp.Click += (_, _) => Process.Start(new ProcessStartInfo("https://www.playstation.com/es-es/support/hardware/ps3/system-software/") { UseShellExecute = true });
        actions.Controls.Add(firmwareHelp);
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
            _resolutionScale.Value = Math.Clamp(settings.ResolutionScalePercent, _resolutionScale.Minimum, _resolutionScale.Maximum);
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
            var settings = new LauncherSettings { Rpcs3Path = _rpcs3Path.Text.Trim(), GameFolder = _gamePath.Text.Trim(), DirectBoot = _directBoot.Checked, ResolutionScalePercent = _resolutionScale.Value };
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

    private void ChooseIsoFile()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Selecciona una imagen ISO propia y compatible con RPCS3",
            Filter = "Imágenes ISO (*.iso)|*.iso|Todos los archivos (*.*)|*.*",
            CheckFileExists = true
        };
        if (File.Exists(_gamePath.Text) && Path.GetExtension(_gamePath.Text).Equals(".iso", StringComparison.OrdinalIgnoreCase))
            dialog.FileName = _gamePath.Text;
        if (dialog.ShowDialog(this) == DialogResult.OK) _gamePath.Text = dialog.FileName;
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

    private string? GetBootTarget()
    {
        var selected = _gamePath.Text.Trim().Trim('"');
        if (File.Exists(selected) && Path.GetExtension(selected).Equals(".iso", StringComparison.OrdinalIgnoreCase))
            return selected;

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
        var bootTarget = GetBootTarget();
        var titleId = ps3Game is null ? null : ReadTitleId(ps3Game);
        var profileReady = false;
        if (emulatorReady)
        {
            try { profileReady = File.Exists(Rpcs3ProfileWriter.GetProfilePath(_rpcs3Path.Text.Trim().Trim('"'))); }
            catch (ArgumentException) { }
            catch (FileNotFoundException) { }
        }
        var selected = _gamePath.Text.Trim().Trim('"');
        var isIso = bootTarget is not null && File.Exists(selected) && Path.GetExtension(selected).Equals(".iso", StringComparison.OrdinalIgnoreCase);
        var gameStatus = bootTarget is null
            ? "Juego: selecciona PS3_GAME o una ISO"
            : isIso
                ? "ISO seleccionada; RPCS3 comprobará el disco y sus claves"
            : titleId is null
                ? "Juego: falta PARAM.SFO legible"
                : titleId.Equals(ExpectedTitleId, StringComparison.OrdinalIgnoreCase)
                    ? $"God of War III: {titleId}"
                    : $"ID distinta: {titleId} (se espera {ExpectedTitleId})";
        var profileStatus = !emulatorReady ? "" : profileReady ? "  ·  Perfil Vega 8: listo" : "  ·  Perfil Vega 8: se creará al iniciar";
        _status.Text = (emulatorReady ? "RPCS3: encontrado" : "RPCS3: selecciona rpcs3.exe") + "  ·  " + gameStatus + profileStatus;
    }

    private void UpdateResolutionLabel()
    {
        var width = 1280 * _resolutionScale.Value / 100;
        var height = 720 * _resolutionScale.Value / 100;
        _resolutionLabel.Text = $"{_resolutionScale.Value}% ({width}×{height})";
    }

    private void SaveGameProfile()
    {
        var emulator = _rpcs3Path.Text.Trim().Trim('"');
        if (!File.Exists(emulator))
        {
            MessageBox.Show(this, "Selecciona primero un rpcs3.exe válido.", "Falta RPCS3", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        try
        {
            var width = 1280 * _resolutionScale.Value / 100;
            var height = 720 * _resolutionScale.Value / 100;
            var result = Rpcs3ProfileWriter.WriteProfile(emulator, _resolutionScale.Value);
            var profilePath = result.ProfilePath;
            AppendLog($"Perfil {ExpectedTitleId} guardado: Vulkan · Vega 8 · escala {_resolutionScale.Value}% ({width}×{height} aprox.).");
            AppendLog($"Ruta: {profilePath}");
            if (result.BackupPath is not null) AppendLog($"Copia previa: {result.BackupPath}");
            AppendLog("RPCS3 aplica este perfil por Title ID; el rendimiento debe medirse en juego.");
            MessageBox.Show(this, $"Perfil guardado para {ExpectedTitleId}.\n\nVulkan: Radeon Vega 8\nResolución interna aproximada: {width}×{height}\n\nCopia del perfil anterior, si existía, guardada junto al archivo.", "Perfil creado", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"No se pudo guardar el perfil: {ex.Message}", "Error de configuración", MessageBoxButtons.OK, MessageBoxIcon.Error);
            AppendLog($"Error al guardar perfil: {ex}");
        }
    }

    private void LaunchGame()
    {
        var emulator = _rpcs3Path.Text.Trim().Trim('"');
        var bootTarget = GetBootTarget();
        if (!File.Exists(emulator))
        {
            MessageBox.Show(this, "Selecciona un rpcs3.exe válido.", "Falta RPCS3", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        if (bootTarget is null)
        {
            MessageBox.Show(this, "Selecciona una carpeta extraída que contenga PS3_GAME\\USRDIR\\EBOOT.BIN o una imagen ISO.", "Juego no seleccionado", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        var ps3Game = GetPs3GameDirectory();
        var titleId = ps3Game is null ? null : ReadTitleId(ps3Game);
        var selected = _gamePath.Text.Trim().Trim('"');
        var isIso = File.Exists(selected) && Path.GetExtension(selected).Equals(".iso", StringComparison.OrdinalIgnoreCase);
        if (!isIso && !ExpectedTitleId.Equals(titleId, StringComparison.OrdinalIgnoreCase))
        {
            MessageBox.Show(this, $"El PARAM.SFO no identifica esta edición como {ExpectedTitleId}. El launcher evita iniciar un título distinto o una carpeta incompleta.", "Juego no validado", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        try
        {
            var profile = Rpcs3ProfileWriter.WriteProfile(emulator, _resolutionScale.Value);
            AppendLog($"Perfil de rendimiento aplicado a {ExpectedTitleId}: Vulkan · Vega 8 · {_resolutionScale.Value}%.");
            if (profile.BackupPath is not null) AppendLog($"Perfil previo respaldado: {profile.BackupPath}");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, $"No se pudo preparar el perfil de RPCS3: {ex.Message}", "Error de perfil", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        var start = new ProcessStartInfo { FileName = emulator, WorkingDirectory = Path.GetDirectoryName(emulator)!, UseShellExecute = false };
        if (_directBoot.Checked) start.ArgumentList.Add("--no-gui");
        start.ArgumentList.Add(bootTarget);
        try
        {
            var process = Process.Start(start);
            if (process is null) throw new InvalidOperationException("Windows no creó el proceso RPCS3.");
            AppendLog(isIso
                ? $"Iniciado RPCS3 (PID {process.Id}) con la ISO directamente; no se extrae ni copia. El perfil BCES00510 solo se aplicará si RPCS3 identifica esa edición."
                : $"Iniciado RPCS3 (PID {process.Id}) con el EBOOT seleccionado.");
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

    private async Task InstallFirmwareAsync(Button button)
    {
        var emulator = _rpcs3Path.Text.Trim().Trim('"');
        if (!File.Exists(emulator) || !Path.GetFileName(emulator).Equals("rpcs3.exe", StringComparison.OrdinalIgnoreCase))
        {
            MessageBox.Show(this, "Selecciona primero un rpcs3.exe válido.", "Falta RPCS3", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        using var dialog = new OpenFileDialog
        {
            Title = "Selecciona el firmware oficial de PS3 descargado desde Sony",
            Filter = "Actualización del sistema PS3 (PS3UPDAT.PUP)|PS3UPDAT.PUP|Archivos PUP (*.PUP)|*.PUP|Todos los archivos (*.*)|*.*",
            CheckFileExists = true
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;

        var running = Process.GetProcessesByName(Path.GetFileNameWithoutExtension(emulator));
        if (running.Length > 0)
        {
            foreach (var process in running) process.Dispose();
            MessageBox.Show(this, "Cierra primero RPCS3. Luego instala el firmware desde esta ventana.", "RPCS3 está abierto", MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }

        button.Enabled = false;
        try
        {
            var start = new ProcessStartInfo { FileName = emulator, WorkingDirectory = Path.GetDirectoryName(emulator)!, UseShellExecute = false };
            start.ArgumentList.Add("--installfw");
            start.ArgumentList.Add(dialog.FileName);
            AppendLog("Iniciando el instalador oficial de RPCS3 para el firmware seleccionado. El archivo no se copia al repositorio.");
            using var process = Process.Start(start) ?? throw new InvalidOperationException("Windows no creó el proceso RPCS3.");
            await process.WaitForExitAsync();
            AppendLog($"El instalador de firmware RPCS3 terminó con código {process.ExitCode}. Revisa RPCS3.log si el firmware no aparece instalado.");
            if (process.ExitCode == 0)
                MessageBox.Show(this, "RPCS3 terminó la instalación del firmware. Revisa su registro y vuelve a iniciar RPCS3 para comprobarlo.", "Instalación finalizada", MessageBoxButtons.OK, MessageBoxIcon.Information);
            else
                MessageBox.Show(this, $"RPCS3 terminó con código {process.ExitCode}. Revisa RPCS3.log para ver el error.", "No se confirmó la instalación", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
        catch (Exception ex)
        {
            AppendLog($"No se pudo instalar el firmware: {ex}");
            MessageBox.Show(this, $"No se pudo ejecutar el instalador de RPCS3: {ex.Message}", "Error al instalar firmware", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
        finally { button.Enabled = true; }
    }

    private void AppendLog(string message)
    {
        if (InvokeRequired) { BeginInvoke(() => AppendLog(message)); return; }
        _output.AppendText($"[{DateTime.Now:HH:mm:ss}] {message}{Environment.NewLine}");
    }
}
