using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;

namespace RKO.UI
{
    public partial class App : Application
    {
        private static bool _handlingCrash;
        private const int MaxRestartCount = 3;

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            DispatcherUnhandledException += App_DispatcherUnhandledException;
            AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
            TaskScheduler.UnobservedTaskException += TaskScheduler_UnobservedTaskException;
        }

        private void TaskScheduler_UnobservedTaskException(object sender, UnobservedTaskExceptionEventArgs e)
        {
            try
            {
                HandleCrash("TaskScheduler", e.Exception);
                e.SetObserved();
            }
            catch { }
        }

        private void CurrentDomain_UnhandledException(object sender, UnhandledExceptionEventArgs e)
        {
            try
            {
                var ex = e.ExceptionObject as Exception ?? new Exception("Unknown AppDomain crash");
                HandleCrash("AppDomain", ex);
            }
            catch { }
        }

        private void App_DispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
        {
            try
            {
                HandleCrash("Dispatcher", e.Exception);
                e.Handled = true;
            }
            catch { }
        }

        private static string RecoveryDir()
        {
            var dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "RKO", "Recovery");
            Directory.CreateDirectory(dir);
            return dir;
        }

        private static int GetRestartCountFromArgs()
        {
            var args = Environment.GetCommandLineArgs();
            for (int i = 0; i < args.Length; i++)
            {
                if (!string.Equals(args[i], "--restart-count", StringComparison.OrdinalIgnoreCase))
                    continue;

                if (i + 1 < args.Length && int.TryParse(args[i + 1], out var n))
                    return Math.Max(0, n);
            }

            return 0;
        }

        private static void HandleCrash(string source, Exception ex)
        {
            if (_handlingCrash) return;
            _handlingCrash = true;

            try
            {
                var dir = RecoveryDir();
                var crashTxt = Path.Combine(dir, "last_crash.txt");
                var currentSession = Path.Combine(dir, "current_session.log");
                var previousSession = Path.Combine(dir, "previous_session.log");

                var crash = "[" + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss") + "] " + source + Environment.NewLine +
                            (ex == null ? "Unknown exception" : ex.ToString()) + Environment.NewLine;
                File.WriteAllText(crashTxt, crash);

                if (File.Exists(currentSession))
                {
                    try { File.Copy(currentSession, previousSession, true); } catch { }
                }

                var restartCount = GetRestartCountFromArgs();
                if (restartCount < MaxRestartCount)
                {
                    var exe = Process.GetCurrentProcess().MainModule.FileName;
                    var args = "--relaunch --restore-logs --reinject --restart-count " + (restartCount + 1).ToString();
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = exe,
                        Arguments = args,
                        UseShellExecute = false,
                        CreateNoWindow = true
                    });
                }
            }
            catch { }
            finally
            {
                try { Current.Shutdown(); } catch { Environment.Exit(1); }
            }
        }
    }
}
