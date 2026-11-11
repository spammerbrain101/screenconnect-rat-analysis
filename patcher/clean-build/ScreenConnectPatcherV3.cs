using System;
using System.IO;
using System.Diagnostics;
using System.Windows.Forms;
using System.Xml;
using System.Collections.Generic;
using dnlib.DotNet;
using dnlib.DotNet.Emit;
using dnlib.DotNet.Writer;

namespace ScreenConnectPatcher
{
    public class MainModule
    {
        private Form mainForm;
        private Button btnPatchInstaller;
        private Button btnPatchSystem;
        private TextBox txtInstallerPath;
        private TextBox txtSystemPath;
        private Label lblStatus;

        public MainModule()
        {
            InitializeComponents();
        }

        private void InitializeComponents()
        {
            mainForm = new Form
            {
                Text = "ScreenConnect Patcher V3",
                Width = 520,
                Height = 320,
                FormBorderStyle = FormBorderStyle.FixedDialog,
                StartPosition = FormStartPosition.CenterScreen
            };

            var lblInstaller = new Label { Text = "Installer (MSI):", Left = 20, Top = 25, Width = 100 };
            txtInstallerPath = new TextBox { Left = 125, Top = 25, Width = 250 };
            var btnBrowseInstaller = new Button { Text = "Browse", Left = 385, Top = 23, Width = 90 };
            btnBrowseInstaller.Click += (s, e) => BrowseFile(txtInstallerPath, "MSI files|*.msi");
            
            btnPatchInstaller = new Button 
            { 
                Text = "Patch Installer", Left = 20, Top = 55, Width = 455, Height = 30 
            };
            btnPatchInstaller.Click += PatchInstaller;

            var lblSystem = new Label { Text = "System Path:", Left = 20, Top = 100, Width = 100 };
            txtSystemPath = new TextBox 
            { 
                Left = 125, Top = 100, Width = 250,
                Text = @"C:\Program Files (x86)\ScreenConnect"
            };
            var btnBrowseSystem = new Button { Text = "Browse", Left = 385, Top = 98, Width = 90 };
            btnBrowseSystem.Click += (s, e) => BrowseFolder(txtSystemPath);
            
            btnPatchSystem = new Button 
            { 
                Text = "Patch System", Left = 20, Top = 130, Width = 455, Height = 30 
            };
            btnPatchSystem.Click += PatchSystem;

            lblStatus = new Label 
            { 
                Left = 20, Top = 175, Width = 455, Height = 100,
                ForeColor = System.Drawing.Color.DarkBlue
            };

            mainForm.Controls.AddRange(new Control[] {
                lblInstaller, txtInstallerPath, btnBrowseInstaller, btnPatchInstaller,
                lblSystem, txtSystemPath, btnBrowseSystem, btnPatchSystem,
                lblStatus
            });
        }

        public void Run()
        {
            Application.EnableVisualStyles();
            Application.Run(mainForm);
        }

        private void BrowseFile(TextBox target, string filter)
        {
            using (var dlg = new OpenFileDialog { Filter = filter })
            {
                if (dlg.ShowDialog() == DialogResult.OK)
                    target.Text = dlg.FileName;
            }
        }

        private void BrowseFolder(TextBox target)
        {
            using (var dlg = new FolderBrowserDialog())
            {
                if (dlg.ShowDialog() == DialogResult.OK)
                    target.Text = dlg.SelectedPath;
            }
        }

        private void Log(string msg)
        {
            lblStatus.Text = msg;
            Application.DoEvents();
        }

        // =================================================================
        // PATCH INSTALLER
        // =================================================================
        private void PatchInstaller(object sender, EventArgs e)
        {
            try
            {
                string msiPath = txtInstallerPath.Text;
                if (!File.Exists(msiPath))
                {
                    MessageBox.Show("Select a valid MSI file.", "Error");
                    return;
                }

                Log("Patching installer: " + Path.GetFileName(msiPath));

                // Patch MSI: remove service auto-start and license callback custom actions
                PatchMsiFile(msiPath);

                Log("Installer patched!\n" +
                    "Install the MSI, then run 'Patch System' on the install dir.\n" +
                    "After install: call /SetupCreateUser.aspx, then DELETE it.");
            }
            catch (Exception ex)
            {
                Log("Error: " + ex.Message);
            }
        }

