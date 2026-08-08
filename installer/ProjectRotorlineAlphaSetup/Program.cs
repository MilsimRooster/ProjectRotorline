using System;
using System.Drawing;
using System.Net;
using System.Windows.Forms;

namespace ProjectRotorlineAlphaSetup
{
    internal static class Program
    {
        [STAThread]
        private static void Main(string[] args)
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;

            if (InstallerSelfTest.TryRunCommand(args))
            {
                return;
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new InstallerForm());
        }
    }

    internal sealed class InstallerForm : Form
    {
        private readonly Label _statusLabel;
        private readonly Label _detailLabel;
        private readonly ProgressBar _progressBar;
        private readonly Button _installButton;

        public InstallerForm()
        {
            Text = "Project Rotorline Alpha Setup";
            ClientSize = new Size(680, 360);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(10, 25, 34);
            ForeColor = Color.White;
            Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);

            Label titleLabel = new Label();
            titleLabel.Text = "PROJECT ROTORLINE";
            titleLabel.Font = new Font("Bahnschrift", 26, FontStyle.Bold);
            titleLabel.ForeColor = Color.FromArgb(232, 181, 72);
            titleLabel.AutoSize = true;
            titleLabel.Location = new Point(34, 30);

            Label alphaLabel = new Label();
            alphaLabel.Text = "WINDOWS ALPHA";
            alphaLabel.Font = new Font("Bahnschrift", 11, FontStyle.Regular);
            alphaLabel.ForeColor = Color.FromArgb(143, 226, 215);
            alphaLabel.AutoSize = true;
            alphaLabel.Location = new Point(38, 78);

            Label descriptionLabel = new Label();
            descriptionLabel.Text =
                "Install the complete 25-mission helicopter operations Alpha.";
            descriptionLabel.Font = new Font("Segoe UI", 11);
            descriptionLabel.ForeColor = Color.FromArgb(215, 223, 226);
            descriptionLabel.AutoSize = true;
            descriptionLabel.Location = new Point(38, 118);

            _statusLabel = new Label();
            _statusLabel.Text = "Ready to install";
            _statusLabel.Font = new Font(
                "Segoe UI Semibold",
                11,
                FontStyle.Bold);
            _statusLabel.ForeColor = Color.White;
            _statusLabel.AutoSize = true;
            _statusLabel.Location = new Point(38, 176);

            _detailLabel = new Label();
            _detailLabel.Text =
                "Release size and hashes are read from one release manifest.";
            _detailLabel.Font = new Font("Segoe UI", 9);
            _detailLabel.ForeColor = Color.FromArgb(165, 181, 187);
            _detailLabel.AutoSize = true;
            _detailLabel.Location = new Point(38, 204);

            _progressBar = new ProgressBar();
            _progressBar.Minimum = 0;
            _progressBar.Maximum = 1000;
            _progressBar.Value = 0;
            _progressBar.Style = ProgressBarStyle.Continuous;
            _progressBar.Location = new Point(38, 238);
            _progressBar.Size = new Size(604, 20);

            _installButton = new Button();
            _installButton.Text = "INSTALL AND PLAY";
            _installButton.Font = new Font("Bahnschrift", 11, FontStyle.Bold);
            _installButton.BackColor = Color.FromArgb(232, 181, 72);
            _installButton.ForeColor = Color.FromArgb(10, 25, 34);
            _installButton.FlatStyle = FlatStyle.Flat;
            _installButton.Location = new Point(422, 286);
            _installButton.Size = new Size(220, 44);
            _installButton.Cursor = Cursors.Hand;
            _installButton.FlatAppearance.BorderSize = 0;
            _installButton.Click += InstallButtonClick;

            Label destinationLabel = new Label();
            destinationLabel.Text =
                "Installs to " + InstallerEngine.GetInstallPath();
            destinationLabel.Font = new Font("Segoe UI", 8);
            destinationLabel.ForeColor = Color.FromArgb(116, 139, 148);
            destinationLabel.AutoSize = true;
            destinationLabel.Location = new Point(38, 301);

            Controls.Add(titleLabel);
            Controls.Add(alphaLabel);
            Controls.Add(descriptionLabel);
            Controls.Add(_statusLabel);
            Controls.Add(_detailLabel);
            Controls.Add(_progressBar);
            Controls.Add(_installButton);
            Controls.Add(destinationLabel);
        }

        private async void InstallButtonClick(object sender, EventArgs e)
        {
            _installButton.Enabled = false;
            ControlBox = false;

            try
            {
                InstallerEngine engine = new InstallerEngine(
                    ReleaseConfiguration.ReleaseTag,
                    ReleaseConfiguration.ReleaseBaseUrl);
                await engine.InstallAsync(ApplyProgress);
                await System.Threading.Tasks.Task.Delay(700);
                Close();
            }
            catch (Exception exception)
            {
                _statusLabel.Text = "Installation failed";
                _detailLabel.Text = exception.Message;
                MessageBox.Show(
                    this,
                    exception.Message,
                    "Project Rotorline Alpha Setup",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                _installButton.Enabled = true;
                ControlBox = true;
            }
        }

        private void ApplyProgress(InstallerProgress progress)
        {
            if (InvokeRequired)
            {
                BeginInvoke(
                    new Action<InstallerProgress>(ApplyProgress),
                    progress);
                return;
            }

            _statusLabel.Text = progress.Status;
            _detailLabel.Text = progress.Detail;
            _progressBar.Value = progress.Permille;
        }
    }
}
