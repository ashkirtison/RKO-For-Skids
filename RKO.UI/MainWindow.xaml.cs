using CefSharp;
using CefSharp.Wpf;
using Microsoft.Win32;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.IO.Pipes;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace RKO.UI
{
    public class TroubleshootResult
    {
        public string Message { get; set; }
        public Brush StatusColor { get; set; }
        public Brush TextColor { get; set; }
        public bool IsIssue { get; set; }
        public string Category { get; set; }
    }

    public partial class MainWindow : Window
    {

        [DllImport("Module.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        public static extern void CuteLatina(string input);
        private FileSystemWatcher _banWatcherFSW;
        private RobloxBanLogWatcher _banWatcher;

        private FileSystemWatcher _consoleWatcher;
        private System.Timers.Timer _readDebounceTimer;
        private DispatcherTimer _consolePollTimer;
        private long _lastLogPosition = 0;
        private bool _isReadingLog = false;
        private const int MaxLines = 200;
        private const string RobloxLogsDir = @"%LOCALAPPDATA%\Roblox\logs";
        private const string RobloxMismatchSig = "Error Code: 50403 incorrect Roblox version.";
        private volatile bool _autoFixInProgress = false;
        private string _currentConsoleLogPath;
        private static readonly Regex _robloxIsoPrefixRx = new Regex(@"^\d{4}-\d{2}-\d{2}T[0-9:\.\-]+Z,?[^\s]*\s*", RegexOptions.Compiled);
        private static readonly Regex _robloxBracketTimeRx = new Regex(@"^\[\d{2}:\d{2}:\d{2}\]\s*", RegexOptions.Compiled);
        private static readonly Regex _robloxBracketTagRx = new Regex(@"^\[[^\]]+\]\s*", RegexOptions.Compiled);
        private static readonly Regex _robloxCsvPrefixRx = new Regex(@"^\d+(?:\.\d+)?,[0-9A-Fa-f]+,[0-9A-Fa-f]+,\d+\s*", RegexOptions.Compiled);
        private static readonly Regex _robloxChannelPrefixRx = new Regex(@"^(?:D?FLog::[A-Za-z0-9_]+:|INFO|WARN|WARNING|ERROR|DEBUG)\s*[:\-]?\s*", RegexOptions.IgnoreCase | RegexOptions.Compiled);
        private static readonly Regex _robloxPrettySuffixRx = new Regex(@"\s*\{[^}]+\}\s*$", RegexOptions.Compiled);
        private static readonly Regex _robloxWhitespaceRx = new Regex(@"\s+", RegexOptions.Compiled);
        private const int InitialRobloxTailLines = 80;
        private const string DiscordOAuthClientId = "YOUR_DISCORD_CLIENT_ID";
        private const string DiscordOAuthRedirectUri = "http://127.0.0.1:53682/callback/";
        private static readonly string[] DiscordOAuthScopes = new[] { "identify", "email" };
        private HttpListener _discordAuthListener;
        private string _discordAuthCodeVerifier;
        private string _discordAuthState;
        private CancellationTokenSource _discordAuthCts;

        public MainWindow()
        {
            bool createdNew;
            _appMutex = new Mutex(true, "Yubbyb_UI_Mutex", out createdNew);
            if (!createdNew)
            {
                MessageBox.Show("Yubbyb is already running.", "Yubbyb",
                    MessageBoxButton.OK, MessageBoxImage.Warning);
                Application.Current.Shutdown();
                return;
            }

            InitializeComponent();
            LoadConfig();


            try
            {
                string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                string monacoPath = System.IO.Path.Combine(exeDir, "Monaco", "index.html");
                var uri = new Uri(monacoPath).AbsoluteUri;
                Mono.Load(uri);
            }
            catch (Exception ex)
            {
                Log($"Failed to load Monaco: {ex.Message}");
            }

            DataContext = this;
            LoadScripts();

            Log("Welcome to Rankalf");
            SetupRobloxProcessMonitor();

            try
            {
                _banWatcher = new RobloxBanLogWatcher();
                _banWatcher.OnEvent = LogRobloxEvent;
                _banWatcher.Start();
                Log("Roblox log watcher started.");
            }
            catch (Exception ex)
            {
                Log($"Failed to start Roblox log watcher: {ex.Message}");
            }

            try
            {
                setupconsolelogwatcher();
            }
            catch (Exception ex)
            {
                Log($"Failed to setup Roblox console log watcher: {ex.Message}");
            }

            Closing += MainWindow_Closing;

        }

        private void setupconsolelogwatcher()
        {
            var dir = Environment.ExpandEnvironmentVariables(RobloxLogsDir);
            if (!System.IO.Directory.Exists(dir))
            {
                Log("Roblox logs folder not found.");
                return;
            }

            _currentConsoleLogPath = GetLatestRobloxLogFile(dir);
            if (!string.IsNullOrEmpty(_currentConsoleLogPath) && System.IO.File.Exists(_currentConsoleLogPath))
            {
                EmitInitialRobloxTail(_currentConsoleLogPath, InitialRobloxTailLines);
                _lastLogPosition = new System.IO.FileInfo(_currentConsoleLogPath).Length;
                Log($"[roblox] watching {System.IO.Path.GetFileName(_currentConsoleLogPath)}");
            }

            _consoleWatcher = new FileSystemWatcher(dir, "*.log")
            {
                NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size,
                IncludeSubdirectories = false,
                EnableRaisingEvents = true
            };
            _consoleWatcher.Changed += ConsoleLogChanged;
            _consoleWatcher.Created += ConsoleLogCreated;
            _consoleWatcher.Renamed += ConsoleLogCreated;

            _readDebounceTimer = new System.Timers.Timer(200) { AutoReset = false };
            _readDebounceTimer.Elapsed += (s, e) =>
            {
                _ = ReadConsoleLogAsync();
            };

            _consolePollTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1.2) };
            _consolePollTimer.Tick += async (s, e) =>
            {
                try
                {
                    var latest = GetLatestRobloxLogFile(dir);
                    if (!string.IsNullOrEmpty(latest) && !string.Equals(latest, _currentConsoleLogPath, StringComparison.OrdinalIgnoreCase))
                    {
                        _currentConsoleLogPath = latest;
                        _lastLogPosition = 0;
                        Log($"[roblox] switched to {System.IO.Path.GetFileName(latest)}");
                    }
                    await ReadConsoleLogAsync();
                }
                catch
                {
                }
            };
            _consolePollTimer.Start();

            _ = ReadConsoleLogAsync();
        }

        private void ConsoleLogChanged(object sender, FileSystemEventArgs e)
        {
            if (!string.IsNullOrEmpty(e.FullPath))
            {
                if (!string.Equals(e.FullPath, _currentConsoleLogPath, StringComparison.OrdinalIgnoreCase) && System.IO.File.Exists(e.FullPath))
                {
                    _currentConsoleLogPath = e.FullPath;
                    _lastLogPosition = 0;
                }
            }
            _readDebounceTimer.Stop();
            _readDebounceTimer.Start();
        }

        private void ConsoleLogCreated(object sender, FileSystemEventArgs e)
        {
            if (!string.IsNullOrEmpty(e.FullPath))
            {
                if (System.IO.File.Exists(e.FullPath))
                {
                    _currentConsoleLogPath = e.FullPath;
                    _lastLogPosition = 0;
                    Log($"[roblox] switched to {System.IO.Path.GetFileName(e.FullPath)}");
                }
            }
            _readDebounceTimer.Stop();
            _readDebounceTimer.Start();
        }

        private async Task ReadConsoleLogAsync()
        {
            if (_isReadingLog) return;
            _isReadingLog = true;
            try
            {
                var path = _currentConsoleLogPath;
                if (string.IsNullOrEmpty(path) || !System.IO.File.Exists(path))
                {
                    return;
                }
                var newLines = new List<string>();
                long newPos = _lastLogPosition;

                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    if (fs.Length < _lastLogPosition) _lastLogPosition = 0;
                    fs.Seek(_lastLogPosition, SeekOrigin.Begin);

                    using (var reader = new StreamReader(fs, Encoding.UTF8))
                    {
                        string line;
                        while ((line = await reader.ReadLineAsync()) != null)
                        {
                            if (!string.IsNullOrWhiteSpace(line))
                                newLines.Add(line);
                        }
                        newPos = fs.Position;
                    }
                }

                _lastLogPosition = newPos;

                if (newLines.Count > 0)
                {
                    bool trigger = false;
                    await Dispatcher.InvokeAsync(() =>
                    {
                        foreach (var ln in newLines)
                        {
                            LogRobloxLine(ln);
                            if (ln.IndexOf(RobloxMismatchSig, StringComparison.OrdinalIgnoreCase) >= 0)
                                trigger = true;
                        }
                    });

                    if (trigger && !_autoFixInProgress)
                    {
                        _autoFixInProgress = true;
                        Log("[roblox] detected incorrect Roblox version. starting auto-fix");
                        await DownloadRobloxVersion();
                        _autoFixInProgress = false;
                    }
                }
            }
            catch (IOException)
            {
            }
            catch (Exception ex)
            {
                Log($"Roblox console reader error: {ex.Message}");
            }
            finally
            {
                _isReadingLog = false;
            }
        }

        private void LogRobloxEvent(RobloxBanEvent ev)
        {
            if (ev == null) return;

            if (!string.IsNullOrWhiteSpace(ev.RawLine))
            {
                LogRobloxLine(ev.RawLine);
                return;
            }

            var text = _robloxPrettySuffixRx.Replace(ev.Message ?? string.Empty, string.Empty).Trim();
            if (text.IndexOf("Alt guard baseline set", StringComparison.OrdinalIgnoreCase) >= 0) return;
            if (text.IndexOf("Username changed:", StringComparison.OrdinalIgnoreCase) >= 0) return;

            LogRobloxLine(text);
        }

        private void LogRobloxLine(string rawLine)
        {
            var cleaned = NormalizeRobloxLine(rawLine);
            if (string.IsNullOrWhiteSpace(cleaned))
            {
                cleaned = rawLine?.Trim();
            }
            if (string.IsNullOrWhiteSpace(cleaned)) return;
            Log($"[roblox] {cleaned}");
        }

        private static string NormalizeRobloxLine(string line)
        {
            if (string.IsNullOrWhiteSpace(line)) return string.Empty;

            var text = line.Trim();
            text = _robloxIsoPrefixRx.Replace(text, string.Empty);
            text = _robloxCsvPrefixRx.Replace(text, string.Empty);
            text = _robloxBracketTimeRx.Replace(text, string.Empty);
            text = _robloxBracketTagRx.Replace(text, string.Empty);
            text = _robloxChannelPrefixRx.Replace(text, string.Empty);
            text = _robloxPrettySuffixRx.Replace(text, string.Empty);
            text = _robloxWhitespaceRx.Replace(text, " ").Trim();

            if (text.StartsWith("[roblox]", StringComparison.OrdinalIgnoreCase))
            {
                text = text.Substring(8).Trim();
            }

            return text;
        }

        private void EmitInitialRobloxTail(string path, int maxLines)
        {
            try
            {
                if (string.IsNullOrWhiteSpace(path) || !System.IO.File.Exists(path)) return;

                var q = new Queue<string>(Math.Max(1, maxLines));
                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (var reader = new StreamReader(fs, Encoding.UTF8))
                {
                    string line;
                    while ((line = reader.ReadLine()) != null)
                    {
                        if (string.IsNullOrWhiteSpace(line)) continue;
                        if (q.Count >= maxLines) q.Dequeue();
                        q.Enqueue(line);
                    }
                }

                foreach (var line in q)
                {
                    LogRobloxLine(line);
                }
                Log($"[roblox] preloaded {q.Count} lines");
            }
            catch (Exception ex)
            {
                Log($"Roblox initial log preload failed: {ex.Message}");
            }
        }

        private static string GetLatestRobloxLogFile(string dir)
        {
            try
            {
                if (string.IsNullOrEmpty(dir) || !System.IO.Directory.Exists(dir)) return null;
                return Directory.EnumerateFiles(dir, "*.log", SearchOption.TopDirectoryOnly)
                    .Select(p => new FileInfo(p))
                    .OrderByDescending(f => f.LastWriteTimeUtc)
                    .Select(f => f.FullName)
                    .FirstOrDefault();
            }
            catch
            {
                return null;
            }
        }

        private Mutex multiInstanceMutex;
        private static Mutex _appMutex;

        public ObservableCollection<FileItem> FileItems { get; } = new ObservableCollection<FileItem>();
        private List<FileItem> _allFileItems = new List<FileItem>();

        public void Log(string message)
        {
            string time = DateTime.Now.ToString("HH:mm:ss");
            string formatted = $"[{time}] - {message}";

            Dispatcher.Invoke(() =>
            {
                OutputList.Items.Add(formatted);
                if (OutputList.Items.Count > 0)
                {
                    OutputList.ScrollIntoView(OutputList.Items[OutputList.Items.Count - 1]);
                }
            });
        }

        private void close(object sender, RoutedEventArgs e)
        {
            _banWatcher?.Dispose();
            Application.Current.Shutdown();
        }
        private void mini(object sender, RoutedEventArgs e) => this.WindowState = WindowState.Minimized;
        private void max(object sender, RoutedEventArgs e)
        {
            this.WindowState = this.WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
        }

        private void SetStatus(bool active)
        {
            if (active)
            {
                //statusindicator.Fill = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#00FF7F"));
                //statustxt.Text = "Active";
                //statustxt.Foreground = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#00FF7F"));
            }
            else
            {
                // statusindicator.Fill = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#555555"));
                // statustxt.Text = "Inactive";
                // statustxt.Foreground = (Brush)FindResource("TextForegroundSecondary");
            }
        }

        private void Drag(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left) this.DragMove();
        }

        private async void Execute(object sender, MouseButtonEventArgs e)
        {
            try
            {
                var browser = Mono.GetBrowser();
                var response = await browser?.MainFrame?.EvaluateScriptAsync("editor.getValue()");
                if (response != null && response.Success && response.Result != null)
                {
                    CuteLatina(response.Result.ToString());
                }
                else
                {
                    Log("Failed to get editor content.");
                }
            }
            catch (Exception ex)
            {
                Log($"Execution failed: {ex.Message}");
            }
        }

        private void Clear(object sender, MouseButtonEventArgs e)
        {
            try
            {
                Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync("editor.setValue('')");
            }
            catch { }
            Log("Editor cleared");
        }

        private void SaveFile(object sender, MouseButtonEventArgs e)
        {
            ShowDialog(SaveFileDialogue, SaveFilePanel);
            SaveFileNameInput.Focus();
        }

        private async void SaveFile_Save_Click(object sender, RoutedEventArgs e)
        {
            string fileName = SaveFileNameInput.Text.Trim();
            if (string.IsNullOrWhiteSpace(fileName))
            {
                Log("Please enter a file name.");
                return;
            }

            if (!fileName.EndsWith(".txt") && !fileName.EndsWith(".lua") && !fileName.EndsWith(".luau"))
                fileName += ".txt";

            try
            {
                var browser = Mono.GetBrowser();
                var response = await browser?.MainFrame?.EvaluateScriptAsync("editor.getValue()");
                if (response != null && response.Success && response.Result != null)
                {
                    string content = response.Result.ToString();
                    string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                    string scriptsDir = System.IO.Path.Combine(exeDir, "Scripts");
                    if (!Directory.Exists(scriptsDir)) Directory.CreateDirectory(scriptsDir);
                    string filePath = System.IO.Path.Combine(scriptsDir, fileName);
                    File.WriteAllText(filePath, content);

                    Log($"Saved: {fileName}");
                    HideDialog(SaveFileDialogue, SaveFilePanel);
                    SaveFileNameInput.Text = "";
                    LoadScripts();
                }
                else
                {
                    Log("Failed to get editor content.");
                }
            }
            catch (Exception ex)
            {
                Log($"Error saving file: {ex.Message}");
            }
        }

        private void SaveFile_Cancel_Click(object sender, RoutedEventArgs e)
        {
            HideDialog(SaveFileDialogue, SaveFilePanel);
            SaveFileNameInput.Text = "";
        }

        private void OpenFile(object sender, MouseButtonEventArgs e)
        {
            ShowDialog(OpenFileDialogue, OpenFilePanel);
            OpenFileNameInput.Focus();
        }

        private void OpenFile_Browse_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog();
            openFileDialog.Filter = "Text files (*.txt)|*.txt|Lua files (*.lua)|*.lua|Luau files (*.luau)|*.luau|All files (*.*)|*.*";
            if (openFileDialog.ShowDialog() == true)
            {
                OpenFilePathInput.Text = openFileDialog.FileName;
                OpenFileNameInput.Text = System.IO.Path.GetFileName(openFileDialog.FileName);
            }
        }

        private void OpenFile_Confirm_Click(object sender, RoutedEventArgs e)
        {
            string filePath = OpenFilePathInput.Text.Trim();
            string displayName = OpenFileNameInput.Text.Trim();
            if (string.IsNullOrWhiteSpace(filePath) || !File.Exists(filePath))
            {
                Log("Please select a valid file.");
                return;
            }

            try
            {
                LoadFileIntoEditor(filePath, !string.IsNullOrWhiteSpace(displayName) ? displayName : System.IO.Path.GetFileName(filePath));
                Log($"Opened: {(!string.IsNullOrWhiteSpace(displayName) ? displayName : System.IO.Path.GetFileName(filePath))}");
                HideDialog(OpenFileDialogue, OpenFilePanel);
                OpenFilePathInput.Text = "";
                OpenFileNameInput.Text = "";
            }
            catch (Exception ex)
            {
                Log($"Error opening file: {ex.Message}");
            }
        }

        private void OpenFile_Cancel_Click(object sender, RoutedEventArgs e)
        {
            HideDialog(OpenFileDialogue, OpenFilePanel);
            OpenFilePathInput.Text = "";
            OpenFileNameInput.Text = "";
        }

        private void ShowDialog(Border container, Border panel)
        {
            container.Visibility = Visibility.Visible;
            DoubleAnimation opacityAnim = new DoubleAnimation(0, 1, TimeSpan.FromSeconds(0.2));
            container.BeginAnimation(OpacityProperty, opacityAnim);
            var scale = (ScaleTransform)panel.RenderTransform;
            DoubleAnimation scaleAnim = new DoubleAnimation(0.9, 1, TimeSpan.FromSeconds(0.2));
            scaleAnim.EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut };
            scale.BeginAnimation(ScaleTransform.ScaleXProperty, scaleAnim);
            scale.BeginAnimation(ScaleTransform.ScaleYProperty, scaleAnim);
        }

        private void HideDialog(Border container, Border panel)
        {
            DoubleAnimation opacityAnim = new DoubleAnimation(1, 0, TimeSpan.FromSeconds(0.2));
            opacityAnim.Completed += (s, e) => { container.Visibility = Visibility.Collapsed; };
            container.BeginAnimation(OpacityProperty, opacityAnim);
            var scale = (ScaleTransform)panel.RenderTransform;
            DoubleAnimation scaleAnim = new DoubleAnimation(1, 0.9, TimeSpan.FromSeconds(0.2));
            scaleAnim.EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseIn };
            scale.BeginAnimation(ScaleTransform.ScaleXProperty, scaleAnim);
            scale.BeginAnimation(ScaleTransform.ScaleYProperty, scaleAnim);
        }

        private async void Inject(object sender, MouseButtonEventArgs e)
        {
            try
            {
                var pipeName = getrobloxpidstring();
                if (string.IsNullOrEmpty(pipeName))
                {
                    Log("Roblox not detected.");
                    return;
                }

                CuteLatina("");
            }
            catch (Exception ex)
            {
                Log($"Inject check failed: {ex.Message}");
            }
        }

        private void LoadScripts()
        {
            FileItems.Clear();
            _allFileItems.Clear();
            try
            {
                string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                string scriptsDir = System.IO.Path.Combine(exeDir, "Scripts");
                if (!Directory.Exists(scriptsDir))
                {
                    Directory.CreateDirectory(scriptsDir);
                    Log("Scripts folder created. Add .txt/.lua/.luau files to populate the explorer.");
                    return;
                }

                var allowed = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".txt", ".lua", ".luau" };
                var files = Directory.EnumerateFiles(scriptsDir, "*.*", SearchOption.TopDirectoryOnly)
                                     .Where(f => allowed.Contains(System.IO.Path.GetExtension(f)));

                foreach (var file in files)
                {
                    var item = new FileItem
                    {
                        Name = System.IO.Path.GetFileName(file),
                        Path = file,
                        Extension = System.IO.Path.GetExtension(file).TrimStart('.').ToUpperInvariant()
                    };
                    FileItems.Add(item);
                    _allFileItems.Add(item);
                }

                if (FileItems.Count == 0) Log("No scripts found in Scripts folder.");
            }
            catch (Exception ex)
            {
                Log($"Failed to load scripts: {ex.Message}");
            }
        }

        private void FileList_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (!(sender is ListBox list)) return;
            DependencyObject original = (DependencyObject)e.OriginalSource;
            while (original != null && !(original is ListBoxItem)) original = VisualTreeHelper.GetParent(original);
            if (original is ListBoxItem item && item.DataContext is FileItem fileItem)
            {
                LoadFileIntoEditor(fileItem.Path, fileItem.Name);
                FileList.SelectedItem = null;
            }
            else if (list.SelectedItem is FileItem sel)
            {
                LoadFileIntoEditor(sel.Path, sel.Name);
                FileList.SelectedItem = null;
            }
        }

        private void FileList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (sender is ListBox list && list.SelectedItem is FileItem selected)
            {
                LoadFileIntoEditor(selected.Path, selected.Name);
                list.SelectedItem = null;
            }
        }

        private void FileList_ItemRightClick(object sender, MouseButtonEventArgs e)
        {
            if (sender is ListBoxItem item) item.IsSelected = true;
        }

        private void ContextMenu_Closed(object sender, RoutedEventArgs e)
        {
            FileList.UnselectAll();
        }

        private void LoadFileIntoEditor(string path, string displayName)
        {
            try
            {
                if (!File.Exists(path)) { Log($"File not found: {displayName}"); return; }
                var text = File.ReadAllText(path);
                string jsonName = JsonConvert.SerializeObject(displayName);
                string jsonContent = JsonConvert.SerializeObject(text);
                string script = $"window.loadFileIntoTab({jsonName}, {jsonContent});";

                var browser = Mono.GetBrowser();
                if (browser?.MainFrame == null) { Log("Browser not ready yet. Try again in a moment."); return; }
                browser.MainFrame.ExecuteJavaScriptAsync(script);
                Log($"Loaded file: {displayName}");
            }
            catch (Exception ex)
            {
                Log($"Failed to load file: {ex.Message}");
            }
        }

        private void GetKey_Click(object sender, RoutedEventArgs e)
        {
            Log("Key system is disabled.");
        }

        private void PasteKey_Click(object sender, RoutedEventArgs e)
        {
            Log("Key system is disabled.");
        }

        private void ResetKey_Click(object sender, RoutedEventArgs e)
        {
            Log("Key system is disabled.");
        }

        private void SubmitKey_Click(object sender, RoutedEventArgs e)
        {
            Log("Key system is disabled.");
        }

        private void OpenSettings_Click(object sender, MouseButtonEventArgs e)
        {
            Sett.Visibility = Visibility.Visible;
            var storyboard = new Storyboard();
            var fadeAnimation = new DoubleAnimation(0.0, 1.0, TimeSpan.FromSeconds(0.2));
            Storyboard.SetTarget(fadeAnimation, SettingsPanel);
            Storyboard.SetTargetProperty(fadeAnimation, new PropertyPath("Opacity"));

            var scaleXAnimation = new DoubleAnimation(0.9, 1.0, TimeSpan.FromSeconds(0.2)) { EasingFunction = new ExponentialEase { EasingMode = EasingMode.EaseOut } };
            Storyboard.SetTarget(scaleXAnimation, SettingsPanel);
            Storyboard.SetTargetProperty(scaleXAnimation, new PropertyPath("(UIElement.RenderTransform).(ScaleTransform.ScaleX)"));

            var scaleYAnimation = new DoubleAnimation(0.9, 1.0, TimeSpan.FromSeconds(0.2)) { EasingFunction = new ExponentialEase { EasingMode = EasingMode.EaseOut } };
            Storyboard.SetTarget(scaleYAnimation, SettingsPanel);
            Storyboard.SetTargetProperty(scaleYAnimation, new PropertyPath("(UIElement.RenderTransform).(ScaleTransform.ScaleY)"));

            storyboard.Children.Add(fadeAnimation);
            storyboard.Children.Add(scaleXAnimation);
            storyboard.Children.Add(scaleYAnimation);
            storyboard.Begin();
        }

        private void CloseSettings_Click(object sender, MouseButtonEventArgs e)
        {
            var storyboard = new Storyboard();
            var fadeAnimation = new DoubleAnimation(1.0, 0.0, TimeSpan.FromSeconds(0.2));
            Storyboard.SetTarget(fadeAnimation, SettingsPanel);
            Storyboard.SetTargetProperty(fadeAnimation, new PropertyPath("Opacity"));

            var scaleXAnimation = new DoubleAnimation(1.0, 0.9, TimeSpan.FromSeconds(0.2)) { EasingFunction = new ExponentialEase { EasingMode = EasingMode.EaseIn } };
            Storyboard.SetTarget(scaleXAnimation, SettingsPanel);
            Storyboard.SetTargetProperty(scaleXAnimation, new PropertyPath("(UIElement.RenderTransform).(ScaleTransform.ScaleX)"));

            var scaleYAnimation = new DoubleAnimation(1.0, 0.9, TimeSpan.FromSeconds(0.2)) { EasingFunction = new ExponentialEase { EasingMode = EasingMode.EaseIn } };
            Storyboard.SetTarget(scaleYAnimation, SettingsPanel);
            Storyboard.SetTargetProperty(scaleYAnimation, new PropertyPath("(UIElement.RenderTransform).(ScaleTransform.ScaleY)"));

            storyboard.Children.Add(fadeAnimation);
            storyboard.Children.Add(scaleXAnimation);
            storyboard.Children.Add(scaleYAnimation);

            storyboard.Completed += (s, args) => { Sett.Visibility = Visibility.Collapsed; };
            storyboard.Begin();
        }
        [DllImport("user32.dll")]
        private static extern uint SetWindowDisplayAffinity(IntPtr hwnd, uint dwAffinity);

        private const uint WDA_NONE = 0x00000000;
        private const uint WDA_EXCLUDEFROMCAPTURE = 0x00000011;

        private System.Windows.Threading.DispatcherTimer _autoSaveLogTimer;
        private string _sessionLogFile;
        private bool _advancedLogs = false;

        private void HideFromOBS_Checked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.HideFromOBS = true);
            var helper = new System.Windows.Interop.WindowInteropHelper(this);
            SetWindowDisplayAffinity(helper.Handle, WDA_EXCLUDEFROMCAPTURE);
            Log("Hide from OBS Enabled");
        }

        private void HideFromOBS_Unchecked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.HideFromOBS = false);
            var helper = new System.Windows.Interop.WindowInteropHelper(this);
            SetWindowDisplayAffinity(helper.Handle, WDA_NONE);
            Log("Hide from OBS Disabled");
        }

        private void AutoSaveLogs_Checked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.AutoSaveLogs = true);
            if (_autoSaveLogTimer == null)
            {
                _autoSaveLogTimer = new System.Windows.Threading.DispatcherTimer();
                _autoSaveLogTimer.Interval = TimeSpan.FromSeconds(10);
                _autoSaveLogTimer.Tick += (s, args) => SaveLogsToDisk();
            }
            _autoSaveLogTimer.Start();
            Log("Auto Save Logs Enabled");
        }

        private void AutoSaveLogs_Unchecked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.AutoSaveLogs = false);
            _autoSaveLogTimer?.Stop();
            Log("Auto Save Logs Disabled");
        }

        private void AdvancedLogs_Checked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.AdvancedLogs = true);
            _advancedLogs = true;
            Log("Advanced Logs Enabled");
        }

        private void AdvancedLogs_Unchecked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.AdvancedLogs = false);
            _advancedLogs = false;
            Log("Advanced Logs Disabled");
        }

        private void SaveLogsToDisk()
        {
            try
            {
                if (OutputList.Items.Count == 0) return;
                string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                string logsDir = System.IO.Path.Combine(exeDir, "Logs");
                if (!Directory.Exists(logsDir)) Directory.CreateDirectory(logsDir);
                if (string.IsNullOrEmpty(_sessionLogFile))
                {
                    string timestamp = DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss");
                    _sessionLogFile = System.IO.Path.Combine(logsDir, $"log_{timestamp}.txt");
                }
                StringBuilder sb = new StringBuilder();
                foreach (var item in OutputList.Items) sb.AppendLine(item.ToString());
                File.WriteAllText(_sessionLogFile, sb.ToString());
            }
            catch (Exception ex) { System.Diagnostics.Debug.WriteLine($"Auto-save failed: {ex.Message}"); }
        }

        private void ClearOutput_Click(object sender, MouseButtonEventArgs e)
        {
            var fadeOut = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(200));
            fadeOut.Completed += (s, _) =>
            {
                OutputList.Items.Clear();
                var fadeIn = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(200));
                OutputList.BeginAnimation(OpacityProperty, fadeIn);
            };
            OutputList.BeginAnimation(OpacityProperty, fadeOut);
        }

        private void CopyOutput_Click(object sender, MouseButtonEventArgs e)
        {
            if (OutputList.Items.Count == 0) return;
            try
            {
                StringBuilder sb = new StringBuilder();
                foreach (var item in OutputList.Items) sb.AppendLine(item.ToString());
                Clipboard.SetText(sb.ToString());
                Log("Output copied to clipboard");
            }
            catch (Exception ex)
            {
                Log($"Failed to copy output: {ex.Message}");
            }
        }

        private void SaveOutput_Click(object sender, MouseButtonEventArgs e)
        {
            ShowDialog(SaveOutputDialogue, SaveOutputPanel);
            SaveOutputNameInput.Focus();
        }

        private void SaveOutput_Confirm_Click(object sender, RoutedEventArgs e)
        {
            string fileName = SaveOutputNameInput.Text.Trim();
            if (string.IsNullOrWhiteSpace(fileName))
            {
                Log("Please enter a file name.");
                return;
            }

            if (!fileName.EndsWith(".txt")) fileName += ".txt";

            try
            {
                StringBuilder sb = new StringBuilder();
                foreach (var item in OutputList.Items) sb.AppendLine(item.ToString());

                string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                string logsDir = System.IO.Path.Combine(exeDir, "Logs");
                if (!Directory.Exists(logsDir)) Directory.CreateDirectory(logsDir);

                string filePath = System.IO.Path.Combine(logsDir, fileName);
                File.WriteAllText(filePath, sb.ToString());

                Log($"Output saved to {fileName}");
                HideDialog(SaveOutputDialogue, SaveOutputPanel);

                System.Diagnostics.Process.Start("explorer.exe", $"/select,\"{filePath}\"");
            }
            catch (Exception ex)
            {
                Log($"Error saving output: {ex.Message}");
            }
        }

        private void SaveOutput_Cancel_Click(object sender, RoutedEventArgs e)
        {
            HideDialog(SaveOutputDialogue, SaveOutputPanel);
        }

        private void RefreshScripts_Click(object sender, MouseButtonEventArgs e)
        {
            LoadScripts();
            Log("Scripts reloaded.");
        }

        private void FileList_ItemClick(object sender, MouseButtonEventArgs e)
        {
            if (sender is ListBoxItem item && item.DataContext is FileItem fileItem)
            {
                LoadFileIntoEditor(fileItem.Path, fileItem.Name);
                FileList.SelectedItem = null;
            }
        }

        private void LoadScript_Click(object sender, RoutedEventArgs e)
        {
            if (sender is MenuItem menuItem && menuItem.DataContext is FileItem fileItem)
            {
                LoadFileIntoEditor(fileItem.Path, fileItem.Name);
            }
        }

        private void DeleteScript_Click(object sender, RoutedEventArgs e)
        {
            if (sender is MenuItem menuItem && menuItem.DataContext is FileItem fileItem)
            {
                try
                {
                    if (File.Exists(fileItem.Path))
                    {
                        File.Delete(fileItem.Path);
                        Log($"Deleted: {fileItem.Name}");
                        LoadScripts();
                    }
                }
                catch (Exception ex)
                {
                    Log($"Error deleting file: {ex.Message}");
                }
            }
        }

        private void PreviewScript_Click(object sender, RoutedEventArgs e)
        {
            if (sender is MenuItem menuItem && menuItem.DataContext is FileItem fileItem)
            {
                try
                {
                    System.Diagnostics.Process.Start("notepad.exe", fileItem.Path);
                    Log($"Previewing: {fileItem.Name}");
                }
                catch (Exception ex)
                {
                    Log($"Error opening preview: {ex.Message}");
                }
            }
        }

        private Grid _activeSettingsPage;
        private void SettingTab_Checked(object sender, RoutedEventArgs e)
        {
            if (sender is RadioButton rb && rb.Content != null)
            {
                string pageName = rb.Content.ToString();
                Grid targetPage = null;
                switch (pageName)
                {
                    case "General": targetPage = (Grid)FindName("GeneralSettings"); break;
                    case "Interface": targetPage = (Grid)FindName("InterfaceSettings"); break;
                    case "Logging": targetPage = (Grid)FindName("LoggingSettings"); break;
                    case "API": targetPage = (Grid)FindName("ApiSettings"); break;
                    case "Roblox": targetPage = (Grid)FindName("RobloxSettings"); break;
                    case "Quick Options": targetPage = (Grid)FindName("QuickOptionsSettings"); break;
                    case "Help & Support": targetPage = (Grid)FindName("HelpSupportSettings"); break;
                }
                if (targetPage != null)
                {
                    if (_activeSettingsPage == null) _activeSettingsPage = (Grid)FindName("GeneralSettings");
                    if (targetPage != _activeSettingsPage) SwitchSettingsPage(targetPage);
                }
            }
        }

        private void SwitchSettingsPage(Grid newPage)
        {
            var oldPage = _activeSettingsPage;
            if (oldPage != null)
            {
                var outStoryboard = new Storyboard();
                var fadeOut = new DoubleAnimation(1, 0, TimeSpan.FromSeconds(0.2));
                Storyboard.SetTarget(fadeOut, oldPage);
                Storyboard.SetTargetProperty(fadeOut, new PropertyPath("Opacity"));
                var slideUp = new DoubleAnimation(0, -30, TimeSpan.FromSeconds(0.2)) { EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseIn } };
                Storyboard.SetTarget(slideUp, oldPage);
                Storyboard.SetTargetProperty(slideUp, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.Y)"));
                outStoryboard.Children.Add(fadeOut);
                outStoryboard.Children.Add(slideUp);
                outStoryboard.Completed += (s, e) => oldPage.Visibility = Visibility.Collapsed;
                outStoryboard.Begin();
            }
            newPage.Visibility = Visibility.Visible;
            var inStoryboard = new Storyboard();
            var fadeIn = new DoubleAnimation(0, 1, TimeSpan.FromSeconds(0.25)) { BeginTime = TimeSpan.FromSeconds(0.1) };
            Storyboard.SetTarget(fadeIn, newPage);
            Storyboard.SetTargetProperty(fadeIn, new PropertyPath("Opacity"));
            var slideIn = new DoubleAnimation(30, 0, TimeSpan.FromSeconds(0.25)) { BeginTime = TimeSpan.FromSeconds(0.1), EasingFunction = new BackEase { EasingMode = EasingMode.EaseOut, Amplitude = 0.3 } };
            Storyboard.SetTarget(slideIn, newPage);
            Storyboard.SetTargetProperty(slideIn, new PropertyPath("(UIElement.RenderTransform).(TranslateTransform.Y)"));
            inStoryboard.Children.Add(fadeIn);
            inStoryboard.Children.Add(slideIn);
            inStoryboard.Begin();
            _activeSettingsPage = newPage;
        }

        private void SearchInput_GotFocus(object sender, RoutedEventArgs e)
        {
            SearchPlaceholder.Visibility = Visibility.Collapsed;
        }

        private void SearchInput_LostFocus(object sender, RoutedEventArgs e)
        {
            if (string.IsNullOrEmpty(SearchInput.Text))
            {
                SearchPlaceholder.Visibility = Visibility.Visible;
            }
        }

        private void SearchInput_TextChanged(object sender, TextChangedEventArgs e)
        {
            string query = SearchInput.Text.Trim().ToLower();
            if (string.IsNullOrEmpty(query))
            {
                SearchPlaceholder.Visibility = SearchInput.IsFocused ? Visibility.Collapsed : Visibility.Visible;
                if (FileItems.Count != _allFileItems.Count)
                {
                    FileItems.Clear();
                    foreach (var item in _allFileItems) FileItems.Add(item);
                }
            }
            else
            {
                SearchPlaceholder.Visibility = Visibility.Collapsed;
                var filtered = _allFileItems.Where(i => i.Name.ToLower().Contains(query)).ToList();
                FileItems.Clear();
                foreach (var item in filtered) FileItems.Add(item);
            }
        }

        private void TopMost_Checked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.TopMost = true);
            this.Topmost = true;
            Log("Top Most Enabled");
        }

        private void TopMost_Unchecked(object sender, RoutedEventArgs e)
        {
            UpdateConfig(c => c.TopMost = false);
            this.Topmost = false;
            Log("Top Most Disabled");
        }

        private void ClearLogCache_Click(object sender, RoutedEventArgs e)
        {
            ShowDialog(ClearLogDialogue, ClearLogPanel);
        }

        private void OpenLogFolder_Click(object sender, RoutedEventArgs e)
        {
            string exeDir = AppDomain.CurrentDomain.BaseDirectory;
            string logsDir = System.IO.Path.Combine(exeDir, "Logs");
            if (!Directory.Exists(logsDir)) Directory.CreateDirectory(logsDir);
            System.Diagnostics.Process.Start("explorer.exe", logsDir);
        }

        private async void RunTroubleshooter_Click(object sender, RoutedEventArgs e)
        {
            var resultPanel = (Border)FindName("TroubleshootResultPanel");
            var resultsList = (ItemsControl)FindName("TroubleshootResultsList");
            var fixBtn = (Button)FindName("TroubleshootFixBtn");

            if (resultPanel != null) resultPanel.Visibility = Visibility.Visible;
            if (fixBtn != null) fixBtn.Visibility = Visibility.Collapsed;

            var results = new ObservableCollection<TroubleshootResult>();
            if (resultsList != null) resultsList.ItemsSource = results;

            var successColor = new SolidColorBrush(Color.FromRgb(0, 255, 0));
            var warningColor = new SolidColorBrush(Color.FromRgb(255, 165, 0));
            var errorColor = new SolidColorBrush(Color.FromRgb(255, 0, 0));
            var normalTextColor = (Brush)FindResource("TextForeground");
            var secondaryTextColor = (Brush)FindResource("TextForegroundSecondary");

            // 1. Check Monaco
            string monacoPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Monaco", "index.html");
            if (File.Exists(monacoPath))
                results.Add(new TroubleshootResult { Message = "Monaco Editor: Found", StatusColor = successColor, TextColor = normalTextColor });
            else
                results.Add(new TroubleshootResult { Message = "Monaco Editor: Missing (index.html not found)", StatusColor = errorColor, TextColor = errorColor, IsIssue = true, Category = "Monaco" });

            // 2. Check Scripts Folder
            string scriptsPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Scripts");
            if (Directory.Exists(scriptsPath))
                results.Add(new TroubleshootResult { Message = "Scripts Folder: Found", StatusColor = successColor, TextColor = normalTextColor });
            else
                results.Add(new TroubleshootResult { Message = "Scripts Folder: Missing", StatusColor = warningColor, TextColor = warningColor, IsIssue = true, Category = "Scripts" });

            // 3. Check Workspace Folder
            string workspacePath = Environment.ExpandEnvironmentVariables(@"%LOCALAPPDATA%\Rankalf\workspace");
            if (Directory.Exists(workspacePath))
                results.Add(new TroubleshootResult { Message = "Workspace: Found", StatusColor = successColor, TextColor = normalTextColor });
            else
                results.Add(new TroubleshootResult { Message = "Workspace: Missing (will be created on use)", StatusColor = warningColor, TextColor = secondaryTextColor });

            // 4. Check Internet Connection
            try
            {
                using (var client = new HttpClient())
                {
                    client.Timeout = TimeSpan.FromSeconds(5);
                    var response = await client.GetAsync("https://www.google.com");
                    if (response.IsSuccessStatusCode)
                        results.Add(new TroubleshootResult { Message = "Internet Connection: Connected", StatusColor = successColor, TextColor = normalTextColor });
                    else
                        results.Add(new TroubleshootResult { Message = "Internet Connection: Limited access", StatusColor = warningColor, TextColor = warningColor });
                }
            }
            catch
            {
                results.Add(new TroubleshootResult { Message = "Internet Connection: No access", StatusColor = errorColor, TextColor = errorColor, IsIssue = true });
            }

            // 5. Check Roblox
            bool robloxInstalled = false;
            try
            {
                using (var key = Registry.CurrentUser.OpenSubKey(@"Software\Roblox\RobloxPlayer"))
                {
                    if (key != null) robloxInstalled = true;
                }
            }
            catch { }

            if (robloxInstalled)
                results.Add(new TroubleshootResult { Message = "Roblox: Installed", StatusColor = successColor, TextColor = normalTextColor });
            else
                results.Add(new TroubleshootResult { Message = "Roblox: Not detected in registry", StatusColor = warningColor, TextColor = warningColor });

            // 6. Check Core DLL
            string coreDllPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Rankalf.Core.dll");
            if (File.Exists(coreDllPath))
                results.Add(new TroubleshootResult { Message = "Core Library: Found", StatusColor = successColor, TextColor = normalTextColor });
            else
                results.Add(new TroubleshootResult { Message = "Core Library: Missing (Injection will fail)", StatusColor = errorColor, TextColor = errorColor, IsIssue = true, Category = "Core" });

            // 7. Check API Connectivity
            try
            {
                using (var client = new HttpClient())
                {
                    client.Timeout = TimeSpan.FromSeconds(5);
                    var response = await client.GetAsync("https://Rankalf.com/api.json");
                    if (response.IsSuccessStatusCode)
                        results.Add(new TroubleshootResult { Message = "API Endpoint: Reachable", StatusColor = successColor, TextColor = normalTextColor });
                    else
                        results.Add(new TroubleshootResult { Message = $"API Endpoint: Returned {response.StatusCode}", StatusColor = warningColor, TextColor = warningColor, IsIssue = true, Category = "API" });
                }
            }
            catch (Exception ex)
            {
                results.Add(new TroubleshootResult { Message = "API Endpoint: Unreachable (Connection error)", StatusColor = errorColor, TextColor = errorColor, IsIssue = true, Category = "API" });
            }

            // Show fix button if there are issues
            if (results.Any(r => r.IsIssue) && fixBtn != null)
            {
                fixBtn.Visibility = Visibility.Visible;
            }
        }

        private void FixTroubleshootIssues_Click(object sender, RoutedEventArgs e)
        {
            var resultsList = (ItemsControl)FindName("TroubleshootResultsList");
            var results = resultsList?.ItemsSource as ObservableCollection<TroubleshootResult>;
            if (results == null) return;

            bool fixedAny = false;

            foreach (var issue in results.Where(r => r.IsIssue))
            {
                switch (issue.Category)
                {
                    case "Scripts":
                        string scriptsPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Scripts");
                        if (!Directory.Exists(scriptsPath))
                        {
                            Directory.CreateDirectory(scriptsPath);
                            File.WriteAllText(System.IO.Path.Combine(scriptsPath, "Welcome.txt"), "-- Welcome to Rankalf!");
                            fixedAny = true;
                        }
                        break;
                    case "Monaco":
                        // Cannot easily fix Monaco without re-downloading, maybe suggest re-install
                        Log("Monaco is missing. Please re-install the application or check your antivirus.");
                        break;
                    case "API":
                        // Suggest manual check for API issues
                        Log("API issues detected. Please check your firewall or proxy settings.");
                        break;
                }
            }

            if (fixedAny)
            {
                Log("Troubleshooter: Some issues were fixed automatically.");
                RunTroubleshooter_Click(null, null); // Re-run to update status
            }
            else
            {
                Log("Troubleshooter: No automatic fixes available for remaining issues.");
                MessageBox.Show("Some issues require manual intervention. Please check the logs or join our Discord for help.", "Troubleshooter", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void JoinDiscord_Click(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Process.Start("https://discord.gg/Rankalf");
        }

        private void VisitWebsite_Click(object sender, RoutedEventArgs e)
        {
            System.Diagnostics.Process.Start("https://Rankalf.com");
        }

        private void ClearLog_Confirm_Click(object sender, RoutedEventArgs e)
        {
            int count = 0;
            string exeDir = AppDomain.CurrentDomain.BaseDirectory;
            string logsDir = System.IO.Path.Combine(exeDir, "Logs");

            if (Directory.Exists(logsDir))
            {
                if (ClearOldLogsCheck.IsChecked == true)
                {
                    var files = Directory.GetFiles(logsDir, "*.txt");
                    foreach (var file in files)
                    {
                        if (file != _sessionLogFile)
                        {
                            try { File.Delete(file); count++; } catch { }
                        }
                    }
                }

                if (ClearCurrentLogCheck.IsChecked == true)
                {
                    OutputList.Items.Clear();
                    if (!string.IsNullOrEmpty(_sessionLogFile) && File.Exists(_sessionLogFile))
                    {
                        try { File.WriteAllText(_sessionLogFile, ""); count++; } catch { }
                    }
                }
            }

            Log($"Cleared {count} log files/entries.");
            HideDialog(ClearLogDialogue, ClearLogPanel);
        }

        private void ClearLog_Cancel_Click(object sender, RoutedEventArgs e)
        {
            HideDialog(ClearLogDialogue, ClearLogPanel);
        }

        private static async Task RespondToBrowserAsync(HttpListenerContext context)
        {
            string responseHtml = "<html><body>Sign-in complete. You can close this window.</body></html>";
            byte[] buffer = Encoding.UTF8.GetBytes(responseHtml);
            context.Response.ContentType = "text/html";
            context.Response.ContentLength64 = buffer.Length;
            await context.Response.OutputStream.WriteAsync(buffer, 0, buffer.Length);
            context.Response.OutputStream.Close();
        }

        private static JObject ExchangeError(string body)
        {
            if (string.IsNullOrWhiteSpace(body))
            {
                return new JObject();
            }
            try
            {
                return JObject.Parse(body);
            }
            catch
            {
                return new JObject();
            }
        }

        private async Task<JObject> ExchangeCodeForTokenAsync(string code, string redirectUri, string codeVerifier)
        {
            using (var http = new HttpClient())
            {
                var values = new Dictionary<string, string>
                {
                    { "client_id", DiscordOAuthClientId },
                    { "grant_type", "authorization_code" },
                    { "code", code },
                    { "redirect_uri", redirectUri },
                    { "code_verifier", codeVerifier }
                };

                var response = await http.PostAsync("https://discord.com/api/oauth2/token", new FormUrlEncodedContent(values));
                string body = await response.Content.ReadAsStringAsync();
                if (!response.IsSuccessStatusCode)
                {
                    var error = ExchangeError(body);
                    string message = error.Value<string>("error_description") ?? body;
                    throw new Exception(message);
                }
                return JObject.Parse(body);
            }
        }

        private async Task<JObject> FetchDiscordUserAsync(string accessToken)
        {
            using (var http = new HttpClient())
            {
                http.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", accessToken);
                var response = await http.GetAsync("https://discord.com/api/users/@me");
                string body = await response.Content.ReadAsStringAsync();
                if (!response.IsSuccessStatusCode)
                {
                    throw new Exception(body);
                }
                return JObject.Parse(body);
            }
        }

        private static string BuildDiscordAuthorizeUrl(string codeChallenge, string state, string redirectUri)
        {
            string scopes = string.Join("%20", DiscordOAuthScopes);
            string redirect = WebUtility.UrlEncode(redirectUri);
            return $"https://discord.com/oauth2/authorize?response_type=code&client_id={DiscordOAuthClientId}&scope={scopes}&redirect_uri={redirect}&code_challenge={codeChallenge}&code_challenge_method=S256&state={state}&prompt=consent";
        }

        private static string CreateCodeVerifier()
        {
            byte[] bytes = new byte[32];
            using (var rng = RandomNumberGenerator.Create())
            {
                rng.GetBytes(bytes);
            }
            return Base64UrlEncode(bytes);
        }

        private static string CreateCodeChallenge(string verifier)
        {
            using (var sha = SHA256.Create())
            {
                byte[] bytes = sha.ComputeHash(Encoding.ASCII.GetBytes(verifier));
                return Base64UrlEncode(bytes);
            }
        }

        private static string CreateStateToken()
        {
            byte[] bytes = new byte[16];
            using (var rng = RandomNumberGenerator.Create())
            {
                rng.GetBytes(bytes);
            }
            return Base64UrlEncode(bytes);
        }

        private static string Base64UrlEncode(byte[] input)
        {
            return Convert.ToBase64String(input).TrimEnd('=').Replace('+', '-').Replace('/', '_');
        }

        private static Dictionary<string, string> ParseQuery(string query)
        {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (string.IsNullOrWhiteSpace(query))
            {
                return result;
            }
            if (query.StartsWith("?"))
            {
                query = query.Substring(1);
            }
            foreach (var part in query.Split('&'))
            {
                if (string.IsNullOrWhiteSpace(part))
                {
                    continue;
                }
                var kv = part.Split(new[] { '=' }, 2);
                var key = WebUtility.UrlDecode(kv[0]);
                var value = kv.Length > 1 ? WebUtility.UrlDecode(kv[1]) : "";
                result[key] = value;
            }
            return result;
        }

        private void StopDiscordAuthListener()
        {
            if (_discordAuthListener == null)
            {
                return;
            }
            try
            {
                _discordAuthListener.Stop();
            }
            catch { }
            try
            {
                _discordAuthListener.Close();
            }
            catch { }
            _discordAuthListener = null;
        }
        private DispatcherTimer processPollTimer;
        private DispatcherTimer _tmr;
        private List<int> currentRobloxPIDs = new List<int>();

        private void SetupRobloxProcessMonitor()
        {
            processPollTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromSeconds(1)
            };
            processPollTimer.Tick += (s, e) => RefreshRobloxProcesses();
            processPollTimer.Start();

            _tmr = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
            _tmr.Tick += tmr_tck;
            _tmr.Start();

            RefreshRobloxProcesses();
        }

        private void RefreshRobloxProcesses()
        {
            var processes = Process.GetProcessesByName("RobloxPlayerBeta");
            var newPIDs = processes.Select(p => p.Id).ToList();

            if (newPIDs.SequenceEqual(currentRobloxPIDs))
                return;

            if (newPIDs.Count == 0 && currentRobloxPIDs.Count > 0)
            {
                KillInjector_Click(null, null);
                SetStatus(false);
            }

            currentRobloxPIDs = newPIDs;

            var updatedList = new List<RobloxProcessEntry>();
            foreach (var proc in processes)
            {
                updatedList.Add(new RobloxProcessEntry
                {
                    DisplayName = $"PID: {proc.Id} - RobloxPlayerBeta.exe",
                    StatusColor = Brushes.LimeGreen,
                    PID = proc.Id,
                    CloseVisible = Visibility.Visible
                });
            }

            if (updatedList.Count == 0)
            {
                updatedList.Add(new RobloxProcessEntry
                {
                    DisplayName = "No Roblox instances",
                    StatusColor = Brushes.Red,
                    PID = -1,
                    CloseVisible = Visibility.Collapsed
                });
            }

            var previousSelected = robloxProcessDropdown.SelectedItem as RobloxProcessEntry;

            robloxProcessDropdown.ItemsSource = updatedList;

            if (previousSelected != null && updatedList.Any(p => p.PID == previousSelected.PID))
            {
                robloxProcessDropdown.SelectedItem = updatedList.First(p => p.PID == previousSelected.PID);
            }
            else if (updatedList.Count == 1)
            {
                robloxProcessDropdown.SelectedIndex = 0;
            }
            else
            {
                robloxProcessDropdown.SelectedIndex = -1;
            }
        }

        private void robloxProcessDropdown_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var selected = robloxProcessDropdown.SelectedItem as RobloxProcessEntry;
            if (selected != null && selected.PID > 0)
            {
                _currentPipeName = $"Rankalf_{selected.PID}";
                pipemonitor.Stop();
                _hasLoggedReady = false;
                _hasLoggedPipeOnline = false;
                startpipestatusmonitor();
                Log($"Target switched to PID {selected.PID}");
            }
        }

        private void CloseProcess_Click(object sender, RoutedEventArgs e)
        {
            if (sender is FrameworkElement element && element.DataContext is RobloxProcessEntry entry)
            {
                if (entry.PID <= 0) return;

                try
                {
                    var process = Process.GetProcessById(entry.PID);
                    process.Kill();
                    Log($"Terminated process: {entry.DisplayName}");
                    RefreshRobloxProcesses();
                }
                catch (Exception ex)
                {
                    Log($"Failed to terminate process: {ex.Message}");
                }
            }
        }

        private async void KillInjector_Click(object sender, RoutedEventArgs e)
        {
            await KillIfRunning("injector", "Injector");
            Log("Injector process termination requested.");
        }

        private async void KillRoblox_Click(object sender, RoutedEventArgs e)
        {
            await KillIfRunning("RobloxPlayerBeta", "Roblox");
            Log("Roblox process termination requested.");
        }

        private void RestartApp_Click(object sender, RoutedEventArgs e)
        {
            var exe = System.Diagnostics.Process.GetCurrentProcess().MainModule.FileName;
            System.Diagnostics.Process.Start(exe);
            Application.Current.Shutdown();
        }

        public class NamedPipeMonitor
        {
            private CancellationTokenSource cnclsrc = null;

            public void Start(string pipename, int intervalms, Action<bool> onstatuschange)
            {
                Stop();
                cnclsrc = new CancellationTokenSource();
                bool? laststatus = null;
                Task.Run(async () =>
                {
                    while (!cnclsrc.IsCancellationRequested)
                    {
                        bool exists = await namedpipeexist(pipename, 250);
                        if (laststatus == null || laststatus.Value != exists)
                        {
                            laststatus = exists;
                            onstatuschange?.Invoke(exists);
                        }
                        await Task.Delay(intervalms, cnclsrc.Token);
                    }
                }, cnclsrc.Token);
            }

            public void Stop()
            {
                cnclsrc?.Cancel();
                cnclsrc = null;
            }

            public static async Task<bool> namedpipeexist(string pipename, int timeoutms = 500)
            {
                try
                {
                    using (var pipe = new NamedPipeClientStream(".", pipename, PipeDirection.Out))
                    {
                        var connecttask = pipe.ConnectAsync(timeoutms);
                        await connecttask;
                        return pipe.IsConnected;
                    }
                }
                catch
                {
                    return false;
                }
            }
        }
        public string getrobloxpidstring()
        {
            var selected = robloxProcessDropdown.SelectedItem as RobloxProcessEntry;

            if (selected != null && selected.PID > 0)
                return $"Rankalf_{selected.PID}";

            var robloxProcesses = Process.GetProcessesByName("RobloxPlayerBeta");
            if (robloxProcesses.Length > 0)
                return $"Rankalf_{robloxProcesses[0].Id}";

            return null;
        }
        private Point _strtdrgpnt;
        private CancellationTokenSource _ppchckcts = null;
        private NamedPipeMonitor pipemonitor = new NamedPipeMonitor();
        private string crrntchckdpp = null;
        private string _currentPipeName = null;
        private bool _hasLoggedReady = false;
        private bool _hasLoggedPipeOnline = false;

        private void tmr_tck(object sender, EventArgs e)
        {
            string key;
            try
            {
                key = getrobloxpidstring();
            }
            catch (Exception ex)
            {
                key = null;
            }

            if (key == _currentPipeName)
                return;

            pipemonitor.Stop();
            _currentPipeName = key;
            _hasLoggedReady = false;
            _hasLoggedPipeOnline = false;

            if (string.IsNullOrEmpty(key))
            {
                SetStatus(false);
                Log("[Internal] Roblox not detected");

            }
            else
            {
                startpipestatusmonitor();
            }
        }


        private void startpipestatusmonitor()
        {
            try
            {
                pipemonitor.Start(_currentPipeName, 1000, onpipestatuschanged);

                if (!_hasLoggedReady)
                {
                    Log($"[Internal] Roblox detected, monitoring pipe '{_currentPipeName}'");
                    _hasLoggedReady = true;
                }
            }
            catch
            {
                SetStatus(false);
            }
        }

        private void onpipestatuschanged(bool online)
        {
            if (!Dispatcher.CheckAccess())
            {
                Dispatcher.Invoke(() => onpipestatuschanged(online));
                return;
            }

            SetStatus(online);

            if (online)
            {
                if (!_hasLoggedPipeOnline)
                {
                    Log("[Internal] Pipe is online");
                    _hasLoggedPipeOnline = true;
                }
            }
            else
            {
                _hasLoggedPipeOnline = false;
            }
        }

        private void SetBackgroundColor_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                var colorCode = BgColorInput.Text.Trim();
                if (!colorCode.StartsWith("#")) colorCode = "#" + colorCode;

                var color = (Color)ColorConverter.ConvertFromString(colorCode);
                var brush = new SolidColorBrush(color);

                MainBorder.Background = brush;

                ApplyTextContrastTheme(IsLightColor(color));

                var sidebarColor = ChangeColorBrightness(color, 1.2f);
                SidebarPanel.Background = new SolidColorBrush(sidebarColor);
                SettingsPanel.Background = new SolidColorBrush(sidebarColor);

                var outputColor = ChangeColorBrightness(color, 1.1f);
                OutputInnerPanel.Background = new SolidColorBrush(outputColor);

                var accentColor = ChangeColorBrightness(color, 1.3f);
                SearchBorder.Background = new SolidColorBrush(accentColor);
                SettingsSidebar.Background = new SolidColorBrush(accentColor);
                RefreshButtonBorder.Background = new SolidColorBrush(accentColor);
                robloxProcessDropdown.Background = new SolidColorBrush(accentColor);

                var toolbarColor = ChangeColorBrightness(color, 1.15f);
                ToolbarBorder.Background = new SolidColorBrush(toolbarColor);

                this.Resources["SelectedItemBackground"] = new SolidColorBrush(accentColor);

                string tabColorHex = "#" + accentColor.R.ToString("X2") + accentColor.G.ToString("X2") + accentColor.B.ToString("X2");
                Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync($"if(window.setTabTheme) window.setTabTheme('{tabColorHex}');");

                Log($"Background color set to {colorCode}");
                UpdateConfig(c => c.BackgroundColor = colorCode);
            }
            catch
            {
                Log("Invalid color code. Use hex format (e.g. #111111)");
            }
        }

        private Color ChangeColorBrightness(Color color, float factor)
        {
            float r = (float)color.R * factor;
            float g = (float)color.G * factor;
            float b = (float)color.B * factor;

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            return Color.FromRgb((byte)r, (byte)g, (byte)b);
        }

        private void BrowseBgImage_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "Image files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg|All files (*.*)|*.*"
            };

            if (openFileDialog.ShowDialog() == true)
            {
                try
                {
                    var imageBrush = new ImageBrush();
                    imageBrush.ImageSource = new BitmapImage(new Uri(openFileDialog.FileName));
                    imageBrush.Stretch = Stretch.UniformToFill;
                    MainBorder.Background = imageBrush;

                    SidebarPanel.Background = Brushes.Transparent;
                    OutputInnerPanel.Background = Brushes.Transparent;
                    ToolbarBorder.Background = Brushes.Transparent;
                    SearchBorder.Background = Brushes.Transparent;
                    RefreshButtonBorder.Background = Brushes.Transparent;
                    robloxProcessDropdown.Background = Brushes.Transparent;
                    FileList.Background = Brushes.Transparent;

                    this.Resources["SelectedItemBackground"] = new SolidColorBrush(Color.FromArgb(128, 30, 30, 30));
                    this.Resources["HoverBackground"] = Brushes.Transparent;

                    Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync($"if(window.setTabTheme) window.setTabTheme('rgba(30, 30, 30, 0.5)');");
                    ApplyTextContrastTheme(IsLightImageFile(openFileDialog.FileName));

                    Log("Background image set.");
                    UpdateConfig(c => c.BackgroundImage = openFileDialog.FileName);
                }
                catch (Exception ex)
                {
                    Log($"Error setting background image: {ex.Message}");
                }
            }
        }

        private void ResetBgImage_Click(object sender, RoutedEventArgs e)
        {
            MainBorder.Background = (SolidColorBrush)FindResource("Background");

            SidebarPanel.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#0c0c0c"));
            SettingsPanel.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#0c0c0c"));
            OutputInnerPanel.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#0a0a0a"));
            SearchBorder.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#111111"));
            SettingsSidebar.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#111111"));
            RefreshButtonBorder.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#111111"));
            robloxProcessDropdown.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#111111"));
            ToolbarBorder.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#0e0e0e"));

            this.Resources["SelectedItemBackground"] = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#1A1A1A"));
            this.Resources["HoverBackground"] = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#1A1A1A"));
            ApplyTextContrastTheme(false);

            Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync($"if(window.setTabTheme) window.setTabTheme('#1a1a1a');");

            BgColorInput.Text = "#080808";
            UpdateConfig(c =>
            {
                c.BackgroundColor = "#080808";
                c.BackgroundImage = null;
            });
            Log("Background reset to default.");
        }

        public class RobloxProcessEntry
        {
            public string DisplayName { get; set; }
            public Brush StatusColor { get; set; }
            public int PID { get; set; }
            public Visibility CloseVisible { get; set; }
        }
        private AppConfig _config = new AppConfig();
        private bool _isRestoringSettings = false;
        private const string ConfigFile = "config.json";

        private void UpdateConfig(Action<AppConfig> updateAction)
        {
            updateAction(_config);
            if (!_isRestoringSettings && _config.SaveSettings)
            {
                SaveConfigInternal();
            }
        }

        private void SaveConfigInternal()
        {
            try
            {
                string json = JsonConvert.SerializeObject(_config, Formatting.Indented);
                File.WriteAllText(ConfigFile, json);
            }
            catch (Exception ex)
            {
                Log($"Failed to save config: {ex.Message}");
            }
        }

        private void SaveSettingConfigs_Checked(object sender, RoutedEventArgs e)
        {
            if (_isRestoringSettings) return;
            _config.SaveSettings = true;
            SaveConfigInternal();
            Log("Settings will be saved automatically.");
        }

        private void SaveSettingConfigs_Unchecked(object sender, RoutedEventArgs e)
        {
            if (_isRestoringSettings) return;
            _config.SaveSettings = false;
            SaveConfigInternal();
            Log("Settings will no longer be saved.");
        }

        private void AutoInject_Checked(object sender, RoutedEventArgs e) => UpdateConfig(c => c.AutoInject = true);
        private void AutoInject_Unchecked(object sender, RoutedEventArgs e) => UpdateConfig(c => c.AutoInject = false);

        private void multiInstance_checked(object sender, RoutedEventArgs e)
        {
            try
            {
                multiInstanceMutex = new Mutex(true, "ROBLOX_singletonEvent");
                addconsoleoutput("Multi instance enabled", cnslmsgtyp.success);
                UpdateConfig(c => c.MultiInstance = true);
            }
            catch (Exception ex)
            {
                addconsoleoutput("Failed to create mutex: " + ex.Message, cnslmsgtyp.error);
            }
        }

        private void multiInstance_unchecked(object sender, RoutedEventArgs e)
        {
            try
            {
                if (multiInstanceMutex != null)
                {
                    multiInstanceMutex.Close();
                    multiInstanceMutex = null;
                    addconsoleoutput("Multi instance disabled", cnslmsgtyp.warning);
                    UpdateConfig(c => c.MultiInstance = false);
                }
            }
            catch (Exception ex)
            {
                addconsoleoutput("Failed to release mutex: " + ex.Message, cnslmsgtyp.error);
            }
        }

        private void SaveScripts_Checked(object sender, RoutedEventArgs e) => UpdateConfig(c => c.SaveScripts = true);
        private void SaveScripts_Unchecked(object sender, RoutedEventArgs e) => UpdateConfig(c => c.SaveScripts = false);

        private void LoadConfig()
        {
            try
            {
                if (!File.Exists(ConfigFile)) return;

                string json = File.ReadAllText(ConfigFile);
                var loadedConfig = JsonConvert.DeserializeObject<AppConfig>(json);

                if (loadedConfig == null) return;

                _config = loadedConfig;
                _config.SaveSettings = false;
                _isRestoringSettings = true;

                SaveSettingsToggle.IsChecked = _config.SaveSettings;

                AutoInjectToggle.IsChecked = _config.AutoInject;
                SaveScriptsToggle.IsChecked = _config.SaveScripts;
                MultiInstanceToggle.IsChecked = _config.MultiInstance;

                TopMostToggle.IsChecked = _config.TopMost;

                HideFromOBSToggle.IsChecked = _config.HideFromOBS;

                if (!string.IsNullOrEmpty(_config.BackgroundImage) && File.Exists(_config.BackgroundImage))
                {
                    try
                    {
                        var imageBrush = new ImageBrush();
                        imageBrush.ImageSource = new BitmapImage(new Uri(_config.BackgroundImage));
                        imageBrush.Stretch = Stretch.UniformToFill;
                        MainBorder.Background = imageBrush;

                        SidebarPanel.Background = Brushes.Transparent;
                        OutputInnerPanel.Background = Brushes.Transparent;
                        ToolbarBorder.Background = Brushes.Transparent;
                        SearchBorder.Background = Brushes.Transparent;
                        RefreshButtonBorder.Background = Brushes.Transparent;
                        robloxProcessDropdown.Background = Brushes.Transparent;
                        FileList.Background = Brushes.Transparent;

                    this.Resources["SelectedItemBackground"] = new SolidColorBrush(Color.FromArgb(128, 30, 30, 30));
                    this.Resources["HoverBackground"] = Brushes.Transparent;

                    Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync($"if(window.setTabTheme) window.setTabTheme('rgba(30, 30, 30, 0.5)');");
                    ApplyTextContrastTheme(IsLightImageFile(_config.BackgroundImage));
                }
                catch (Exception ex)
                {
                    Log($"Failed to restore background image: {ex.Message}");
                }
                }
                else
                {
                    BgColorInput.Text = _config.BackgroundColor;
                    SetBackgroundColor_Click(null, null);
                }

                AutoSaveLogsToggle.IsChecked = _config.AutoSaveLogs;
                AdvancedLogsToggle.IsChecked = _config.AdvancedLogs;

                Log("Configuration loaded.");
            }
            catch (Exception ex)
            {
                Log($"Failed to load config: {ex.Message}");
            }
            finally
            {
                _isRestoringSettings = false;
            }
        }

        private void ApplyTextContrastTheme(bool lightBackground)
        {
            if (lightBackground)
            {
                this.Resources["TextForeground"] = new SolidColorBrush(Colors.Black);
                this.Resources["TextForegroundSecondary"] = new SolidColorBrush(Color.FromRgb(45, 45, 45));
                this.Resources["OutputLogForeground"] = new SolidColorBrush(Colors.Black);
                Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync("if(window.monaco && window.editor){monaco.editor.setTheme('vs');}");
                return;
            }

            this.Resources["TextForeground"] = new SolidColorBrush(Colors.White);
            this.Resources["TextForegroundSecondary"] = new SolidColorBrush(Color.FromRgb(161, 161, 170));
            this.Resources["OutputLogForeground"] = new SolidColorBrush(Color.FromRgb(204, 204, 204));
            Mono.GetBrowser()?.MainFrame?.ExecuteJavaScriptAsync("if(window.monaco && window.editor){monaco.editor.setTheme('vs-dark');}");
        }

        private static bool IsLightColor(Color color)
        {
            double luminance = (0.299 * color.R + 0.587 * color.G + 0.114 * color.B) / 255.0;
            return luminance >= 0.55;
        }

        private static bool IsLightImageFile(string imagePath)
        {
            try
            {
                if (string.IsNullOrWhiteSpace(imagePath) || !File.Exists(imagePath)) return false;

                var bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.DecodePixelWidth = 96;
                bmp.UriSource = new Uri(imagePath, UriKind.Absolute);
                bmp.EndInit();
                bmp.Freeze();

                var formatted = new FormatConvertedBitmap(bmp, PixelFormats.Bgra32, null, 0);
                int stride = formatted.PixelWidth * 4;
                byte[] pixels = new byte[stride * formatted.PixelHeight];
                formatted.CopyPixels(pixels, stride, 0);

                long sampleCount = 0;
                double lumTotal = 0;
                for (int i = 0; i + 3 < pixels.Length; i += 4)
                {
                    byte b = pixels[i];
                    byte g = pixels[i + 1];
                    byte r = pixels[i + 2];
                    lumTotal += (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
                    sampleCount++;
                }

                if (sampleCount == 0) return false;
                return (lumTotal / sampleCount) >= 0.55;
            }
            catch
            {
                return false;
            }
        }

        private void MainWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (_config.SaveSettings)
            {
                SaveConfigInternal();
            }
            _consolePollTimer?.Stop();
            _consolePollTimer = null;
            _readDebounceTimer?.Stop();
            _readDebounceTimer?.Dispose();
            _readDebounceTimer = null;
            _consoleWatcher?.Dispose();
            _consoleWatcher = null;
            _discordAuthCts?.Cancel();
            StopDiscordAuthListener();
        }

        public class FileItem { public string Name { get; set; } = ""; public string Path { get; set; } = ""; public string Extension { get; set; } = ""; }
        public class AppConfig
        {
            public string AccentColor { get; set; } = "#1db954";
            public bool SaveSettings { get; set; } = false;
            public bool AutoInject { get; set; } = false;
            public bool SaveScripts { get; set; } = true;
            public bool AutoExecute { get; set; } = false;
            public bool TopMost { get; set; } = false;
            public bool HideFromOBS { get; set; } = false;
            public string BackgroundColor { get; set; } = "#080808";
            public string BackgroundImage { get; set; } = null;
            public bool AutoSaveLogs { get; set; } = false;
            public bool AdvancedLogs { get; set; } = false;
            public bool MultiInstance { get; set; } = false;
            public string DiscordUserId { get; set; } = "";
            public bool DiscordUserIdPrompted { get; set; } = false;
        }

        private void StackPanel_IsMouseDirectlyOverChanged(object sender, DependencyPropertyChangedEventArgs e)
        {

        }


        public enum cnslmsgtyp { info, success, warning, error, roblox }

        private void addconsoleoutput(string message, cnslmsgtyp type = cnslmsgtyp.info)
        {
            string prefix = "";
            switch (type)
            {
                case cnslmsgtyp.success: prefix = "[SUCCESS]"; break;
                case cnslmsgtyp.warning: prefix = "[WARN]"; break;
                case cnslmsgtyp.error: prefix = "[ERROR]"; break;
                case cnslmsgtyp.roblox: prefix = "[roblox]"; break;
                case cnslmsgtyp.info: default: prefix = "[INFO]"; break;
            }
            Log($"{prefix} {message}");
        }

        private void RpcStatus(string status)
        {
        }

        private async Task CheckAndUpdateRankalfFiles(bool force = false)
        {

        }

        private async void CheckUpdates_Click(object sender, RoutedEventArgs e)
        {
            await CheckAndUpdateRankalfFiles(false);
        }

        private async void ForceUpdate_Click(object sender, RoutedEventArgs e)
        {
            await CheckAndUpdateRankalfFiles(true);
        }

        private async Task DownloadFile(string url, string fileName)
        {
            if (_advancedLogs) Log($"[Advanced] Downloading {url} to {fileName}");
            HttpClient client = new HttpClient();
            try
            {
                byte[] fileBytes = await client.GetByteArrayAsync(url);
                using (var fs = new FileStream(fileName, FileMode.Create, FileAccess.Write, FileShare.None))
                {
                    await fs.WriteAsync(fileBytes, 0, fileBytes.Length);
                }
            }
            finally
            {
                client.Dispose();
            }
        }

        private async Task KillIfRunning(string processName, string displayName)
        {
            await Task.Run(() =>
            {
                foreach (var proc in Process.GetProcessesByName(processName))
                {
                    try
                    {
                        proc.Kill();
                        proc.WaitForExit(1000);
                        Dispatcher.Invoke(() => addconsoleoutput($"{displayName} was running and has been closed", cnslmsgtyp.warning));
                    }
                    catch
                    {
                        Dispatcher.Invoke(() => addconsoleoutput($"Failed to close {displayName} process", cnslmsgtyp.error));
                    }
                }
            });
        }

        private void DecryptDllIfNeeded(string dllPath)
        {
            if (!File.Exists(dllPath))
                throw new FileNotFoundException("DLL not found", dllPath);

            byte[] file = File.ReadAllBytes(dllPath);

            if (file.Length >= 2 && file[0] == 'M' && file[1] == 'Z')
                return;

            if (file.Length < 4 ||
                file[0] != (byte)'Y' || file[1] != (byte)'B' ||
                file[2] != (byte)'X' || file[3] != (byte)'E')
                throw new Exception("Not an encrypted Rankalf DLL");

            int keyLen = file[4];
            byte[] key = new byte[keyLen];
            Buffer.BlockCopy(file, 5, key, 0, keyLen);
            for (int i = 0; i < keyLen; i++)
                key[i] ^= 0xAA;

            int dataOffset = 5 + keyLen;
            byte[] data = new byte[file.Length - dataOffset];
            Buffer.BlockCopy(file, dataOffset, data, 0, data.Length);

            for (int i = 0; i < data.Length; i++)
            {
                byte prev = (i > 0) ? data[i - 1] : (byte)i;
                data[i] ^= key[(i + prev) % keyLen];
            }

            if (data[0] != 'M' || data[1] != 'Z')
                throw new Exception("Decrypted data is not a valid DLL (missing MZ header)");

            File.WriteAllBytes(dllPath, data);
            if (_advancedLogs) Log("[Advanced] DLL decrypted and saved.");
        }

        private async Task<bool> WaitForPipeSmartAsync(string pipeName, Process target, int timeoutMs)
        {
            if (_advancedLogs) Log($"[Advanced] Waiting for pipe '{pipeName}' (Timeout: {timeoutMs}ms)");
            var sw = Stopwatch.StartNew();
            int delay = 100;
            var rnd = new Random();

            while (sw.ElapsedMilliseconds < timeoutMs)
            {
                try
                {
                    if (await NamedPipeMonitor.namedpipeexist(pipeName))
                    {
                        if (_advancedLogs) Log($"[Advanced] Pipe '{pipeName}' found. Handshake successful.");
                        return true;
                    }
                }
                catch (Exception ex)
                {
                    if (_advancedLogs) Log($"[Advanced] Handshake check failed: {ex.Message}");
                }

                try
                {
                    if (target.HasExited) return false;
                }
                catch { return false; }

                int jitter = rnd.Next(0, 60);
                await Task.Delay(delay + jitter);

                if (delay < 600) delay = Math.Min(600, delay + 80);
            }
            if (_advancedLogs) Log($"[Advanced] Handshake timed out for pipe '{pipeName}' after {timeoutMs}ms");
            return false;
        }

        private async Task<string> GetLatestRobloxVersionAsync()
        {
            try
            {
                using (var client = new HttpClient())
                {
                    var text = await client.GetStringAsync("https://setup.rbxcdn.com/DeployHistory.txt");
                    var lines = text.Split('\n').Reverse();
                    foreach (var line in lines)
                    {
                        if (line.StartsWith("New WindowsPlayer version-"))
                        {
                            var match = System.Text.RegularExpressions.Regex.Match(line, @"version-([a-f0-9]+)");
                            if (match.Success)
                                return match.Groups[1].Value;
                        }
                    }
                }
            }
            catch { }
            return null;
        }

        private Dictionary<string, string> GetExtractMap()
        {
            return new Dictionary<string, string>
            {
                { "RobloxApp.zip", "" },
                { "redist.zip", "" },
                { "shaders.zip", "shaders/" },
                { "ssl.zip", "ssl/" },
                { "WebView2.zip", "" },
                { "WebView2RuntimeInstaller.zip", "WebView2RuntimeInstaller/" },
                { "content-avatar.zip", "content/avatar/" },
                { "content-configs.zip", "content/configs/" },
                { "content-fonts.zip", "content/fonts/" },
                { "content-models.zip", "content/models/" },
                { "content-sky.zip", "content/sky/" },
                { "content-sounds.zip", "content/sounds/" },
                { "content-textures2.zip", "content/textures/" },
                { "content-terrain.zip", "PlatformContent/pc/terrain/" },
                { "content-platform-fonts.zip", "PlatformContent/pc/fonts/" },
                { "content-platform-dictionaries.zip", "PlatformContent/pc/shared_compression_dictionaries/" },
                { "content-textures3.zip", "PlatformContent/pc/textures/" },
                { "extracontent-luapackages.zip", "ExtraContent/LuaPackages/" },
                { "extracontent-translations.zip", "ExtraContent/translations/" },
                { "extracontent-models.zip", "ExtraContent/models/" },
                { "extracontent-textures.zip", "ExtraContent/textures/" },
                { "extracontent-places.zip", "ExtraContent/places/" }
            };
        }

        private string GetRobloxExtractPath(string zipName)
        {
            var map = GetExtractMap();
            return map.ContainsKey(zipName) ? map[zipName] : null;
        }

        private async Task<List<Tuple<string, string, long>>> FetchPkgManifest(string version)
        {
            List<Tuple<string, string, long>> result = new List<Tuple<string, string, long>>();
            try
            {
                string url = $"https://setup-aws.rbxcdn.com/version-{version}-rbxPkgManifest.txt";
                using (HttpClient client = new HttpClient())
                {
                    string response = await client.GetStringAsync(url);
                    string[] lines = response.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);

                    for (int i = 1; i + 3 < lines.Length; i += 4)
                    {
                        string name = lines[i].Trim();
                        string hash = lines[i + 1].Trim();
                        long size = long.Parse(lines[i + 2].Trim());

                        result.Add(Tuple.Create(name, hash, size));
                    }
                }
            }
            catch (Exception ex)
            {
                addconsoleoutput("Manifest fetch error: " + ex.Message);
            }
            return result;
        }

        private async Task<bool> DownloadFile(string url, string output, long expectedSize)
        {
            try
            {
                using (HttpClient client = new HttpClient())
                {
                    HttpResponseMessage response = await client.GetAsync(url);
                    response.EnsureSuccessStatusCode();

                    using (FileStream fs = new FileStream(output, FileMode.Create, FileAccess.Write))
                    {
                        await response.Content.CopyToAsync(fs);
                    }
                }

                FileInfo fi = new FileInfo(output);
                if (fi.Exists && fi.Length == expectedSize)
                    return true;

                fi.Delete();
            }
            catch (Exception ex)
            {
                addconsoleoutput("Download error: " + ex.Message);
            }

            return false;
        }

        private async Task<bool> ExtractZipFile(string zipPath, string destRoot, string zipName)
        {
            return await Task.Run(() =>
            {
                try
                {
                    string relative = GetRobloxExtractPath(zipName);
                    if (relative == null)
                    {
                        addconsoleoutput("Unknown extraction path: " + zipName);
                        return false;
                    }

                    string outputDir = System.IO.Path.Combine(destRoot, relative);

                    using (System.IO.Compression.ZipArchive archive = System.IO.Compression.ZipFile.OpenRead(zipPath))
                    {
                        foreach (System.IO.Compression.ZipArchiveEntry entry in archive.Entries)
                        {
                            if (string.IsNullOrWhiteSpace(entry.Name))
                                continue;

                            string fullPath = System.IO.Path.Combine(outputDir, entry.FullName);
                            string dir = System.IO.Path.GetDirectoryName(fullPath);

                            if (!Directory.Exists(dir))
                                Directory.CreateDirectory(dir);

                            entry.ExtractToFile(fullPath, true);
                        }
                    }
                    return true;
                }
                catch (Exception ex)
                {
                    addconsoleoutput("Extraction failed (" + zipName + "): " + ex.Message);
                    return false;
                }
            });
        }

        public static class RobloxLocator
        {
            public static string GetRobloxInstallDir()
            {
                var dir = TryGetDirFromCommand(@"roblox\shell\open\command");
                if (!string.IsNullOrEmpty(dir)) return dir;

                dir = TryGetDirFromCommand(@"roblox\DefaultIcon");
                if (!string.IsNullOrEmpty(dir)) return dir;

                dir = FindDirByScan();
                if (!string.IsNullOrEmpty(dir)) return dir;

                try
                {
                    using (var wc = new WebClient())
                    {
                        string robloxVersionRaw = wc.DownloadString(
                            "https://versioncompatibility.api.roblox.com/GetCurrentClientVersionUpload/?apiKey=76e5a40c-3ae1-4028-9f10-7c62520bd94f&binaryType=WindowsPlayer"
                        ).Trim().Replace("\"", "");

                        if (!string.IsNullOrWhiteSpace(robloxVersionRaw))
                        {
                            string versionsRoot = System.IO.Path.Combine(
                                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                                "Roblox", "Versions"
                            );

                            if (!Directory.Exists(versionsRoot))
                                Directory.CreateDirectory(versionsRoot);

                            string newDir = System.IO.Path.Combine(versionsRoot, robloxVersionRaw);
                            if (!Directory.Exists(newDir))
                                Directory.CreateDirectory(newDir);

                            return newDir;
                        }
                    }
                }
                catch { }

                return null;
            }

            public static async Task<bool> EnsureRobloxProtocolRegisteredAsync()
            {
                var dir = GetRobloxInstallDir();
                if (string.IsNullOrEmpty(dir)) return false;

                var exe = System.IO.Path.Combine(dir, "RobloxPlayerBeta.exe");
                if (!File.Exists(exe)) return false;

                var version = await FetchCurrentClientVersionAsync();

                bool updated = false;

                using (var baseKey = Registry.CurrentUser.CreateSubKey(@"Software\Classes\roblox", true))
                {
                    if (baseKey == null) return false;

                    using (var defaultIcon = baseKey.CreateSubKey("DefaultIcon", true))
                    {
                        var val = defaultIcon.GetValue(null) as string;
                        if (string.IsNullOrWhiteSpace(val) || !File.Exists(val.Trim('"')))
                        {
                            defaultIcon.SetValue(null, exe, RegistryValueKind.String);
                            updated = true;
                        }
                    }

                    using (var cmd = baseKey.CreateSubKey(@"shell\open\command", true))
                    {
                        var val = cmd.GetValue(null) as string;
                        var expectedCmd = $"\"{exe}\" %1";

                        if (string.IsNullOrWhiteSpace(val) ||
                            val.IndexOf("RobloxPlayerBeta.exe", StringComparison.OrdinalIgnoreCase) < 0)
                        {
                            cmd.SetValue(null, expectedCmd, RegistryValueKind.String);
                            updated = true;
                        }

                        if (!string.IsNullOrWhiteSpace(version))
                            cmd.SetValue("version", version, RegistryValueKind.String);
                    }
                }

                if (updated)
                {
                    var testPath = TryGetDirFromCommand(@"roblox\shell\open\command");
                    return !string.IsNullOrEmpty(testPath);
                }

                return true;
            }

            private static string TryGetDirFromCommand(string subKey)
            {
                try
                {
                    using (var key = Registry.ClassesRoot.OpenSubKey(subKey))
                    {
                        if (key == null) return null;

                        var raw = key.GetValue(null) as string;
                        if (string.IsNullOrWhiteSpace(raw)) return null;

                        var m = System.Text.RegularExpressions.Regex.Match(raw, "\"([^\"]+)\"");
                        var path = m.Success ? m.Groups[1].Value : raw.Trim();

                        path = path.Trim('"');
                        var dir = File.Exists(path) ? System.IO.Path.GetDirectoryName(path) : System.IO.Path.GetDirectoryName(path);
                        if (!string.IsNullOrEmpty(dir) && Directory.Exists(dir)) return dir;
                    }
                }
                catch { }
                return null;
            }

            private static string FindDirByScan()
            {
                try
                {
                    var root = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Roblox", "Versions");
                    if (!Directory.Exists(root)) return null;

                    var best = Directory.EnumerateDirectories(root, "version-*")
                        .Select(d => new DirectoryInfo(d))
                        .OrderByDescending(d => d.LastWriteTimeUtc)
                        .FirstOrDefault(d => File.Exists(System.IO.Path.Combine(d.FullName, "RobloxPlayerBeta.exe")));

                    return best?.FullName;
                }
                catch { }
                return null;
            }

            public static async Task<string> FetchCurrentClientVersionAsync()
            {
                try
                {
                    using (var wc = new WebClient())
                    {
                        var s = await wc.DownloadStringTaskAsync(
                            "https://versioncompatibility.api.roblox.com/GetCurrentClientVersionUpload/?apiKey=76e5a40c-3ae1-4028-9f10-7c62520bd94f&binaryType=WindowsPlayer");
                        return s.Trim().Replace("\"", "");
                    }
                }
                catch { return null; }
            }
        }

        private static string NormalizeVersion(string v)
        {
            if (string.IsNullOrWhiteSpace(v)) return null;
            v = v.Trim().Trim('"');
            if (v.StartsWith("version-", StringComparison.OrdinalIgnoreCase))
                v = v.Substring("version-".Length);
            return v;
        }

        private async Task<bool> TryDeleteDirectoryWithRetries(string path, int maxAttempts = 6, int delayMs = 300)
        {
            if (string.IsNullOrWhiteSpace(path) || !Directory.Exists(path)) return true;

            for (int attempt = 1; attempt <= maxAttempts; attempt++)
            {
                try
                {
                    foreach (var f in Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories))
                    {
                        try { File.SetAttributes(f, FileAttributes.Normal); } catch { }
                    }
                    foreach (var d in Directory.EnumerateDirectories(path, "*", SearchOption.AllDirectories))
                    {
                        try { File.SetAttributes(d, FileAttributes.Normal); } catch { }
                    }

                    Directory.Delete(path, recursive: true);
                    return true;
                }
                catch
                {
                    await Task.Delay(delayMs);
                }
            }
            return !Directory.Exists(path);
        }

        private bool VerifyRobloxInstallation(string dir)
        {
            string exe = System.IO.Path.Combine(dir, "RobloxPlayerBeta.exe");
            if (!File.Exists(exe))
            {
                addconsoleoutput("Verification failed: RobloxPlayerBeta.exe missing.");
                return false;
            }

            string[] critical = { "content", "shaders", "PlatformContent" };
            foreach (var c in critical)
            {
                if (!Directory.Exists(System.IO.Path.Combine(dir, c)))
                {
                    addconsoleoutput($"Verification warning: Critical folder '{c}' missing.");
                }
            }
            return true;
        }

        private async Task DownloadRobloxVersion(string versionOverride = null)
        {
            string backupDir = null;
            string targetInstallDir = null;
            try
            {
                if (!await RobloxLocator.EnsureRobloxProtocolRegisteredAsync())
                    addconsoleoutput("Roblox install/registry not found. Proceeding to download full client");
                else
                    addconsoleoutput("Roblox ok");

                string currentInstallDir = RobloxLocator.GetRobloxInstallDir();
                if (string.IsNullOrEmpty(currentInstallDir))
                {
                    addconsoleoutput("Could not locate Roblox installation");
                    return;
                }

                addconsoleoutput("Roblox install folder: " + currentInstallDir);

                foreach (var proc in System.Diagnostics.Process.GetProcessesByName("RobloxPlayerBeta"))
                {
                    try { proc.Kill(); proc.WaitForExit(3000); } catch { }
                }

                string version = NormalizeVersion(versionOverride);
                if (string.IsNullOrEmpty(version))
                {
                    using (var wc = new System.Net.WebClient())
                    {
                        string robloxVersionRaw = wc.DownloadString(
                            "https://versioncompatibility.api.roblox.com/GetCurrentClientVersionUpload/?apiKey=76e5a40c-3ae1-4028-9f10-7c62520bd94f&binaryType=WindowsPlayer"
                        );
                        version = NormalizeVersion(robloxVersionRaw);
                    }
                }

                if (string.IsNullOrEmpty(version))
                {
                    addconsoleoutput("Could not get Roblox version");
                    return;
                }

                addconsoleoutput("Roblox version: " + version);

                string versionsDir = null;
                try
                {
                    versionsDir = System.IO.Directory.GetParent(currentInstallDir)?.FullName;
                }
                catch { }

                if (string.IsNullOrEmpty(versionsDir) || !Directory.Exists(versionsDir))
                    versionsDir = currentInstallDir;

                targetInstallDir = System.IO.Path.Combine(versionsDir, $"version-{version}");
                addconsoleoutput("Target folder: " + targetInstallDir);

                if (Directory.Exists(targetInstallDir))
                {
                    addconsoleoutput("Cleaning existing target folder…");
                    if (!await TryDeleteDirectoryWithRetries(targetInstallDir))
                    {
                        addconsoleoutput("Failed to clean target folder, aborting.");
                        return;
                    }
                }

                Directory.CreateDirectory(targetInstallDir);

                var manifest = await FetchPkgManifest(version);
                if (manifest == null || manifest.Count == 0)
                {
                    addconsoleoutput("Manifest fetch failed");
                    return;
                }

                var neededPackages = GetExtractMap();

                foreach (var pkg in neededPackages)
                {
                    var entry = manifest.Find(x => x.Item1 == pkg.Key);
                    if (entry == null)
                    {
                        addconsoleoutput("Missing from manifest: " + pkg.Key);
                        continue;
                    }

                    string url = $"https://setup-aws.rbxcdn.com/version-{version}-{entry.Item1}";
                    string localZip = System.IO.Path.Combine(System.IO.Path.GetTempPath(), entry.Item1);

                    addconsoleoutput("Downloading: " + entry.Item1);
                    bool success = await DownloadFile(url, localZip, entry.Item3);

                    if (!success)
                    {
                        addconsoleoutput("Retry: " + entry.Item1);
                        success = await DownloadFile(url, localZip, entry.Item3);
                    }

                    if (!success)
                    {
                        addconsoleoutput("Failed to download: " + pkg.Key);
                        continue;
                    }

                    bool extracted = await ExtractZipFile(localZip, targetInstallDir, pkg.Key);
                    try { File.Delete(localZip); } catch { }

                    if (!extracted)
                        addconsoleoutput("Extraction failed: " + pkg.Key);
                }

                string settingsPath = System.IO.Path.Combine(targetInstallDir, "AppSettings.xml");
                try
                {
                    string xmlContent =
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
                        "<Settings>\n" +
                        "\t<ContentFolder>content</ContentFolder>\n" +
                        "\t<BaseUrl>http://www.roblox.com</BaseUrl>\n" +
                        "</Settings>\n";

                    File.WriteAllText(settingsPath, xmlContent, System.Text.Encoding.UTF8);
                    addconsoleoutput("Created AppSettings.xml");
                }
                catch (Exception ex)
                {
                    addconsoleoutput("Failed to create AppSettings.xml: " + ex.Message);
                }

                if (!VerifyRobloxInstallation(targetInstallDir))
                {
                    addconsoleoutput("Installation verification failed.");
                    return;
                }

                if (!await RobloxLocator.EnsureRobloxProtocolRegisteredAsync())
                    addconsoleoutput("Roblox registry still missing after install");
                else
                    addconsoleoutput("Roblox registry OK");

                try
                {
                    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                    {
                        FileName = System.IO.Path.Combine(targetInstallDir, "RobloxPlayerBeta.exe"),
                        WorkingDirectory = targetInstallDir,
                        UseShellExecute = true
                    });
                    addconsoleoutput("Launched Roblox");
                }
                catch (Exception ex)
                {
                    addconsoleoutput("Failed to launch Roblox: " + ex.Message);
                }
            }
            catch (Exception ex)
            {
                addconsoleoutput("Fatal error: " + ex.Message);
            }
        }

        private static bool LooksLikeHash(string s)
        {
            if (string.IsNullOrWhiteSpace(s)) return false;
            var t = s.StartsWith("version-", StringComparison.OrdinalIgnoreCase) ? s.Substring(8) : s;
            if (t.Length < 8) return false;
            foreach (var c in t)
            {
                bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                if (!hex) return false;
            }
            return true;
        }

        private void versionHashBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            installVersionBtn.IsEnabled = LooksLikeHash(versionHashBox.Text);
        }

        private async void InstallVersion_Click(object sender, RoutedEventArgs e)
        {
            CloseSettings_Click(null, null);

            var raw = (versionHashBox.Text ?? "").Trim();
            if (!LooksLikeHash(raw))
            {
                addconsoleoutput("Invalid version hash");
                return;
            }
            var hash = raw.StartsWith("version-", StringComparison.OrdinalIgnoreCase) ? raw.Substring(8) : raw;
            addconsoleoutput("Installing version-" + hash + " …");
            await DownloadRobloxVersion(hash);
        }

        private async void UseLiveVersion_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                using (var wc = new WebClient())
                {
                    var s = wc.DownloadString(
                        "https://versioncompatibility.api.roblox.com/GetCurrentClientVersionUpload/?apiKey=76e5a40c-3ae1-4028-9f10-7c62520bd94f&binaryType=WindowsPlayer"
                    ).Trim().Replace("\"", "");
                    var hash = s.Replace("version-", "");
                    versionHashBox.Text = hash;
                    addconsoleoutput("Live version: version-" + hash);
                }
            }
            catch (Exception ex)
            {
                addconsoleoutput("Failed to fetch live version: " + ex.Message);
            }
        }

        private async void UseLatestPreBuild_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                using (var client = new HttpClient())
                {
                    var text = await client.GetStringAsync("https://setup.rbxcdn.com/DeployHistory.txt");
                    var lines = text.Split('\n').Reverse();
                    foreach (var line in lines)
                    {
                        if (line.StartsWith("New WindowsPlayer version-"))
                        {
                            var m = System.Text.RegularExpressions.Regex.Match(line, @"version-([a-f0-9]+)");
                            if (m.Success)
                            {
                                var latest = m.Groups[1].Value;
                                versionHashBox.Text = latest;
                                addconsoleoutput("Latest (DeployHistory): version-" + latest);
                                break;
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                addconsoleoutput("Failed to fetch DeployHistory: " + ex.Message);
            }
        }

        private async Task<bool> IsPreBuildAvailableAsync()
        {
            try
            {
                string latestHistory = await GetLatestRobloxVersionAsync();
                if (string.IsNullOrEmpty(latestHistory))
                    return false;

                using (var wc = new WebClient())
                {
                    string robloxVersionRaw = wc.DownloadString(
                        "https://versioncompatibility.api.roblox.com/GetCurrentClientVersionUpload/?apiKey=76e5a40c-3ae1-4028-9f10-7c62520bd94f&binaryType=WindowsPlayer"
                    ).Trim().Replace("\"", "");

                    string liveVersion = robloxVersionRaw.Replace("version-", "").Trim();

                    return !string.Equals(latestHistory, liveVersion, StringComparison.OrdinalIgnoreCase);
                }
            }
            catch
            {
                return false;
            }
        }

        private async void DownloadPreBuild_Click(object sender, RoutedEventArgs e)
        {
            CloseSettings_Click(null, null);

            if (await IsPreBuildAvailableAsync())
            {
                string preBuildHash = await GetLatestRobloxVersionAsync();
                addconsoleoutput($"Pre-Build detected: version-{preBuildHash}");

                await DownloadRobloxVersion(preBuildHash);
            }
            else
            {
                addconsoleoutput("No pre-build available");
            }
        }

        private void OpenRoblox_Click(object sender, RoutedEventArgs e)
        {
            var dir = RobloxLocator.GetRobloxInstallDir();
            var exe = string.IsNullOrEmpty(dir) ? null : System.IO.Path.Combine(dir, "RobloxPlayerBeta.exe");
            if (!string.IsNullOrEmpty(exe) && System.IO.File.Exists(exe))
            {
                try
                {
                    System.Diagnostics.Process.Start(exe);
                    addconsoleoutput("Launched Roblox");
                }
                catch (Exception ex) { addconsoleoutput("Launch failed: " + ex.Message); }
            }
            else addconsoleoutput("Roblox not found.");
        }

        private void OpenRobloxFolder_Click(object sender, RoutedEventArgs e)
        {
            var dir = RobloxLocator.GetRobloxInstallDir();
            if (!string.IsNullOrEmpty(dir) && System.IO.Directory.Exists(dir))
                System.Diagnostics.Process.Start("explorer.exe", dir);
            else
                addconsoleoutput("Roblox folder not found");
        }
    }

    [Flags]
    public enum BanFlags
    {
        None = 0,
        KickMessage = 1 << 0,
        Error600 = 1 << 1,
        ExploitBanText = 1 << 2,
        AccountModeration = 1 << 3,
        Kick267 = 1 << 4,
        Restrict773 = 1 << 5
    }

    public enum RobloxBanEventType
    {
        KickMessage,
        Error600,
        ExploitBan,
        AccountModeration,
        Disconnect,
        Generic,
        PreJoinBlocked
    }

    public sealed class RobloxBanEvent
    {
        public RobloxBanEventType Type;
        public string Message;
        public string Code;
        public string RawLine;
        public string RID;
        public DateTime WhenUtc;
        public BanFlags Flags;
    }

    public sealed class RobloxBanLogWatcher : IDisposable
    {
        public bool EnableDebug = false;
        public int DedupeWindowSeconds = 8;
        public int AccountDebounceMs = 300;

        private string _baselineUsername;
        private DateTime _lastUserWarnUtc = DateTime.MinValue;

        private static readonly Regex _loginUsernameRx =
            new Regex(@"""username""\s*:\s*""([^""]+)""", RegexOptions.IgnoreCase | RegexOptions.Compiled);

        private static string UserBaselinePath()
        {
            var dir = System.IO.Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Rankalf", "workspace");
            System.IO.Directory.CreateDirectory(dir);
            return System.IO.Path.Combine(dir, "rbx_user.txt");
        }

        private void LoadUserBaseline()
        {
            try
            {
                var p = UserBaselinePath();
                if (System.IO.File.Exists(p))
                    _baselineUsername = System.IO.File.ReadAllText(p, Encoding.UTF8).Trim();
            }
            catch { }
        }

        private void SaveUserBaseline(string username)
        {
            try
            {
                System.IO.File.WriteAllText(UserBaselinePath(), username ?? "", Encoding.UTF8);
            }
            catch { }
        }

        private bool _hjActive;
        private DateTime _hjStartUtc;
        private bool _hjDmInit;
        private DateTime _hjDmInitUtc;
        private int _hjNoDmHits;
        private bool _hjNetStart;

        private CancellationTokenSource _hjTimerCts;

        public int PreDmTimeoutSeconds = 4;
        public int PostDmTimeoutSeconds = 3;

        public Action<RobloxBanEvent> OnEvent;
        public Action<BanFlags, RobloxBanEvent> OnBypassRequested;
        public Action<string> OnDebug;

        private readonly string _logsDir =
            System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Roblox", "logs");

        private FileSystemWatcher _fsw;
        private readonly ConcurrentDictionary<string, long> _offsets = new ConcurrentDictionary<string, long>();
        private readonly CancellationTokenSource _cts = new CancellationTokenSource();
        private readonly ConcurrentDictionary<string, DateTime> _recent = new ConcurrentDictionary<string, DateTime>();

        private readonly Tuple<RobloxBanEventType, Regex>[] _rules;

        private readonly object _accLock = new object();
        private RobloxBanEvent _accPending;
        private DateTime _accEmitAtUtc = DateTime.MinValue;
        private bool _accScheduled;

        private bool HeuristicProcess(string line)
        {
            var l = line.ToLowerInvariant();
            var now = DateTime.UtcNow;

            if (l.Contains("setstage: (stage:ugcgame)") || l.Contains("launchugcgame: (stage:luaapp)"))
            {
                Heur_BeginJoin("stage", line);
                return false;
            }

            if (!_hjActive &&
                (l.Contains("join: initJoin blocked before DataModel ialized dm") ||
                 l.Contains("::start datamodel(") ||
                 l.Contains("::run datamodel(") ||
                 l.Contains("replacedatamodel")))
            {
                _hjActive = true;
                _hjDmInit = true;
                _hjDmInitUtc = now;
                _hjNoDmHits = 0;
                _hjNetStart = false;
                _hjTimerCts = new CancellationTokenSource();
                var tok = _hjTimerCts.Token;

                Task.Run(async () =>
                {
                    try
                    {
                        await Task.Delay(PostDmTimeoutSeconds * 1000, tok);
                        if (tok.IsCancellationRequested) return;
                        if (_hjActive && _hjDmInit && !_hjNetStart)
                        {
                            var ev = BuildEvent(RobloxBanEventType.Error600,
                                "Join blocked after DataModel (no network handshake)", null, null, line);
                            PublishOnce(ev);
                            Heur_Reset();
                            if (EnableDebug) OnDebug?.Invoke("[BanWatcher] post-DM timeout (late start) fired");
                        }
                    }
                    catch { }
                }, tok);

                return false;
            }

            if (!_hjActive) return false;

            if (l.Contains("join: initialized dm") ||
                l.Contains("::start datamodel(") ||
                l.Contains("::run datamodel(") ||
                l.Contains("replacedatamodel"))
            {
                _hjDmInit = true;
                _hjDmInitUtc = now;
                var tok = _hjTimerCts != null ? _hjTimerCts.Token : new CancellationTokenSource().Token;

                Task.Run(async () =>
                {
                    try
                    {
                        await Task.Delay(PostDmTimeoutSeconds * 1000, tok);
                        if (tok.IsCancellationRequested) return;
                        if (_hjActive && _hjDmInit && !_hjNetStart)
                        {
                            var ev = BuildEvent(RobloxBanEventType.Error600,
                                "Join blocked after DataModel (no network handshake)", null, null, line);
                            PublishOnce(ev);
                            Heur_Reset();
                            if (EnableDebug) OnDebug?.Invoke("[BanWatcher] post-DM timeout fired");
                        }
                    }
                    catch { }
                }, tok);

                return false;
            }

            if (l.Contains("networkclient:create") ||
                l.Contains("client will attempt to connect") ||
                l.Contains("connection accepted") ||
                l.Contains("replicator created"))
            {
                _hjNetStart = true;
                Heur_Reset();
                return false;
            }

            bool bounceToExperienceDetail = l.Contains("application did receive notification") &&
                                            (l.Contains("experiencedetail") || l.Contains("experience detail"));
            bool bounceToHome = l.Contains("application did receive notification") && l.Contains("data(home)");
            bool bounceToLuaAppNotInit = l.Contains("returntoluaappinternal") && l.Contains("app not yet initialized");

            if (!_hjDmInit && (bounceToExperienceDetail || bounceToHome || bounceToLuaAppNotInit))
            {
                var evA = BuildEvent(RobloxBanEventType.Error600,
                    "Join blocked before DataModel (bounce)", null, null, line);
                PublishOnce(evA);
                Heur_Reset();
                if (EnableDebug) OnDebug?.Invoke("[BanWatcher] pre-DM bounce fired");
                return true;
            }

            if (_hjDmInit && !_hjNetStart && (bounceToExperienceDetail || bounceToHome || bounceToLuaAppNotInit))
            {
                var evB = BuildEvent(RobloxBanEventType.Error600,
                    "Join blocked after DataModel (bounced without handshake)", null, null, line);
                PublishOnce(evB);
                Heur_Reset();
                if (EnableDebug) OnDebug?.Invoke("[BanWatcher] post-DM bounce fired");
                return true;
            }

            if ((now - _hjStartUtc).TotalSeconds > 30) Heur_Reset();
            return false;
        }

        private void Heur_Reset()
        {
            _hjActive = false;
            _hjDmInit = false;
            _hjNetStart = false;
            _hjNoDmHits = 0;
            _hjStartUtc = DateTime.MinValue;
            _hjDmInitUtc = DateTime.MinValue;
            if (_hjTimerCts != null) { try { _hjTimerCts.Cancel(); } catch { } _hjTimerCts.Dispose(); _hjTimerCts = null; }
        }

        private void Heur_BeginJoin(string why, string rawLine)
        {
            Heur_Reset();
            _hjActive = true;
            _hjStartUtc = DateTime.UtcNow;
            _hjDmInit = false;
            _hjDmInitUtc = DateTime.MinValue;
            _hjNoDmHits = 0;
            _hjNetStart = false;

            _hjTimerCts = new CancellationTokenSource();
            var tok = _hjTimerCts.Token;

            if (EnableDebug && OnDebug != null) OnDebug("[BanWatcher] join begin (" + why + ")");

            Task.Run(async () =>
            {
                try
                {
                    await Task.Delay(PreDmTimeoutSeconds * 1000, tok);
                    if (tok.IsCancellationRequested) return;
                    if (_hjActive && !_hjDmInit)
                    {
                        var ev = BuildEvent(RobloxBanEventType.Error600,
                            "Join blocked before DataModel (timeout)", null, null, rawLine);
                        PublishOnce(ev);
                        Heur_Reset();
                        if (EnableDebug && OnDebug != null) OnDebug("[BanWatcher] pre-DM timeout fired");
                    }
                }
                catch { }
            }, tok);
        }

        public RobloxBanLogWatcher()
        {
            _rules = new Tuple<RobloxBanEventType, Regex>[]
            {
                Tuple.Create(
                    RobloxBanEventType.KickMessage,
                    new Regex(@"Server Kick Message:\s*(?<msg>.+?)(?:\s*\|\s*CODE\s+(?<code>[A-Z0-9\-]+))?$",
                              RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.Error600,
                    new Regex(@"avoid an enforcement action.*account.*(?:Error\s*Code[:\s]*|Code[:\s]*)600",
                              RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.Error600,
                    new Regex(@"Error\s*Code[:\s]*600", RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.ExploitBan,
                    new Regex(@"You have been banned.*", RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.ExploitBan,
                    new Regex(@"banned\s+for\s+exploiting", RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.Disconnect,
                    new Regex(@"Disconnection Notification\.\s*Reason:\s*(?<rid>\d+)",
                                RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.Disconnect,
                    new Regex(@"Disconnect reason received:\s*(?<rid>\d+)",
                                RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.Disconnect,
                    new Regex(@"Sending\s+disconnect\s+with\s+reason:\s*(?<rid>\d+)",
                                RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.AccountModeration,
                    new Regex(@"The user is moderated(?: with type:\s*`?(?<msg>[^`]+)`?)?",
                              RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.AccountModeration,
                    new Regex(@"moderationState\s*is:`?\s*ModerationState\.Banned`?",
                              RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
                Tuple.Create(
                    RobloxBanEventType.AccountModeration,
                    new Regex(@"\bBan\s+\d+\s+(?:Day|Days|Week|Weeks|Month|Months|Year|Years)\b",
                              RegexOptions.IgnoreCase | RegexOptions.Compiled)
                ),
            };
        }

        public void Start()
        {
            Directory.CreateDirectory(_logsDir);

            _fsw = new FileSystemWatcher(_logsDir, "*.log");
            _fsw.IncludeSubdirectories = false;
            _fsw.NotifyFilter = NotifyFilters.FileName | NotifyFilters.LastWrite | NotifyFilters.Size;
            _fsw.Created += (s, e) => Task.Run(() => ProcessDelta(e.FullPath));
            _fsw.Changed += (s, e) => Task.Run(() => ProcessDelta(e.FullPath));
            _fsw.EnableRaisingEvents = true;

            string[] files = new string[0];
            try { files = Directory.GetFiles(_logsDir, "*.log"); } catch { }
            for (int i = 0; i < files.Length; i++)
            {
                var fi = new FileInfo(files[i]);
                _offsets.TryAdd(files[i], fi.Length);
            }

            if (EnableDebug && OnDebug != null) OnDebug("[BanWatcher] started; dir=" + _logsDir);
        }

        public void Stop()
        {
            if (_fsw != null) { _fsw.Dispose(); _fsw = null; }
            _cts.Cancel();
        }

        public void Dispose() { Stop(); }

        private async Task ProcessDelta(string path, int retries = 3)
        {
            if (_cts.IsCancellationRequested) return;
            try { await Task.Delay(60, _cts.Token); } catch { }

            long start = _offsets.GetOrAdd(path, 0);
            long length;
            try { length = new FileInfo(path).Length; } catch { return; }
            if (length <= start) { _offsets[path] = length; return; }

            for (int attempt = 0; attempt < retries; attempt++)
            {
                try
                {
                    using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                    using (var sr = new StreamReader(fs, Encoding.UTF8, true, 4096))
                    {
                        fs.Seek(start, SeekOrigin.Begin);
                        string line;
                        while ((line = sr.ReadLine()) != null)
                            Inspect(line);
                        _offsets[path] = fs.Position;
                    }
                    return;
                }
                catch (IOException)
                {
                    await Task.Delay(80);
                }
            }
        }

        private void Inspect(string line)
        {
            if (string.IsNullOrWhiteSpace(line)) return;
            if (HeuristicProcess(line)) return;


            var mUser = _loginUsernameRx.Match(line);
            if (mUser.Success && mUser.Groups.Count > 1)
            {
                var seen = mUser.Groups[1].Value;

                if (string.IsNullOrEmpty(_baselineUsername))
                {
                    _baselineUsername = seen;
                    SaveUserBaseline(_baselineUsername);
                    if (OnEvent != null)
                    {
                        OnEvent(new RobloxBanEvent
                        {
                            Type = RobloxBanEventType.Generic,
                            Message = "Alt guard baseline set ➜ " + _baselineUsername,
                            WhenUtc = DateTime.UtcNow,
                            Flags = BanFlags.None
                        });
                    }
                }
                else if (!string.Equals(seen, _baselineUsername, StringComparison.OrdinalIgnoreCase))
                {
                    var now = DateTime.UtcNow;
                    if ((now - _lastUserWarnUtc).TotalSeconds >= 8)
                    {
                        _lastUserWarnUtc = now;
                        if (OnEvent != null)
                        {
                            OnEvent(new RobloxBanEvent
                            {
                                Type = RobloxBanEventType.Generic,
                                Message = "⚠ Username changed: expected " + _baselineUsername + " → seen " + seen,
                                WhenUtc = now,
                                Flags = BanFlags.None
                            });
                        }
                    }
                }
            }

            for (int i = 0; i < _rules.Length; i++)
            {
                var tup = _rules[i];
                var kind = tup.Item1;
                var rx = tup.Item2;

                var m = rx.Match(line);
                if (!m.Success) continue;

                string msg = (m.Groups["msg"] != null && m.Groups["msg"].Success) ? m.Groups["msg"].Value : line;
                string code = (m.Groups["code"] != null && m.Groups["code"].Success) ? m.Groups["code"].Value : null;
                string rid = (m.Groups["rid"] != null && m.Groups["rid"].Success) ? m.Groups["rid"].Value : null;

                msg = Clean(msg);

                if (kind == RobloxBanEventType.Disconnect)
                {
                    if (rid == "267" || rid == "773") kind = RobloxBanEventType.ExploitBan;
                    else kind = RobloxBanEventType.Generic;
                }

                if (kind == RobloxBanEventType.AccountModeration)
                {
                    string dur = ExtractBanDuration(msg);
                    var compact = (dur != null) ? "Account moderated: " + dur : "Account moderated/banned";
                    var ev = BuildEvent(kind, compact, code, rid, line);

                    lock (_accLock)
                    {
                        if (_accPending == null || (_accPending.Message.IndexOf("Ban ", StringComparison.OrdinalIgnoreCase) < 0 && dur != null))
                            _accPending = ev;

                        _accEmitAtUtc = DateTime.UtcNow.AddMilliseconds(AccountDebounceMs);

                        if (!_accScheduled)
                        {
                            _accScheduled = true;
                            Task.Run(async () =>
                            {
                                while (DateTime.UtcNow < _accEmitAtUtc && !_cts.IsCancellationRequested)
                                    await Task.Delay(50);

                                RobloxBanEvent toEmit = null;
                                lock (_accLock)
                                {
                                    toEmit = _accPending;
                                    _accPending = null;
                                    _accScheduled = false;
                                }
                                if (toEmit != null) PublishOnce(toEmit);
                            });
                        }
                    }
                    return;
                }

                var outEv = BuildEvent(kind, msg, code, rid, line);
                PublishOnce(outEv);
                break;
            }
        }


        private RobloxBanEvent BuildEvent(RobloxBanEventType kind, string msg, string code, string rid, string raw)
        {
            if (!string.IsNullOrEmpty(code)) msg = msg + " [CODE=" + code + "]";
            if (!string.IsNullOrEmpty(rid)) msg = msg + " [RID=" + rid + "]";

            var whenUtc = DateTime.UtcNow;
            var flags = ToFlags(kind, code, rid);

            msg = Pretty(msg, flags);

            return new RobloxBanEvent
            {
                Type = kind,
                Message = msg,
                Code = code,
                RawLine = raw,
                RID = rid,
                WhenUtc = whenUtc,
                Flags = flags
            };
        }

        private void PublishOnce(RobloxBanEvent ev)
        {
            if (ev.Type == RobloxBanEventType.Generic || ev.Type == RobloxBanEventType.Disconnect)
                return;

            string key = ((int)ev.Type).ToString() + "|" + ev.Message;
            var now = DateTime.UtcNow;
            DateTime prev;
            if (_recent.TryGetValue(key, out prev) && (now - prev).TotalSeconds < DedupeWindowSeconds)
                return;
            _recent[key] = now;

            if (OnEvent != null) OnEvent(ev);

            if (ev.Flags != BanFlags.None && OnBypassRequested != null)
                OnBypassRequested(ev.Flags, ev);

            if (EnableDebug && OnDebug != null)
                OnDebug("[BanWatcher] " + ev.Type + " :: " + ev.Message);
        }

        private static string Clean(string line)
        {
            line = Regex.Replace(line, @"^\d{4}-\d{2}-\d{2}T[0-9:\.]+Z,[^ ]+\s*", "", RegexOptions.Compiled);
            line = Regex.Replace(line, @"^\[\d{2}:\d{2}:\d{2}\]\s*", "", RegexOptions.Compiled);
            line = Regex.Replace(line, @"^\[[^\]]+\]\s*", "", RegexOptions.Compiled);
            line = Regex.Replace(line, @"^\s*Warning:\s*", "", RegexOptions.IgnoreCase | RegexOptions.Compiled);
            return line.Trim();
        }

        private static string ExtractBanDuration(string s)
        {
            var m = Regex.Match(s, @"\bBan\s+\d+\s+(?:Day|Days|Week|Weeks|Month|Months|Year|Years)\b",
                                RegexOptions.IgnoreCase);
            return m.Success ? m.Value : null;
        }

        private static BanFlags ToFlags(RobloxBanEventType kind, string code, string rid)
        {
            BanFlags f = BanFlags.None;
            if (kind == RobloxBanEventType.KickMessage) f |= BanFlags.KickMessage;
            if (kind == RobloxBanEventType.Error600) f |= BanFlags.Error600;
            if (kind == RobloxBanEventType.ExploitBan) f |= BanFlags.ExploitBanText;
            if (kind == RobloxBanEventType.AccountModeration) f |= BanFlags.AccountModeration;
            if (rid == "267") f |= BanFlags.Kick267;
            if (rid == "773") f |= BanFlags.Restrict773;
            return f;
        }

        private static string Pretty(string msg, BanFlags flags)
        {
            if ((flags & BanFlags.Error600) != 0) msg += " {Alt-avoidance (600)}";
            if ((flags & BanFlags.Kick267) != 0) msg += " {Kick/ban (267)}";
            if ((flags & BanFlags.Restrict773) != 0) msg += " {Restricted/alt block (773)}";
            if (msg.IndexOf("[CODE=BAC", StringComparison.OrdinalIgnoreCase) >= 0)
                msg += " {Server anti-cheat (BAC)}";
            return msg;
        }

    }
}