        private void PatchMsiFile(string msiPath)
        {
            // MSI files are OLE structured storage documents
            // We need to modify the ServiceInstall and CustomAction tables
            // to prevent auto-start and license callbacks
            
            // Backup original
            string backup = msiPath + ".original";
            if (!File.Exists(backup))
                File.Copy(msiPath, backup);

            // Use msiexec to export tables, modify, and reimport
            // Or use direct binary patching of the MSI stream
            
            string tempDir = Path.Combine(Path.GetTempPath(), "SC_Patcher_" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempDir);

            try
            {
                // Export MSI tables using msiexec
                var psi = new ProcessStartInfo
                {
                    FileName = "msidb.exe",
                    Arguments = string.Format("-d \"{0}\" -f \"{1}\" -e ServiceInstall ServiceControl CustomAction Property", 
                        msiPath, tempDir),
                    CreateNoWindow = true,
                    UseShellExecute = false
                };
                Process.Start(psi)?.WaitForExit(10000);

                // Modify the exported IDT files to remove auto-start
                // and license validation custom actions
                string serviceInstallFile = Path.Combine(tempDir, "ServiceInstall.idt");
                if (File.Exists(serviceInstallFile))
                {
                    // Remove or modify StartType column (set to 3 = Demand Start)
                    PatchIdtFile(serviceInstallFile, 6, "3"); // Column 6 = StartType
                    Log("  Patched: MSI ServiceInstall (disabled auto-start)");
                }

                string customActionFile = Path.Combine(tempDir, "CustomAction.idt");
                if (File.Exists(customActionFile))
                {
                    // Remove custom actions that call license.screenconnect.com
                    RemoveLicenseCustomActions(customActionFile);
                    Log("  Patched: MSI CustomAction (removed license callbacks)");
                }

                // Reimport modified tables
                psi = new ProcessStartInfo
                {
                    FileName = "msidb.exe",
                    Arguments = string.Format("-d \"{0}\" -f \"{1}\" -i ServiceInstall ServiceControl CustomAction Property",
                        msiPath, tempDir),
                    CreateNoWindow = true,
                    UseShellExecute = false
                };
                Process.Start(psi)?.WaitForExit(10000);
            }
            finally
            {
                Directory.Delete(tempDir, true);
            }
        }

        private void PatchIdtFile(string idtFile, int columnIndex, string newValue)
        {
            string[] lines = File.ReadAllLines(idtFile);
            // IDT format: header (3 lines) + data rows
            for (int i = 3; i < lines.Length; i++)
            {
                string[] cols = lines[i].Split('\t');
                if (cols.Length > columnIndex)
                {
                    cols[columnIndex] = newValue;
                    lines[i] = string.Join("\t", cols);
                }
            }
            File.WriteAllLines(idtFile, lines);
        }

        private void RemoveLicenseCustomActions(string idtFile)
        {
            string[] lines = File.ReadAllLines(idtFile);
            List<string> filtered = new List<string>();
            // Keep header (first 3 lines)
            for (int i = 0; i < 3 && i < lines.Length; i++)
                filtered.Add(lines[i]);
            // Filter data rows - remove rows containing "license" or "screenconnect.com"
            for (int i = 3; i < lines.Length; i++)
            {
                if (!lines[i].ToLower().Contains("license.screenconnect.com") &&
                    !lines[i].ToLower().Contains("validatelicen"))
                    filtered.Add(lines[i]);
            }
            File.WriteAllLines(idtFile, filtered.ToArray());
        }

        // =================================================================
        // PATCH SYSTEM
        // =================================================================
        private void PatchSystem(object sender, EventArgs e)
        {
            try
            {
                string systemPath = txtSystemPath.Text;
                if (!Directory.Exists(systemPath))
                {
                    MessageBox.Show("Select a valid ScreenConnect install directory.", "Error");
                    return;
                }

                Log("Patching system: " + systemPath);

                // Step 1: Find and patch the ScreenConnect server assemblies
                string binDir = Path.Combine(systemPath, "bin");
                if (!Directory.Exists(binDir))
                    binDir = systemPath;

                int patchedCount = 0;
                string[] dllFiles = Directory.GetFiles(binDir, "ScreenConnect*.dll");
                string[] exeFiles = Directory.GetFiles(binDir, "ScreenConnect*.exe");
                
                List<string> allFiles = new List<string>();
                allFiles.AddRange(dllFiles);
                allFiles.AddRange(exeFiles);

                foreach (string asmPath in allFiles)
                {
                    if (PatchLicenseManager(asmPath))
                    {
                        patchedCount++;
                        Log("  Patched: " + Path.GetFileName(asmPath));
                    }
                }

                if (patchedCount == 0)
                    Log("  WARNING: No license methods found - assembly may have changed!");

                // Step 2: Delete license.xml
                DeleteLicenseFile(systemPath);

                // Step 3: Delete/rename SetupWizard.aspx
                DeleteSetupWizard(systemPath);

                // Step 4: Patch web.config
                PatchWebConfig(systemPath);

                // Step 5: Block license server in hosts
                PatchHostsFile();

                // Step 6: Restart service
                RestartScreenConnectService();

                Log(string.Format("System patched! ({0} assemblies modified)\n" +
                    "IMPORTANT: Go to /SetupCreateUser.aspx to create admin user,\n" +
                    "then DELETE that page! Default: Administrator / Administrator",
                    patchedCount));
            }
            catch (Exception ex)
            {
                Log("Error: " + ex.Message);
            }
        }

        // =================================================================
        // CORE: License Bypass using dnlib
        // =================================================================
        private bool PatchLicenseManager(string assemblyPath)
        {
            try
            {
                ModuleDefMD module = ModuleDefMD.Load(assemblyPath);
                bool foundAny = false;

                foreach (TypeDef type in module.GetTypes())
                {
                    string typeName = type.FullName.ToString();
                    
                    if (!typeName.Contains("LicenseManager") && 
                        !typeName.Contains("GetCapabilitiesAsync") &&
                        !typeName.Contains("TryAcquireLicense"))
                        continue;

                    foreach (MethodDef method in type.Methods)
                    {
                        string methodName = method.Name.String;

                        // Patch 1: GetCapabilitiesAsync::MoveNext()
                        // Returns license capabilities - we make it return unlimited
                        if (methodName.Contains("MoveNext") && 
                            (typeName.Contains("GetCapabilities") || typeName.Contains("Capabilities")))
                        {
                            PatchMethodToReturnTrue(method, module);
                            foundAny = true;
                        }

                        // Patch 2: TryAcquireLicenseAsync
                        // Tries to acquire a license from pool - we make it always succeed
                        if (methodName.Contains("TryAcquireLicense"))
                        {
                            PatchMethodToReturnTrue(method, module);
                            foundAny = true;
                        }

                        // Patch 3: Any method with "License" + "Valid" or "Check"
                        if ((methodName.Contains("Valid") || methodName.Contains("Check")) &&
                            method.ReturnType.FullName.Contains("Boolean"))
                        {
                            PatchMethodToReturnTrue(method, module);
                            foundAny = true;
                        }
                    }
                }

                if (foundAny)
                {
                    // Backup original
                    string backupPath = assemblyPath + ".bak";
                    if (!File.Exists(backupPath))
                        File.Copy(assemblyPath, backupPath);

                    // Write patched assembly
                    module.Write(assemblyPath);
                }

                return foundAny;
            }
            catch
            {
                return false;
            }
        }

        private void PatchMethodToReturnTrue(MethodDef method, ModuleDefMD module)
        {
            if (!method.HasBody)
                return;

            // Replace method body with: ldc.i4.1 (true) ; ret
            method.Body.Instructions.Clear();
            method.Body.Variables.Clear();
            method.Body.ExceptionHandlers.Clear();

            // Add: return true
            Instruction retInstr = Instruction.Create(OpCodes.Ret);
            Instruction loadTrue = Instruction.Create(OpCodes.Ldc_I4_1);
            
            method.Body.Instructions.Add(loadTrue);
            method.Body.Instructions.Add(retInstr);
            
            // Update method parameters/return type if needed
            method.Body.UpdateInstructionOffsets();
        }

        // =================================================================
        // Delete license.xml
        // =================================================================
        private void DeleteLicenseFile(string systemPath)
        {
            string appData = Path.Combine(systemPath, "App_Data");
            string licenseFile = Path.Combine(appData, "license.xml");
            
            if (File.Exists(licenseFile))
            {
                File.Delete(licenseFile);
                Log("  Deleted: App_Data/license.xml");
            }
            else
            {
                Log("  license.xml not found (already removed)");
            }
        }

        // =================================================================
        // Delete SetupWizard.aspx
        // =================================================================
        private void DeleteSetupWizard(string systemPath)
        {
            string setupWizard = Path.Combine(systemPath, "SetupWizard.aspx");
            if (File.Exists(setupWizard))
            {
                string disabled = setupWizard + ".disabled";
                if (File.Exists(disabled))
                    File.Delete(disabled);
                File.Move(setupWizard, disabled);
                Log("  Renamed: SetupWizard.aspx -> .disabled");
            }
            else
            {
                Log("  SetupWizard.aspx not found (already removed)");
            }
        }

        // =================================================================
        // Patch web.config
        // =================================================================
        private void PatchWebConfig(string systemPath)
        {
            string webConfigPath = Path.Combine(systemPath, "web.config");
            if (!File.Exists(webConfigPath))
            {
                Log("  web.config not found");
                return;
            }

            try
            {
                var doc = new XmlDocument();
                doc.Load(webConfigPath);

                var nodes = doc.SelectNodes("//connectionStrings/add[@name='SessionDatabase']");
                if (nodes != null && nodes.Count > 0)
                {
                    var elem = nodes[0] as XmlElement;
                    if (elem != null)
                    {
                        elem.SetAttribute("connectionString",
                            "Data Source=|DataDirectory|/Session.db; " +
                            "DateTimeKind=Utc; Foreign Keys=true; " +
                            "Page Size=4096; Journal Mode=WAL; " +
                            "BaseSchemaName=; Cache Size=1000; " +
                            "Memory Mapped Size=10000000000; DateTimeFormat=Ticks");
                        elem.SetAttribute("providerName", "SQLite");
                        Log("  Patched: web.config SessionDatabase");
                    }
                }

                doc.Save(webConfigPath);
            }
            catch (Exception ex)
            {
                Log("  web.config error: " + ex.Message);
            }
        }

        // =================================================================
        // Block license server in hosts file
        // =================================================================
        private void PatchHostsFile()
        {
            string hostsPath = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.System),
                @"drivers\etc\hosts");

            try
            {
                string hosts = File.ReadAllText(hostsPath);
                if (!hosts.Contains("license.screenconnect.com"))
                {
                    File.AppendAllText(hostsPath,
                        "\n# ScreenConnect Patcher - Block license server\n" +
                        "127.0.0.1 license.screenconnect.com\n" +
                        "127.0.0.1 licensing.screenconnect.com\n");
                    Log("  Blocked: license.screenconnect.com in hosts");
                }
                else
                {
                    Log("  Hosts entry already exists");
                }
            }
            catch (UnauthorizedAccessException)
            {
                Log("  Could not edit hosts (run as Administrator)");
            }
        }

        // =================================================================
        // Restart ScreenConnect service
        // =================================================================
        private void RestartScreenConnectService()
        {
            try
            {
                // Find and restart the ScreenConnect service
                var psi = new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = "/c net stop ScreenConnect* & net start ScreenConnect*",
                    CreateNoWindow = true,
                    UseShellExecute = false,
                    RedirectStandardOutput = true
                };
                Process.Start(psi)?.WaitForExit(15000);
                Log("  Service restarted");
            }
            catch
            {
                Log("  Restart service manually after patching");
            }
        }
    }

    static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
            var patcher = new MainModule();
            patcher.Run();
        }
    }
}
